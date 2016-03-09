/* A simple standalone XML-RPC server program based on Abyss that uses SSL
   (Secure Sockets Layer) via OpenSSL, with authentication.

   This is like the 'ssl_server' example, except that it requires
   authentication, which means it is much harder to use - you have to set up a
   certificate, private key, DSA parameters, and Diffie-Hellman parameters.

   Example:

     $ ./ssl_secure_server 8080 &
     $ ./curl_client https://localhost:8080/RPC2

   Before the above will work, you have to create a variety of files and put
   them where this program expects to find them.  Example:

     $ mkdir /tmp/ssltest
     $ cd /tmp/ssltest
     $ openssl genpkey -genparam -algorithm DSA >dsap.pem
     $ openssl genpkey -paramfile dsap.pem >dsakey.pem
     $ openssl req -new dsakey.pem >csr.csr
     $ openssl x509 -req -in csr.csr -signkey dsakey.pem >certificate.pem
     $ openssl dhparam -in dsap.pem -dsaparam -outform PEM >dhparam.pem


   You can drive the most difficult part of this example (initial SSL
   handshake) with the 'openssl' program that comes with OpenSSL, as follows.
   
      $ ./ssl_secure_server 8080 &
      $ openssl -connect localhost:8080 -state 

   The 'openssl' command connects and handshakes with the server, then waits
   for you to type stuff to send to the server.  You would have to type a
   complete HTTP header followed by a valid XML-RPC call to complete the
   demonstration.

   This uses the "provide your own Abyss server" mode of operation, 
   as opposed to other Xmlrpc-c facilities that create an Abyss server under
   the covers, because this is the only way to get SSL.

   NOTE: We deliberately don't check error indications here to make the code
   easier to read.  If you're having trouble getting this code to run, by all
   means add checks of the "error" and "env" variables!
*/

#define _XOPEN_SOURCE 600
#define WIN32_LEAN_AND_MEAN  /* required by xmlrpc-c/server_abyss.h */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <openssl/ssl.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/abyss_openssl.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */


static void
sslInfoCallback(const SSL * const sslP,
                int         const where,
                int         const ret) {

    const char * str;
    int const w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT)
        str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
        str = "SSL_accept";
    else
        str = "undefined";

    if (where & SSL_CB_LOOP) {
        fprintf(stderr, "%s:%s\n", str, SSL_state_string_long(sslP));
    } else if (where & SSL_CB_ALERT) {
        str = (where & SSL_CB_READ) ? "read" : "write";
        fprintf(stderr, "SSL3 alert %s:%s:%s\n",
                str,
                SSL_alert_type_string_long(ret),
                SSL_alert_desc_string_long(ret));
    } else if (where & SSL_CB_EXIT) {
        if (ret == 0)
            fprintf(stderr, "%s:failed in %s\n", 
                    str, SSL_state_string_long(sslP));
        else if (ret < 0) {
            fprintf(stderr, "%s:error in %s\n",
                    str, SSL_state_string_long(sslP));
        }
    }
}



static void
printPeerIpAddr(TSession * const abyssSessionP) {

#ifdef _WIN32
    struct abyss_win_chaninfo * channelInfoP;
#else
    struct abyss_unix_chaninfo * channelInfoP;
#endif
    struct sockaddr_in * sockAddrInP;
    unsigned char * ipAddr;  /* 4 byte array */

    SessionGetChannelInfo(abyssSessionP, (void*)&channelInfoP);

    sockAddrInP = (struct sockaddr_in *) &channelInfoP->peerAddr;

    ipAddr = (unsigned char *)&sockAddrInP->sin_addr.s_addr;

    printf("RPC is from IP address %u.%u.%u.%u\n",
           ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
}



static xmlrpc_server_shutdown_fn requestShutdown;

static void
shutdownAbyss(xmlrpc_env * const faultP,
              void *       const context,
              const char * const comment,
              void *       const callInfo) {
    
    TServer * const abyssServerP = context;

    xmlrpc_env_init(faultP);

    ServerTerminate(abyssServerP);
}



static xmlrpc_value *
sample_add(xmlrpc_env *   const envP, 
           xmlrpc_value * const paramArrayP,
           void *         const serverInfo,
           void *         const channelInfo) {
    
    xmlrpc_int x, y, z;

    printPeerIpAddr(channelInfo);

    /* Parse our argument array. */
    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    if (envP->fault_occurred)
        return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_build_value(envP, "i", z);
}



int 
main(int           const argc, 
     const char ** const argv) {

    struct xmlrpc_method_info3 const methodInfo = {
        .methodName     = "sample.add",
        .methodFunction = &sample_add,
        .serverInfo = NULL
    };

    SSL_CTX * sslCtxP;
    FILE * certFileP;
    TChanSwitch * chanSwitchP;
    TServer abyssServer;
    xmlrpc_registry * registryP;
    xmlrpc_env env;
    const char * error;

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port number "
                "on which to listen for XML-RPC calls.  "
                "You specified %d.\n",  argc-1);
        exit(1);
    }

    AbyssInit(&error);
    
    xmlrpc_env_init(&env);

    sslCtxP = SSL_CTX_new(SSLv23_server_method());

    SSL_CTX_set_cipher_list(sslCtxP, "ALL:!aNULL");

    SSL_CTX_use_certificate_chain_file(sslCtxP,
                                       "/tmp/ssltest/certificate.pem");
            
    SSL_CTX_use_PrivateKey_file(sslCtxP, "/tmp/ssltest/dsakey.pem",
                                SSL_FILETYPE_PEM);

    certFileP = fopen("/tmp/ssltest/dhparam.pem", "r");

    DH * const dhP = PEM_read_DHparams(certFileP, NULL, NULL, NULL);

    SSL_CTX_set_tmp_dh(sslCtxP, dhP);

    SSL_CTX_set_info_callback(sslCtxP, sslInfoCallback);

    ChanSwitchOpenSslCreateIpV4Port(atoi(argv[1]), sslCtxP,
                                    &chanSwitchP, &error);

    ServerCreateSwitch(&abyssServer, chanSwitchP, &error);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method3(&env, registryP, &methodInfo);

    xmlrpc_registry_set_shutdown(registryP, &shutdownAbyss, &abyssServer);

    xmlrpc_server_abyss_set_handlers2(&abyssServer, "/RPC2", registryP);

    ServerInit(&abyssServer);

    printf("Running server...\n");

    ServerRun(&abyssServer);
        /* This waits for TCP connections and processes them as XML-RPC
           RPCs indefinitely (until system.shutdown method performed).
        */

    ServerFree(&abyssServer);
    ChanSwitchDestroy(chanSwitchP);
    SSL_CTX_free(sslCtxP);
    AbyssTerm();

    return 0;
}

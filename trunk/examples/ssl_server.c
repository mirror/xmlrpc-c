/* A simple standalone XML-RPC server program based on Abyss that uses SSL
   (Secure Sockets Layer) via OpenSSL.

   ===> WE HAVE NEVER SEEN THIS EXAMPLE WORK.  EITHER IT OR THE ABYSS
   ===> OPENSSL LIBRARY STUFF IS PROBABLY BROKEN.

   You can drive this with the 'openssl' program that comes with OpenSSL as
   follows.
   
      $ ./ssl_server 8080 &
      $ openssl -connect localhost:8080 -state

   But we have never seen this successfully form a connection.  (See above).

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
    SSL_CTX * const sslCtxP = SSL_CTX_new(SSLv23_server_method());

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

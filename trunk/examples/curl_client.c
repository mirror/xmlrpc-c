/* Same as xmlrpc_sample_add_client.c, except it demonstrates use of Curl
   transport parameters, including timeout and SSL stuff.

   You specify the server URL as an argument, which can be an "http:" URL for
   true XML-RPC or "https:" for the secure SSL/TLS variation of XML-RPC.

   Example that works with 'xmlrpc_sample_add_server' example server:

     $ ./curl_client http::/localhost:8080/RPC2

   Example that works with 'ssl_server' example server:

     $ ./curl_client https::/localhost:8080/RPC2
   
*/

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */



static void 
die_if_fault_occurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



static void
add(xmlrpc_client * const clientP,
    const char *    const serverUrl,
    int             const addend,
    int             const adder) {

    const char * const methodName = "sample.add";

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_int32 sum;

    xmlrpc_env_init(&env);

    printf("Making XMLRPC call to server url '%s' method '%s' "
           "to request the sum "
           "of %d and %d...\n", serverUrl, methodName, addend, adder);

    /* Make the remote procedure call */

    xmlrpc_client_call2f(&env, clientP, serverUrl, methodName, &resultP,
                         "(ii)", (xmlrpc_int32) addend, (xmlrpc_int32) adder);
    die_if_fault_occurred(&env);

    /* Get our sum and print it out. */
    xmlrpc_read_int(&env, resultP, &sum);
    die_if_fault_occurred(&env);
    printf("The sum is %d\n", sum);
    
    /* Dispose of our result value. */
    xmlrpc_DECREF(resultP);

    xmlrpc_env_clean(&env);
}



int 
main(int           const argc, 
     const char ** const argv) {

    const char * serverUrl;

    xmlrpc_env env;
    struct xmlrpc_curl_xportparms transportParms;
    struct xmlrpc_clientparms clientParms;
    xmlrpc_client * clientP;

    if (argc-1 < 1) {
        fprintf(stderr, "You must specify the server URL as an argument.  "
                "Example: http://localhost:8080/RPC2\n");
        exit(1);
    } else {
        serverUrl = argv[1];
    }

    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    /* Required before any use of Xmlrpc-c client library: */
    xmlrpc_client_setup_global_const(&env);
    die_if_fault_occurred(&env);

    transportParms.network_interface = NULL;
    transportParms.no_ssl_verifypeer = 1;
    transportParms.no_ssl_verifyhost = 1;
    transportParms.user_agent = NULL;
    transportParms.ssl_cert = NULL;
    transportParms.sslcerttype = NULL;
    transportParms.sslcertpasswd = NULL;
    transportParms.sslkey = NULL;
    transportParms.sslkeytype = NULL;
    transportParms.sslengine = NULL;
    transportParms.sslengine_default = 0;
    transportParms.sslversion = XMLRPC_SSLVERSION_DEFAULT;
    transportParms.cainfo = NULL;
    transportParms.capath = NULL;
    transportParms.randomfile = NULL;
    transportParms.egdsocket= NULL;
    transportParms.ssl_cipher_list= "ALL:eNULL:aNULL";
    transportParms.timeout = 2000;  /* milliseconds */

    clientParms.transport = "curl";
    clientParms.transportparmsP = &transportParms;
    clientParms.transportparm_size = XMLRPC_CXPSIZE(timeout);

    /* Create a client object */
    xmlrpc_client_create(&env, 0, NULL, NULL,
                         &clientParms, XMLRPC_CPSIZE(transportparm_size),
                         &clientP);

    die_if_fault_occurred(&env);

    /* If our server is running 'xmlrpc_sample_add_server' normally, the
       RPC will finish almost instantly.  UNLESS the adder is 1, in which
       case said server is programmed to take 3 seconds to do the
       computation, thus allowing us to demonstrate a timeout.
    */

    add(clientP, serverUrl, 5, 7);
        /* Should finish instantly */

    add(clientP, serverUrl, 5, 1);
        /* Should time out after 2 seconds */

    xmlrpc_env_clean(&env);
    xmlrpc_client_destroy(clientP);
    xmlrpc_client_teardown_global_const();

    return 0;
}


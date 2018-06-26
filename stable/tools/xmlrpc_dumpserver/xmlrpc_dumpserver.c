/*=============================================================================
                              xmlrpc_dumpserver
===============================================================================

  This program runs an XML-RPC server that does nothing but print to Standard
  Output the contents of every call it receives, and send a failure response.

  You use this to test a client or to learn about XML-RPC.

  Example:

    $ xmlrpc_dumpserver -port=8080 &

    $ xmlrpc localhost:8080 dummymethod s/hello i/5

=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "xmlrpc_config.h"  /* information about this build environment */
#include "bool.h"
#include "int.h"
#include "mallocvar.h"
#include "girstring.h"
#include "casprintf.h"
#include "cmdline_parser.h"
#include "dumpvalue.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server_abyss.h"

#define NAME "xmlrpc command line program"
#define VERSION "1.0"

struct CmdlineInfo {
    unsigned int port;
};



static void
parseCommandLine(int                  const argc,
                 const char **        const argv,
                 struct CmdlineInfo * const cmdlineP,
                 const char **        const errorP) {

    cmdlineParser const cp = cmd_createOptionParser();

    const char * error;

    cmd_defineOption(cp, "port",             OPTTYPE_UINT);

    cmd_processOptions(cp, argc, argv, &error);

    if (error) {
        casprintf(errorP, "Command syntax error.  %s", error);
        strfree(error);
    } else {
        *errorP = NULL;  /* initial assumption */

        if (cmd_argumentCount(cp) > 0)
            casprintf(errorP, "No non-option arguments are possible.  "
                      "You specified %u", cmd_argumentCount(cp));

        if (!*errorP) {
            if (cmd_optionIsPresent(cp, "port")) {
                cmdlineP->port = cmd_getOptionValueUint(cp, "port");
            } else {
                casprintf(errorP, "You must specify the TCP port number "
                          "on which to listen for XML-RPC calls with -port");
            }
        }
    }
    cmd_destroyOptionParser(cp);
}



static void
freeCmdline(struct CmdlineInfo const cmdline ATTR_UNUSED) {

}



static void
dumpParameters(xmlrpc_value * const paramArrayP) {

    xmlrpc_env env;
    int arraySize;
    unsigned int paramSeq;

    xmlrpc_env_init(&env);
        
    arraySize = xmlrpc_array_size(&env, paramArrayP);

    assert(!env.fault_occurred);
    assert(arraySize >= 0);

    {
        unsigned int const paramCt = arraySize;

        printf("Number of parameters: %u\n", paramCt);

        printf("\n");

        for (paramSeq = 0; paramSeq < paramCt; ++paramSeq) {

            xmlrpc_value * paramValueP;

            xmlrpc_array_read_item(&env, paramArrayP, paramSeq, &paramValueP);

            assert(!env.fault_occurred);

            printf("Parameter %u:\n", paramSeq);
            printf("\n");

            dumpValue("  ", paramValueP);

            xmlrpc_DECREF(paramValueP);
        
            printf("\n");
        }
    }
    xmlrpc_env_clean(&env);
}



static xmlrpc_value *
defaultMethod(xmlrpc_env *   const envP,
              const char *   const callInfoP ATTR_UNUSED,
              const char *   const methodName,
              xmlrpc_value * const paramArrayP,
              void *         const serverInfo ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a *xmlrpc_default_method, i.e. a default method for an Xmlrpc-c
   registry, which is meant to be called by an XML-RPC server when it receives
   a call for a method that is not registered by name.
-----------------------------------------------------------------------------*/
    printf("Server has received a call of method '%s'\n", methodName);

    printf("\n");

    assert(xmlrpc_value_type(paramArrayP) == XMLRPC_TYPE_ARRAY);

    dumpParameters(paramArrayP);

    xmlrpc_faultf(envP,
                  "This XML-RPC server is a test server that fails all "
                  "RPCs.  But the fact that it failed this way tells you that "
                  "the call _was_ valid in XML-RPC terms.  The server "
                  "printed to Standard Output a description of the valid "
                  "call.");

    /* Return value is irrelevant because we failed the RPC, but to prevent
       a compiler warning:
    */
    return NULL;
}



static void
setServerParm(xmlrpc_server_abyss_parms * const serverparmP,
              xmlrpc_registry *           const registryP,
              unsigned int                const portNum,
              size_t *                    const lengthP) {

    serverparmP->config_file_name   = NULL;
    serverparmP->registryP          = registryP;
    serverparmP->port_number        = portNum;
    serverparmP->log_file_name      = NULL;
    serverparmP->keepalive_timeout  = 0;
    serverparmP->keepalive_max_conn = 0;
    serverparmP->timeout            = 0;
    serverparmP->dont_advertise     = false;
    serverparmP->socket_bound       = false;
    serverparmP->uri_path           = NULL;
    serverparmP->chunk_response     = false;
    serverparmP->enable_shutdown    = true;

    *lengthP = XMLRPC_APSIZE(enable_shutdown);
}



static void
runServer(unsigned int  const portNum,
          const char ** const errorP) {

    xmlrpc_registry * registryP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);
    if (env.fault_occurred)
        casprintf(errorP, "xmlrpc_registry_new() failed.  %s",
                  env.fault_string);
    else {
        xmlrpc_registry_set_default_method(&env, registryP,
                                           &defaultMethod, NULL);

        if (env.fault_occurred)
            casprintf(errorP, "xmlrpc_registry_add_method3() failed.  %s",
                      env.fault_string);
        else {
            xmlrpc_server_abyss_parms serverparm;
            size_t parmLength;

            setServerParm(&serverparm, registryP, portNum, &parmLength);

            printf("Running XML-RPC server on TCP Port %u...\n", portNum);

            xmlrpc_server_abyss(&env, &serverparm, parmLength);

            if (env.fault_occurred)
                casprintf(errorP, "xmlrpc_server_abyss() failed.  %s",
                          env.fault_string);

            /* xmlrpc_server_abyss() never returns unless it fails */
            *errorP = NULL;
        }
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    int exitCode;
    struct CmdlineInfo cmdline;
    const char * error;

    parseCommandLine(argc, argv, &cmdline, &error);

    if (error) {
        fprintf(stderr, "Command syntax error.  %s\n", error);
        strfree(error);
        exitCode = 10;
    } else {
        const char * error;

        runServer(cmdline.port, &error);

        if (error) {
            fprintf(stderr, "Server on port %u failed.  %s\n",
                    cmdline.port, error);
            strfree(error);
            exitCode = 1;
        } else
            exitCode = 0;

        freeCmdline(cmdline);
    }
    return exitCode;
}




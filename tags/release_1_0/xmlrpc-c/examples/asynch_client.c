/* A simple asynchronous XML-RPC client written in C. */

#include <stdio.h>

#include <xmlrpc.h>
#include <xmlrpc_client.h>

#include "config.h"  /* information about this build environment */

#define NAME "XML-RPC C Test Client asynch_client"
#define VERSION "1.0"

static void die_if_fault_occurred (xmlrpc_env *env)
{
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}



static void 
print_state_name_callback(const char *   const server_url ATTR_UNUSED,
                          const char *   const method_name ATTR_UNUSED,
                          xmlrpc_value * const param_array,
                          void *         const user_data ATTR_UNUSED,
                          xmlrpc_env *   const env,
                          xmlrpc_value * const result) {

    int state_number;
    char *state_name;

    /* Check to see if a fault occurred. */
    die_if_fault_occurred(env);

    /* Get our state name. */
    xmlrpc_parse_value(env, result, "s", &state_name);
    die_if_fault_occurred(env);

    /* Our first four arguments provide helpful context. Let's grab the
    ** state number from our parameter array. */
    xmlrpc_parse_value(env, param_array, "(i)", &state_number);
    die_if_fault_occurred(env);
    
    printf("State #%d: %s\n", state_number, state_name);
}



int 
main(int           const argc, 
     const char ** const argv ATTR_UNUSED) {

    int i;

    if (argc-1 > 0) {
        fprintf(stderr, "There are no arguments.\n");
        exit(1);
    }
    
    /* Start up our XML-RPC client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);

    /* Make a whole bunch of asynch calls. */
    for (i = 40; i < 45; i++)
        xmlrpc_client_call_asynch("http://betty.userland.com/RPC2",
                                  "examples.getStateName",
                                  print_state_name_callback, NULL,
                                  "(i)", (xmlrpc_int32) i);
    
    /* Wait for all calls to complete. */
    xmlrpc_client_event_loop_finish_asynch();

    /* shutdown our XML-RPC client library. */
    xmlrpc_client_cleanup();

    return 0;
}

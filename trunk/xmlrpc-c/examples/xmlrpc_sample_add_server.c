/* A simple standalone XML-RPC server written in C. */

#include <stdio.h>

#include <xmlrpc.h>
#include <xmlrpc_abyss.h>

#include "config.h"  /* information about this build environment */

static xmlrpc_value *
sample_add(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 x, y, z;

    /* Parse our argument array. */
    xmlrpc_parse_value(env, param_array, "(ii)", &x, &y);
    if (env->fault_occurred)
	return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_build_value(env, "i", z);
}



int 
main (int           const argc, 
      const char ** const argv) {

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The Abyss " 
                "configuration file name.  You specified %d.\n",  argc-1);
        fprintf(stderr, "A suitable example Abyss configuration file is "
                "abyss.conf in the examples/ directory of the Xmlrpc-c "
                "source tree\n");
        exit(1);
    }

    xmlrpc_server_abyss_init(XMLRPC_SERVER_ABYSS_NO_FLAGS, argv[1]);
    xmlrpc_server_abyss_add_method("sample.add", &sample_add, NULL);

    printf("server: switching to background.\n");
    xmlrpc_server_abyss_run();

    /* We never reach this point. */
    return 0;
}

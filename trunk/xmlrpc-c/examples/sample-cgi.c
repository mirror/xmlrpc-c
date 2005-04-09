/* A simple CGI-based XML-RPC server written in C. */

#include <xmlrpc.h>
#include <xmlrpc_cgi.h>

#include "config.h"  /* information about this build environment */

static xmlrpc_value *
sample_add(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 x, y, z;

    /* Parse our argument array. */
    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    if (env->fault_occurred)
        return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_int_new(env, z);
}



int 
main(int           const argc ATTR_UNUSED, 
     const char ** const argv ATTR_UNUSED) {

    /* Process our request. */
    xmlrpc_cgi_init(XMLRPC_CGI_NO_FLAGS);
    xmlrpc_cgi_add_method_w_doc("sample.add", &sample_add, NULL,
                                "i:ii", "Add two integers.");
    xmlrpc_cgi_process_call();
    xmlrpc_cgi_cleanup();

    return 0;
}

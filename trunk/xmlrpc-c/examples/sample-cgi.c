/* A simple CGI-based XML-RPC server written in C. */

#include <xmlrpc.h>
#include <xmlrpc_cgi.h>

xmlrpc_value *
sample_add (xmlrpc_env *env, xmlrpc_value *param_array, void *user_data)
{
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

int main (int argc, char **argv)
{
    /* Process our request. */
    xmlrpc_cgi_init(XMLRPC_CGI_NO_FLAGS);
    xmlrpc_cgi_add_method("sample.add", &sample_add, NULL);
    xmlrpc_cgi_process_call();
    xmlrpc_cgi_cleanup();
}

/* A simple standalone XML-RPC server written in C. */

#include <stdio.h>

#include <xmlrpc.h>
#include <xmlrpc_abyss.h>

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
    if (argc != 2) {
	fprintf(stderr, "Usage: servertest abyss.conf\n");
	exit(1);
    }

    xmlrpc_server_abyss_init(XMLRPC_SERVER_ABYSS_NO_FLAGS, argv[1]);
    xmlrpc_server_abyss_add_method("sample.add", &sample_add, NULL);

    printf("server: switching to background.\n");
    xmlrpc_server_abyss_run();
}

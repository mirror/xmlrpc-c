/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */


#include <stdio.h>

#include "xmlrpc.h"
#include "xmlrpc_client.h"

#define NAME "XML-RPC C Test Client"
#define VERSION "0.1"

void die_if_fault_occurred (xmlrpc_env *env)
{
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}

static void print_state_name_callback (char *server_url,
				       char *method_name,
				       xmlrpc_value *param_array,
				       void *user_data,
				       xmlrpc_env *env,
				       xmlrpc_value *result)
{
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

int main (int argc, char** argv)
{
    xmlrpc_env env;
    int i;
    
    /* Start up our XML-RPC client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
    xmlrpc_env_init(&env);

    /* Make a whole bunch of asynchronous calls. */
    for (i = 40; i < 45; i++) {
	xmlrpc_client_call_asynch("http://betty.userland.com/RPC2",
				  "examples.getStateName",
				  print_state_name_callback, NULL,
				  "(i)", (xmlrpc_int32) i);
    }

    /* Wait for all calls to complete. */
    xmlrpc_client_event_loop_finish_asynch();    

    /* Shutdown our XML-RPC client library. */
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

    return 0;
}

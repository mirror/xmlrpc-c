/* Copyright (C) 2001 by Eric Kidd. All rights reserved.
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


#include "xmlrpc_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "xmlrpc.h"
#include "xmlrpc_cgi.h"


/*=========================================================================
**  Output Routines
**=========================================================================
**  These routines send various kinds of responses to the server.
*/

static void send_xml (char *xml_data, size_t xml_len)
{
    /* Send our CGI headers back to the server. */
    fprintf(stdout, "Status: 200 OK\n");
    fprintf(stdout, "Content-type: text/plain\n\n");

    /* Blast out our data. */
    fwrite(xml_data, sizeof(char), xml_len, stdout);
}

static void send_internal_error ()
{
    /* Send an error header. */
    fprintf(stdout, "Status: 500 Internal Server Error\n");
    fprintf(stdout, "Content-type: text/html\n\n");
    
    /* Send an error message. */
    fprintf(stdout, "<title>500 Internal Server Error</title>\n");
    fprintf(stdout, "<h1>500 Internal Server Error</h1>\n");
    fprintf(stdout, "<p>Unexpected error processing XML-RPC request.</p>\n");
}


/*=========================================================================
**  die_if_fault_occurred
**=========================================================================
**  Certain kinds of errors aren't worth the trouble of generating
**  an XML-RPC fault. For these, we just send status 500 to our web server
**  and log the fault to our server log.
*/

static void die_if_fault_occurred (xmlrpc_env *env)
{
    if (env->fault_occurred) {
        fprintf(stderr, "Unexpected XML-RPC fault: %s (%d)\n",
                env->fault_string, env->fault_code);
	send_internal_error();
        exit(1);
    }
}


/*=========================================================================
**  Initialization, Cleanup & Method Registry
**=========================================================================
**  These are all related, so we group them together.
*/

static xmlrpc_registry *registry;

void xmlrpc_cgi_init (int flags)
{
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    registry = xmlrpc_registry_new(&env);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}

void xmlrpc_cgi_cleanup (void)
{
    xmlrpc_registry_free(registry);
}

void xmlrpc_cgi_add_method (char *method_name,
			    xmlrpc_method method,
			    void *user_data)
{
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method(&env, registry, NULL, method_name,
			       method, user_data);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}


/*=========================================================================
**  get_body
**=========================================================================
**  Slurp the body of the request into an xmlrpc_mem_block.
*/

static xmlrpc_mem_block *get_body (xmlrpc_env *env, size_t length)
{
    xmlrpc_mem_block *result;
    char *contents;
    size_t count;

    XMLRPC_ASSERT_ENV_OK(env);

    /* Error-handling preconditions. */
    result = NULL;

    /* XXX - Puke if length is too big. */

    /* Allocate our memory block. */
    result = xmlrpc_mem_block_new(env, length);
    XMLRPC_FAIL_IF_FAULT(env);
    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, result);

    /* Get our data off the network. */
    count = fread(contents, sizeof(char), length, stdin);
    if (count < length)
	XMLRPC_FAIL2(env, XMLRPC_INTERNAL_ERROR,
		     "Expected %z bytes, received %z", length, count);

 cleanup:
    if (env->fault_occurred) {
	if (result)
	    xmlrpc_mem_block_free(result);
	return NULL;
    }
    return result;
}


/*=========================================================================
**  xmlrpc_cgi_process_call
**=========================================================================
**  Parse the incoming XML-RPC call, find the right method, call it, and
**  serialize our response.
*/

void xmlrpc_cgi_process_call (void)
{
    xmlrpc_env env;
    char *method, *type, *length_str;
    int length;
    xmlrpc_mem_block *input, *output;
    char *input_data, *output_data;
    size_t input_size, output_size;

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    input = output = NULL;

    /* Get our HTTP information from the environment. */
    method = getenv("REQUEST_METHOD");
    type = getenv("CONTENT_TYPE");
    length_str = getenv("CONTENT_LENGTH");

    /* Perform some sanity checks. */
    if (!method || 0 != strcmp(method, "POST"))
	XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Expected HTTP method POST");
    if (!type || 0 != strcmp(type, "text/xml"))
	XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Expected text/xml content");
    if (!length_str)
	XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Content-length required");

    /* Get our content length. */
    length = atoi(length_str);
    if (length <= 0)
	XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Content-length must be > 0");

    /* Get our body. */
    input = get_body(&env, length);
    XMLRPC_FAIL_IF_FAULT(&env);
    input_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, input);
    input_size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, input);

    /* Process our call. */
    output = xmlrpc_registry_process_call(&env, registry, NULL,
					  input_data, input_size);
    XMLRPC_FAIL_IF_FAULT(&env);
    output_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output);
    output_size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);

    /* Send our data. */
    send_xml(output_data, output_size);
    
 cleanup:
    if (input)
	xmlrpc_mem_block_free(input);
    if (output)
	xmlrpc_mem_block_free(output);
    
    if (env.fault_occurred) {
	/* XXX - We should return better HTTP error codes than this does. */
	die_if_fault_occurred(&env);
    }

    xmlrpc_env_clean(&env);    
}

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

#include "xmlrpc_config.h"

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"


/*=========================================================================
**  XML-RPC Server Method Registry
**=========================================================================
**  A method registry maintains a list of functions, and handles
**  dispatching. To build an XML-RPC server, just add a communications
**  protocol. :-)
*/

xmlrpc_registry *xmlrpc_registry_new (xmlrpc_env *env)
{
    xmlrpc_value *methods;
    xmlrpc_registry *registry;

    XMLRPC_ASSERT_ENV_OK(env);
    
    /* Error-handling preconditions. */
    methods = NULL;
    registry = NULL;

    /* Allocate our memory. */
    methods = xmlrpc_struct_new(env);
    XMLRPC_FAIL_IF_FAULT(env);
    registry = (xmlrpc_registry*) malloc(sizeof(xmlrpc_registry));
    XMLRPC_FAIL_IF_NULL(registry, env, XMLRPC_INTERNAL_ERROR,
			"Could not allocate memory for registry");

    /* Set everything up. */
    registry->_methods = methods;

 cleanup:
    if (env->fault_occurred) {
	if (methods)
	    xmlrpc_DECREF(methods);
	if (registry)
	    free(registry);
	return NULL;
    }
    return registry;
}

void xmlrpc_registry_free (xmlrpc_registry *registry)
{
    XMLRPC_ASSERT_PTR_OK(registry);
    XMLRPC_ASSERT(registry->_methods != XMLRPC_BAD_POINTER);

    xmlrpc_DECREF(registry->_methods);
    registry->_methods = XMLRPC_BAD_POINTER;
    free(registry);
}


/*=========================================================================
**  xmlrpc_registry_add_method
**=========================================================================
**  See xmlrpc.h for more documentation.
*/

void xmlrpc_registry_add_method (xmlrpc_env *env,
				 xmlrpc_registry *registry,
				 char *host,
				 char *method_name,
				 xmlrpc_method method,
				 void *user_data)
{
    xmlrpc_value *method_info;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(registry);
    XMLRPC_ASSERT(host == NULL);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_PTR_OK(method);

    /* Error-handling preconditions. */
    method_info = NULL;

    /* Store our method and user data into our hash table. */
    method_info = xmlrpc_build_value(env, "(pp)", (void*) method, user_data);
    XMLRPC_FAIL_IF_FAULT(env);
    xmlrpc_struct_set_value(env, registry->_methods, method_name, method_info);
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (method_info)
	xmlrpc_DECREF(method_info);

}


/*=========================================================================
**  xmlrpc_registry_process_call
**=========================================================================
**  See xmlrpc.h for more documentation.
**
**  We should really add support for default handlers and other niftyness.
*/

xmlrpc_mem_block *xmlrpc_registry_process_call (xmlrpc_env *env2,
						xmlrpc_registry *registry,
						char *host,
						char *xml_data,
						size_t xml_len)
{
    xmlrpc_env fault;
    xmlrpc_mem_block *output;
    char *method_name;
    xmlrpc_value *param_array, *result, *method_info;
    void *method_ptr, *user_data;
    xmlrpc_method method;

    XMLRPC_ASSERT_ENV_OK(env2);
    XMLRPC_ASSERT_PTR_OK(xml_data);
    
    /* Error-handling preconditions. */
    xmlrpc_env_init(&fault);
    method_name = NULL;
    param_array = NULL;
    result = NULL;
    output = NULL;

    /* fwrite(xml_data, sizeof(char), xml_len, stderr); */

    /* Allocate our output buffer.
    ** If this fails, we need to die in a special fashion. */
    output = xmlrpc_mem_block_new(env2, 0);
    if (env2->fault_occurred)
	goto panic;

    /* Parse the call. */
    xmlrpc_parse_call(&fault, xml_data, xml_len, &method_name, &param_array);
    XMLRPC_FAIL_IF_FAULT(&fault);

    /* Look up the method info for the specified method. */
    method_info = xmlrpc_struct_get_value(&fault, registry->_methods,
					  method_name);
    if (fault.fault_occurred)
	XMLRPC_FAIL1(&fault, XMLRPC_NO_SUCH_METHOD_ERROR,
		     "Method %s not defined", method_name);

    /* Extract our method information. */
    xmlrpc_parse_value(&fault, method_info, "(pp)", &method_ptr, &user_data);
    XMLRPC_FAIL_IF_FAULT(&fault);
    method = (xmlrpc_method) method_ptr;

    /* Call the method. */
    result = (*method)(&fault, param_array, user_data);
    XMLRPC_ASSERT((result && !fault.fault_occurred) ||
		  (!result && fault.fault_occurred));
    XMLRPC_FAIL_IF_FAULT(&fault);

    /* Serialize the result.
    ** If this fails, we may have left stuff in the buffer, so it's easiest
    ** just to panic. */
    xmlrpc_serialize_response(env2, output, result);
    if (env2->fault_occurred)
	goto panic;

 cleanup:
    /* If an ordinary fault occurred, serialize it normally. */
    if (fault.fault_occurred) {
	xmlrpc_serialize_fault(env2, output, &fault);
	if (env2->fault_occurred)
	    goto panic;
    }

    /* Do our cleanup. */
    xmlrpc_env_clean(&fault);
    if (method_name)
	free(method_name);
    if (param_array)
	xmlrpc_DECREF(param_array);
    if (result)
	xmlrpc_DECREF(result);

    /* Return our output buffer. */
    return output;

 panic:
    /* XXX - We shut down the server (or at least this thread) if we can't
    ** serialize our output. This should only be caused by an out-of-memory
    ** error (or something similar). It's extremely difficult to do anything
    ** intelligent under these circumstances. */
    XMLRPC_FATAL_ERROR("An error occured while serializing our output");
    return NULL;    
}

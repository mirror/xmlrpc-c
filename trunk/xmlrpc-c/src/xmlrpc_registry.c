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
#include <stdlib.h>

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
    registry->_introspection_enabled = 1;
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
**  xmlrpc_registry_disable_introspection
**=========================================================================
**  See xmlrpc.h for more documentation.
*/

void xmlrpc_registry_disable_introspection (xmlrpc_registry *registry)
{
    XMLRPC_ASSERT_PTR_OK(registry);
    registry->_introspection_enabled = 0;
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
**  dispatch_call
**=========================================================================
**  An internal method which actually does the dispatch. This may get
**  prettified and exported at some point in the future.
*/

static xmlrpc_value *
dispatch_call(xmlrpc_env *env, xmlrpc_registry *registry,
	      char *method_name, xmlrpc_value *param_array)
{
    xmlrpc_value *method_info, *result;
    void *method_ptr, *user_data;
    xmlrpc_method method;

    /* Error-handling preconditions. */
    result = NULL;

    /* Look up the method info for the specified method. */
    method_info = xmlrpc_struct_get_value(env, registry->_methods,
					  method_name);
    if (env->fault_occurred)
	XMLRPC_FAIL1(env, XMLRPC_NO_SUCH_METHOD_ERROR,
		     "Method %s not defined", method_name);

    /* Extract our method information. */
    xmlrpc_parse_value(env, method_info, "(pp)", &method_ptr, &user_data);
    XMLRPC_FAIL_IF_FAULT(env);
    method = (xmlrpc_method) method_ptr;

    /* Call the method. */
    result = (*method)(env, param_array, user_data);
    XMLRPC_ASSERT((result && !env->fault_occurred) ||
		  (!result && env->fault_occurred));
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (env->fault_occurred) {
	if (result)
	    xmlrpc_DECREF(result);
	return NULL;
    }
    return result;
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
    xmlrpc_value *param_array, *result;

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

    /* Perform the call. */
    result = dispatch_call(&fault, registry, method_name, param_array);
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


/*=========================================================================
**  system.multicall
**=========================================================================
**  Low-tech support for transparent, boxed methods.
*/

static xmlrpc_value *
call_one_method (xmlrpc_env *env, xmlrpc_registry *registry,
		 xmlrpc_value *method_info)
{
    xmlrpc_value *result_val, *result;
    char *method_name;
    xmlrpc_value *param_array;

    /* Error-handling preconditions. */
    result = result_val = NULL;

    /* Extract our method name and parameters. */
    xmlrpc_parse_value(env, method_info, "{s:s,s:A,*}",
		       "methodName", &method_name,
		       "params", &param_array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Watch out for a deep recursion attack. */
    if (strcmp(method_name, "system.multicall") == 0)
	XMLRPC_FAIL(env, XMLRPC_REQUEST_REFUSED_ERROR,
		    "Recursive system.multicall strictly forbidden");
    
    /* Perform the call. */
    result_val = dispatch_call(env, registry, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(env);
    
    /* Build our one-item result array. */
    result = xmlrpc_build_value(env, "(V)", result_val);
    XMLRPC_FAIL_IF_FAULT(env);
	
 cleanup:
    if (result_val)
	xmlrpc_DECREF(result_val);
    if (env->fault_occurred) {
	if (result)
	    xmlrpc_DECREF(result);
	return NULL;
    }
    return result;
}

static xmlrpc_value *
system_multicall (xmlrpc_env *env,
		  xmlrpc_value *param_array,
		  void *user_data)
{
    xmlrpc_registry *registry;
    xmlrpc_value *methlist, *methinfo, *results, *result;
    size_t size, i;
    xmlrpc_env env2;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(param_array);
    XMLRPC_ASSERT_PTR_OK(user_data);

    /* Error-handling preconditions. */
    results = result = NULL;
    xmlrpc_env_init(&env2);
    
    /* Turn our arguments into something more useful. */
    registry = (xmlrpc_registry*) user_data;
    xmlrpc_parse_value(env, param_array, "(A)", &methlist);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Create an empty result list. */
    results = xmlrpc_build_value(env, "()");
    XMLRPC_FAIL_IF_FAULT(env);

    /* Loop over our input list, calling each method in turn. */
    size = xmlrpc_array_size(env, methlist);
    XMLRPC_ASSERT_ENV_OK(env);
    for (i = 0; i < size; i++) {
	methinfo = xmlrpc_array_get_item(env, methlist, i);
	XMLRPC_ASSERT_ENV_OK(env);

	/* Call our method. */
	xmlrpc_env_clean(&env2);
	xmlrpc_env_init(&env2);
	result = call_one_method(&env2, registry, methinfo);

	/* Turn any fault into a structure. */
	if (env2.fault_occurred) {
	    XMLRPC_ASSERT(result == NULL);
	    result = 
		xmlrpc_build_value(env, "{s:i,s:s}",
				   "faultCode", (xmlrpc_int32) env2.fault_code,
				   "faultString", env2.fault_string);
	    XMLRPC_FAIL_IF_FAULT(env);
	}

	/* Append this method result to our master array. */
	xmlrpc_array_append_item(env, results, result);
	xmlrpc_DECREF(result);
	result = NULL;
	XMLRPC_FAIL_IF_FAULT(env);
    }

 cleanup:
    xmlrpc_env_clean(&env2);
    if (result)
	xmlrpc_DECREF(result);
    if (env->fault_occurred) {
	if (results)
	    xmlrpc_DECREF(results);
	return NULL;
    }
    return results;
}


/*=========================================================================
**  system.listMethods
**=========================================================================
**  List all available methods by name.
*/

static xmlrpc_value *
system_listMethods (xmlrpc_env *env,
		    xmlrpc_value *param_array,
		    void *user_data)
{
    xmlrpc_registry *registry;
    xmlrpc_value *method_names, *method_name, *method_info;
    size_t size, i;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(param_array);
    XMLRPC_ASSERT_PTR_OK(user_data);

    /* Error-handling preconditions. */
    method_names = NULL;

    /* Turn our arguments into something more useful. */
    registry = (xmlrpc_registry*) user_data;
    xmlrpc_parse_value(env, param_array, "()");
    XMLRPC_FAIL_IF_FAULT(env);

    /* Make sure we're allowed to introspect. */
    if (!registry->_introspection_enabled)
	XMLRPC_FAIL(env, XMLRPC_INTROSPECTION_DISABLED_ERROR,
		    "Introspection disabled for security reasons");

    /* Iterate over all the methods in the registry, adding their names
    ** to a list. */
    method_names = xmlrpc_build_value(env, "()");
    XMLRPC_FAIL_IF_FAULT(env);
    size = xmlrpc_struct_size(env, registry->_methods);
    XMLRPC_FAIL_IF_FAULT(env);
    for (i = 0; i < size; i++) {
	xmlrpc_struct_get_key_and_value(env, registry->_methods, i,
					&method_name, &method_info);
	XMLRPC_FAIL_IF_FAULT(env);
	xmlrpc_array_append_item(env, method_names, method_name);
	XMLRPC_FAIL_IF_FAULT(env);
    }

 cleanup:
    if (env->fault_occurred) {
	if (method_names)
	    xmlrpc_DECREF(method_names);
	return NULL;
    }
    return method_names;
}


/*=========================================================================
**  system.methodHelp
**=========================================================================
**  Get the help string for a particular method.
*/

static xmlrpc_value *
system_methodHelp (xmlrpc_env *env,
		   xmlrpc_value *param_array,
		   void *user_data)
{
    xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, "Not implemented");
    return NULL;
}


/*=========================================================================
**  system.methodSignature
**=========================================================================
**  Return an array of arrays describing possible signatures for this
**  method.
*/

static xmlrpc_value *
system_methodSignature (xmlrpc_env *env,
			xmlrpc_value *param_array,
			void *user_data)
{
    xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, "Not implemented");
    return NULL;
}


/*=========================================================================
**  xmlrpc_registry_install_system_methods
**=========================================================================
**  Install the standard methods under system.*.
**  This particular function is highly experimental, and may disappear
**  without warning.
*/

void
xmlrpc_registry_install_system_methods (xmlrpc_env *env,
					xmlrpc_registry *registry)
{
    xmlrpc_registry_add_method(env, registry, NULL, "system.listMethods",
			       &system_listMethods, registry);
    XMLRPC_FAIL_IF_FAULT(env);
    xmlrpc_registry_add_method(env, registry, NULL, "system.methodSignature",
			       &system_methodSignature, registry);
    XMLRPC_FAIL_IF_FAULT(env);
    xmlrpc_registry_add_method(env, registry, NULL, "system.methodHelp",
			       &system_methodHelp, registry);
    XMLRPC_FAIL_IF_FAULT(env);
    xmlrpc_registry_add_method(env, registry, NULL, "system.multicall",
			       &system_multicall, registry);
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
}

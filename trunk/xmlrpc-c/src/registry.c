/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "system_method.h"

#include "registry.h"

/*=========================================================================
  XML-RPC Server Method Registry
===========================================================================
  A method registry is a list of XML-RPC methods for a server to
  implement, along with the details of how to implement each -- most
  notably a function pointer for a function that executes the method.

  To build an XML-RPC server, just add a communication facility.

  The registry object consists principally of an xmlrpc_value.  That
  xmlrpc_value is a struct, with method name as key.  Each value in
  the struct a "method info" array.  A method info array has the
  following items:

     0: cptr: method function ptr, Type 1
     1: cptr: user data
     2: array: signature list
     3: string: help text.
     4: cptr: method function ptr, Type 2

  One of the method funciton pointers is null and the other isn't.
  (The reason there are two is backward compatibility.  Old programs
  set up the registry with Type 1; modern ones set it up with Type 2).
    
  The signature list array contains one item for each form of call (a
  single method might have multiple forms, e.g. one takes two integer
  arguments; another takes a single string).  The array for each form
  represents a signature.  It has an item for each parameter and one
  for the result.  Item 0 is for the result, the rest are in order of
  the parameters.  Each item of that array is the name of the XML-RPC
  element that represents that type, e.g.  "int" or
  "dateTime.iso8601".

  Example signature:

    (("int"
      "double"
      "double"
     )
     ("int"
     )
    )

  The signature list array is empty to indicate that there is no signature
  information in the registry (it doesn't mean there are no valid forms
  of calling the method -- just that the registry declines to state).

=========================================================================*/



xmlrpc_registry *
xmlrpc_registry_new(xmlrpc_env * const envP) {

    xmlrpc_registry * registryP;

    XMLRPC_ASSERT_ENV_OK(envP);
    
    MALLOCVAR(registryP);

    if (registryP == NULL)
        xmlrpc_faultf(envP, "Could not allocate memory for registry");
    else {
        registryP->_introspection_enabled = true;
        registryP->_default_method        = NULL;
        registryP->_preinvoke_method      = NULL;
        registryP->_shutdown_server_fn    = NULL;

        registryP->_methods = xmlrpc_struct_new(envP);
        if (!envP->fault_occurred) {
            xmlrpc_installSystemMethods(envP, registryP);
        }    
        if (envP->fault_occurred)
            free(registryP);
    }
    return registryP;
}



void 
xmlrpc_registry_free(xmlrpc_registry * const registryP) {

    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT(registryP->_methods != XMLRPC_BAD_POINTER);

    xmlrpc_DECREF(registryP->_methods);

    if (registryP->_default_method != NULL)
        xmlrpc_DECREF(registryP->_default_method);

    if (registryP->_preinvoke_method != NULL)
        xmlrpc_DECREF(registryP->_preinvoke_method);

    free(registryP);
}



static void 
registryAddMethod(xmlrpc_env *      const envP,
                  xmlrpc_registry * const registryP,
                  const char *      const methodName,
                  xmlrpc_method1          method1,
                  xmlrpc_method2          method2,
                  const char *      const signatureString,
                  const char *      const help,
                  void *            const serverInfoP) {

    const char * const helpString =
        help ? help : "No help is available for this method.";

    xmlrpc_env env;
    xmlrpc_value * signatureListP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT(method1 != NULL || method2 != NULL);

    xmlrpc_env_init(&env);

    xmlrpc_buildSignatureArray(&env, signatureString, &signatureListP);
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Can't interpret signature string '%s'.  %s",
                      signatureString, env.fault_string);
    else {
        xmlrpc_value * methodInfoP;

        XMLRPC_ASSERT_VALUE_OK(signatureListP);

        methodInfoP = xmlrpc_build_value(
            envP, "(ppAsp)", method1, serverInfoP,
            signatureListP, helpString, method2);
        if (!envP->fault_occurred) {
            xmlrpc_struct_set_value(envP, registryP->_methods,
                                    methodName, methodInfoP);

            xmlrpc_DECREF(methodInfoP);
        }
        xmlrpc_DECREF(signatureListP);
    }
    xmlrpc_env_clean(&env);
}



void 
xmlrpc_registry_add_method_w_doc(
    xmlrpc_env *      const envP,
    xmlrpc_registry * const registryP,
    const char *      const host ATTR_UNUSED,
    const char *      const methodName,
    xmlrpc_method1    const method,
    void *            const serverInfo,
    const char *      const signatureString,
    const char *      const help) {

    XMLRPC_ASSERT(host == NULL);

    registryAddMethod(envP, registryP, methodName, method, NULL,
                      signatureString, help, serverInfo);
}



void 
xmlrpc_registry_add_method(xmlrpc_env *      const envP,
                           xmlrpc_registry * const registryP,
                           const char *      const host,
                           const char *      const methodName,
                           xmlrpc_method1          method,
                           void *            const serverInfoP) {

    xmlrpc_registry_add_method_w_doc(
        envP, registryP, host, methodName,
        method, serverInfoP, "?", "No help is available for this method.");
}



void
xmlrpc_registry_add_method2(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP,
                            const char *      const methodName,
                            xmlrpc_method2          method,
                            const char *      const signatureString,
                            const char *      const help,
                            void *            const serverInfo) {

    registryAddMethod(envP, registryP, methodName, NULL, method,
                      signatureString, help, serverInfo);
}



void 
xmlrpc_registry_set_default_method(xmlrpc_env *          const envP,
                                   xmlrpc_registry *     const registryP,
                                   xmlrpc_default_method       handler,
                                   void *                const serverInfoP) {

    xmlrpc_value * methodInfoP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(handler);

    methodInfoP = xmlrpc_build_value(envP, "(pp)", (void*) handler,
                                     serverInfoP);
    if (!envP->fault_occurred) {
        /* First dispose of any pre-existing default method */
        if (registryP->_default_method)
            xmlrpc_DECREF(registryP->_default_method);

        registryP->_default_method = methodInfoP;
    }    
}




void 
xmlrpc_registry_set_preinvoke_method(xmlrpc_env *      const envP,
                                     xmlrpc_registry * const registryP,
                                     xmlrpc_preinvoke_method handler,
                                     void *            const serverInfo) {

    xmlrpc_value * methodInfoP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(handler);

    /* Store our method and user data into our hash table. */
    methodInfoP = xmlrpc_build_value(envP, "(pp)",
                                     (void*) handler, serverInfo);
    if (!envP->fault_occurred) {
        if (registryP->_preinvoke_method)
            xmlrpc_DECREF(registryP->_preinvoke_method);
        registryP->_preinvoke_method = methodInfoP;
    }
}



void
xmlrpc_registry_set_shutdown(xmlrpc_registry *           const registryP,
                             xmlrpc_server_shutdown_fn * const shutdownFn,
                             void *                      const context) {

    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(shutdownFn);

    registryP->_shutdown_server_fn = shutdownFn;

    registryP->_shutdown_context = context;
}



static void
callPreinvokeMethodIfAny(xmlrpc_env *      const envP,
                         xmlrpc_registry * const registryP,
                         const char *      const methodName,
                         xmlrpc_value *    const paramArrayP) {

    /* Get the preinvoke method, if it is set. */
    if (registryP->_preinvoke_method) {
        xmlrpc_preinvoke_method preinvoke_method;
        void * serverInfo;

        xmlrpc_parse_value(envP, registryP->_preinvoke_method, "(pp)",
                           &preinvoke_method, &serverInfo);
        if (!envP->fault_occurred)
            (*preinvoke_method)(envP, methodName,
                                paramArrayP, serverInfo);
    }
}



static void
callNamedMethod(xmlrpc_env *    const envP,
                xmlrpc_value *  const methodInfoP,
                xmlrpc_value *  const paramArrayP,
                void *          const callInfoP,
                xmlrpc_value ** const resultPP) {

    xmlrpc_method1 methodType1;
    xmlrpc_method2 methodType2;
    void * serverInfoP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_parse_value(&env, methodInfoP, "(pp--p*)",
                       &methodType1, &serverInfoP, &methodType2);
    if (envP->fault_occurred)
        xmlrpc_faultf(envP, "Internal method registry error.  Couldn't "
                      "parse method info.  %s", env.fault_string);
    else {
        if (methodType2)
            *resultPP = (*methodType2)(envP, paramArrayP,
                                       serverInfoP, callInfoP);
        else {
            assert(methodType1);
            *resultPP = (*methodType1)(envP, paramArrayP, serverInfoP);
        }
    }
    xmlrpc_env_clean(&env);
}



static void
callDefaultMethod(xmlrpc_env *    const envP,
                  xmlrpc_value *  const defaultMethodInfo,
                  const char *    const methodName,
                  xmlrpc_value *  const paramArrayP,
                  void *          const callInfoP,
                  xmlrpc_value ** const resultPP) {

    xmlrpc_default_method defaultMethod;
    void * serverInfo;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_parse_value(envP, defaultMethodInfo, "(pp)",
                       &defaultMethod, &serverInfo);
    if (envP->fault_occurred)
        xmlrpc_faultf(envP, "Internal method registry error.  Couldn't "
                      "parse default method info.  %s", env.fault_string);
    else
        *resultPP = (*defaultMethod)(envP, callInfoP, methodName,
                                     paramArrayP, serverInfo);

    xmlrpc_env_clean(&env);
}
    


void
xmlrpc_dispatchCall(xmlrpc_env *      const envP, 
                    xmlrpc_registry * const registryP,
                    const char *      const methodName, 
                    xmlrpc_value *    const paramArrayP,
                    void *            const callInfoP,
                    xmlrpc_value **   const resultPP) {

    callPreinvokeMethodIfAny(envP, registryP, methodName, paramArrayP);
    if (!envP->fault_occurred) {
        xmlrpc_value * methodInfoP;

        xmlrpc_struct_find_value(envP, registryP->_methods,
                                 methodName, &methodInfoP);
        if (!envP->fault_occurred) {
            if (methodInfoP) {
                callNamedMethod(envP, methodInfoP, paramArrayP, callInfoP,
                                resultPP);
                xmlrpc_DECREF(methodInfoP);
            } else {
                if (registryP->_default_method)
                    callDefaultMethod(envP, registryP->_default_method, 
                                      methodName, paramArrayP, callInfoP,
                                      resultPP);
                else {
                    /* No matching method, and no default. */
                    xmlrpc_env_set_fault_formatted(
                        envP, XMLRPC_NO_SUCH_METHOD_ERROR,
                        "Method '%s' not defined", methodName);
                }
            } 
        }
    }
    /* For backward compatibility, for sloppy users: */
    if (envP->fault_occurred)
        *resultPP = NULL;
}



/*=========================================================================
**  xmlrpc_registry_process_call
**=========================================================================
**
*/

void
xmlrpc_registry_process_call2(xmlrpc_env *        const envP,
                              xmlrpc_registry *   const registryP,
                              const char *        const callXml,
                              size_t              const callXmlLen,
                              void *              const callInfo,
                              xmlrpc_mem_block ** const responseXmlPP) {

    xmlrpc_mem_block * responseXmlP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(callXml);
    
    xmlrpc_traceXml("XML-RPC CALL", callXml, callXmlLen);

    /* Allocate our output buffer.
    ** If this fails, we need to die in a special fashion. */
    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        const char * methodName;
        xmlrpc_value * paramArrayP;
        xmlrpc_env fault;
        xmlrpc_env parseEnv;

        xmlrpc_env_init(&fault);
        xmlrpc_env_init(&parseEnv);

        xmlrpc_parse_call(&parseEnv, callXml, callXmlLen, 
                          &methodName, &paramArrayP);

        if (parseEnv.fault_occurred)
            xmlrpc_env_set_fault_formatted(
                &fault, XMLRPC_PARSE_ERROR,
                "Call XML not a proper XML-RPC call.  %s",
                parseEnv.fault_string);
        else {
            xmlrpc_value * result;
            
            xmlrpc_dispatchCall(&fault, registryP, methodName, paramArrayP,
                                callInfo, &result);

            if (!fault.fault_occurred) {
                xmlrpc_serialize_response(envP, responseXmlP, result);

                /* A comment here used to say that
                   xmlrpc_serialize_response() could fail and "leave
                   stuff in the buffer."  Don't know what that means,
                   but it sounds like something that needs to be
                   fixed.  The old code aborted the program here if
                   xmlrpc_serialize_repsonse() failed.  04.11.17 
                */
                xmlrpc_DECREF(result);
            } 
            xmlrpc_strfree(methodName);
            xmlrpc_DECREF(paramArrayP);
        }
        if (!envP->fault_occurred && fault.fault_occurred)
            xmlrpc_serialize_fault(envP, responseXmlP, &fault);

        xmlrpc_env_clean(&parseEnv);
        xmlrpc_env_clean(&fault);

        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
        else {
            *responseXmlPP = responseXmlP;
            xmlrpc_traceXml("XML-RPC RESPONSE", 
                            XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlP),
                            XMLRPC_MEMBLOCK_SIZE(char, responseXmlP));
        }
    }
}



xmlrpc_mem_block *
xmlrpc_registry_process_call(xmlrpc_env *      const envP,
                             xmlrpc_registry * const registryP,
                             const char *      const host ATTR_UNUSED,
                             const char *      const callXml,
                             size_t            const callXmlLen) {

    xmlrpc_mem_block * responseXmlP;

    xmlrpc_registry_process_call2(envP, registryP, callXml, callXmlLen, NULL,
                                  &responseXmlP);

    return responseXmlP;
}



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
** Copyright (C) 2001 by Luke Howard. All rights reserved.
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

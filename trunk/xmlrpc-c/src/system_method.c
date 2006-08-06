/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "registry.h"

#include "system_method.h"


struct systemMethodReg {
/*----------------------------------------------------------------------------
   Information needed to register a system method
-----------------------------------------------------------------------------*/
    const char *   const methodName;
    xmlrpc_method2 const methodFunction;
    const char *   const signatureString;
    const char *   const helpText;
};



void 
xmlrpc_registry_disable_introspection(xmlrpc_registry * const registryP) {

    XMLRPC_ASSERT_PTR_OK(registryP);

    registryP->_introspection_enabled = false;
}



static void
translateTypeSpecifierToName(xmlrpc_env *  const envP,
                             char          const typeSpecifier,
                             const char ** const typeNameP) {

    switch (typeSpecifier) {
    case 'i': *typeNameP = "int";              break;
    case 'b': *typeNameP = "boolean";          break;
    case 'd': *typeNameP = "double";           break;
    case 's': *typeNameP = "string";           break;
    case '8': *typeNameP = "dateTime.iso8601"; break;
    case '6': *typeNameP = "base64";           break;
    case 'S': *typeNameP = "struct";           break;
    case 'A': *typeNameP = "array";            break;
    case 'n': *typeNameP = "nil";              break;
    default:
        xmlrpc_faultf(envP, 
                      "Method registry contains invalid signature "
                      "data.  It contains the type specifier '%c'",
                      typeSpecifier);
    }
}
                


static void
parseOneTypeSpecifier(xmlrpc_env *   const envP,
                      const char *   const startP,
                      xmlrpc_value * const signatureP,
                      const char **  const nextP) {
/*----------------------------------------------------------------------------
   Parse one type specifier at 'startP' within a signature string.

   Add the appropriate item for it to the array 'signatureP'.

   Return as *nextP the location the signature string just past the
   type specifier, and also past the colon that comes after the 
   type specifier for a return value.
-----------------------------------------------------------------------------*/
    const char * typeName;
    const char * cursorP;

    cursorP = startP;
                
    translateTypeSpecifierToName(envP, *cursorP, &typeName);
    
    if (!envP->fault_occurred) {
        xmlrpc_value * typeP;
        int sigArraySize;
        
        /* Append the appropriate string to the signature. */
        typeP = xmlrpc_string_new(envP, typeName);
        xmlrpc_array_append_item(envP, signatureP, typeP);
        xmlrpc_DECREF(typeP);
        
        ++cursorP; /* move past the type specifier */
        
        sigArraySize = xmlrpc_array_size(envP, signatureP);
        if (!envP->fault_occurred) {
            if (sigArraySize == 1) {
                /* We parsed off the result type, so we should now
                   see the colon that separates the result type from
                   the parameter types
                */
                if (*cursorP != ':')
                    xmlrpc_faultf(envP, "No colon (':') after "
                                  "the result type specifier");
                else
                    ++cursorP;
            }
        }
    }
    *nextP = cursorP;
}



static void
parseOneSignature(xmlrpc_env *    const envP,
                  const char *    const startP,
                  xmlrpc_value ** const signaturePP,
                  const char **   const nextP) {
/*----------------------------------------------------------------------------
   Parse one signature from the signature string that starts at 'startP'.

   Return that signature as an array xmlrpc_value pointer
   *signaturePP.  The array has one element for the return value,
   followed by one element for each parameter described in the
   signature.  That element is a string naming the return value or
   parameter type, e.g. "int".

   Return as *nextP the location in the signature string of the next
   signature (i.e. right after the next comma).  If there is no next
   signature (the string ends before any comma), make it point to the
   terminating NUL.
-----------------------------------------------------------------------------*/
    xmlrpc_value * signatureP;

    signatureP = xmlrpc_array_new(envP);  /* Start with empty array */
    if (!envP->fault_occurred) {
        const char * cursorP;

        cursorP = startP;  /* start at the beginning */

        while (!envP->fault_occurred && *cursorP != ',' && *cursorP != '\0')
            parseOneTypeSpecifier(envP, cursorP, signatureP, &cursorP);

        if (!envP->fault_occurred) {
            if (xmlrpc_array_size(envP, signatureP) < 1)
                xmlrpc_faultf(envP, "empty signature (a signature "
                              "must have at least  return value type)");
            if (*cursorP != '\0') {
                assert(*cursorP == ',');
                ++cursorP;
            }
            *nextP = cursorP;
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(signatureP);
        else
            *signaturePP = signatureP;
    }
}    



void
xmlrpc_buildSignatureArray(xmlrpc_env *    const envP,
                           const char *    const sigListString,
                           xmlrpc_value ** const resultPP) {
/*----------------------------------------------------------------------------
  Turn the signature string 'sig' (e.g. "ii,s") into an array
  as *resultP.  The array contains one element for each signature in
  the string.  (Signatures are separated by commas.  The "ii,s" example
  is two signatures: "ii" and "s").  Each element is itself an array
  as described under parseOneSignature().

  If 'sigListString' is NULL, make an empty array.
-----------------------------------------------------------------------------*/
    xmlrpc_value * signatureListP;

    signatureListP = xmlrpc_array_new(envP);
    if (!envP->fault_occurred) {
        if (sigListString == NULL || xmlrpc_streq(sigListString, "?")) {
            /* No signatures -- leave the array empty */
        } else {
            const char * cursorP;
            
            cursorP = &sigListString[0];
            
            while (!envP->fault_occurred && *cursorP != '\0') {
                xmlrpc_value * signatureP;
                
                parseOneSignature(envP, cursorP, &signatureP, &cursorP);
                
                /* cursorP now points at next signature in the list or the
                   terminating NUL.
                */
                
                if (!envP->fault_occurred) {
                    xmlrpc_array_append_item(envP, signatureListP, signatureP);
                    xmlrpc_DECREF(signatureP);
                }
            }
            if (!envP->fault_occurred) {
                unsigned int const arraySize = 
                    xmlrpc_array_size(envP, signatureListP);
                XMLRPC_ASSERT_ENV_OK(envP);
                if (arraySize < 1)
                    xmlrpc_faultf(envP, "Signature string is empty.");
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(signatureListP);
    }
    *resultPP = signatureListP;
}



/*=========================================================================
  system.multicall
=========================================================================*/

static void
callOneMethod(xmlrpc_env *      const envP,
              xmlrpc_registry * const registryP,
              xmlrpc_value *    const rpcDescP,
              void *            const callInfo,
              xmlrpc_value **   const resultPP) {

    const char * methodName;
    xmlrpc_value * paramArrayP;

    XMLRPC_ASSERT_ENV_OK(envP);

    if (xmlrpc_value_type(rpcDescP) != XMLRPC_TYPE_STRUCT)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR,
            "An element of the multicall array is type %u, but should "
            "be a struct (with members 'methodName' and 'params')",
            xmlrpc_value_type(rpcDescP));
    else {
        xmlrpc_decompose_value(envP, rpcDescP, "{s:s,s:A,*}",
                               "methodName", &methodName,
                               "params", &paramArrayP);
        if (!envP->fault_occurred) {
            /* Watch out for a deep recursion attack. */
            if (xmlrpc_streq(methodName, "system.multicall"))
                xmlrpc_env_set_fault_formatted(
                    envP,
                    XMLRPC_REQUEST_REFUSED_ERROR,
                    "Recursive system.multicall forbidden");
            else {
                xmlrpc_env env;
                xmlrpc_value * resultValP;

                xmlrpc_env_init(&env);
                xmlrpc_dispatchCall(&env, registryP, methodName, paramArrayP,
                                    callInfo,
                                    &resultValP);
                if (env.fault_occurred) {
                    /* Method failed, so result is a fault structure */
                    *resultPP = 
                        xmlrpc_build_value(
                            envP, "{s:i,s:s}",
                            "faultCode", (xmlrpc_int32) env.fault_code,
                            "faultString", env.fault_string);
                } else {
                    *resultPP = xmlrpc_build_value(envP, "(V)", resultValP);

                    xmlrpc_DECREF(resultValP);
                }
                xmlrpc_env_clean(&env);
            }
            xmlrpc_DECREF(paramArrayP);
            xmlrpc_strfree(methodName);
        }
    }
}



static void
getMethListFromMulticallPlist(xmlrpc_env *    const envP,
                              xmlrpc_value *  const paramArrayP,
                              xmlrpc_value ** const methlistPP) {

    if (xmlrpc_array_size(envP, paramArrayP) != 1)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "system.multicall takes one parameter, which is an "
            "array, each element describing one RPC.  You "
            "supplied %u arguments", 
            xmlrpc_array_size(envP, paramArrayP));
    else {
        xmlrpc_value * methlistP;

        xmlrpc_array_read_item(envP, paramArrayP, 0, &methlistP);

        XMLRPC_ASSERT_ENV_OK(envP);

        if (xmlrpc_value_type(methlistP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR,
                "system.multicall's parameter should be an array, "
                "each element describing one RPC.  But it is type "
                "%u instead.", xmlrpc_value_type(methlistP));
        else
            *methlistPP = methlistP;

        if (envP->fault_occurred)
            xmlrpc_DECREF(methlistP);
    }
}



static xmlrpc_value *
system_multicall(xmlrpc_env *   const envP,
                 xmlrpc_value * const paramArrayP,
                 void *         const serverInfo,
                 void *         const callInfo) {

    xmlrpc_registry * registryP;
    xmlrpc_value * resultsP;
    xmlrpc_value * methlistP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_ARRAY_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    resultsP = NULL;  /* defeat compiler warning */

    /* Turn our arguments into something more useful. */
    registryP = (xmlrpc_registry*) serverInfo;

    getMethListFromMulticallPlist(envP, paramArrayP, &methlistP);
    if (!envP->fault_occurred) {
        /* Create an initially empty result list. */
        resultsP = xmlrpc_array_new(envP);
        if (!envP->fault_occurred) {
            /* Loop over our input list, calling each method in turn. */
            unsigned int const methodCount =
                xmlrpc_array_size(envP, methlistP);
            unsigned int i;
            for (i = 0; i < methodCount && !envP->fault_occurred; ++i) {
                xmlrpc_value * const methinfoP = 
                    xmlrpc_array_get_item(envP, methlistP, i);
            
                xmlrpc_value * resultP;
            
                XMLRPC_ASSERT_ENV_OK(envP);
            
                callOneMethod(envP, registryP, methinfoP, callInfo, &resultP);
            
                if (!envP->fault_occurred) {
                    /* Append this method result to our master array. */
                    xmlrpc_array_append_item(envP, resultsP, resultP);
                    xmlrpc_DECREF(resultP);
                }
            }
            if (envP->fault_occurred)
                xmlrpc_DECREF(resultsP);
            xmlrpc_DECREF(methlistP);
        }
    }
    return resultsP;
}



static struct systemMethodReg const multicall = {
    "system.multicall",
    &system_multicall,
    "A:A",
    "Process an array of calls, and return an array of results.  Calls should "
    "be structs of the form {'methodName': string, 'params': array}. Each "
    "result will either be a single-item array containg the result value, or "
    "a struct of the form {'faultCode': int, 'faultString': string}.  This "
    "is useful when you need to make lots of small calls without lots of "
    "round trips.",
};


/*=========================================================================
   system.listMethods
=========================================================================*/


static xmlrpc_value *
system_listMethods(xmlrpc_env *   const env,
                   xmlrpc_value * const param_array,
                   void *         const serverInfo,
                   void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry *registry;
    xmlrpc_value *method_names, *method_name, *method_info;
    size_t size, i;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(param_array);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    /* Error-handling preconditions. */
    method_names = NULL;

    /* Turn our arguments into something more useful. */
    registry = (xmlrpc_registry*) serverInfo;
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

static struct systemMethodReg const listMethods = {
    "system.listMethods",
    &system_listMethods,
    "A:",
    "Return an array of all available XML-RPC methods on this server.",
};



/*=========================================================================
  system.methodHelp
=========================================================================*/


static xmlrpc_value *
system_methodHelp(xmlrpc_env *   const env,
                  xmlrpc_value * const param_array,
                  void *         const serverInfo,
                  void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry *registry;
    char *method_name;
    xmlrpc_value *ignored1, *ignored2, *ignored3, *help;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(param_array);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    /* Turn our arguments into something more useful. */
    registry = (xmlrpc_registry*) serverInfo;
    xmlrpc_parse_value(env, param_array, "(s)", &method_name);
    XMLRPC_FAIL_IF_FAULT(env);
    
    /* Make sure we're allowed to introspect. */
    if (!registry->_introspection_enabled)
        XMLRPC_FAIL(env, XMLRPC_INTROSPECTION_DISABLED_ERROR,
                    "Introspection disabled for security reasons");
    
    /* Get our documentation string. */
    xmlrpc_parse_value(env, registry->_methods, "{s:(VVVV*),*}",
                       method_name, &ignored1, &ignored2, &ignored3, &help);
    XMLRPC_FAIL_IF_FAULT(env);
    
 cleanup:
    if (env->fault_occurred)
        return NULL;
    xmlrpc_INCREF(help);
    return help;
}


static struct systemMethodReg const methodHelp = {
    "system.methodHelp",
    &system_methodHelp,
    "s:s",
    "Given the name of a method, return a help string.",
};



static void
getMethodInfo(xmlrpc_env *      const envP,
              xmlrpc_registry * const registryP,
              const char *      const methodName,
              xmlrpc_value **   const methodInfoPP) {
/*----------------------------------------------------------------------------
   Look up the method info for the named method.  Method info
   is an array (ppss):
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * methodInfoP;
    
    xmlrpc_env_init(&env);
    
    xmlrpc_struct_find_value(&env, registryP->_methods, methodName,
                             &methodInfoP);
    
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Unable to look up method named '%s' in the "
                      "registry.  %s", methodName, env.fault_string);
    else {
        if (methodInfoP == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_NO_SUCH_METHOD_ERROR,
                "Method '%s' does not exist", methodName);
    }
    *methodInfoPP = methodInfoP;

    xmlrpc_env_clean(&env);
}



/*=========================================================================
  system.methodSignature
==========================================================================*/

static void
buildNoSigSuppliedResult(xmlrpc_env *    const envP,
                         xmlrpc_value ** const resultPP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    *resultPP = xmlrpc_string_new(&env, "undef");
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Unable to construct 'undef'.  %s",
                      env.fault_string);

    xmlrpc_env_clean(&env);
}
    


static void
getSignatureList(xmlrpc_env *      const envP,
                 xmlrpc_registry * const registryP,
                 const char *      const methodName,
                 xmlrpc_value **   const signatureListPP) {
/*----------------------------------------------------------------------------
  Get the signature list array for method named 'methodName' from registry
  'registryP'.

  If there is no signature information for the method in the registry,
  return *signatureListPP == NULL.

  Nonexistent method is considered a failure.
-----------------------------------------------------------------------------*/
    xmlrpc_value * methodInfoP;

    getMethodInfo(envP, registryP, methodName, &methodInfoP);
    if (!envP->fault_occurred) {
        xmlrpc_env env;
        xmlrpc_value * signatureListP;
        
        xmlrpc_env_init(&env);
        
        xmlrpc_array_read_item(&env, methodInfoP, 2, &signatureListP);

        if (env.fault_occurred)
            xmlrpc_faultf(envP, "Failed to read signature list "
                          "from method info array.  %s",
                          env.fault_string);
        else {
            int arraySize;

            arraySize = xmlrpc_array_size(&env, signatureListP);
            if (env.fault_occurred)
                xmlrpc_faultf(envP, "xmlrpc_array_size() on signature "
                              "list array failed!  %s", env.fault_string);
            else {
                if (arraySize == 0)
                    *signatureListPP = NULL;
                else {
                    *signatureListPP = signatureListP;
                    xmlrpc_INCREF(signatureListP);
                }
            }
            xmlrpc_DECREF(signatureListP);
        }
        xmlrpc_env_clean(&env);
        xmlrpc_DECREF(methodInfoP);
    }
}



static xmlrpc_value *
system_methodSignature(xmlrpc_env *   const envP,
                       xmlrpc_value * const paramArrayP,
                       void *         const serverInfo,
                       void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry * const registryP = (xmlrpc_registry *) serverInfo;

    xmlrpc_value * retvalP;
    const char * methodName;
    xmlrpc_env env;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_env_init(&env);

    /* Turn our arguments into something more useful. */
    xmlrpc_decompose_value(&env, paramArrayP, "(s)", &methodName);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid parameter list.  %s", env.fault_string);
    else {
        if (!registryP->_introspection_enabled)
            xmlrpc_env_set_fault(envP, XMLRPC_INTROSPECTION_DISABLED_ERROR,
                                 "Introspection disabled on this server");
        else {
            xmlrpc_value * signatureListP;

            getSignatureList(envP, registryP, methodName, &signatureListP);

            if (!envP->fault_occurred) {
                if (signatureListP)
                    retvalP = signatureListP;
                else
                    buildNoSigSuppliedResult(envP, &retvalP);
            }
        }
        xmlrpc_strfree(methodName);
    }
    xmlrpc_env_clean(&env);

    return retvalP;
}



static struct systemMethodReg const methodSignature = {
    "system.methodSignature",
    &system_methodSignature,
    "s:s",
    "Given the name of a method, return an array of legal signatures. "
    "Each signature is an array of strings.  The first item of each signature "
    "is the return type, and any others items are parameter types.",
};




/*=========================================================================
  system.shutdown
==========================================================================*/

static xmlrpc_value *
system_shutdown(xmlrpc_env *   const envP,
                xmlrpc_value * const paramArrayP,
                void *         const serverInfo,
                void *         const callInfo ATTR_UNUSED) {
    
    xmlrpc_registry * const registryP = (xmlrpc_registry *) serverInfo;

    xmlrpc_value * retvalP;
    const char * comment;
    xmlrpc_env env;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_env_init(&env);

    retvalP = NULL;  /* quiet compiler warning */

    /* Turn our arguments into something more useful. */
    xmlrpc_decompose_value(&env, paramArrayP, "(s)", &comment);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid parameter list.  %s", env.fault_string);
    else {
        if (!registryP->_shutdown_server_fn)
            xmlrpc_env_set_fault(
                envP, 0, "This server program is not capable of "
                "shutting down");
        else {
            registryP->_shutdown_server_fn(
                &env, registryP->_shutdown_context, comment);

            if (env.fault_occurred)
                xmlrpc_env_set_fault(envP, env.fault_code, env.fault_string);
            else {
                retvalP = xmlrpc_int_new(&env, 0);
                
                if (env.fault_occurred)
                    xmlrpc_faultf(envP,
                                  "Failed to construct return value.  %s",
                                  env.fault_string);
            }
        }
        xmlrpc_strfree(comment);
    }
    xmlrpc_env_clean(&env);

    return retvalP;
}



static struct systemMethodReg const shutdown = {
    "system.shutdown",
    &system_shutdown,
    "i:s",
    "Shut down the server.  Return code is always zero.",
};



/*============================================================================
  Installer of system methods
============================================================================*/

static void
registerSystemMethod(xmlrpc_env *           const envP,
                     xmlrpc_registry *      const registryP,
                     struct systemMethodReg const methodReg) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    
    xmlrpc_registry_add_method2(
        &env, registryP, methodReg.methodName,
        methodReg.methodFunction,
        methodReg.signatureString, methodReg.helpText, registryP);
    
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Failed to register '%s' system method.  %s",
                      methodReg.methodName, envP->fault_string);
    
    xmlrpc_env_clean(&env);
}



void
xmlrpc_installSystemMethods(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Install the built-in methods (system.*) into registry 'registryP'.
-----------------------------------------------------------------------------*/
    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, listMethods);

    if (!envP->fault_occurred) 
        registerSystemMethod(envP, registryP, methodSignature);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodHelp);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, multicall);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, shutdown);
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


/*=========================================================================
  XML-RPC Server Method Registry
===========================================================================
  These are the functions that implement the XML-RPC method registry.

  A method registry is a list of XML-RPC methods for a server to
  implement, along with the details of how to implement each -- most
  notably a function pointer for a function that executes the method.

  To build an XML-RPC server, just add a communication facility.

  Copyright information is at end of file

=========================================================================*/

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
#include "method.h"
#include "system_method.h"

#include "registry.h"


static void
signatureDestroy(struct xmlrpc_signature * const signatureP) {

    if (signatureP->argList)
        free(signatureP->argList);

    free(signatureP);
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
makeRoomInArgList(xmlrpc_env *              const envP,
                  struct xmlrpc_signature * const signatureP,
                  unsigned int              const minArgCount) {

    if (signatureP->argListSpace < minArgCount) {
        REALLOCARRAY(signatureP->argList, minArgCount);
        if (signatureP->argList == NULL) {
            xmlrpc_faultf(envP, "Couldn't get memory for a argument list for "
                          "a method signature with %u arguments", minArgCount);
            signatureP->argListSpace = 0;
        }
    }
}



static void
parseArgumentTypeSpecifiers(xmlrpc_env *              const envP,
                            const char *              const startP,
                            struct xmlrpc_signature * const signatureP,
                            const char **             const nextPP) {
    const char * cursorP;

    cursorP = startP;  /* start at the beginning */

    while (!envP->fault_occurred && *cursorP != ',' && *cursorP != '\0') {
        const char * typeName;

        translateTypeSpecifierToName(envP, *cursorP, &typeName);

        if (!envP->fault_occurred) {
            ++cursorP;

            makeRoomInArgList(envP, signatureP, signatureP->argCount + 1);

            signatureP->argList[signatureP->argCount++] = typeName;
        }
    }
    if (!envP->fault_occurred) {
        if (*cursorP) {
            XMLRPC_ASSERT(*cursorP == ',');
            ++cursorP;  /* Move past the signature and comma */
        }
    }
    if (envP->fault_occurred) 
        free(signatureP->argList);

    *nextPP = cursorP;
}



static void
parseOneSignature(xmlrpc_env *               const envP,
                  const char *               const startP,
                  struct xmlrpc_signature ** const signaturePP,
                  const char **              const nextPP) {
/*----------------------------------------------------------------------------
   Parse one signature from the signature string that starts at 'startP'.

   Return that signature as a signature object *signaturePP.

   Return as *nextP the location in the signature string of the next
   signature (i.e. right after the next comma).  If there is no next
   signature (the string ends before any comma), make it point to the
   terminating NUL.
-----------------------------------------------------------------------------*/
    struct xmlrpc_signature * signatureP;

    MALLOCVAR(signatureP);
    if (signatureP == NULL)
        xmlrpc_faultf(envP, "Couldn't get memory for signature");
    else {
        const char * cursorP;

        signatureP->argListSpace = 0;  /* Start with no argument space */
        signatureP->argList = NULL;   /* Nothing allocated yet */
        signatureP->argCount = 0;  /* Start with no arguments */

        cursorP = startP;  /* start at the beginning */

        if (*cursorP == ',' || *cursorP == '\0')
            xmlrpc_faultf(envP, "empty signature (a signature "
                          "must have at least  return value type)");
        else {
            translateTypeSpecifierToName(envP, *cursorP, &signatureP->retType);

            ++cursorP;

            if (*cursorP != ':')
                xmlrpc_faultf(envP, "No colon (':') after "
                              "the result type specifier");
            else {
                ++cursorP;

                parseArgumentTypeSpecifiers(envP, cursorP, signatureP, nextPP);
            }
        }
        if (envP->fault_occurred)
            free(signatureP);
        else
            *signaturePP = signatureP;
    }
}    



static void
destroySignatures(struct xmlrpc_signature * const firstSignatureP) {

    struct xmlrpc_signature * p;
    struct xmlrpc_signature * nextP;

    for (p = firstSignatureP; p; p = nextP) {
        nextP = p->nextP;
        signatureDestroy(p);
    }
}



static void
listSignatures(xmlrpc_env *               const envP,
               const char *               const sigListString,
               struct xmlrpc_signature ** const firstSignaturePP) {
    
    struct xmlrpc_signature ** p;
    const char * cursorP;

    *firstSignaturePP = NULL;  /* Start with empty list */
    
    p = firstSignaturePP;
    cursorP = &sigListString[0];
    
    while (!envP->fault_occurred && *cursorP != '\0') {
        struct xmlrpc_signature * signatureP;
        
        parseOneSignature(envP, cursorP, &signatureP, &cursorP);
        
        /* cursorP now points at next signature in the list or the
           terminating NUL.
        */
        
        if (!envP->fault_occurred) {
            signatureP->nextP = NULL;
            *p = signatureP;
            p = &signatureP->nextP;
        }
    }
    if (envP->fault_occurred)
        destroySignatures(*firstSignaturePP);
}



static void
signatureListCreate(xmlrpc_env *            const envP,
                    const char *            const sigListString,
                    xmlrpc_signatureList ** const signatureListPP) {

    xmlrpc_signatureList * signatureListP;

    XMLRPC_ASSERT_ENV_OK(envP);
    
    MALLOCVAR(signatureListP);

    if (signatureListP == NULL)
        xmlrpc_faultf(envP, "Could not allocate memory for signature list");
    else {
        signatureListP->firstSignatureP = NULL;

        if (sigListString == NULL || xmlrpc_streq(sigListString, "?")) {
            /* No signatures -- leave the list empty */
        } else {
            listSignatures(envP, sigListString,
                           &signatureListP->firstSignatureP);

            if (!envP->fault_occurred) {
                if (!signatureListP->firstSignatureP)
                    xmlrpc_faultf(envP, "Signature string is empty.");

                if (envP->fault_occurred)
                    destroySignatures(signatureListP->firstSignatureP);
            }
        }
        if (envP->fault_occurred)
            free(signatureListP);

        *signatureListPP = signatureListP;
    }
}



static void
signatureListDestroy(xmlrpc_signatureList * const signatureListP) {

    destroySignatures(signatureListP->firstSignatureP);

    free(signatureListP);
}



xmlrpc_registry *
xmlrpc_registry_new(xmlrpc_env * const envP) {

    xmlrpc_registry * registryP;

    XMLRPC_ASSERT_ENV_OK(envP);
    
    MALLOCVAR(registryP);

    if (registryP == NULL)
        xmlrpc_faultf(envP, "Could not allocate memory for registry");
    else {
        registryP->introspectionEnabled  = true;
        registryP->defaultMethodFunction = NULL;
        registryP->preinvokeFunction     = NULL;
        registryP->shutdownServerFn      = NULL;
        registryP->dialect               = xmlrpc_dialect_i8;

        xmlrpc_methodListCreate(envP, &registryP->methodListP);
        if (!envP->fault_occurred)
            xmlrpc_installSystemMethods(envP, registryP);

        if (envP->fault_occurred)
            free(registryP);
    }
    return registryP;
}



void 
xmlrpc_registry_free(xmlrpc_registry * const registryP) {

    XMLRPC_ASSERT_PTR_OK(registryP);

    xmlrpc_methodListDestroy(registryP->methodListP);

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
                  void *            const userData) {

    const char * const helpString =
        help ? help : "No help is available for this method.";

    xmlrpc_env env;
    xmlrpc_signatureList * signatureListP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT(method1 != NULL || method2 != NULL);

    xmlrpc_env_init(&env);

    signatureListCreate(&env, signatureString, &signatureListP);

    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Can't interpret signature string '%s'.  %s",
                      signatureString, env.fault_string);
    else {
        xmlrpc_methodInfo * methodP;

        xmlrpc_methodCreate(&env, method1, method2, userData,
                            signatureListP, helpString, &methodP);

        if (!env.fault_occurred) {
            xmlrpc_methodListAdd(&env, registryP->methodListP, methodName,
                                 methodP);

            if (env.fault_occurred)
                xmlrpc_methodDestroy(methodP);
        }
        if (env.fault_occurred)
            signatureListDestroy(signatureListP);
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
                                   xmlrpc_default_method       function,
                                   void *                const userData) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(function);

    /* Note: this may be the first default method, or it may be a replacement
       of the current one.
    */

    registryP->defaultMethodFunction = function;
    registryP->defaultMethodUserData = userData;
}




void 
xmlrpc_registry_set_preinvoke_method(xmlrpc_env *      const envP,
                                     xmlrpc_registry * const registryP,
                                     xmlrpc_preinvoke_method function,
                                     void *            const userData) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(function);

    registryP->preinvokeFunction = function;
    registryP->preinvokeUserData = userData;
}



void
xmlrpc_registry_set_shutdown(xmlrpc_registry *           const registryP,
                             xmlrpc_server_shutdown_fn * const shutdownFn,
                             void *                      const context) {

    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(shutdownFn);

    registryP->shutdownServerFn = shutdownFn;

    registryP->shutdownContext = context;
}



void
xmlrpc_registry_set_dialect(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP,
                            xmlrpc_dialect    const dialect) {

    if (dialect != xmlrpc_dialect_i8 &&
        dialect != xmlrpc_dialect_apache)
        xmlrpc_faultf(envP, "Invalid dialect argument -- not of type "
                      "xmlrpc_dialect.  Numerical value is %u", dialect);
    else
        registryP->dialect = dialect;
}



static void
callNamedMethod(xmlrpc_env *        const envP,
                xmlrpc_methodInfo * const methodP,
                xmlrpc_value *      const paramArrayP,
                void *              const callInfoP,
                xmlrpc_value **     const resultPP) {

    if (methodP->methodFnType2)
        *resultPP =
            methodP->methodFnType2(envP, paramArrayP,
                                   methodP->userData, callInfoP);
    else {
        assert(methodP->methodFnType1);
        *resultPP =
            methodP->methodFnType1(envP, paramArrayP, methodP->userData);
    }
}



void
xmlrpc_dispatchCall(xmlrpc_env *      const envP, 
                    xmlrpc_registry * const registryP,
                    const char *      const methodName, 
                    xmlrpc_value *    const paramArrayP,
                    void *            const callInfoP,
                    xmlrpc_value **   const resultPP) {

    if (registryP->preinvokeFunction)
        registryP->preinvokeFunction(envP, methodName, paramArrayP,
                                     registryP->preinvokeUserData);

    if (!envP->fault_occurred) {
        xmlrpc_methodInfo * methodP;

        xmlrpc_methodListLookupByName(registryP->methodListP, methodName,
                                      &methodP);

        if (methodP)
            callNamedMethod(envP, methodP, paramArrayP, callInfoP, resultPP);
        else {
            if (registryP->defaultMethodFunction)
                *resultPP = registryP->defaultMethodFunction(
                    envP, callInfoP, methodName, paramArrayP,
                    registryP->defaultMethodUserData);
            else {
                /* No matching method, and no default. */
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NO_SUCH_METHOD_ERROR,
                    "Method '%s' not defined", methodName);
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
            xmlrpc_value * resultP;
            
            xmlrpc_dispatchCall(&fault, registryP, methodName, paramArrayP,
                                callInfo, &resultP);

            if (!fault.fault_occurred) {
                xmlrpc_serialize_response2(envP, responseXmlP,
                                           resultP, registryP->dialect);

                xmlrpc_DECREF(resultP);
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

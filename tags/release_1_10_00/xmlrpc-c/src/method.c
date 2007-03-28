/*=========================================================================
  XML-RPC server method registry
  Method services
===========================================================================
  These are the functions that implement the method objects that
  the XML-RPC method registry uses.

  By Bryan Henderson, December 2006.

  Contributed to the public domain by its author.
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
#include "registry.h"

#include "method.h"


void
xmlrpc_methodCreate(xmlrpc_env *           const envP,
                    xmlrpc_method1               methodFnType1,
                    xmlrpc_method2               methodFnType2,
                    void *                 const userData,
                    xmlrpc_signatureList * const signatureListP,
                    const char *           const helpText,
                    xmlrpc_methodInfo **   const methodPP) {

    xmlrpc_methodInfo * methodP;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(methodP);

    if (methodP == NULL)
        xmlrpc_faultf(envP, "Unable to allocate storage for a method "
                      "descriptor");
    else {
        methodP->methodFnType1  = methodFnType1;
        methodP->methodFnType2  = methodFnType2;
        methodP->userData       = userData;
        methodP->signatureListP = signatureListP;
        methodP->helpText       = strdup(helpText);
    
        *methodPP = methodP;
    }
}



void
xmlrpc_methodDestroy(xmlrpc_methodInfo * const methodP) {
    
    xmlrpc_strfree(methodP->helpText);

    free(methodP);
}



void
xmlrpc_methodListCreate(xmlrpc_env *         const envP,
                        xmlrpc_methodList ** const methodListPP) {

    xmlrpc_methodList * methodListP;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(methodListP);

    if (methodListP == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate method list descriptor");
    else {
        methodListP->firstMethodP = NULL;
        methodListP->lastMethodP = NULL;

        *methodListPP = methodListP;
    }
}



void
xmlrpc_methodListDestroy(xmlrpc_methodList * methodListP) {

    xmlrpc_methodNode * p;
    xmlrpc_methodNode * nextP;

    for (p = methodListP->firstMethodP; p; p = nextP) {
        nextP = p->nextP;

        xmlrpc_methodDestroy(p->methodP);
        xmlrpc_strfree(p->methodName);
        free(p);
    }

    free(methodListP);
}



void
xmlrpc_methodListLookupByName(xmlrpc_methodList *  const methodListP,
                              const char *         const methodName,
                              xmlrpc_methodInfo ** const methodPP) {


    /* We do a simple linear lookup along a linked list.
       If speed is important, we can make this a binary tree instead.
    */

    xmlrpc_methodNode * p;
    xmlrpc_methodInfo * methodP;

    for (p = methodListP->firstMethodP, methodP = NULL;
         p && !methodP;
         p = p->nextP) {

        if (xmlrpc_streq(p->methodName, methodName))
            methodP = p->methodP;
    }
    *methodPP = methodP;
}



void
xmlrpc_methodListAdd(xmlrpc_env *        const envP,
                     xmlrpc_methodList * const methodListP,
                     const char *        const methodName,
                     xmlrpc_methodInfo * const methodP) {
    
    xmlrpc_methodInfo * existingMethodP;

    XMLRPC_ASSERT_ENV_OK(envP);

    xmlrpc_methodListLookupByName(methodListP, methodName, &existingMethodP);
    
    if (existingMethodP)
        xmlrpc_faultf(envP, "Method named '%s' already registered",
                      methodName);
    else {
        xmlrpc_methodNode * methodNodeP;

        MALLOCVAR(methodNodeP);
        
        if (methodNodeP == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate method node");
        else {
            methodNodeP->methodName = strdup(methodName);
            methodNodeP->methodP = methodP;
            methodNodeP->nextP = NULL;
            
            if (!methodListP->firstMethodP)
                methodListP->firstMethodP = methodNodeP;

            if (methodListP->lastMethodP)
                methodListP->lastMethodP->nextP = methodNodeP;

            methodListP->lastMethodP = methodNodeP;
        }
    }
}


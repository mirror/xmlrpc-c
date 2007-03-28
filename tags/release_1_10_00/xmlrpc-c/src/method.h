#ifndef METHOD_H_INCLUDED
#define METHOD_H_INCLUDED

#include "xmlrpc-c/base.h"

typedef struct {
/*----------------------------------------------------------------------------
   Everything a registry knows about one XML-RPC method
-----------------------------------------------------------------------------*/
    /* One of the methodTypeX fields is NULL and the other isn't.
       (The reason there are two is backward compatibility.  Old
       programs set up the registry with Type 1; modern ones set it up
       with Type 2.
    */
    xmlrpc_method1 methodFnType1;
        /* The method function, if it's type 1.  Null if it's not */
    xmlrpc_method2 methodFnType2;
        /* The method function, if it's type 2.  Null if it's not */
    void * userData;
        /* Passed to method function */
    struct xmlrpc_signatureList * signatureListP;
        /* Stuff returned by system method system.methodSignature.
           Empty list doesn't mean there are no valid forms of calling the
           method -- just that the registry declines to state.
        */
    const char * helpText;
        /* Stuff returned by system method system.methodHelp */
} xmlrpc_methodInfo;

typedef struct xmlrpc_methodNode {
    struct xmlrpc_methodNode * nextP;
    const char * methodName;
    xmlrpc_methodInfo * methodP;
} xmlrpc_methodNode;

typedef struct xmlrpc_methodList {
    xmlrpc_methodNode * firstMethodP;
    xmlrpc_methodNode * lastMethodP;
} xmlrpc_methodList;

void
xmlrpc_methodCreate(xmlrpc_env *           const envP,
                    xmlrpc_method1               methodFnType1,
                    xmlrpc_method2               methodFnType2,
                    void *                 const userData,
                    struct xmlrpc_signatureList * const signatureListP,
                    const char *           const helpText,
                    xmlrpc_methodInfo **   const methodPP);

void
xmlrpc_methodDestroy(xmlrpc_methodInfo * const methodP);

void
xmlrpc_methodListCreate(xmlrpc_env *         const envP,
                        xmlrpc_methodList ** const methodListPP);

void
xmlrpc_methodListDestroy(xmlrpc_methodList * methodListP);

void
xmlrpc_methodListLookupByName(xmlrpc_methodList *  const methodListP,
                              const char *         const methodName,
                              xmlrpc_methodInfo ** const methodPP);

void
xmlrpc_methodListAdd(xmlrpc_env *        const envP,
                     xmlrpc_methodList * const methodListP,
                     const char *        const methodName,
                     xmlrpc_methodInfo * const methodP);



#endif

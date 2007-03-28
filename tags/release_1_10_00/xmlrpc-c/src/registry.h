#ifndef REGISTRY_H_INCLUDED
#define REGISTRY_H_INCLUDED

#include "bool.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"


struct xmlrpc_signature {
    struct xmlrpc_signature * nextP;
    const char * retType;
        /* Name of the XML-RPC element that represents the return value
           type, e.g. "int" or "dateTime.iso8601"
        */
    unsigned int argCount;
        /* Number of arguments method takes */
    unsigned int argListSpace;
        /* Number of slots that exist in the argList[] (i.e. memory is
           allocated)
        */
    const char ** argList;
        /* Array of size 'argCount'.  argList[i] is the name of the type
           of argument i.  Like 'retType', e.g. "string".

           The strings are constants, not malloc'ed.
        */
};

typedef struct xmlrpc_signatureList {
    /* A list of signatures for a method.  Each signature describes one
       alternative form of invoking the method (a
       single method might have multiple forms, e.g. one takes two integer
       arguments; another takes a single string).
    */
    struct xmlrpc_signature * firstSignatureP;
} xmlrpc_signatureList;

struct xmlrpc_registry {
    bool                        introspectionEnabled;
    struct xmlrpc_methodList *  methodListP;
    xmlrpc_default_method       defaultMethodFunction;
    void *                      defaultMethodUserData;
    xmlrpc_preinvoke_method     preinvokeFunction;
    void *                      preinvokeUserData;
    xmlrpc_server_shutdown_fn * shutdownServerFn;
        /* Function that can be called to shut down the server that is
           using this registry.  NULL if none.
        */
    void * shutdownContext;
        /* Context for _shutdown_server_fn -- understood only by
           that function, passed to it as argument.
        */
};

void
xmlrpc_dispatchCall(struct _xmlrpc_env *     const envP, 
                    struct xmlrpc_registry * const registryP,
                    const char *             const methodName, 
                    struct _xmlrpc_value *   const paramArrayP,
                    void *                   const callInfoP,
                    struct _xmlrpc_value **  const resultPP);

#endif

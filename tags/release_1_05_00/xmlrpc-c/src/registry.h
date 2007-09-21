#ifndef REGISTRY_H_INCLUDED
#define REGISTRY_H_INCLUDED

#include "bool.h"

struct _xmlrpc_registry {
    bool           _introspection_enabled;
    xmlrpc_value * _methods;
    xmlrpc_value * _default_method;
    xmlrpc_value * _preinvoke_method;
};

void
xmlrpc_dispatchCall(struct _xmlrpc_env *      const envP, 
                    struct _xmlrpc_registry * const registryP,
                    const char *              const methodName, 
                    struct _xmlrpc_value *    const paramArrayP,
                    struct _xmlrpc_value **   const resultPP);

#endif

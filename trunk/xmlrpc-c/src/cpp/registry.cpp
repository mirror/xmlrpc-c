#include <string>
#include <memory>

#include "girerr.hpp"
using girerr::throwf;
using girerr::error;
#include "xmlrpc.h"
#include "xmlrpc.hpp"
#include "registry.hpp"

using std::string;

namespace xmlrpc_c {



registry::registry() {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    this->c_registryP = xmlrpc_registry_new(&env);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



registry::~registry(void) {

    xmlrpc_registry_free(this->c_registryP);
}



static vector<xmlrpc_c::value>
vectorFromXmlrpcArray(xmlrpc_value * const arrayP) {
/*----------------------------------------------------------------------------
   Convert an XML-RPC array in C (not C++) form to an STL vector.

   This is glue code to allow us to hook up C++ Xmlrpc-c code to 
   C Xmlrpc-c code.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    unsigned int const arraySize = xmlrpc_array_size(&env, arrayP);

    vector<xmlrpc_c::value> retval;
    
    for (unsigned int i = 0; i < arraySize; ++i) {
        xmlrpc_value * arrayItemP;
        xmlrpc_array_read_item(&env, arrayP, i, &arrayItemP);

        retval[i] = xmlrpc_c::value(arrayItemP);
        xmlrpc_DECREF(arrayItemP);
    }
    return retval;
}



static xmlrpc_value *
c_executeMethod(xmlrpc_env *   const envP,
                xmlrpc_value * const paramArrayP,
                void *         const methodPtr) {
/*----------------------------------------------------------------------------
   This is a function designed to be called via a C registry to
   execute an XML-RPC method, but use a C++ method object to do the
   work.  You register this function as the method function and a
   pointer to the C++ method object as the method data in the C
   registry.

   If we had a pure C++ registry, this would be unnecessary.
-----------------------------------------------------------------------------*/
    xmlrpc_c::method * const methodP = (xmlrpc_c::method *) methodPtr;
    vector<xmlrpc_c::value> const params(vectorFromXmlrpcArray(paramArrayP));

    xmlrpc_c::value * resultP;

    try {
        methodP->execute(params, &resultP);
    } catch (xmlrpc_c::fault fault) {
        xmlrpc_env_set_fault(envP, fault.faultCode, 
                             fault.faultDescription.c_str()); 
    }
    auto_ptr<xmlrpc_c::value> autoResultP(resultP);

    return resultP->c_value();
}


xmlrpc_method const c_executeMethodP = &c_executeMethod;


void
registry::addMethod(string           const name,
                    xmlrpc_c::method const method) {

    xmlrpc_env env;
    
    xmlrpc_env_init(&env);

    // In the pure C++ version we will just add a pointer to the method
    // to a list along with the method's name.

	xmlrpc_registry_add_method_w_doc(
        &env, this->c_registryP, NULL,
        name.c_str(), &c_executeMethod, 
        (void*) &method, method.signature().c_str(), method.help().c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
registry::processCall(string    const& body,
                      string ** const  responsePP) const {
/*----------------------------------------------------------------------------
   Process an XML-RPC call whose XML (HTTP body) is 'body'.

   Return the response XML as *responsePP.  Caller must delete this string.

   If we are unable to execute the call, we throw an error.  But if
   the call executes and the method merely fails in an XML-RPC sense, we
   don't.  In that case, responseP indicates the failure.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_mem_block * output;

    xmlrpc_env_init(&env);

    // For the pure C++ version, this will have to parse the XML 'body'
    // into a method name and parameters, look up the method name in
    // the registry, call the method's execute() method, then marshall
    // the result into XML and return it as *responsePP.  This will be
    // more or less like what xmlrpc_registry_process_call() does.

    output = xmlrpc_registry_process_call(
        &env, this->c_registryP, NULL, body.c_str(), body.length());

    if (env.fault_occurred)
        throw(error(env.fault_string));

    *responsePP = new string(XMLRPC_MEMBLOCK_CONTENTS(char, output),
                             XMLRPC_MEMBLOCK_SIZE(char, output));
    
    xmlrpc_mem_block_free (output);
}



}  // namespace

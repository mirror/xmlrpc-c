#include <limits.h>
#include <string>
#include <memory>
#include <list>
#include <algorithm>

#include "girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"

using std::string;
using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


param_list::param_list(unsigned int paramCount = 0) {

    this->paramVector.reserve(paramCount);
}


 
void
param_list::add(xmlrpc_c::value const param) {

    this->paramVector.push_back(param);
}



int
param_list::getInt(unsigned int const paramNumber,
                   int          const minimum = -INT_MAX,
                   int          const maximum = INT_MAX) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_INT)
        throw(fault("Parameter that is supposed to be integer is not", 
                    fault::CODE_TYPE));

    int const intvalue(static_cast<int>(
        value_int(this->paramVector[paramNumber])));

    if (intvalue < minimum)
        throw(fault("Integer parameter too low", fault::CODE_TYPE));

    if (intvalue > maximum)
        throw(fault("Integer parameter too high", fault::CODE_TYPE));

    return intvalue;
}



bool
param_list::getBoolean(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_BOOLEAN)
        throw(fault("Parameter that is supposed to be boolean is not", 
                    fault::CODE_TYPE));

    return static_cast<bool>(value_boolean(this->paramVector[paramNumber]));
}



void
param_list::verifyEnd(unsigned int const paramNumber) const {

    if (paramNumber < this->paramVector.size())
        throw(fault("Too many parameters", fault::CODE_TYPE));
    if (paramNumber > this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));
}



method::~method() {
    if (this->refcount > 0)
        throw(error("Destroying referenced object"));
}



void
method::incref() {
    ++this->refcount;
}



void
method::decref(bool * const unreferencedP) {

    if (this->refcount == 0)
        throw(error("Decrementing ref count of unreferenced object"));
    --this->refcount;
    *unreferencedP = (this->refcount == 0);
}
 


method_ptr::method_ptr() : methodP(NULL) {};



method_ptr::method_ptr(method * const _methodP) {
    this->methodP = _methodP;
    this->methodP->incref();
}



method_ptr::method_ptr(method_ptr const& methodPtr) { // copy constructor
    this->methodP = methodPtr.methodP;
    this->methodP->incref();
}
    
 

method_ptr::~method_ptr() {
    bool dead;
    this->methodP->decref(&dead);
    if (dead)
        delete(this->methodP);
}
 

   
method_ptr
method_ptr::operator=(method_ptr const& methodPtr) {
    if (this->methodP != NULL)
        throw(error("Already instantiated"));
    this->methodP = methodPtr.methodP;
    this->methodP->incref();
    return *this;
}



method *
method_ptr::get() const {
    return methodP;
}



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



static xmlrpc_c::param_list
pListFromXmlrpcArray(xmlrpc_value * const arrayP) {
/*----------------------------------------------------------------------------
   Convert an XML-RPC array in C (not C++) form to a parameter list object
   that can be passed to a method execute method.

   This is glue code to allow us to hook up C++ Xmlrpc-c code to 
   C Xmlrpc-c code.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    unsigned int const arraySize = xmlrpc_array_size(&env, arrayP);

    xmlrpc_c::param_list paramList(arraySize);
    
    for (unsigned int i = 0; i < arraySize; ++i) {
        xmlrpc_value * arrayItemP;
        xmlrpc_array_read_item(&env, arrayP, i, &arrayItemP);

        paramList.add(xmlrpc_c::value(arrayItemP));
        
        xmlrpc_DECREF(arrayItemP);
    }
    return paramList;
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

   Since we can't throw an error back to the C code, we catch anything
   the XML-RPC method's execute() method throws, and any error we
   encounter in processing the result it returns, and turn it into an
   XML-RPC method failure.  This will cause a leak if the execute()
   method actually created a result, since it will not get destroyed.
-----------------------------------------------------------------------------*/
    xmlrpc_c::method * const methodP = 
        static_cast<xmlrpc_c::method *>(methodPtr);
    xmlrpc_c::param_list const paramList(pListFromXmlrpcArray(paramArrayP));

    xmlrpc_value * retval;

    try {
        const xmlrpc_c::value * resultP;

        try {
            methodP->execute(paramList, &resultP);
        } catch (xmlrpc_c::fault fault) {
            xmlrpc_env_set_fault(envP, fault.getCode(), 
                                 fault.getDescription().c_str()); 
        }
        if (envP->fault_occurred)
            retval = NULL;
        else {
            // The following declaration makes *resultP get destroyed
            auto_ptr<xmlrpc_c::value> 
                autoResultP(const_cast<xmlrpc_c::value *>(resultP));
            retval = resultP->c_value();
        }
    } catch (...) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Unexpected error executing the code for this "
                             "particular method, detected by the Xmlrpc-c "
                             "method registry code.  The method did not "
                             "fail; rather, it did not complete at all.");
        retval = NULL;
    }
    return retval;
}
 


void
registry::addMethod(string               const name,
                    xmlrpc_c::method_ptr const methodPtr) {


    this->methodList.push_back(methodPtr);

    xmlrpc_env env;
    
    xmlrpc_env_init(&env);

    xmlrpc_c::method * const methodP(methodPtr.get());

	xmlrpc_registry_add_method_w_doc(
        &env, this->c_registryP, NULL,
        name.c_str(), &c_executeMethod, 
        (void*) methodP, 
        methodP->signature().c_str(), methodP->help().c_str());

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

    // For the pure C++ version, this will have to parse the XML
    // 'body' into a method name and parameters, look up the method
    // name in the registry, call the method's execute() method, then
    // marshall the result into XML and return it as *responsePP.  It
    // will also have to execute system methods (e.g. self
    // description) itself.  This will be more or less like what
    // xmlrpc_registry_process_call() does.

    output = xmlrpc_registry_process_call(
        &env, this->c_registryP, NULL, body.c_str(), body.length());

    if (env.fault_occurred)
        throw(error(env.fault_string));

    *responsePP = new string(XMLRPC_MEMBLOCK_CONTENTS(char, output),
                             XMLRPC_MEMBLOCK_SIZE(char, output));
    
    xmlrpc_mem_block_free (output);
}

xmlrpc_registry *
registry::c_registry() const {

    return this->c_registryP;
}

}  // namespace

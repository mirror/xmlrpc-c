#include <string>

#include "casprintf.h"

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/xml.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {

xmlrpc_value *
cArrayFromParamList(paramList const& paramList) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_value * paramArrayP;

    paramArrayP = xmlrpc_array_new(&env);
    if (!env.fault_occurred) {
        for (unsigned int i = 0;
             i < paramList.size() && !env.fault_occurred;
             ++i) {
            
            xmlrpc_array_append_item(&env, paramArrayP,
                                     paramList[i].cValue());
        }
    }
    if (env.fault_occurred) {
        xmlrpc_DECREF(paramArrayP);
        throw(error(env.fault_string));
    }
    return paramArrayP;
}

} // namespace


namespace xmlrpc_c {
namespace xml {


void
generateCall(string    const& methodName,
             paramList const& paramList,
             string *  const  callXmlP) {
/*----------------------------------------------------------------------------
   Generate the XML for an XML-RPC call, given a method name and parameter
   list.
-----------------------------------------------------------------------------*/
    class memblockWrapper {
        xmlrpc_mem_block * const memblockP;
    public:
        memblockWrapper(xmlrpc_mem_block * const memblockP) :
            memblockP(memblockP) {}

        ~memblockWrapper() {
            XMLRPC_MEMBLOCK_FREE(char, memblockP);
        }
    };

    xmlrpc_mem_block * callXmlMP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    callXmlMP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    if (!env.fault_occurred) {
        memblockWrapper callXmlHolder(callXmlMP);
            // Makes callXmlMP get freed at end of scope

        xmlrpc_value * const paramArrayP(cArrayFromParamList(paramList));

        xmlrpc_serialize_call(&env, callXmlMP, methodName.c_str(),
                              paramArrayP);
        
        *callXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, callXmlMP),
                           XMLRPC_MEMBLOCK_SIZE(char, callXmlMP));
        
        xmlrpc_DECREF(paramArrayP);
    }
    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
parseResponse(string       const& responseXml,
              rpcOutcome * const  outcomeP) {
/*----------------------------------------------------------------------------
   Parse the XML for an XML-RPC response into an XML-RPC result value.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_value * c_resultP;
    int faultCode;
    const char * faultString;

    xmlrpc_parse_response2(&env, responseXml.c_str(), responseXml.size(),
                           &c_resultP, &faultCode, &faultString);

    if (env.fault_occurred)
        throwf("Unable to find XML-RPC response in what server sent back.  %s",
               env.fault_string);
    else {
        if (faultString) {
            *outcomeP =
                rpcOutcome(fault(faultString,
                                 static_cast<fault::code_t>(faultCode)));
            xmlrpc_strfree(faultString);
        } else
            *outcomeP = rpcOutcome(value(c_resultP));
    }
}



void
parseSuccessfulResponse(string  const& responseXml,
                        value * const  resultP) {
/*----------------------------------------------------------------------------
   Same as parseResponse(), but expects the response to indicate success;
   throws an error if it doesn't.
-----------------------------------------------------------------------------*/
    rpcOutcome outcome;

    parseResponse(responseXml, &outcome);

    if (!outcome.succeeded())
        throwf("RPC response indicates it failed.  %s",
               outcome.getFault().getDescription().c_str());

    *resultP = outcome.getResult();
}



void
trace(string const& label,
      string const& xml) {
    
    xmlrpc_traceXml(label.c_str(), xml.c_str(), xml.size());

}


}} // namespace

/*=============================================================================
                                client.cpp
===============================================================================
  This is the C++ XML-RPC client library for Xmlrpc-c.

  Note that unlike most of Xmlprc-c's C++ API, this is _not_ based on the
  C client library.  This code is independent of the C client library, and
  is based directly on the client XML transport libraries (with a little
  help from internal C utility libraries).
=============================================================================*/

#include <string>

#include "girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"
/* transport_config.h defines XMLRPC_DEFAULT_TRANSPORT,
    MUST_BUILD_WININET_CLIENT, MUST_BUILD_CURL_CLIENT,
    MUST_BUILD_LIBWWW_CLIENT 
*/
#include "transport_config.h"

#if MUST_BUILD_WININET_CLIENT
#include "xmlrpc_wininet_transport.h"
#endif
#if MUST_BUILD_CURL_CLIENT
#include "xmlrpc_curl_transport.h"
#endif
#if MUST_BUILD_LIBWWW_CLIENT
#include "xmlrpc_libwww_transport.h"
#endif

#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/xml.hpp"
#include "xmlrpc-c/client.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {
/*----------------------------------------------------------------------------
   Use an object of this class to set up to remove a reference to an
   xmlrpc_value object (a C object with manual reference management)
   at then end of a scope -- even if the scope ends with a throw.
-----------------------------------------------------------------------------*/
class cValueWrapper {
    xmlrpc_value * valueP;
public:
    cValueWrapper(xmlrpc_value * valueP) : valueP(valueP) {}
    ~cValueWrapper() { xmlrpc_DECREF(valueP); }
};


class memblockStringWrapper {

public:    
    memblockStringWrapper(string const value) {

        xmlrpc_env env;
        xmlrpc_env_init(&env);
        
        this->memblockP = XMLRPC_MEMBLOCK_NEW(char, &env, value.size());

        if (env.fault_occurred)
            throw(error(env.fault_string));
    }
    
    memblockStringWrapper(xmlrpc_mem_block * const memblockP) :
        memblockP(memblockP) {};

    ~memblockStringWrapper() {
        XMLRPC_MEMBLOCK_FREE(char, this->memblockP);
    }

    xmlrpc_mem_block * memblockP;
};

} // namespace

namespace xmlrpc_c {

carriageParm_http0::carriageParm_http0() :
    c_serverInfoP(NULL) {}



carriageParm_http0::carriageParm_http0(string const serverUrl) {
    this->c_serverInfoP = NULL;

    this->instantiate(serverUrl);
}


carriageParm_http0::~carriageParm_http0() {

    if (this->c_serverInfoP)
        xmlrpc_server_info_free(this->c_serverInfoP);
}



void
carriageParm_http0::instantiate(string const serverUrl) {

    if (c_serverInfoP)
        throw(error("object already instantiated"));
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    this->c_serverInfoP = xmlrpc_server_info_new(&env, serverUrl.c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
carriageParm_http0::setBasicAuth(string const username,
                                 string const password) {

    if (!c_serverInfoP)
        throw(error("object not instantiated"));
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_server_info_set_basic_auth(
        &env, this->c_serverInfoP, username.c_str(), password.c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



carriageParm_curl0::carriageParm_curl0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}


carriageParm_libwww0::carriageParm_libwww0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



carriageParm_wininet0::carriageParm_wininet0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



clientXmlTransport::~clientXmlTransport() {
}



clientXmlTransport_http::~clientXmlTransport_http() {}



void
clientXmlTransport_http::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of type carriageParm_http"));

    memblockStringWrapper callXmlM(callXml);

    xmlrpc_mem_block * responseXmlMP;

    this->c_transportOpsP->call(&env,
                                this->c_transportP,
                                carriageParmHttpP->c_serverInfoP,
                                callXmlM.memblockP,
                                &responseXmlMP);

    if (env.fault_occurred)
        throw(error(env.fault_string));

    memblockStringWrapper responseHolder(responseXmlMP);
        // Makes responseXmlMP get freed at end of scope
    
    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlMP),
                           XMLRPC_MEMBLOCK_SIZE(char, responseXmlMP));
}



clientXmlTransport_curl::clientXmlTransport_curl(
    string const networkInterface = "",
    bool   const noSslVerifyPeer = false,
    bool   const noSslVerifyHost = false) {

#if MUST_BUILD_CURL_CLIENT
    struct xmlrpc_curl_xportparms transportParms; 

    transportParms.network_interface = 
        networkInterface.size() > 0 ? networkInterface.c_str() : NULL;
    transportParms.no_ssl_verifypeer = noSslVerifyPeer;
    transportParms.no_ssl_verifyhost = noSslVerifyHost;

    this->c_transportOpsP = &xmlrpc_curl_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_curl_transport_ops.create(
        &env, 0, "", "", (xmlrpc_xportparms *)&transportParms,
        XMLRPC_CXPSIZE(no_ssl_verifyhost),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
#else
    throw("There is no Curl client XML transport in this XML-RPC client "
          "library");
    if (networkInterface || noSslVerifyPeer || noSslVerifyHost) {
        /* eliminate unused parameter compiler warning */
    }
#endif
}
 


clientXmlTransport_curl::~clientXmlTransport_curl() {

    this->c_transportOpsP->destroy(this->c_transportP);
}


clientXmlTransport_libwww::clientXmlTransport_libwww(
    string const appname = "",
    string const appversion = "") {

#if MUST_BUILD_LIBWWW_CLIENT
    this->c_transportOpsP = &xmlrpc_libwww_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_libwww_transport_ops.create(
        &env, 0, appname.c_str(), appversion.c_str(), NULL, 0,
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
#else
    throw(error("There is no Libwww client XML transport "
                "in this XML-RPC client library"));
    if (appname || appversion) {
        /* eliminate unused parameter compiler warning */
    }
#endif
}
 


clientXmlTransport_libwww::~clientXmlTransport_libwww() {

    this->c_transportOpsP->destroy(this->c_transportP);
}



clientXmlTransport_wininet::clientXmlTransport_wininet(
    bool const allowInvalidSslCerts = false
    ) {

#if MUST_BUILD_WININET_CLIENT
    struct xmlrpc_wininet_xportparms transportParms; 

    transportParms.allowInvalidSSLCerts = allowInvalidSslCerts;

    this->c_transportOpsP = &xmlrpc_wininet_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_wininet_transport_ops.create(
        &env, 0, "", "", &transportParms, XMLRPC_WXPSIZE(allowInvalidSSLCerts),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
#else
    throw(error("There is no Wininet client XML transport "
                "in this XML-RPC client library"));
    if (allowInvalidSslCerts) {
        /* eliminate unused parameter compiler warning */
    }
#endif
}
 


clientXmlTransport_wininet::~clientXmlTransport_wininet() {

    this->c_transportOpsP->destroy(this->c_transportP);
}



clientXml::clientXml(clientXmlTransport * const transportP) :
    transportP(transportP) {}
     


void
clientXml::call(carriageParm * const  carriageParmP,
                string         const  methodName,
                paramList      const& paramList,
                value *        const  resultP) {

    string callXml;
    string responseXml;

    xml::generateCall(methodName, paramList, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    this->transportP->call(carriageParmP, callXml, &responseXml);

    xml::trace("XML-RPC RESPONSE", responseXml);

    xml::parseResponse(responseXml, resultP);
}
 


connection::connection(client *       const clientP,
                       carriageParm * const carriageParmP) :
    clientP(clientP), carriageParmP(carriageParmP) {}



connection::~connection() {}



rpc::rpc(string    const  methodName,
         xmlrpc_c::paramList const& paramList) {

    this->methodName = methodName;
    this->paramList  = paramList;
    this->finished   = false;
}



rpc::~rpc() {

}



void
rpc::call(client       * const clientP,
          carriageParm * const carriageParmP) {

    if (this->finished)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->call(carriageParmP,
                  this->methodName,
                  this->paramList,
                  &this->result);

    this->finished = true;
}



void
rpc::call(connection const& connection) {

    this->call(connection.clientP, connection.carriageParmP);

}


 
void
rpc::start(client       * const clientP,
           carriageParm * const carriageParmP) {
    
    // Asynchronicity not coded yet

    this->call(clientP, carriageParmP);
}


 
void
rpc::start(xmlrpc_c::connection const& connection) {
    
    // Asynchronicity not coded yet

    this->call(connection);
}



void
rpc::finish() {
/*----------------------------------------------------------------------------
   Anyone who does RPCs asynchronously will want to make his own class
   derived from 'rpc' and override this with a finish() that does
   something.

   Typically, finish() will queue the RPC so some other thread will
   deal with the fact that the RPC is finished.


   In the absence of the aforementioned queueing, the RPC becomes
   unreferenced as soon as our Caller releases his reference, so the
   RPC gets destroyed when we return.
-----------------------------------------------------------------------------*/



}

    

value
rpc::getResult() const {

    if (!this->finished)
        throw(error("Attempt to get result of RPC that is not finished."));

    return this->result;
}



bool
rpc::isFinished() const {
    return this->finished;
}



clientSimple::clientSimple() {
    
    if (string(XMLRPC_DEFAULT_TRANSPORT) == string("curl"))
        this->transportP = new clientXmlTransport_curl;
    else if (string(XMLRPC_DEFAULT_TRANSPORT) == string("libwww"))
        this->transportP = new clientXmlTransport_libwww;
    else if (string(XMLRPC_DEFAULT_TRANSPORT) == string("wininet"))
        this->transportP = new clientXmlTransport_wininet;
    else
        throw("INTERNAL ERROR: "
              "Default client XML transport is not one we recognize");

    this->clientP = new clientXml(transportP);
}



clientSimple::~clientSimple() {

    delete this->clientP;
    delete this->transportP;
}



void
clientSimple::call(string  const serverUrl,
                   string  const methodName,
                   value * const resultP) {

    carriageParm_http0 carriageParm(serverUrl);

    rpc rpc(methodName, paramList());

    rpc.call(this->clientP, &carriageParm);
    
    *resultP = rpc.getResult();
}


namespace {

void
makeParamArray(string          const format,
               xmlrpc_value ** const paramArrayPP,
               va_list               args) {
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    /* The format is a sequence of parameter specifications, such as
       "iiii" for 4 integer parameters.  We add parentheses to make it
       an array of those parameters: "(iiii)".
    */
    string const arrayFormat("(" + string(format) + ")");
    const char * tail;

    xmlrpc_build_value_va(&env, arrayFormat.c_str(),
                          args, paramArrayPP, &tail);

    if (env.fault_occurred)
        throw(error(env.fault_string));

    if (strlen(tail) != 0) {
        /* xmlrpc_build_value_va() parses off a single value specification
           from its format string, and 'tail' points to whatever is after
           it.  Our format string should have been a single array value,
           meaning tail is end-of-string.  If it's not, that means
           something closed our array early.
        */
        xmlrpc_DECREF(*paramArrayPP);
        throw(error("format string is invalid.  It apparently has a "
                    "stray right parenthesis"));
    }
}

}  // namespace


void
clientSimple::call(string  const serverUrl,
                   string  const methodName,
                   string  const format,
                   value * const resultP,
                   ...) {

    carriageParm_http0 carriageParm(serverUrl);

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_value * paramArrayP;

    va_list args;
    va_start(args, resultP);
    makeParamArray(format, &paramArrayP, args);
    va_end(args);

    if (env.fault_occurred)
        throw(error(env.fault_string));
    else {
        cValueWrapper paramArrayWrapper(paramArrayP); // ensure destruction
        unsigned int const paramCount = xmlrpc_array_size(&env, paramArrayP);
        
        if (env.fault_occurred)
            throw(error(env.fault_string));
        
        paramList paramList;
        for (unsigned int i = 0; i < paramCount; ++i) {
            xmlrpc_value * paramP;
            xmlrpc_array_read_item(&env, paramArrayP, i, &paramP);
            if (env.fault_occurred)
                throw(error(env.fault_string));
            else {
                cValueWrapper paramWrapper(paramP); // ensure destruction
                paramList.add(value(paramP));
            }
        }
        rpc rpc(methodName, paramList);
        rpc.call(this->clientP, &carriageParm);
        *resultP = rpc.getResult();
    }
}



void
clientSimple::call(string    const  serverUrl,
                   string    const  methodName,
                   paramList const& paramList,
                   value *   const  resultP) {
    
    carriageParm_http0 carriageParm(serverUrl);

    rpc rpc(methodName, paramList);

    rpc.call(this->clientP, &carriageParm);
    
    *resultP = rpc.getResult();
}


} // namespace

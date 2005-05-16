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

#include "pthreadx.h"
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



xmlTransaction::xmlTransaction() {
    
    int rc;

    rc = pthread_mutex_init(&this->lock, NULL);

    if (rc != 0)
        throw(error("Unable to initialize pthread mutex"));

    this->refcount   = 0;
}



xmlTransaction::~xmlTransaction() {
    if (this->refcount > 0)
        throw(error("Destroying referenced object"));

    int rc;

    rc = pthread_mutex_destroy(&this->lock);

    if (rc != 0)
        throw(error("Unable to destroy pthread mutex"));
}



void
xmlTransaction::incref() {
    pthread_mutex_lock(&this->lock);
    ++this->refcount;
    pthread_mutex_unlock(&this->lock);
}



void
xmlTransaction::decref(bool * const unreferencedP) {

    if (this->refcount == 0)
        throw(error("Decrementing ref count of unreferenced object"));
    pthread_mutex_lock(&this->lock);
    --this->refcount;
    *unreferencedP = (this->refcount == 0);
    pthread_mutex_unlock(&this->lock);
}
 


void
xmlTransaction::finish(string const& responseXml) const {

    xml::trace("XML-RPC RESPONSE", responseXml);
}



void
xmlTransaction::finishErr(error const& error) const {

    if (error.what()) {}
}



// TODO: use a template or base class to do the common stuff between
// rpcPtr, xmlTransactionPtr, and methodPtr. 

xmlTransactionPtr::xmlTransactionPtr() : xmlTransactionP(NULL) {}



xmlTransactionPtr::xmlTransactionPtr(xmlTransaction * const xmlTransactionP) {

    this->xmlTransactionP = xmlTransactionP;
    xmlTransactionP->incref();
}
    
 

xmlTransactionPtr::xmlTransactionPtr(xmlTransactionPtr const& xmlTranP) {
    // copy constructor
    this->xmlTransactionP = xmlTranP.xmlTransactionP;
    if (this->xmlTransactionP)
        this->xmlTransactionP->incref();
}
    
 

xmlTransactionPtr::~xmlTransactionPtr() {
    bool dead;
    this->xmlTransactionP->decref(&dead);
    if (dead)
        delete(this->xmlTransactionP);
}


 
xmlTransactionPtr
xmlTransactionPtr::operator=(xmlTransactionPtr const& xmlTransactionPtr) {
    if (this->xmlTransactionP != NULL)
        throw(error("Already instantiated"));
    this->xmlTransactionP = xmlTransactionPtr.xmlTransactionP;
    this->xmlTransactionP->incref();
    return *this;
}

   

xmlTransaction *
xmlTransactionPtr::operator->() const {
    return this->xmlTransactionP;
}



clientXmlTransport::~clientXmlTransport() {}



void
clientXmlTransport::start(carriageParm *    const  carriageParmP,
                          string            const& callXml,
                          xmlTransactionPtr const& xmlTranP) {
    
    string responseXml;

    this->call(carriageParmP, callXml, &responseXml);

    xmlTranP->finish(responseXml);
}



void
clientXmlTransport::finishAsync(xmlrpc_c::timeout const timeout) {
    if (timeout.finite == timeout.finite)
        throw(error("This class does not have finishAsync()"));
}



void
clientXmlTransport::asyncComplete(
    struct xmlrpc_call_info * const callInfoP,
    xmlrpc_mem_block *        const responseXmlMP,
    xmlrpc_env                const transportEnv) {

    xmlTransactionPtr * const xmlTranPP =
        reinterpret_cast<xmlTransactionPtr *>(callInfoP);

    try {
        if (transportEnv.fault_occurred) {
            (*xmlTranPP)->finishErr(error(transportEnv.fault_string));
        } else {
            string const responseXml(
                XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlMP),
                XMLRPC_MEMBLOCK_SIZE(char, responseXmlMP));
            (*xmlTranPP)->finish(responseXml);
        }
    } catch(error const error) {
        /* We can't throw an error back to C code, and the async_complete
           interface does not provide for failure, so we define ->finish()
           as not being capable of throwing an error.
        */
        assert(false);
    }
    delete(xmlTranPP);

    /* Ordinarily, *xmlTranPP is the last reference to **xmlTranPP
       (The xmlTransaction), so it will get destroyed too.  But
       ->finish() could conceivably create a new reference to
       **xmlTranPP, and then it would keep living.
    */
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



void
clientXmlTransport_http::start(
    carriageParm *    const  carriageParmP,
    string            const& callXml,
    xmlTransactionPtr const& xmlTranP) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of type carriageParm_http"));

    memblockStringWrapper callXmlM(callXml);

    /* xmlTranP2 is the reference to the XML transaction that is held by
       the running transaction, in C code.  It lives in dynamically allocated
       storage and xmlTranP2P points to it.
    */
    xmlTransactionPtr * const xmlTranP2P(new xmlTransactionPtr(xmlTranP));

    try {
        this->c_transportOpsP->send_request(
            &env,
            this->c_transportP,
            carriageParmHttpP->c_serverInfoP,
            callXmlM.memblockP,
            &this->asyncComplete,
            reinterpret_cast<xmlrpc_call_info *>(xmlTranP2P));

        if (env.fault_occurred)
            throw(error(env.fault_string));
    } catch (...) {
        delete xmlTranP2P;
        throw;
    }
}



void
clientXmlTransport_http::finishAsync(xmlrpc_c::timeout const timeout) {

    xmlrpc_timeoutType const c_timeoutType(
        timeout.finite ? timeout_yes : timeout_no);
    xmlrpc_timeout const c_timeout(timeout.duration);

    this->c_transportOpsP->finish_asynch(
        this->c_transportP, c_timeoutType, c_timeout);
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



void
client::start(carriageParm * const  carriageParmP,
              string         const  methodName,
              paramList      const& paramList,
              rpcPtr         const& rpcP) {
/*----------------------------------------------------------------------------
   Start an RPC, wait for it to complete, and finish it.

   Usually, a derived class overrides this with something that does
   not wait for the RPC to complete, but rather arranges for something
   to finish the RPC later when the RPC does complete.
-----------------------------------------------------------------------------*/
    value result;

    call(carriageParmP, methodName, paramList, &result);

    rpcP->finish(result);
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
 


void
clientXml::start(carriageParm * const  carriageParmP,
                 string         const  methodName,
                 paramList      const& paramList,
                 rpcPtr         const& rpcP) {

    string callXml;

    xml::generateCall(methodName, paramList, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    xmlTransactionPtr xmlTranP(new xmlTransaction_rpc(rpcP));

    this->transportP->start(carriageParmP, callXml, xmlTranP);
}
 


void
clientXml::finishAsync(xmlrpc_c::timeout const timeout) {

    transportP->finishAsync(timeout);
}




connection::connection(client *       const clientP,
                       carriageParm * const carriageParmP) :
    clientP(clientP), carriageParmP(carriageParmP) {}



connection::~connection() {}



rpc::rpc(string              const  methodName,
         xmlrpc_c::paramList const& paramList) {
    
    int rc;

    rc = pthread_mutex_init(&this->lock, NULL);

    if (rc != 0)
        throw(error("Unable to initialize pthread mutex"));

    this->state      = STATE_UNFINISHED;
    this->methodName = methodName;
    this->paramList  = paramList;
    this->refcount   = 1;
}



rpc::~rpc() {
    if (this->refcount > 0)
        throw(error("Destroying referenced object"));

    int rc;

    rc = pthread_mutex_destroy(&this->lock);

    if (rc != 0)
        throw(error("Unable to destroy pthread mutex"));

    if (this->state == STATE_ERROR)
        delete(this->errorP);
    if (this->state == STATE_FAILED)
        delete(this->faultP);
}



void
rpc::incref() {
    pthread_mutex_lock(&this->lock);
    ++this->refcount;
    pthread_mutex_unlock(&this->lock);
}



void
rpc::decref(bool * const unreferencedP) {

    if (this->refcount == 0)
        throw(error("Decrementing ref count of unreferenced object"));
    pthread_mutex_lock(&this->lock);
    --this->refcount;
    *unreferencedP = (this->refcount == 0);
    pthread_mutex_unlock(&this->lock);
}
 


void
rpc::call(client       * const clientP,
          carriageParm * const carriageParmP) {

    if (this->state != STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->call(carriageParmP,
                  this->methodName,
                  this->paramList,
                  &this->result);

    this->state = STATE_SUCCEEDED;
}



void
rpc::call(connection const& connection) {

    this->call(connection.clientP, connection.carriageParmP);

}


 
void
rpc::start(client       * const clientP,
           carriageParm * const carriageParmP) {
    
    if (this->state != STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->start(carriageParmP,
                   this->methodName,
                   this->paramList,
                   rpcPtr(this));
}


 
void
rpc::start(xmlrpc_c::connection const& connection) {
    
    this->start(connection.clientP, connection.carriageParmP);
}



void
rpc::finish(value const& result) {

    this->state = STATE_SUCCEEDED;
    this->result = result;
    this->notifyComplete();
}



void
rpc::finishErr(error const& error) {

    this->state = STATE_ERROR;
    this->errorP = new girerr::error(error);
    this->notifyComplete();
}



void
rpc::finishFail(fault const& fault) {

    this->state = STATE_FAILED;
    this->faultP = new xmlrpc_c::fault(fault);
    this->notifyComplete();
}



void
rpc::notifyComplete() {
/*----------------------------------------------------------------------------
   Anyone who does RPCs asynchronously and doesn't use polling will
   want to make his own class derived from 'rpc' and override this
   with a notifyFinish() that does something.

   Typically, notifyFinish() will queue the RPC so some other thread
   will deal with the fact that the RPC is finished.


   In the absence of the aforementioned queueing, the RPC becomes
   unreferenced as soon as our Caller releases his reference, so the
   RPC gets destroyed when we return.
-----------------------------------------------------------------------------*/

}

    

value
rpc::getResult() const {

    switch (this->state) {
    case STATE_UNFINISHED:
        throw(error("Attempt to get result of RPC that is not finished."));
        break;
    case STATE_ERROR:
        throw(*this->errorP);
        break;
    case STATE_FAILED:
        throw(*this->faultP);
        break;
    case STATE_SUCCEEDED: {
        // All normal
    }
    }

    return this->result;
}



bool
rpc::isFinished() const {
    return (this->state != STATE_UNFINISHED);
}



rpcPtr::rpcPtr() : rpcP(NULL) {}



rpcPtr::rpcPtr(rpc * const rpcP) {

    this->rpcP = rpcP;
    rpcP->incref();
}
    
 

rpcPtr::rpcPtr(rpcPtr const& rpcPtr) { // copy constructor

    this->rpcP = rpcPtr.rpcP;
    if (this->rpcP)
        this->rpcP->incref();
}
    
 

rpcPtr::rpcPtr(string              const  methodName,
               xmlrpc_c::paramList const& paramList) {

    this->rpcP = new rpc(methodName, paramList);
}



rpcPtr::~rpcPtr() {
    bool dead;
    this->rpcP->decref(&dead);
    if (dead) {
        delete(this->rpcP);
    }
}


 
rpcPtr
rpcPtr::operator=(rpcPtr const& rpcPtr) {
    if (this->rpcP != NULL)
        throw(error("Already instantiated"));
    this->rpcP = rpcPtr.rpcP;
    this->rpcP->incref();
    return *this;
}

   

rpc *
rpcPtr::operator->() const {
    return this->rpcP;
}



xmlTransaction_rpc::xmlTransaction_rpc(rpcPtr const& rpcP) :
    rpcP(rpcP) {}



void
xmlTransaction_rpc::finish(string const& responseXml) const {

    xml::trace("XML-RPC RESPONSE", responseXml);

    value result;

    try {
        xml::parseResponse(responseXml, &result);
        this->rpcP->finish(result);
    } catch (fault const fault) {
        this->rpcP->finishFail(fault);
    } catch (error const error) {
        this->rpcP->finishErr(error);
    }
}



void
xmlTransaction_rpc::finishErr(error const& error) const {

    this->rpcP->finishErr(error);
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

    rpcPtr rpcPtr(methodName, paramList());

    rpcPtr->call(this->clientP, &carriageParm);
    
    *resultP = rpcPtr->getResult();
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
        rpcPtr rpcPtr(methodName, paramList);
        rpcPtr->call(this->clientP, &carriageParm);
        *resultP = rpcPtr->getResult();
    }
}



void
clientSimple::call(string    const  serverUrl,
                   string    const  methodName,
                   paramList const& paramList,
                   value *   const  resultP) {
    
    carriageParm_http0 carriageParm(serverUrl);
    
    rpcPtr rpcPtr(methodName, paramList);

    rpcPtr->call(this->clientP, &carriageParm);
    
    *resultP = rpcPtr->getResult();
}


} // namespace

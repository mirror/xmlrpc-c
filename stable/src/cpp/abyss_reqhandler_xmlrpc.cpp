#include <string>

using namespace std;

#include "xmlrpc-c/AbyssServer.hpp"
using xmlrpc_c::AbyssServer;
#include "xmlrpc-c/registry.hpp"

#include "xmlrpc-c/abyss_reqhandler_xmlrpc.hpp"


namespace xmlrpc_c {

abyssReqhandlerXmlrpc::abyssReqhandlerXmlrpc(
    xmlrpc_c::registryPtr const& registryP) :
    registryP(registryP)
{}



static void
handleXmlRpc(AbyssServer::Session * const sessionP,
             xmlrpc_c::registry *   const registryP,
             bool *                 const responseStartedP) {

    string const callXml(sessionP->body());
    string responseXml;

    registryP->processCall(callXml, &responseXml);

    sessionP->setRespStatus(200);

    sessionP->setRespContentType("text/xml charset=utf-8");

    sessionP->setRespContentLength(responseXml.size());

    *responseStartedP = true;

    sessionP->writeResponse(responseXml);
}



void
abyssReqhandlerXmlrpc::abortRequest(
    AbyssServer::Session * const  sessionP,
    bool                   const  responseStarted,
    AbyssServer::Exception const& e) {

    if (responseStarted) {
        // We can't send an error response because we failed
        // partway through sending a non-error response.  All we can
        // do is perhaps log the error and then close the connection.
        this->handleUnreportableFailure(e);
    } else 
        sessionP->sendErrorResponse(e);
}



void
abyssReqhandlerXmlrpc::handleRequest(
    AbyssServer::Session * const sessionP,
    bool *                 const handledP) {

    bool responseStarted;
        // We have at least started to send an HTTP response.  (This is
        // important in error handling, because it means it is too late to
        // send an error response).

    responseStarted = false;

    try {
        switch (sessionP->method()) {
        case AbyssServer::Session::METHOD_POST: {
            if (sessionP->uriPathName() == "/RPC2") {

                handleXmlRpc(sessionP, this->registryP.get(),
                             &responseStarted);
                *handledP = true;
            } else
                *handledP = false;
        } break;
        default:
            *handledP = false;
        }
    } catch (AbyssServer::Exception const& e) {
        this->abortRequest(sessionP, responseStarted, e);
        *handledP = true;
    } catch (exception const& e) {
        this->abortRequest(sessionP, responseStarted,
                           AbyssServer::Exception(500, e.what()));
        *handledP = true;
    }
}



void
abyssReqhandlerXmlrpc::handleUnreportableFailure(
    AbyssServer::Exception const& ) {
/*-----------------------------------------------------------------------------
   This is the default implementation of the virtual method.
-----------------------------------------------------------------------------*/
}



}  // namespace

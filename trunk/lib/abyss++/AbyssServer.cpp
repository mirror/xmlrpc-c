#include <cassert>
#include <string>
#include <stdexcept>
#include <map>
#include <string.h>

using namespace std;

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include <xmlrpc-c/abyss.h>

#include "xmlrpc-c/AbyssServer.hpp"



AbyssServer::Exception::Exception(unsigned short const  httpStatusCode,
                                  string         const& explanation) :

    runtime_error(explanation),
    _httpStatusCode(httpStatusCode)
{}



unsigned short
AbyssServer::Exception::httpStatusCode() const {
    return this->_httpStatusCode;
}



AbyssServer::ReqHandler::~ReqHandler() {}



size_t
AbyssServer::ReqHandler::handleReqStackSize() const {

    return 0;  // Means default
}



void
AbyssServer::ReqHandler::terminate() {}



AbyssServer::Session::Session(TSession * const cSessionP) :
    cSessionP(cSessionP)
{
    this->responseStarted = false;
}



AbyssServer::Session::Method
AbyssServer::Session::method() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    switch (requestInfoP->method) {
    case m_unknown: return METHOD_UNKNOWN;
    case m_get:     return METHOD_GET;
    case m_put:     return METHOD_PUT;
    case m_head:    return METHOD_HEAD;
    case m_post:    return METHOD_POST;
    case m_delete:  return METHOD_DELETE;
    case m_trace:   return METHOD_TRACE;
    case m_options: return METHOD_OPTIONS;
    }
}



string const
AbyssServer::Session::requestLine() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    assert(requestInfoP->requestline);

    return string(requestInfoP->requestline);
}



string const
AbyssServer::Session::uriPathName() const {
/*-----------------------------------------------------------------------------
   The path name from the URI for the HTTP request in this session.

   If the path name is empty (e.g. for URI http://example.com), this is a null
   string (in contrast to the Abyss C interface, in which it is "*").

   According to the URI RFC, RFC 3986, if the path name is
   "http://example.com/example.html", the path name is "/example.html" (note
   the leading slash), and that is what we return.
-----------------------------------------------------------------------------*/
    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    string const requestInfoUri(requestInfoP->uri);

    // There seems to be a weird design in which if the URI has a null
    // string path name, requestInfoP->uri is "*".  According to the RFC,
    // all URIs have a path name, and it may be a null string.

    return requestInfoUri == "*" ? "" : requestInfoUri;
}



vector<string> const
AbyssServer::Session::uriPathNameSegment() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    string const requestInfoUri(requestInfoP->uri);

    vector<string> retval;

    retval.reserve(10);  // A guess

    // There seems to be a weird design in which if the URI has an empty
    // path name, requestInfoP->uri is "*".  According to the RFC,
    // all URIs have a path name, and it may be empty.

    if (requestInfoUri == "*") {
        // empty path; no segments
    } else {
        string const pathName(requestInfoUri);

        if (pathName.length() < 1 || pathName[0] != '/')
            throwf("INTERNAL ERROR: SessionGetRequestInfo returned "
                   "'uri' field that does not begin with a slash: '%s'",
                   pathName.c_str());

        for (size_t p = 1; p < pathName.length(); ) {
            size_t const slashPos(pathName.find('/', p));

            size_t const segEnd(slashPos == string::npos ?
                                pathName.length() : slashPos);

            retval.push_back(pathName.substr(p, segEnd - p));

            p = slashPos == string::npos ? segEnd : slashPos + 1;
        }
    }
    return retval;
}



bool
AbyssServer::Session::uriHasQuery() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->query != NULL;
}



string const
AbyssServer::Session::uriQuery() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->query)
        return string(requestInfoP->query);
    else
        throwf("Request URI has no query part");
}



static map<string, string> const
formInputFromQuery(string const& query) {
/*-----------------------------------------------------------------------------
   'query' interpreted as HTML form input.

   E.g. if 'query' is "employee=5&department=Q", we return a map with two
   elements:

     "employee"  => "5"
     "department => "Q"

-----------------------------------------------------------------------------*/
    map<string, string> retval;

    for (size_t pairBeg = 0; pairBeg < query.length(); ) {
        size_t pairEnd;

        pairEnd = query.find('&', pairBeg);
        if (pairEnd == string::npos)
            pairEnd = query.length();

        size_t keyEnd;

        keyEnd = query.find('=', pairBeg);

        if (keyEnd == string::npos || keyEnd >= pairEnd)
            throwf("form input key/value pair '%s' "
                   "does not contain an equal sign",
                   query.substr(pairBeg, pairEnd - pairBeg).c_str());

        size_t const keyBeg(pairBeg);
        size_t const valueBeg(keyEnd + 1);
        size_t const valueEnd(pairEnd);

        retval[query.substr(keyBeg, keyEnd - keyBeg)] =
            query.substr(valueBeg, valueEnd - valueBeg);

        assert(pairEnd >= pairBeg);

        pairBeg = pairEnd + 1;
    }

    return retval;
}



map<string, string> const
AbyssServer::Session::formInput() const {
/*-----------------------------------------------------------------------------
   The query portion of the URI for this session, interpreted as HTML form
   input.

   HTML form input takes the form of text key/value pairs.  Our result is a
   std::map that represents those key/value pairs in the obvious way.

   If the URI has no query part, throw an error.
-----------------------------------------------------------------------------*/
    return formInputFromQuery(this->uriQuery());
}



bool
AbyssServer::Session::hasHost() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->host != NULL;
}



string const
AbyssServer::Session::host() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->host)
        return string(requestInfoP->host);
    else
        throwf("Request does not specify a host");
}



bool
AbyssServer::Session::hasFrom() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->from != NULL;
}



string const
AbyssServer::Session::from() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->from)
        return string(requestInfoP->from);
    else
        throwf("Request header does not have a 'from' field");
}



bool
AbyssServer::Session::hasUseragent() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->useragent != NULL;
}



string const
AbyssServer::Session::useragent() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->from)
        return string(requestInfoP->useragent);
    else
        throwf("Request header does not have a 'useragent' field");
}



bool
AbyssServer::Session::hasReferer() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->referer != NULL;
}



string const
AbyssServer::Session::referer() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->referer)
        return string(requestInfoP->from);
    else
        throwf("Request header does not have a 'referer' field");
}



bool
AbyssServer::Session::userIsAuthenticated() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->user != NULL;
}



string const
AbyssServer::Session::user() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    if (requestInfoP->user)
        return string(requestInfoP->user);
    else
        throwf("Request header does not identify a user or "
               "server could not authenticate the identity");
}



unsigned short
AbyssServer::Session::port() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->port;
}



bool
AbyssServer::Session::keepalive() const {

    const TRequestInfo * requestInfoP;

    SessionGetRequestInfo(this->cSessionP, &requestInfoP);

    return requestInfoP->keepalive;
}


void
AbyssServer::Session::setRespStatus(unsigned short const statusCode) {

    ResponseStatus(this->cSessionP, statusCode);
}



void
AbyssServer::Session::setRespContentType(string const& contentType) {

    ResponseContentType(this->cSessionP, contentType.c_str());
}



void
AbyssServer::Session::setRespContentLength(uint64_t const contentLength) {

    ResponseContentLength(this->cSessionP, contentLength);
}



void
AbyssServer::Session::startWriteResponse() {

    // Note that ResponseWriteStart() assumes no response has been started; it
    // asserts that fact.

    if (this->responseStarted)
        throwf("Attempt to write multiple responses in same session");

    ResponseWriteStart(this->cSessionP);

    this->responseStarted = true;
}



void
AbyssServer::Session::endWriteResponse() {

    ResponseWriteEnd(this->cSessionP);
}



void
AbyssServer::Session::writeResponseBody(string const& bodyPart) {
/*-----------------------------------------------------------------------------
   Write some of the response.  If it's a chunked response, this constitutes
   one chunk.  If not, it's just part of the monolithic response.  Either
   way, you can call this multiple times for one response.
-----------------------------------------------------------------------------*/
    ResponseWriteBody(this->cSessionP, bodyPart.data(), bodyPart.size());
}



void
AbyssServer::Session::writeResponse(string const& body) {

    this->startWriteResponse();

    this->writeResponseBody(body);

    this->endWriteResponse();
}



void
AbyssServer::Session::sendErrorResponse(string const& explanation) {
/*-----------------------------------------------------------------------------
  Send a response indicating that the HTTP request failed, with text
  explanation 'explanation'.  The response is HTML and contains an appropriate
  content-type header, regardless of any content type that may be set in the
  object already.

  Assume the HTTP status code is already set in the object.
-----------------------------------------------------------------------------*/
    ResponseError2(this->cSessionP, explanation.c_str());
}



void
AbyssServer::Session::sendErrorResponse(Exception const& e) {
/*-----------------------------------------------------------------------------
  Send a response indicating that the HTTP request failed, with HTTP status
  code and text explanation 'e'.  The response is HTML and contains an
  appropriate content-type header, regardless of any content type that may be
  set in the object already.
-----------------------------------------------------------------------------*/
    this->setRespStatus(e.httpStatusCode());

    this->sendErrorResponse(e.what());
}



AbyssServer::AbyssServer(AbyssChanSwitch * const chanSwitchP) {

    const char * error;

    ServerCreateSwitch(&this->cServer,
                       chanSwitchP->cChanSwitchP(),
                       &error);

    if (error)
        throw runtime_error(error);
}



void
AbyssServer::init() {

    const char * error;

    ServerInit2(&this->cServer, &error);

    if (error)
        throw runtime_error(error);
}



void
AbyssServer::run() {

    ServerRun(&this->cServer);
}



void
AbyssServer::runOnce() {

    ServerRunOnce(&this->cServer);
}



AbyssServer::~AbyssServer() {

    ServerFree(&this->cServer);
}



static void
cHandleRequest(void *       const handlerPtr,
               TSession *   const cSessionP,
               abyss_bool * const handledP) {

    AbyssServer::ReqHandler * const reqHandlerP(
        reinterpret_cast<AbyssServer::ReqHandler *>(handlerPtr));

    try {
        AbyssServer::Session session(cSessionP);

        bool handled;

        reqHandlerP->handleRequest(&session, &handled);

        *handledP = handled;
    } catch (...) {
        *handledP = false;
    }
}



static void
cTermHandler(void * const handlerPtr) {

    AbyssServer::ReqHandler * const reqHandlerP(
        reinterpret_cast<AbyssServer::ReqHandler *>(handlerPtr));

    // It's not supposed to throw an error, and there's nothing we could do
    // but abort the program if it does, so we don't catch exceptions.
    
    reqHandlerP->terminate();
}



void
AbyssServer::addRequestHandler(ReqHandler * const handlerP) {

    ServerReqHandler3 cReqHandler;

    cReqHandler.handleReq          = cHandleRequest;

    cReqHandler.userdata           = handlerP;

    cReqHandler.handleReqStackSize = handlerP->handleReqStackSize();

    cReqHandler.term               = cTermHandler;

    abyss_bool success;

    ServerAddHandler3(&this->cServer, &cReqHandler, &success);

    if (!success)
        throwf("ServerAddHandler3() failed");
}




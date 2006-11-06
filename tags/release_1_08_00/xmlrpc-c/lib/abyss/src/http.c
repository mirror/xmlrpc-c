/* Copyright information is at the end of the file */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"
#include "mallocvar.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"

#include "server.h"
#include "session.h"
#include "conn.h"
#include "token.h"
#include "date.h"
#include "data.h"

#include "http.h"

/*********************************************************************
** Request Parser
*********************************************************************/

/*********************************************************************
** Request
*********************************************************************/

static void
initRequestInfo(TRequestInfo * const requestInfoP,
                httpVersion    const httpVersion,
                const char *   const requestLine,
                TMethod        const httpMethod,
                const char *   const host,
                unsigned int   const port,
                const char *   const path,
                const char *   const query) {
/*----------------------------------------------------------------------------
  Set up the request info structure.  For information that is
  controlled by headers, use the defaults -- I.e. the value that
  applies if the request contains no applicable header.
-----------------------------------------------------------------------------*/
    requestInfoP->requestline = strdup(requestLine);
    requestInfoP->method      = httpMethod;
    requestInfoP->host        = xmlrpc_strdupnull(host);
    requestInfoP->port        = port;
    requestInfoP->uri         = strdup(path);
    requestInfoP->query       = xmlrpc_strdupnull(query);
    requestInfoP->from        = NULL;
    requestInfoP->useragent   = NULL;
    requestInfoP->referer     = NULL;
    requestInfoP->user        = NULL;

    if (httpVersion.major > 1 ||
        (httpVersion.major == 1 && httpVersion.minor >= 1))
        requestInfoP->keepalive = TRUE;
    else
        requestInfoP->keepalive = FALSE;
}



static void
freeRequestInfo(TRequestInfo * const requestInfoP) {

    xmlrpc_strfreenull(requestInfoP->host);

    xmlrpc_strfreenull(requestInfoP->user);
}



void
RequestInit(TSession * const sessionP,
            TConn *    const connectionP) {

    time_t nowtime;

    sessionP->validRequest = false;  /* Don't have valid request yet */

    time(&nowtime);
    sessionP->date = *gmtime(&nowtime);

    sessionP->conn = connectionP;

    sessionP->responseStarted = FALSE;

    sessionP->chunkedwrite = FALSE;
    sessionP->chunkedwritemode = FALSE;

    ListInit(&sessionP->cookies);
    ListInit(&sessionP->ranges);
    TableInit(&sessionP->request_headers);
    TableInit(&sessionP->response_headers);

    sessionP->status = 0;  /* No status from handler yet */

    StringAlloc(&(sessionP->header));
}



void
RequestFree(TSession * const sessionP) {

    if (sessionP->validRequest)
        freeRequestInfo(&sessionP->requestInfo);

    ListFree(&sessionP->cookies);
    ListFree(&sessionP->ranges);
    TableFree(&sessionP->request_headers);
    TableFree(&sessionP->response_headers);
    StringFree(&(sessionP->header));
}



static void
readRequestLine(TSession * const sessionP,
                char **    const requestLineP,
                uint16_t * const httpErrorCodeP) {

    char * line;
    abyss_bool gotNonEmptyLine;

    *httpErrorCodeP = 0;  /* initial assumption */
    gotNonEmptyLine = false;

    /* Ignore CRLFs in the beginning of the request (RFC2068-P30) */
    while (!gotNonEmptyLine && *httpErrorCodeP == 0) {
        abyss_bool success;
        success = ConnReadHeader(sessionP->conn, &line);
        if (success) {
            if (line[0] != '\0')
                gotNonEmptyLine = true;
        } else
            *httpErrorCodeP = 408;  /* Request Timeout */
    }
    *requestLineP = line;
}



static void
unescapeUri(char *       const uri,
            abyss_bool * const errorP) {

    char * x;
    char * y;

    x = y = uri;
    
    *errorP = FALSE;

    while (*x && !*errorP) {
        switch (*x) {
        case '%': {
            char c;
            ++x;
            c = tolower(*x++);
            if ((c >= '0') && (c <= '9'))
                c -= '0';
            else if ((c >= 'a') && (c <= 'f'))
                c -= 'a' - 10;
            else
                *errorP = TRUE;

            if (!*errorP) {
                char d;
                d = tolower(*x++);
                if ((d >= '0') && (d <= '9'))
                    d -= '0';
                else if ((d >= 'a') && (d <= 'f'))
                    d -= 'a' - 10;
                else
                    *errorP = TRUE;

                if (!*errorP)
                    *y++ = ((c << 4) | d);
            }
        } break;

        default:
            *y++ = *x++;
            break;
        }
    }
    *y = '\0';
}



static void
parseHostPort(const char *     const hostport,
              const char **    const hostP,
              unsigned short * const portP,
              uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Parse a 'hostport', a string in the form www.acme.com:8080 .

   Return the host name part (www.acme.com) as *hostP (in newly
   malloced storage), and the port part (8080) as *portP.

   Default the port to 80 if 'hostport' doesn't have the port part.
-----------------------------------------------------------------------------*/
    char * buffer;
    char * colonPos;

    buffer = strdup(hostport);

    colonPos = strchr(buffer, ':');
    if (colonPos) {
        const char * p;
        uint32_t port;

        *colonPos = '\0';  /* Split hostport at the colon */

        for (p = colonPos + 1, port = 0;
             isdigit(*p) && port < 65535;
             (port = port * 10 + (*p - '0')), ++p);
            
        if (*p || port == 0)
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            *hostP = strdup(buffer);
            *portP = port;
            *httpErrorCodeP = 0;
        }
    } else {
        *hostP          = strdup(buffer);
        *portP          = 80;
        *httpErrorCodeP = 0;
    }
    free(buffer);
}



static void
parseRequestUri(char *           const requestUri,
                const char **    const hostP,
                unsigned short * const portP,
                const char **    const pathP,
                const char **    const queryP,
                uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
  Parse the request URI (in the request line
  "GET http://www.myserver.com:8080/myfile.cgi?parm HTTP/1.1",
  "http://www.myserver.com:8080/myfile.cgi?parm" is the request URI).

  Return as *hostP the "www.myserver.com" in the above example.  If
  that part of the URI doesn't exist, return *hostP == NULL.

  Return as *portP the 8080 in the above example.  If it doesn't exist,
  return 80.

  Return as *pathP the "/myfile.cgi" in the above example.  If it
  doesn't exist, return "*".

  Return as *queryP the "parm" in the above example.  If it doesn't
  exist, return *queryP == NULL.

  Return strings in newly malloc'ed storage.

  This destroys 'requestUri'.  We should fix that.
-----------------------------------------------------------------------------*/
    abyss_bool error;

    unescapeUri(requestUri, &error);
    
    if (error)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        char * requestUriNoQuery;
           /* The request URI with any query (the stuff marked by a question
              mark at the end of a request URI) chopped off.
           */
        {
            /* Split requestUri at the question mark */
            char * const qmark = strchr(requestUri, '?');
            
            if (qmark) {
                *qmark = '\0';
                *queryP = strdup(qmark + 1);
            } else
                *queryP = NULL;

            requestUriNoQuery = requestUri;
        }

        if (requestUriNoQuery[0] == '/') {
            *hostP = NULL;
            *pathP = strdup(requestUriNoQuery);
            *portP = 80;
            *httpErrorCodeP = 0;
        } else {
            if (!xmlrpc_strneq(requestUriNoQuery, "http://", 7))
                *httpErrorCodeP = 400;  /* Bad Request */
            else {
                char * const hostportpath = &requestUriNoQuery[7];
                char * const slashPos = strchr(hostportpath, '/');

                const char * host;
                const char * path;
                unsigned short port;

                char * hostport;
                
                if (slashPos) {
                    char * p;
                    path = strdup(slashPos);
                    
                    /* Nul-terminate the host name.  To make space for
                       it, slide the whole name back one character.
                       This moves it into the space now occupied by
                       the end of "http://", which we don't need.
                    */
                    for (p = hostportpath; *p != '/'; ++p)
                        *(p-1) = *p;
                    *(p-1) = '\0';
                    
                    hostport = hostportpath - 1;
                    *httpErrorCodeP = 0;
                } else {
                    path = strdup("*");
                    hostport = hostportpath;
                    *httpErrorCodeP = 0;
                }
                if (!*httpErrorCodeP)
                    parseHostPort(hostport, &host, &port, httpErrorCodeP);
                if (*httpErrorCodeP)
                    xmlrpc_strfree(path);

                *hostP  = host;
                *portP  = port;
                *pathP  = path;
            }
        }
    }
}



static void
parseRequestLine(char *           const requestLine,
                 TMethod *        const httpMethodP,
                 httpVersion *    const httpVersionP,
                 const char **    const hostP,
                 unsigned short * const portP,
                 const char **    const pathP,
                 const char **    const queryP,
                 abyss_bool *     const moreLinesP,
                 uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Modifies *requestLine and returns pointers to its storage!
-----------------------------------------------------------------------------*/
    const char * httpMethodName;
    char * p;

    p = requestLine;

    /* Jump over spaces */
    NextToken((const char **)&p);

    httpMethodName = GetToken(&p);
    if (!httpMethodName)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        char * requestUri;

        if (xmlrpc_streq(httpMethodName, "GET"))
            *httpMethodP = m_get;
        else if (xmlrpc_streq(httpMethodName, "PUT"))
            *httpMethodP = m_put;
        else if (xmlrpc_streq(httpMethodName, "OPTIONS"))
            *httpMethodP = m_options;
        else if (xmlrpc_streq(httpMethodName, "DELETE"))
            *httpMethodP = m_delete;
        else if (xmlrpc_streq(httpMethodName, "POST"))
            *httpMethodP = m_post;
        else if (xmlrpc_streq(httpMethodName, "TRACE"))
            *httpMethodP = m_trace;
        else if (xmlrpc_streq(httpMethodName, "HEAD"))
            *httpMethodP = m_head;
        else
            *httpMethodP = m_unknown;
        
        /* URI and Query Decoding */
        NextToken((const char **)&p);

        
        requestUri = GetToken(&p);
        if (!requestUri)
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            const char * host;
            unsigned short port;
            const char * path;
            const char * query;

            parseRequestUri(requestUri, &host, &port, &path, &query,
                            httpErrorCodeP);

            if (!*httpErrorCodeP) {
                const char * httpVersion;

                NextToken((const char **)&p);
        
                /* HTTP Version Decoding */
                
                httpVersion = GetToken(&p);
                if (httpVersion) {
                    uint32_t vmin, vmaj;
                    if (sscanf(httpVersion, "HTTP/%d.%d", &vmaj, &vmin) != 2)
                        *httpErrorCodeP = 400;  /* Bad Request */
                    else {
                        httpVersionP->major = vmaj;
                        httpVersionP->minor = vmin;
                        *httpErrorCodeP = 0;  /* no error */
                    }
                    *moreLinesP = TRUE;
                } else {
                    /* There is no HTTP version, so this is a single
                       line request.
                    */
                    *httpErrorCodeP = 0;  /* no error */
                    *moreLinesP = FALSE;
                }
                if (*httpErrorCodeP) {
                    xmlrpc_strfree(host);
                    xmlrpc_strfree(path);
                    xmlrpc_strfree(query);
                }
            }
            *hostP = host;
            *portP = port;
            *pathP = path;
            *queryP = query;
        }
    }
}



static void
strtolower(char * const s) {

    char * t;

    t = &s[0];
    while (*t) {
        *t = tolower(*t);
        ++t;
    }
}



static void
getFieldNameToken(char **    const pP,
                  char **    const fieldNameP,
                  uint16_t * const httpErrorCodeP) {
    
    char * fieldName;

    NextToken((const char **)pP);
    
    fieldName = GetToken(pP);
    if (!fieldName)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        if (fieldName[strlen(fieldName)-1] != ':')
            /* Not a valid field name */
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            fieldName[strlen(fieldName)-1] = '\0';  /* remove trailing colon */

            strtolower(fieldName);
            
            *httpErrorCodeP = 0;  /* no error */
            *fieldNameP = fieldName;
        }
    }
}



static void
processHeader(const char * const fieldName,
              char *       const fieldValue,
              TSession *   const sessionP,
              uint16_t *   const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   We may modify *fieldValue, and we put pointers to *fieldValue and
   *fieldName into *sessionP.

   We must fix this some day.  *sessionP should point to individual
   malloc'ed strings.
-----------------------------------------------------------------------------*/
    *httpErrorCodeP = 0;  /* initial assumption */

    if (xmlrpc_streq(fieldName, "connection")) {
        if (xmlrpc_strcaseeq(fieldValue, "keep-alive"))
            sessionP->requestInfo.keepalive = TRUE;
        else
            sessionP->requestInfo.keepalive = FALSE;
    } else if (xmlrpc_streq(fieldName, "host")) {
        if (sessionP->requestInfo.host) {
            xmlrpc_strfree(sessionP->requestInfo.host);
            sessionP->requestInfo.host = NULL;
        }
        parseHostPort(fieldValue, &sessionP->requestInfo.host,
                      &sessionP->requestInfo.port, httpErrorCodeP);
    } else if (xmlrpc_streq(fieldName, "from"))
        sessionP->requestInfo.from = fieldValue;
    else if (xmlrpc_streq(fieldName, "user-agent"))
        sessionP->requestInfo.useragent = fieldValue;
    else if (xmlrpc_streq(fieldName, "referer"))
        sessionP->requestInfo.referer = fieldValue;
    else if (xmlrpc_streq(fieldName, "range")) {
        if (xmlrpc_strneq(fieldValue, "bytes=", 6)) {
            abyss_bool succeeded;
            succeeded = ListAddFromString(&sessionP->ranges, &fieldValue[6]);
            *httpErrorCodeP = succeeded ? 0 : 400;
        }
    } else if (xmlrpc_streq(fieldName, "cookies")) {
        abyss_bool succeeded;
        succeeded = ListAddFromString(&sessionP->cookies, fieldValue);
        *httpErrorCodeP = succeeded ? 0 : 400;
    }
}



static void
readAndProcessHeaders(TSession * const sessionP,
                      uint16_t * const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Read all the HTTP headers from the session *sessionP, which has at
   least one header coming.  Update *sessionP to reflect the
   information in the headers.

   If we find an error in the headers or while trying to read them, we
   return an appropriate HTTP error code as *httpErrorCodeP.  Otherwise,x
   we return *httpErrorCodeP = 0.
-----------------------------------------------------------------------------*/
    abyss_bool endOfHeaders;

    assert(!sessionP->validRequest);
        /* Calling us doesn't make sense if there is already a valid request */

    *httpErrorCodeP = 0;  /* initial assumption */
    endOfHeaders = false;  /* Caller assures us there is at least one header */

    while (!endOfHeaders && !*httpErrorCodeP) {
        char * p;
        abyss_bool succeeded;
        succeeded = ConnReadHeader(sessionP->conn, &p);
        if (!succeeded)
            *httpErrorCodeP = 408;  /* Request Timeout */
        else {
            if (!*p)
                /* We have reached the empty line so all the request
                   was read.
                */
                endOfHeaders = true;
            else {
                char * fieldName;
                getFieldNameToken(&p, &fieldName, httpErrorCodeP);
                if (!*httpErrorCodeP) {
                    char * fieldValue;
                    
                    NextToken((const char **)&p);
                    
                    fieldValue = p;
                    
                    TableAdd(&sessionP->request_headers,
                             fieldName, fieldValue);
                    
                    processHeader(fieldName, fieldValue, sessionP,
                                  httpErrorCodeP);
                }
            }
        }
    }
}



void
RequestRead(TSession * const sessionP) {

    uint16_t httpErrorCode;  /* zero for no error */
    char * requestLine;

    readRequestLine(sessionP, &requestLine, &httpErrorCode);
    if (!httpErrorCode) {
        TMethod httpMethod;
        const char * host;
        const char * path;
        const char * query;
        unsigned short port;
        abyss_bool moreHeaders;

        parseRequestLine(requestLine, &httpMethod, &sessionP->version,
                         &host, &port, &path, &query,
                         &moreHeaders, &httpErrorCode);

        if (!httpErrorCode) {
            initRequestInfo(&sessionP->requestInfo, sessionP->version,
                            requestLine,
                            httpMethod, host, port, path, query);

            if (moreHeaders)
                readAndProcessHeaders(sessionP, &httpErrorCode);

            if (httpErrorCode == 0)
                sessionP->validRequest = true;

            xmlrpc_strfreenull(host);
            xmlrpc_strfree(path);
            xmlrpc_strfreenull(query);
        }
    }
    if (httpErrorCode)
        ResponseStatus(sessionP, httpErrorCode);
}



char *
RequestHeaderValue(TSession *   const sessionP,
                   const char * const name) {

    return (TableFind(&sessionP->request_headers, name));
}



abyss_bool
RequestValidURI(TSession * const sessionP) {

    if (!sessionP->requestInfo.uri)
        return FALSE;
    
    if (xmlrpc_streq(sessionP->requestInfo.uri, "*"))
        return (sessionP->requestInfo.method != m_options);

    if (strchr(sessionP->requestInfo.uri, '*'))
        return FALSE;

    return TRUE;
}



abyss_bool
RequestValidURIPath(TSession * const sessionP) {

    uint32_t i;
    const char * p;

    p = sessionP->requestInfo.uri;

    i = 0;

    if (*p == '/') {
        i = 1;
        while (*p)
            if (*(p++) == '/') {
                if (*p == '/')
                    break;
                else if ((strncmp(p,"./",2) == 0) || (strcmp(p, ".") == 0))
                    ++p;
                else if ((strncmp(p, "../", 2) == 0) ||
                         (strcmp(p, "..") == 0)) {
                    p += 2;
                    --i;
                    if (i == 0)
                        break;
                }
                /* Prevent accessing hidden files (starting with .) */
                else if (*p == '.')
                    return FALSE;
                else
                    if (*p)
                        ++i;
            }
    }
    return (*p == 0 && i > 0);
}



abyss_bool
RequestAuth(TSession *r,char *credential,char *user,char *pass) {

    char *p,*x;
    char z[80],t[80];

    p=RequestHeaderValue(r,"authorization");
    if (p) {
        NextToken((const char **)&p);
        x=GetToken(&p);
        if (x) {
            if (strcasecmp(x,"basic")==0) {
                NextToken((const char **)&p);
                sprintf(z,"%s:%s",user,pass);
                Base64Encode(z,t);

                if (strcmp(p,t)==0) {
                    r->requestInfo.user=strdup(user);
                    return TRUE;
                };
            };
        }
    };

    sprintf(z,"Basic realm=\"%s\"",credential);
    ResponseAddField(r,"WWW-Authenticate",z);
    ResponseStatus(r,401);
    return FALSE;
}



/*********************************************************************
** Range
*********************************************************************/

abyss_bool RangeDecode(char *str,uint64_t filesize,uint64_t *start,uint64_t *end)
{
    char *ss;

    *start=0;
    *end=filesize-1;

    if (*str=='-')
    {
        *start=filesize-strtol(str+1,&ss,10);
        return ((ss!=str) && (!*ss));
    };

    *start=strtol(str,&ss,10);

    if ((ss==str) || (*ss!='-'))
        return FALSE;

    str=ss+1;

    if (!*str)
        return TRUE;

    *end=strtol(str,&ss,10);

    if ((ss==str) || (*ss) || (*end<*start))
        return FALSE;

    return TRUE;
}

/*********************************************************************
** HTTP
*********************************************************************/

const char *
HTTPReasonByStatus(uint16_t const code) {

    struct _HTTPReasons {
        uint16_t status;
        const char * reason;
    };

    static struct _HTTPReasons const reasons[] =  {
        { 100,"Continue" }, 
        { 101,"Switching Protocols" }, 
        { 200,"OK" }, 
        { 201,"Created" }, 
        { 202,"Accepted" }, 
        { 203,"Non-Authoritative Information" }, 
        { 204,"No Content" }, 
        { 205,"Reset Content" }, 
        { 206,"Partial Content" }, 
        { 300,"Multiple Choices" }, 
        { 301,"Moved Permanently" }, 
        { 302,"Moved Temporarily" }, 
        { 303,"See Other" }, 
        { 304,"Not Modified" }, 
        { 305,"Use Proxy" }, 
        { 400,"Bad Request" }, 
        { 401,"Unauthorized" }, 
        { 402,"Payment Required" }, 
        { 403,"Forbidden" }, 
        { 404,"Not Found" }, 
        { 405,"Method Not Allowed" }, 
        { 406,"Not Acceptable" }, 
        { 407,"Proxy Authentication Required" }, 
        { 408,"Request Timeout" }, 
        { 409,"Conflict" }, 
        { 410,"Gone" }, 
        { 411,"Length Required" }, 
        { 412,"Precondition Failed" }, 
        { 413,"Request Entity Too Large" }, 
        { 414,"Request-URI Too Long" }, 
        { 415,"Unsupported Media Type" }, 
        { 500,"Internal Server Error" }, 
        { 501,"Not Implemented" }, 
        { 502,"Bad Gateway" }, 
        { 503,"Service Unavailable" }, 
        { 504,"Gateway Timeout" }, 
        { 505,"HTTP Version Not Supported" },
        { 000, NULL }
    };
    const struct _HTTPReasons * reasonP;

    reasonP = &reasons[0];

    while (reasonP->status <= code)
        if (reasonP->status == code)
            return reasonP->reason;
        else
            ++reasonP;

    return "No Reason";
}



int32_t
HTTPRead(TSession *   const s ATTR_UNUSED,
         const char * const buffer ATTR_UNUSED,
         uint32_t     const len ATTR_UNUSED) {

    return 0;
}



abyss_bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len) {

    abyss_bool succeeded;

    if (sessionP->chunkedwrite && sessionP->chunkedwritemode) {
        char chunkHeader[16];

        sprintf(chunkHeader, "%x\r\n", len);

        succeeded =
            ConnWrite(sessionP->conn, chunkHeader, strlen(chunkHeader));
        if (succeeded) {
            succeeded = ConnWrite(sessionP->conn, buffer, len);
            if (succeeded)
                succeeded = ConnWrite(sessionP->conn, "\r\n", 2);
        }
    } else
        succeeded = ConnWrite(sessionP->conn, buffer, len);

    return succeeded;
}



abyss_bool
HTTPWriteEndChunk(TSession * const sessionP) {

    abyss_bool retval;

    if (sessionP->chunkedwritemode && sessionP->chunkedwrite) {
        /* May be one day trailer dumping will be added */
        sessionP->chunkedwritemode = FALSE;
        retval = ConnWrite(sessionP->conn, "0\r\n\r\n", 5);
    } else
        retval = TRUE;

    return retval;
}



abyss_bool
HTTPKeepalive(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Return value: the connection should be kept alive after the session
   *sessionP is over.
-----------------------------------------------------------------------------*/
    return (sessionP->requestInfo.keepalive &&
            !sessionP->serverDeniesKeepalive &&
            sessionP->status < 400);
}



/******************************************************************************
**
** http.c
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
******************************************************************************/

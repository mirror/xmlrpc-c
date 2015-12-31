/* Copyright information is at the end of the file */

#define _XOPEN_SOURCE 600   /* For strdup() */
#define _BSD_SOURCE   /* For xmlrpc_strcaseeq() */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"
#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base64_int.h"
#include "xmlrpc-c/abyss.h"

#include "server.h"
#include "session.h"
#include "conn.h"
#include "token.h"
#include "date.h"
#include "data.h"

#include "http.h"



const char *
RequestHeaderValue(TSession *   const sessionP,
                   const char * const name) {

    return TableValue(&sessionP->requestHeaderFields, name);
}



bool
HTTPRequestHasValidUri(TSession * const sessionP) {

    if (!sessionP->requestInfo.uri)
        return false;
    
    if (xmlrpc_streq(sessionP->requestInfo.uri, "*"))
        return (sessionP->requestInfo.method != m_options);

    if (strchr(sessionP->requestInfo.uri, '*'))
        return false;

    return true;
}



bool
HTTPRequestHasValidUriPath(TSession * const sessionP) {

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
                else if ((xmlrpc_strneq(p,"./", 2)) || (xmlrpc_streq(p, ".")))
                    ++p;
                else if ((xmlrpc_strneq(p, "../", 2)) ||
                         (xmlrpc_streq(p, ".."))) {
                    p += 2;
                    --i;
                    if (i == 0)
                        break;
                }
                /* Prevent accessing hidden files (starting with .) */
                else if (*p == '.')
                    return false;
                else
                    if (*p)
                        ++i;
            }
    }
    return (*p == 0 && i > 0);
}



abyss_bool
RequestAuth(TSession *   const sessionP,
            const char * const credential,
            const char * const user,
            const char * const pass) {
/*----------------------------------------------------------------------------
   Authenticate requester, in a very simplistic fashion.

   If the request executing on session *sessionP specifies basic
   authentication (via Authorization header) with username 'user', password
   'pass', then return true.  Else, return false and set up an authorization
   failure response (HTTP response status 401) that says user must supply an
   identity in the 'credential' domain.

   When we return true, we also set the username in the request info for the
   session to 'user' so that a future SessionGetRequestInfo can get it.
-----------------------------------------------------------------------------*/
    bool authorized;
    const char * authValue;

    authValue = RequestHeaderValue(sessionP, "authorization");
    if (authValue) {
        char * const valueBuffer = malloc(strlen(authValue));
            /* A buffer we can mangle as we parse the authorization: value */

        if (!authValue)
            /* Should return error, but we have no way to do that */
            authorized = false;
        else {
            const char * authType;
            char * authHdrPtr;

            strcpy(valueBuffer, authValue);
            authHdrPtr = &valueBuffer[0];

            NextToken((const char **)&authHdrPtr);
            GetTokenConst(&authHdrPtr, &authType);
            if (authType) {
                if (xmlrpc_strcaseeq(authType, "basic")) {
                    const char * userPass;
                    char userPassEncoded[80];
    
                    NextToken((const char **)&authHdrPtr);
    
                    xmlrpc_asprintf(&userPass, "%s:%s", user, pass);
                    xmlrpc_base64Encode(userPass, userPassEncoded);
                    xmlrpc_strfree(userPass);
    
                    if (xmlrpc_streq(authHdrPtr, userPassEncoded)) {
                        sessionP->requestInfo.user = xmlrpc_strdupsol(user);
                        authorized = true;
                    } else
                        authorized = false;
                } else
                    authorized = false;
            } else
                authorized = false;

            free(valueBuffer);
        }
    } else
        authorized = false;

    if (!authorized) {
        const char * hdrValue;
        xmlrpc_asprintf(&hdrValue, "Basic realm=\"%s\"", credential);
        ResponseAddField(sessionP, "WWW-Authenticate", hdrValue);

        xmlrpc_strfree(hdrValue);

        ResponseStatus(sessionP, 401);
    }
    return authorized;
}



/*********************************************************************
** Range
*********************************************************************/

abyss_bool
RangeDecode(char *            const strArg,
            xmlrpc_uint64_t   const filesize,
            xmlrpc_uint64_t * const start,
            xmlrpc_uint64_t * const end) {

    char *str;
    char *ss;

    str = strArg;  /* initial value */

    *start=0;
    *end=filesize-1;

    if (*str=='-')
    {
        *start=filesize-strtol(str+1,&ss,10);
        return ((ss!=str) && (!*ss));
    };

    *start=strtol(str,&ss,10);

    if ((ss==str) || (*ss!='-'))
        return false;

    str=ss+1;

    if (!*str)
        return true;

    *end=strtol(str,&ss,10);

    if ((ss==str) || (*ss) || (*end<*start))
        return false;

    return true;
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



bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len) {

    bool succeeded;

    if (sessionP->chunkedwrite && sessionP->chunkedwritemode) {
        char chunkHeader[16];

        sprintf(chunkHeader, "%x\r\n", len);

        succeeded =
            ConnWrite(sessionP->connP, chunkHeader, strlen(chunkHeader));
        if (succeeded) {
            succeeded = ConnWrite(sessionP->connP, buffer, len);
            if (succeeded)
                succeeded = ConnWrite(sessionP->connP, "\r\n", 2);
        }
    } else
        succeeded = ConnWrite(sessionP->connP, buffer, len);

    return succeeded;
}



bool
HTTPWriteEndChunk(TSession * const sessionP) {

    bool retval;

    if (sessionP->chunkedwrite && sessionP->chunkedwritemode) {
        /* May be one day trailer dumping will be added */
        sessionP->chunkedwritemode = false;
        retval = ConnWrite(sessionP->connP, "0\r\n\r\n", 5);
    } else
        retval = true;

    return retval;
}



bool
HTTPKeepalive(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Return value: the connection should be kept alive after the session
   *sessionP is over.
-----------------------------------------------------------------------------*/
    return (sessionP->requestInfo.keepalive &&
            !sessionP->serverDeniesKeepalive &&
            sessionP->status < 400);
}



bool
HTTPWriteContinue(TSession * const sessionP) {

    char const continueStatus[] = "HTTP/1.1 100 continue\r\n\r\n";
        /* This is a status line plus an end-of-headers empty line */

    return ConnWrite(sessionP->connP, continueStatus, strlen(continueStatus));
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

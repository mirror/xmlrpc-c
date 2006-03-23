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
#include "abyss_info.h"

#include "http.h"

/*********************************************************************
** Request Parser
*********************************************************************/

/*********************************************************************
** Request
*********************************************************************/

static void
initRequestInfo(TRequestInfo * const requestInfoP) {

    requestInfoP->query     = NULL;
    requestInfoP->host      = NULL;
    requestInfoP->from      = NULL;
    requestInfoP->useragent = NULL;
    requestInfoP->referer   = NULL;
    requestInfoP->user      = NULL;
    requestInfoP->requestline=NULL;

}



static void
freeRequestInfo(TRequestInfo * const requestInfoP) {

    if (requestInfoP->requestline)
        xmlrpc_strfree(requestInfoP->requestline);

    if (requestInfoP->user)
        xmlrpc_strfree(requestInfoP->user);
}



void
RequestInit(TSession * const sessionP,
            TConn *    const connectionP) {

    time_t nowtime;

    time(&nowtime);
    sessionP->date = *gmtime(&nowtime);

    sessionP->keepalive = FALSE;
    initRequestInfo(&sessionP->request_info);

    sessionP->versionmajor=0;
    sessionP->versionminor=9;
    sessionP->conn = connectionP;

    sessionP->done = FALSE;

    sessionP->chunkedwrite = FALSE;
    sessionP->chunkedwritemode = FALSE;

    ListInit(&sessionP->cookies);
    ListInit(&sessionP->ranges);
    TableInit(&sessionP->request_headers);
    TableInit(&sessionP->response_headers);

    sessionP->status=0;

    StringAlloc(&(sessionP->header));
}



void
RequestFree(TSession * const sessionP) {

    freeRequestInfo(&sessionP->request_info);

    ListFree(&sessionP->cookies);
    ListFree(&sessionP->ranges);
    TableFree(&sessionP->request_headers);
    TableFree(&sessionP->response_headers);
    StringFree(&(sessionP->header));
}



static void
readRequestLine(TSession *   const sessionP,
                char **      const requestLineP,
                uint16_t *   const httpErrorCodeP) {

    *httpErrorCodeP = 0;

    /* Ignore CRLFs in the beginning of the request (RFC2068-P30) */
    do {
        abyss_bool success;
        success = ConnReadHeader(sessionP->conn, requestLineP);
        if (!success)
            *httpErrorCodeP = 408;  /* Request Timeout */
    } while ((*requestLineP)[0] == '\0' && !*httpErrorCodeP);
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
parseHostPort(char *     const hostport,
              TSession*  const sessionP,
              uint16_t * const httpErrorCodeP) {
    
    char * colonPos;

    colonPos = strchr(hostport, ':');
    if (colonPos) {
        const char * p;
        uint32_t port;

        *colonPos = '\0';  /* Split hostport at the colon */

        sessionP->request_info.host = hostport;

        for (p = colonPos + 1, port = 0;
             isdigit(*p) && port < 65535;
             (port = port * 10 + (*p - '0')), ++p);
            
        sessionP->port = port;

        if (*p || port == 0)
            *httpErrorCodeP = 400;  /* Bad Request */
        else
            *httpErrorCodeP = 0;
    } else {
        sessionP->request_info.host = hostport;
        sessionP->port = 80;
        *httpErrorCodeP = 0;
    }
}



static void
parseRequestUri(char *     const requestUri,
                TSession * const sessionP,
                uint16_t * const httpErrorCodeP) {
/*----------------------------------------------------------------------------
  Parse the request URI (in the request line
  "GET http://www.myserver.com/myfile?parm HTTP/1.1",
  "http://www.myserver.com/myfile?parm" is the request URI).

  Place the information it contains in *sessionP.

  This destroys *requestUri and puts pointers into *requestUri into
  *sessionP!

  This is extremely ugly.  We need to redo it with dynamically allocated
  storage.  *sessionP should point to individual malloc'ed strings.
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
                sessionP->request_info.query = qmark + 1;
            } else
                sessionP->request_info.query = NULL;
        }
        
        requestUriNoQuery = requestUri;

        if (requestUriNoQuery[0] == '/') {
            sessionP->request_info.host = NULL;
            sessionP->request_info.uri = requestUriNoQuery;
        } else {
            if (!xmlrpc_strneq(requestUriNoQuery, "http://", 7))
                *httpErrorCodeP = 400;  /* Bad Request */
            else {
                char * const hostportpath = &requestUriNoQuery[7];
                char * const slashPos =
                    strchr(sessionP->request_info.uri, '/');
                char * hostport;
                
                if (slashPos) {
                    char * p;
                    sessionP->request_info.uri = slashPos;
                    
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
                    sessionP->request_info.uri = "*";
                    hostport = hostportpath;
                    *httpErrorCodeP = 0;
                }
                if (!*httpErrorCodeP)
                    parseHostPort(hostport, sessionP, httpErrorCodeP);
            }
        }
    }
}



static void
processRequestLine(TSession *   const sessionP,
                   char *       const header1,
                   abyss_bool * const moreLinesP,
                   uint16_t *   const httpErrorCodeP) {
    
    const char * httpMethodName;
    char * p;

    p = header1;

    /* Jump over spaces */
    NextToken((const char **)&p);

    sessionP->request_info.requestline = strdup(p);
    
    httpMethodName = GetToken(&p);
    if (!httpMethodName)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        char * requestUri;

        if (xmlrpc_streq(httpMethodName, "GET"))
            sessionP->request_info.method = m_get;
        else if (xmlrpc_streq(httpMethodName, "PUT"))
            sessionP->request_info.method = m_put;
        else if (xmlrpc_streq(httpMethodName, "OPTIONS"))
            sessionP->request_info.method = m_options;
        else if (xmlrpc_streq(httpMethodName, "DELETE"))
            sessionP->request_info.method = m_delete;
        else if (xmlrpc_streq(httpMethodName, "POST"))
            sessionP->request_info.method = m_post;
        else if (xmlrpc_streq(httpMethodName, "TRACE"))
            sessionP->request_info.method = m_trace;
        else if (xmlrpc_streq(httpMethodName, "HEAD"))
            sessionP->request_info.method = m_head;
        else
            sessionP->request_info.method = m_unknown;
        
        /* URI and Query Decoding */
        NextToken((const char **)&p);

        
        requestUri = GetToken(&p);
        if (!requestUri)
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            parseRequestUri(requestUri, sessionP, httpErrorCodeP);

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
                        sessionP->versionmajor = vmaj;
                        sessionP->versionminor = vmin;
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
            }
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
        /* must handle the jigsaw TE,keepalive */
        if (xmlrpc_strcaseeq(fieldValue, "keep-alive"))
            sessionP->keepalive = TRUE;
        else
            sessionP->keepalive = FALSE;
    } else if (xmlrpc_streq(fieldName, "host"))
        parseHostPort(fieldValue, sessionP, httpErrorCodeP);
    else if (xmlrpc_streq(fieldName, "from"))
        sessionP->request_info.from = fieldValue;
    else if (xmlrpc_streq(fieldName, "user-agent"))
        sessionP->request_info.useragent = fieldValue;
    else if (xmlrpc_streq(fieldName, "referer"))
        sessionP->request_info.referer = fieldValue;
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



abyss_bool
RequestRead(TSession * const sessionP) {
    uint16_t httpErrorCode;  /* zero for no error */
    char * requestLine;

    readRequestLine(sessionP, &requestLine, &httpErrorCode);
    if (!httpErrorCode) {
        abyss_bool moreHeaders;
        processRequestLine(sessionP, requestLine,
                           &moreHeaders, &httpErrorCode);

        while (moreHeaders && !httpErrorCode) {
            char * p;
            abyss_bool succeeded;
            succeeded = ConnReadHeader(sessionP->conn, &p);
            if (!succeeded)
                httpErrorCode = 408;  /* Request Timeout */
            else {
                if (!*p)
                    /* We have reached the empty line so all the request
                       was read.
                    */
                    moreHeaders = FALSE;
                else {
                    char * fieldName;
                    getFieldNameToken(&p, &fieldName, &httpErrorCode);
                    if (!httpErrorCode) {
                        char * fieldValue;

                        NextToken((const char **)&p);
                        
                        fieldValue = p;
                        
                        TableAdd(&sessionP->request_headers,
                                 fieldName, fieldValue);
                        
                        processHeader(fieldName, fieldValue, sessionP,
                                      &httpErrorCode);
                    }
                }
            }
        }
    }
    if (httpErrorCode)
        ResponseStatus(sessionP, httpErrorCode);

    return !httpErrorCode;
}



char *RequestHeaderValue(TSession *r,char *name)
{
    return (TableFind(&r->request_headers,name));
}



abyss_bool
RequestValidURI(TSession * const sessionP) {

    if (!sessionP->request_info.uri)
        return FALSE;
    
    if (xmlrpc_streq(sessionP->request_info.uri, "*"))
        return (sessionP->request_info.method != m_options);

    if (strchr(sessionP->request_info.uri, '*'))
        return FALSE;

    return TRUE;
}



abyss_bool
RequestValidURIPath(TSession * const sessionP) {

    uint32_t i;
    const char * p;

    p = sessionP->request_info.uri;

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
                    r->request_info.user=strdup(user);
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

static const char *
HTTPReasonByStatus(uint16_t const code) {

    static struct _HTTPReasons {
        uint16_t status;
        const char * reason;
    } *r, reasons[] = 
    {
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

    r = &reasons[0];

    while (r->status <= code)
        if (r->status == code)
            return r->reason;
        else
            ++r;

    return "No Reason";
}



int32_t
HTTPRead(TSession *   const s ATTR_UNUSED,
         const char * const buffer ATTR_UNUSED,
         uint32_t     const len ATTR_UNUSED) {

    return 0;
}



abyss_bool
HTTPWrite(TSession *   const s,
          const char * const buffer,
          uint32_t     const len) {

    if (s->chunkedwrite && s->chunkedwritemode)
    {
        char t[16];

        if (ConnWrite(s->conn,t,sprintf(t,"%x"CRLF,len)))
            if (ConnWrite(s->conn,buffer,len))
                return ConnWrite(s->conn,CRLF,2);

        return FALSE;
    }
    
    return ConnWrite(s->conn,buffer,len);
}

abyss_bool HTTPWriteEnd(TSession *s)
{
    if (!s->chunkedwritemode)
        return TRUE;

    if (s->chunkedwrite)
    {
        /* May be one day trailer dumping will be added */
        s->chunkedwritemode=FALSE;
        return ConnWrite(s->conn,"0"CRLF CRLF,5);
    }

    s->keepalive=FALSE;
    return TRUE;
}

/*********************************************************************
** Response
*********************************************************************/

void
ResponseError(TSession * const sessionP) {

    const char * const reason = HTTPReasonByStatus(sessionP->status);
    char z[500];

    ResponseAddField(sessionP, "Content-type", "text/html");

    ResponseWrite(sessionP);
    
    sprintf(z,
            "<HTML><HEAD><TITLE>Error %d</TITLE></HEAD>"
            "<BODY><H1>Error %d</H1><P>%s</P>" SERVER_HTML_INFO 
            "</BODY></HTML>",
            sessionP->status, sessionP->status, reason);

    ConnWrite(sessionP->conn, z, strlen(z)); 
}



abyss_bool ResponseChunked(TSession *r)
{
    /* This is only a hope, things will be real only after a call of
       ResponseWrite
    */
    r->chunkedwrite=(r->versionmajor>=1) && (r->versionminor>=1);
    r->chunkedwritemode=TRUE;

    return TRUE;
}

void ResponseStatus(TSession *r,uint16_t code)
{
    r->status=code;
}



uint16_t
ResponseStatusFromErrno(int const errnoArg) {

    uint16_t code;

    switch (errnoArg) {
    case EACCES:
        code=403;
        break;
    case ENOENT:
        code=404;
        break;
    default:
        code=500;
    }
    return code;
}



void
ResponseStatusErrno(TSession * const sessionP) {

    ResponseStatus(sessionP, ResponseStatusFromErrno(errno));
}



abyss_bool
ResponseAddField(TSession *   const sessionP,
                 const char * const name,
                 const char * const value) {

    return TableAdd(&sessionP->response_headers, name, value);
}



void
ResponseWrite(TSession * const sessionP) {

    struct _TServer * const srvP = ConnServer(sessionP->conn)->srvP;

    abyss_bool connclose=TRUE;
    char z[64];
    unsigned int i;

    /* if status == 0 then this is an error */
    if (sessionP->status == 0)
        sessionP->status = 500;

    /* the request was treated */
    sessionP->done = TRUE;

    {
        const char * const reason = HTTPReasonByStatus(sessionP->status);
        sprintf(z,"HTTP/1.1 %d ", sessionP->status);
        ConnWrite(sessionP->conn, z, strlen(z));
        ConnWrite(sessionP->conn, reason, strlen(reason));
        ConnWrite(sessionP->conn, CRLF, 2);
    }
    /* generation of the connection field */
    if (sessionP->status < 400 && sessionP->keepalive)
        connclose = FALSE;

    ResponseAddField(sessionP, "Connection",
                     (connclose ? "close" : "Keep-Alive"));

    if (!connclose) {
        sprintf(z, "timeout=%u, max=%u",
                srvP->keepalivetimeout, srvP->keepalivemaxconn);

        ResponseAddField(sessionP, "Keep-Alive", z);

        if (sessionP->chunkedwrite && sessionP->chunkedwritemode) {
            if (!ResponseAddField(sessionP, "Transfer-Encoding", "chunked")) {
                sessionP->chunkedwrite = FALSE;
                sessionP->keepalive    = FALSE;
            }
        }
    } else {
        sessionP->chunkedwrite =FALSE;
        sessionP->keepalive    =FALSE;
    }
    
    /* generation of the date field */
    if ((sessionP->status >= 200) && DateToString(&sessionP->date, z))
        ResponseAddField(sessionP, "Date", z);

    /* Generation of the server field */
    if (srvP->advertise)
        ResponseAddField(sessionP, "Server", SERVER_HVERSION);

    /* send all the fields */
    for (i = 0; i < sessionP->response_headers.size; ++i) {
        TTableItem * const ti = &sessionP->response_headers.item[i];
        ConnWrite(sessionP->conn, ti->name, strlen(ti->name));
        ConnWrite(sessionP->conn, ": ", 2);
        ConnWrite(sessionP->conn, ti->value, strlen(ti->value));
        ConnWrite(sessionP->conn, CRLF, 2);
    }

    ConnWrite(sessionP->conn, CRLF, 2);  
}



abyss_bool
ResponseWriteBody(TSession *   const sessionP,
                  const char * const data,
                  uint32_t     const len) {

    return HTTPWrite(sessionP, data, len);
}



abyss_bool
ResponseWriteEnd(TSession * const sessionP) {

    return HTTPWriteEnd(sessionP);
}



abyss_bool
ResponseContentType(TSession *   const serverP,
                    const char * const type) {

    return ResponseAddField(serverP, "Content-type", type);
}



abyss_bool ResponseContentLength(TSession *r,uint64_t len)
{
    char z[32];

    sprintf(z,"%llu",len);
    return ResponseAddField(r,"Content-length",z);
}


/*********************************************************************
** MIMEType
*********************************************************************/

struct MIMEType {
    TList typeList;
    TList extList;
    TPool pool;
};


static MIMEType * globalMimeTypeP = NULL;



MIMEType *
MIMETypeCreate(void) {
 
    MIMEType * MIMETypeP;

    MALLOCVAR(MIMETypeP);

    if (MIMETypeP) {
        ListInit(&MIMETypeP->typeList);
        ListInit(&MIMETypeP->extList);
        PoolCreate(&MIMETypeP->pool, 1024);
    }
    return MIMETypeP;
}



void
MIMETypeDestroy(MIMEType * const MIMETypeP) {

    PoolFree(&MIMETypeP->pool);
}



void
MIMETypeInit(void) {

    if (globalMimeTypeP != NULL)
        abort();

    globalMimeTypeP = MIMETypeCreate();
}



void
MIMETypeTerm(void) {

    if (globalMimeTypeP == NULL)
        abort();

    MIMETypeDestroy(globalMimeTypeP);

    globalMimeTypeP = NULL;
}



static void
mimeTypeAdd(MIMEType *   const MIMETypeP,
            const char * const type,
            const char * const ext,
            abyss_bool * const successP) {
    
    uint16_t index;
    void * mimeTypesItem;
    abyss_bool typeIsInList;

    assert(MIMETypeP != NULL);

    typeIsInList = ListFindString(&MIMETypeP->typeList, type, &index);
    if (typeIsInList)
        mimeTypesItem = MIMETypeP->typeList.item[index];
    else
        mimeTypesItem = (void*)PoolStrdup(&MIMETypeP->pool, type);

    if (mimeTypesItem) {
        abyss_bool extIsInList;
        extIsInList = ListFindString(&MIMETypeP->extList, ext, &index);
        if (extIsInList) {
            MIMETypeP->typeList.item[index] = mimeTypesItem;
            *successP = TRUE;
        } else {
            void * extItem = (void*)PoolStrdup(&MIMETypeP->pool, ext);
            if (extItem) {
                abyss_bool addedToMimeTypes;

                addedToMimeTypes =
                    ListAdd(&MIMETypeP->typeList, mimeTypesItem);
                if (addedToMimeTypes) {
                    abyss_bool addedToExt;
                    
                    addedToExt = ListAdd(&MIMETypeP->extList, extItem);
                    *successP = addedToExt;
                    if (!*successP)
                        ListRemove(&MIMETypeP->typeList);
                } else
                    *successP = FALSE;
                if (!*successP)
                    PoolReturn(&MIMETypeP->pool, extItem);
            } else
                *successP = FALSE;
        }
    } else
        *successP = FALSE;
}




abyss_bool
MIMETypeAdd2(MIMEType *   const MIMETypeArg,
             const char * const type,
             const char * const ext) {

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    abyss_bool success;

    if (MIMETypeP == NULL)
        success = FALSE;
    else 
        mimeTypeAdd(MIMETypeP, type, ext, &success);

    return success;
}



abyss_bool
MIMETypeAdd(const char * const type,
            const char * const ext) {

    return MIMETypeAdd2(globalMimeTypeP, type, ext);
}



static const char *
mimeTypeFromExt(MIMEType *   const MIMETypeP,
                const char * const ext) {

    const char * retval;
    uint16_t extindex;
    abyss_bool extIsInList;

    assert(MIMETypeP != NULL);

    extIsInList = ListFindString(&MIMETypeP->extList, ext, &extindex);
    if (!extIsInList)
        retval = NULL;
    else
        retval = MIMETypeP->typeList.item[extindex];
    
    return retval;
}



const char *
MIMETypeFromExt2(MIMEType *   const MIMETypeArg,
                 const char * const ext) {

    const char * retval;

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromExt(MIMETypeP, ext);

    return retval;
}



const char *
MIMETypeFromExt(const char * const ext) {

    return MIMETypeFromExt2(globalMimeTypeP, ext);
}



static void
findExtension(const char *  const fileName,
              const char ** const extP) {

    unsigned int extPos = 0;  /* stifle unset variable warning */
        /* Running estimation of where in fileName[] the extension starts */
    abyss_bool extFound;
    unsigned int i;

    /* We're looking for the last dot after the last slash */
    for (i = 0, extFound = FALSE; fileName[i]; ++i) {
        char const c = fileName[i];
        
        if (c == '.') {
            extFound = TRUE;
            extPos = i + 1;
        }
        if (c == '/')
            extFound = FALSE;
    }

    if (extFound)
        *extP = &fileName[extPos];
    else
        *extP = NULL;
}



static const char *
mimeTypeFromFileName(MIMEType *   const MIMETypeP,
                     const char * const fileName) {

    const char * retval;
    const char * ext;

    assert(MIMETypeP != NULL);
    
    findExtension(fileName, &ext);

    if (ext)
        retval = MIMETypeFromExt2(MIMETypeP, ext);
    else
        retval = "application/octet-stream";

    return retval;
}



const char *
MIMETypeFromFileName2(MIMEType *   const MIMETypeArg,
                      const char * const fileName) {

    const char * retval;
    
    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromFileName(MIMETypeP, fileName);

    return retval;
}



const char *
MIMETypeFromFileName(const char * const fileName) {

    return MIMETypeFromFileName2(globalMimeTypeP, fileName);
}



static abyss_bool
fileContainsText(const char * const fileName) {
/*----------------------------------------------------------------------------
   Return true iff we can read the contents of the file named 'fileName'
   and see that it appears to be composed of plain text characters.
-----------------------------------------------------------------------------*/
    abyss_bool retval;
    abyss_bool fileOpened;
    TFile file;

    fileOpened = FileOpen(&file, fileName, O_BINARY | O_RDONLY);
    if (fileOpened) {
        char const ctlZ = 26;
        unsigned char buffer[80];
        int32_t readRc;
        unsigned int i;

        readRc = FileRead(&file, buffer, sizeof(buffer));
       
        if (readRc >= 0) {
            unsigned int bytesRead = readRc;
            abyss_bool nonTextFound;

            nonTextFound = FALSE;  /* initial value */
    
            for (i = 0; i < bytesRead; ++i) {
                char const c = buffer[i];
                if (c < ' ' && !isspace(c) && c != ctlZ)
                    nonTextFound = TRUE;
            }
            retval = !nonTextFound;
        } else
            retval = FALSE;
        FileClose(&file);
    } else
        retval = FALSE;

    return retval;
}


 
static const char *
mimeTypeGuessFromFile(MIMEType *   const MIMETypeP,
                      const char * const fileName) {

    const char * retval;
    const char * ext;

    findExtension(fileName, &ext);

    retval = NULL;

    if (ext && MIMETypeP)
        retval = MIMETypeFromExt2(MIMETypeP, ext);
    
    if (!retval) {
        if (fileContainsText(fileName))
            retval = "text/plain";
        else
            retval = "application/octet-stream";  
    }
    return retval;
}



const char *
MIMETypeGuessFromFile2(MIMEType *   const MIMETypeArg,
                       const char * const fileName) {

    return mimeTypeGuessFromFile(MIMETypeArg ? MIMETypeArg : globalMimeTypeP,
                                 fileName);
}



const char *
MIMETypeGuessFromFile(const char * const fileName) {

    return mimeTypeGuessFromFile(globalMimeTypeP, fileName);
}

                                  

/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d)
{
    /* Conversion table. */
    static char tbl[64] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3',
        '4','5','6','7','8','9','+','/'
    };

    uint32_t i,length=strlen(s);
    char *p=d;
    
    /* Transform the 3x8 bits to 4x6 bits, as required by base64. */
    for (i = 0; i < length; i += 3)
    {
        *p++ = tbl[s[0] >> 2];
        *p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
        *p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
        *p++ = tbl[s[2] & 0x3f];
        s += 3;
    }
    
    /* Pad the result if necessary... */
    if (i == length + 1)
        *(p - 1) = '=';
    else if (i == length + 2)
        *(p - 1) = *(p - 2) = '=';
    
    /* ...and zero-terminate it. */
    *p = '\0';
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

/*****************************************************************************
                                 abyss.h
******************************************************************************

  This file is the interface header for the Abyss HTTP server component of
  XML-RPC For C/C++ (Xmlrpc-c).

  The Abyss component of Xmlrpc-c is based on the independently developed
  and distributed Abyss web server package from 2001.

  Copyright information is at the end of the file.
****************************************************************************/

#ifndef _ABYSS_H_
#define _ABYSS_H_


#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#include "xmlrpc_config.h"
#else
#include <inttypes.h>
#endif
/*********************************************************************
** Paths and so on...
*********************************************************************/

#ifdef WIN32
#define DEFAULT_ROOT        "c:\\abyss"
#define DEFAULT_DOCS        DEFAULT_ROOT"\\htdocs"
#define DEFAULT_CONF_FILE   DEFAULT_ROOT"\\conf\\abyss.conf"
#define DEFAULT_LOG_FILE    DEFAULT_ROOT"\\log\\abyss.log"
#else
#ifdef __rtems__
#define DEFAULT_ROOT        "/abyss"
#else
#define DEFAULT_ROOT        "/usr/local/abyss"
#endif
#define DEFAULT_DOCS        DEFAULT_ROOT"/htdocs"
#define DEFAULT_CONF_FILE   DEFAULT_ROOT"/conf/abyss.conf"
#define DEFAULT_LOG_FILE    DEFAULT_ROOT"/log/abyss.log"
#endif

/*********************************************************************
** Maximum numer of simultaneous connections
*********************************************************************/

#define MAX_CONN    16

/*********************************************************************
** Server Info Definitions
*********************************************************************/

#define SERVER_VERSION      "0.3"
#define SERVER_HVERSION     "ABYSS/0.3"
#define SERVER_HTML_INFO \
  "<p><HR><b><i><a href=\"http:\057\057abyss.linuxave.net\">" \
  "ABYSS Web Server</a></i></b> version "SERVER_VERSION"<br>" \
  "&copy; <a href=\"mailto:mmoez@bigfoot.com\">Moez Mahfoudh</a> - 2000</p>"
#define SERVER_PLAIN_INFO \
  CRLF "----------------------------------------" \
       "----------------------------------------" \
  CRLF "ABYSS Web Server version "SERVER_VERSION CRLF"(C) Moez Mahfoudh - 2000"

/*********************************************************************
** General purpose definitions
*********************************************************************/

#ifdef WIN32
#define strcasecmp(a,b) stricmp((a),(b))
#else
#define ioctlsocket(a,b,c)  ioctl((a),(b),(c))
#endif  /* WIN32 */

#ifndef NULL
#define NULL ((void *)0)
#endif  /* NULL */

#ifndef TRUE
#define TRUE    1
#endif  /* TRUE */

#ifndef FALSE
#define FALSE    0
#endif  /* FALSE */

#ifdef WIN32
#define LBR "\n"
#else
#define LBR "\n"
#endif  /* WIN32 */

typedef int abyss_bool;
enum abyss_foreback {ABYSS_FOREGROUND, ABYSS_BACKGROUND};

/*********************************************************************
** Buffer
*********************************************************************/

typedef struct
{
    void *data;
    uint32_t size;
    uint32_t staticid;
} TBuffer;

abyss_bool BufferAlloc(TBuffer *buf,uint32_t memsize);
abyss_bool BufferRealloc(TBuffer *buf,uint32_t memsize);
void BufferFree(TBuffer *buf);


/*********************************************************************
** String
*********************************************************************/

typedef struct
{
    TBuffer buffer;
    uint32_t size;
} TString;

abyss_bool StringAlloc(TString *s);
abyss_bool StringConcat(TString *s,char *s2);
abyss_bool StringBlockConcat(TString *s,char *s2,char **ref);
void StringFree(TString *s);
char *StringData(TString *s);


/*********************************************************************
** List
*********************************************************************/

typedef struct {
    void **item;
    uint16_t size;
    uint16_t maxsize;
    abyss_bool autofree;
} TList;

void
ListInit(TList * const listP);

void
ListInitAutoFree(TList * const listP);

void
ListFree(TList * const listP);

void
ListFreeItems(TList * const listP);

abyss_bool
ListAdd(TList * const listP,
        void *  const str);

void
ListRemove(TList * const listP);

abyss_bool
ListAddFromString(TList * const listP,
                  char *  const c);

abyss_bool
ListFindString(TList *    const listP,
               char *     const str,
               uint16_t * const indexP);


/*********************************************************************
** Table
*********************************************************************/

typedef struct 
{
    char *name,*value;
    uint16_t hash;
} TTableItem;

typedef struct
{
    TTableItem *item;
    uint16_t size,maxsize;
} TTable;

void TableInit(TTable *t);
void TableFree(TTable *t);
abyss_bool TableAdd(TTable *t,char *name,char *value);
abyss_bool TableAddReplace(TTable *t,char *name,char *value);
abyss_bool TableFindIndex(TTable *t,char *name,uint16_t *index);
char *TableFind(TTable *t,char *name);


/*********************************************************************
** Thread
*********************************************************************/

#ifdef WIN32
#include <windows.h>
#define  THREAD_ENTRYTYPE  WINAPI
#else
#define  THREAD_ENTRYTYPE
#include <pthread.h>
#endif  /* WIN32 */

typedef uint32_t (THREAD_ENTRYTYPE *TThreadProc)(void *);
#ifdef WIN32
typedef HANDLE TThread;
#else
typedef pthread_t TThread;
typedef void* (*PTHREAD_START_ROUTINE)(void *);
#endif  /* WIN32 */

abyss_bool ThreadCreate(TThread *t,TThreadProc func,void *arg);
abyss_bool ThreadRun(TThread *t);
abyss_bool ThreadStop(TThread *t);
abyss_bool ThreadKill(TThread *t);
void ThreadWait(uint32_t ms);
void ThreadExit( TThread *t, int ret_value );
void ThreadClose( TThread *t );

/*********************************************************************
** Mutex
*********************************************************************/

#ifdef WIN32
typedef HANDLE TMutex;
#else
typedef pthread_mutex_t TMutex;
#endif  /* WIN32 */

abyss_bool MutexCreate(TMutex *m);
abyss_bool MutexLock(TMutex *m);
abyss_bool MutexUnlock(TMutex *m);
abyss_bool MutexTryLock(TMutex *m);
void MutexFree(TMutex *m);


/*********************************************************************
** Pool
*********************************************************************/

typedef struct _TPoolZone
{
    char *pos,*maxpos;
    struct _TPoolZone *next,*prev;
/*  char data[0]; */
    char data[1];
} TPoolZone;

typedef struct
{
    TPoolZone *firstzone,*currentzone;
    uint32_t zonesize;
    TMutex mutex;
} TPool;

abyss_bool PoolCreate(TPool *p,uint32_t zonesize);
void PoolFree(TPool *p);

void *PoolAlloc(TPool *p,uint32_t size);
char *PoolStrdup(TPool *p,char *s);


/*********************************************************************
** Socket
*********************************************************************/

#ifdef WIN32
typedef SOCKET TSocket;
#else
typedef int TSocket;
#endif  /* WIN32 */

/*********************************************************************
** Connection
*********************************************************************/

typedef struct _TConn TConn;

/*********************************************************************
** Server
*********************************************************************/

typedef struct {
    /* Before Xmlrpc-c 1.04, the internal server representation,
       struct _TServer, was exposed to users and was the only way to
       set certain parameters of the server.  Now, use the (new)
       ServerSet...() functions.  Use the HAVE_ macros to determine
       which method you have to use.
    */
    struct _TServer * srvP;
} TServer;

abyss_bool
ServerCreate(TServer *    const serverP,
             const char * const name,
             uint16_t     const port,
             const char * const filespath,
             const char * const logfilename);

abyss_bool
ServerCreateSocket(TServer *    const serverP,
                   const char * const name,
                   TSocket      const socketFd,
                   const char * const filespath,
                   const char * const logfilename);

abyss_bool
ServerCreateNoAccept(TServer *    const serverP,
                     const char * const name,
                     const char * const filespath,
                     const char * const logfilename);

void
ServerFree(TServer * const serverP);

#define HAVE_SERVER_SET_KEEPALIVE_TIMEOUT 1
void
ServerSetKeepaliveTimeout(TServer * const serverP,
                          uint32_t  const keepaliveTimeout);

#define HAVE_SERVER_SET_KEEPALIVE_MAX_CONN 1
void
ServerSetKeepaliveMaxConn(TServer * const serverP,
                          uint32_t  const keepaliveMaxConn);

#define HAVE_SERVER_SET_TIMEOUT 1
void
ServerSetTimeout(TServer * const serverP,
                 uint32_t  const timeout);

#define HAVE_SERVER_SET_ADVERTISE 1
void
ServerSetAdvertise(TServer *  const serverP,
                   abyss_bool const advertise);

void
ServerInit(TServer * const serverP);

void
ServerRun(TServer * const serverP);

void
ServerRunOnce(TServer * const serverP);

void
ServerRunOnce2(TServer *           const serverP,
               enum abyss_foreback const foregroundBackground);

void
ServerRunConn(TServer * const serverP,
              TSocket   const connectedSocket);

void
ServerDaemonize(TServer * const serverP);

/*********************************************************************
** Range
*********************************************************************/

abyss_bool RangeDecode(char *str,uint64_t filesize,uint64_t *start,uint64_t *end);

/*********************************************************************
** Date
*********************************************************************/

#include <time.h>

typedef struct tm TDate;

abyss_bool DateToString(TDate *tm,char *s);
abyss_bool DateToLogString(TDate *tm,char *s);

abyss_bool DateDecode(char *s,TDate *tm);

int32_t DateCompare(TDate *d1,TDate *d2);

abyss_bool DateFromGMT(TDate *d,time_t t);
abyss_bool DateFromLocal(TDate *d,time_t t);

abyss_bool DateInit(void);

/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d);

/*********************************************************************
** Session
*********************************************************************/

typedef enum {
    m_unknown, m_get, m_put, m_head, m_post, m_delete, m_trace, m_options
} TMethod;

typedef struct {
    TMethod method;
    char *uri;
    char *query;
    char *host;
    char *from;
    char *useragent;
    char *referer;
    char *requestline;
    char *user;
} TRequestInfo;

typedef struct _TSession TSession;

abyss_bool
SessionRefillBuffer(TSession * const sessionP);

size_t
SessionReadDataAvail(TSession * const sessionP);

void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP);

void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP);

/*********************************************************************
** Request
*********************************************************************/

#define CR      '\r'
#define LF      '\n'
#define CRLF    "\r\n"

abyss_bool RequestValidURI(TSession *r);
abyss_bool RequestValidURIPath(TSession *r);
abyss_bool RequestUnescapeURI(TSession *r);

char *RequestHeaderValue(TSession *r,char *name);

abyss_bool RequestRead(TSession *r);
void RequestInit(TSession *r,TConn *c);
void RequestFree(TSession *r);

abyss_bool RequestAuth(TSession *r,char *credential,char *user,char *pass);

/*********************************************************************
** Response
*********************************************************************/

abyss_bool ResponseAddField(TSession *r,char *name,char *value);
void ResponseWrite(TSession *r);

abyss_bool ResponseChunked(TSession *s);

void ResponseStatus(TSession *r,uint16_t code);
void ResponseStatusErrno(TSession *r);

abyss_bool ResponseContentType(TSession *r,char *type);
abyss_bool ResponseContentLength(TSession *r,uint64_t len);

void ResponseError(TSession *r);


/*********************************************************************
** HTTP
*********************************************************************/

int32_t HTTPRead(TSession *s,char *buffer,uint32_t len);

abyss_bool HTTPWrite(TSession *s,char *buffer,uint32_t len);
abyss_bool HTTPWriteEnd(TSession *s);

typedef abyss_bool (*URIHandler) (TSession *); /* deprecated */

struct URIHandler2;

typedef void (*initHandlerFn)(struct URIHandler2 *,
                              abyss_bool *);

typedef void (*termHandlerFn)(struct URIHandler2 *);

typedef void (*handleReq2Fn)(struct URIHandler2 *,
                             TSession *,
                             abyss_bool *);

typedef struct URIHandler2 {
    initHandlerFn init;
    termHandlerFn term;
    handleReq2Fn  handleReq2;
    URIHandler    handleReq1;  /* deprecated */
    void *        userdata;
} URIHandler2;

void
ServerAddHandler2(TServer *     const srvP,
                  URIHandler2 * const handlerP,
                  abyss_bool *  const successP);

abyss_bool
ServerAddHandler(TServer * const srvP,
                 URIHandler const handler);

void
ServerDefaultHandler(TServer *  const srvP,
                     URIHandler const handler);

void
LogWrite(TServer *    const srvP,
         const char * const c);


/*********************************************************************
** MIMEType
*********************************************************************/

void MIMETypeInit(void);
abyss_bool MIMETypeAdd(char *type,char *ext);
char *MIMETypeFromExt(char *ext);
char *MIMETypeFromFileName(char *filename);
char *MIMETypeGuessFromFile(char *filename);


/*********************************************************************
** Conf
*********************************************************************/

abyss_bool ConfReadMIMETypes(char *filename);
abyss_bool ConfReadServerFile(const char *filename,TServer *srv);


/*********************************************************************
** Trace
*********************************************************************/

void TraceMsg(char *fmt,...);
void TraceExit(char *fmt,...);


/*********************************************************************
** Session
*********************************************************************/

abyss_bool SessionLog(TSession *s);


#ifdef __cplusplus
}
#endif

/*****************************************************************************
** Here is the copyright notice from the Abyss web server project file from
** which this file is derived.
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
#endif  /* _ABYSS_H_ */

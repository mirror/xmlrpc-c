#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#include "conn.h"

/*********************************************************************
** Request
*********************************************************************/

abyss_bool RequestValidURI(TSession *r);
abyss_bool RequestValidURIPath(TSession *r);
abyss_bool RequestUnescapeURI(TSession *r);

abyss_bool RequestRead(TSession *r);
void RequestInit(TSession *r,TConn *c);
void RequestFree(TSession *r);

abyss_bool RequestAuth(TSession *r,char *credential,char *user,char *pass);

/*********************************************************************
** HTTP
*********************************************************************/

int32_t
HTTPRead(TSession *   const sessionP,
         const char * const buffer,
         uint32_t     const len);

abyss_bool
HTTPWrite(TSession *   const sessionP,
          const char * const buffer,
          uint32_t     const len);

abyss_bool
HTTPWriteEnd(TSession * const sessionP);

const char *
MIMETypeFromExt(const char * const ext);

const char *
MIMETypeFromFileName(const char * const fileName);

const char *
MIMETypeGuessFromFile(const char * const filename);

#endif

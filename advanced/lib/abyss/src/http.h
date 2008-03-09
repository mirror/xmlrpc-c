#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#include "conn.h"

/*********************************************************************
** Request
*********************************************************************/

abyss_bool RequestValidURI(TSession * const r);
abyss_bool RequestValidURIPath(TSession * const r);
abyss_bool RequestUnescapeURI(TSession *r);

void RequestRead(TSession * const r);
void RequestInit(TSession * const r,TConn * const c);
void RequestFree(TSession * const r);

abyss_bool
RequestAuth(TSession *   const sessionP,
            const char * const credential,
            const char * const user,
            const char * const pass);

/*********************************************************************
** HTTP
*********************************************************************/

const char *
HTTPReasonByStatus(uint16_t const code);

int32_t
HTTPRead(TSession *   const sessionP,
         const char * const buffer,
         uint32_t     const len);

abyss_bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len);

abyss_bool
HTTPWriteEndChunk(TSession * const sessionP);

abyss_bool
HTTPKeepalive(TSession * const sessionP);

abyss_bool
HTTPWriteContinue(TSession * const sessionP);

#endif

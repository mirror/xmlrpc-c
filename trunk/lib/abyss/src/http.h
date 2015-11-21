#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#include <sys/types.h>

bool
HTTPRequestHasValidUri(TSession * const sessionP);

bool
HTTPRequestHasValidUriPath(TSession * const sessionP);

const char *
HTTPReasonByStatus(uint16_t const code);

bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len);

bool
HTTPWriteEndChunk(TSession * const sessionP);

bool
HTTPKeepalive(TSession * const sessionP);

bool
HTTPWriteContinue(TSession * const sessionP);

#endif

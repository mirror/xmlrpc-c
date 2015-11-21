#ifndef SESSION_READ_REQUEST_H_INCLUDED
#define SESSION_READ_REQUEST_H_INCLUDED

#include "session.h"

void
SessionReadRequest(TSession *    const sessionP,
                   uint32_t      const timeout,
                   const char ** const errorP,
                   uint16_t *    const httpErrorCodeP);

#endif

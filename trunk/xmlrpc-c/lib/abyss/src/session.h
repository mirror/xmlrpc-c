#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include "xmlrpc-c/abyss.h"
#include "date.h"
#include "data.h"

typedef struct {
    uint8_t major;
    uint8_t minor;
} httpVersion;

struct _TSession {
    TRequestInfo request_info;
    uint32_t nbfileds;
    TList cookies;
    TList ranges;

    uint16_t status;
        /* Response status from handler.  Zero means handler has not
           set it.
        */
    TString header;

    abyss_bool serverDeniesKeepalive;
        /* Server doesn't want keepalive for this session, regardless of
           what happens in the session.  E.g. because the connection has
           already been kept alive long enough.
        */
    abyss_bool responseStarted;
        /* Handler has at least started the response (i.e. called
           ResponseWrite())
        */

    struct _TConn * conn;

    httpVersion version;

    TTable request_headers;
    TTable response_headers;

    TDate date;

    abyss_bool chunkedwrite;
    abyss_bool chunkedwritemode;
};


#endif

#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include "xmlrpc-c/abyss.h"
#include "date.h"
#include "data.h"

struct _TSession {
    TRequestInfo request_info;
    uint32_t nbfileds;
    uint16_t port;
    TList cookies;
    TList ranges;

    uint16_t status;
    TString header;

    abyss_bool keepalive;
    abyss_bool cankeepalive;
    abyss_bool done;

    struct _TConn * conn;

    uint8_t versionminor;
    uint8_t versionmajor;

    TTable request_headers;
    TTable response_headers;

    TDate date;

    abyss_bool chunkedwrite;
    abyss_bool chunkedwritemode;
};


#endif

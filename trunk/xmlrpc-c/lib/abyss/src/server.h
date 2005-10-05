#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include "xmlrpc-c/abyss.h"

#include "file.h"

struct _TServer {
    abyss_bool socketBound;
        /* The listening socket exists and is bound to a local address
           (may already be listening as well)
        */
    TSocket listensock;
        /* Meaningful only when 'socketBound' is true: file descriptor of
           the listening socket.
        */
    const char * logfilename;
    abyss_bool logfileisopen;
    TFile logfile;
    TMutex logmutex;
    char *name;
    char *filespath;
    uint16_t port;
        /* Meaningful only when 'socketBound' is false: port number to which
           we should bind the listening socket
        */
    uint32_t keepalivetimeout;
    uint32_t keepalivemaxconn;
    uint32_t timeout;
    TList handlers;
    TList defaultfilenames;
    void *defaulthandler;
    abyss_bool advertise;
#ifdef _UNIX
    uid_t uid;
    gid_t gid;
    TFile pidfile;
#endif  
};


#endif

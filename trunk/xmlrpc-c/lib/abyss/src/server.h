#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <sys/types.h>

#include "xmlrpc-c/abyss.h"

#include "file.h"
#include "data.h"


struct _TServer {
    abyss_bool terminationRequested;
        /* User wants this server to terminate as soon as possible,
           in particular before accepting any more connections and without
           waiting for any.
        */
    abyss_bool chanSwitchBound;
        /* The channel switch exists and is bound to a local address
           (may already be listening as well)
        */
    TChanSwitch * chanSwitchP;
        /* Meaningful only when 'chanSwitchBound' is true: the channel
           switch which directs connections from clients to this server.
        */
    abyss_bool weCreatedChanSwitch;
        /* We created the channel switch 'chanSwitchP', as
           opposed to 1) User supplied it; or 2) there isn't one.
        */
    const char * logfilename;
    abyss_bool logfileisopen;
    TFile logfile;
    TMutex logmutex;
    const char * name;
    const char * filespath;
    abyss_bool serverAcceptsConnections;
        /* We listen for and accept TCP connections for HTTP transactions.
           (The alternative is the user supplies a TCP-connected socket
           for each transaction)
        */
    uint16_t port;
        /* Meaningful only when 'chanSwitchBound' is false: TCP port
           number to which we should bind the switch.
        */
    uint32_t keepalivetimeout;
    uint32_t keepalivemaxconn;
    uint32_t timeout;
        /* Maximum time in seconds the server will wait to read a header
           or a data chunk from the channel.
        */
    TList handlers;
    TList defaultfilenames;
    void * defaulthandler;
    abyss_bool advertise;
    MIMEType * mimeTypeP;
        /* NULL means to use the global MIMEType object */
    abyss_bool useSigchld;
        /* Meaningless if not using forking for threads.
           TRUE means user will call ServerHandleSigchld to indicate that
           a SIGCHLD signal was received, and server shall use that as its
           indication that a child has died.  FALSE means server will not
           be aware of SIGCHLD and will instead poll for existence of PIDs
           to determine if a child has died.
        */
#ifndef WIN32
    uid_t uid;
    gid_t gid;
    TFile pidfile;
#endif
};


void
ServerBackgroundProcessComplete(pid_t const pid);

#endif

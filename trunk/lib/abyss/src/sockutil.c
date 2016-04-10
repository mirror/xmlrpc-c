/*=============================================================================
                                 sockutil.c
===============================================================================
  This is utility functions for use in channel and channel switches
  that use Unix sockets.
=============================================================================*/

#include "xmlrpc_config.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#if HAVE_SYS_FILIO_H
  #include <sys/filio.h>
#endif

#include "c_util.h"
#include "int.h"
#include "girstring.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "netinet/in.h"
#include "xmlrpc-c/abyss.h"

#include "sockutil.h"


void
sockutil_interruptPipeInit(sockutil_InterruptPipe * const pipeP,
                           const char **            const errorP) {

    int pipeFd[2];
    int rc;

    rc = pipe(pipeFd);

    if (rc != 0)
        xmlrpc_asprintf(errorP, "Unable to create a pipe to use to interrupt "
                        "waits.  pipe() failed with errno %d (%s)",
                        errno, strerror(errno));
    else {
        *errorP = NULL;
        pipeP->interruptorFd = pipeFd[1];
        pipeP->interrupteeFd = pipeFd[0];
    }
}



void
sockutil_interruptPipeTerm(sockutil_InterruptPipe const pipe) {

    close(pipe.interruptorFd);
    close(pipe.interrupteeFd);
}



void
sockutil_interruptPipeInterrupt(sockutil_InterruptPipe const interruptPipe) {

    unsigned char const zero[1] = {0u};

    write(interruptPipe.interruptorFd, &zero, sizeof(zero));
}



bool
sockutil_connected(int const fd) {
/*----------------------------------------------------------------------------
   Return TRUE iff the socket on file descriptor 'fd' is in the connected
   state.

   If 'fd' does not identify a stream socket or we are unable to determine
   the state of the stream socket, the answer is "false".
-----------------------------------------------------------------------------*/
    bool connected;
    struct sockaddr sockaddr;
    socklen_t nameLen;
    int rc;

    nameLen = sizeof(sockaddr);

    rc = getpeername(fd, &sockaddr, &nameLen);

    if (rc == 0)
        connected = true;
    else
        connected = false;

    return connected;
}



void
sockutil_getSockName(int                const sockFd,
                     struct sockaddr ** const sockaddrPP,
                     size_t  *          const sockaddrLenP,
                     const char **      const errorP) {

    unsigned char * sockName;
    socklen_t nameSize;

    nameSize = sizeof(struct sockaddr) + 1;
    
    sockName = malloc(nameSize);

    if (sockName == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate space for socket name");
    else {
        int rc;
        socklen_t nameLen;
        nameLen = nameSize;  /* initial value */
        rc = getsockname(sockFd, (struct sockaddr *)sockName, &nameLen);

        if (rc < 0)
            xmlrpc_asprintf(errorP, "getsockname() failed.  errno=%d (%s)",
                            errno, strerror(errno));
        else {
            if (nameLen > nameSize-1)
                xmlrpc_asprintf(errorP,
                                "getsockname() says the socket name is "
                                "larger than %u bytes, which means it is "
                                "not in the expected format.",
                                nameSize-1);
            else {
                *sockaddrPP = (struct sockaddr *)sockName;
                *sockaddrLenP = nameLen;
                *errorP = NULL;
            }
        }
        if (*errorP)
            free(sockName);
    }
}



void
sockutil_getPeerName(int                const sockFd,
                     struct sockaddr ** const sockaddrPP,
                     size_t  *          const sockaddrLenP,
                     const char **      const errorP) {

    unsigned char * peerName;
    socklen_t nameSize;

    nameSize = sizeof(struct sockaddr) + 1;
    
    peerName = malloc(nameSize);

    if (peerName == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate space for peer name");
    else {
        int rc;
        socklen_t nameLen;
        nameLen = nameSize;  /* initial value */
        rc = getpeername(sockFd, (struct sockaddr *)peerName, &nameLen);

        if (rc < 0)
            xmlrpc_asprintf(errorP, "getpeername() failed.  errno=%d (%s)",
                            errno, strerror(errno));
        else {
            if (nameLen > nameSize-1)
                xmlrpc_asprintf(errorP,
                                "getpeername() says the socket name is "
                                "larger than %u bytes, which means it is "
                                "not in the expected format.",
                                nameSize-1);
            else {
                *sockaddrPP = (struct sockaddr *)peerName;
                *sockaddrLenP = nameLen;
                *errorP = NULL;
            }
        }
        if (*errorP)
            free(peerName);
    }
}



static void
formatPeerInfoInet(const struct sockaddr_in * const sockaddrInP,
                   socklen_t                  const sockaddrLen,
                   const char **              const peerStringP) {

    if (sockaddrLen < sizeof(*sockaddrInP))
        xmlrpc_asprintf(peerStringP, "??? getpeername() returned "
                        "the wrong size");
    else {
        unsigned char * const ipaddr = (unsigned char *)
            &sockaddrInP->sin_addr.s_addr;
        xmlrpc_asprintf(peerStringP, "%u.%u.%u.%u:%hu",
                        ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                        ntohs(sockaddrInP->sin_port));
    }
}



static void
formatPeerInfoInet6(const struct sockaddr_in6 * const sockaddrIn6P,
                    socklen_t                   const sockaddrLen,
                    const char **               const peerStringP) {

    if (sockaddrLen < sizeof(*sockaddrIn6P))
        xmlrpc_asprintf(peerStringP, "??? getpeername() returned "
                        "the wrong size");
    else {
        /* Gcc on Debian 6 gives a bewildering error message about aliasing
           if we try to dereference sockaddrIn6P in various ways, regardless
           of casts to void * or anything else.  So we copy the data structure
           as raw memory contents and then access the copy.  12.02.05.
        */
        struct sockaddr_in6 sockaddr;

        MEMSCPY(&sockaddr, sockaddrIn6P);

        {
            char buffer[256];
            const char * rc;

            rc = inet_ntop(AF_INET6, &sockaddr.sin6_addr,
                           buffer, sizeof(buffer));

            /* Punt the supposedly impossible error */
            if (rc == NULL)
                STRSCPY(buffer, "???");

            xmlrpc_asprintf(peerStringP, "[%s]:%hu",
                            buffer, sockaddr.sin6_port);
        }
    }
}



void
sockutil_formatPeerInfo(int           const sockFd,
                        const char ** const peerStringP) {

    struct sockaddr sockaddr;
    socklen_t sockaddrLen;
    int rc;

    sockaddrLen = sizeof(sockaddr);
    
    rc = getpeername(sockFd, &sockaddr, &sockaddrLen);
    
    if (rc < 0)
        xmlrpc_asprintf(peerStringP, "?? getpeername() failed.  errno=%d (%s)",
                        errno, strerror(errno));
    else {
        switch (sockaddr.sa_family) {
        case AF_INET:
            formatPeerInfoInet((const struct sockaddr_in *) &sockaddr,
                               sockaddrLen,
                               peerStringP);
            break;
        case AF_INET6:
            formatPeerInfoInet6((const struct sockaddr_in6 *) &sockaddr,
                                sockaddrLen,
                                peerStringP);
            break;
        default:
            xmlrpc_asprintf(peerStringP, "??? AF=%u", sockaddr.sa_family);
        }
    }
}



void
sockutil_listen(int           const sockFd,
                uint32_t      const backlog,
                const char ** const errorP) {

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, &minus1, sizeof(minus1));

    rc = listen(sockFd, backlog);

    if (rc == -1)
        xmlrpc_asprintf(errorP, "listen() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
}



void
sockutil_waitForConnection(int                    const listenSockFd,
                           sockutil_InterruptPipe const interruptPipe,
                           bool *                 const interruptedP,
                           const char **          const errorP) {
/*----------------------------------------------------------------------------
   Wait for the listening socket to have a connection ready to accept.

   We return before the requested condition holds if the process receives
   (and catches) a signal, but only if it receives that signal a certain
   time after we start running.  (That means this behavior isn't useful
   for most purposes).

   We furthermore return before the requested condition holds if someone sends
   a byte through the listening socket's interrupt pipe (or has sent one
   previously since the most recent time the pipe was drained).

   Return *interruptedP == true if we return before there is a connection
   ready to accept.
-----------------------------------------------------------------------------*/
    struct pollfd pollfds[2];
    int rc;

    pollfds[0].fd = listenSockFd;
    pollfds[0].events = POLLIN;

    pollfds[1].fd = interruptPipe.interrupteeFd;
    pollfds[1].events = POLLIN;
    
    rc = poll(pollfds, ARRAY_SIZE(pollfds), -1);

    if (rc < 0) {
        if (errno == EINTR) {
            *errorP       = NULL;
            *interruptedP = true;
        } else {
            xmlrpc_asprintf(errorP, "poll() failed, errno = %d (%s)",
                            errno, strerror(errno));
            *interruptedP = false; /* quiet compiler warning */
        }
    } else if (pollfds[0].revents & POLLHUP) {
        xmlrpc_asprintf(errorP, "INTERNAL ERROR: listening socket "
                        "is not listening");
    } else if (pollfds[1].revents & POLLHUP) {
        xmlrpc_asprintf(errorP, "INTERNAL ERROR: interrupt socket hung up");
    } else if (pollfds[0].revents & POLLERR) {
        xmlrpc_asprintf(errorP, "listening socket is in POLLERR status");
    } else if (pollfds[1].revents & POLLHUP) {
        xmlrpc_asprintf(errorP, "interrupt socket is in POLLERR status");
    } else {
        *errorP       = NULL;
        *interruptedP = !(pollfds[0].revents & POLLIN);
    }
}



void
sockutil_setSocketOptions(int           const fd,
                          const char ** const errorP) {

    int32_t n = 1;
    int rc;

    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n));

    if (rc < 0)
        xmlrpc_asprintf(errorP, "Failed to set socket options.  "
                        "setsockopt() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
}



static void
traceSocketBound(const struct sockaddr * const sockAddrP,
                 socklen_t               const sockAddrLen) {

    if (sockAddrP->sa_family == AF_INET &&
        sockAddrLen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in * const sockAddrInP =
            (const struct sockaddr_in *)sockAddrP;

        unsigned char * const ipaddr = (unsigned char *)
            &sockAddrInP->sin_addr.s_addr;

        fprintf(stderr, "Bound socket for channel switch to "
                "AF_INET port %u.%u.%u.%u:%hu\n",
                ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                ntohs(sockAddrInP->sin_port));
    } else {
        fprintf(stderr, "Bound socket for channel switch to address of "
                "family %d\n", sockAddrP->sa_family);
    }
}



void
sockutil_bindSocketToPort(int                     const fd,
                          const struct sockaddr * const sockAddrP,
                          socklen_t               const sockAddrLen,
                          const char **           const errorP) {
    
    int rc;

    rc = bind(fd, sockAddrP, sockAddrLen);

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind socket "
                        "to the socket address.  "
                        "bind() failed with errno %d (%s)",
                        errno, strerror(errno));
    else {
        *errorP = NULL;

        if (SwitchTraceIsActive)
            traceSocketBound(sockAddrP, sockAddrLen);
    }
}



void
sockutil_bindSocketToPortInet(int           const fd,
                              uint16_t      const portNumber,
                              const char ** const errorP) {
    
    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind IPv4 socket "
                        "to port number %hu.  "
                        "bind() failed with errno %d (%s)",
                        portNumber, errno, strerror(errno));
    else
        *errorP = NULL;
}



void
sockutil_bindSocketToPortInet6(int                     const fd,
                               uint16_t                const portNumber,
                               const char **           const errorP) {

    struct sockaddr_in6 name;
    int rc;

    name.sin6_family = AF_INET6;
    name.sin6_port   = htons(portNumber);
    name.sin6_addr = in6addr_any;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind IPv6 socket "
                        "to port number %hu.  "
                        "bind() failed with errno %d (%s)",
                        portNumber, errno, strerror(errno));
    else
        *errorP = NULL;
}




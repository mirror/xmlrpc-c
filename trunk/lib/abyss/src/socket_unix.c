/*=============================================================================
                                 socket_unix.c
===============================================================================
  This is the implementation of TChanSwitch and TChannel (and
  obsolete TSocket) for a standard Unix (POSIX)
  stream socket -- what you create with a socket() C library call.
=============================================================================*/

#include "xmlrpc_config.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
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

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"

#include "socket_unix.h"



struct socketUnix {
/*----------------------------------------------------------------------------
   The properties/state of a TChanSwitch or TChannel unique to the
   Unix variety.
-----------------------------------------------------------------------------*/
    int fd;
        /* File descriptor of the POSIX socket (such as is created by
           socket() in the C library) for the socket.
        */
    abyss_bool userSuppliedFd;
        /* The file descriptor and associated POSIX socket belong to the
           user; we did not create it.
        */
};



static abyss_bool
connected(int const fd) {
/*----------------------------------------------------------------------------
   Return TRUE iff the socket on file descriptor 'fd' is in the connected
   state.

   If 'fd' does not identify a stream socket or we are unable to determine
   the state of the stream socket, the answer is "false".
-----------------------------------------------------------------------------*/
    abyss_bool connected;
    struct sockaddr sockaddr;
    socklen_t nameLen;
    int rc;

    nameLen = sizeof(sockaddr);

    rc = getpeername(fd, &sockaddr, &nameLen);

    if (rc == 0)
        connected = TRUE;
    else
        connected = FALSE;

    return connected;
}



void
SocketUnixInit(const char ** const errorP) {

    *errorP = NULL;
}



void
SocketUnixTerm(void) {

}



/*=============================================================================
      TChannel
=============================================================================*/

static void
channelDestroy(TChannel * const channelP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    if (!socketUnixP->userSuppliedFd)
        close(socketUnixP->fd);

    free(socketUnixP);
}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             abyss_bool *          const failedP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    size_t bytesLeft;
    abyss_bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        ssize_t rc;
        
        rc = send(socketUnixP->fd, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);

        if (ChannelTraceIsActive) {
            if (rc < 0)
                fprintf(stderr, "Abyss channel: send() failed.  errno=%d (%s)",
                        errno, strerror(errno));
            else if (rc == 0)
                fprintf(stderr, "Abyss channel: send() failed.  "
                        "Socket closed.\n");
            else
                fprintf(stderr, "Abyss channel: sent %u bytes: '%.*s'\n",
                        rc, rc, &buffer[len-bytesLeft]);
        }
        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



static ChannelReadImpl channelRead;

static void
channelRead(TChannel *      const channelP, 
            unsigned char * const buffer, 
            uint32_t        const bufferSize,
            uint32_t *      const bytesReceivedP,
            abyss_bool *    const failedP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    int rc;
    rc = recv(socketUnixP->fd, buffer, bufferSize, 0);

    if (rc < 0) {
        *failedP = TRUE;
        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel: "
                    "Failed to receive data from socket.  "
                    "recv() failed with errno %d (%s)\n",
                    errno, strerror(errno));
    } else {
        *failedP = FALSE;
        *bytesReceivedP = rc;

        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel: read %u bytes: '%.*s'\n",
                    *bytesReceivedP, (int)(*bytesReceivedP), buffer);
    }
}



static ChannelWaitImpl channelWait;

static uint32_t
channelWait(TChannel * const channelP,
            abyss_bool const rd,
            abyss_bool const wr,
            uint32_t   const timems) {

    struct socketUnix * const socketUnixP = channelP->implP;

    fd_set rfds, wfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (rd)
        FD_SET(socketUnixP->fd, &rfds);

    if (wr)
        FD_SET(socketUnixP->fd, &wfds);

    tv.tv_sec  = timems / 1000;
    tv.tv_usec = timems % 1000;

    for (;;) {
        int rc;

        rc = select(socketUnixP->fd + 1, &rfds, &wfds, NULL,
                    (timems == TIME_INFINITE ? NULL : &tv));

        switch(rc) {   
        case 0: /* time out */
            return 0;

        case -1:  /* socket error */
            if (errno == EINTR)
                break;
            
            return 0;
            
        default:
            if (FD_ISSET(socketUnixP->fd, &rfds))
                return 1;
            if (FD_ISSET(socketUnixP->fd, &wfds))
                return 2;
            return 0;
        }
    }
}



void
ChannelUnixGetPeerName(TChannel *         const channelP,
                       struct sockaddr ** const sockaddrPP,
                       size_t  *          const sockaddrLenP,
                       const char **      const errorP) {

    struct socketUnix * const socketUnixP = channelP->implP;

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
        rc = getpeername(socketUnixP->fd, (struct sockaddr *)peerName,
                         &nameLen);

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



static ChannelFormatPeerInfoImpl channelFormatPeerInfo;

static void
channelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    struct sockaddr sockaddr;
    socklen_t sockaddrLen;
    int rc;

    sockaddrLen = sizeof(sockaddr);
    
    rc = getpeername(socketUnixP->fd, &sockaddr, &sockaddrLen);
    
    if (rc < 0)
        xmlrpc_asprintf(peerStringP, "?? getpeername() failed.  errno=%d (%s)",
                        errno, strerror(errno));
    else {
        switch (sockaddr.sa_family) {
        case AF_INET: {
            struct sockaddr_in * const sockaddrInP =
                (struct sockaddr_in *) &sockaddr;
            if (sockaddrLen < sizeof(*sockaddrInP))
                xmlrpc_asprintf(peerStringP, "??? getpeername() returned "
                                "the wrong size");
            else {
                unsigned char * const ipaddr = (unsigned char *)
                    &sockaddrInP->sin_addr.s_addr;
                xmlrpc_asprintf(peerStringP, "%u.%u.%u.%u:%hu",
                                ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                                sockaddrInP->sin_port);
            }
        } break;
        default:
            xmlrpc_asprintf(peerStringP, "??? AF=%u", sockaddr.sa_family);
        }
    }
}



static struct TChannelVtbl const channelVtbl = {
    &channelDestroy,
    &channelWrite,
    &channelRead,
    &channelWait,
    &channelFormatPeerInfo,
};



static void
makeChannelInfo(struct abyss_unix_chaninfo ** const channelInfoPP,
                struct sockaddr               const peerAddr,
                socklen_t                     const peerAddrLen,
                const char **                 const errorP) {

    struct abyss_unix_chaninfo * channelInfoP;

    MALLOCVAR(channelInfoP);
    
    if (channelInfoP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        channelInfoP->peerAddrLen = peerAddrLen;
        channelInfoP->peerAddr    = peerAddr;
        
        *channelInfoPP = channelInfoP;

        *errorP = NULL;
    }
}



static void
makeChannelFromFd(int const fd,
                  TChannel ** const channelPP,
                  const char ** const errorP) {

    struct socketUnix * socketUnixP;

    MALLOCVAR(socketUnixP);
    
    if (socketUnixP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Unix "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        socketUnixP->fd = fd;
        socketUnixP->userSuppliedFd = TRUE;
        
        ChannelCreate(&channelVtbl, socketUnixP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP)
            free(socketUnixP);
    }
}



void
ChannelUnixCreateFd(int                           const fd,
                    TChannel **                   const channelPP,
                    struct abyss_unix_chaninfo ** const channelInfoPP,
                    const char **                 const errorP) {

    struct sockaddr peerAddr;
    socklen_t peerAddrLen;
    int rc;

    peerAddrLen = sizeof(peerAddrLen);

    rc = getpeername(fd, &peerAddr, &peerAddrLen);

    if (rc != 0) {
        if (errno == ENOTCONN)
            xmlrpc_asprintf(errorP, "Socket on file descriptor %d is not in "
                            "connected state.", fd);
        else
            xmlrpc_asprintf(errorP, "getpeername() failed on fd %d.  "
                            "errno=%d (%s)", fd, errno, strerror(errno));
    } else {
        makeChannelInfo(channelInfoPP, peerAddr, peerAddrLen, errorP);
        if (!*errorP) {
            makeChannelFromFd(fd, channelPP, errorP);

            if (*errorP)
                free(*channelInfoPP);
        }
    }
}



/*=============================================================================
      TChanSwitch
=============================================================================*/

static SwitchDestroyImpl chanSwitchDestroy;

static void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct socketUnix * const socketUnixP = chanSwitchP->implP;

    if (!socketUnixP->userSuppliedFd)
        close(socketUnixP->fd);

    free(socketUnixP);
}



static SwitchListenImpl  chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct socketUnix * const socketUnixP = chanSwitchP->implP;

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(socketUnixP->fd, IPPROTO_TCP, TCP_NODELAY,
               &minus1, sizeof(minus1));

    rc = listen(socketUnixP->fd, backlog);

    if (rc == -1)
        xmlrpc_asprintf(errorP, "listen() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
}



static SwitchAcceptImpl chanSwitchAccept;

static void
chanSwitchAccept(TChanSwitch * const chanSwitchP,
                 TChannel **   const channelPP,
                 void **       const channelInfoPP,
                 const char ** const errorP) {
/*----------------------------------------------------------------------------
   Accept a connection via the channel switch *chanSwitchP.  Return as
   *channelPP the channel for the accepted connection.

   If no connection is waiting at *chanSwitchP, wait until one is.

   If we receive a signal while waiting, return immediately with
   *channelPP == NULL.
-----------------------------------------------------------------------------*/
    struct socketUnix * const listenSocketP = chanSwitchP->implP;

    abyss_bool interrupted;
    TChannel * channelP;

    interrupted = FALSE; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    while (!channelP && !*errorP && !interrupted) {
        struct sockaddr peerAddr;
        socklen_t size = sizeof(peerAddr);
        int rc;

        rc = accept(listenSocketP->fd, &peerAddr, &size);

        if (rc >= 0) {
            int const acceptedFd = rc;
            struct socketUnix * acceptedSocketP;

            MALLOCVAR(acceptedSocketP);

            if (!acceptedSocketP)
                xmlrpc_asprintf(errorP, "Unable to allocate memory");
            else {
                struct abyss_unix_chaninfo * channelInfoP;
                acceptedSocketP->fd = acceptedFd;
                acceptedSocketP->userSuppliedFd = FALSE;
                
                makeChannelInfo(&channelInfoP, peerAddr, size, errorP);
                if (!*errorP) {
                    *channelInfoPP = channelInfoP;

                    ChannelCreate(&channelVtbl, acceptedSocketP, &channelP);
                    if (!channelP)
                        xmlrpc_asprintf(errorP,
                                        "Failed to create TChannel object.");
                    else
                        *errorP = NULL;

                    if (*errorP)
                        free(channelInfoP);
                }
                if (*errorP)
                    free(acceptedSocketP);
            }
            if (*errorP)
                close(acceptedFd);
        } else if (errno == EINTR)
            interrupted = TRUE;
        else
            xmlrpc_asprintf(errorP, "accept() failed, errno = %d (%s)",
                            errno, strerror(errno));
    }
    *channelPP = channelP;
}



static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
};



static void
setSocketOptions(int           const fd,
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
bindSocketToPort(int              const fd,
                 struct in_addr * const addrP,
                 uint16_t         const portNumber,
                 const char **    const errorP) {

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    if (addrP)
        name.sin_addr = *addrP;
    else
        name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind socket to port number %hu.  "
                        "bind() failed with errno %d (%s)",
                        portNumber, errno, strerror(errno));
    else
        *errorP = NULL;
}



void
ChanSwitchUnixCreate(unsigned short const portNumber,
                     TChanSwitch ** const chanSwitchPP,
                     const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create a POSIX-socket-based channel switch.

   Use an IP socket.

   Set the socket's local address so that a subsequent "listen" will listen
   on all IP addresses, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    struct socketUnix * socketUnixP;

    MALLOCVAR(socketUnixP);

    if (!socketUnixP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Unix socket "
                        "descriptor structure.");
    else {
        int rc;
        rc = socket(AF_INET, SOCK_STREAM, 0);
        if (rc < 0)
            xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                            errno, strerror(errno));
        else {
            socketUnixP->fd = rc;
            socketUnixP->userSuppliedFd = FALSE;

            setSocketOptions(socketUnixP->fd, errorP);
            if (!*errorP) {
                bindSocketToPort(socketUnixP->fd, NULL, portNumber, errorP);
                
                if (!*errorP)
                    ChanSwitchCreate(&chanSwitchVtbl, socketUnixP,
                                     chanSwitchPP);
            }
            if (*errorP)
                close(socketUnixP->fd);
        }
        if (*errorP)
            free(socketUnixP);
    }
}



void
ChanSwitchUnixCreateFd(int            const fd,
                       TChanSwitch ** const chanSwitchPP,
                       const char **  const errorP) {

    struct socketUnix * socketUnixP;

    if (connected(fd))
        xmlrpc_asprintf(errorP,
                        "Socket (file descriptor %d) is in connected "
                        "state.", fd);
    else {
        MALLOCVAR(socketUnixP);

        if (socketUnixP == NULL)
            xmlrpc_asprintf(errorP, "unable to allocate memory for Unix "
                            "socket descriptor.");
        else {
            TChanSwitch * chanSwitchP;

            socketUnixP->fd = fd;
            socketUnixP->userSuppliedFd = TRUE;
            
            ChanSwitchCreate(&chanSwitchVtbl, socketUnixP, &chanSwitchP);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
            if (*errorP)
                free(socketUnixP);
        }
    }
}



/*=============================================================================
      obsolete TSocket interface
=============================================================================*/


void
SocketUnixCreateFd(int        const fd,
                   TSocket ** const socketPP) {

    TSocket * socketP;
    const char * error;

    if (connected(fd)) {
        TChannel * channelP;
        struct abyss_unix_chaninfo * channelInfoP;
        ChannelUnixCreateFd(fd, &channelP, &channelInfoP, &error);
        if (!error)
            SocketCreateChannel(channelP, channelInfoP, &socketP);
    } else {
        TChanSwitch * chanSwitchP;
        ChanSwitchUnixCreateFd(fd, &chanSwitchP, &error);
        if (!error)
            SocketCreateChanSwitch(chanSwitchP, &socketP);
    }
    if (error) {
        *socketPP = NULL;
        xmlrpc_strfree(error);
    } else
        *socketPP = socketP;
}

/*=============================================================================
                                 socket_win.c
===============================================================================
  This is the implementation of TSocket for a Windows Winsock socket.

  As of August 2006, this is just a framework, adapted from the POSIX
  socket version (socket_unix.c) and past versions of Abyss Windows
  socket code.  It doesn't compile and is just here as a starting
  point in case a Windows programmer wants to make Abyss work on
  Windows.

=============================================================================*/

#include "xmlrpc_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <winsock.h>

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"

#include "socket_win.h"


struct socketWin {
/*----------------------------------------------------------------------------
   The properties/state of a TSocket unique to a Unix TSocket.
-----------------------------------------------------------------------------*/
    SOCKET winsock;
    abyss_bool userSuppliedWinsock;
        /* 'socket' was supplied by the user; it belongs to him */
};



void
SocketWinInit(const char ** const errorP) {

    abyss_bool retval;
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD(2, 0);
 
    err = WSAStartup(wVersionRequested, &wsaData);

    if (err != 0)
        xmlrpc_asprintf(errorP, "WSAStartup() faild with rc %d", err);
    else
        *errorP = NULL;
}



void
SocketWinTerm(void) {
    
    WSACleanup();
}



/*=============================================================================
      TChannel
=============================================================================*/

static ChannelWriteImpl              channelWrite;
static ChannelReadImpl               channelRead;
static ChannelWaitImpl               channelWait;
static ChannelAvailableReadBytesImpl channelAvailableReadBytes;
static ChannelFormatPeerInfoImpl     channelFormatPeerInfo;



static void
channelDestroy(TChannel * const channelP) {

    struct socketWin * const socketWinP = channelP->implP;

    if (!socketWinP->userSuppliedWinsock)
        close(socketWinP->winsock);

    free(socketWinP);
}



static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             abyss_bool *          const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    size_t bytesLeft;
    abyss_bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        ssize_t rc;
        
        rc = send(socketWinP->winsock, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);

        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



static uint32_t
channelRead(TChannel * const channelP, 
            char *     const buffer, 
            uint32_t   const len) {

    struct socketWin * const socketWinP = channelP->implP;

    int rc;
    rc = recv(socketWinP->winsock, buffer, len, 0);
    return rc;
}



static uint32_t
channelWait(TChannel *  const channelP,
            abyss_bool  const rd,
            abyss_bool  const wr,
            uint32_t    const timems) {

    struct socketWin * const socketWinP = channelP->implP;

    fd_set rfds, wfds;
    TIMEVAL tv;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (rd)
        FD_SET(socketWinP->winsock, &rfds);

    if (wr)
        FD_SET(socketWinP->winsock, &wfds);

    tv.tv_sec  = timems / 1000;
    tv.tv_usec = timems % 1000;

    for (;;) {
        int rc;

        rc = select(socketWinP->fd + 1, &rfds, &wfds, NULL,
                    (timems == TIME_INFINITE ? NULL : &tv));

        switch(rc) {   
        case 0: /* time out */
            return 0;

        case -1:  /* socket error */
            if (errno == EINTR)
                break;
            
            return 0;
            
        default:
            if (FD_ISSET(socketWinP->winsock, &rfds))
                return 1;
            if (FD_ISSET(socketWinP->winsock, &wfds))
                return 2;
            return 0;
        }
    }
}



static uint32_t
channelAvailableReadBytes(TChannel * const channelP) {

    struct socketWin * const socketWinP = channelP->implP;

    uint32_t x;
    int rc;

    rc = ioctlsocket(socketWinP->winsock, FIONREAD, &x);

    return rc == 0 ? x : 0;
}




void
ChannelWinGetPeerName(TChannel *           const channelP,
                      struct sockaddr_in * const inAddrP,
                      const char **        const errorP) {

    struct socketWin * const socketWinP = channelP->implP;

    socklen_t addrlen;
    int rc;
    struct sockaddr sockAddr;

    addrlen = sizeof(sockAddr);
    
    rc = getpeername(socketWinP->winsock, &sockAddr, &addrlen);

    if (rc < 0)
        xmlrpc_asprintf(errorP, "getpeername() failed.  errno=%d (%s)",
                        errno, strerror(errno));
    else {
        if (addrlen != sizeof(sockAddr))
            xmlrpc_asprintf(errorP, "getpeername() returned a socket address "
                            "of the wrong size: %u.  Expected %u",
                            addrlen, sizeof(sockAddr));
        else {
            if (sockAddr.sa_family != AF_INET)
                xmlrpc_asprintf(errorP,
                                "Socket does not use the Inet (IP) address "
                                "family.  Instead it uses family %d",
                                sockAddr.sa_family);
            else {
                *inAddrP = *(struct sockaddr_in *)&sockAddr;

                *errorP = NULL;
            }
        }
    }
}



static struct TChannelVtbl const channelVtbl = {
    &channelDestroy,
    &channelWrite,
    &channelRead,
    &channelWait,
    &channelAvailableReadBytes,
    &channelFormatPeerInfo,
};



static SocketDestroyImpl            socketDestroy;
static SocketWriteImpl              socketWrite;
static SocketReadImpl               socketRead;
static SocketConnectImpl            socketConnect;
static SocketBindImpl               socketBind;
static SocketListenImpl             socketListen;
static SocketAcceptImpl             socketAccept;
static SocketErrorImpl              socketError;
static SocketWaitImpl               socketWait;
static SocketAvailableReadBytesImpl socketAvailableReadBytes;
static SocketGetPeerNameImpl        socketGetPeerName;


static struct TSocketVtbl const vtbl = {
    &socketDestroy,
    &socketWrite,
    &socketRead,
    &socketConnect,
    &socketBind,
    &socketListen,
    &socketAccept,
    &socketError,
    &socketWait,
    &socketAvailableReadBytes,
    &socketGetPeerName
};



void
SocketWinCreate(TSocket ** const socketPP) {

    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);

    if (socketWinP) {
        SOCKET rc;
        rc = socket(AF_INET, SOCK_STREAM, 0);
        if (rc < 0)
            *socketPP = NULL;
        else {
            socketUnixP->winsock = rc;
            socketUnixP->userSuppliedWinsock = FALSE;
            
            {
                int32_t n = 1;
                int rc;
                rc = setsockopt(socketUnixP->fd, SOL_SOCKET, SO_REUSEADDR,
                                (char*)&n, sizeof(n));
                if (rc < 0)
                    *socketPP = NULL;
                else
                    SocketCreate(&vtbl, socketWinP, socketPP);
            }
            if (!*socketPP)
                closesocket(socketWinP->winsock);
        }
        if (!*socketPP)
            free(socketWinP);
    } else
        *socketPP = NULL;
}



static void
makeChannelFromWinsock(SOCKET        const winsock,
                       TChannel **   const channelPP,
                       const char ** const errorP) {

    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);
    
    if (socketWinP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        socketWinP->winsock = winsock;
        socketWinP->userSuppliedWinsock = TRUE;
        
        ChannelCreate(&channelVtbl, socketWinP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP)
            free(socketWinP);
    }
}



void
ChannelWinCreateWinsock(SOCKET                       const winsock,
                        TChannel **                  const channelPP,
                        struct abyss_win_chaninfo ** const channelInfoPP,
                        const char **                const errorP) {

    *channelInfoP = NULL;

    makeChannelFromWinsock(winsock, channelPP, errorP);
}



/*=============================================================================
      TChanSwitch
=============================================================================*/

static SwitchDestroyImpl chanSwitchDestroy;
static SwitchListenImpl  chanSwitchListen;
static SwitchAcceptImpl  chanSwitchAccept;

static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
};



void
ChanSwitchWinCreate(uint16_t       const portNumber,
                    TChanSwitch ** const chanSwitchPP,
                    const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create a Winsock-based channel switch.

   Set the socket's local address so that a subsequent "listen" will listen
   on all IP addresses, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    struct socketUnix * socketWinP;

    MALLOCVAR(socketWinP);

    if (!socketWinP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows socket "
                        "descriptor structure.");
    else {
        SOCKET winsock;

        todo("Create winsock");
        int rc;

        if (rc < 0)
            xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                            errno, strerror(errno));
        else {
            socketWinP->winsock = winsock;
            socketWinP->userSuppliedFd = FALSE;

            if (*errorP)
                todo("destroy winsock");
        }
        if (*errorP)
            free(socketWinP);
    }
}




static void
bindSocketToPort(SOCKETK          const winsock,
                 struct in_addr * const addrP,
                 uint16_t         const portNumber,
                 const char **    const errorP) {
    
    struct socketWin * const socketWinP = socketP->implP;

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    name.sin_addr   = *addrP;

    rc = bind(socketWinP->fd, (struct sockaddr *)&name, sizeof(name));

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind socket to port number %hu.  "
                        "bind() failed with errno %d (%s)",
                        portNumber, errno, strerror(errno));
    else
        *errorP = NULL;
}



void
ChanSwitchWinCreateWinsock(SOCKET         const winsock,
                           TChanSwitch ** const chanSwitchPP,
                           const char **  const errorP) {

    struct socketWin * socketWinP;

    if (connected(winsock))
        xmlrpc_asprintf(errorP, "Socket is not in connected state.");
    else {
        MALLOCVAR(socketWinP);

        if (socketWinP == NULL)
            xmlrpc_asprintf(errorP, "unable to allocate memory for Windows "
                            "socket descriptor.");
        else {
            TChanSwitch * chanSwitchP;

            socketWinP->winsock = winsock;
            socketWinP->userSuppliedWinsock = TRUE;
            
            ChanSwitchCreate(&chanSwitchVtbl, socketWinP, &chanSwitchP);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
            if (*errorP)
                free(socketWinP);
        }
    }
}



void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct socketWin * const socketWinP = chanSwitchP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

    free(socketWinP);
}



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
        xmlrpc_asprintf(errorP, "setsockopt() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
}



static void
chanSwitchAccept(TChanSwitch *    const chanSwitchP,
                 TChannel **      const channelP,
                 void **          const channelInfoPP,
                 const char **    const errorP) {
/*----------------------------------------------------------------------------
   Accept a connection via the channel switch *chanSwitchP.  Return as
   *channelPP the channel for the accepted connection.

   If no connection is waiting at *chanSwitchP, wait until one is.

   If we receive a signal while waiting, return immediately with
   *channelPP == NULL.
-----------------------------------------------------------------------------*/
    struct socketWin * const listenSocketWinP = listenSocketP->implP;

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
            int const acceptedWinsock = rc;
            struct socketWin * acceptedSocketP;

            MALLOCVAR(acceptedSocketP);

            if (!acceptedSocketP)
                xmlrpc_asprintf(errorP, "Unable to allocate memory");
            else {
                acceptedSocketP->winsock = acceptedWinsock;
                acceptedSocketP->userSuppliedFd = FALSE;
                
                *channelInfoPP = NULL;

                ChannelCreate(&channelVtbl, acceptedSocketP, &channelP);
                if (!channelP)
                    xmlrpc_asprintf(errorP,
                                    "Failed to create TChannel object.");
                else
                    *errorP = NULL;
                
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



/*=============================================================================
      obsolete TSocket interface
=============================================================================*/


void
SocketWinCreateWinsock(SOCKET     const winsock,
                       TSocket ** const socketPP) {

    TSocket * socketP;
    const char * error;

    if (connected(winsock)) {
        TChannel * channelP;
        void * channelInfoP;
        ChannelWinCreateWinsock(winsock, &channelP, &channelInfoP, &error);
        if (!error)
            SocketCreateChannel(channelP, channelInfoP, &socketP);
    } else {
        TChanSwitch * chanSwitchP;
        ChanSwitchWinCreateWinsock(winsock, &chanSwitchP, &error);
        if (!error)
            SocketCreateChanSwitch(chanSwitchP, &socketP);
    }
    if (error) {
        *socketPP = NULL;
        xmlrpc_strfree(error);
    } else
        *socketPP = socketP;
}

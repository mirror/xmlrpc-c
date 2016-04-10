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
    bool userSuppliedFd;
        /* The file descriptor and associated POSIX socket belong to the
           user; we did not create it.
        */
    sockutil_InterruptPipe interruptPipe;

    bool isListening;
        /* We've done a 'listen' on the socket */
};



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

    sockutil_interruptPipeTerm(socketUnixP->interruptPipe);

    if (!socketUnixP->userSuppliedFd)
        close(socketUnixP->fd);

    free(socketUnixP);
}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    size_t bytesLeft;
    bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = false;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        ssize_t rc;

        // We'd like to use MSG_NOSIGNAL here, to prevent this send() from
        // causing a SIGPIPE if the other end of the socket is closed, but
        // MSG_NOSIGNAL is not standard enough.  An SO_NOSIGPIPE socket
        // option is another way, but even less standard.  So instead, the
        // thread simply must be set to ignore SIGPIPE.
        
        rc = send(socketUnixP->fd, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);

        if (ChannelTraceIsActive) {
            if (rc < 0)
                fprintf(stderr, "Abyss channel: send() failed.  errno=%d (%s)",
                        errno, strerror(errno));
            else if (rc == 0)
                fprintf(stderr, "Abyss channel: send() failed.  "
                        "Socket closed.\n");
            else {
                size_t const bytesTransferred = rc;
                fprintf(stderr, "Abyss channel: sent %u bytes: '%.*s'\n",
                        (unsigned)bytesTransferred,
                        (int)(MIN(bytesTransferred, 4096)),
                        &buffer[len-bytesLeft]);
            }
        }
        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = true;
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
            bool *          const failedP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    int rc;
    rc = recv(socketUnixP->fd, buffer, bufferSize, 0);

    if (rc < 0) {
        *failedP = true;
        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel: "
                    "Failed to receive data from socket.  "
                    "recv() failed with errno %d (%s)\n",
                    errno, strerror(errno));
    } else {
        *failedP = false;
        *bytesReceivedP = rc;

        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel: read %u bytes: '%.*s'\n",
                    *bytesReceivedP, (int)(*bytesReceivedP), buffer);
    }
}



static ChannelWaitImpl channelWait;

static void
channelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timeoutMs,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {
/*----------------------------------------------------------------------------
   Wait for the channel to be immediately readable or writable.

   Readable means there is at least one byte of data to read or the
   partner has disconnected.  Writable means the channel will take at
   least one byte of data to send or the partner has disconnected.

   'waitForRead' and 'waitForWrite' determine which of these
   conditions for which to wait; if both are true, we wait for either
   one.

   We return before the requested condition holds if 'timeoutMs'
   milliseconds pass.  timeoutMs == TIME_INFINITE means infinity.

   We return before the requested condition holds if the process receives
   (and catches) a signal, but only if it receives that signal a certain
   time after we start running.  (That means this function isn't useful
   for most purposes).

   Return *readyToReadP == true if the reason for returning is that
   the channel is immediately readable.  But only if 'readyToReadP' is
   non-null.  *readyToWriteP is analogous
   for writable.  Both may be true.

   Return *failedP true iff we fail to wait for the requested condition
   because of some unusual problem and 'failedP' is non-null.  Being
   interrupted by a signal is not a failure.

   If one of these return value pointers is NULL, don't return that
   value.
-----------------------------------------------------------------------------*/
    struct socketUnix * const socketUnixP = channelP->implP;

    /* Design note: some old systems may not have poll().  We're assuming
       that we don't have to run on any such system.  select() is more
       universal, but can't handle a file descriptor with a high number.

       pselect() and ppoll() would allow us to be properly
       interruptible by a signal -- we would add a signal mask to our
       arguments.  But ppoll() is fairly rare.  pselect() is more
       common, but in older Linux systems it doesn't actually work.
    */
    bool readyToRead, readyToWrite, failed;
    struct pollfd pollfds[2];
    int rc;

    pollfds[0].fd = socketUnixP->fd;
    pollfds[0].events =
        (waitForRead  ? POLLIN  : 0) |
        (waitForWrite ? POLLOUT : 0);

    pollfds[1].fd = socketUnixP->interruptPipe.interrupteeFd;
    pollfds[1].events = POLLIN;
    
    rc = poll(pollfds, ARRAY_SIZE(pollfds),
              timeoutMs == TIME_INFINITE ? -1 : (int)timeoutMs);

    if (rc < 0) {
        if (errno == EINTR) {
            failed       = false;
            readyToRead  = false;
            readyToWrite = false;
        } else {
            failed       = true;
            readyToRead  = false; /* quiet compiler warning */
            readyToWrite = false; /* quiet compiler warning */
        }
    } else {
        failed       = false;
        readyToRead  = !!(pollfds[0].revents & POLLIN);
        readyToWrite = !!(pollfds[0].revents & POLLOUT);
    }

    if (failedP)
        *failedP       = failed;
    if (readyToReadP)
        *readyToReadP  = readyToRead;
    if (readyToWriteP)
        *readyToWriteP = readyToWrite;
}



static ChannelInterruptImpl channelInterrupt;

static void
channelInterrupt(TChannel * const channelP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in channelWait()
  now or in the future.

  TODO: Make a way to reset this so that future channelWait()s can once
  again wait.
-----------------------------------------------------------------------------*/
    struct socketUnix * const socketUnixP = channelP->implP;

    sockutil_interruptPipeInterrupt(socketUnixP->interruptPipe);
}



void
ChannelUnixGetPeerName(TChannel *         const channelP,
                       struct sockaddr ** const sockaddrPP,
                       size_t  *          const sockaddrLenP,
                       const char **      const errorP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    sockutil_getPeerName(socketUnixP->fd, sockaddrPP, sockaddrLenP, errorP);
}



static ChannelFormatPeerInfoImpl channelFormatPeerInfo;

static void
channelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    struct socketUnix * const socketUnixP = channelP->implP;

    sockutil_formatPeerInfo(socketUnixP->fd, peerStringP);
}



static struct TChannelVtbl const channelVtbl = {
    &channelDestroy,
    &channelWrite,
    &channelRead,
    &channelWait,
    &channelInterrupt,
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
        
        *errorP = NULL;
    }
    *channelInfoPP = channelInfoP;
}



static void
makeChannelFromFd(int           const fd,
                  TChannel **   const channelPP,
                  const char ** const errorP) {

    struct socketUnix * socketUnixP;

    MALLOCVAR(socketUnixP);
    
    if (socketUnixP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Unix "
                        "channel descriptor");
    else {
        TChannel * channelP;
        
        socketUnixP->fd = fd;
        socketUnixP->userSuppliedFd = true;

        sockutil_interruptPipeInit(&socketUnixP->interruptPipe, errorP);

        if (!*errorP) {
            ChannelCreate(&channelVtbl, socketUnixP, &channelP);
        
            if (channelP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel descriptor.");
            else {
                *channelPP = channelP;
                *errorP = NULL;
            }
            if (*errorP)
                sockutil_interruptPipeTerm(socketUnixP->interruptPipe);
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

    if (!sockutil_connected(fd))
        xmlrpc_asprintf(errorP, "Socket on file descriptor %d is not in "
                        "connected state.", fd);
    else {
        struct sockaddr * peerAddrP;
        size_t peerAddrLen;
        const char * error;

        sockutil_getPeerName(fd, &peerAddrP, &peerAddrLen, &error);

        if (error) {
            xmlrpc_asprintf(errorP, "Failed to get identity of client.  %s",
                            error);
            xmlrpc_strfree(error);
        } else {
            makeChannelInfo(channelInfoPP, *peerAddrP, peerAddrLen, errorP);
            if (!*errorP) {
                makeChannelFromFd(fd, channelPP, errorP);

                if (*errorP)
                    free(*channelInfoPP);
            }
            free(peerAddrP);
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

    sockutil_interruptPipeTerm(socketUnixP->interruptPipe);

    if (!socketUnixP->userSuppliedFd)
        close(socketUnixP->fd);

    free(socketUnixP);
}



static SwitchListenImpl chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct socketUnix * const socketUnixP = chanSwitchP->implP;

    if (socketUnixP->isListening)
        xmlrpc_asprintf(errorP, "Channel switch is already listening");
    else {
        sockutil_listen(socketUnixP->fd, backlog, errorP);

        if (!*errorP)
            socketUnixP->isListening = true;
    }
}



static void
createChannelForAccept(int             const acceptedFd,
                       struct sockaddr const peerAddr,
                       TChannel **     const channelPP,
                       void **         const channelInfoPP,
                       const char **   const errorP) {
/*----------------------------------------------------------------------------
   Make a channel object (TChannel) out of a socket just created by
   accept() on a listening socket -- i.e. a socket for a client connection.

   'acceptedFd' is the file descriptor of the socket.

   'peerAddr' is the address of the client, from accept().
-----------------------------------------------------------------------------*/
    struct abyss_unix_chaninfo * channelInfoP;

    makeChannelInfo(&channelInfoP, peerAddr, sizeof(peerAddr), errorP);
    if (!*errorP) {
        struct socketUnix * acceptedSocketP;

        MALLOCVAR(acceptedSocketP);

        if (!acceptedSocketP)
            xmlrpc_asprintf(errorP, "Unable to allocate memory");
        else {
            acceptedSocketP->fd = acceptedFd;
            acceptedSocketP->userSuppliedFd = false;

            sockutil_interruptPipeInit(
                &acceptedSocketP->interruptPipe, errorP);

            if (!*errorP) {
                TChannel * channelP;

                ChannelCreate(&channelVtbl, acceptedSocketP, &channelP);
                if (!channelP)
                    xmlrpc_asprintf(errorP,
                                    "Failed to create TChannel object.");
                else {
                    *errorP        = NULL;
                    *channelPP     = channelP;
                    *channelInfoPP = channelInfoP;
                }
                if (*errorP)
                    sockutil_interruptPipeTerm(acceptedSocketP->interruptPipe);
            }
            if (*errorP)
                free(acceptedSocketP);
        }
        if (*errorP)
            free(channelInfoP);
    }
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

    bool interrupted;
    TChannel * channelP;

    interrupted = false; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    while (!channelP && !*errorP && !interrupted) {

        sockutil_waitForConnection(listenSocketP->fd,
                                   listenSocketP->interruptPipe,
                                   &interrupted, errorP);

        if (!*errorP && !interrupted) {
            struct sockaddr peerAddr;
            socklen_t peerAddrLen;
            int rc;

            peerAddrLen = sizeof(peerAddr);  /* initial value */

            rc = accept(listenSocketP->fd, &peerAddr, &peerAddrLen);

            if (rc >= 0) {
                int const acceptedFd = rc;

                createChannelForAccept(acceptedFd, peerAddr,
                                       &channelP, channelInfoPP, errorP);

                if (*errorP)
                    close(acceptedFd);
            } else if (errno == EINTR)
                interrupted = true;
            else
                xmlrpc_asprintf(errorP, "accept() failed, errno = %d (%s)",
                                errno, strerror(errno));
        }
    }
    *channelPP = channelP;
}



static SwitchInterruptImpl chanSwitchInterrupt;

static void
chanSwitchInterrupt(TChanSwitch * const chanSwitchP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in chanSwitchAccept()
  now or in the future.

  TODO: Make a way to reset this so that future chanSwitchAccept()s can once
  again wait.
-----------------------------------------------------------------------------*/
    struct socketUnix * const listenSocketP = chanSwitchP->implP;

    unsigned char const zero[1] = {0u};

    write(listenSocketP->interruptPipe.interruptorFd, &zero, sizeof(zero));
}



static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
    &chanSwitchInterrupt,
};



static void
createChanSwitch(int            const fd,
                 bool           const userSuppliedFd,
                 TChanSwitch ** const chanSwitchPP,
                 const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create a channel switch from the bound, but not yet listening, socket
   with file descriptor 'fd'.

   Return the handle of the new channel switch as *chanSwitchPP.

   'userSuppliedFd' means the file descriptor (socket) shall _not_ belong to
   the channel switch, so destroying the channel switch does not close it.
-----------------------------------------------------------------------------*/
    struct socketUnix * socketUnixP;

    assert(!sockutil_connected(fd));

    if (SwitchTraceIsActive)
        fprintf(stderr, "Creating Unix listen-socket based channel switch\n");

    MALLOCVAR(socketUnixP);

    if (socketUnixP == NULL)
        xmlrpc_asprintf(errorP, "unable to allocate memory for Unix "
                        "channel switch descriptor.");
    else {
        TChanSwitch * chanSwitchP;

        socketUnixP->fd = fd;
        socketUnixP->userSuppliedFd = userSuppliedFd;
        socketUnixP->isListening = false;
            
        sockutil_interruptPipeInit(&socketUnixP->interruptPipe, errorP);

        if (!*errorP) {
            ChanSwitchCreate(&chanSwitchVtbl, socketUnixP, &chanSwitchP);
            if (*errorP)
                sockutil_interruptPipeTerm(socketUnixP->interruptPipe);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
        }
        if (*errorP)
            free(socketUnixP);
    }
}



static void
switchCreateIpV4Port(unsigned short          const portNumber,
                     TChanSwitch **          const chanSwitchPP,
                     const char **           const errorP) {
/*----------------------------------------------------------------------------
   Create a POSIX-socket-based channel switch for an IPv4 endpoint.

   Set the socket's local address so that a subsequent "listen" will listen on
   all interfaces, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    int rc;
    rc = socket(PF_INET, SOCK_STREAM, 0);
    if (rc < 0)
        xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                        errno, strerror(errno));
    else {
        int const socketFd = rc;

        sockutil_setSocketOptions(socketFd, errorP);
        if (!*errorP) {
            sockutil_bindSocketToPortInet(socketFd, portNumber, errorP);

            if (!*errorP) {
                bool const userSupplied = false;
                createChanSwitch(socketFd, userSupplied, chanSwitchPP, errorP);
            }
        }
        if (*errorP)
            close(socketFd);
    }
}



static void
switchCreateIpV6Port(unsigned short          const portNumber,
                     TChanSwitch **          const chanSwitchPP,
                     const char **           const errorP) {
/*----------------------------------------------------------------------------
  Same as switchCreateIpV4Port(), except for IPv6.
-----------------------------------------------------------------------------*/
    int rc;
    rc = socket(PF_INET6, SOCK_STREAM, 0);
    if (rc < 0)
        xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                        errno, strerror(errno));
    else {
        int const socketFd = rc;

        sockutil_setSocketOptions(socketFd, errorP);
        if (!*errorP) {
            sockutil_bindSocketToPortInet6(socketFd, portNumber, errorP);

            if (!*errorP) {
                bool const userSupplied = false;
                createChanSwitch(socketFd, userSupplied, chanSwitchPP, errorP);
            }
        }
        if (*errorP)
            close(socketFd);
    }
}



void
ChanSwitchUnixCreate(unsigned short const portNumber,
                     TChanSwitch ** const chanSwitchPP,
                     const char **  const errorP) {

    switchCreateIpV4Port(portNumber, chanSwitchPP, errorP);
}



void
ChanSwitchUnixCreate2(int                     const protocolFamily,
                      const struct sockaddr * const sockAddrP,
                      socklen_t               const sockAddrLen,
                      TChanSwitch **          const chanSwitchPP,
                      const char **           const errorP) {

    int rc;
    rc = socket(protocolFamily, SOCK_STREAM, 0);
    if (rc < 0)
        xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                        errno, strerror(errno));
    else {
        int const socketFd = rc;

        if (SwitchTraceIsActive)
            fprintf(stderr, "Created socket for protocol family %d\n",
                    protocolFamily);

        sockutil_setSocketOptions(socketFd, errorP);
        if (!*errorP) {
            sockutil_bindSocketToPort(socketFd, sockAddrP, sockAddrLen,
                                      errorP);

            if (!*errorP) {
                bool const userSupplied = false;
                createChanSwitch(socketFd, userSupplied, chanSwitchPP, errorP);
            }
        }
        if (*errorP)
            close(socketFd);
    }

}



void
ChanSwitchUnixCreateIpV6Port(unsigned short const portNumber,
                             TChanSwitch ** const chanSwitchPP,
                             const char **  const errorP) {

    switchCreateIpV6Port(portNumber, chanSwitchPP, errorP);
}



void
ChanSwitchUnixCreateFd(int            const fd,
                       TChanSwitch ** const chanSwitchPP,
                       const char **  const errorP) {

    if (sockutil_connected(fd))
        xmlrpc_asprintf(errorP,
                        "Socket (file descriptor %d) is in connected "
                        "state.", fd);
    else {
        bool const userSupplied = true;
        createChanSwitch(fd, userSupplied, chanSwitchPP, errorP);
    }
}



void
ChanSwitchUnixGetListenName(TChanSwitch *      const chanSwitchP,
                            struct sockaddr ** const sockaddrPP,
                            size_t  *          const sockaddrLenP,
                            const char **      const errorP) {
/*----------------------------------------------------------------------------
   The primary case where this is useful is where the user created the channel
   switch with a parameters telling the OS to pick the TCP port.  In that
   case, this is the only way the user can find out what port the OS picked.
-----------------------------------------------------------------------------*/
    struct socketUnix * const socketUnixP = chanSwitchP->implP;
    
    if (!socketUnixP->isListening)
        xmlrpc_asprintf(errorP, "Channel Switch is not listening");
    else
        sockutil_getSockName(socketUnixP->fd,
                             sockaddrPP, sockaddrLenP, errorP);
}



/*=============================================================================
      obsolete TSocket interface
=============================================================================*/


void
SocketUnixCreateFd(int        const fd,
                   TSocket ** const socketPP) {

    TSocket * socketP;
    const char * error;

    if (sockutil_connected(fd)) {
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




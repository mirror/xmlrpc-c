/*=============================================================================
                                 socket_openssl.c
===============================================================================
  This is the implementation of TChanSwitch and TChannel
  for an SSL (Secure Sockets Layer) connection based on an OpenSSL
  connection object -- what you create with SSL_new().

  This is just a template for future development.  It does not function
  (or even compile) today.

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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "bool.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"
#include "xmlrpc-c/abyss_openssl.h"

#include "sockutil.h"
#include "socket_openssl.h"



struct ChannelOpenSsl {
/*----------------------------------------------------------------------------
   The properties/state of a TChannel unique to the OpenSSL variety.
-----------------------------------------------------------------------------*/
    int fd;
        /* File descriptor of the TCP connection underlying the SSL connection
        */
    SSL * sslP;
        /* SSL connection handle */
    bool userSuppliedSsl;
        /* The SSL connection belongs to the user; we did not create it. */
};



void
SocketOpenSslInit(const char ** const errorP) {

	SSL_load_error_strings();
        /* readable error messages, don't call this if memory is tight */
	SSL_library_init();   /* initialize library */

	/* actions_to_seed_PRNG(); */

	*errorP = NULL;
}



void
SocketOpenSslTerm(void) {

	ERR_free_strings();

}



/*=============================================================================
      TChannel
=============================================================================*/

static ChannelDestroyImpl channelDestroy;

static void
channelDestroy(TChannel * const channelP) {

    struct ChannelOpenSsl * const channelOpenSslP = channelP->implP;

    if (!channelOpenSslP->userSuppliedSsl)
        SSL_shutdown(channelOpenSslP->sslP);

    free(channelOpenSslP);
}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP) {

    struct ChannelOpenSsl * const channelOpenSslP = channelP->implP;

    int bytesLeft;
    bool error;

    assert(sizeof(int) >= sizeof(len));

    for (bytesLeft = len, error = false; bytesLeft > 0 && !error; ) {
        int const maxSend = (int)(-1) >> 1;

        int rc;
        
        rc = SSL_write(channelOpenSslP->sslP, &buffer[len-bytesLeft],
                       MIN(maxSend, bytesLeft));

        if (ChannelTraceIsActive) {
            if (rc <= 0)
                fprintf(stderr,
                        "Abyss socket: SSL_write() failed.  rc=%d/%d",
                        rc, SSL_get_error(channelOpenSslP->sslP, rc));
            else
                fprintf(stderr, "Abyss socket: sent %u bytes: '%.*s'\n",
                        rc, rc, &buffer[len-bytesLeft]);
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

    struct ChannelOpenSsl * const channelOpenSslP = channelP->implP;

    int rc;
    rc = SSL_read(channelOpenSslP->sslP, buffer, bufferSize);

    if (rc < 0) {
        *failedP = true;
        if (ChannelTraceIsActive)
            fprintf(stderr, "Failed to receive data from OpenSSL connection.  "
                    "SSL_read() failed with rc %d/%d\n",
                    rc, SSL_get_error(channelOpenSslP->sslP, rc));
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
channelWait(TChannel * const channelP ATTR_UNUSED,
            bool       const waitForRead ATTR_UNUSED,
            bool       const waitForWrite ATTR_UNUSED,
            uint32_t   const timeoutMs ATTR_UNUSED,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {
/*----------------------------------------------------------------------------
  See socket_unix.c for an explanation of the purpose of this
  subroutine.

  We don't actually fulfill that purpose, though, because we don't know
  how yet.  Instead, we return immediately and hope that if Caller
  subsequently does a read or write, it blocks until it can do its thing.
-----------------------------------------------------------------------------*/
    *readyToReadP = true;
    *readyToWriteP = true;
    *failedP = false;
}



static ChannelInterruptImpl channelInterrupt;

static void
channelInterrupt(TChannel * const channelP ATTR_UNUSED) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in channelWait()
  now or in the future.
-----------------------------------------------------------------------------*/

    /* This is trivial, since channelWait() doesn't actually wait */
}



static ChannelFormatPeerInfoImpl channelFormatPeerInfo;

static void
channelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    struct ChannelOpenSsl * const channelOpenSslP = channelP->implP;

    sockutil_formatPeerInfo(channelOpenSslP->fd, peerStringP);
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
makeChannelInfo(struct abyss_openSsl_chaninfo ** const channelInfoPP,
                SSL *                            const sslP,
                const char **                    const errorP) {

    struct abyss_openSsl_chaninfo * channelInfoP;

    MALLOCVAR(channelInfoP);
    
    if (channelInfoP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        int const sockFd = SSL_get_fd(sslP);
        struct sockaddr * peerAddrP;
        size_t peerAddrLen;
        const char * error;

        sockutil_getPeerName(sockFd, &peerAddrP, &peerAddrLen, &error);

        if (error) {
            xmlrpc_asprintf(errorP, "Could not get identity of client.  %s",
                            error);
            xmlrpc_strfree(error);
        } else {
            channelInfoP->peerAddrLen = peerAddrLen;
            channelInfoP->peerAddr    = *peerAddrP;

            free(peerAddrP);
        }

        if (*errorP)
            free(channelInfoP);
        else
            *channelInfoPP = channelInfoP;
    }
}



static void
makeChannelFromSsl(SSL *         const sslP,
                   bool          const userSuppliedSsl,
                   TChannel **   const channelPP,
                   const char ** const errorP) {

    struct ChannelOpenSsl * channelOpenSslP;

    MALLOCVAR(channelOpenSslP);
    
    if (channelOpenSslP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for OpenSSL "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        channelOpenSslP->sslP = sslP;
        channelOpenSslP->userSuppliedSsl = userSuppliedSsl;
        
        /* This should be ok as far as I can tell */
        ChannelCreate(&channelVtbl, channelOpenSslP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP)
            free(channelOpenSslP);
    }
}



void
ChannelOpenSslCreateSsl(SSL *                            const sslP,
                        TChannel **                      const channelPP,
                        struct abyss_openSsl_chaninfo ** const channelInfoPP,
                        const char **                    const errorP) {

    assert(sslP);

    makeChannelInfo(channelInfoPP, sslP, errorP);
    if (!*errorP) {
        bool const userSuppliedTrue = true;

        makeChannelFromSsl(sslP, userSuppliedTrue, channelPP, errorP);
        
        if (*errorP) {
            free(*channelInfoPP);
        }
    }
}



/*=============================================================================
      TChanSwitch
=============================================================================*/

struct ChanSwitchOpenSsl {
/*----------------------------------------------------------------------------
   The properties/state of a TChanSwitch uniqe to the OpenSSL variety.

   Note that OpenSSL deals only in connected sockets, so this switch
   doesn't really have anything to do with OpenSSL except that it
   creates OpenSSL TChannels.  The switch is just a POSIX listening
   socket, and is almost identical to the Abyss Unix channel switch.
-----------------------------------------------------------------------------*/
    int listenFd;
        /* File descriptor of the POSIX socket (such as is created by
           socket() in the C library) for the listening socket.
        */
    bool userSuppliedFd;
        /* The file descriptor and associated POSIX socket belong to the
           user; we did not create it.
        */
    SSL_CTX * sslCtxP;
        /* The context in which we create all our OpenSSL connections */

    sockutil_InterruptPipe interruptPipe;
        /* We use this to interrupt a wait for the next client to arrive */
};



static SwitchDestroyImpl chanSwitchDestroy;

static void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct ChanSwitchOpenSsl * const chanSwitchOpenSslP = chanSwitchP->implP;
 
    sockutil_interruptPipeTerm(chanSwitchOpenSslP->interruptPipe);

    SSL_CTX_free(chanSwitchOpenSslP->sslCtxP);

    if (!chanSwitchOpenSslP->userSuppliedFd)
        close(chanSwitchOpenSslP->listenFd);

    free(chanSwitchOpenSslP);
}



static SwitchListenImpl chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct ChanSwitchOpenSsl * const chanSwitchOpenSslP = chanSwitchP->implP;

    sockutil_listen(chanSwitchOpenSslP->listenFd, backlog, errorP);
}



static void
createSslFromAcceptedConn(int           const acceptedFd,
                          SSL_CTX *     const sslCtxP,
                          SSL **        const sslPP,
                          const char ** const errorP) {
          
    SSL * sslP;
      
    sslP = SSL_new(sslCtxP);

    if (!sslP)
        xmlrpc_asprintf(errorP, "Failed to create SSL connection "
                        "object.  SSL_new() failed");
    else {
        int succeeded;

        succeeded = SSL_set_fd(sslP, acceptedFd);
        
        if (!succeeded)
            xmlrpc_asprintf(errorP, "SSL_set_fd(%d) failed.", acceptedFd);
        else {
            int rc;

            rc = SSL_accept(sslP);

            if (rc != 1)
                xmlrpc_asprintf(errorP, "SSL_accept() failed.  rc=%d/%d",
                                rc, SSL_get_error(sslP, rc));
        }
        if (*errorP)
            SSL_free(sslP);
        else {
            *sslPP = sslP;
        }
    }
}



static void
createChannelFromAcceptedConn(int             const acceptedFd,
                              SSL_CTX *       const sslCtxP,
                              TChannel **     const channelPP,
                              void **         const channelInfoPP,
                              const char **   const errorP) {

    struct ChannelOpenSsl * channelOpenSslP;

    MALLOCVAR(channelOpenSslP);

    if (!channelOpenSslP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        SSL * sslP;
        const char * error;

        createSslFromAcceptedConn(acceptedFd, sslCtxP, &sslP, &error);
        
        if (error) {
            xmlrpc_asprintf(errorP, "Failed to create an OpenSSL connection "
                            "from the accepted TCP connection.  %s", error);
            xmlrpc_strfree(error);
        } else {
            struct abyss_openSsl_chaninfo * channelInfoP;

            makeChannelInfo(&channelInfoP, sslP, errorP);
            if (!*errorP) {
                bool const userSuppliedFalse = false;

                makeChannelFromSsl(sslP, userSuppliedFalse,
                                   channelPP, errorP);

                if (*errorP)
                    free(channelInfoP);
                else
                    *channelInfoPP = channelInfoP;
            }
            if (*errorP)
                SSL_free(sslP);
            else
                channelOpenSslP->sslP = sslP;
        }
        if (*errorP)
            free(channelOpenSslP);
    }
}



static SwitchAcceptImpl  chanSwitchAccept;

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
    struct ChanSwitchOpenSsl * const chanSwitchOpenSslP = chanSwitchP->implP;

    bool interrupted;
    TChannel * channelP;

    interrupted = false; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    while (!channelP && !*errorP && !interrupted) {
        struct sockaddr peerAddr;
        socklen_t peerAddrLen;
        int rc;

        peerAddrLen = sizeof(peerAddr);  /* initial value */

        rc = accept(chanSwitchOpenSslP->listenFd, &peerAddr, &peerAddrLen);

        if (rc >= 0) {
            int const acceptedFd = rc;

            const char * error;

            createChannelFromAcceptedConn(
                acceptedFd, chanSwitchOpenSslP->sslCtxP,
                channelPP, channelInfoPP, &error);

            if (error) {
                xmlrpc_asprintf(errorP, "Failed to create a channel from the "
                                "TCP connection we accepted.  %s", error);
                xmlrpc_strfree(error);
            }
            if (*errorP)
                close(acceptedFd);
        } else if (errno == EINTR)
            interrupted = true;
        else
            xmlrpc_asprintf(errorP, "accept() failed, errno = %d (%s)",
                            errno, strerror(errno));
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
    struct ChanSwitchOpenSsl * const chanSwitchOpenSslP = chanSwitchP->implP;

    unsigned char const zero[1] = {0u};

    write(chanSwitchOpenSslP->interruptPipe.interruptorFd,
          &zero, sizeof(zero));
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

    struct ChanSwitchOpenSsl * chanSwitchOpenSslP;

    assert(!sockutil_connected(fd));

    if (SwitchTraceIsActive)
        fprintf(stderr, "Creating OpenSSL-based channel switch\n");

    MALLOCVAR(chanSwitchOpenSslP);

    if (chanSwitchOpenSslP == NULL)
        xmlrpc_asprintf(errorP, "unable to allocate memory for OpenSSL "
                        "channel switch descriptor.");
    else {
        TChanSwitch * chanSwitchP;

        chanSwitchOpenSslP->sslCtxP = SSL_CTX_new(SSLv23_server_method());

        chanSwitchOpenSslP->listenFd = fd;
        chanSwitchOpenSslP->userSuppliedFd = userSuppliedFd;
            
        sockutil_interruptPipeInit(&chanSwitchOpenSslP->interruptPipe, errorP);

        if (!*errorP) {
            ChanSwitchCreate(&chanSwitchVtbl, chanSwitchOpenSslP,
                             &chanSwitchP);
            if (*errorP)
                sockutil_interruptPipeTerm(chanSwitchOpenSslP->interruptPipe);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
        }
        if (*errorP)
            free(chanSwitchOpenSslP);
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
ChanSwitchOpenSslCreate(int                     const protocolFamily,
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
ChanSwitchOpenSslCreateIpV4Port(unsigned short const portNumber,
                                TChanSwitch ** const chanSwitchPP,
                                const char **  const errorP) {

    switchCreateIpV4Port(portNumber, chanSwitchPP, errorP);
}



void
ChanSwitchOpenSslCreateIpV6Port(unsigned short const portNumber,
                                TChanSwitch ** const chanSwitchPP,
                                const char **  const errorP) {

    switchCreateIpV6Port(portNumber, chanSwitchPP, errorP);
}



void
ChanSwitchOpenSslCreateFd(int            const fd,
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



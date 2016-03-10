#ifndef ABYSS_OPENSSL_H_INCLUDED
#define ABYSS_OPENSSL_H_INCLUDED
/*=============================================================================
                               abyss_openssl.h
===============================================================================
  Declarations to be used with an Abyss server that can server HTTPS URLs
  via OpenSSL.

  Note that there is no equivalent of this file for the original two Abyss
  channel types, "Unix" and "Windows".  Those are built into
  <xmlrpc-c/abyss.h>, for historical reasons.
=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <xmlrpc-c/abyss.h>
#include <openssl/ssl.h>

struct abyss_openSsl_chaninfo {
    /* TODO: figure out useful information to put in here.
       Maybe authenticated host name.
       Maybe authentication level.
       Maybe a certificate.
    */
    size_t peerAddrLen;
    struct sockaddr peerAddr;
};

void
ChanSwitchOpenSslCreate(int                     const protocolFamily,
                        const struct sockaddr * const sockAddrP,
                        socklen_t               const sockAddrLen,
                        SSL_CTX *               const sslCtxP,
                        TChanSwitch **          const chanSwitchPP,
                        const char **           const errorP);

void
ChanSwitchOpenSslCreateIpV4Port(unsigned short const portNumber,
                                SSL_CTX *      const sslCtxP,
                                TChanSwitch ** const chanSwitchPP,
                                const char **  const errorP);

void
ChanSwitchOpenSslCreateIpV6Port(unsigned short const portNumber,
                                SSL_CTX *      const sslCtxP,
                                TChanSwitch ** const chanSwitchPP,
                                const char **  const errorP);

void
ChanSwitchOpenSslCreateFd(int            const fd,
                          SSL_CTX *      const sslCtxP,
                          TChanSwitch ** const chanSwitchPP,
                          const char **  const errorP);

void
ChannelOpenSslCreateSsl(SSL *                            const sslP,
                        TChannel **                      const channelPP,
                        struct abyss_openSsl_chaninfo ** const channelInfoPP,
                        const char **                    const errorP);

#ifdef __cplusplus
}
#endif

#endif

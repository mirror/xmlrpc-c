#include <stdexcept>

using namespace std;

#ifdef _WIN32
  /* With _WIN32, <xmlrpc-c/abyss.h> does not declare
     ChanSwitchOpenSslCreate */
  #error This module does not compile for a Windows target.
#endif

#include <sys/socket.h>
#include <openssl/ssl.h>

#include "xmlrpc-c/abyss.h"
#include "xmlrpc-c/abyss_openssl.h"


#include "xmlrpc-c/AbyssChanSwitchOpenSsl.hpp"




namespace xmlrpc_c {



AbyssChanSwitchOpenSsl::AbyssChanSwitchOpenSsl(
    int                     const protocolFamily,
    const struct sockaddr * const sockAddrP,
    socklen_t               const sockAddrLen,
    SSL_CTX *               const sslCtxP) {

    const char * error;

    ChanSwitchOpenSslCreate(protocolFamily, sockAddrP, sockAddrLen, sslCtxP,
                            &this->_cChanSwitchP, &error);

    if (error)
        throw runtime_error(error);
}



AbyssChanSwitchOpenSsl::AbyssChanSwitchOpenSsl(
    unsigned short const listenPortNum,
    SSL_CTX *      const sslCtxP) {

    const char * error;

    ChanSwitchOpenSslCreateIpV4Port(listenPortNum, sslCtxP,
                                    &this->_cChanSwitchP, &error);

    if (error)
        throw runtime_error(error);
}



}  // namespace

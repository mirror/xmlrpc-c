#ifndef SERVER_PSTREAM_HPP_INCLUDED
#define SERVER_PSTREAM_HPP_INCLUDED

#ifdef WIN32
#include <winsock.h>  /* For XMLRPC_SOCKET (= SOCKET) */
#endif

#include <xmlrpc-c/config.h>  /* For XMLRPC_SOCKET */
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/packetsocket.hpp>

namespace xmlrpc_c {

class serverPstreamConn {

public:

    class constrOpt {
    public:
        constrOpt();
        ~constrOpt();

        constrOpt & registryPtr       (xmlrpc_c::registryPtr      const& arg);
        constrOpt & registryP         (const xmlrpc_c::registry * const& arg);
        constrOpt & socketFd          (XMLRPC_SOCKET  const& arg);

    private:
        struct constrOpt_impl * implP;
        friend class serverPstreamConn;
    };

    serverPstreamConn(constrOpt const& opt);

    ~serverPstreamConn();

    void
    runOnce(volatile const int * const interruptP,
            bool *               const eofP);

    void
    runOnce(bool * const eofP);

    void
    runOnceNoWait(bool * const eofP,
                  bool * const didOneP);

    void
    runOnceNoWait(bool * const eofP);

private:
    struct serverPstreamConn_impl * implP;
};


} // namespace

#endif

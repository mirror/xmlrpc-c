#ifndef SERVER_PSTREAM_HPP_INCLUDED
#define SERVER_PSTREAM_HPP_INCLUDED

#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/packetsocket.hpp>

namespace xmlrpc_c {

class serverPstreamConn {

public:

    class constrOpt {
    public:
        constrOpt();

        constrOpt & registryPtr       (xmlrpc_c::registryPtr      const& arg);
        constrOpt & registryP         (const xmlrpc_c::registry * const& arg);
        constrOpt & socketFd          (xmlrpc_socket  const& arg);

        struct value {
            xmlrpc_c::registryPtr      registryPtr;
            const xmlrpc_c::registry * registryP;
            xmlrpc_socket              socketFd;
        } value;
        struct {
            bool registryPtr;
            bool registryP;
            bool socketFd;
        } present;
    };

    serverPstreamConn::serverPstreamConn(constrOpt const& opt);

    serverPstreamConn::~serverPstreamConn();

    void
    serverPstreamConn::runOnce(bool * const eofP);

private:

    // 'registryP' is what we actually use; 'registryHolder' just holds a
    // reference to 'registryP' so the registry doesn't disappear while
    // this server exists.  But note that if the creator doesn't supply
    // a registryPtr, 'registryHolder' is just a placeholder variable and
    // the creator is responsible for making sure the registry doesn't
    // go anywhere while the server exists.

    registryPtr registryHolder;
    const registry * registryP;

    packetSocket * packetSocketP;
        // The packet socket over which we received RPCs.
        // This is permanently connected to our fixed client.

    void
    serverPstreamConn::establishRegistry(constrOpt const& opt);

    void
    serverPstreamConn::establishPacketSocket(constrOpt const& opt);
};


} // namespace

#endif

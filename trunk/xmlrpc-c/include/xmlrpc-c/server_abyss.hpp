#ifndef SERVER_ABYSS_HPP_INCLUDED
#define SERVER_ABYSS_HPP_INCLUDED

#include "xmlrpc-c/base.hpp"
#include "abyss.h"

namespace xmlrpc_c {

class serverAbyss {
    
public:
    class constrOpt {
    public:
        constrOpt();

        constrOpt & registryP         (const xmlrpc_c::registry * const& arg);
        constrOpt & socketFd          (xmlrpc_socket  const& arg);
        constrOpt & portNumber        (uint           const& arg);
        constrOpt & logFileName       (std::string    const& arg);
        constrOpt & keepaliveTimeout  (uint           const& arg);
        constrOpt & keepaliveMaxConn  (uint           const& arg);
        constrOpt & timeout           (uint           const& arg);
        constrOpt & dontAdvertise     (bool           const& arg);

        struct value {
            const xmlrpc_c::registry * registryP;
            xmlrpc_socket  socketFd;
            uint           portNumber;
            std::string    logFileName;
            uint           keepaliveTimeout;
            uint           keepaliveMaxConn;
            uint           timeout;
            bool           dontAdvertise;
        } value;
        struct {
            bool registryP;
            bool socketFd;
            bool portNumber;
            bool logFileName;
            bool keepaliveTimeout;
            bool keepaliveMaxConn;
            bool timeout;
            bool dontAdvertise;
        } present;
    };

    serverAbyss(constrOpt const& opt);

    serverAbyss(
        xmlrpc_c::registry const& registry,
        unsigned int       const  portNumber = 8080,
        std::string        const& logFileName = "",
        unsigned int       const  keepaliveTimeout = 0,
        unsigned int       const  keepaliveMaxConn = 0,
        unsigned int       const  timeout = 0,
        bool               const  dontAdvertise = false,
        bool               const  socketBound = false,
        xmlrpc_socket      const  socketFd = 0
        );
    ~serverAbyss();
    
    void
    run();

    void
    runOnce();

    void
    runConn(int const socketFd);
    
private:
    TServer cServer;

    void
    serverAbyss::setAdditionalServerParms(constrOpt const& opt);

    void
    serverAbyss::initialize(constrOpt const& opt);
};


void
server_abyss_set_handlers(TServer *          const  srvP,
                          xmlrpc_c::registry const& registry);

void
server_abyss_set_handlers(TServer *                  const srvP,
                          const xmlrpc_c::registry * const registryP);

} // namespace

#endif

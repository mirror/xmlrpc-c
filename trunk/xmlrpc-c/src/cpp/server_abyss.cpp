#include <cassert>
#include <string>
#include <memory>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/server_abyss.h"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_abyss.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {

namespace {


static void 
sigterm(int const sig) {
    TraceExit("Signal %d received. Exiting...\n",sig);
}



static void 
sigchld(int const sig) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   We respond by reaping the zombie process.

   Implementation note: In some systems, just setting the signal handler
   to SIG_IGN (ignore signal) does this.  In others, it doesn't.
-----------------------------------------------------------------------------*/
#ifndef _WIN32
    /* Reap zombie children until there aren't any more. */

    bool zombiesExist;
    bool error;

    assert(sig == SIGCHLD);
    
    zombiesExist = true;  // initial assumption
    error = false;  // no error yet
    while (zombiesExist && !error) {
        int status;
        pid_t const pid = waitpid((pid_t) -1, &status, WNOHANG);
    
        if (pid == 0)
            zombiesExist = false;
        else if (pid < 0) {
            /* because of ptrace */
            if (errno == EINTR) {
                // This is OK - it's a ptrace notification
            } else
                error = true;
        }
    }
#endif /* _WIN32 */
}



void
setupSignalHandlers(void) {
#ifndef _WIN32
    struct sigaction mysigaction;
   
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* These signals abort the program, with tracing */
    mysigaction.sa_handler = sigterm;
    sigaction(SIGTERM, &mysigaction, NULL);
    sigaction(SIGINT,  &mysigaction, NULL);
    sigaction(SIGHUP,  &mysigaction, NULL);
    sigaction(SIGUSR1, &mysigaction, NULL);

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, NULL);
   
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, NULL);
#endif
}    


} // namespace



serverAbyss::constrOpt::constrOpt() {
    present.registryP        = false;
    present.socketFd         = false;
    present.portNumber       = false;
    present.logFileName      = false;
    present.keepaliveTimeout = false;
    present.keepaliveMaxConn = false;
    present.timeout          = false;
    present.dontAdvertise    = false;

    // Set default values
    value.dontAdvertise = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverAbyss::constrOpt & \
serverAbyss::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(registryP,        const registry *);
DEFINE_OPTION_SETTER(socketFd,         xmlrpc_socket);
DEFINE_OPTION_SETTER(portNumber,       uint);
DEFINE_OPTION_SETTER(logFileName,      string);
DEFINE_OPTION_SETTER(keepaliveTimeout, uint);
DEFINE_OPTION_SETTER(keepaliveMaxConn, uint);
DEFINE_OPTION_SETTER(timeout,          uint);
DEFINE_OPTION_SETTER(dontAdvertise,    bool);



void
serverAbyss::setAdditionalServerParms(constrOpt const& opt) {

    /* The following ought to be parameters on ServerCreate(), but it
       looks like plugging them straight into the TServer structure is
       the only way to set them.  
    */

    if (opt.present.keepaliveTimeout)
        ServerSetKeepaliveTimeout(&this->cServer, opt.value.keepaliveTimeout);
    if (opt.present.keepaliveMaxConn)
        ServerSetKeepaliveMaxConn(&this->cServer, opt.value.keepaliveMaxConn);
    if (opt.present.timeout)
        ServerSetTimeout(&this->cServer, opt.value.timeout);
    ServerSetAdvertise(&this->cServer, !opt.value.dontAdvertise);
}



void
serverAbyss::initialize(constrOpt const& opt) {

    if (!opt.present.registryP)
        throwf("You must specify the 'registryP' option");
    
    if (opt.present.portNumber && opt.present.socketFd)
        throwf("You can't specify both portNumber and socketFd options");

    DateInit();
    MIMETypeInit();
    
    const char * const logfileArg(opt.present.logFileName ?
                                  opt.value.logFileName.c_str() : NULL);

    const char * const serverName("XmlRpcServer");
        
    if (opt.present.socketFd)
        ServerCreateSocket(&this->cServer, serverName, opt.value.socketFd,
                           DEFAULT_DOCS, logfileArg);
    else if (opt.present.portNumber) {
        if (opt.value.portNumber > 0xffff)
            throw(error("Port number exceeds the maximum possible port number "
                        "(65535)"));

        ServerCreate(&this->cServer, serverName, opt.value.portNumber,
                     DEFAULT_DOCS, logfileArg);
    } else
        ServerCreateNoAccept(&this->cServer, serverName,
                             DEFAULT_DOCS, logfileArg);

    try {
        setAdditionalServerParms(opt);
        
        xmlrpc_c::server_abyss_set_handlers(&this->cServer,
                                            opt.value.registryP);
        
        if (opt.present.portNumber || opt.present.socketFd)
            ServerInit(&this->cServer);
        
        setupSignalHandlers();
    } catch (...) {
        ServerFree(&this->cServer);
        throw;
    }
}



serverAbyss::serverAbyss(constrOpt const& opt) {

    initialize(opt);
}



serverAbyss::serverAbyss(
    xmlrpc_c::registry const& registry,
    unsigned int       const  portNumber,
    string             const& logFileName,
    unsigned int       const  keepaliveTimeout,
    unsigned int       const  keepaliveMaxConn,
    unsigned int       const  timeout,
    bool               const  dontAdvertise,
    bool               const  socketBound,
    xmlrpc_socket      const  socketFd) {
/*----------------------------------------------------------------------------
  This is a backward compatibility interface.  This used to be the only
  constructor.
-----------------------------------------------------------------------------*/
    serverAbyss::constrOpt opt;

    opt.registryP(&registry);
    if (logFileName.length() > 0)
        opt.logFileName(logFileName);
    if (keepaliveTimeout > 0)
        opt.keepaliveTimeout(keepaliveTimeout);
    if (keepaliveMaxConn > 0)
        opt.keepaliveMaxConn(keepaliveMaxConn);
    if (timeout > 0)
        opt.timeout(timeout);
    opt.dontAdvertise(dontAdvertise);
    if (socketBound)
        opt.socketFd(socketFd);
    else
        opt.portNumber(portNumber);

    initialize(opt);
}



serverAbyss::~serverAbyss() {

    ServerFree(&this->cServer);
}



void
serverAbyss::run() {

    ServerRun(&this->cServer);
}
 


void
serverAbyss::runOnce() {

    ServerRunOnce(&this->cServer);
}



void
serverAbyss::runConn(int const socketFd) {

    ServerRunConn(&this->cServer, socketFd);
}



void
server_abyss_set_handlers(TServer *          const  srvP,
                          xmlrpc_c::registry const& registry) {

    xmlrpc_server_abyss_set_handlers(srvP, registry.c_registry());
}



void
server_abyss_set_handlers(TServer *                  const srvP,
                          const xmlrpc_c::registry * const registryP) {

    xmlrpc_server_abyss_set_handlers(srvP, registryP->c_registry());
}



} // namespace


/*=============================================================================
                                wininet.cpp
===============================================================================
  This is the Wininet XML transport of the C++ XML-RPC client library for
  Xmlrpc-c.
=============================================================================*/

#include <stdlib.h>
#include <cassert>
#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"
#include "xmlrpc-c/base_int.h"

#include "xmlrpc_wininet_transport.h"

/* transport_config.h defines MUST_BUILD_WININET_CLIENT */
#include "transport_config.h"

#include "xmlrpc-c/client.hpp"


using namespace std;
using namespace xmlrpc_c;


namespace {

carriageParm_wininet0::carriageParm_wininet0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



#if MUST_BUILD_WININET_CLIENT

clientXmlTransport_wininet::clientXmlTransport_wininet(
    bool const allowInvalidSslCerts
    ) {

    struct xmlrpc_wininet_xportparms transportParms; 

    transportParms.allowInvalidSSLCerts = allowInvalidSslCerts;

    this->c_transportOpsP = &xmlrpc_wininet_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_wininet_transport_ops.create(
        &env, 0, "", "", &transportParms, XMLRPC_WXPSIZE(allowInvalidSSLCerts),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}

#else  // MUST_BUILD_WININET_CLIENT

clientXmlTransport_wininet::clientXmlTransport_wininet(bool const) {

    throw(error("There is no Wininet client XML transport "
                "in this XML-RPC client library"));
}

#endif
 


clientXmlTransport_wininet::~clientXmlTransport_wininet() {

    this->c_transportOpsP->destroy(this->c_transportP);
}

} // namespace

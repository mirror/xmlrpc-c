/*=============================================================================
                                libwww.cpp
===============================================================================
  This is the Libwww XML transport of the C++ XML-RPC client library for
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

#include "xmlrpc_libwww_transport.h"

/* transport_config.h defines MUST_BUILD_LIBWWW_CLIENT */
#include "transport_config.h"

#include "xmlrpc-c/client.hpp"


using namespace std;
using namespace xmlrpc_c;


namespace {

carriageParm_libwww0::carriageParm_libwww0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



#if MUST_BUILD_LIBWWW_CLIENT

clientXmlTransport_libwww::clientXmlTransport_libwww(
    string const appname,
    string const appversion) {

    this->c_transportOpsP = &xmlrpc_libwww_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_libwww_transport_ops.create(
        &env, 0, appname.c_str(), appversion.c_str(), NULL, 0,
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}

#else  // MUST_BUILD_LIBWWW_CLIENT
 clientXmlTransport_libwww::clientXmlTransport_libwww(string, string) {

    throw(error("There is no Libwww client XML transport "
                "in this XML-RPC client library"));
}

#endif


clientXmlTransport_libwww::~clientXmlTransport_libwww() {

    this->c_transportOpsP->destroy(this->c_transportP);
}

} // namespace

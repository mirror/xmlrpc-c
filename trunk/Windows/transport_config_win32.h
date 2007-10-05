
/* transport_config.h - hadcrafted for WIN32/_MSC_VER (MSVC) */
#ifndef  _transport_config_h_
#define  _transport_config_h_

#define MUST_BUILD_WININET_CLIENT 1
#define MUST_BUILD_CURL_CLIENT 0
#define MUST_BUILD_LIBWWW_CLIENT 0
static const char * const XMLRPC_DEFAULT_TRANSPORT = "wininet";

/*
Set to zero if you do not wish to build the http.sys
based XMLRPC-C Server
*/
#define MUST_BUILD_HTTP_SYS_SERVER 1

/*
We use pragma statements to tell the linker what we need to link
with.  Since Curl requires Winsock, Winmm, and libcurl, and no other
project does, if we are building with a curl client XML transport we
tell the linker what libs we need to add.

Alternatively, the USER can add the libraries to LINK with as NEEDED!

*/

#if MUST_BUILD_CURL_CLIENT > 0

#ifdef _DEBUG
#pragma comment( lib, "../lib/libcurld.lib" )
#else
#pragma comment( lib, "../lib/libcurl.lib" )
#endif

#pragma comment( lib, "Winmm.lib" )
#pragma comment( lib, "Ws2_32.lib" )

#endif   /* #if MUST_BUILD_CURL_CLIENT > 0 */

#endif   /* #ifndef  _transport_config_h_ */

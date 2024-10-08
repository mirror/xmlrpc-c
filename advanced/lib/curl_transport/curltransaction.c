/*=============================================================================
    curlTransaction
=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "mallocvar.h"

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
#include "version.h"

#include <curl/curl.h>
#if defined(NEED_CURL_TYPES_H)
#include <curl/types.h>
#endif
#include <curl/easy.h>

#include "curlversion.h"

#include "curltransaction.h"


/* ABOUT LIBCURL AND SIGNALS:

   The Curl library has two timeout functions: one on original connection
   (including DNS lookup) and another on the individual transactions.  In the
   original implementation, both use SIGALRM.  But it is a terrible idea for a
   library to mess with signals, because they are process-global.  In fact, if
   the program is multithreaded, this can be disastrous as a different thread
   can receive the signal from the thread that is waiting.  Programs even
   crash.

   It is easy enough for Curl to time out the transaction without using
   SIGALRM, and in current implementations it does.  But for the DNS lookup,
   using the traditional DNS lookup library, SIGALRM is the only way.
   Therefore, the user must make a choice: have a timeout or have multiple
   threads.  To reflect that choice, Curl has the CURLOPT_NOSIGNAL setting.
   When you set that, Curl does the right thing as keeps its hands off the
   signals, but DNS lookups are indefinite.  If you don't, then the connect
   timeout (which defaults to 5 minutes and is user-settable) is effective
   against the DNS lookup, but you had better not have multiple threads.

   For backward compatibility, the default is to use SIGALRM.  That means
   single-threaded programs continue to enjoy DNS lookup timeouts even when
   using new Curl.

   There is an optional way of building the Curl library -- with the ARES
   library -- that allows the connect timeout to work on the DNS lookup without
   SIGALRM

   In Xmlrpc-c, we always set CURLOPT_NOSIGNAL, to avoid undefined behavior in
   multithreaded programs.  This means that if the Curl library was not built
   for ARES, the DNS lookup will not time out.  For consistency, we also set
   CURLOPT_CONNECTTIMEOUT to infinite.  That way, users see the same behavior
   with ARES.

   It wasn't always this way.  Before Xmlrpc-c 1.41, we set CURLOPT_NOSIGNAL
   only when the user specified the 'timeout' curl transport option, and we
   never set CURLOPT_CONNECTTIMEOUT.  This means programs that have a nice 5
   minute (Curl default) DNS lookup timeout with old Xmlrpc-c, (and happen not
   to have the SIGALRM-related crash problem) have an indefinite wait with
   current Xmlrpc-c.  This was an unfortunate break to provide a more usable
   interface to future users.

   For the old Curl that does not have CURLOPT_NOSIGNAL, we fail any attempt
   to set a timeout (and we provide the user a way to know whether such
   failure would occur).
*/

struct curlTransaction {
    /* This is all stuff that really ought to be in a Curl object, but
       the Curl library is a little too simple for that.  So we build
       a layer on top of Curl, and define this "transaction," as an
       object subordinate to a Curl "session."  A Curl session has
       zero or one transactions in progress.  The Curl session
       "private data" is a pointer to the CurlTransaction object for
       the current transaction.
    */
    CURL * curlSessionP;
        /* Handle for the Curl session that hosts this transaction.
           Note that only one transaction at a time can use a particular
           Curl session, so this had better not be a session that some other
           transaction is using simultaneously.
        */
    curlt_finishFn * finish;
    curlt_progressFn * progress;
    void * userContextP;
        /* Meaningful to our client; opaque to us */
    CURLcode result;
        /* Result of the transaction (succeeded, TCP connect failed, etc.).
           A properly executed HTTP transaction (request & response) counts
           as a successful transaction.  When 'result' show success,
           curl_easy_get_info() tells you whether the transaction succeeded
           at the HTTP level.
        */
    char curlError[CURL_ERROR_SIZE];
        /* Error message from Curl */
    struct curl_slist * headerList;
        /* The HTTP headers for the transaction */
    const char * serverUrl;  /* malloc'ed - belongs to this object */
        /* The URL for the transaction */
    xmlrpc_mem_block * postDataP;
        /* The data to send for the POST method */
    xmlrpc_mem_block * responseDataP;
        /* This is normally where to put the body of the HTTP response.  But
           because of a quirk of Curl, if the response is not valid HTTP,
           rather than this just being irrelevant, it is the place that Curl
           puts the server's non-HTTP response.  That can be useful for error
           reporting.
        */
};



static void
addHeader(xmlrpc_env *         const envP,
          struct curl_slist ** const headerListP,
          const char *         const headerText) {

    struct curl_slist * newHeaderList;
    newHeaderList = curl_slist_append(*headerListP, headerText);
    if (newHeaderList == NULL)
        xmlrpc_faultf(envP,
                      "Could not add header '%s'.  "
                      "curl_slist_append() failed.", headerText);
    else
        *headerListP = newHeaderList;
}



static void
addContentTypeHeader(xmlrpc_env *         const envP,
                     struct curl_slist ** const headerListP) {

    addHeader(envP, headerListP, "Content-Type: text/xml");
}



static const char *
xmlrpcUserAgentPart(bool const reportIt) {

    const char * retval;

    if (reportIt) {
        curl_version_info_data * const curlInfoP =
            curl_version_info(CURLVERSION_NOW);
        char curlVersion[32];

        XMLRPC_SNPRINTF(curlVersion, sizeof(curlVersion), "%u.%u.%u",
                        (curlInfoP->version_num >> 16) & 0xff,
                        (curlInfoP->version_num >>  8) & 0xff,
                        (curlInfoP->version_num >>  0) & 0xff
            );

        xmlrpc_asprintf(&retval,
                        "Xmlrpc-c/%s Curl/%s",
                        XMLRPC_C_VERSION, curlVersion);
    } else
        xmlrpc_asprintf(&retval, "%s", "");

    return retval;
}



static void
addUserAgentHeader(xmlrpc_env *         const envP,
                   struct curl_slist ** const headerListP,
                   bool                 const reportXmlrpc,
                   const char *         const userAgent) {
/*----------------------------------------------------------------------------
   Add a User-Agent HTTP header to the Curl header list *headerListP,
   if appropriate.

   'reportXmlrpc' means we want to tell the client what XML-RPC agent
   is being used -- Xmlrpc-c and layers below.

   'userAgent' is a string describing the layers above Xmlrpc-c.  We
   assume it is in the proper format to be included in a User-Agent
   header.  (We should probably fix that some day -- take ownership
   of that format).
-----------------------------------------------------------------------------*/
    if (reportXmlrpc || userAgent) {
        /* Add the header */

        /* Note: Curl has a CURLOPT_USERAGENT option that does some of this
           work.  We prefer to be totally in control, though, so we build
           the header explicitly.
        */

        const char * const xmlrpcPart = xmlrpcUserAgentPart(reportXmlrpc);

        if (xmlrpc_strnomem(xmlrpcPart))
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "User-Agent header");
        else {
            const char * const userPart = userAgent ? userAgent : "";
            const char * const space = userAgent && reportXmlrpc ? " " : "";

            const char * userAgentHeader;

            xmlrpc_asprintf(&userAgentHeader,
                            "User-Agent: %s%s%s",
                            userPart, space, xmlrpcPart);

            if (xmlrpc_strnomem(userAgentHeader))
                xmlrpc_faultf(envP, "Couldn't allocate memory for "
                              "User-Agent header");
            else {
                addHeader(envP, headerListP, userAgentHeader);

                xmlrpc_strfree(userAgentHeader);
            }
            xmlrpc_strfree(xmlrpcPart);
        }
    }
}



static void
addAuthorizationHeader(xmlrpc_env *         const envP,
                       struct curl_slist ** const headerListP,
                       const char *         const hdrValue) {

    const char * authorizationHeader;

    xmlrpc_asprintf(&authorizationHeader, "Authorization: %s", hdrValue);

    if (xmlrpc_strnomem(authorizationHeader))
        xmlrpc_faultf(envP, "Couldn't allocate memory for "
                      "Authorization header");
    else {
        addHeader(envP, headerListP, authorizationHeader);

        xmlrpc_strfree(authorizationHeader);
    }
}



/*
  In HTTP 1.1, the client can send the header "Expect: 100-continue", which
  tells the server that the client isn't going to send the body until the
  server tells it to by sending a "continue" response (HTTP response code 100).
  The server is obligated to send that response.

  However, many servers are broken and don't send the Continue response.

  Early libcurl did not send the Expect: header, thus worked fine with such
  broken servers.  But as of ca. 2007, libcurl sends the Expect:, and waits
  for the response, when the body is large.  It gives up after 3 seconds and
  sends the body anyway.

  To accomodate the broken servers and for backward compatibility, we always
  force libcurl not to send the Expect and consequently not to wait for the
  response, using the hackish (but according to libcurl design) method of
  including an entry in our explicit header list that is an Expect: header
  with an empty argument.  This causes libcurl not to send any Expect: header.
  This is since 1.19; we may find there are also servers and/or libcurl levels
  that can't work with that.

  We may find a case where the Expect/Continue protocol is desirable.  If we
  do, we should add a transport option to request the function and let libcurl
  do its thing when the user requests it.

  The purpose of Expect/Continue is to save the client the trouble of
  generating and/or sending the body when the server is just going to reject
  the transaction based on the headers -- like maybe because the body is
  too big.
*/


static void
addExpectHeader(xmlrpc_env *         const envP,
                struct curl_slist ** const headerListP) {

    addHeader(envP, headerListP, "Expect:");
        /* Don't send Expect header.  See explanation above. */
}



static void
createCurlHeaderList(xmlrpc_env *               const envP,
                     const char *               const authHdrValue,
                     bool                       const dontAdvertise,
                     const char *               const userAgent,
                     struct curl_slist **       const headerListP) {

    struct curl_slist * headerList;

    headerList = NULL;  /* initial value - empty list */

    addContentTypeHeader(envP, &headerList);
    if (!envP->fault_occurred) {
        addUserAgentHeader(envP, &headerList, !dontAdvertise, userAgent);
        if (!envP->fault_occurred) {
            if (authHdrValue)
                addAuthorizationHeader(envP, &headerList, authHdrValue);
        }
        if (!envP->fault_occurred)
            addExpectHeader(envP, &headerList);
    }
    if (envP->fault_occurred)
        curl_slist_free_all(headerList);

    *headerListP = headerList;
}



static size_t
collect(void *  const ptr,
        size_t  const size,
        size_t  const nmemb,
        void  * const streamP) {
/*----------------------------------------------------------------------------
   This is a Curl write function.  Curl calls this to deliver the
   HTTP response body to the Curl client.

   But as a design quirk, Curl also calls this when there is no HTTP body
   because the response from the server is not valid HTTP.  In that case,
   Curl calls this to deliver the raw contents of the response.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * const responseXmlP = streamP;
    char * const buffer = ptr;
    size_t const length = nmemb * size;

    size_t retval;
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_mem_block_append(&env, responseXmlP, buffer, length);
    if (env.fault_occurred)
        retval = (size_t)-1;
    else {
        retval = length;
            /* Really.  Analogy to fread() would require this to be 'nmemb',
               but Curl does expect the byte count.
            */
    }

    return retval;
}



static int
curlProgress(void * const contextP,
             double const dltotal,
             double const dlnow,
             double const ultotal,
             double const ulnow) {
/*----------------------------------------------------------------------------
   This is a Curl "progress function."  It's something various Curl functions
   call every so often, including whenever something gets interrupted by the
   process receiving, and catching, a signal.  There are two purposes of a
   Curl progress function: 1) lets us log the progress of a long-running
   transaction such as a big download, e.g. by displaying a progress bar
   somewhere.  2) allows us to tell the Curl function, via our return code,
   that calls it that we don't want to wait anymore for the operation to
   complete.

   In Curl versions before March 2007, we get called once per second and
   signals have no effect.  In current Curl, we usually get called immediately
   after a signal gets caught while Curl is waiting to receive a response from
   the server.  But Curl doesn't properly synchronize with signals, so it may
   miss one and then we don't get called until the next scheduled
   one-per-second call.

   All we do is pass the call through to the curlTransaction's progress
   function (the one that the creator of the curlTransaction registered).

   This function is not as important as it once was for interrupting purposes.
   This module used to use curl_easy_perform(), which can be interrupted only
   via this progress function.  But because of the above-mentioned failure of
   Curl to properly synchronize signals (and Bryan's failure to get Curl
   developers to accept code to fix it), we now use the Curl "multi" facility
   instead and do our own pselect().  But this function still normally gets
   called by curl_multi_perform(), which the transport tries to call even when
   the user has requested interruption, because we don't trust our ability to
   abort a running Curl transaction.  curl_multi_perform() reliably winds up a
   Curl transaction when this function tells it to.
-----------------------------------------------------------------------------*/
    curlTransaction * const curlTransactionP = contextP;

    bool abort;

    /* We require anyone setting us up as the Curl progress function to
       supply a progress function:
    */
    assert(curlTransactionP);
    assert(curlTransactionP->progress);

    curlTransactionP->progress(curlTransactionP->userContextP,
                               dltotal, dlnow, ultotal, ulnow,
                               &abort);

    return abort;
}



static int
curlXferinfo(void *     const contextP,
             curl_off_t const dltotal,
             curl_off_t const dlnow,
             curl_off_t const ultotal,
             curl_off_t const ulnow) {
/*----------------------------------------------------------------------------
   This is a curl "transfer information" callback function, to be called
   back from certain libcurl data transfer calls.

   In old Curl, one would set up Curl to call a function of type
   'curl_progress_callback', with arguments of type 'double'.  In later
   versions of Curl, that facility was replaced with one in which you set
   up Curl to call a function of type 'curl_xferinfo_callback', with
   arguments of type 'curl_off_t', instead.  Xmlrpc-c was written for the
   older version and its interface to its own user is patterned after
   it (so it uses 'double' arguments).

   This function is an adapter for use with newer Curl libraries.  We
   just make the call that an older Curl library would have made itself.
-----------------------------------------------------------------------------*/

    return curlProgress(contextP, dltotal, dlnow, ultotal, ulnow);
}



static void
setupAuth(xmlrpc_env *               const envP ATTR_UNUSED,
          CURL *                     const curlSessionP,
          const xmlrpc_server_info * const serverInfoP,
          const char **              const authHdrValueP) {
/*----------------------------------------------------------------------------
   Set the options in the Curl session 'curlSessionP' to set up the HTTP
   authentication described by *serverInfoP.

   But we have an odd special function for backward compatibility, because
   this code dates to a time when libcurl did not have the ability to
   handle authentication, but we provided such function nonetheless by
   building our own Authorization: header.  But we did this only for
   HTTP basic authentication.

   So the special function is this: if libcurl is too old to have
   authorization options and *serverInfoP allows basic authentication, return
   as *authHdrValueP an appropriate parameter for the Authorization: Basic:
   HTTP header.  Otherwise, return *authHdrValueP == NULL.
-----------------------------------------------------------------------------*/
    CURLcode rc;

    /* We don't worry if libcurl is too old for specific kinds of
       authentication; they're defined only as _allowed_ authentication
       methods, for when client and server are capable of using it, and unlike
       with basic authentication, we have no historical commitment to consider
       an old libcurl as capable of doing these.

       Note that curl_easy_setopt(CURLOPT_HTTPAUTH) succeeds even if there are
       flags in it argument that weren't defined when it was written.
    */

    if (serverInfoP->userNamePw)
        curl_easy_setopt(curlSessionP, CURLOPT_USERPWD,
                         serverInfoP->userNamePw);

    rc = curl_easy_setopt(
        curlSessionP, CURLOPT_HTTPAUTH,
        (serverInfoP->allowedAuth.basic        ? CURLAUTH_BASIC        : 0) |
        (serverInfoP->allowedAuth.digest       ? CURLAUTH_DIGEST       : 0) |
        (serverInfoP->allowedAuth.gssnegotiate ? CURLAUTH_GSSNEGOTIATE : 0) |
        (serverInfoP->allowedAuth.ntlm         ? CURLAUTH_NTLM         : 0));

    if (rc != CURLE_OK) {
        /* Curl is too old to do authentication, so we do it ourselves
           with an explicit header if we have to.
        */
        if (serverInfoP->allowedAuth.basic) {
            *authHdrValueP = strdup(serverInfoP->basicAuthHdrValue);
            if (*authHdrValueP == NULL)
                xmlrpc_faultf(envP, "Unable to allocate memory for basic "
                              "authentication header");
        } else
            *authHdrValueP = NULL;
    } else
        *authHdrValueP = NULL;
}



static void
setCurlTimeout(CURL *       const curlSessionP ATTR_UNUSED,
               unsigned int const timeoutMs ATTR_UNUSED) {

#if HAVE_CURL_NOSIGNAL
    unsigned int const timeoutSec = (timeoutMs + 999)/1000;

    assert((long)timeoutSec == (int)timeoutSec);
        /* Calling requirement */
    curl_easy_setopt(curlSessionP, CURLOPT_TIMEOUT, (long)timeoutSec);

    /* Diagnostic note: You can use the --max-time option on the 'curl'
       program to exercise CURLOPT_TIMEOUT.
    */
#else
    /* Caller should not have called us */
    abort();
#endif
}



static void
setCurlConnectTimeout(CURL *       const curlSessionP ATTR_UNUSED,
                      unsigned int const timeoutMs ATTR_UNUSED) {

#if HAVE_CURL_NOSIGNAL
    unsigned int const timeoutSec = (timeoutMs + 999)/1000;

    assert((long)timeoutSec == (int)timeoutSec);
        /* Calling requirement */
    curl_easy_setopt(curlSessionP, CURLOPT_CONNECTTIMEOUT, (long)timeoutSec);

    /* Diagnostic note: You can use the --connect-timeout option on the 'curl'
       program to exercise CURLOPT_CONNECTTIMEOUT.
    */
#else
    /* Caller should not have called us */
    abort();
#endif
}



static void
assertConstantsMatch(void) {
/*----------------------------------------------------------------------------
   There are some constants that we define as part of the Xmlrpc-c
   interface that are identical to constants in the Curl interface to
   make curl option setting work.  This function asserts such
   formally.
-----------------------------------------------------------------------------*/
#define assertMatch(xmlrpc_name, curl_name) \
  assert((unsigned)xmlrpc_name == (unsigned)curl_name)

    assertMatch(XMLRPC_SSLVERSION_DEFAULT , CURL_SSLVERSION_DEFAULT);
    assertMatch(XMLRPC_SSLVERSION_TLSv1   , CURL_SSLVERSION_TLSv1  );
    assertMatch(XMLRPC_SSLVERSION_SSLv2   , CURL_SSLVERSION_SSLv2  );
    assertMatch(XMLRPC_SSLVERSION_SSLv3   , CURL_SSLVERSION_SSLv3  );

    assertMatch(XMLRPC_HTTPAUTH_BASIC        , CURLAUTH_BASIC       );
    assertMatch(XMLRPC_HTTPAUTH_DIGEST       , CURLAUTH_DIGEST      );
    assertMatch(XMLRPC_HTTPAUTH_GSSNEGOTIATE , CURLAUTH_GSSNEGOTIATE);
    assertMatch(XMLRPC_HTTPAUTH_NTLM         , CURLAUTH_NTLM        );

    assertMatch(XMLRPC_HTTPPROXY_HTTP   , CURLPROXY_HTTP   );
    assertMatch(XMLRPC_HTTPPROXY_SOCKS5 , CURLPROXY_SOCKS5 );

#undef assertMatch
}



/* About Curl and GSSAPI credential delegation:

   Up through Curl 7.21.6, libcurl always delegates GSSAPI credentials, which
   means it gives the client's secrets to the server so the server can operate
   on the client's behalf.  In mid-2011, this was noticed to be a major
   security exposure, because the server is not necessarily trustworthy.
   One is supposed to delegate one's credentials only to a server one trusts.
   So in 7.21.7, Curl never delegates GSSAPI credentials.

   But that causes problems for clients that _do_ trust their server, which
   had always relied upon Curl's delegation.

   So starting in 7.22.0, Curl gives the user the choice.  The default is no
   delegation, but the Curl user can set the CURLOPT_GSSAPI_DELEGATION flag to
   order delegation.

   Complicating matters is that some people made local variations of Curl
   during the transition phase, so the version number alone isn't
   determinative, so we rely on it only where we have to.

   So Xmlrpc-c gives the same choice to its own user, via its
   'gssapi_delegation' Curl transport option.

   Current Xmlrpc-c can be linked with, and compiled with, any version of
   Curl, so it has to carefully consider all the possibilities.
*/



static bool
curlAlwaysDelegatesGssapi(void) {
/*----------------------------------------------------------------------------
   The Curl library we're using always delegates GSSAPI credentials
   (we don't have a choice).

   This works with Curl as distributed by the Curl project, but there are
   other versions of Curl for which it doesn't -- those versions report
   older version numbers but in fact don't always delegate.  Some never
   delegate, and some give the user the option.
-----------------------------------------------------------------------------*/
    curl_version_info_data * const curlInfoP =
        curl_version_info(CURLVERSION_NOW);

    return (curlInfoP->version_num <= 0x071506);  /* 7.21.6 */
}



static void
requestGssapiDelegation(CURL * const curlSessionP ATTR_UNUSED,
                        bool * const gotItP) {
/*----------------------------------------------------------------------------
   Set up the Curl session *curlSessionP to delegate its GSSAPI credentials to
   the server.

   Return *gotitP is true iff we succeed.  We fail when the version of libcurl
   for which we are compiled or to which we are linked is not capable of such
   delegation.
-----------------------------------------------------------------------------*/
#if HAVE_CURL_GSSAPI_DELEGATION
    int rc;

    rc = curl_easy_setopt(curlSessionP, CURLOPT_GSSAPI_DELEGATION,
                          CURLGSSAPI_DELEGATION_FLAG);

    if (rc == CURLE_OK)
        *gotItP = true;
    else {
        /* The only way curl_easy_setopt() could have failed is that we
           are running with an old libcurl from before
           CURLOPT_GSSAPI_DELEGATION was invented.
        */
        if (curlAlwaysDelegatesGssapi()) {
            /* No need to request delegation; we got it anyway */
            *gotItP = true;
        } else
            *gotItP = false;
    }
#else
    if (curlAlwaysDelegatesGssapi())
        *gotItP = true;
    else {
        /* The library may be able to do credential delegation on request, but
           we have no way to request it; the Curl for which we are compiled is
           too old.
        */
        *gotItP = false;
    }
#endif
}



static void
setupKeepalive(const struct curlSetup * const curlSetupP,
               CURL *                   const curlSessionP,
               xmlrpc_env *             const envP) {
/*----------------------------------------------------------------------------
   We fail if our libcurl is too old to have the keepalive capability.

   We really ought to have something the user can call to find out whether the
   transport can do keepalive so he can know not to attempt to set up
   keepalive and thereby avoid this failure, but today there is no mechanism
   in place for the user to get any information specific to a Curl transport.
-----------------------------------------------------------------------------*/
#if HAVE_CURL_KEEPALIVE
    if (curlSetupP->tcpKeepalive) {
        curl_easy_setopt(curlSessionP, CURLOPT_TCP_KEEPALIVE, 1);
    }
    if (curlSetupP->tcpKeepidle) {
        curl_easy_setopt(curlSessionP,
                         CURLOPT_TCP_KEEPIDLE, curlSetupP->tcpKeepidle);
    }
    if (curlSetupP->tcpKeepintvl) {
        curl_easy_setopt(curlSessionP,
                         CURLOPT_TCP_KEEPINTVL, curlSetupP->tcpKeepintvl);
    }
    envP->fault_occurred = false;
#else
    if (curlSetupP->tcpKeepalive ||
        curlSetupP->tcpKeepidle || curlSetupP->tcpKeepintvl) {
        xmlrpc_faultf(envP,
                      "Attempt to control TCP keepalive with a Curl "
                      "library that is too old to have that capability");
    }
#endif
}



static void
setupProgressFunction(curlTransaction * const transP) {

    CURL * const curlSessionP = transP->curlSessionP;

    if (transP->progress) {
#if defined(HAVE_CURL_XFERINFOFUNCTION)
        if (false) {
             /* Defeat unused function compiler warning */
            curlXferinfo(NULL, 0, 0, 0, 0);
        }
        curl_progress_callback const progFnOpt = &curlProgress;
        curl_easy_setopt(curlSessionP, CURLOPT_XFERINFOFUNCTION, progFnOpt);
        curl_easy_setopt(curlSessionP, CURLOPT_XFERINFODATA, transP);
#else
        curl_xferinfo_callback const progFnOpt = &curlXferinfo;
        curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSFUNCTION, progFnOpt);
        curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSDATA, transP);
#endif
        curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 0);
    } else
        curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 1);
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void
setupOldOpenSslOpts(CURL *                   const curlSessionP,
                    const struct curlSetup * const curlSetupP) {
/*----------------------------------------------------------------------------
   Set Curl's RANDOM_FILE and EGDSOCKET options.  These are meaningful only if
   we're using the OpenSSL library, before version 1.1, and current Curl
   libraries don't even work with those old libraries, so they just ignore
   these options.  Since Curl 7.84, Curl documentation deprecates these
   options.
-----------------------------------------------------------------------------*/
#if defined(CURL_DOES_OLD_OPENSSL)
    if (curlSetupP->randomFile)
        curl_easy_setopt(curlSessionP, CURLOPT_RANDOM_FILE,
                         curlSetupP->randomFile);
    if (curlSetupP->egdSocket)
        curl_easy_setopt(curlSessionP, CURLOPT_EGDSOCKET,
                         curlSetupP->egdSocket);
#endif
}
#pragma GCC diagnostic pop



static void
setupCurlSession(xmlrpc_env *               const envP,
                 curlTransaction *          const transP,
                 const xmlrpc_server_info * const serverInfoP,
                 bool                       const dontAdvertise,
                 const char *               const userAgent,
                 const char *               const unixSocketPath,
                 const struct curlSetup *   const curlSetupP) {
/*----------------------------------------------------------------------------
   Set up the Curl session for the transaction *transP so that
   a subsequent curl_easy_perform() would perform said transaction.

   *serverInfoP tells what sort of authentication to set up etc.  This is an
   embarassment, as the xmlrpc_server_info type is part of the Xmlrpc-c
   interface, whereas this module is supposed to be for generic TCP via Curl.
   Some day, we need to replace this with a type (probably identical) not tied
   to Xmlrpc-c.

   'unixSocketPath' is the path name of the unix socket to use for the session
   (which determines the server, along with the URL -- this is a kludgy
   variation on HTTP).  NULL means no unix socket is involved.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = transP->curlSessionP;

    assertConstantsMatch();

    /* A Curl session is serial -- it processes zero or one transaction
       at a time.  We use the "private" attribute of the Curl session to
       indicate which transaction it is currently processing.  This is
       important when the transaction finishes, because libcurl will just
       tell us that something finished on a particular session, not that
       a particular transaction finished.
    */

    /* It is our policy to do a libcurl call only where necessary, I.e.  not
       to set what is the default anyhow.  The reduction in calls may save
       some time, but mostly, it will save us encountering rare bugs or
       suffering from backward incompatibilities in future libcurl.  I.e. we
       don't exercise any more of libcurl than we have to.
    */

    curl_easy_setopt(curlSessionP, CURLOPT_NOSIGNAL, 1);
        /* See discussion of CURLOPT_NOSIGNAL above */

    curl_easy_setopt(curlSessionP, CURLOPT_PRIVATE, transP);

    curl_easy_setopt(curlSessionP, CURLOPT_POST, 1);
    curl_easy_setopt(curlSessionP, CURLOPT_URL, transP->serverUrl);
    if (unixSocketPath) {
        curl_easy_setopt(curlSessionP,
                         CURLOPT_UNIX_SOCKET_PATH, unixSocketPath);
    }

    XMLRPC_MEMBLOCK_APPEND(char, envP, transP->postDataP, "\0", 1);
    if (!envP->fault_occurred) {
        curl_easy_setopt(curlSessionP, CURLOPT_POSTFIELDS,
                         XMLRPC_MEMBLOCK_CONTENTS(char, transP->postDataP));
        curl_easy_setopt(curlSessionP, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(curlSessionP, CURLOPT_FILE, transP->responseDataP);
            /* CURLOPT_FILE is the older name for CURLOPT_WRITEDATA */
        curl_easy_setopt(curlSessionP, CURLOPT_HEADER, 0);
        curl_easy_setopt(curlSessionP, CURLOPT_ERRORBUFFER, transP->curlError);

        setupProgressFunction(transP);

        curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYPEER,
                         curlSetupP->sslVerifyPeer);
        curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYHOST,
                         curlSetupP->sslVerifyHost ? 2 : 0);

        if (curlSetupP->networkInterface)
            curl_easy_setopt(curlSessionP, CURLOPT_INTERFACE,
                             curlSetupP->networkInterface);
        if (curlSetupP->referer)
            curl_easy_setopt(curlSessionP, CURLOPT_REFERER,
                             curlSetupP->referer);
        if (curlSetupP->sslCert)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERT,
                             curlSetupP->sslCert);
        if (curlSetupP->sslCertType)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERTTYPE,
                             curlSetupP->sslCertType);
        if (curlSetupP->sslCertPasswd)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERTPASSWD,
                             curlSetupP->sslCertPasswd);
        if (curlSetupP->sslKey)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEY,
                             curlSetupP->sslKey);
        if (curlSetupP->sslKeyType)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEYTYPE,
                             curlSetupP->sslKeyType);
        if (curlSetupP->sslKeyPasswd)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEYPASSWD,
                             curlSetupP->sslKeyPasswd);
        if (curlSetupP->sslEngine)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLENGINE,
                             curlSetupP->sslEngine);
        if (curlSetupP->sslEngineDefault)
            /* 3rd argument seems to be required by some Curl */
            curl_easy_setopt(curlSessionP, CURLOPT_SSLENGINE_DEFAULT, 1l);
        if (curlSetupP->sslVersion != XMLRPC_SSLVERSION_DEFAULT)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLVERSION,
                             curlSetupP->sslVersion);
        if (curlSetupP->caInfo)
            curl_easy_setopt(curlSessionP, CURLOPT_CAINFO,
                             curlSetupP->caInfo);
        if (curlSetupP->caPath)
            curl_easy_setopt(curlSessionP, CURLOPT_CAPATH,
                             curlSetupP->caPath);
        setupOldOpenSslOpts(curlSessionP, curlSetupP);

        if (curlSetupP->sslCipherList)
            curl_easy_setopt(curlSessionP, CURLOPT_SSL_CIPHER_LIST,
                             curlSetupP->sslCipherList);

        if (curlSetupP->proxy)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXY, curlSetupP->proxy);
        if (curlSetupP->proxyAuth != CURLAUTH_BASIC)
            /* Note that the Xmlrpc-c default and the Curl default are
               different.  Xmlrpc-c is none, while Curl is basic.  One reason
               for this is that it makes our extensible parameter list scheme,
               wherein zero always means default, easier.
            */
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYAUTH,
                             curlSetupP->proxyAuth);
        if (curlSetupP->proxyPort)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYPORT,
                             curlSetupP->proxyPort);
        if (curlSetupP->proxyUserPwd)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYUSERPWD,
                             curlSetupP->proxyUserPwd);
        if (curlSetupP->proxyType)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYTYPE,
                             curlSetupP->proxyType);

        if (curlSetupP->verbose)
            curl_easy_setopt(curlSessionP, CURLOPT_VERBOSE, 1l);

        if (curlSetupP->timeout)
            setCurlTimeout(curlSessionP, curlSetupP->timeout);

        if (curlSetupP->connectTimeout)
            setCurlConnectTimeout(curlSessionP, curlSetupP->connectTimeout);
        else
            curl_easy_setopt(curlSessionP, CURLOPT_CONNECTTIMEOUT,
                             LONG_MAX/1000);
                /* Some documentation says 0 means indefinite and other says 0
                   means 5 minutes.  The latter appears to be true.  Some
                   libcurl (e.g. 7.12.2, but not 7.21.0) has an apparent bug
                   in which anything larger than LONG_MAX/1000 results in an
                   instantaneous timeout.
                */
        if (curlSetupP->gssapiDelegation) {
            bool gotIt;
            requestGssapiDelegation(curlSessionP, &gotIt);

            if (!gotIt)
                xmlrpc_faultf(envP, "Cannot honor 'gssapi_delegation' "
                              "Curl transport option.  "
                              "This version of libcurl is not "
                              "capable of delegating GSSAPI credentials");
        }

        if (!envP->fault_occurred) {
            const char * authHdrValue;
                /* NULL means we don't have to construct an explicit
                   Authorization: header.  non-null means we have to
                   construct one with this as its value.
                */

            setupAuth(envP, curlSessionP, serverInfoP, &authHdrValue);
            if (!envP->fault_occurred) {
                struct curl_slist * headerList;
                createCurlHeaderList(envP, authHdrValue,
                                     dontAdvertise, userAgent,
                                     &headerList);
                if (!envP->fault_occurred) {
                    curl_easy_setopt(
                        curlSessionP, CURLOPT_HTTPHEADER, headerList);
                    transP->headerList = headerList;
                }
                if (authHdrValue)
                    xmlrpc_strfree(authHdrValue);
            }
        }
    }

    if (!envP->fault_occurred)
        setupKeepalive(curlSetupP, curlSessionP, envP);

}



void
curlTransaction_create(xmlrpc_env *               const envP,
                       CURL *                     const curlSessionP,
                       const xmlrpc_server_info * const serverP,
                       xmlrpc_mem_block *         const callXmlP,
                       xmlrpc_mem_block *         const responseXmlP,
                       bool                       const dontAdvertise,
                       const char *               const userAgent,
                       const struct curlSetup *   const curlSetupStuffP,
                       void *                     const userContextP,
                       curlt_finishFn *           const finish,
                       curlt_progressFn *         const progress,
                       curlTransaction **         const curlTransactionPP) {

    curlTransaction * curlTransactionP;

    MALLOCVAR(curlTransactionP);
    if (curlTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create Curl transaction.");
    else {
        curlTransactionP->finish       = finish;
        curlTransactionP->curlSessionP = curlSessionP;
        curlTransactionP->userContextP = userContextP;
        curlTransactionP->progress     = progress;

        /* Curl sometimes neglects to set 'curlError', so we set it here to
           a value that means no explanation available.
        */
        curlTransactionP->curlError[0] = '\0';

        curlTransactionP->serverUrl = xmlrpc_strdupsol(serverP->serverUrl);

        curlTransactionP->postDataP     = callXmlP;
        curlTransactionP->responseDataP = responseXmlP;

        setupCurlSession(envP, curlTransactionP,
                         serverP, dontAdvertise, userAgent,
                         serverP->unixSocketPath,
                         curlSetupStuffP);

        if (envP->fault_occurred) {
            xmlrpc_strfree(curlTransactionP->serverUrl);
            free(curlTransactionP);
        }
    }
    *curlTransactionPP = curlTransactionP;
}



void
curlTransaction_destroy(curlTransaction * const curlTransactionP) {

    curl_slist_free_all(curlTransactionP->headerList);
    xmlrpc_strfree(curlTransactionP->serverUrl);

    free(curlTransactionP);
}



static void
interpretCurlEasyError(const char ** const descriptionP,
                       CURLcode      const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_easy_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLcode) %d", code);
#endif
}



/* CURL quirks:

   We have seen Curl report that the transaction completed OK (CURLE_OK) when
   the server sent back garbage instead of an HTTP response (because it wasn't
   an HTTP server).  In that case Curl reports zero in place of the response
   code.  It's strange that Curl doesn't report that protocol violation at a
   higher level (perhaps with more detail), but apparently it does not, so we
   go by the HTTP_CODE value.  Note that if the server closes the connection
   without responding at all, Curl calls the transaction failed with an "empty
   reply from server" error code.

   It appears to be the case that when the server sends non-HTTP garbage, Curl
   reports it as the HTTP response body.  E.g. we had an inetd server respond
   with a "library not found" error message because the server connected
   Standard Error to the socket.  The 'curl' program typed out the error
   message, naked, and exited with exit status zero.  We exploit this
   discovery to give better error reporting to our user.

   We saw this with Curl 7.16.1.
*/



static const char *
formatDataReceived(curlTransaction * const curlTransactionP) {

    const char * retval;

    if (XMLRPC_MEMBLOCK_SIZE(char, curlTransactionP->responseDataP) == 0)
        retval = xmlrpc_strdupsol("");
    else {
        xmlrpc_asprintf(
            &retval,
            "Raw data from server: '%s'\n",
            XMLRPC_MEMBLOCK_CONTENTS(char, curlTransactionP->responseDataP));
    }
    return retval;
}



void
curlTransaction_getError(curlTransaction * const curlTransactionP,
                         xmlrpc_env *      const envP) {
/*----------------------------------------------------------------------------
   Determine whether the transaction *curlTransactionP was successful in HTTP
   terms.  Assume the transaction did complete.  Return as *envP an indication
   of whether the transaction failed and if so, how.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    if (curlTransactionP->result != CURLE_OK) {
        /* We've seen Curl just return a null string for an explanation
           (e.g. when TCP connect() fails because IP address doesn't exist).
        */
        const char * explanation;

        if (strlen(curlTransactionP->curlError) == 0)
            interpretCurlEasyError(&explanation, curlTransactionP->result);
        else
            xmlrpc_asprintf(&explanation, "%s", curlTransactionP->curlError);

        xmlrpc_env_set_fault_formatted(
            &env, XMLRPC_NETWORK_ERROR, "libcurl failed even to execute the "
            "HTTP transaction, explaining:  %s", explanation);

        xmlrpc_strfree(explanation);
    } else {
        CURLcode res;
        long http_result;

        res = curl_easy_getinfo(curlTransactionP->curlSessionP,
                                CURLINFO_HTTP_CODE, &http_result);
            /* CURLINFO_HTTP_CODE is the old name for CURLINFO_RESPONSE_CODE */

        if (res != CURLE_OK)
            xmlrpc_env_set_fault_formatted(
                &env, XMLRPC_INTERNAL_ERROR,
                "Curl performed the HTTP transaction, but was "
                "unable to say what the HTTP result code was.  "
                "curl_easy_getinfo(CURLINFO_HTTP_CODE) says: %s",
                curlTransactionP->curlError);
        else {
            if (http_result == 0) {
                /* See above for what this case means */
                const char * const dataReceived =
                    formatDataReceived(curlTransactionP);

                xmlrpc_env_set_fault_formatted(
                    &env, XMLRPC_NETWORK_ERROR,
                    "Server is not an XML-RPC server.  Its response to our "
                    "call is not valid HTTP.  Or it's valid HTTP with a "
                    "response code of zero.  %s", dataReceived);
                xmlrpc_strfree(dataReceived);
            } else if (http_result != 200)
                xmlrpc_env_set_fault_formatted(
                    &env, XMLRPC_NETWORK_ERROR,
                    "HTTP response code is %ld, not 200",
                    http_result);
        }
    }

    if (env.fault_occurred) {
        xmlrpc_env_set_fault_formatted(
            envP,
            env.fault_code,
            "HTTP POST to URL '%s' failed.  %s",
            curlTransactionP->serverUrl, env.fault_string);
    }
    xmlrpc_env_clean(&env);
}



void
curlTransaction_finish(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP,
                       CURLcode          const result) {

    curlTransactionP->result = result;

    if (curlTransactionP->finish)
        curlTransactionP->finish(envP, curlTransactionP->userContextP);
}



CURL *
curlTransaction_curlSession(curlTransaction * const curlTransactionP) {

    return curlTransactionP->curlSessionP;

}

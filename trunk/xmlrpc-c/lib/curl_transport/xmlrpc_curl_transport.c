/*=============================================================================
                           xmlrpc_curl_transport
===============================================================================
   Curl-based client transport for Xmlrpc-c

   By Bryan Henderson 04.12.10.

   Contributed to the public domain by its author.
=============================================================================*/

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "bool.h"
#include "mallocvar.h"
#include "linklist.h"
#include "sstring.h"
#include "casprintf.h"
#include "pthreadx.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
#include "version.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#if defined (WIN32) && defined(_DEBUG)
#  include <crtdbg.h>
#  define new DEBUG_NEW
#  define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#  undef THIS_FILE
   static char THIS_FILE[] = __FILE__;
#endif /*WIN32 && _DEBUG*/



struct curlSetup {

    /* This is all client transport properties that are implemented as
       simple Curl session properties (i.e. the transport basically just
       passed them through to Curl without looking at them).

       People occasionally want to replace all this with something where
       the Xmlrpc-c user simply does the curl_easy_setopt() call and this
       code need not know about all these options.  Unfortunately, that's
       a significant modularity violation.  Either the Xmlrpc-c user
       controls the Curl object or he doesn't.  If he does, then he
       shouldn't use libxmlrpc_client -- he should just copy some of this
       code into his own program.  If he doesn't, then he should never see
       the Curl library.

       Speaking of modularity: the only reason this is a separate struct
       is to make the code easier to manage.  Ideally, the fact that these
       particular properties of the transport are implemented by simple
       Curl session setup would be known only at the lowest level code
       that does that setup.
    */

    const char * networkInterface;
        /* This identifies the network interface on the local side to
           use for the session.  It is an ASCIIZ string in the form
           that the Curl recognizes for setting its CURLOPT_INTERFACE
           option (also the --interface option of the Curl program).
           E.g. "9.1.72.189" or "giraffe-data.com" or "eth0".  

           It isn't necessarily valid, but it does have a terminating NUL.

           NULL means we have no preference.
        */
    xmlrpc_bool sslVerifyPeer;
        /* In an SSL connection, we should authenticate the server's SSL
           certificate -- refuse to talk to him if it isn't authentic.
           This is equivalent to Curl's CURLOPT_SSL_VERIFY_PEER option.
        */
    xmlrpc_bool sslVerifyHost;
        /* In an SSL connection, we should verify that the server's
           certificate (independently of whether the certificate is
           authentic) indicates the host name that is in the URL we
           are using for the server.
        */

    const char * sslCert;
    const char * sslCertType;
    const char * sslCertPasswd;
    const char * sslKey;
    const char * sslKeyType;
    const char * sslKeyPasswd;
    const char * sslEngine;
    bool         sslEngineDefault;
    unsigned int sslVersion;
    const char * caInfo;
    const char * caPath;
    const char * randomFile;
    const char * egdSocket;
    const char * sslCipherList;
};



struct xmlrpc_client_transport {
    pthread_mutex_t listLock;
    struct list_head rpcList;
        /* List of all RPCs that exist for this transport.  An RPC exists
           from the time the user requests it until the time the user 
           acknowledges it is done.
        */
    CURL * syncCurlSessionP;
        /* Handle for a Curl library session object that we use for
           all synchronous RPCs.  An async RPC has one of its own,
           and consequently does not share things such as persistent
           connections and cookies with any other RPC.
        */
    
    const char * userAgent;
        /* Prefix for the User-Agent HTTP header, reflecting facilities
           outside of Xmlrpc-c.  The actual User-Agent header consists
           of this prefix plus information about Xmlrpc-c.  NULL means
           none.
        */
    struct curlSetup curlSetupStuff;
};

typedef struct {
    /* This is all stuff that really ought to be in a Curl object,
       but the Curl library is a little too simple for that.  So we
       build a layer on top of Curl, and define this "transaction," as
       an object subordinate to a Curl "session."
       */
    CURL * curlSessionP;
        /* Handle for the Curl session that hosts this transaction */
    char curlError[CURL_ERROR_SIZE];
        /* Error message from Curl */
    struct curl_slist * headerList;
        /* The HTTP headers for the transaction */
    const char * serverUrl;  /* malloc'ed - belongs to this object */
} curlTransaction;



typedef struct {
    struct list_head link;  /* link in transport's list of RPCs */
    CURL * curlSessionP;
        /* The Curl session we use for this transaction.  Note that only
           one RPC at a time can use a particular Curl session, so this
           had better not be a session that some other RPC is using
           simultaneously.
        */
    curlTransaction * curlTransactionP;
        /* The object which does the HTTP transaction, with no knowledge
           of XML-RPC or Xmlrpc-c.
        */
    xmlrpc_mem_block * responseXmlP;
    xmlrpc_bool threadExists;
    pthread_t thread;
    xmlrpc_transport_asynch_complete complete;
        /* Routine to call to complete the RPC after it is complete HTTP-wise.
           NULL if none.
        */
    struct xmlrpc_call_info * callInfoP;
        /* User's identifier for this RPC */
} rpc;



static size_t 
collect(void *  const ptr, 
        size_t  const size, 
        size_t  const nmemb,  
        FILE  * const stream) {
/*----------------------------------------------------------------------------
   This is a Curl output function.  Curl calls this to deliver the
   HTTP response body.  Curl thinks it's writing to a POSIX stream.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * const responseXmlP = (xmlrpc_mem_block *) stream;
    char * const buffer = ptr;
    size_t const length = nmemb * size;

    size_t retval;
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_mem_block_append(&env, responseXmlP, buffer, length);
    if (env.fault_occurred)
        retval = (size_t)-1;
    else
        /* Really?  Shouldn't it be like fread() and return 'nmemb'? */
        retval = length;
    
    return retval;
}



static void
initWindowsStuff(xmlrpc_env * const envP ATTR_UNUSED) {

#if defined (WIN32)
    /* This is CRITICAL so that cURL-Win32 works properly! */
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD(1, 1);
    
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "Winsock startup failed.  WSAStartup returned rc %d", err);
    else {
        if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
            /* Tell the user that we couldn't find a useable */ 
            /* winsock.dll. */ 
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Winsock reported that "
                "it does not implement the requested version 1.1.");
        }
        if (envP->fault_occurred)
            WSACleanup();
    }
#endif
}



static void
getXportParms(xmlrpc_env *  const envP ATTR_UNUSED,
              const struct xmlrpc_curl_xportparms * const curlXportParmsP,
              size_t        const parmSize,
              struct xmlrpc_client_transport * const transportP) {
/*----------------------------------------------------------------------------
   Get the parameters out of *curlXportParmsP and update *transportP
   to reflect them.

   *curlXportParmsP is a 'parmSize' bytes long prefix of
   struct xmlrpc_curl_xportparms.

   curlXportParmsP is something the user created.  It's designed to be
   friendly to the user, not to this program, and is encumbered by
   lots of backward compatibility constraints.  In particular, the
   user may have coded and/or compiled it at a time that struct
   xmlrpc_curl_xportparms was smaller than it is now!

   So that's why we don't simply attach a copy of *curlXportParmsP to
   *transportP.

   To the extent that *curlXportParmsP is too small to contain a parameter,
   we return the default value for that parameter.

   Special case:  curlXportParmsP == NULL means there is no input at all.
   In that case, we return default values for everything.
-----------------------------------------------------------------------------*/
    struct curlSetup * const curlSetupP = &transportP->curlSetupStuff;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(user_agent))
        transportP->userAgent = NULL;
    else if (curlXportParmsP->user_agent == NULL)
        transportP->userAgent = NULL;
    else
        transportP->userAgent = strdup(curlXportParmsP->user_agent);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(network_interface))
        curlSetupP->networkInterface = NULL;
    else if (curlXportParmsP->network_interface == NULL)
        curlSetupP->networkInterface = NULL;
    else
        curlSetupP->networkInterface =
            strdup(curlXportParmsP->network_interface);

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(no_ssl_verifypeer))
        curlSetupP->sslVerifyPeer = TRUE;
    else
        curlSetupP->sslVerifyPeer = !curlXportParmsP->no_ssl_verifypeer;
        
    if (!curlXportParmsP || 
        parmSize < XMLRPC_CXPSIZE(no_ssl_verifyhost))
        curlSetupP->sslVerifyHost = TRUE;
    else
        curlSetupP->sslVerifyHost = !curlXportParmsP->no_ssl_verifyhost;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(ssl_cert))
        curlSetupP->sslCert = NULL;
    else if (curlXportParmsP->ssl_cert == NULL)
        curlSetupP->sslCert = NULL;
    else
        curlSetupP->sslCert = strdup(curlXportParmsP->ssl_cert);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslcerttype))
        curlSetupP->sslCertType = NULL;
    else if (curlXportParmsP->sslcerttype == NULL)
        curlSetupP->sslCertType = NULL;
    else
        curlSetupP->sslCertType = strdup(curlXportParmsP->sslcerttype);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslcertpasswd))
        curlSetupP->sslCertPasswd = NULL;
    else if (curlXportParmsP->sslcertpasswd == NULL)
        curlSetupP->sslCertPasswd = NULL;
    else
        curlSetupP->sslCertPasswd = strdup(curlXportParmsP->sslcertpasswd);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkey))
        curlSetupP->sslKey = NULL;
    else if (curlXportParmsP->sslkey == NULL)
        curlSetupP->sslKey = NULL;
    else
        curlSetupP->sslKey = strdup(curlXportParmsP->sslkey);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkeytype))
        curlSetupP->sslKeyType = NULL;
    else if (curlXportParmsP->sslkeytype == NULL)
        curlSetupP->sslKeyType = NULL;
    else
        curlSetupP->sslKeyType = strdup(curlXportParmsP->sslkeytype);
    
        if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkeypasswd))
        curlSetupP->sslKeyPasswd = NULL;
    else if (curlXportParmsP->sslkeypasswd == NULL)
        curlSetupP->sslKeyPasswd = NULL;
    else
        curlSetupP->sslKeyPasswd = strdup(curlXportParmsP->sslkeypasswd);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslengine))
        curlSetupP->sslEngine = NULL;
    else if (curlXportParmsP->sslengine == NULL)
        curlSetupP->sslEngine = NULL;
    else
        curlSetupP->sslEngine = strdup(curlXportParmsP->sslengine);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslengine_default))
        curlSetupP->sslEngineDefault = FALSE;
    else
        curlSetupP->sslEngineDefault = !!curlXportParmsP->sslengine_default;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslversion))
        curlSetupP->sslVersion = 0;
    else if (curlXportParmsP->sslversion == 0)
        curlSetupP->sslVersion = 0;
    else
        curlSetupP->sslVersion = curlXportParmsP->sslversion;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(cainfo))
        curlSetupP->caInfo = NULL;
    else if (curlXportParmsP->cainfo == NULL)
        curlSetupP->caInfo = NULL;
    else
        curlSetupP->caInfo = strdup(curlXportParmsP->cainfo);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(capath))
        curlSetupP->caPath = NULL;
    else if (curlXportParmsP->capath == NULL)
        curlSetupP->caPath = NULL;
    else
        curlSetupP->caPath = strdup(curlXportParmsP->capath);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(randomfile))
        curlSetupP->randomFile = NULL;
    else if (curlXportParmsP->randomfile == NULL)
        curlSetupP->randomFile = NULL;
    else
        curlSetupP->randomFile = strdup(curlXportParmsP->randomfile);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(egdsocket))
        curlSetupP->egdSocket = NULL;
    else if (curlXportParmsP->egdsocket == NULL)
        curlSetupP->egdSocket = NULL;
    else
        curlSetupP->egdSocket = strdup(curlXportParmsP->egdsocket);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(ssl_cipher_list))
        curlSetupP->sslCipherList = NULL;
    else if (curlXportParmsP->ssl_cipher_list == NULL)
        curlSetupP->sslCipherList = NULL;
    else
        curlSetupP->sslCipherList = strdup(curlXportParmsP->ssl_cipher_list);

}



static void
freeXportParms(const struct xmlrpc_client_transport * const transportP) {

    const struct curlSetup * const curlSetupP = &transportP->curlSetupStuff;

    if (curlSetupP->sslCipherList)
        strfree(curlSetupP->sslCipherList);
    if (curlSetupP->egdSocket)
        strfree(curlSetupP->egdSocket);
    if (curlSetupP->randomFile)
        strfree(curlSetupP->randomFile);
    if (curlSetupP->caPath)
        strfree(curlSetupP->caPath);
    if (curlSetupP->caInfo)
        strfree(curlSetupP->caInfo);
    if (curlSetupP->sslEngine)
        strfree(curlSetupP->sslEngine);
    if (curlSetupP->sslKeyPasswd)
        strfree(curlSetupP->sslKeyPasswd);
    if (curlSetupP->sslKeyType)
        strfree(curlSetupP->sslKeyType);
    if (curlSetupP->sslKey)
        strfree(curlSetupP->sslKey);
    if (curlSetupP->sslCertType)
        strfree(curlSetupP->sslCertType);
    if (curlSetupP->sslCert)
        strfree(curlSetupP->sslCert);
    if (curlSetupP->networkInterface)
        strfree(curlSetupP->networkInterface);
    if (transportP->userAgent)
        strfree(transportP->userAgent);
}



static void
createSyncCurlSession(xmlrpc_env * const envP,
                      CURL **      const curlSessionPP) {
/*----------------------------------------------------------------------------
   Create a Curl session to be used for multiple serial transactions.
   The Curl session we create is not complete -- it still has to be
   further set up for each particular transaction.

   We can't set up anything here that changes from one transaction to the
   next.

   We don't bother setting up anything that has to be set up for an
   asynchronous transaction because code that is common between synchronous
   and asynchronous transactions takes care of that anyway.

   That leaves things, such as cookies, that don't exist for
   asynchronous transactions, and are common to multiple serial
   synchronous transactions.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = curl_easy_init();

    if (curlSessionP == NULL)
        xmlrpc_faultf(envP, "Could not create Curl session.  "
                      "curl_easy_init() failed.");
    else {
        /* The following is a trick.  CURLOPT_COOKIEFILE is the name
           of the file containing the initial cookies for the Curl
           session.  But setting it is also what turns on the cookie
           function itself, whereby the Curl library accepts and
           stores cookies from the server and sends them back on
           future requests.  We don't have a file of initial cookies, but
           we want to turn on cookie function, so we set the option to
           something we know does not validly name a file.  Curl will
           ignore the error and just start up cookie function with no
           initial cookies.
        */
        curl_easy_setopt(curlSessionP, CURLOPT_COOKIEFILE, "");

        *curlSessionPP = curlSessionP;
    }
}



static void
destroySyncCurlSession(CURL * const curlSessionP) {

    curl_easy_cleanup(curlSessionP);
}



static void 
create(xmlrpc_env *                      const envP,
       int                               const flags ATTR_UNUSED,
       const char *                      const appname ATTR_UNUSED,
       const char *                      const appversion ATTR_UNUSED,
       const struct xmlrpc_xportparms *  const transportparmsP,
       size_t                            const parm_size,
       struct xmlrpc_client_transport ** const handlePP) {
/*----------------------------------------------------------------------------
   This does the 'create' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    struct xmlrpc_curl_xportparms * const curlXportParmsP = 
        (struct xmlrpc_curl_xportparms *) transportparmsP;

    struct xmlrpc_client_transport * transportP;

    initWindowsStuff(envP);

    MALLOCVAR(transportP);
    if (transportP == NULL)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR, 
            "Unable to allocate transport descriptor.");
    else {
        pthread_mutex_init(&transportP->listLock, NULL);
        
        list_make_empty(&transportP->rpcList);

        /*
         * This is the main global constructor for the app. Call this before
         * _any_ libcurl usage. If this fails, *NO* libcurl functions may be
         * used, or havoc may be the result.
         */
        curl_global_init(CURL_GLOBAL_ALL);

        /* The above makes it look like Curl is not re-entrant.  We should
           check into that.
        */

        getXportParms(envP, curlXportParmsP, parm_size, transportP);

        if (!envP->fault_occurred) {
            createSyncCurlSession(envP, &transportP->syncCurlSessionP);
            
            if (envP->fault_occurred)
                freeXportParms(transportP);
        }                 
        if (envP->fault_occurred)
            free(transportP);
    }
    *handlePP = transportP;
}



static void
termWindowStuff(void) {

#if defined (WIN32)
    WSACleanup();
#endif
}



static void 
destroy(struct xmlrpc_client_transport * const clientTransportP) {
/*----------------------------------------------------------------------------
   This does the 'destroy' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(clientTransportP != NULL);

    XMLRPC_ASSERT(list_is_empty(&clientTransportP->rpcList));

    destroySyncCurlSession(clientTransportP->syncCurlSessionP);

    freeXportParms(clientTransportP);

    curl_global_cleanup();

    pthread_mutex_destroy(&clientTransportP->listLock);

    termWindowStuff();

    free(clientTransportP);
}



static void
addHeader(xmlrpc_env * const envP,
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



static void
addUserAgentHeader(xmlrpc_env *         const envP,
                   struct curl_slist ** const headerListP,
                   const char *         const userAgent) {
    
    if (userAgent) {
        curl_version_info_data * const curlInfoP =
            curl_version_info(CURLVERSION_NOW);
        char curlVersion[32];
        const char * userAgentHeader;
        
        snprintf(curlVersion, sizeof(curlVersion), "%u.%u.%u",
                (curlInfoP->version_num >> 16) && 0xff,
                (curlInfoP->version_num >>  8) && 0xff,
                (curlInfoP->version_num >>  0) && 0xff
            );
                  
        casprintf(&userAgentHeader,
                  "User-Agent: %s Xmlrpc-c/%s Curl/%s",
                  userAgent, XMLRPC_C_VERSION, curlVersion);
        
        if (userAgentHeader == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "User-Agent header");
        else {
            addHeader(envP, headerListP, userAgentHeader);
            
            strfree(userAgentHeader);
        }
    }
}



static void
addAuthorizationHeader(xmlrpc_env *         const envP,
                       struct curl_slist ** const headerListP,
                       const char *         const basicAuthInfo) {

    if (basicAuthInfo) {
        const char * authorizationHeader;
            
        casprintf(&authorizationHeader, "Authorization: %s", basicAuthInfo);
            
        if (authorizationHeader == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "Authorization header");
        else {
            addHeader(envP, headerListP, authorizationHeader);

            strfree(authorizationHeader);
        }
    }
}



static void
createCurlHeaderList(xmlrpc_env *               const envP,
                     const xmlrpc_server_info * const serverP,
                     const char *               const userAgent,
                     struct curl_slist **       const headerListP) {

    struct curl_slist * headerList;

    headerList = NULL;  /* initial value - empty list */

    addContentTypeHeader(envP, &headerList);
    if (!envP->fault_occurred) {
        addUserAgentHeader(envP, &headerList, userAgent);
        if (!envP->fault_occurred) {
            addAuthorizationHeader(envP, &headerList, 
                                   serverP->_http_basic_auth);
        }
    }
    if (envP->fault_occurred)
        curl_slist_free_all(headerList);
    else
        *headerListP = headerList;
}



static void
setupCurlSession(xmlrpc_env *             const envP,
                 curlTransaction *        const curlTransactionP,
                 xmlrpc_mem_block *       const callXmlP,
                 xmlrpc_mem_block *       const responseXmlP,
                 const struct curlSetup * const curlSetupP) {
/*----------------------------------------------------------------------------
   Set up the Curl session for the transaction *curlTransactionP so that
   a subsequent curl_easy_perform() will perform said transaction.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    curl_easy_setopt(curlSessionP, CURLOPT_POST, 1);
    curl_easy_setopt(curlSessionP, CURLOPT_URL, curlTransactionP->serverUrl);
    curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYPEER,
                     curlSetupP->sslVerifyPeer);
    curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYHOST,
                     curlSetupP->sslVerifyHost ? 2 : 0);

    if (curlSetupP->networkInterface)
        curl_easy_setopt(curlSessionP, CURLOPT_INTERFACE,
                         curlSetupP->networkInterface);
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
        curl_easy_setopt(curlSessionP, CURLOPT_SSLENGINE_DEFAULT);
    if (curlSetupP->sslVersion)
        curl_easy_setopt(curlSessionP, CURLOPT_SSLVERSION,
                         curlSetupP->sslVersion);
    if (curlSetupP->caInfo)
        curl_easy_setopt(curlSessionP, CURLOPT_CAINFO,
                         curlSetupP->caInfo);
    if (curlSetupP->caPath)
        curl_easy_setopt(curlSessionP, CURLOPT_CAPATH,
                         curlSetupP->caPath);
    if (curlSetupP->randomFile)
        curl_easy_setopt(curlSessionP, CURLOPT_RANDOM_FILE,
                         curlSetupP->randomFile);
    if (curlSetupP->egdSocket)
        curl_easy_setopt(curlSessionP, CURLOPT_EGDSOCKET,
                         curlSetupP->egdSocket);
    if (curlSetupP->sslCipherList)
        curl_easy_setopt(curlSessionP, CURLOPT_SSL_CIPHER_LIST,
                         curlSetupP->sslCipherList);

    XMLRPC_MEMBLOCK_APPEND(char, envP, callXmlP, "\0", 1);
    if (!envP->fault_occurred) {
        curl_easy_setopt(curlSessionP, CURLOPT_POSTFIELDS, 
                         XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP));
        
        curl_easy_setopt(curlSessionP, CURLOPT_FILE, responseXmlP);
        curl_easy_setopt(curlSessionP, CURLOPT_HEADER, 0 );
        curl_easy_setopt(curlSessionP, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(curlSessionP, CURLOPT_ERRORBUFFER, 
                         curlTransactionP->curlError);
        curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 1);
        
        curl_easy_setopt(curlSessionP, CURLOPT_HTTPHEADER, 
                         curlTransactionP->headerList);
    }
}



static void
createCurlTransaction(xmlrpc_env *               const envP,
                      CURL *                     const curlSessionP,
                      const xmlrpc_server_info * const serverP,
                      xmlrpc_mem_block *         const callXmlP,
                      xmlrpc_mem_block *         const responseXmlP,
                      const char *               const userAgent,
                      const struct curlSetup *   const curlSetupStuffP,
                      curlTransaction **         const curlTransactionPP) {

    curlTransaction * curlTransactionP;

    MALLOCVAR(curlTransactionP);
    if (curlTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create Curl transaction.");
    else {
        curlTransactionP->curlSessionP = curlSessionP;

        curlTransactionP->serverUrl = strdup(serverP->_server_url);
        if (curlTransactionP->serverUrl == NULL)
            xmlrpc_faultf(envP, "Out of memory to store server URL.");
        else {
            createCurlHeaderList(envP, serverP, userAgent,
                                 &curlTransactionP->headerList);
            
            if (!envP->fault_occurred)
                setupCurlSession(envP, curlTransactionP,
                                 callXmlP, responseXmlP,
                                 curlSetupStuffP);

            if (envP->fault_occurred)
                strfree(curlTransactionP->serverUrl);
        }
        if (envP->fault_occurred)
            free(curlTransactionP);
    }
    *curlTransactionPP = curlTransactionP;
}



static void
destroyCurlTransaction(curlTransaction * const curlTransactionP) {

    curl_slist_free_all(curlTransactionP->headerList);
    strfree(curlTransactionP->serverUrl);

    free(curlTransactionP);
}



static void
performCurlTransaction(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP) {

    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    CURLcode res;

    res = curl_easy_perform(curlSessionP);
    
    if (res != CURLE_OK)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NETWORK_ERROR, "Curl failed to perform "
            "HTTP POST request.  curl_easy_perform() says: %s", 
            curlTransactionP->curlError);
    else {
        CURLcode res;
        long http_result;
        res = curl_easy_getinfo(curlSessionP, CURLINFO_HTTP_CODE, 
                                &http_result);

        if (res != CURLE_OK)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, 
                "Curl performed the HTTP POST request, but was "
                "unable to say what the HTTP result code was.  "
                "curl_easy_getinfo(CURLINFO_HTTP_CODE) says: %s", 
                curlTransactionP->curlError);
        else {
            if (http_result != 200)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NETWORK_ERROR, "HTTP response: %ld",
                    http_result);
        }
    }
}



static void
doAsyncRpc2(void * const arg) {

    rpc * const rpcP = arg;

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    performCurlTransaction(&env, rpcP->curlTransactionP);

    rpcP->complete(rpcP->callInfoP, rpcP->responseXmlP, env);

    xmlrpc_env_clean(&env);
}



#ifdef WIN32

static unsigned __stdcall 
doAsyncRpc(void * arg) {
    doAsyncRpc2(arg);
    return 0;
}

#else
static void *
doAsyncRpc(void * arg) {
    doAsyncRpc2(arg);
    return NULL;
}

#endif



static void
createThread(xmlrpc_env * const envP,
             void * (*threadRoutine)(void *),
             rpc *        const rpcP,
             pthread_t *  const threadP) {

    int rc;

    rc = pthread_create(threadP, NULL, threadRoutine, rpcP);
    switch (rc) {
    case 0: 
        break;
    case EAGAIN:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "System resources exceeded.");
        break;
    case EINVAL:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "Parameter error");
        break;
    case ENOMEM:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "No memory for new thread.");
        break;
    default:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "Unrecognized error code %d.", rc);
        break;
    }
}



static void
createRpc(xmlrpc_env *                     const envP,
          struct xmlrpc_client_transport * const clientTransportP,
          CURL *                           const curlSessionP,
          const xmlrpc_server_info *       const serverP,
          xmlrpc_mem_block *               const callXmlP,
          xmlrpc_mem_block *               const responseXmlP,
          xmlrpc_transport_asynch_complete       complete, 
          struct xmlrpc_call_info *        const callInfoP,
          rpc **                           const rpcPP) {

    rpc * rpcP;

    MALLOCVAR(rpcP);
    if (rpcP == NULL)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "Couldn't allocate memory for rpc object");
    else {
        rpcP->callInfoP = callInfoP;
        rpcP->complete  = complete;
        rpcP->responseXmlP = responseXmlP;
        rpcP->threadExists = FALSE;

        rpcP->curlSessionP = curlSessionP;
        createCurlTransaction(envP,
                              curlSessionP,
                              serverP,
                              callXmlP, responseXmlP, 
                              clientTransportP->userAgent,
                              &clientTransportP->curlSetupStuff,
                              &rpcP->curlTransactionP);
        if (!envP->fault_occurred) {
            list_init_header(&rpcP->link, rpcP);
            pthread_mutex_lock(&clientTransportP->listLock);
            list_add_head(&clientTransportP->rpcList, &rpcP->link);
            pthread_mutex_unlock(&clientTransportP->listLock);
            
            if (envP->fault_occurred)
                destroyCurlTransaction(rpcP->curlTransactionP);
        }
        if (envP->fault_occurred)
            free(rpcP);
    }
    *rpcPP = rpcP;
}



static void 
destroyRpc(rpc * const rpcP) {

    XMLRPC_ASSERT_PTR_OK(rpcP);
    XMLRPC_ASSERT(!rpcP->threadExists);

    destroyCurlTransaction(rpcP->curlTransactionP);

    list_remove(&rpcP->link);

    free(rpcP);
}



static void
performRpc(xmlrpc_env * const envP,
           rpc *        const rpcP) {

    performCurlTransaction(envP, rpcP->curlTransactionP);
}



static void
startRpc(xmlrpc_env * const envP,
         rpc *        const rpcP) {

    createThread(envP, &doAsyncRpc, rpcP, &rpcP->thread);
    if (!envP->fault_occurred)
        rpcP->threadExists = TRUE;
}



static void 
sendRequest(xmlrpc_env *                     const envP, 
            struct xmlrpc_client_transport * const clientTransportP,
            const xmlrpc_server_info *       const serverP,
            xmlrpc_mem_block *               const callXmlP,
            xmlrpc_transport_asynch_complete       complete,
            struct xmlrpc_call_info *        const callInfoP) {
/*----------------------------------------------------------------------------
   Initiate an XML-RPC rpc asynchronously.  Don't wait for it to go to
   the server.

   Unless we return failure, we arrange to have complete() called when
   the rpc completes.

   This does the 'send_request' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    rpc * rpcP;
    xmlrpc_mem_block * responseXmlP;

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        CURL * const curlSessionP = curl_easy_init();
    
        if (curlSessionP == NULL)
            xmlrpc_faultf(envP, "Could not create Curl session.  "
                          "curl_easy_init() failed.");
        else {
            createRpc(envP, clientTransportP, curlSessionP, serverP,
                      callXmlP, responseXmlP,
                      complete, callInfoP,
                      &rpcP);

            if (!envP->fault_occurred) {
                startRpc(envP, rpcP);
                
                if (envP->fault_occurred)
                    destroyRpc(rpcP);
            }
            if (envP->fault_occurred)
                curl_easy_cleanup(curlSessionP);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
    /* The user's eventual finish_asynch call will destroy this RPC,
       Curl session, and response buffer
    */
}



static void * 
finishRpc(struct list_head * const headerP, 
          void *             const context ATTR_UNUSED) {
    
    rpc * const rpcP = headerP->itemP;

    if (rpcP->threadExists) {
        void *status;
        int result;

        result = pthread_join(rpcP->thread, &status);
        
        rpcP->threadExists = FALSE;
    }

    XMLRPC_MEMBLOCK_FREE(char, rpcP->responseXmlP);

    curl_easy_cleanup(rpcP->curlSessionP);

    destroyRpc(rpcP);

    return NULL;
}



static void 
finishAsynch(
    struct xmlrpc_client_transport * const clientTransportP ATTR_UNUSED,
    xmlrpc_timeoutType               const timeoutType ATTR_UNUSED,
    xmlrpc_timeout                   const timeout ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Wait for the threads of all outstanding RPCs to exit and destroy those
   RPCs.

   This does the 'finish_asynch' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    /* We ignore any timeout request.  Some day, we should figure out how
       to set an alarm and interrupt running threads.
    */

    pthread_mutex_lock(&clientTransportP->listLock);

    list_foreach(&clientTransportP->rpcList, finishRpc, NULL);

    pthread_mutex_unlock(&clientTransportP->listLock);
}



static void
call(xmlrpc_env *                     const envP,
     struct xmlrpc_client_transport * const clientTransportP,
     const xmlrpc_server_info *       const serverP,
     xmlrpc_mem_block *               const callXmlP,
     xmlrpc_mem_block **              const responsePP) {

    xmlrpc_mem_block * responseXmlP;
    rpc * rpcP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(serverP);
    XMLRPC_ASSERT_PTR_OK(callXmlP);
    XMLRPC_ASSERT_PTR_OK(responsePP);

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        createRpc(envP, clientTransportP, clientTransportP->syncCurlSessionP,
                  serverP,
                  callXmlP, responseXmlP,
                  NULL, NULL,
                  &rpcP);

        if (!envP->fault_occurred) {
            performRpc(envP, rpcP);

            *responsePP = responseXmlP;
            
            destroyRpc(rpcP);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
}



struct xmlrpc_client_transport_ops xmlrpc_curl_transport_ops = {
    &create,
    &destroy,
    &sendRequest,
    &call,
    &finishAsynch,
};

/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */

#ifndef HAVE_WIN32_CONFIG_H
#include "xmlrpc_config.h"
#else
#include "xmlrpc_win32_config.h"
#endif

#undef PACKAGE
#undef VERSION

#include <stddef.h>

#include "xmlrpc.h"
#include "xmlrpc_int.h"
#include "xmlrpc_client.h"
#include "xmlrpc_client_int.h"

/* Include our libwww headers. */
#include "WWWLib.h"
#include "WWWHTTP.h"
#include "WWWInit.h"

/* Include our libwww SSL headers, if available. */
#if HAVE_LIBWWW_SSL
#include "WWWSSL.h"
#endif

#include "xmlrpc_libwww_transport.h"

/* This value was discovered by Rick Blair. His efforts shaved two seconds
** off of every request processed. Many thanks. */
#define SMALLEST_LEGAL_LIBWWW_TIMEOUT (21)


/*=========================================================================
**  Initialization and Shutdown
**=========================================================================
*/

#if 0
static int printer (const char * fmt, va_list pArgs);
static int tracer (const char * fmt, va_list pArgs);
#endif

static int saved_flags;
static HTList *xmlrpc_conversions;

void libwww_transport_client_init(int flags,
            char *appname,
            char *appversion)
{
    saved_flags = flags;
    if (!(saved_flags & XMLRPC_CLIENT_SKIP_LIBWWW_INIT)) {
    
    /* We initialize the library using a robot profile, because we don't
    ** care about redirects or HTTP authentication, and we want to
    ** reduce our application footprint as much as possible. */
    HTProfile_newRobot(appname, appversion);

    /* Ilya Goldberg <igg@mit.edu> provided the following code to access
    ** SSL-protected servers. */
#if HAVE_LIBWWW_SSL
    /* Set the SSL protocol method. By default, it is the highest
    ** available protocol. Setting it up to SSL_V23 allows the client
    ** to negotiate with the server and set up either TSLv1, SSLv3,
    ** or SSLv2 */
    HTSSL_protMethod_set(HTSSL_V23);

    /* Set the certificate verification depth to 2 in order to be able to
    ** validate self-signed certificates */
    HTSSL_verifyDepth_set(2);

    /* Register SSL stuff for handling ssl access. The parameter we pass
    ** is NO because we can't be pre-emptive with POST */
    HTSSLhttps_init(NO);
#endif /* HAVE_LIBWWW_SSL */
    
    /* For interoperability with Frontier, we need to tell libwww *not*
    ** to send 'Expect: 100-continue' headers. But if we're not sending
    ** these, we shouldn't wait for them. So set our built-in delays to
        ** the smallest legal values. */
    HTTP_setBodyWriteDelay (SMALLEST_LEGAL_LIBWWW_TIMEOUT,
                SMALLEST_LEGAL_LIBWWW_TIMEOUT);
    
    /* We attempt to disable all of libwww's chatty, interactive
    ** prompts. Let's hope this works. */
    HTAlert_setInteractive(NO);

    /* Here are some alternate setup calls which will help greatly
    ** with debugging, should the need arise.
    **
    ** HTProfile_newNoCacheClient(appname, appversion);
    ** HTAlert_setInteractive(YES);
    ** HTPrint_setCallback(printer);
    ** HTTrace_setCallback(tracer); */
    }

    /* Set up our list of conversions for XML-RPC requests. This is a
    ** massively stripped-down version of the list in libwww's HTInit.c.
    ** XXX - This is hackish; 10.0 is an arbitrary, large quality factor
    ** designed to override the built-in converter for XML. */
    xmlrpc_conversions = HTList_new();
    HTConversion_add(xmlrpc_conversions, "text/xml", "*/*",
             HTThroughLine, 10.0, 0.0, 0.0);
}

void libwww_transport_client_cleanup()
{
    if (!(saved_flags & XMLRPC_CLIENT_SKIP_LIBWWW_INIT)) {
    HTProfile_delete();
    }
}


/*=========================================================================
**  Debugging Callbacks
**=========================================================================
*/

#if 0

static int printer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stdout, fmt, pArgs));
}

static int tracer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stderr, fmt, pArgs));
}

#endif


/*=========================================================================
**  HTTP Error Reporting
**=======================================================================*/

static void 
set_fault_from_http_request(xmlrpc_env * const envP,
                            int          const status,
                            HTRequest *  const requestP) {
/*----------------------------------------------------------------------------
   Assuming 'requestP' identifies a completed libwww HTTP request, set
   *envP according to its success/error status.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_PTR_OK(requestP);

    if (status == 200) {
        /* No error.  Don't set one in *envP */
    } else {
        /* Get an error message from libwww. The middle three
           parameters to HTDialog_errorMessage appear to be ignored.
           XXX - The documentation for this API is terrible, so we may
           be using it incorrectly.  
        */
        HTList * const errStack = HTRequest_error(requestP);
    
        if (errStack == NULL) {
            /* I think this is probably impossible, because we didn't get
               status 200, but I don't completely understand HTTP and libwww.
            */
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_NETWORK_ERROR,
                "HTTP error #%d occurred, but there was no additional "
                "error information supplied", status);
        } else {
            const char * const msg = 
                HTDialog_errorMessage(requestP, HT_A_MESSAGE, HT_MSG_NULL,
                                      "An error occurred", errStack);

            if (msg == NULL)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NETWORK_ERROR,
                    "HTTP error #%d occurred.  Libwww's "
                    "HTDialog_errorMessage() routine was mysteriously "
                    "unable to interpret the additional error information, "
                    "so we have none to report.", status);
            else {
                /* Set our fault.  Note that this may inlcude line breaks,
                   because 'msg' may.  We should fix that.  Formatting for
                   display is none of our business at this level.
                */
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NETWORK_ERROR,
                    "HTTP error #%d occurred.  libwww says %s", status, msg);
                
                xmlrpc_strfree(msg);
            }
        }
    }
}



/* Fail if we didn't get a 200 OK message from the remote HTTP server. */
#define FAIL_IF_NOT_200_OK(status,env,request) \
    do { \
        if ((status) != 200) { \
            set_fault_from_http_request((env), (status), (request)); \
            goto cleanup; \
        } \
    } while (0)



/*=========================================================================
  HTCookie callback to check for auth cookie and set it.
=========================================================================*/

PRIVATE BOOL 
xmlrpc_authcookie_store(HTRequest * const request ATTR_UNUSED, 
                        HTCookie *  const cookie, 
                        void *      const param ATTR_UNUSED) {

    /* First check to see if the cookie exists at all. */
    if (cookie) {
        /* Check for auth cookie. */
        if (!strcasecmp("auth", HTCookie_name(cookie)))
            /* Set auth cookie as HTTP_COOKIE_AUTH. */
            setenv("HTTP_COOKIE_AUTH", HTCookie_value(cookie), 1);
    }
    return YES;
}



/*=========================================================================
  HTCookie callback to get auth value and store it in cookie.
=========================================================================*/

PRIVATE HTAssocList *
xmlrpc_authcookie_grab(HTRequest * const request ATTR_UNUSED,
                       void *      const param ATTR_UNUSED) {

    /* Create associative list for cookies. */
    HTAssocList *alist = HTAssocList_new();

    /* If HTTP_COOKIE_AUTH is set, pass that to the list. */
    if (getenv("HTTP_COOKIE_AUTH") != NULL)
        HTAssocList_addObject(alist, "auth", getenv("HTTP_COOKIE_AUTH"));

    return alist;
}



/*=========================================================================
**  libwww_transport_info
**=========================================================================
**  This data structure holds the transport information about a 
**  particular call, including the lower-level HTTP information.
*/

typedef struct {

    /* These fields are used when performing synchronous calls. */
    int is_done;
    int http_status;

    /* Low-level information used by libwww. */
    HTRequest *request;
    HTChunk *response_data;
    HTParentAnchor *source_anchor;
    HTAnchor *dest_anchor;

    /* Tag along call_info pointer for later referencing. */
    call_info *info; 
} libwww_transport_info;

static void delete_source_anchor (HTParentAnchor *anchor)
{
    /* We need to clear the document first, or else libwww won't
    ** really delete the anchor. */
    HTAnchor_setDocument(anchor, NULL);

    /* XXX - Deleting this anchor causes HTLibTerminate to dump core. */
    /* HTAnchor_delete(anchor); */
}

void *libwww_transport_info_new (xmlrpc_env *env,
                                 xmlrpc_server_info *server,
                                 call_info *info)
{
    libwww_transport_info *retval;
    HTRqHd request_headers;
    HTStream *target_stream;

    /* Allocate our structure. */
    retval = (libwww_transport_info*) malloc(sizeof(libwww_transport_info));
    XMLRPC_FAIL_IF_NULL(retval, env, XMLRPC_INTERNAL_ERROR,
                        "Out of memory in libwww_transport_info_new");
    /* Clear contents. */
    memset(retval, 0, sizeof(libwww_transport_info));

    /* Set up our basic members. */
    retval->is_done = 0;
    retval->http_status = 0;
    retval->info = info;

    /* Start cookie handler. */
    HTCookie_init();
    HTCookie_setCallbacks(xmlrpc_authcookie_store, NULL,
                          xmlrpc_authcookie_grab, NULL);

    /* Create a HTRequest object. */
    retval->request = HTRequest_new();
    XMLRPC_FAIL_IF_NULL(retval, env, XMLRPC_INTERNAL_ERROR,
                        "HTRequest_new failed");
    
    /* Install ourselves as the request context. */
    HTRequest_setContext(retval->request, info);

    /* XXX - Disable the 'Expect:' header so we can talk to Frontier. */
    request_headers = HTRequest_rqHd(retval->request);
    request_headers = request_headers & ~HT_C_EXPECT;
    HTRequest_setRqHd(retval->request, request_headers);

    /* Send an authorization header if we need one. */
    if (server->_http_basic_auth)
        HTRequest_addCredentials(retval->request, "Authorization",
                                 server->_http_basic_auth);
    
    /* Make sure there is no XML conversion handler to steal our data.
    ** The 'override' parameter is currently ignored by libwww, so our
    ** list of conversions must be designed to co-exist with the built-in
    ** conversions. */
    HTRequest_setConversion(retval->request, xmlrpc_conversions, NO);

    /* Set up our response buffer. */
    target_stream = HTStreamToChunk(retval->request,
                                    &retval->response_data, 0);
    XMLRPC_FAIL_IF_NULL(retval->response_data, env, XMLRPC_INTERNAL_ERROR,
                        "HTStreamToChunk failed");
    XMLRPC_ASSERT(target_stream != NULL);
    HTRequest_setOutputStream(retval->request, target_stream);
    HTRequest_setOutputFormat(retval->request, WWW_SOURCE);

    /* Call has already been serialized for us into info->serialized_xml. */

    /* Build a source anchor which points to our serialized call. */
    retval->source_anchor = HTTmpAnchor(NULL);
    XMLRPC_FAIL_IF_NULL(retval->source_anchor, env, XMLRPC_INTERNAL_ERROR,
            "Could not build source anchor");
    HTAnchor_setDocument(retval->source_anchor,
             xmlrpc_mem_block_contents(info->serialized_xml));
    HTAnchor_setFormat(retval->source_anchor, HTAtom_for("text/xml"));
    HTAnchor_setLength(retval->source_anchor,
               xmlrpc_mem_block_size(info->serialized_xml));

    /* Get our destination anchor. */
    retval->dest_anchor = HTAnchor_findAddress(server->_server_url);
    XMLRPC_FAIL_IF_NULL(retval->dest_anchor, env, XMLRPC_INTERNAL_ERROR,
            "Could not build destination anchor");
    
 cleanup:
    if (env->fault_occurred) {
    if (retval) {
        if (retval->request)
        HTRequest_delete(retval->request);
        if (retval->response_data)
        HTChunk_delete(retval->response_data);
        if (retval->source_anchor) {
        /* See below for comments about deleting the source and dest
        ** anchors. This is a bit of a black art. */
        delete_source_anchor(retval->source_anchor);
        }
        free(retval);
    }
    return NULL;
    }
    return retval;
}

void libwww_transport_info_free (void *transport_info)
{
    libwww_transport_info *info = (libwww_transport_info *)transport_info;
    XMLRPC_ASSERT_PTR_OK(info);
    XMLRPC_ASSERT(info->request != XMLRPC_BAD_POINTER);
    XMLRPC_ASSERT(info->response_data != XMLRPC_BAD_POINTER);

    /* Flush our request object and the data we got back. */
    HTRequest_delete(info->request);
    info->request = XMLRPC_BAD_POINTER;
    HTChunk_delete(info->response_data);
    info->response_data = XMLRPC_BAD_POINTER;

    /* This anchor points to private data, so we're allowed to delete it.  */
    delete_source_anchor(info->source_anchor);

    /* WARNING: We can't delete the destination anchor, because this points
    ** to something in the outside world, and lives in a libwww hash table.
    ** Under certain circumstances, this anchor may have been reissued to
    ** somebody else. So over time, the anchor cache will grow. If this
    ** is a problem for your application, read the documentation for
    ** HTAnchor_deleteAll.
    **
    ** However, we CAN check to make sure that no documents have been
    ** attached to the anchor. This assertion may fail if you're using
    ** libwww for something else, so please feel free to comment it out. */
    /* XMLRPC_ASSERT(HTAnchor_document(info->dest_anchor) == NULL); */

    free(info);
}



/*=========================================================================
**  parse_response_chunk
**=========================================================================
**  Extract the data from the response buffer, make sure we actually got
**  something, and parse it as an XML-RPC response.
*/

static xmlrpc_value *
parse_response_chunk(xmlrpc_env * const envP, 
                     call_info *  const infoP) {

    xmlrpc_value *retval;
    libwww_transport_info *t_info;
    t_info = (libwww_transport_info *)infoP->transport_info;

    /* Error-handling preconditions. */
    retval = NULL;

    /* Check to make sure that w3c-libwww actually sent us some data.
    ** XXX - This may happen if libwww is shut down prematurely, believe it
    ** or not--we'll get a 200 OK and no data. Gag me with a bogus design
    ** decision. This may also fail if some naughty libwww converter
    ** ate our data unexpectedly. */
    if (!HTChunk_data(t_info->response_data))
        XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, "w3c-libwww returned no data");

    xmlrpc_traceXml("XML-RPC RESPONSE", 
                    HTChunk_data(t_info->response_data),
                    HTChunk_size(t_info->response_data));
    
    /* Parse the response. */
    retval = xmlrpc_parse_response(envP, HTChunk_data(t_info->response_data),
                                   HTChunk_size(t_info->response_data));
    XMLRPC_FAIL_IF_FAULT(envP);

 cleanup:
    if (envP->fault_occurred) {
        if (retval)
            xmlrpc_DECREF(retval);
        return NULL;
    }
    return retval;
}



/*=========================================================================
**  xmlrpc_client_call
**=========================================================================
**  A synchronous XML-RPC client.
*/


/*=========================================================================
**  synch_terminate_handler
**=========================================================================
**  This handler gets called when a synchronous request is completed.
*/

static int 
synch_terminate_handler (HTRequest *  const request,
                         HTResponse * const response ATTR_UNUSED,
                         void *       const param ATTR_UNUSED,
                         int          const status) {

    call_info *info;
    libwww_transport_info *t_info;

    /* Fetch the call_info structure describing this call. */
    info = (call_info*) HTRequest_context(request);

    t_info = (libwww_transport_info *)info->transport_info;
    t_info->is_done = 1;
    t_info->http_status = status;

    HTEventList_stopLoop();

    return HT_OK;
}


xmlrpc_value *
libwww_transport_client_call_server_params(
    xmlrpc_env *         const envP,
    xmlrpc_server_info * const server,
    const char *         const method_name,
    xmlrpc_value *       const param_array) {

    call_info *info;
    libwww_transport_info *t_info;
    xmlrpc_value *retval;

    HTRequest *request;
    HTParentAnchor *src;
    HTAnchor *dst;
    int ok;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    info = NULL;
    retval = NULL;

    /* Create our call_info structure. */
    info = call_info_new(envP, server, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(envP);

    /* Setup our transport-specific data. */
    t_info = (libwww_transport_info *)info->transport_info;
    request = t_info->request;
    src = t_info->source_anchor;
    dst = t_info->dest_anchor;

    /* Install our request handler. */
    HTRequest_addAfter(request, &synch_terminate_handler, NULL, NULL, HT_ALL,
                       HT_FILTER_LAST, NO);

    /* Start our request running. */
    ok = HTPostAnchor(src, dst, request);
    if (!ok)
        XMLRPC_FAIL(envP, XMLRPC_INTERNAL_ERROR,
                    "Could not start POST request");

    /* Run our event-processing loop. This will exit when we've downloaded
    ** our data, or when the event loop gets exited for another reason.
    ** We re-run the event loop until it exits for the *right* reason. */
    while (!t_info->is_done)
        HTEventList_newLoop();

    /* Make sure we got a "200 OK" message from the remote server. */
    FAIL_IF_NOT_200_OK(t_info->http_status, envP, request);

    /* XXX - Check to make sure response type is text/xml here. */
    
    /* Parse our response. */
    retval = parse_response_chunk(envP, info);
    XMLRPC_FAIL_IF_FAULT(envP);

 cleanup:
    if (info)
    call_info_free(info);
    if (envP->fault_occurred) {
    if (retval)
        xmlrpc_DECREF(retval);
    return NULL;
    }
    return retval;
}


/*=========================================================================
**  Event Loop
**=========================================================================
**  We manage a fair bit of internal state about our event loop. This is
**  needed to determine when (and if) we should exit the loop.
*/

static int outstanding_asynch_calls = 0;
static int event_loop_flags = 0;
static int timer_called = 0;

static void register_asynch_call (void)
{
    XMLRPC_ASSERT(outstanding_asynch_calls >= 0);
    outstanding_asynch_calls++;
}

static void unregister_asynch_call (void)
{
    XMLRPC_ASSERT(outstanding_asynch_calls > 0);
    outstanding_asynch_calls--;
    if (outstanding_asynch_calls == 0 &&
    (event_loop_flags | XMLRPC_CLIENT_FINISH_ASYNCH))
    HTEventList_stopLoop();
}

int xmlrpc_client_asynch_calls_are_unfinished (void)
{
    return (outstanding_asynch_calls > 0);
}

/* A handy timer callback which cancels the running event loop. */
static int timer_callback (HTTimer *timer ATTR_UNUSED,
               void *user_data ATTR_UNUSED,
               HTEventType event)
{
    XMLRPC_ASSERT(event == HTEvent_TIMEOUT);
    timer_called = 1;
    HTEventList_stopLoop();
    
    /* XXX - The meaning of this return value is undocumented, but close
    ** inspection of libwww's source suggests that we want to return HT_OK. */
    return HT_OK;
}



void 
xmlrpc_client_event_loop_run_general(int       const flags, 
                                     timeout_t const milliseconds) {
/*----------------------------------------------------------------------------
   Process all responses from outstanding requests as they come in.
   Return when there are no more outstanding responses.

   Or, if 'flags' has the XMLRPC_CLIENT_USE_TIMEOUT flag set, return
   when 'milliseconds' milliseconds have elapsed, regardless of whether
   there are still outstanding responses.

   The processing we do consists of telling libwww to process the
   completion of the libwww request.  That normally includes calling
   the xmlrpc_libwww_transport request termination handler, because
   the submitter of the libwww request would have registered that as a
   callback.
-----------------------------------------------------------------------------*/
    if (xmlrpc_client_asynch_calls_are_unfinished()) {
        HTTimer *timer;

        event_loop_flags = flags;
        
        /* Run an appropriate event loop. */
        if (event_loop_flags & XMLRPC_CLIENT_USE_TIMEOUT) {
            
            /* Run our event loop with a timer. Note that we need to be very
            ** careful about race conditions--timers can be fired in either
            ** HTimer_new or HTEventList_newLoop. And if our callback were to
            ** get called before we entered the loop, we would never exit.
            ** So we use a private flag of our own--we can't even rely on
            ** HTTimer_hasTimerExpired, because that only checks the time,
            ** not whether our callback has been run. Yuck. */
            timer_called = 0;
            timer = HTTimer_new(NULL, &timer_callback, NULL,
                                milliseconds, YES, NO);
            XMLRPC_ASSERT(timer != NULL);
            if (!timer_called)
                HTEventList_newLoop();
            HTTimer_delete(timer);
            
        } else {
            /* Run our event loop without a timer. */
            HTEventList_newLoop();
        }
        
        /* Reset our flags, so we don't interfere with direct calls to the
        ** libwww event loop functions. */
        event_loop_flags = 0;
    } else {
        /* There are *no* calls to process.  This may mean that none
           of the asynch calls were ever set up, and the client's
           callbacks have already been called with an error, or that
           all outstanding calls were completed during a previous
           synchronous call.  
        */
    }
}



void xmlrpc_client_event_loop_end (void)
{
    HTEventList_stopLoop();
}

void libwww_transport_finish_asynch (void)
{
    xmlrpc_client_event_loop_run_general(XMLRPC_CLIENT_FINISH_ASYNCH, 0);
}

void xmlrpc_client_event_loop_finish_asynch_timeout (timeout_t milliseconds)
{
    int flags = XMLRPC_CLIENT_FINISH_ASYNCH | XMLRPC_CLIENT_USE_TIMEOUT;
    xmlrpc_client_event_loop_run_general(flags, milliseconds);
}

void xmlrpc_client_event_loop_run (void)
{
    xmlrpc_client_event_loop_run_general(XMLRPC_CLIENT_NO_FLAGS, 0);
}

void xmlrpc_client_event_loop_run_timeout (timeout_t milliseconds)
{
    int flags = XMLRPC_CLIENT_USE_TIMEOUT;
    xmlrpc_client_event_loop_run_general(flags, milliseconds);
}



/*=========================================================================
**  xmlrpc_client_call_asynch
**=========================================================================
**  An asynchronous XML-RPC client.
*/

static int 
asynch_terminate_handler(HTRequest *  const request,
                         HTResponse * const response ATTR_UNUSED,
                         void *       const param ATTR_UNUSED,
                         int          const status) {
/*----------------------------------------------------------------------------
   Handle the termination of a libwww request.

   This is the bottom half of the xmlrpc_libwww_transport asynchronous
   call dispatcher.  It's what the dispatcher registers with libwww so
   that libwww calls it when a request that xmlrpc_libwww_transport
   submitted to it is complete.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    call_info *info;
    libwww_transport_info *t_info;
    xmlrpc_value *value;

    XMLRPC_ASSERT_PTR_OK(request);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    value = NULL;

    /* Fetch the call_info structure describing this call. */
    info = (call_info*) HTRequest_context(request);
    t_info = (libwww_transport_info *)info->transport_info;

    /* Unregister this call from the event loop. Among other things, this
    ** may decide to stop the event loop. (But remember, we must check to
    ** see if we've been registered, to prevent a nasty race condition
    ** with our top-half handler. */
    if (info->asynch_call_is_registered)
        unregister_asynch_call();

    /* Give up if an error occurred. */
    FAIL_IF_NOT_200_OK(status, &env, request);

    /* XXX - Check to make sure response type is text/xml here. */
    
    /* Parse our response. */
    value = parse_response_chunk(&env, info);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Call the user's callback function with the result */
    (*info->callback)(info->server_url, info->method_name, info->param_array,
                      info->user_data, &env, value);

cleanup:
    if (value)
        xmlrpc_DECREF(value);

    if (env.fault_occurred) {
        /* Call the user's callback function with our result */
        (*info->callback)(info->server_url, info->method_name,
                          info->param_array, info->user_data, &env, NULL);
    }

    /* Free our call_info structure. Again, we need to be careful
    ** about race conditions with xmlrpc_client_call_asynch here. */
    if (info->asynch_call_is_registered)
        call_info_free(info);

    xmlrpc_env_clean(&env);
    return HT_OK;
}



/* This is where we initiate an asynchronous request.
**/
void libwww_transport_send_request(xmlrpc_env *env, call_info *info)
{
    libwww_transport_info *t_info;

    HTRequest *request;
    HTParentAnchor *src;
    HTAnchor *dst;
    int ok;

    XMLRPC_ASSERT_PTR_OK(env);
    XMLRPC_ASSERT_PTR_OK(info);

    t_info = (libwww_transport_info *)info->transport_info;
    XMLRPC_ASSERT_PTR_OK(t_info);

    request = t_info->request;
    src = t_info->source_anchor;
    dst = t_info->dest_anchor;

    /* Install our request handler. */
    HTRequest_addAfter(request, &asynch_terminate_handler, NULL, NULL, HT_ALL,
               HT_FILTER_LAST, NO);

    /* Start our request running. Under certain pathological conditions,
    ** this may invoke asynch_terminate_handler immediately, regardless of
    ** the result code returned. So we have to be careful about who
    ** frees our call_info structure. */
    ok = HTPostAnchor(src, dst, request);
    if (!ok)
    XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR,
            "Could not start POST request");

    /* Register our asynchronous call with the event loop. Once this
    ** is done, we are no longer responsible for freeing the call_info
    ** structure. */
    register_asynch_call();

    /* This is now set by the caller of this function. */
    /* info->asynch_call_is_registered = 1; */

 cleanup:
    if (env->fault_occurred) {
    /* Report the error immediately. */
    (*info->callback)(info->server_url, info->method_name,
                    info->param_array, info->user_data,
            env, NULL);
    }
    //xmlrpc_env_clean(env);
}




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


#include "xmlrpc_config.h"
#undef PACKAGE
#undef VERSION

#include <stddef.h>

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"
#include "xmlrpc_client.h"

/* Include our libwww headers. */
#include "WWWLib.h"
#include "WWWHTTP.h"
#include "WWWInit.h"


/* This value was discovered by Rick Blair. His efforts shaved two seconds
** off of every request processed. Many thanks. */
#define SMALLEST_LEGAL_LIBWWW_TIMEOUT (21)


/*=========================================================================
**  Initialization and Shutdown
**=========================================================================
*/

static int printer (const char * fmt, va_list pArgs);
static int tracer (const char * fmt, va_list pArgs);

static int saved_flags;

void xmlrpc_client_init(int flags,
			char *appname,
			char *appversion)
{
    saved_flags = flags;
    if (!(saved_flags & XMLRPC_CLIENT_SKIP_LIBWWW_INIT)) {
	
	/* We initialize the library using a robot profile, because we don't
	** care about redirects or HTTP authentication, and we want to
	** reduce our application footprint as much as possible. */
	HTProfile_newRobot(appname, appversion);
	
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
}

void xmlrpc_client_cleanup()
{
    if (!(saved_flags & XMLRPC_CLIENT_SKIP_LIBWWW_INIT)) {
	HTProfile_delete();
    }
}


/*=========================================================================
**  Debugging Callbacks
**=========================================================================
*/

static int printer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stdout, fmt, pArgs));
}

static int tracer (const char * fmt, va_list pArgs)
{
    return (vfprintf(stderr, fmt, pArgs));
}


/*=========================================================================
**  HTTP Error Reporting
**=========================================================================
**  This code converts an HTTP error from libwww into an XML-RPC fault.
*/

/* Fail if we didn't get a 200 OK message from the remote HTTP server. */
#define FAIL_IF_NOT_200_OK(status,env,request) \
    do { \
        if ((status) != 200) { \
            set_fault_from_http_request((env), (status), (request)); \
            goto cleanup; \
        } \
    } while (0)

static set_fault_from_http_request (xmlrpc_env *env,
				    int status,
				    HTRequest *request)
{
    HTList *err_stack;
    HTError *error;
    char *msg;

    XMLRPC_ASSERT_PTR_OK(request);

    /* Try to get an error message from libwww. The middle three
    ** parameters to HTDialog_errorMessage appear to be ignored.
    ** XXX - The documentation for this API is terrible, so we may be using
    ** it incorrectly. */
    err_stack = HTRequest_error(request);
    XMLRPC_ASSERT(err_stack != NULL);
    msg = HTDialog_errorMessage(request, HT_A_MESSAGE, HT_MSG_NULL,
				"An error occurred", err_stack);
    XMLRPC_ASSERT(msg != NULL);

    /* Set our fault. We break this into multiple lines because it
    ** will generally contain line breaks to begin with. */
    xmlrpc_env_set_fault_formatted(env, XMLRPC_NETWORK_ERROR,
				   "HTTP error #%d occurred\n%s",
				   status, msg);

    /* Free our error message. */
    free(msg);
}


/*=========================================================================
**  call_info
**=========================================================================
**  This data structure holds all the information about a particular call,
**  include the XML-RPC information and the lower-level HTTP information.
*/

typedef struct {

    /* These fields are used when performing synchronous calls. */
    int is_done;
    int http_status;

    /* To prevent race conditions between the top half and the bottom
    ** half of our asynch support, we need to keep track of who owns this
    ** structure and whether the asynch call has been registered with
    ** the event loop. If this flag is true, then the
    ** asynch_terminate_handler must unregister the call and destroy this
    ** data structure. */
    int asynch_call_is_registered;
    
    /* These fields are used when performing asynchronous calls.
    ** The _asynch_data_holder contains server_url, method_name and
    ** param_array, so it's the only thing we need to free. */
    xmlrpc_value *_asynch_data_holder;
    char *server_url;
    char *method_name;
    xmlrpc_value *param_array;
    xmlrpc_response_handler callback;
    void *user_data;

    /* Low-level information used by libwww. */
    HTRequest *request;
    HTChunk *response_data;
    HTParentAnchor *source_anchor;
    HTAnchor *dest_anchor;

    /* The serialized XML data passed to this call. We keep this around
    ** for use by our source_anchor field. */
    xmlrpc_mem_block *serialized_xml;
} call_info;

static void delete_source_anchor (HTParentAnchor *anchor)
{
    /* We need to clear the document first, or else libwww won't
    ** really delete the anchor. */
    HTAnchor_setDocument(anchor, NULL);

    /* XXX - Deleting this anchor causes HTLibTerminate to dump core. */
    /* HTAnchor_delete(anchor); */
}

static call_info *call_info_new (xmlrpc_env *env,
				 char *server_url,
				 char *method_name,
				 xmlrpc_value *param_array)
{
    call_info *retval;
    HTRqHd request_headers;
    HTStream *target_stream;
    int serialized_xml_valid;

    /* Allocate our structure. */
    retval = (call_info*) malloc(sizeof(call_info));
    XMLRPC_FAIL_IF_NULL(retval, env, XMLRPC_INTERNAL_ERROR, "Out of memory");
    retval->asynch_call_is_registered = 0;
    retval->request        = NULL;
    retval->response_data  = NULL;
    retval->source_anchor  = NULL;
    retval->dest_anchor    = NULL;
    retval->serialized_xml = NULL;
    retval->_asynch_data_holder = NULL;

    /* Set up our basic members. */
    retval->is_done = 0;
    retval->http_status = 0;

    /* Create a HTRequest object. */
    retval->request = HTRequest_new();
    XMLRPC_FAIL_IF_NULL(retval, env, XMLRPC_INTERNAL_ERROR,
			"HTRequest_new failed");
    
    /* Install ourselves as the request context. */
    HTRequest_setContext(retval->request, retval);

    /* XXX - Disable the 'Expect:' header so we can talk to Frontier. */
    request_headers = HTRequest_rqHd(retval->request);
    request_headers = request_headers & ~HT_C_EXPECT;
    HTRequest_setRqHd(retval->request, request_headers);

    /* Set up our response buffer. */
    target_stream = HTStreamToChunk(retval->request,
				    &retval->response_data, 0);
    XMLRPC_FAIL_IF_NULL(retval->response_data, env, XMLRPC_INTERNAL_ERROR,
			"HTStreamToChunk failed");
    XMLRPC_ASSERT(target_stream != NULL);
    HTRequest_setOutputStream(retval->request, target_stream);    
    HTRequest_setOutputFormat(retval->request, WWW_SOURCE);

    /* Serialize our call. */
    retval->serialized_xml = xmlrpc_mem_block_new(env, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    xmlrpc_serialize_call(env, retval->serialized_xml,
			  method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Build a source anchor which points to our serialized call. */
    retval->source_anchor = HTTmpAnchor(NULL);
    XMLRPC_FAIL_IF_NULL(retval->source_anchor, env, XMLRPC_INTERNAL_ERROR,
			"Could not build source anchor");
    HTAnchor_setDocument(retval->source_anchor,
			 xmlrpc_mem_block_contents(retval->serialized_xml));
    HTAnchor_setFormat(retval->source_anchor, HTAtom_for("text/xml"));
    HTAnchor_setLength(retval->source_anchor,
		       xmlrpc_mem_block_size(retval->serialized_xml));

    /* Get our destination anchor. */
    retval->dest_anchor = HTAnchor_findAddress(server_url);
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
	    if (retval->serialized_xml)
		xmlrpc_mem_block_free(retval->serialized_xml);
	    free(retval);
	}
	return NULL;
    }
    return retval;
}

static void call_info_free (call_info *info)
{
    XMLRPC_ASSERT_PTR_OK(info);
    XMLRPC_ASSERT(info->request != XMLRPC_BAD_POINTER);
    XMLRPC_ASSERT(info->response_data != XMLRPC_BAD_POINTER);

    /* If this has been allocated, we're responsible for destroying it. */
    if (info->_asynch_data_holder)
	xmlrpc_DECREF(info->_asynch_data_holder);

    /* Flush our request object and the data we got back. */
    HTRequest_delete(info->request);
    info->request = XMLRPC_BAD_POINTER;
    HTChunk_delete(info->response_data);
    info->request = XMLRPC_BAD_POINTER;

    /* This anchor points to private data, so we're allowed to delete it.  */
    delete_source_anchor(info->source_anchor);

    /* Now we can blow away the XML data pointed to by the source_anchor. */
    xmlrpc_mem_block_free(info->serialized_xml);

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

static void call_info_set_asynch_data (xmlrpc_env *env,
				       call_info *info,
				       char *server_url,
				       char *method_name,
				       xmlrpc_value *param_array,
				       xmlrpc_response_handler callback,
				       void *user_data)
{
    xmlrpc_value *holder;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(info);
    XMLRPC_ASSERT(info->_asynch_data_holder == NULL);
    XMLRPC_ASSERT_PTR_OK(server_url);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    holder = NULL;

    /* Install our callback and user_data.
    ** (We're not responsible for destroying the user_data.) */
    info->callback  = callback;
    info->user_data = user_data;

    /* Build an XML-RPC data structure to hold our other data. This makes
    ** copies of server_url and method_name, and increments the reference
    ** to the param_array. */
    holder = xmlrpc_build_value(env, "(ssV)",
				server_url, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Parse the newly-allocated structure into our public member variables.
    ** This doesn't make any new references, so we can dispose of the whole
    ** thing by DECREF'ing the one master reference. Nifty, huh? */
    xmlrpc_parse_value(env, holder, "(ssV)",
		       &info->server_url,
		       &info->method_name,
		       &info->param_array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Hand over ownership of the holder to the call_info struct. */
    info->_asynch_data_holder = holder;
    holder = NULL;

 cleanup:
    if (env->fault_occurred) {
	if (holder)
	    xmlrpc_DECREF(holder);
    }
}


/*=========================================================================
**  xmlrpc_client_call
**=========================================================================
**  A synchronous XML-RPC client.
*/

static int synch_terminate_handler (HTRequest *request,
				    HTResponse *response,
				    void *param,
				    int status);

xmlrpc_value * xmlrpc_client_call (xmlrpc_env *env,
				   char *server_url,
				   char *method_name,
				   char *format,
				   ...)
{
    va_list args;
    xmlrpc_value *arg_array, *retval;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(format);

    /* Error-handling preconditions. */
    arg_array = retval = NULL;

    /* Build our argument array. */
    va_start(args, format);
    arg_array = xmlrpc_build_value_va(env, format, args);
    va_end(args);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Perform the actual XML-RPC call. */
    retval = xmlrpc_client_call_params(env, server_url, method_name,
				       arg_array);
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (arg_array)
	xmlrpc_DECREF(arg_array);
    if (env->fault_occurred) {
	if (retval)
	    xmlrpc_DECREF(retval);
	return NULL;
    }
    return retval;
}

xmlrpc_value * xmlrpc_client_call_params (xmlrpc_env *env,
					  char *server_url,
					  char *method_name,
					  xmlrpc_value *param_array)
{
    call_info *info;
    xmlrpc_value *retval;

    HTRequest *request;
    HTParentAnchor *src;
    HTAnchor *dst;
    int ok;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    info = NULL;
    retval = NULL;

    /* Create our call_info structure. */
    info = call_info_new(env, server_url, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(env);
    request = info->request;
    src = info->source_anchor;
    dst = info->dest_anchor;

    /* Install our request handler. */
    HTRequest_addAfter(request, &synch_terminate_handler, NULL, NULL, HT_ALL,
		       HT_FILTER_LAST, NO);

    /* Start our request running. */
    ok = HTPostAnchor(src, dst, request);
    if (!ok)
	XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR,
		    "Could not start POST request");

    /* Run our event-processing loop. This will exit when we've downloaded
    ** our data, or when the event loop gets exited for another reason.
    ** We re-run the event loop until it exits for the *right* reason. */
    while (!info->is_done)
	HTEventList_newLoop();

    /* Make sure we got a "200 OK" message from the remote server. */
    FAIL_IF_NOT_200_OK(info->http_status, env, request);

    /* XXX - Check to make sure response type is text/xml here. */
    
    /* Parse our response. */
    retval = xmlrpc_parse_response(env, HTChunk_data(info->response_data),
				   HTChunk_size(info->response_data));
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (info)
	call_info_free(info);
    if (env->fault_occurred) {
	if (retval)
	    xmlrpc_DECREF(retval);
	return NULL;
    }
    return retval;
}


/*=========================================================================
**  synch_terminate_handler
**=========================================================================
**  This handler gets called when a synchronous request is completed.
*/

static int synch_terminate_handler (HTRequest *request,
				    HTResponse *response,
				    void *param,
				    int status)
{
    call_info *info;

    /* Fetch the call_info structure describing this call. */
    info = (call_info*) HTRequest_context(request);

    info->is_done = 1;
    info->http_status = status;

    HTEventList_stopLoop();
    return HT_OK;
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
static int timer_callback (HTTimer *timer, void *user_data, HTEventType event)
{
    XMLRPC_ASSERT(event == HTEvent_TIMEOUT);
    timer_called = 1;
    HTEventList_stopLoop();
}

void xmlrpc_client_event_loop_run_general (int flags, timeout_t milliseconds)
{
    HTTimer *timer;

    /* If there are *no* calls to process, just go ahead and return.
    ** This may mean that none of the asynch calls were ever set up,
    ** and the client's callbacks have already been called with an error,
    ** or that all outstanding calls were completed during a previous
    ** synchronous call. */
    if (!xmlrpc_client_asynch_calls_are_unfinished())
	return;

    /* Set up our flags. */
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
}

void xmlrpc_client_event_loop_end (void)
{
    HTEventList_stopLoop();
}

void xmlrpc_client_event_loop_finish_asynch (void)
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

void xmlrpc_client_call_asynch (char *server_url,
				char *method_name,
				xmlrpc_response_handler callback,
				void *user_data,
				char *format,
				...)
{
    xmlrpc_env env;
    va_list args;
    xmlrpc_value *param_array;

    XMLRPC_ASSERT_PTR_OK(format);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    param_array = NULL;

    /* Build our argument array. */
    va_start(args, format);
    param_array = xmlrpc_build_value_va(&env, format, args);
    va_end(args);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Perform the actual XML-RPC call. */
    xmlrpc_client_call_asynch_params(server_url, method_name,
				     callback, user_data, param_array);

 cleanup:
    if (env.fault_occurred) {
	(*callback)(server_url, method_name, param_array, user_data,
		    &env, NULL);
    }

    if (param_array)
	xmlrpc_DECREF(param_array);
    xmlrpc_env_clean(&env);
}

static int asynch_terminate_handler (HTRequest *request,
				     HTResponse *response,
				     void *param,
				     int status);

void xmlrpc_client_call_asynch_params (char *server_url,
				       char *method_name,
				       xmlrpc_response_handler callback,
				       void *user_data,
				       xmlrpc_value *param_array)
{
    xmlrpc_env env;
    call_info *info;

    HTRequest *request;
    HTParentAnchor *src;
    HTAnchor *dst;
    int ok;

    XMLRPC_ASSERT_PTR_OK(server_url);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_PTR_OK(callback);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    info = NULL;

    /* Create our call_info structure. */
    info = call_info_new(&env, server_url, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(&env);
    request = info->request;
    src = info->source_anchor;
    dst = info->dest_anchor;

    /* Add some more data to our call_info struct. */
    call_info_set_asynch_data(&env, info, server_url, method_name, param_array,
			      callback, user_data);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Install our request handler. */
    HTRequest_addAfter(request, &asynch_terminate_handler, NULL, NULL, HT_ALL,
		       HT_FILTER_LAST, NO);

    /* Start our request running. Under certain pathological conditions,
    ** this may invoke asynch_terminate_handler immediately, regardless of
    ** the result code returned. So we have to be careful about who
    ** frees our call_info structure. */
    ok = HTPostAnchor(src, dst, request);
    if (!ok)
	XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR,
		    "Could not start POST request");

    /* Register our asynchronous call with the event loop. Once this
    ** is done, we are no longer responsible for freeing the call_info
    ** structure. */
    register_asynch_call();
    info->asynch_call_is_registered = 1;

 cleanup:
    if (info && !info->asynch_call_is_registered)
	call_info_free(info);
    if (env.fault_occurred) {
	/* Report the error immediately. */
	(*callback)(server_url, method_name, param_array, user_data,
		    &env, NULL);
    }
    xmlrpc_env_clean(&env);
}


/*=========================================================================
**  xmlrpc_client_call_asynch
**=========================================================================
**  The bottom half of our asynchronous call dispatcher.
*/

static int asynch_terminate_handler (HTRequest *request,
				     HTResponse *response,
				     void *param,
				     int status)
{
    xmlrpc_env env;
    call_info *info;
    xmlrpc_value *value;

    XMLRPC_ASSERT_PTR_OK(request);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    value = NULL;

    /* Fetch the call_info structure describing this call. */
    info = (call_info*) HTRequest_context(request);

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
    value = xmlrpc_parse_response(&env, HTChunk_data(info->response_data),
				  HTChunk_size(info->response_data));
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Pass the response to our callback function. */
    (*info->callback)(info->server_url, info->method_name, info->param_array,
		      info->user_data, &env, value);

 cleanup:
    if (value)
	xmlrpc_DECREF(value);

    if (env.fault_occurred) {
	/* Pass the fault to our callback function. */
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

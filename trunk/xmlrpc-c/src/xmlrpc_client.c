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
#include <stdio.h>

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"
#include "xmlrpc_client.h"

/* Include our libwww headers. */
#include "WWWLib.h"
#include "WWWHTTP.h"
#include "WWWInit.h"

/* Include our libwww SSL headers, if available. */
#if HAVE_LIBWWW_SSL
#include "WWWSSL.h"
#endif /* HAVE_LIBWWW_SSL */

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
	** with debugging, should the need arise. */
#if 0
	HTSetTraceMessageMask("sop");
	HTPrint_setCallback(printer);
	HTTrace_setCallback(tracer);
#endif
    }

    /* Set up our list of conversions for XML-RPC requests. This is a
    ** massively stripped-down version of the list in libwww's HTInit.c.
    ** XXX - This is hackish; 10.0 is an arbitrary, large quality factor
    ** designed to override the built-in converter for XML. */
    xmlrpc_conversions = HTList_new();
    HTConversion_add(xmlrpc_conversions, "text/xml", "*/*",
		     HTThroughLine, 10.0, 0.0, 0.0);
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

static void set_fault_from_http_request (xmlrpc_env *env,
					 int status,
					 HTRequest *request)
{
    HTList *err_stack;
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
**  xmlrpc_server_info
**=========================================================================
*/

xmlrpc_server_info *xmlrpc_server_info_new (xmlrpc_env *env,
					    char *server_url)
{
    xmlrpc_server_info *server;
    char *url_copy;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);

    /* Error-handling preconditions. */
    server = NULL;
    url_copy = NULL;

    /* Allocate our memory blocks. */
    server = (xmlrpc_server_info*) malloc(sizeof(xmlrpc_server_info));
    XMLRPC_FAIL_IF_NULL(server, env, XMLRPC_INTERNAL_ERROR,
			"Couldn't allocate memory for xmlrpc_server_info");
    url_copy = (char*) malloc(strlen(server_url) + 1);
    XMLRPC_FAIL_IF_NULL(url_copy, env, XMLRPC_INTERNAL_ERROR,
			"Couldn't allocate memory for server URL");

    /* Build our object. */
    strcpy(url_copy, server_url);
    server->_server_url = url_copy;
    server->_http_basic_auth = NULL;

 cleanup:
    if (env->fault_occurred) {
	if (url_copy)
	    free(url_copy);
	if (server)
	    free(server);
	return NULL;
    }
    return server;
}

xmlrpc_server_info * xmlrpc_server_info_copy(xmlrpc_env *env,
                                             xmlrpc_server_info *src_server)
{
    xmlrpc_server_info *new_server;
    char *url_copy, *auth_copy;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(src_server);

    /* Error-handling preconditions. */
    new_server = NULL;
    url_copy = NULL;
    auth_copy = NULL;

    /* Allocate our memory blocks. */
    new_server = (xmlrpc_server_info*) malloc(sizeof(xmlrpc_server_info));
    XMLRPC_FAIL_IF_NULL(new_server, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for xmlrpc_server_info");
    url_copy = (char*) malloc(strlen(src_server->_server_url) + 1);
    XMLRPC_FAIL_IF_NULL(url_copy, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for server URL");
    if (src_server->_http_basic_auth) {
	auth_copy = (char*) malloc(strlen(src_server->_http_basic_auth) + 1);
	XMLRPC_FAIL_IF_NULL(auth_copy, env, XMLRPC_INTERNAL_ERROR,
			    "Couldn't allocate memory for auth info");
    }
    
    /* Build our object. */
    strcpy(url_copy, src_server->_server_url);
    new_server->_server_url = url_copy;
    if (src_server->_http_basic_auth) {
	strcpy(auth_copy, src_server->_http_basic_auth);
	new_server->_http_basic_auth = auth_copy;
    }

 cleanup:
    if (env->fault_occurred) {
        if (url_copy)
            free(url_copy);
        if (auth_copy)
            free(auth_copy);
        if (new_server)
            free(new_server);
        return NULL;
    }
    return new_server;

}

void xmlrpc_server_info_free (xmlrpc_server_info *server)
{
    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT(server->_server_url != XMLRPC_BAD_POINTER);

    if (server->_http_basic_auth)
	free(server->_http_basic_auth);
    free(server->_server_url);
    server->_server_url = XMLRPC_BAD_POINTER;
    free(server);
}

void xmlrpc_server_info_set_basic_auth (xmlrpc_env *env,
					xmlrpc_server_info *server,
					char *username,
					char *password)
{
    size_t username_len, password_len, raw_token_len;
    char *raw_token;
    xmlrpc_mem_block *token;
    char *token_data, *auth_type, *auth_header;
    size_t token_len, auth_type_len, auth_header_len;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(username);
    XMLRPC_ASSERT_PTR_OK(password);

    /* Error-handling preconditions. */
    raw_token = NULL;
    token = NULL;
    auth_header = NULL;

    /* Calculate some lengths. */
    username_len = strlen(username);
    password_len = strlen(password);
    raw_token_len = username_len + password_len + 1;

    /* Build a raw token of the form 'username:password'. */
    raw_token = (char*) malloc(raw_token_len + 1);
    XMLRPC_FAIL_IF_NULL(raw_token, env, XMLRPC_INTERNAL_ERROR,
			"Couldn't allocate memory for auth token");
    strcpy(raw_token, username);
    raw_token[username_len] = ':';
    strcpy(&raw_token[username_len + 1], password);

    /* Encode our raw token using Base64. */
    token = xmlrpc_base64_encode_without_newlines(env,
						  (unsigned char*) raw_token,
						  raw_token_len);
    XMLRPC_FAIL_IF_FAULT(env);
    token_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, token);
    token_len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, token);

    /* Build our actual header value. (I hate string processing in C.) */
    auth_type = "Basic ";
    auth_type_len = strlen(auth_type);
    auth_header_len = auth_type_len + token_len;
    auth_header = (char*) malloc(auth_header_len + 1);
    XMLRPC_FAIL_IF_NULL(auth_header, env, XMLRPC_INTERNAL_ERROR,
			"Couldn't allocate memory for auth header");
    memcpy(auth_header, auth_type, auth_type_len);
    memcpy(&auth_header[auth_type_len], token_data, token_len);
    auth_header[auth_header_len] = '\0';

    /* Clean up any pre-existing authentication information, and install
    ** the new value. */
    if (server->_http_basic_auth)
	free(server->_http_basic_auth);
    server->_http_basic_auth = auth_header;

 cleanup:
    if (raw_token)
	free(raw_token);
    if (token)
	xmlrpc_mem_block_free(token);
    if (env->fault_occurred) {
	if (auth_header)
	    free(auth_header);	
    }
}


/*=========================================================================
**  call_info
**=========================================================================
**  This data structure holds all the information about a particular call,
**  including the XML-RPC information and the lower-level HTTP information.
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
				 xmlrpc_server_info *server,
				 char *method_name,
				 xmlrpc_value *param_array)
{
    call_info *retval;
    HTRqHd request_headers;
    HTStream *target_stream;

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
    info->response_data = XMLRPC_BAD_POINTER;

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
static xmlrpc_value *parse_response_chunk (xmlrpc_env *env,
					   call_info *info);


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

xmlrpc_value * xmlrpc_client_call_server (xmlrpc_env *env,
					  xmlrpc_server_info *server,
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
    retval = xmlrpc_client_call_server_params(env, server, method_name,
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
    xmlrpc_server_info *server;
    xmlrpc_value *retval;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);

    /* Error-handling preconditions. */
    server = NULL;
    retval = NULL;

    /* Build a server info object and make our call. */
    server = xmlrpc_server_info_new(env, server_url);
    XMLRPC_FAIL_IF_FAULT(env);
    retval = xmlrpc_client_call_server_params(env, server, method_name,
					      param_array);
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (server)
	xmlrpc_server_info_free(server);
    if (env->fault_occurred) {
	if (retval)
	    xmlrpc_DECREF(retval);
	return NULL;
    }
    return retval;
}

xmlrpc_value *xmlrpc_client_call_server_params (xmlrpc_env *env,
						xmlrpc_server_info *server,
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
    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    info = NULL;
    retval = NULL;

    /* Create our call_info structure. */
    info = call_info_new(env, server, method_name, param_array);
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
    retval = parse_response_chunk(env, info);
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
				    HTResponse *response ATTR_UNUSED,
				    void *param ATTR_UNUSED,
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
**  parse_response_chunk
**=========================================================================
**  Extract the data from the response buffer, make sure we actually got
**  something, and parse it as an XML-RPC response.
*/

static xmlrpc_value *parse_response_chunk (xmlrpc_env *env, call_info *info)
{
    xmlrpc_value *retval;

    /* Error-handling preconditions. */
    retval = NULL;

    /* Check to make sure that w3c-libwww actually sent us some data.
    ** XXX - This may happen if libwww is shut down prematurely, believe it
    ** or not--we'll get a 200 OK and no data. Gag me with a bogus design
    ** decision. This may also fail if some naughty libwww converter
    ** ate our data unexpectedly. */
    if (!HTChunk_data(info->response_data))
	XMLRPC_FAIL(env, XMLRPC_NETWORK_ERROR, "w3c-libwww returned no data");
    
    /* Parse the response. */
    retval = xmlrpc_parse_response(env, HTChunk_data(info->response_data),
				   HTChunk_size(info->response_data));
    XMLRPC_FAIL_IF_FAULT(env);

 cleanup:
    if (env->fault_occurred) {
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

static int asynch_terminate_handler (HTRequest *request,
				     HTResponse *response,
				     void *param,
				     int status);


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


void xmlrpc_client_call_server_asynch (xmlrpc_server_info *server,
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
    xmlrpc_client_call_server_asynch_params(server, method_name,
					    callback, user_data, param_array);

 cleanup:
    if (env.fault_occurred) {
	(*callback)(server->_server_url, method_name, param_array, user_data,
		    &env, NULL);
    }

    if (param_array)
	xmlrpc_DECREF(param_array);
    xmlrpc_env_clean(&env);
}


void xmlrpc_client_call_asynch_params (char *server_url,
				       char *method_name,
				       xmlrpc_response_handler callback,
				       void *user_data,
				       xmlrpc_value *param_array)
{
    xmlrpc_env env;
    xmlrpc_server_info *server;

    XMLRPC_ASSERT_PTR_OK(server_url);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    server = NULL;

    /* Build a server info object and make our call. */
    server = xmlrpc_server_info_new(&env, server_url);
    XMLRPC_FAIL_IF_FAULT(&env);
    xmlrpc_client_call_server_asynch_params(server, method_name,
					    callback, user_data,
					    param_array);

 cleanup:
    if (server)
	xmlrpc_server_info_free(server);

    if (env.fault_occurred) {
	(*callback)(server_url, method_name, param_array, user_data,
		    &env, NULL);
    }
}


void xmlrpc_client_call_server_asynch_params (xmlrpc_server_info *server,
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

    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_PTR_OK(callback);
    XMLRPC_ASSERT_VALUE_OK(param_array);

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    info = NULL;

    /* Create our call_info structure. */
    info = call_info_new(&env, server, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(&env);
    request = info->request;
    src = info->source_anchor;
    dst = info->dest_anchor;

    /* Add some more data to our call_info struct. */
    call_info_set_asynch_data(&env, info, server->_server_url, method_name,
			      param_array, callback, user_data);
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
	(*callback)(server->_server_url, method_name, param_array, user_data,
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
				     HTResponse *response ATTR_UNUSED,
				     void *param ATTR_UNUSED,
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
    value = parse_response_chunk(&env, info);
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

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
#include <unistd.h> /* for usleep */
#else
#include "xmlrpc_win32_config.h"
#endif

#undef PACKAGE
#undef VERSION

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "xmlrpc.h"
#include "xmlrpc_int.h"
#include "xmlrpc_client.h"
#include "xmlrpc_client_int.h"
/* transport_config.h defines XMLRPC_DEFAULT_TRANSPORT,
    ENABLE_WININET_CLIENT, ENABLE_CURL_CLIENT, ENABLE_LIBWWW_CLIENT 
*/
#include "transport_config.h"

#if ENABLE_WININET_CLIENT
#include "xmlrpc_wininet_transport.h"
#endif
#if ENABLE_CURL_CLIENT
#include "xmlrpc_curl_transport.h"
#endif
#if ENABLE_LIBWWW_CLIENT
#include "xmlrpc_libwww_transport.h"
#endif

/*=========================================================================
**  Initialization and Shutdown
**=========================================================================
*/

transport_client_init    g_transport_client_init = NULL;
transport_client_cleanup g_transport_client_cleanup = NULL;
transport_info_new       g_transport_info_new = NULL;
transport_info_free      g_transport_info_free = NULL;
transport_send_request   g_transport_send_request = NULL;
transport_client_call_server_params g_transport_client_call_server_params = NULL;
transport_finish_asynch g_transport_finish_asynch = NULL;

extern void
xmlrpc_client_init(int    const flags,
                   char * const appname,
                   char * const appversion) {

    struct xmlrpc_clientparms clientparms;

    /* As our interface does not allow for failure, we just fail silently ! */
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    clientparms.transport = XMLRPC_DEFAULT_TRANSPORT;

    xmlrpc_client_init2(&env, flags,
                        appname, appversion,
                        &clientparms, XMLRPC_CPSIZE(transport));
    XMLRPC_FAIL_IF_FAULT(&env);
    
 cleanup:
    xmlrpc_env_clean(&env);
}

const char * 
xmlrpc_client_get_default_transport(xmlrpc_env * const env ATTR_UNUSED) {

    return XMLRPC_DEFAULT_TRANSPORT;
}


static void
setupWininetTransport(void) {

#if ENABLE_WININET_CLIENT
    g_transport_client_init = wininet_transport_client_init;
    g_transport_client_cleanup = wininet_transport_client_cleanup;
    g_transport_info_new = wininet_transport_info_new;
    g_transport_info_free = wininet_transport_info_free;
    g_transport_send_request = wininet_transport_send_request;
    g_transport_client_call_server_params =
        wininet_transport_client_call_server_params;
    g_transport_finish_asynch = wininet_transport_finish_asynch;
#endif
}



static void
setupCurlTransport(void) {
#if ENABLE_CURL_CLIENT
    
    g_transport_client_init = curl_transport_client_init;
    g_transport_client_cleanup = curl_transport_client_cleanup;
    g_transport_info_new = curl_transport_info_new;
    g_transport_info_free = curl_transport_info_free;
    g_transport_send_request = curl_transport_send_request;
    g_transport_client_call_server_params =
        curl_transport_client_call_server_params;
    g_transport_finish_asynch = curl_transport_finish_asynch;
#endif
}



static void
setupLibwwwTransport(void) {
#if ENABLE_LIBWWW_CLIENT

    g_transport_client_init = libwww_transport_client_init;
    g_transport_client_cleanup = libwww_transport_client_cleanup;
    g_transport_info_new = libwww_transport_info_new;
    g_transport_info_free = libwww_transport_info_free;
    g_transport_send_request = libwww_transport_send_request;
    g_transport_client_call_server_params =
        libwww_transport_client_call_server_params;
    g_transport_finish_asynch = libwww_transport_finish_asynch;
#endif
}



void 
xmlrpc_client_init2(xmlrpc_env *                const env,
                    int                         const flags,
                    char *                      const appname,
                    char *                      const appversion,
                    struct xmlrpc_clientparms * const clientparmsP,
                    unsigned int                const parm_size) {

    if (g_transport_client_init == NULL) {
        const char * transport;

        if (parm_size < XMLRPC_CPSIZE(transport) ||
            clientparmsP->transport == NULL) {
            /* He didn't specify a transport.  Use the default */
            transport = xmlrpc_client_get_default_transport(env);
            XMLRPC_FAIL_IF_FAULT(env);
        } else
            transport = clientparmsP->transport;
        
        if (strcmp(transport, "wininet") == 0)
            setupWininetTransport();
        else if (strcmp(transport, "curl") == 0)
            setupCurlTransport();
        else if (strcmp(transport, "libwww") == 0)
            setupLibwwwTransport();

        if (g_transport_client_init)
            g_transport_client_init(flags, appname, appversion);
        else {
            xmlrpc_env_set_fault_formatted(
                env, XMLRPC_INTERNAL_ERROR, 
                "Unknown transport: '%s'", transport);
        }

cleanup:
        if (env->fault_occurred)
            xmlrpc_client_cleanup();
    }
}



void xmlrpc_client_cleanup()
{
    if (g_transport_client_cleanup)
        g_transport_client_cleanup();

    /* Clean up transport init function pointers. */
    g_transport_client_init = NULL;
    g_transport_client_cleanup = NULL;
    g_transport_info_new = NULL;
    g_transport_info_free = NULL;
    g_transport_send_request = NULL;
    g_transport_client_call_server_params = NULL;
    g_transport_finish_asynch = NULL;
}


xmlrpc_value * 
xmlrpc_client_call_params(xmlrpc_env *   const env,
                          const char *   const server_url,
                          const char *   const method_name,
                          xmlrpc_value * const argP) {

    xmlrpc_value *retval;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);

    if (!g_transport_client_init)
        xmlrpc_env_set_fault_formatted(
            env, XMLRPC_INTERNAL_ERROR, 
            "Xmlrpc-c client instance has not been initialized "
            "(need to call xmlrpc_client_init2()).");
    else {
        xmlrpc_server_info *server;
        
        /* Build a server info object and make our call. */
        server = xmlrpc_server_info_new(env, server_url);
        if (!env->fault_occurred) {
            XMLRPC_ASSERT(g_transport_client_call_server_params != NULL);

            retval = g_transport_client_call_server_params(
                env, server, method_name, argP);
            
            xmlrpc_server_info_free(server);
        }
    }
        
    if (!env->fault_occurred)
        XMLRPC_ASSERT_VALUE_OK(retval);

    return retval;
}



static xmlrpc_value * 
xmlrpc_client_call_va(xmlrpc_env * const envP,
                      const char * const server_url,
                      const char * const method_name,
                      const char * const format,
                      va_list            args) {

    xmlrpc_value * argP;
    xmlrpc_value * retval;
    xmlrpc_env argenv;
    const char * suffix;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(format);

    /* Build our argument value. */
    xmlrpc_env_init(&argenv);
    xmlrpc_build_value_va(&argenv, format, args, &argP, &suffix);
    if (argenv.fault_occurred) {
        xmlrpc_env_set_fault_formatted(
            envP, argenv.fault_code, "Invalid arguments.  %s",
            argenv.fault_string);
        xmlrpc_env_clean(&argenv);
    } else {
        XMLRPC_ASSERT_VALUE_OK(argP);

        if (*suffix != '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);
        else {
            /* Perform the actual XML-RPC call. */
            retval = xmlrpc_client_call_params(envP, server_url, method_name,
                                               argP);
            if (!envP->fault_occurred)
                XMLRPC_ASSERT_VALUE_OK(retval);
        }
        xmlrpc_DECREF(argP);
    }
    return retval;
}



xmlrpc_value * 
xmlrpc_client_call(xmlrpc_env * const envP,
                   const char *       const server_url,
                   const char *       const method_name,
                   const char *       const format,
                   ...) {

    xmlrpc_value * result;
    va_list args;

    va_start(args, format);
    result = xmlrpc_client_call_va(envP, server_url,
                                   method_name, format, args);
    va_end(args);

    return result;
}



xmlrpc_value * 
xmlrpc_client_call_server(xmlrpc_env *         const envP,
                          xmlrpc_server_info * const server,
                          const char *         const method_name,
                          const char *         const format, 
                          ...) {

    va_list args;
    xmlrpc_value * argP;
    xmlrpc_value * retval;
    const char * suffix;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(format);

    /* Build our argument */
    va_start(args, format);
    xmlrpc_build_value_va(envP, format, args, &argP, &suffix);
    va_end(args);

    if (!envP->fault_occurred) {
        if (*suffix != '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);
        else {
            retval = g_transport_client_call_server_params(
                envP, server, method_name,
                argP);
        }
        xmlrpc_DECREF(argP);
    }
    return retval;
}


/* Unimplemented, by design.
** You should have better knowledge of your threading to handle
** when the asynch calls have finished. Your callback function
** is _always_ called back whether an error occurred or a successful
** result was returned.
*/
void xmlrpc_client_event_loop_finish_asynch (void)
{
    g_transport_finish_asynch();
}

/* Assume the worst.. That only parts of the call_info are valid. */
void 
call_info_free (call_info * const info) {

    XMLRPC_ASSERT_PTR_OK(info);

    /* Free any transport related info. */
    if (info->transport_info)
        if (g_transport_info_free)
            g_transport_info_free(info->transport_info);

    /* If this has been allocated, we're responsible for destroying it. */
    if (info->_asynch_data_holder)
        xmlrpc_DECREF(info->_asynch_data_holder);

    /* Now we can blow away the XML data. */
    if (info->serialized_xml)
         xmlrpc_mem_block_free(info->serialized_xml);

    free(info);
}



call_info * 
call_info_new(xmlrpc_env *         const envP,
              xmlrpc_server_info * const server,
              const char *         const method_name,
              xmlrpc_value *       const argP) {
/*----------------------------------------------------------------------------
   Create a call_info object.  A call_info object represents an XML-RPC
   call.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * xmlP;
    call_info *retval;

    /* Allocate our structure. */
    retval = (call_info*) malloc(sizeof(call_info));
    XMLRPC_FAIL_IF_NULL(server, envP, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for xmlrpc_call_info");
    /* Clear contents. */
    memset(retval, 0, sizeof(call_info));

    /* Make the XML for our call */
    xmlP = xmlrpc_mem_block_new(envP, 0);
    XMLRPC_FAIL_IF_FAULT(envP);
    xmlrpc_serialize_call(envP, xmlP, method_name, argP);
    XMLRPC_FAIL_IF_FAULT(envP);

    xmlrpc_traceXml("XML-RPC CALL", 
                    XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
                    XMLRPC_MEMBLOCK_SIZE(char, xmlP));
                                                             
    retval->serialized_xml = xmlP;

    XMLRPC_FAIL_IF_NULL(g_transport_info_new, envP, XMLRPC_INTERNAL_ERROR,
                        "Transport uninitialized.");
    retval->transport_info = g_transport_info_new(envP, server, retval);
    XMLRPC_FAIL_IF_FAULT(envP);

 cleanup:
    if (envP->fault_occurred) {
        if (retval)
            call_info_free(retval);
        retval = NULL;
    }
    return retval;
}



static void 
call_info_set_asynch_data(xmlrpc_env *   const env,
                          call_info *    const info,
                          const char *   const server_url,
                          const char *   const method_name,
                          xmlrpc_value * const argP,
                          xmlrpc_response_handler callback,
                          void *         const user_data) {

    xmlrpc_value *holder;

    /* Error-handling preconditions. */
    holder = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(info);
    XMLRPC_ASSERT(info->_asynch_data_holder == NULL);
    XMLRPC_ASSERT_PTR_OK(server_url);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(argP);

    /* Install our callback and user_data.
    ** (We're not responsible for destroying the user_data.) */
    info->callback  = callback;
    info->user_data = user_data;

    /* Build an XML-RPC data structure to hold our other data. This makes
    ** copies of server_url and method_name, and increments the reference
    ** to the argument *argP. */
    holder = xmlrpc_build_value(env, "(ssV)",
                                server_url, method_name, argP);
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
**  xmlrpc_server_info
**=========================================================================
*/

xmlrpc_server_info *
xmlrpc_server_info_new (xmlrpc_env * const env,
                        const char * const server_url) {

    xmlrpc_server_info *server;
    char *url_copy;

    /* Error-handling preconditions. */
    server = NULL;
    url_copy = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);

    /* Allocate our memory blocks. */
    server = (xmlrpc_server_info*) malloc(sizeof(xmlrpc_server_info));
    XMLRPC_FAIL_IF_NULL(server, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for xmlrpc_server_info");
    memset(server, 0, sizeof(xmlrpc_server_info));
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
                                             xmlrpc_server_info *aserver)
{
    xmlrpc_server_info *server;
    char *url_copy, *auth_copy;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(aserver);

    /* Error-handling preconditions. */
    server = NULL;
    url_copy = NULL;
    auth_copy = NULL;

    /* Allocate our memory blocks. */
    server = (xmlrpc_server_info*) malloc(sizeof(xmlrpc_server_info));
    XMLRPC_FAIL_IF_NULL(server, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for xmlrpc_server_info");
    url_copy = (char*) malloc(strlen(aserver->_server_url) + 1);
    XMLRPC_FAIL_IF_NULL(url_copy, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for server URL");
    auth_copy = (char*) malloc(strlen(aserver->_http_basic_auth) + 1);
    XMLRPC_FAIL_IF_NULL(auth_copy, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for authentication info");

    /* Build our object. */
    strcpy(url_copy, aserver->_server_url);
    server->_server_url = url_copy;
    strcpy(auth_copy, aserver->_http_basic_auth);
    server->_http_basic_auth = auth_copy;

    cleanup:
    if (env->fault_occurred) {
        if (url_copy)
            free(url_copy);
        if (auth_copy)
            free(auth_copy);
        if (server)
            free(server);
        return NULL;
    }
    return server;

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

/*=========================================================================
**  xmlrpc_client_call_asynch
**=========================================================================
*/

/* MRB-Eliminated a call to xmlrpc_client_call_asynch_params.
**     This was unnecessary overhead, just to allow for a
**     "...)" version of the xmlrpc_client_call_server_asynch_params call. */
void 
xmlrpc_client_call_asynch(const char * const server_url,
                          const char * const method_name,
                          xmlrpc_response_handler callback,
                          void *       const user_data,
                          const char * const format,
                          ...) {

    xmlrpc_env env;
    va_list args;
    xmlrpc_value * argP;
    xmlrpc_server_info *server;
    const char * suffix;

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    argP = NULL;
    server = NULL;

    XMLRPC_ASSERT_PTR_OK(format);
    XMLRPC_ASSERT_PTR_OK(server_url);

    /* Build our argument array. */
    va_start(args, format);
    xmlrpc_build_value_va(&env, format, args, &argP, &suffix);
    va_end(args);
    XMLRPC_FAIL_IF_FAULT(&env);

    if (*suffix != '\0')
        xmlrpc_env_set_fault_formatted(
            &env, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
            "specifier: '%s'.  There must be exactly one arument.",
            suffix);
    else {
        /* Build a server info object and make our call. */
        server = xmlrpc_server_info_new(&env, server_url);
        XMLRPC_FAIL_IF_FAULT(&env);
        
        /* Perform the actual XML-RPC call. */
        xmlrpc_client_call_server_asynch_params(server, method_name,
                                                callback, user_data, argP);
    }
 cleanup:
    if (server)
        xmlrpc_server_info_free(server);

    if (env.fault_occurred) {
        (*callback)(server_url, method_name, argP, user_data,
                    &env, NULL);
    }

    if (argP)
        xmlrpc_DECREF(argP);
    xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const server,
                                 const char *         const method_name,
                                 xmlrpc_response_handler callback,
                                 void *               const user_data,
                                 const char *         const format,
                                 ...) {

    xmlrpc_env env;
    va_list args;
    xmlrpc_value * argP;
    const char * suffix;

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    argP = NULL;

    XMLRPC_ASSERT_PTR_OK(format);

    /* Build our argument array. */
    va_start(args, format);
    xmlrpc_build_value_va(&env, format, args, &argP, &suffix);
    va_end(args);
    XMLRPC_FAIL_IF_FAULT(&env);

        if (*suffix != '\0')
            xmlrpc_env_set_fault_formatted(
                &env, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);
        else {
            /* Perform the actual XML-RPC call. */
            xmlrpc_client_call_server_asynch_params(server, method_name,
                                                    callback, user_data,
                                                    argP);
        }
 cleanup:
    if (env.fault_occurred) {
        (*callback)(server->_server_url, method_name, argP, user_data,
                    &env, NULL);
    }

    if (argP)
        xmlrpc_DECREF(argP);
    xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_call_server_asynch_params(xmlrpc_server_info * const server,
                                        const char *         const method_name,
                                        xmlrpc_response_handler callback,
                                        void *               const user_data,
                                        xmlrpc_value *       const argP) {
    xmlrpc_env env;
    call_info *info;

    /* Error-handling preconditions. */
    xmlrpc_env_init(&env);
    info = NULL;

    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_PTR_OK(callback);
    XMLRPC_ASSERT_VALUE_OK(argP);

    /* Create our call_info structure. */
    info = call_info_new(&env, server, method_name, argP);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Add some more data to our call_info struct. */
    call_info_set_asynch_data(&env, info, server->_server_url, method_name,
                              argP, callback, user_data);
    XMLRPC_FAIL_IF_FAULT(&env);

    g_transport_send_request(&env, info);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* We are no longer responsible for freeing the call_info now
    ** that your helper has asynchronous control of it. */
    info->asynch_call_is_registered = 1;

 cleanup:
    if (info && !info->asynch_call_is_registered)
        call_info_free(info);
    if (env.fault_occurred) {
        /* Report the error immediately. */
        (*callback)(server->_server_url, method_name, argP, user_data,
                    &env, NULL);
    }
    xmlrpc_env_clean(&env);
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

    /* Error-handling preconditions. */
    raw_token = NULL;
    token = NULL;
    token_data = auth_type = auth_header = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server);
    XMLRPC_ASSERT_PTR_OK(username);
    XMLRPC_ASSERT_PTR_OK(password);

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
    token = xmlrpc_base64_encode_without_newlines(env, (unsigned char*) raw_token,
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


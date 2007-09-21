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
#	include "xmlrpc_config.h"
#else
#	include "xmlrpc_win32_config.h"
#endif

#undef PACKAGE
#undef VERSION

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"
#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc_client.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#if defined (WIN32) && defined(_DEBUG)
#		include <crtdbg.h>
#		define new DEBUG_NEW
#		define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#		undef THIS_FILE
		static char THIS_FILE[] = __FILE__;
#endif /*WIN32 && _DEBUG*/

/* Threading model uses pthreads */
#define Lock()	pthread_mutex_lock (&async_handle_critsec);
#define Unlock() pthread_mutex_unlock (&async_handle_critsec);
#define InitLock() pthread_mutex_init (&async_handle_critsec, NULL);
#define DeleteLock() pthread_mutex_destroy (&async_handle_critsec);

#if WIN32
#define SLEEP(n) { int i; for (i=0; i < n; i++) Sleep(1000); }
#else
#include <unistd.h>
#define SLEEP(n) { int i; for (i=0; i < n; i++) usleep(999999); }
#endif


/* list of running Async callback functions. */
static running_thread_list _runningThreadList = { NULL, NULL };
static pthread_mutex_t async_handle_critsec;

typedef struct _curl_transport_info
{
    /* ie. For libwww, you might have a HTRequest here. */
	pthread_t _thread;
} curl_transport_info;


/* For debugging thread info and curl result codes. */
/* #define tdbg_printf printf */
#define tdbg_printf (void *)

/*=========================================================================
**  transport implementation
**=========================================================================
*/

void curl_transport_client_init(int flags,
                              char * appname,
                              char * appversion)
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);

#if defined (WIN32)
	{
		/* This is CRITICAL so that cURL-Win32 works properly! */
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;
		wVersionRequested = MAKEWORD(1, 1);
     
		err = WSAStartup(wVersionRequested, &wsaData);
		if ( LOBYTE( wsaData.wVersion ) != 1 || 
		   HIBYTE( wsaData.wVersion ) != 1 )
		{
			/* Tell the user that we couldn't find a useable */ 
			/* winsock.dll. */ 
			WSACleanup();
			XMLRPC_FAIL(&env, XMLRPC_INTERNAL_ERROR, "Winsock reported that "
						"it does not support the requested version 1.1.\n");
		}
	}
#endif

	/*
	 * This is the main global constructor for the app. Call this before
	 * _any_ libcurl usage. If this fails, *NO* libcurl functions may be
	 * used, or havoc may be the result.
	 */
	curl_global_init(CURL_GLOBAL_DEFAULT);

	/* Initialize critical section used to guard async callback list. */
	InitLock ();

 cleanup:
    xmlrpc_env_clean(&env);
}

static void cleanup_asynch_thread_list(running_thread_list *list)
{
    void *status;
    int result;
    Lock ();
    while (list->AsyncThreadHead)
    {
        pthread_t thread = list->AsyncThreadHead->_thread;
        //MRB 20010821-Commented out. Let worker unregister it:
        //unregister_asynch_thread (list, &thread);


        Unlock ();


        // MRB-Must unlock, in case the helper calls back
        //     into the xmlrpc_client code.
        result = pthread_join(thread, &status);


        Lock ();
    }
    //MRB 20010821-Commented out. Let worker unregister it:
    //list->AsyncThreadTail = list->AsyncThreadHead = NULL;
    Unlock ();
}

void curl_transport_client_cleanup()
{
	/* Async thread cleanup. */
	cleanup_asynch_thread_list(&_runningThreadList);

	/* Delete callback list critical section */
	DeleteLock ();

	curl_global_cleanup();

#if defined (WIN32)
	WSACleanup();
#endif
}

/* Assume the worst.. That only parts of the call_info are valid.
** Remember, you may only free -your- transport related data only. */
void curl_transport_info_free(void *transport_info)
{
	void * result; /* posix result variable for pthread_join */
	int success; /* posix result code for pthread_join */

    /* _______ Free anything you may have created in transport_prep_info. */

	curl_transport_info *t_info = (curl_transport_info *)transport_info;

    XMLRPC_ASSERT_PTR_OK(transport_info);
	
	if (t_info->_thread)
	{
		pthread_cancel (t_info->_thread);
		success = pthread_join (t_info->_thread, &result);
		pthread_detach (t_info->_thread);
	}

    /* _______ End */
    free(transport_info);
}

/* Perform anything necessary to prepare a request over your given
** transport. Errors are returned through env. */
void *curl_transport_info_new(xmlrpc_env *env,
                                xmlrpc_server_info *server,
                                call_info *call_info)
{
    curl_transport_info *t_info = malloc(sizeof(curl_transport_info));
    XMLRPC_FAIL_IF_NULL(t_info, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for transport_info");

    /* _______ Prep a request to send the xmlrpc over our transport. */

    /* Set up our basic members. */
	t_info->_thread = (pthread_t) NULL;

    /* Unlike the libwww transport, basic authorization is
+   ** handled by client_call_server_params
    */

    /* Report errors with:
    ** XMLRPC_FAIL( env, XMLRPC_INTERNAL_ERROR, "Your fail message." ); */

    /* _______ End */

 cleanup:
    if (env->fault_occurred) {
        g_transport_info_free(t_info);
        t_info = 0;
    }

    return t_info;
}

#if defined (WIN32)
static unsigned __stdcall worker (void * arg)
#else
void * worker(void *arg)
#endif /* WIN32 */
{
    xmlrpc_env env;
    call_info *info = (call_info *)arg;
    curl_transport_info *t_info = (curl_transport_info *)(info->transport_info);
    xmlrpc_value *value;
    
	/* MRB-Pause thread til members of t_info are setup. */
    Lock();
    Unlock();

    tdbg_printf("worker: t_info(0x%08X)->_thread= %08X *(%08X)\n", t_info, &(t_info->_thread),t_info->_thread);

    xmlrpc_env_init( &env );
    value = xmlrpc_client_call_params (&env,
                                       info->server_url,
                                       info->method_name,
                                       info->param_array);
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

	/* Designate that our helper thread has finished. 
	** This means it is safe to leave xmlrpc_client_cleanup. */
	Lock ();
	unregister_asynch_thread (&_runningThreadList, &(t_info->_thread));
	Unlock ();

    /* Free our call_info structure. Again, we need to be careful
    ** about race conditions with xmlrpc_client_call_asynch here. */
    if (info->asynch_call_is_registered)
        call_info_free(info);

    xmlrpc_env_clean(&env);

#if defined (WIN32)
	return 0;
#else
    return NULL;
#endif
}

void curl_transport_finish_asynch(void)
{
        cleanup_asynch_thread_list(&_runningThreadList);
}


/*
** Called for asynchronous requests. Transport vars are prep'd before
** this function is called. Also, the call_info is completely filled in.
*/
void curl_transport_send_request(xmlrpc_env *env, call_info *info)
{
    /* _______ Handle the request at this stage. */
    pthread_t worker_id;
    int success;
    curl_transport_info *t_info = (curl_transport_info *)info->transport_info;

 	/* MRB-This allows for the thread to start, but it blocks
	**     until the member is set by also calling Lock(). */
    Lock ();
    success = pthread_create(&worker_id, NULL, worker, info);
    t_info->_thread = worker_id;
    switch(success)
    {
    case 0: break;
    case EAGAIN:
       XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: System Resources exceeded.");
       break;
    case EINVAL:
       XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: Param Error for attr.");
       break;
    case ENOMEM:
       XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: No memory for new thread.");
       break;
    default:
       XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: Unknown pthread error.");
       break;
    }
    register_asynch_thread(&_runningThreadList, &worker_id);
	Unlock ();

    /* _______ End */
 cleanup:
	return;
}


static size_t collect( void *ptr, size_t  size, size_t  nmemb,  FILE  *stream);

xmlrpc_value * curl_transport_client_call_server_params (xmlrpc_env *env,
                                          xmlrpc_server_info *server,
					  char *method_name,
					  xmlrpc_value *param_array)
{
    char *server_url = server->_server_url;
	char *auth_header = NULL;
	xmlrpc_value *retval;
    xmlrpc_mem_block * serialized_xml;
    xmlrpc_mem_block * response_data;
    struct curl_slist * headerList;
    CURL *curl;
    CURLcode res;
    int attempt;
    long http_result;
    char curlError[CURL_ERROR_SIZE];

    int max_attempt = 3;
    int sleep_s = 1;

    retval = NULL;
    serialized_xml = NULL;
    response_data = NULL;
    headerList = NULL;
    curl = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_PTR_OK(server_url);
    XMLRPC_ASSERT_PTR_OK(method_name);
    XMLRPC_ASSERT_VALUE_OK(param_array);


    /* Serialize our call. */
    serialized_xml = xmlrpc_mem_block_new(env, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    
    xmlrpc_serialize_call(env, serialized_xml, method_name, param_array);
    XMLRPC_FAIL_IF_FAULT(env);

	response_data = xmlrpc_mem_block_new(env, 0);
    XMLRPC_FAIL_IF_FAULT(env);

    curl = curl_easy_init();
    if (curl == NULL)
	{
        XMLRPC_FAIL(env, XMLRPC_INTERNAL_ERROR, "Could not initialize curl");
	}

    curl_easy_setopt(curl, CURLOPT_POST, 1 );
    curl_easy_setopt(curl, CURLOPT_URL, server_url);
    xmlrpc_mem_block_append( env, serialized_xml, "\0", 1 );
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xmlrpc_mem_block_contents( serialized_xml ) );
    curl_easy_setopt(curl, CURLOPT_FILE, response_data );
    curl_easy_setopt(curl, CURLOPT_HEADER, 0 );
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect );
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError );

    headerList = curl_slist_append( headerList, "Content-Type: text/xml" );
    if (headerList == NULL)
	{
        XMLRPC_FAIL( env, XMLRPC_INTERNAL_ERROR, "Could not add header." );
	}

	/* Send an authorization header if we need one. */
	if(server->_http_basic_auth)
	{
		/* Make the authentication header "Authorization: " */
		/* we need 15 + length of _http_basic_auth + 1 for null */

#ifdef WIN32
		auth_header = malloc(strlen(server->_http_basic_auth) + 15 + 1);
#else
		auth_header = malloc(strnlen(server->_http_basic_auth) + 15 + 1);
#endif

		XMLRPC_FAIL_IF_NULL(auth_header, env, XMLRPC_INTERNAL_ERROR,
			"Couldn't allocate memory for Authentication Header");

		memcpy(auth_header,"Authorization: ", 15);
		memcpy(auth_header + 15, server->_http_basic_auth,
#ifdef WIN32
		strlen(server->_http_basic_auth) + 1
#else
		strnlen(server->_http_basic_auth) + 1
#endif
		);

		headerList = curl_slist_append(headerList, auth_header );
		if (headerList == NULL)
		{
			XMLRPC_FAIL( env, XMLRPC_INTERNAL_ERROR,
				"Could not add authentication header." );
		}
		/* clean up the memory we used for the authentication header */
		free(auth_header);
	}

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList );
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1 );
    attempt = 0; res = CURLE_COULDNT_CONNECT;
    while (attempt++ < max_attempt && res == CURLE_COULDNT_CONNECT)
    {
        res = curl_easy_perform(curl);
        tdbg_printf("curl_transport_client_call_server_params: curl_easy_perform result=%d\n", res);
        if (res == CURLE_COULDNT_CONNECT)
            SLEEP(sleep_s);
    }
    if (res != CURLE_OK)
    {
       XMLRPC_FAIL1( env, XMLRPC_NETWORK_ERROR, "Curl error \"%s\"", curlError );
	}

    attempt = 0;
    do
    {
        res = curl_easy_getinfo( curl, CURLINFO_HTTP_CODE, &http_result );
        tdbg_printf("curl_transport_client_call_server_params: curl_easy_getinfo result=%d\n", res);
        if (res == CURLE_COULDNT_CONNECT)
            SLEEP(sleep_s);
    }
	while (++attempt < max_attempt && res == CURLE_COULDNT_CONNECT);

    if (res != CURLE_OK)
	{
        XMLRPC_FAIL1( env, XMLRPC_INTERNAL_ERROR, "Curl error \"%s\"", curlError );
	}

    if (http_result != 200)
    {
		XMLRPC_FAIL1( env, XMLRPC_NETWORK_ERROR, "HTTP response: %d", http_result );
	}

    retval = xmlrpc_parse_response(env, xmlrpc_mem_block_contents(response_data),
                                   xmlrpc_mem_block_size(response_data) );
    XMLRPC_FAIL_IF_FAULT(env);

cleanup:
    if (serialized_xml)
        xmlrpc_mem_block_free(serialized_xml);

    if (response_data)
        xmlrpc_mem_block_free(response_data);

    if (headerList)
        curl_slist_free_all(headerList);

    if (curl)
        curl_easy_cleanup(curl);

    if (env->fault_occurred)
    {
        if (retval)
            xmlrpc_DECREF(retval);
        return NULL;
    }
    return retval;
}

static size_t collect( void *ptr, size_t  size, size_t  nmemb,  FILE  *stream)
{
    xmlrpc_env env;

    xmlrpc_env_init( &env );
    xmlrpc_mem_block_append( &env, (xmlrpc_mem_block *) stream, ptr, nmemb * size );
    XMLRPC_FAIL_IF_FAULT( &env );

    return nmemb * size;

 cleanup:
    return (size_t) -1;

}

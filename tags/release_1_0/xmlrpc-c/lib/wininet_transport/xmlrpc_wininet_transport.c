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

#include <stddef.h>
#include <errno.h>

#if defined (WIN32)
#	include <wininet.h>
#endif /*WIN32*/

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

typedef struct _wininet_transport_info
{
    /* These fields are used when performing synchronous calls. */
    unsigned long http_status;

	BOOL isAsync;

    /* Low-level information used by WinInet. */
	HINTERNET hHttpRequest;
	HINTERNET hURL;

	/* Win32 Thread Handle to deliver Aync Response Message.
	** These should be replaced with a ThreadPool in coming versions */
 	pthread_t _thread;

} wininet_transport_info;


static int saved_flags;
static HINTERNET hSyncInternetSession = NULL;
static HINTERNET hAsyncInternetSession = NULL;

static xmlrpc_value *parse_response_chunk (xmlrpc_env *env, call_info *info);

/* For debugging thread info and curl result codes. */
/* #define tdbg_printf printf */
#define tdbg_printf (void *)

/*=========================================================================
**  transport implementation
**=========================================================================
*/

void wininet_transport_client_init (int flags,
                              char *appname,
                              char *appversion)
{
	/* Initialize critical section used to guard async callback list. */
	InitLock ();

    saved_flags = flags;
	/* Tells the Internet DLL to initialize internal data structures 
	   and prepare for future calls from the application. */
	if (hSyncInternetSession == NULL)
		hSyncInternetSession = InternetOpen (appname,
			INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

	if (hAsyncInternetSession == NULL)
		hAsyncInternetSession = InternetOpen (appname, 
			INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
}

static void cleanup_asynch_thread_list (running_thread_list *list)
{
	Lock ();
	while (list->AsyncThreadHead)
	{
		void *status;
		pthread_t thread = list->AsyncThreadHead->_thread;
		unregister_asynch_thread (list, &thread);

		Unlock ();

		/* MRB-Must unlock, in case the helper calls back
		**     into the xmlrpc_client code. */
		pthread_join (thread, &status);

		Lock ();
	}
	list->AsyncThreadTail = list->AsyncThreadHead = NULL;
	Unlock ();
}

void wininet_transport_client_cleanup ()
{
	/* Async thread cleanup. */
	cleanup_asynch_thread_list (&_runningThreadList);

	/* Delete callback list critical section */
	DeleteLock ();

	if (hSyncInternetSession)
		InternetCloseHandle (hSyncInternetSession);
	hSyncInternetSession = NULL;

	if (hAsyncInternetSession)
		InternetCloseHandle (hAsyncInternetSession);
	hAsyncInternetSession = NULL;

}


/*=========================================================================
**  HTTP Error Reporting
**=========================================================================
**  This code converts an HTTP error from libwww into an XML-RPC fault.
*/

/* Fail if we didn't get a 200 OK message from the remote HTTP server. */
#define FAIL_IF_NOT_200_OK(status,env, request) \
    for (;;) { \
        if ((status) != 200) { \
            set_fault_from_http_request ((env), (status), (request)); \
            goto cleanup; \
        } \
		break; \
    }

static void set_fault_from_http_request (xmlrpc_env *env,
					 int status, HINTERNET request)
{
	unsigned long msgLen = 1024;
	char errMsg [1024];
	*errMsg = 0;
	HttpQueryInfo (request, HTTP_QUERY_STATUS_TEXT, errMsg, &msgLen, NULL);

    /* Set our fault. We break this into multiple lines because it
    ** will generally contain line breaks to begin with. */
    xmlrpc_env_set_fault_formatted (env, XMLRPC_NETWORK_ERROR,
				   "HTTP error #%d occurred\n %s", status, errMsg);

}

/* Assume the worst.. That only parts of the call_info are valid.
** Remember, you may only free -your- transport related data only. */
void wininet_transport_info_free (void *transport_info)
{
	void * result; /* posix result variable for pthread_join */
	int success; /* posix result code for pthread_join */

    /* _______ Free anything you may have created in transport_prep_info. */

	wininet_transport_info *t_info = (wininet_transport_info *)transport_info;

    XMLRPC_ASSERT_PTR_OK (transport_info);

	if (t_info->hHttpRequest)
		InternetCloseHandle (t_info->hHttpRequest);

	if (t_info->hURL)
		InternetCloseHandle (t_info->hURL);

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
void *wininet_transport_info_new (xmlrpc_env *env,
                                xmlrpc_server_info *server,
                                call_info *call_info)
{
    wininet_transport_info *t_info = malloc (sizeof(wininet_transport_info));
    XMLRPC_FAIL_IF_NULL (t_info, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for transport_info");

    /* _______ Prep a request to send the xmlrpc over our transport. */

    /* Set up our basic members. */
	t_info->_thread = NULL;
    t_info->http_status = 0;
	t_info->isAsync = FALSE;
	t_info->hHttpRequest = NULL;
	t_info->hURL = NULL;

	/* Basic Authorization is performed in 
	** wininet_transport_client_call_server_params  */

    /* _______ End */

 cleanup:
    if (env->fault_occurred) {
        g_transport_info_free (t_info);
        t_info = 0;
    }

    return t_info;
}


#if defined (WIN32)
static unsigned __stdcall worker (void * arg)
#else
void * worker (void *arg)
#endif /* WIN32 */
{
    xmlrpc_env env;
    call_info *info = (call_info *)arg;
    wininet_transport_info *t_info = (wininet_transport_info *)info->transport_info;
    xmlrpc_value *value;
    
	/* MRB-Pause thread til members of t_info are setup. */
    Lock();
    Unlock();

    xmlrpc_env_init (&env);
    value = xmlrpc_client_call_params (&env,
                                       info->server_url,
                                       info->method_name,
                                       info->param_array);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Pass the response to our callback function. */
    (*info->callback) (info->server_url, info->method_name, info->param_array,
                      info->user_data, &env, value);

 cleanup:
    if (value)
        xmlrpc_DECREF (value);

    if (env.fault_occurred) {
        /* Pass the fault to our callback function. */
        (*info->callback) (info->server_url, info->method_name,
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
        call_info_free (info);

    xmlrpc_env_clean (&env);

#if defined (WIN32)
	return 0;
#else
    return NULL;
#endif
}

void wininet_transport_finish_asynch (void)
{
    /* MRB-You should define your own job synchronization
           model. You are guaranteed to have your callback invoked,
           either for success or failure, after the user has called
           xmlrpc_client_call_asynch. */
}

/*
** Called for asynchronous requests. Transport vars are prep'd before
** this function is called. Also, the call_info is completely filled in.
*/
void wininet_transport_send_request (xmlrpc_env *env, call_info *info)
{
    /* _______ Handle the request at this stage. */
    pthread_t worker_id;
    int success;
	wininet_transport_info *t_info = (wininet_transport_info *)info->transport_info;

	/* MRB-This allows for the thread to start, but it blocks
	**     until the member is set by also calling Lock(). */
	Lock();
    success = pthread_create(&worker_id, NULL, worker, info);
	t_info->_thread = worker_id;
    switch (success)
    {
    case 0: break;
    case EAGAIN:
       XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: System Resources exceeded.");
       break;
    case EINVAL:
       XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: Param Error for attr.");
       break;
    case ENOMEM:
       XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: No memory for new thread.");
       break;
    default:
       XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "pthread_create failed: Unknown pthread error.");
       break;
    }
	register_asynch_thread (&_runningThreadList, &worker_id);
	Unlock ();

    /* _______ End */
 cleanup:
	return;
}


/* Declare WinInet status callback. */
void CALLBACK statusCallback (HINTERNET hInternet,
                          unsigned long dwContext,
                          unsigned long dwInternetStatus,
                          void * lpvStatusInformation,
                          unsigned long dwStatusInformationLength);

xmlrpc_value * wininet_transport_client_call_server_params (xmlrpc_env *env,
                                          xmlrpc_server_info *server,
					  char *method_name,
					  xmlrpc_value *param_array)
{
	call_info *info;
	wininet_transport_info *t_info;
	xmlrpc_value *retval;

	LPTSTR pMsg = NULL;
	LPVOID pMsgMem = NULL;

	unsigned long lastErr;
	unsigned long reqFlags = INTERNET_FLAG_NO_UI;
	BOOL bOK = FALSE;
	char * auth_header = NULL;
	char * contentType = "Content-Type: text/xml\r\n";
	char * acceptTypes[] = {"text/xml", NULL};
	unsigned long queryLen = sizeof (unsigned long);
	char Scheme[100];
	char Host[255];
	char Path[255];
	char ExtraInfo[255];
	URL_COMPONENTS uc;
	memset (&uc, 0, sizeof (uc));
	uc.dwStructSize = sizeof (uc);
	uc.lpszScheme = Scheme;
	uc.dwSchemeLength = 100;
	uc.lpszHostName = Host;
	uc.dwHostNameLength = 255;
	uc.lpszUrlPath = Path;
	uc.dwUrlPathLength = 255;
	uc.lpszExtraInfo = ExtraInfo;
	uc.dwExtraInfoLength = 255;
     

	XMLRPC_ASSERT_ENV_OK (env);
	XMLRPC_ASSERT_PTR_OK (server);
	XMLRPC_ASSERT_PTR_OK (method_name);
	XMLRPC_ASSERT_VALUE_OK (param_array);

	/* Error-handling preconditions. */
	info = NULL;
	retval = NULL;

	/* Create our call_info structure. */
	info = call_info_new (env, server, method_name, param_array);
	XMLRPC_FAIL_IF_FAULT (env);

	/* Setup our transport-specific data. */
	t_info = (wininet_transport_info *)info->transport_info;

	/* If the URL cannot be parsed then report an INTERNAL ERROR */
	if (InternetCrackUrl (server->_server_url, strlen (server->_server_url),
		ICU_ESCAPE, &uc) == FALSE)
		XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "Unable to parse the URL.");

	uc.nPort = (unsigned short) ((uc.nPort) ? uc.nPort : INTERNET_DEFAULT_HTTP_PORT);
	t_info->hURL = InternetConnect (hSyncInternetSession, 
		uc.lpszHostName, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

	/* Start our request running. */
	if (_strnicmp (uc.lpszScheme, "https", 5) == 0)
		reqFlags |= INTERNET_FLAG_SECURE |INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

	t_info->hHttpRequest = HttpOpenRequest (t_info->hURL, "POST",
		uc.lpszUrlPath, "HTTP/1.1", NULL, (const char **)&acceptTypes,
		reqFlags, 1);

	XMLRPC_FAIL_IF_NULL(t_info->hHttpRequest,env, XMLRPC_INTERNAL_ERROR,
						"Unable to open the requested URL.");

	bOK = HttpAddRequestHeaders (t_info->hHttpRequest, contentType, 
		strlen (contentType), HTTP_ADDREQ_FLAG_ADD|HTTP_ADDREQ_FLAG_REPLACE );

	if (!bOK)
	{
		XMLRPC_FAIL (env, XMLRPC_INTERNAL_ERROR, "Could not set Content-Type.");
	}

	/* Send an authorization header if we need one. */
	if (server->_http_basic_auth)
	{
		/*
		** Make the authentication header "Authorization: "
      	** we need 15 + length of _http_basic_auth + 1 for null 
		*/

		auth_header = malloc(strlen(server->_http_basic_auth) + 15 + 1);

		XMLRPC_FAIL_IF_NULL(auth_header, env, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for Authentication Header");

		memcpy(auth_header,"Authorization: ", 15);
		memcpy(auth_header + 15, server->_http_basic_auth,
			strlen(server->_http_basic_auth) + 1);
		bOK = HttpAddRequestHeaders (t_info->hHttpRequest, auth_header,
			strlen(auth_header), 
			HTTP_ADDREQ_FLAG_ADD|HTTP_ADDREQ_FLAG_REPLACE );

		if (!bOK)
		{
			XMLRPC_FAIL( env, XMLRPC_INTERNAL_ERROR,
				"Could not add authentication header." );
		}
		/* clean up the memory we used for the authentication header */
		free(auth_header);

	}

	/* Provide the user with transport status information */
	InternetSetStatusCallback (t_info->hHttpRequest, statusCallback);

Again:
	/* Send the requested XML remote procedure command  */
	bOK = HttpSendRequest (t_info->hHttpRequest, NULL/*headers*/, 0, 
		xmlrpc_mem_block_contents (info->serialized_xml), 
		xmlrpc_mem_block_size(info->serialized_xml));
	if (!bOK)
	{
		lastErr = GetLastError ();

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS, 
			NULL,
			lastErr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &pMsgMem,
			0, NULL);


		if (pMsgMem == NULL)
		{
			switch (lastErr)
			{
			case ERROR_INTERNET_CANNOT_CONNECT:
				pMsg = "Sync HttpSendRequest failed: Connection refused.";
				break;
			case ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED:
				pMsg = "Sync HttpSendRequest failed: Client authorization certificate needed.";
				break;

			/* The following conditions are recommendations that microsoft 
			** provides in their knowledge base. 
			*/

			// HOWTO: Handle Invalid Certificate Authority Error with WinInet (Q182888) 
			case ERROR_INTERNET_INVALID_CA:
				fprintf (stderr, "Sync HttpSendRequest failed: "
					"The function is unfamiliar with the certificate "
					"authority that generated the server's certificate. ");
				reqFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;

				InternetSetOption (t_info->hHttpRequest, INTERNET_OPTION_SECURITY_FLAGS,
									&reqFlags, sizeof (reqFlags));

				goto Again;
				break;

			// HOWTO: Make SSL Requests Using WinInet (Q168151)
			case ERROR_INTERNET_SEC_CERT_CN_INVALID:
				fprintf (stderr, "Sync HttpSendRequest failed: "
					"The SSL certificate common name (host name field) is incorrect\r\n "
					"for example, if you entered www.server.com and the common name "
					"on the certificate says www.different.com. ");
				
				reqFlags = INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

				InternetSetOption (t_info->hHttpRequest, INTERNET_OPTION_SECURITY_FLAGS,
									&reqFlags, sizeof (reqFlags));

				goto Again;
				break;

			case INTERNET_FLAG_IGNORE_CERT_DATE_INVALID:
				fprintf (stderr, "Sync HttpSendRequest failed: "
					"The SSL certificate date that was received from the server is "
					"bad. The certificate is expired. ");

				reqFlags = INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

				InternetSetOption (t_info->hHttpRequest, INTERNET_OPTION_SECURITY_FLAGS,
									&reqFlags, sizeof (reqFlags));

				goto Again;
				break;

			default:
				pMsg = (LPTSTR)pMsgMem = LocalAlloc (LPTR, MAX_PATH);
				sprintf (pMsg, "Sync HttpSendRequest failed: GetLastError (%d)", lastErr);
				break;

			}
		}
		else
		{
			pMsg = (LPTSTR)(pMsgMem); 

		}
		XMLRPC_FAIL (env, XMLRPC_NETWORK_ERROR, pMsg);

	}

	bOK = HttpQueryInfo (t_info->hHttpRequest, 
		HTTP_QUERY_FLAG_NUMBER|HTTP_QUERY_STATUS_CODE,
		&t_info->http_status, &queryLen, NULL);
	if (!bOK)
	{
		lastErr = GetLastError ();
		FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM, 
			NULL,
			lastErr,
			MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &pMsgMem,
			1024,NULL);

		pMsg = (pMsgMem) ? (LPTSTR)(pMsgMem) : "Sync HttpQueryInfo failed.";
		XMLRPC_FAIL (env, XMLRPC_NETWORK_ERROR, pMsg);

	}


    /* Make sure we got a "200 OK" message from the remote server. */
    FAIL_IF_NOT_200_OK(t_info->http_status, env, t_info->hHttpRequest);

    /* Parse our response. */
    retval = parse_response_chunk (env, info);
    XMLRPC_FAIL_IF_FAULT (env);

 cleanup:
	/* Since the XMLRPC_FAIL calls goto cleanup, we must handle
	** the free'ing of the memory here. */
	if (pMsgMem != NULL)
	{
		LocalFree( pMsgMem );
	}

    if (info)
		call_info_free(info);
    if (env->fault_occurred)
	{
		if (retval)
			xmlrpc_DECREF(retval);
		return NULL;
    }
    return retval;
}

/*=========================================================================
**  parse_response_chunk
**=========================================================================
**  Extract the data from the response buffer, make sure we actually got
**  something, and parse it as an XML-RPC response.
*/

static xmlrpc_value *parse_response_chunk (xmlrpc_env *env, call_info *info)
{
	INTERNET_BUFFERS inetBuffer;
	LPTSTR pMsg = NULL;
	PVOID pMsgMem = NULL;
	unsigned long dwFlags;
	unsigned long dwErr = 0; 
	unsigned long nExpected = 0;
	unsigned long dwLen = sizeof (unsigned long);
	void * body = NULL;
	BOOL bOK = FALSE;
    /* Error-handling preconditions. */
    xmlrpc_value *retval = NULL;

	wininet_transport_info *t_info = (wininet_transport_info *)info->transport_info;


	/* Initialize InternetBuffer */
	inetBuffer.dwStructSize = sizeof (INTERNET_BUFFERS);
	inetBuffer.Next = NULL;
	inetBuffer.lpcszHeader = NULL;
	inetBuffer.dwHeadersTotal = inetBuffer.dwHeadersLength = 0;
	inetBuffer.dwOffsetHigh = inetBuffer.dwOffsetLow = 0;
	inetBuffer.dwBufferLength = 0;

	bOK = HttpQueryInfo (t_info->hHttpRequest, 
		HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER, 
		&inetBuffer.dwBufferTotal, &dwLen, NULL);

	if (!bOK)
	{
		dwErr = GetLastError ();
		FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM, 
			NULL,
			dwErr,
			MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &pMsgMem,
			1024,NULL);

		pMsg = (pMsgMem) ? (LPTSTR)(pMsgMem) : "Sync HttpQueryInfo failed.";
		XMLRPC_FAIL (env, XMLRPC_NETWORK_ERROR, pMsg);

	}

    if (inetBuffer.dwBufferTotal == 0)
		XMLRPC_FAIL (env, XMLRPC_NETWORK_ERROR, "WinInet returned no data");

	body = inetBuffer.lpvBuffer = calloc (inetBuffer.dwBufferTotal, sizeof (TCHAR));
	dwFlags = IRF_SYNC;
	inetBuffer.dwBufferLength = nExpected = inetBuffer.dwBufferTotal;
	InternetQueryDataAvailable (t_info->hHttpRequest, &inetBuffer.dwBufferLength,
		0/* reserved*/, 0/* reserved*/);

	/* Read Response from InternetFile */
	do
	{
		if (inetBuffer.dwBufferLength != 0)
			bOK = InternetReadFileEx (t_info->hHttpRequest, &inetBuffer, dwFlags, 1);

		if (!bOK)
			dwErr = GetLastError ();

		if (dwErr)
		{
			if (dwErr == WSAEWOULDBLOCK || dwErr == ERROR_IO_PENDING) 
			{
				/* Non-block socket operation wait 10 msecs */
				SleepEx (10, TRUE);
				/* Reset dwErr to zero for next pass */
				dwErr = 0;

			}
			else
			{
				FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM, 
					NULL,
					dwErr,
					MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) &pMsgMem,
					1024,NULL);
				pMsg = (pMsgMem) ? (LPTSTR)(pMsgMem) : "ASync InternetReadFileEx failed.";
				XMLRPC_FAIL (env, XMLRPC_NETWORK_ERROR, pMsg);

			}
		}
		
		if (inetBuffer.dwBufferLength)
		{
			TCHAR * bufptr = inetBuffer.lpvBuffer;
			bufptr += inetBuffer.dwBufferLength;
			inetBuffer.lpvBuffer = bufptr;
			nExpected -= inetBuffer.dwBufferLength;
			/* Adjust inetBuffer.dwBufferLength when it is greater than the
			 * expected end of file */ 
 			if (inetBuffer.dwBufferLength > nExpected)
				inetBuffer.dwBufferLength = nExpected; 

		}
		else
			inetBuffer.dwBufferLength = nExpected;
		dwErr = 0;
	} while  (nExpected != 0);


    /* Parse the response. */
    retval = xmlrpc_parse_response (env, body, inetBuffer.dwBufferTotal);
    XMLRPC_FAIL_IF_FAULT (env);

 cleanup:
	/* Since the XMLRPC_FAIL calls goto cleanup, we must handle
	** the free'ing of the memory here. */
	if (pMsgMem != NULL)
	{
		LocalFree( pMsgMem );
	}

	if (body)
		free (body);

    if (env->fault_occurred) {
	if (retval)
	    xmlrpc_DECREF (retval);
	return NULL;
    }
    return retval;
}

/*=========================================================================
**  xmlrpc_client_call_asynch
**=========================================================================
**  An asynchronous XML-RPC client.
*/

/* Use this callback for debugging purposes to track the status of
** your request. */
void CALLBACK statusCallback (HINTERNET hInternet,
                         unsigned long dwContext,
                         unsigned long dwInternetStatus,
                         void * lpvStatusInformation,
                         unsigned long dwStatusInformationLength)
{
	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_RESOLVING_NAME:
		break;

	case INTERNET_STATUS_NAME_RESOLVED:
		break;

	case INTERNET_STATUS_HANDLE_CREATED:
		break;

	case INTERNET_STATUS_CONNECTING_TO_SERVER:
		break;

	case INTERNET_STATUS_REQUEST_SENT:
		break;

	case INTERNET_STATUS_SENDING_REQUEST:
		break;

	case INTERNET_STATUS_CONNECTED_TO_SERVER:
		break;

	case INTERNET_STATUS_RECEIVING_RESPONSE:
		break;

	case INTERNET_STATUS_RESPONSE_RECEIVED:
		break;

	case INTERNET_STATUS_CLOSING_CONNECTION:
		break;

	case INTERNET_STATUS_CONNECTION_CLOSED:
		break;

	case INTERNET_STATUS_HANDLE_CLOSING:
		return;
		break;

	case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		break;

	case INTERNET_STATUS_REDIRECT:
		break;

	case INTERNET_STATUS_REQUEST_COMPLETE:
		/* This indicates the data is ready. */
		break;

	default:
		break;
     }
}

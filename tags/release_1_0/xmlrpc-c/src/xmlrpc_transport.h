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


#ifndef  _XMLRPC_TRANSPORT_H_
#define  _XMLRPC_TRANSPORT_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include "xmlrpc_pthreads.h" /* For threading helpers. */

/*=========================================================================
**  Transport function type declarations.
**=========================================================================
*/
typedef void (*transport_client_init)(int flags,
                                      char *appname,
                                      char *appversion);
    
typedef void (*transport_client_cleanup)(void);

typedef void * (*transport_info_new)(xmlrpc_env *env,
                                     xmlrpc_server_info *server,
                                     call_info *call_info);

typedef void (*transport_info_free)(void *transport_info);

typedef void (*transport_send_request)(xmlrpc_env *env, 
                                       call_info *info);

typedef xmlrpc_value * (*transport_client_call_server_params)(
    xmlrpc_env * env,
    xmlrpc_server_info * server,
    const char * method_name,
    xmlrpc_value *param_array);

typedef void (*transport_finish_asynch)(void);

/*=========================================================================
**  Transport implementation macro definition.

    An individual transport's interface header file can use this to 
    declare all the transport's exported functions.
**=========================================================================
*/
#define DECLARE_TRANSPORT_FUNCTIONS(prefix) \
extern void prefix##_transport_client_init( \
  int flags, \
  char *appname, \
  char *appversion); \
extern void prefix##_transport_client_cleanup(void); \
extern void *prefix##_transport_info_new( \
  xmlrpc_env *env, \
  xmlrpc_server_info *server, \
  call_info *call_info); \
extern void prefix##_transport_info_free( \
  void *transport_info); \
extern void prefix##_transport_send_request( \
  xmlrpc_env *env, \
  call_info *info); \
extern xmlrpc_value * prefix##_transport_client_call_server_params( \
  xmlrpc_env *env, \
  xmlrpc_server_info *server, \
  const char *method_name, \
  xmlrpc_value *param_array); \
extern void prefix##_transport_finish_asynch(void); \



/*=========================================================================
**  Transport Helper Functions and declarations.
**=========================================================================
*/
typedef struct _running_thread_info
{
	struct _running_thread_info * Next;
	struct _running_thread_info * Last;

	pthread_t _thread;
} running_thread_info;


/* list of running Async callback functions. */
typedef struct _running_thread_list
{
	running_thread_info * AsyncThreadHead;
	running_thread_info * AsyncThreadTail;
} running_thread_list;

/* MRB-WARNING: Only call when you have successfully
**     acquired the Lock/Unlock mutex! */
void register_asynch_thread (running_thread_list *list, pthread_t *thread);

/* MRB-WARNING: Only call when you have successfully
**     acquired the Lock/Unlock mutex! */
void unregister_asynch_thread (running_thread_list *list, pthread_t *thread);


#ifdef __cplusplus
}
#endif

#endif

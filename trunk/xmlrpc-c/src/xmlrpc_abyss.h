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


#ifndef  _XMLRPC_ABYSS_H_
#define  _XMLRPC_ABYSS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*=========================================================================
**  XML-RPC Server (based on Abyss)
**=========================================================================
**  An simple XML-RPC server based on the Abyss web server. If errors
**  occur during server setup, the server will exit. In general, if you
**  want to use this API, you'll need to be familiar with Abyss.
**
**  There are two ways to use Abyss:
**    1) You can use the handy wrapper functions.
**    2) You can set up Abyss yourself, and install the appropriate
**       handlers manually.
*/

#define XMLRPC_SERVER_ABYSS_NO_FLAGS (0)


/*=========================================================================
**  Handy Wrapper Functions
**=========================================================================
**  If you don't want to muck around with the internals of Abyss, you'll
**  find these functions quite helpful.
*/

/* Call this function to create a new Abyss webserver with the default
** options. If you've already initialized Abyss, you can instead call
** xmlrpc_server_abyss_init_registry and install the appropriate handlers
** yourself. (See below for more information about our handlers.) */
void 
xmlrpc_server_abyss_init(int          const flags, 
                         const char * const config_file);

/* Start the Abyss webserver running. Under Unix, this routine will attempt
** to do a detaching fork, drop root privileges (if any) and create a pid
** file. Under Windows, this routine merely starts the server.
** This routine never returns.
**
** Once you call this routine, it is illegal to register any more methods. */
void
xmlrpc_server_abyss_run (void);



/* Same as xmlrpc_server_abyss_run(), except you get to specify a "runfirst"
** function.  The server runs this just before executing the actual server
** function, after any daemonizing.  NULL for 'runfirst' means no runfirst
** function.  'runfirstArg' is the argument the server passes to the runfirst
** function.
**/
void 
xmlrpc_server_abyss_run_first(void (runfirst(void *)),
                              void * const runfirstArg);

/*=========================================================================
**  Content Handlers
**=========================================================================
**  These handlers are installed by xmlrpc_server_abyss_init.
**
**  If you start Abyss manually, you will need to #define
**  XMLRPC_SERVER_WANT_ABYSS_HANDLERS and #include abyss.h, before
**  including this header.
*/

#ifdef XMLRPC_SERVER_WANT_ABYSS_HANDLERS

/* Dispatch requests to /RPC2 to the appropriate handler. Install this
** using ServerAddHandler. This handler assumes that it can read from
** the method registry without running into race conditions or anything
** nasty like that. */
extern bool
xmlrpc_server_abyss_rpc2_handler (TSession *r);

/* Return a "404 Not Found" for all requests. Install this using
** ServerDefaultHandler if you don't want to serve any HTML or
** GIFs from your htdocs directory. */
extern bool
xmlrpc_server_abyss_default_handler (TSession *r);

#endif /* XMLRPC_SERVER_WANT_ABYSS_HANDLERS */


/*=========================================================================
**  Method Registry
**=========================================================================
**  Abyss uses an internal method registry. You can access it using
**  these functions.
*/

/* This is called automatically by xmlrpc_server_abyss_init. */
void xmlrpc_server_abyss_init_registry (void);

/* Fetch the internal registry, if you happen to need it. */
extern xmlrpc_registry *
xmlrpc_server_abyss_registry (void);

/* A quick & easy shorthand for adding a method. Depending on
** how you've configured your copy of Abyss, it's probably not safe to
** call this method after calling xmlrpc_server_abyss_run. */
void xmlrpc_server_abyss_add_method (char *method_name,
				     xmlrpc_method method,
				     void *user_data);

/* As above, but provide documentation (see xmlrpc_registry_add_method_w_doc
** for more information). You should really use this one. */
extern void
xmlrpc_server_abyss_add_method_w_doc (char *method_name,
				      xmlrpc_method method,
				      void *user_data,
				      char *signature,
				      char *help);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _XMLRPC_ABYSS_H_ */

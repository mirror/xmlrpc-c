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
** SUCH DAMAGE.
**
** There is more copyright information in the bottom half of this file. 
** Please see it for more details. */

#ifndef HAVE_WIN32_CONFIG_H
#include "xmlrpc_config.h"
#else
#include "xmlrpc_win32_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "xmlrpc.h"

#include "abyss.h"
#define  XMLRPC_SERVER_WANT_ABYSS_HANDLERS
#include "xmlrpc_abyss.h"



/*=========================================================================
**  die_if_fault_occurred
**=========================================================================
**  If certain kinds of out-of-memory errors occur during server setup,
**  we want to quit and print an error.
*/

static void die_if_fault_occurred(xmlrpc_env *env) {
    if (env->fault_occurred) {
        fprintf(stderr, "Unexpected XML-RPC fault: %s (%d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}



/*=========================================================================
**  XML-RPC Server Method Registry
**=========================================================================
**  A simple front-end to our method registry.
*/

/* XXX - This variable is *not* currently threadsafe. Once the server has
** been started, it must be treated as read-only. */
static xmlrpc_registry *registry;

void 
xmlrpc_server_abyss_init_registry(void) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);
    registry = xmlrpc_registry_new(&env);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);
}



xmlrpc_registry *
xmlrpc_server_abyss_registry(void) {
    return registry;
}



/* A quick & easy shorthand for adding a method. */
void 
xmlrpc_server_abyss_add_method (char *        const method_name,
                                xmlrpc_method const method,
                                void *        const user_data) {
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method(&env, registry, NULL, method_name,
                               method, user_data);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);
}



void
xmlrpc_server_abyss_add_method_w_doc (char *        const method_name,
                                      xmlrpc_method const method,
                                      void *        const user_data,
                                      char *        const signature,
                                      char *        const help) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method_w_doc(&env, registry, NULL, method_name,
                                     method, user_data, signature, help);
    die_if_fault_occurred(&env);
    xmlrpc_env_clean(&env);    
}



/*=========================================================================
**  send_xml_data
**=========================================================================
**  Blast some XML data back to the client.
*/

static void 
send_xml_data (TSession * const r, 
               char *     const buffer, 
               uint64     const len) {

    const char * const http_cookie = NULL;
        /* This used to set http_cookie to getenv("HTTP_COOKIE"), but
           that doesn't make any sense -- environment variables are not
           appropriate for this.  So for now, cookie code is disabled.
           - Bryan 2004.10.03.
        */

    /* fwrite(buffer, sizeof(char), len, stderr); */

    /* XXX - Is it safe to chunk our response? */
    ResponseChunked(r);

    ResponseStatus(r, 200);
    
    if (http_cookie) {
        /* There's an auth cookie, so pass it back in the response. */

        char *cookie_response;
 
        cookie_response = malloc(10+strlen(http_cookie));
        sprintf(cookie_response, "auth=%s", http_cookie);
        
        /* Return abyss response. */
        ResponseAddField(r, "Set-Cookie", cookie_response);

        free(cookie_response);
    }   
 
  
    ResponseContentType(r, "text/xml; charset=\"utf-8\"");
    ResponseContentLength(r, len);
    
    ResponseWrite(r);
    
    HTTPWrite(r, buffer, len);
    HTTPWriteEnd(r);
}



/*=========================================================================
**  send_error
**=========================================================================
**  Send an error back to the client. We always return true, so we can
**  be called as the last statement in a handler.
*/

static bool 
send_error(TSession * const r, 
           uint16     const status) {

    ResponseStatus(r, status);
    ResponseError(r);
    return TRUE;
}



/*=========================================================================
**  get_buffer_data
**=========================================================================
**  Extract some data from the TConn's underlying input buffer. Do not
**  extract more than 'max'.
*/

static void
get_buffer_data(TSession * const r, 
                int        const max, 
                char **    const out_start, 
                int *      const out_len) {

    /* Point to the start of our data. */
    *out_start = &r->conn->buffer[r->conn->bufferpos];

    /* Decide how much data to retrieve. */
    *out_len = r->conn->buffersize - r->conn->bufferpos;
    if (*out_len > max)
        *out_len = max;

    /* Update our buffer position. */
    r->conn->bufferpos += *out_len;
}



/*=========================================================================
**  get_body
**=========================================================================
**  Slurp the body of the request into an xmlrpc_mem_block.
*/

static xmlrpc_mem_block *
get_body(xmlrpc_env * const env, 
         TSession *   const r, 
         int          const content_size) {

    xmlrpc_mem_block *body;
    int bytes_read;
    char *chunk_ptr;
    int chunk_len;

    /* Error-handling preconditions. */
    body = NULL;

    /* Allocate a memory block to hold the body. */
    body = xmlrpc_mem_block_new(env, 0);
    XMLRPC_FAIL_IF_FAULT(env);

    bytes_read = 0;

    /* Get our first chunk of data from the buffer. */
    get_buffer_data(r, content_size - bytes_read, &chunk_ptr, &chunk_len);
    bytes_read += chunk_len;
    XMLRPC_TYPED_MEM_BLOCK_APPEND(char, env, body, chunk_ptr, chunk_len);
    XMLRPC_FAIL_IF_FAULT(env);

    while (bytes_read < content_size) {
    
        /* Reset our read buffer & flush data from previous reads. */
        ConnReadInit(r->conn);

        /* Read more network data into our buffer. If we encounter a timeout,
        ** exit immediately.
        ** XXX - We're very forgiving about the timeout here. We allow a
        ** full timeout per network read, which would allow somebody to
        ** keep a connection alive nearly indefinitely. But it's hard to do
        ** anything intelligent here without very complicated code. */
        if (!(ConnRead(r->conn, r->server->timeout)))
            XMLRPC_FAIL(env, XMLRPC_TIMEOUT_ERROR, "POST timed out");

        /* Get the next chunk of data from the buffer. */
        get_buffer_data(r, content_size - bytes_read, &chunk_ptr, &chunk_len);
        bytes_read += chunk_len;
        XMLRPC_TYPED_MEM_BLOCK_APPEND(char, env, body, chunk_ptr, chunk_len);
        XMLRPC_FAIL_IF_FAULT(env);
    }

 cleanup:
    if (env->fault_occurred) {
        if (body)
            xmlrpc_mem_block_free(body);
        return NULL;
    }
    return body;
}



static void
storeCookies(TSession *     const httpRequestP,
             unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
   Get the cookie settings from the HTTP headers and remember them for
   use in responses.
-----------------------------------------------------------------------------*/
    const char * const cookie = RequestHeaderValue(httpRequestP, "cookie");
    if (cookie) {
        /* 
           Setting the value in an environment variable doesn't make
           any sense.  So for now, cookie code is disabled.
           -Bryan 04.10.03.

        setenv("HTTP_COOKIE", cookie, 1);
        */
    }
    /* TODO: parse HTTP_COOKIE to find auth pair, if there is one */

    *httpErrorP = 0;
}




static void
validateContentType(TSession *     const httpRequestP,
                    unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
   If the client didn't specify a content-type of "text/xml", return      
   "400 Bad Request".  We can't allow the client to default this header,
   because some firewall software may rely on all XML-RPC requests
   using the POST method and a content-type of "text/xml". 
-----------------------------------------------------------------------------*/
    const char * const content_type =
        RequestHeaderValue(httpRequestP, "content-type");
    if (content_type == NULL || strcmp(content_type, "text/xml") != 0)
        *httpErrorP = 400;
}



static void
processContentLength(TSession *     const httpRequestP,
                     unsigned int * const inputLenP,
                     unsigned int * const httpErrorP) {
/*----------------------------------------------------------------------------
  Make sure the content length is present and non-zero.  This is
  technically required by XML-RPC, but we only enforce it because we
  don't want to figure out how to safely handle HTTP < 1.1 requests
  without it.  If the length is missing, return "411 Length Required". 
-----------------------------------------------------------------------------*/
    const char * const content_length = 
        RequestHeaderValue(httpRequestP, "content-length");
    if (content_length == NULL)
        *httpErrorP = 411;
    else {
        int const contentLengthValue = atoi(content_length);
        if (contentLengthValue <= 0)
            *httpErrorP = 400;
        else {
            *httpErrorP = 0;
            *inputLenP = (unsigned int)contentLengthValue;
        }
    }
}



/*=========================================================================
**  xmlrpc_server_abyss_rpc2_handler
**=========================================================================
**  This handler processes all requests to '/RPC2'. See the header for
**  more documentation.
*/

bool 
xmlrpc_server_abyss_rpc2_handler (TSession * const r) {

    unsigned int httpError;
    int input_len;
    xmlrpc_env env;
    xmlrpc_mem_block *body, *output;
    char *data;
    size_t size;

    /* We only handle requests to /RPC2, the default XML-RPC URL.
       Everything else we pass through to other handlers. 
    */
    if (strcmp(r->uri, "/RPC2") != 0)
        return FALSE;

    /* We only support the POST method. For anything else, return
       "405 Method Not Allowed". 
    */
    if (r->method != m_post)
        return send_error(r, 405);

    storeCookies(r, &httpError);
    if (httpError)
        return send_error(r, httpError);

    validateContentType(r, &httpError);
    if (httpError)
        return send_error(r, httpError);

    processContentLength(r, &input_len, &httpError);
    if (httpError)
        return send_error(r, httpError);

    /*---------------------------------------------------------------------
      We've made it far enough into this function to set up an error-
      handling context and our error-handling preconditions. 
    */
    xmlrpc_env_init(&env);
    body = output = NULL;

    /* SECURITY: Make sure our content length is legal.
       XXX - We can cast 'input_len' because we know it's >= 0, yes? 
    */
    if ((size_t) input_len > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        XMLRPC_FAIL(&env, XMLRPC_LIMIT_EXCEEDED_ERROR,
                    "XML-RPC request too large");

    /* Read XML data off the wire. */
    body = get_body(&env, r, input_len);
    XMLRPC_FAIL_IF_FAULT(&env);
    
    /* Process the function call. */
    data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, body);
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, body);
    output = xmlrpc_registry_process_call(&env, registry, NULL, data, size);
    XMLRPC_FAIL_IF_FAULT(&env);

    /* Send our the result. */
    data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output);
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    send_xml_data(r, data, size);

 cleanup:
    if (env.fault_occurred) {
        if (env.fault_code == XMLRPC_TIMEOUT_ERROR)
            /* 408 Request Timeout */
            send_error(r, 408);
        else
            /* 500 Internal Server Error */
            send_error(r, 500);
    }

    if (body)
        xmlrpc_mem_block_free(body);
    if (output)
        xmlrpc_mem_block_free(output);

    xmlrpc_env_clean(&env);
    return TRUE;
}



/*=========================================================================
**  xmlrpc_server_abyss_default_handler
**=========================================================================
**  This handler returns a 404 Not Found for all requests. See the header
**  for more documentation.
*/

bool 
xmlrpc_server_abyss_default_handler (TSession * const r) {
    return send_error(r, 404);
}



/**************************************************************************
**
** The code below was adapted from the main.c file of the Abyss webserver
** project. In addition to the other copyrights on this file, the following
** code is also under this copyright:
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
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
** SUCH DAMAGE.
**
**************************************************************************/

#include <time.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
/* Must check this
#include <sys/io.h>
*/
#endif  /* _WIN32 */

#ifdef _UNIX
#include <sys/signal.h>
#include <sys/wait.h>
#include <grp.h>
#endif


#ifdef _UNIX
static void 
sigterm(int const sig) {
    TraceExit("Signal %d received. Exiting...\n",sig);
}
#endif


#ifdef _UNIX
static void 
sigchld(int const sig ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   We respond by reaping the zombie process.

   Implementation note: In some systems, just setting the signal handler
   to SIG_IGN (ignore signal) does this.  In others, it doesn't.
-----------------------------------------------------------------------------*/
    pid_t pid;
    int status;
    
    /* Reap defunct children until there aren't any more. */
    for (;;) {
        pid = waitpid( (pid_t) -1, &status, WNOHANG );
    
        /* none left */
        if (pid==0)
            break;
    
        if (pid<0) {
            /* because of ptrace */
            if (errno==EINTR)   
                continue;
        
            break;
        }
    }
}
#endif /* _UNIX */

static TServer srv;



void 
xmlrpc_server_abyss_init(int          const flags ATTR_UNUSED, 
                         const char * const config_file) {

    xmlrpc_server_abyss_init_registry();

    DateInit();
    MIMETypeInit();

    ServerCreate(&srv, "XmlRpcServer", 8080, DEFAULT_DOCS, NULL);
    
    ConfReadServerFile(config_file, &srv);
    
    ServerAddHandler(&srv, xmlrpc_server_abyss_rpc2_handler);
    ServerDefaultHandler(&srv, xmlrpc_server_abyss_default_handler);
    
    ServerInit(&srv);
}




static void
setupSignalHandlers(void) {
#ifdef _UNIX
    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* These signals abort the program, with tracing */
    mysigaction.sa_handler = sigterm;
    sigaction(SIGTERM, &mysigaction, NULL);
    sigaction(SIGINT,  &mysigaction, NULL);
    sigaction(SIGHUP,  &mysigaction, NULL);
    sigaction(SIGUSR1, &mysigaction, NULL);

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, NULL);
    
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, NULL);
#endif
}    



void 
xmlrpc_server_abyss_run_first(void (runfirst(void *)),
                              void * const runfirstArg) {

    setupSignalHandlers();

#ifdef _UNIX
    /* Become a daemon */
    switch (fork()) {
    case 0:
        break;
    case -1:
        TraceExit("Unable to become a daemon");
    default:
        exit(0);
    };
    
    setsid();

    /* Change the current user if we are root */
    if (getuid()==0) {
        if (srv.uid == (uid_t)-1)
            TraceExit("Can't run under root privileges.  "
                      "Please add a User option in your "
                      "Abyss configuration file.");

#ifdef HAVE_SETGROUPS   
        if (setgroups(0,NULL)==(-1))
            TraceExit("Failed to setup the group.");
        if (srv.gid != (gid_t)-1)
            if (setgid(srv.gid)==(-1))
                TraceExit("Failed to change the group.");
#endif
        
        if (setuid(srv.uid) == -1)
            TraceExit("Failed to change the user.");
    };
    
    if (srv.pidfile!=(-1)) {
        char z[16];
    
        sprintf(z,"%d",getpid());
        FileWrite(&srv.pidfile,z,strlen(z));
        FileClose(&srv.pidfile);
    };
#endif
    
    /* We run the user supplied runfirst after forking, but before accepting
       connections (helpful when running with threads)
    */
    if (runfirst)
        runfirst(runfirstArg);

    ServerRun(&srv);

    /* We can't exist here because ServerRun doesn't return */
    XMLRPC_ASSERT(0);
}



void 
xmlrpc_server_abyss_run(void) {
    xmlrpc_server_abyss_run_first(NULL, NULL);
}

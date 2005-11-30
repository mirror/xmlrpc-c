/* Copyright information is at the end of the file. */

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/abyss.h"
#include "socket.h"
#include "server.h"
#include "conn.h"

/*********************************************************************
** Conn
*********************************************************************/

TConn *
ConnAlloc() {
    return (TConn *)malloc(sizeof(TConn));
}



void
ConnFree(TConn * const connectionP) {
    free(connectionP);
}



static uint32_t
THREAD_ENTRYTYPE ConnJob(TConn * const connectionP) {
/*----------------------------------------------------------------------------
   This is the root function for a thread that processes a connection
   (performs HTTP transactions).
-----------------------------------------------------------------------------*/
    connectionP->connected=TRUE;
    (connectionP->job)(connectionP);
    connectionP->connected=FALSE;
    ThreadExit(&connectionP->thread, 0);
    return 0;
}



abyss_bool
ConnCreate2(TConn *             const connectionP, 
            TServer *           const serverP,
            TSocket             const connectedSocket,
            void            ( *       func)(TConn *),
            enum abyss_foreback const foregroundBackground) {

    abyss_bool retval;
    abyss_bool success;
    uint16_t peerPortNumber;
    
    connectionP->server     = serverP;
    connectionP->socket     = connectedSocket;
    connectionP->buffersize = 0;
    connectionP->bufferpos  = 0;
    connectionP->connected  = TRUE;
    connectionP->job        = func;
    connectionP->inbytes    = 0;
    connectionP->outbytes   = 0;
    connectionP->trace      = getenv("ABYSS_TRACE_CONN");

    SocketGetPeerName(connectedSocket,
                      &connectionP->peerip, &peerPortNumber, &success);

    if (success) {
        retval = FALSE;  /* quiet compiler warning */
        switch (foregroundBackground) {
        case ABYSS_FOREGROUND:
            connectionP->hasOwnThread = FALSE;
            retval = TRUE;
            break;
        case ABYSS_BACKGROUND:
            connectionP->hasOwnThread = TRUE;
            retval = ThreadCreate(&connectionP->thread,
                                  (TThreadProc)ConnJob, 
                                  connectionP);
            break;
        }
    } else
        retval = FALSE;

    return retval;
}



abyss_bool
ConnCreate(TConn *   const connectionP,
           TSocket * const socketP,
           void (*func)(TConn *)) {

    return ConnCreate2(connectionP, connectionP->server, *socketP,
                       func, ABYSS_BACKGROUND);
}



abyss_bool
ConnProcess(TConn * const connectionP) {
    abyss_bool retval;

    if (connectionP->hasOwnThread) {
        /* There's a background thread to handle this connection.  Set
           it running.
        */
        retval = ThreadRun(&(connectionP->thread));
    } else {
        /* No background thread.  We just handle it here while Caller waits. */
        (connectionP->job)(connectionP);
        connectionP->connected = FALSE;
        retval = TRUE;
    }
    return retval;
}



void
ConnClose(TConn * const connectionP) {
    if (connectionP->hasOwnThread)
        ThreadClose(&connectionP->thread);
}



abyss_bool
ConnKill(TConn * connectionP) {
    connectionP->connected = FALSE;
    return ThreadKill(&(connectionP->thread));
}



void
ConnReadInit(TConn * const connectionP) {
    if (connectionP->buffersize>connectionP->bufferpos) {
        connectionP->buffersize -= connectionP->bufferpos;
        memmove(connectionP->buffer,
                connectionP->buffer+connectionP->bufferpos,
                connectionP->buffersize);
        connectionP->bufferpos = 0;
    } else
        connectionP->buffersize=connectionP->bufferpos = 0;

    connectionP->inbytes=connectionP->outbytes = 0;
}



static void
traceSocketRead(const char * const label,
                const char * const buffer,
                unsigned int const size) {

    unsigned int nonPrintableCount;
    unsigned int i;
    
    nonPrintableCount = 0;  /* Initial value */
    
    for (i = 0; i < size; ++i) {
        if (!isprint(buffer[i]) && buffer[i] != '\n' && buffer[i] != '\r')
            ++nonPrintableCount;
    }
    if (nonPrintableCount > 0)
        fprintf(stderr, "%s contains %u nonprintable characters.\n", 
                label, nonPrintableCount);
    
    fprintf(stderr, "%s:\n", label);
    fprintf(stderr, "%.*s\n", (int)size, buffer);
}



abyss_bool
ConnRead(TConn *  const connectionP,
         uint32_t const timeout) {

    while (SocketWait(&(connectionP->socket),TRUE,FALSE,timeout*1000) == 1) {
        uint32_t x;
        uint32_t y;
        
        x = SocketAvailableReadBytes(&connectionP->socket);

        /* Avoid lost connections */
        if (x <= 0)
            break;
        
        /* Avoid Buffer overflow and lost Connections */
        if (x + connectionP->buffersize >= BUFFER_SIZE)
            x = BUFFER_SIZE-connectionP->buffersize - 1;
        
        y = SocketRead(&connectionP->socket,
                       connectionP->buffer + connectionP->buffersize,
                       x);
        if (y > 0) {
            if (connectionP->trace)
                traceSocketRead("READ FROM SOCKET:",
                                connectionP->buffer + connectionP->buffersize,
                                y);

            connectionP->inbytes += y;
            connectionP->buffersize += y;
            connectionP->buffer[connectionP->buffersize] = '\0';
            return TRUE;
        }
        break;
    }
    return FALSE;
}


            
abyss_bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size) {

    abyss_bool failed;

    SocketWrite(&connectionP->socket, buffer, size, &failed);

    if (!failed)
        connectionP->outbytes += size;

    return !failed;
}



abyss_bool
ConnWriteFromFile(TConn *  const connectionP,
                  TFile *  const fileP,
                  uint64_t const start,
                  uint64_t const end,
                  void *   const buffer,
                  uint32_t const buffersizeArg,
                  uint32_t const rate) {

    abyss_bool retval;
    uint32_t waittime;
    abyss_bool success;
    uint32_t buffersize;

    if (rate > 0) {
        buffersize = MIN(buffersizeArg, rate);
        waittime = (1000 * buffersize) / rate;
    } else {
        buffersize = buffersizeArg;
        waittime = 0;
    }

    success = FileSeek(fileP, start, SEEK_SET);
    if (!success)
        retval = FALSE;
    else {
        uint64_t bytesread;

        bytesread = 0;  /* initial value */

        while (bytesread <= end - start) {
            uint64_t const bytesToRead = MIN(buffersize,
                                             (end - start + 1) - bytesread);
            uint64_t bytesReadThisTime;

            bytesReadThisTime = FileRead(fileP, buffer, bytesToRead);
            bytesread += bytesReadThisTime;
            
            if (bytesReadThisTime > 0)
                ConnWrite(connectionP, buffer, bytesReadThisTime);
            else
                break;
            
            if (waittime > 0)
                ThreadWait(waittime);
        }
        retval = (bytesread > end - start);
    }
    return retval;
}



abyss_bool
ConnReadLine(TConn * const connectionP,
             char ** const z) {

    uint32_t const timeout = connectionP->server->srvP->timeout;

    char * p;
    char * t;
    abyss_bool first;
    clock_t to;
    clock_t start;

    p = *z = connectionP->buffer + connectionP->bufferpos;
    start = clock();

    first = TRUE;

    for (;;) {
        to = (clock() - start) / CLOCKS_PER_SEC;
        if ((uint32_t)to > timeout)
            break;

        if (first) {
            if (connectionP->bufferpos >= connectionP->buffersize)
                if (!ConnRead(connectionP, timeout-to))
                    break;

            first = FALSE;
        } else {
            if (!ConnRead(connectionP, timeout-to))
                break;
        }

        t = strchr(p, LF);
        if (t) {
            if ((*p != LF) && (*p != CR)) {
                if (!*(t+1))
                    continue;

                p = t;

                if ((*(p+1) == ' ') || (*(p+1) == '\t')) {
                    if (p > *z)
                        if (*(p-1) == CR)
                            *(p-1) = ' ';

                    *(p++) = ' ';
                    continue;
                }
            } else {
                /* emk - 04 Jan 2001 - Bug fix to leave buffer
                ** pointing at start of body after reading blank
                ** line following header. */
                p = t;
            }

            connectionP->bufferpos += p + 1 - *z;

            if (p > *z)
                if (*(p-1) == CR)
                    --p;

            *p = '\0';
            return TRUE;
        }
    }
    return FALSE;
}



TServer *
ConnServer(TConn * const connectionP) {
    return connectionP->server;
}



/*******************************************************************************
**
** conn.c
**
** This file is part of the ABYSS Web server project.
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
******************************************************************************/

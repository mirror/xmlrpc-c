/* Copyright information is at the end of the file. */

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

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
traceBuffer(const char * const label,
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



static void
traceSocketRead(TConn *      const connectionP,
                unsigned int const size) {

    if (connectionP->trace)
        traceBuffer("READ FROM SOCKET:",
                    connectionP->buffer + connectionP->buffersize, size);
}



static void
traceSocketWrite(TConn *      const connectionP,
                 const char * const buffer,
                 unsigned int const size,
                 abyss_bool   const failed) {

    if (connectionP->trace) {
        const char * const label =
            failed ? "FAILED TO WRITE TO SOCKET:" : "WROTE TO SOCKET";
        traceBuffer(label, buffer, size);
    }
}



static uint32_t
bufferSpace(TConn * const connectionP) {
    
    return BUFFER_SIZE - connectionP->buffersize;
}
                    


abyss_bool
ConnRead(TConn *  const connectionP,
         uint32_t const timeout) {
/*----------------------------------------------------------------------------
   Read some stuff on connection *connectionP from the socket.

   Don't wait more than 'timeout' seconds for data to arrive.  Fail if
   nothing arrives within that time.
-----------------------------------------------------------------------------*/
    time_t const deadline = time(NULL) + timeout;

    abyss_bool cantGetData;
    abyss_bool gotData;

    cantGetData = FALSE;
    gotData = FALSE;
    
    while (!gotData && !cantGetData) {
        int const timeLeft = deadline - time(NULL);

        if (timeLeft <= 0)
            cantGetData = TRUE;
        else {
            int rc;
            
            rc = SocketWait(&connectionP->socket, TRUE, FALSE,
                            timeLeft * 1000);
            
            if (rc != 1)
                cantGetData = TRUE;
            else {
                uint32_t bytesAvail;
            
                bytesAvail = SocketAvailableReadBytes(&connectionP->socket);
                
                if (bytesAvail <= 0)
                    cantGetData = TRUE;
                else {
                    uint32_t const bytesToRead =
                        MIN(bytesAvail, bufferSpace(connectionP)-1);

                    uint32_t bytesRead;

                    bytesRead = SocketRead(
                        &connectionP->socket,
                        connectionP->buffer + connectionP->buffersize,
                        bytesToRead);
                    if (bytesRead > 0) {
                        traceSocketRead(connectionP, bytesRead);
                        connectionP->inbytes += bytesRead;
                        connectionP->buffersize += bytesRead;
                        connectionP->buffer[connectionP->buffersize] = '\0';
                        gotData = TRUE;
                    }
                }
            }
        }
    }
    if (gotData)
        return TRUE;
    else
        return FALSE;
}


            
abyss_bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size) {

    abyss_bool failed;

    SocketWrite(&connectionP->socket, buffer, size, &failed);

    traceSocketWrite(connectionP, buffer, size, failed);

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



static void
processHeaderLine(char *       const start,
                  const char * const headerStart,
                  abyss_bool * const gotHeaderP,
                  char **      const nextP) {
/*----------------------------------------------------------------------------
  If there's enough data in the buffer at *pP, process a line of HTTP
  header.

  It is part of a header that starts at 'headerStart' and has been
  previously processed up to *pP.  The data in the buffer is
  terminated by a NUL.

  WE MODIFY THE DATA.

  Process means:

     - Determine whether more data from the socket is needed to get a full
       header (or to determine that we've already one -- note that we may
       have to see the next line to know if it's a continuation line).

       Return the determination as *gotHeaderP.

     - blank out the first line delimiter (LF or CRLF) if we know there
       is a continuation line after it (blanking out the delimiter fuses
       the two lines).  In that case, move the cursor *pP to point to
       continuation line.

     - If there's a full header at 'lineStart' now, replace the final line
       delimiter (LF or CRLF) with a NUL and make the cursor *pP
       point to the buffer content following the header and its line
       delimiter.
-----------------------------------------------------------------------------*/
    abyss_bool gotHeader;
    char * lfPos;
    char * p;

    p = start;

    gotHeader = FALSE;  /* initial assumption */

    lfPos = strchr(p, LF);
    if (lfPos) {
        if ((*p != LF) && (*p != CR)) {
            /* We're looking at a non-empty line */
            if (*(lfPos+1) == '\0') {
                /* There's nothing in the buffer after the line, so we
                   don't know if there's a continuation line coming.
                   Must read more.
                */
            } else {
                p = lfPos; /* Point to LF */
                
                /* If the next line starts with whitespace, it's a
                   continuation line, so blank out the line
                   delimiter (LF or CRLF) so as to join the next
                   line with this one.
                */
                if ((*(p+1) == ' ') || (*(p+1) == '\t')) {
                    if (p > headerStart && *(p-1) == CR)
                        *(p-1) = ' ';
                    *p++ = ' ';
                } else
                    gotHeader = TRUE;
            }
        } else {
            /* We're looking at an empty line (i.e. what marks the
               end of the header)
            */
            p = lfPos;  /* Point to LF */
            gotHeader = TRUE;
        }
    }

    if (gotHeader) {
        /* 'p' points to the final LF */

        /* Replace the LF or the CR in CRLF with NUL, so as to terminate
           the string at 'headerStart' that is the full header.
        */
        if (p > headerStart && *(p-1) == CR)
            *(p-1) = '\0';  /* NUL out CR in CRLF */
        else
            *p = '\0';  /* NUL out LF */

        ++p;  /* Point to next line in buffer */
    }
    *gotHeaderP = gotHeader;
    *nextP = p;
}



abyss_bool
ConnReadHeader(TConn * const connectionP,
               char ** const headerP) {
/*----------------------------------------------------------------------------
   Read an HTTP header on connection *connectionP.

   An HTTP header is basically a line, except that if a line starts
   with white space, it's a continuation of the previous line.  A line
   is delimited by either LF or CRLF.

   In the course of reading, we read at least one character past the
   line delimiter at the end of the header; we may read much more.  We
   leave everything after the header (and its line delimiter) in the
   internal buffer, with the buffer pointer pointing to it.

   We use stuff already in the internal buffer (perhaps left by a
   previous call to this subroutine) before reading any more from from
   the socket.

   Return as *headerP the header value.  This is in the connection's
   internal buffer.  This contains no line delimiters.
-----------------------------------------------------------------------------*/
    uint32_t const deadline = time(NULL) + connectionP->server->srvP->timeout;

    abyss_bool retval;
    char * p;
    char * headerStart;
    abyss_bool error;
    abyss_bool gotHeader;

    p = connectionP->buffer + connectionP->bufferpos;
    headerStart = p;

    gotHeader = FALSE;
    error = FALSE;

    while (!gotHeader && !error) {
        int const timeLeft = deadline - time(NULL);

        if (timeLeft <= 0)
            error = TRUE;
        else {
            if (p >= connectionP->buffer + connectionP->buffersize)
                /* Need more data from the socket to chew on */
                error = !ConnRead(connectionP, timeLeft);

            if (!error) {
                assert(connectionP->buffer + connectionP->buffersize > p);
                processHeaderLine(p, headerStart, &gotHeader, &p);
            }
        }
    }
    if (gotHeader) {
        /* We've consumed this part of the buffer (but be careful --
           you can't reuse that part of the buffer because the string
           we're returning is in it!
        */
        connectionP->bufferpos += p - headerStart;
        *headerP = headerStart;
        retval = TRUE;
    } else
        retval = FALSE;

    return retval;
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

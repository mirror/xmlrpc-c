#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "bool.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"
#include "server.h"
#include "http.h"
#include "conn.h"

#include "session.h"



static void
refillBuffer(TSession *    const sessionP,
             const char ** const errorP) {

    struct _TServer * const srvP = sessionP->connP->server->srvP;

    /* Reset our read buffer and flush data from previous reads. */
    ConnReadInit(sessionP->connP);

    *errorP = NULL;  /* initial assumption */

    if (sessionP->continueRequired) {
        bool succeeded;
        succeeded = HTTPWriteContinue(sessionP);
        if (!succeeded)
            xmlrpc_asprintf(errorP, "Failed to send a Continue header "
                            "to the client to tell it to go ahead with "
                            "sending the body");
    }
    if (!*errorP) {
        const char * error;

        sessionP->continueRequired = false;

        /* Read more network data into our buffer.  Fail if we time out
           before client sends any data or client closes the connection or
           there's some network error.  We're very forgiving about the
           timeout here.  We allow a full timeout per network read, which
           would allow somebody to keep a connection alive nearly
           indefinitely.  But it's hard to do anything intelligent here
           without very complicated code.
        */
        ConnRead(sessionP->connP, srvP->timeout, NULL, NULL, &error);
        if (error) {
            xmlrpc_asprintf(errorP, "Failed to get more data from "
                            "the client.  %s", error);
            xmlrpc_strfree(error);
        }
    }
}



abyss_bool
SessionRefillBuffer(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Get the next piece of HTTP request body from the connection into the
   buffer.  Add at least a byte to the buffer; wait for the data (subject to
   the server's timeout parameters) as necessary.
   
   I.e. read data from the socket.

   "piece" is not any formal part of the body - its just some arbitrary
   amount of data - at least one byte - that we choose to read.

   We fail if the session is in a failed state.  Because the methods that
   extract data from the buffer were designed without the ability to fail,
   this is often the only way we have to communicate to the Abyss user that
   things have gone pear-shape.  And even with this, we are unable to tell
   the user why - we just have a binary "failed" return value.
-----------------------------------------------------------------------------*/
    bool succeeded;

    if (sessionP->failureReason)
        succeeded = false;
    else {
        const char * error;

        refillBuffer(sessionP, &error);

        if (error) {
            sessionP->failureReason = error;
            succeeded = false;
        } else
            succeeded = true;
    }
    return succeeded;
}



size_t
SessionReadDataAvail(TSession * const sessionP) {

    return sessionP->connP->buffersize - sessionP->connP->bufferpos;

}



static const char *
firstLfPos(TConn * const connectionP) {
/*----------------------------------------------------------------------------
   Return a pointer in the connection's receive buffer to the first
   LF (linefeed aka newline) character in the buffer.

   If there is no LF in the buffer, return NULL.
-----------------------------------------------------------------------------*/
    const char * const bufferStart =
        connectionP->buffer.t + connectionP->bufferpos;
    const char * const bufferEnd =
        connectionP->buffer.t + connectionP->buffersize;

    const char * p;

    for (p = bufferStart; p < bufferEnd && *p != '\n'; ++p);

    if (p < bufferEnd)
        return p;
    else
        return NULL;
}



static void
getLine(TConn *       const connectionP,
        const char ** const lineP,
        const char ** const errorP) {
/*----------------------------------------------------------------------------
   Extract one line from the connection buffer.

   If there is a full line in the buffer now, including line end delimiter, at
   the current position, return it as *lineP in newly malloced storage and
   advance the current buffer position past the line.

   Do not include the line end delimiter in the returned line.

   If there is no full line, return NULL as *lineP.

   But if there is no full line and the connection buffer is full (so that
   there won't ever be a full line), fail.
-----------------------------------------------------------------------------*/
    const char * lfPos;

    lfPos = firstLfPos(connectionP);

    if (lfPos) {
        unsigned int const lfIdx = lfPos - connectionP->buffer.t;

        char * line;
        assert(lfIdx > connectionP->bufferpos);
        line = malloc(lfIdx - connectionP->bufferpos + 1);
        if (!line)
            xmlrpc_asprintf(errorP, "Memory allocation failed for small "
                            "buffer to read a line from the client");
        else {
            unsigned int i, j;
            for (i = connectionP->bufferpos, j = 0; i < lfIdx; ++i, ++j)
                line[j] = connectionP->buffer.t[i];
            line[j] = '\0';

            /* Chop off the CR */
            if (j > 1 && line[j-1] == '\r')
                line[j-1] = '\0';

            connectionP->bufferpos = lfIdx + 1;
            
            *errorP = NULL;
            *lineP = line;
        }
    } else {
        unsigned int const usedByteCt =
            connectionP->buffersize - connectionP->bufferpos;

        if (usedByteCt + 1 >= BUFFER_SIZE) {
            /* + 1 for a NUL.

               We don't have a full line yet, and a buffer refill won't
               help because the buffer is full.
            */
            xmlrpc_asprintf(errorP, "Line received from client does not "
                            "fit in the server's connection buffer.");
        } else {
            *errorP = NULL;
            *lineP = NULL;
        }
    }
}



static void
parseHexDigit(char           const digit,
              unsigned int * const valueP,
              const char **  const errorP) {

    *errorP = NULL;  /* initial assumption */

    switch (toupper(digit)) {
    case '0': *valueP = 0x0; break;
    case '1': *valueP = 0x1; break;
    case '2': *valueP = 0x2; break;
    case '3': *valueP = 0x3; break;
    case '4': *valueP = 0x4; break;
    case '5': *valueP = 0x5; break;
    case '6': *valueP = 0x6; break;
    case '7': *valueP = 0x7; break;
    case '8': *valueP = 0x8; break;
    case '9': *valueP = 0x9; break;
    case 'A': *valueP = 0xA; break;
    case 'B': *valueP = 0xB; break;
    case 'C': *valueP = 0xC; break;
    case 'D': *valueP = 0xD; break;
    case 'E': *valueP = 0xE; break;
    case 'F': *valueP = 0xF; break;
    default:
        xmlrpc_asprintf(errorP, "Invalid hex digit: 0x%02x", (unsigned)digit);
    }
}



static void
parseChunkHeader(const char *  const line,
                 uint32_t *    const chunkSizeP,
                 const char ** const errorP) {

    /* The chunk header is normally just a hexadecimal coding of the chunk
       size, but that can theoretically be followed by a semicolon, and then a
       "chunk extension."  The standard says a server must ignore a chunk
       extension it does not understand, which for us is all chunk extensions.
    */

    char * const semPos = strchr(line, ';');

    const char * const end = semPos ? semPos : line + strlen(line);
        /* Points just past the end of the chunk size */

    unsigned int accum;
    const char * p;

    for (accum = 0, p = &line[0], *errorP = NULL;
         p < end && !*errorP; ++p) {

        unsigned int digitValue;
        
        parseHexDigit(*p, &digitValue, errorP);
        
        if (!*errorP) {
            accum <<= 4;
            accum += digitValue;
        }
    }    
    *chunkSizeP = accum;
}



static void
getChunkHeader(TSession *    const sessionP, 
               bool *        const gotHeaderP,
               uint32_t *    const chunkSizeP,
               const char ** const errorP) {
/*-----------------------------------------------------------------------------
   Assuming the stream from the client is positioned to the beginning of a
   chunk header (the line that contains the chunk size and chunk
   extension), read off the line and return as *chunkSizeP the chunk size
   it indicates.

   Iff a whole line is not in the buffer, leave the stream alone and return
   *gotHeaderP == false.
-----------------------------------------------------------------------------*/
    const char * line;
    const char * error;

    getLine(sessionP->connP, &line, &error);

    if (error) {
        xmlrpc_asprintf(errorP, "Failed trying to process a chunk header.  %s",
                        error);
        xmlrpc_strfree(error);
    } else {
        if (line) {
            parseChunkHeader(line, chunkSizeP, errorP);

            if (!*errorP) {
                *gotHeaderP = true;
            }
            xmlrpc_strfree(line);
        } else {
            *errorP = NULL;
            *gotHeaderP = false;
        }
    }
}



static void
processChunkDelimIfThere(TSession *    const sessionP,
                         const char ** const errorP) {
/*-----------------------------------------------------------------------------
   Read off a chunk delimiter (CRLF) from the connection buffer, if there
   is one there.

   Fail if there is something inconsistent with CRLF there.

   Do nothing if there is nothing or just part of CRLF there; Caller can
   try again later when there is more data.
-----------------------------------------------------------------------------*/
    uint32_t const byteCt =
        sessionP->connP->buffersize - sessionP->connP->bufferpos;

    if (byteCt < 1) {
        *errorP = NULL;
    } else {
        const char * const next = 
            &sessionP->connP->buffer.t[sessionP->connP->bufferpos];

        if (next[0] != '\r')
            xmlrpc_asprintf(errorP, "Garbage where chunk delimiter should be");
        if (byteCt < 2)
            *errorP = NULL;
        else {
            if (next[1] != '\n')
                xmlrpc_asprintf(errorP,
                                "Garbage where chunk delimiter should be");
            else {
                *errorP = NULL;
                sessionP->chunkState.position = CHUNK_ATHEADER;
                sessionP->connP->bufferpos += 2;
            }
        }
    }
}



static void
processChunkHeaderIfThere(TSession *    const sessionP,
                          const char ** const errorP) {

    bool gotHeader;
    uint32_t chunkSize;

    getChunkHeader(sessionP, &gotHeader, &chunkSize, errorP);

    if (!*errorP) {
        if (gotHeader) {
            if (chunkSize == 0)
                sessionP->chunkState.position = CHUNK_EOF;
            else {
                sessionP->chunkState.position = CHUNK_INCHUNK;
                sessionP->chunkState.bytesLeftCt = chunkSize;
            }
        }
    }
}



static void
getChunkData(TSession *    const sessionP,
             size_t        const max,
             const char ** const outStartP, 
             size_t *      const outLenP) {
                 
    uint32_t const bufferPos = sessionP->connP->bufferpos;

    uint32_t const bytesInBufferCt = 
        sessionP->connP->buffersize - sessionP->connP->bufferpos;

    uint32_t const outLen =
        MIN(MIN(sessionP->chunkState.bytesLeftCt,
                bytesInBufferCt),
            max);

    *outStartP = &sessionP->connP->buffer.t[bufferPos];

    sessionP->connP->bufferpos += outLen;

    sessionP->chunkState.bytesLeftCt -= outLen;

    if (sessionP->chunkState.bytesLeftCt == 0)
        sessionP->chunkState.position = CHUNK_AFTERCHUNK;

    *outLenP = outLen;
}



static void
getSomeChunkedRequestBody(TSession *    const sessionP, 
                          size_t        const max, 
                          abyss_bool *  const eofP,
                          const char ** const outStartP, 
                          size_t *      const outLenP,
                          const char ** const errorP) {
/*-----------------------------------------------------------------------------
  Same as SessionGetReadData, but assuming the client is sending the body
  in chunked form.

  Iff we encounter the end of body indication (a zero-size chunk),
  return *eofP true and nothing as *outStartP or *outLenP.
-----------------------------------------------------------------------------*/
    /* In chunked form, the client sends a sequence of chunks.  Each chunk is
       introduced by a one-line header.  That header tells the size of the
       chunk.  The end of the body is delimited by a zero-length chunk.
    */
    if (sessionP->chunkState.position == CHUNK_EOF) {
        xmlrpc_asprintf(errorP, "The client has previously sent an "
                        "end-of-data indication (zero-length chunk)");
    } else {
        if (sessionP->chunkState.position == CHUNK_AFTERCHUNK) {
            processChunkDelimIfThere(sessionP, errorP);
        }
        if (!*errorP) {
            if (sessionP->chunkState.position == CHUNK_ATHEADER) {
                processChunkHeaderIfThere(sessionP, errorP);
            }
        }
        if (!*errorP) {
            if (sessionP->chunkState.position == CHUNK_INCHUNK) {
                *eofP = false;
                getChunkData(sessionP, max, outStartP, outLenP);
            } else if (sessionP->chunkState.position == CHUNK_EOF) {
                *eofP = true;
            } else {
                *eofP      = false;
                *outLenP   = 0;
                *outStartP = NULL;
            }
        }
    }
}



static void
getSomeUnchunkedRequestBody(TSession *    const sessionP, 
                            size_t        const max, 
                            const char ** const outStartP, 
                            size_t *      const outLenP) {
/*-----------------------------------------------------------------------------
   Same as SessionGetReadData, but assuming the client is just sending a
   plain stream of bytes for the body; not chunked.
-----------------------------------------------------------------------------*/
    uint32_t const bufferPos = sessionP->connP->bufferpos;

    *outStartP = &sessionP->connP->buffer.t[bufferPos];

    assert(bufferPos <= sessionP->connP->buffersize);

    *outLenP = MIN(max, sessionP->connP->buffersize - bufferPos);

    /* move pointer past the bytes we are returning */
    sessionP->connP->bufferpos += *outLenP;

    assert(sessionP->connP->bufferpos <= sessionP->connP->buffersize);
}



void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP) {
/*----------------------------------------------------------------------------
   Extract some HTTP request body which the server has read and buffered for
   the session.  Don't get or wait for any data that has not yet arrived.  Do
   not return more than 'max'.

   We return a pointer to the first byte as *outStartP, and the length in
   bytes as *outLenP.  The memory pointed to belongs to the session.

   Iff the request body is chunked, we can be unable to do this (e.g. the
   chunk header is invalid).  But for historical reasons, we can't fail, so we
   just put the session in a failed state and tell Caller there is no data,
   and when Caller next calls SessionRefillBuffer, he will find out.
-----------------------------------------------------------------------------*/
    if (sessionP->requestIsChunked) {
        abyss_bool eof;
        getSomeChunkedRequestBody(sessionP, max, &eof, outStartP, outLenP,
                                  &sessionP->failureReason);
        if (eof)
            xmlrpc_asprintf(&sessionP->failureReason,
                            "End of request body encountered");
    } else {
        getSomeUnchunkedRequestBody(sessionP, max, outStartP, outLenP);
    }
}



void
SessionGetBody(TSession *    const sessionP, 
               size_t        const max, 
               abyss_bool *  const eofP,
               const char ** const outStartP, 
               size_t *      const outLenP,
               const char ** const errorP) {
/*-----------------------------------------------------------------------------
   Get some of the HTTP request body.

   If there is some request body in the connection buffer already, return that
   immediately.  If not, wait for some to arrive and return that.  Don't wait
   for more than one byte (but return more if possible without waiting), and
   don't return more than 'max' bytes.

   We return a pointer to the first byte as *outStartP, and the length in
   bytes as *outLenP.  The memory pointed to belongs to the session.

   Iff the data indicates end-of-body (possible only when the body is chunked),
   return *eofP == true and nothing as *outStartP and *outLenP.

   Assume the session has already received and processed the HTTP header.
-----------------------------------------------------------------------------*/
    if (sessionP->failureReason)
        xmlrpc_asprintf(errorP, "The session has previously failed: %s",
                        sessionP->failureReason);
    else {
        size_t       outLen;
        abyss_bool   eof;
        const char * error;

        for (outLen = 0, eof = false, error = NULL;
             outLen == 0 && !eof && !error; ) {
            if (sessionP->requestIsChunked)
                getSomeChunkedRequestBody(sessionP, max, &eof,
                                          outStartP, &outLen,
                                          &error);
            else {
                getSomeUnchunkedRequestBody(sessionP, max, outStartP, &outLen);
                error = NULL;
                eof   = false;
            }
            if (outLen == 0 && !eof && !error)
                refillBuffer(sessionP, &error);
        }
        if (error)
            sessionP->failureReason = xmlrpc_strdupsol(error);

        *errorP  = error;
        *eofP    = eof;
        *outLenP = outLen;
    }
}



void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP) {
    
    *requestInfoPP = &sessionP->requestInfo;
}



void
SessionGetChannelInfo(TSession * const sessionP,
                      void **    const channelInfoPP) {
    
    *channelInfoPP = sessionP->connP->channelInfoP;
}



abyss_bool
SessionLog(TSession * const sessionP) {

    const char * logline;
    const char * user;
    const char * date;
    const char * peerInfo;

    if (sessionP->validRequest) {
        if (sessionP->requestInfo.user)
            user = sessionP->requestInfo.user;
        else
            user = "no_user";
    } else
        user = "???";
    
    DateToLogString(sessionP->date, &date);
    
    ConnFormatClientAddr(sessionP->connP, &peerInfo);
    
    xmlrpc_asprintf(&logline, "%s - %s - [%s] \"%s\" %d %u",
                    peerInfo,
                    user,
                    date, 
                    sessionP->validRequest ?
                        sessionP->requestInfo.requestline : "???",
                    sessionP->status,
                    sessionP->connP->outbytes
        );
    xmlrpc_strfree(peerInfo);
    xmlrpc_strfree(date);
    
    LogWrite(sessionP->connP->server, logline);

    xmlrpc_strfree(logline);
        
    return true;
}



void *
SessionGetDefaultHandlerCtx(TSession * const sessionP) {

    struct _TServer * const srvP = sessionP->connP->server->srvP;

    return srvP->defaultHandlerContext;
}

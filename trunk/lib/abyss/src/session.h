#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include "xmlrpc-c/abyss.h"
#include "bool.h"
#include "date.h"
#include "data.h"
#include "conn.h"

typedef struct {
    uint8_t major;
    uint8_t minor;
} httpVersion;

typedef enum {
    /* This tells what is supposed to be at the current read position
       in the connection buffer.
    */
    CHUNK_ATHEADER,   /* A chunk header (including EOF marker) */
    CHUNK_INCHUNK,    /* Somewhere in chunk data */
    CHUNK_AFTERCHUNK, /* The delimiter after a chunk */
    CHUNK_EOF         /* Past EOF marker */
} ChunkPosition;

struct _TSession {
    bool validRequest;
        /* Client has sent, and server has recognized, a valid HTTP request.
           This is false when the session is new.  If and when the server
           reads the request from the client and finds it to be valid HTTP,
           it becomes true.
        */
    const char * failureReason;
        /* This is non-null to indicate that we have encountered a protocol
           error or other problem in the session and the session cannot
           continue.  The value is a text explanation of the problem.
        */
    TRequestInfo requestInfo;
        /* Some of the strings this references are in individually malloc'ed
           memory, but some are pointers into arbitrary other data structures
           that happen to live as long as the session.  Some day, we will
           fix that.

           'requestInfo' is valid only if 'validRequest' is true.
        */
    uint32_t nbfileds;
    TList cookies;
    TList ranges;

    uint16_t status;
        /* Response status from handler.  Zero means session is not ready
           for a response yet.  This can mean that we ran a handler and it
           did not call ResponseStatus() to declare this fact.
        */
    TString header;

    bool serverDeniesKeepalive;
        /* Server doesn't want keepalive for this session, regardless of
           what happens in the session.  E.g. because the connection has
           already been kept alive long enough.
        */
    bool responseStarted;
        /* Handler has at least started the response (i.e. called
           ResponseWriteStart())
        */

    struct _TConn * connP;

    httpVersion version;

    TTable requestHeaderFields;
        /* All the fields of the header of the HTTP request.  The key is the
           field name in lower case.  The value is the verbatim value from
           the field.
        */

    TTable responseHeaderFields;
        /* All the fields of the header of the HTTP response.
           This gets successively computed; at any moment, it is the list of
           fields the user has requested so far.  It also includes fields
           Abyss itself has decided to include.  (Blechh.  This needs to be
           cleaned up).

           Each table item is an HTTP header field.  The Name component of the
           table item is the header field name (it is syntactically valid but
           not necessarily a defined field name) and the Value component is the
           header field value (it is syntactically valid but not necessarily
           semantically valid).
        */

    time_t date;

    bool chunkedwrite;
    bool chunkedwritemode;

    bool continueRequired;
        /* This client must receive 100 (continue) status before it will
           send more of the body of the request.
        */

    bool requestIsChunked;
        /* The request body (PUT/POST data) is chunked. */

    struct {
        /* Meaningful only when 'requestIsChunked' is true */

        ChunkPosition position;

        uint32_t bytesLeftCt;
            /* Number of bytes left to read in the current chunk.  Always
               greater than zero except locally.

               Meaningful only when 'position' is INCHUNK.
            */
    } chunkState;
};

/*----------------------------------------------------------------------------
   Following are methods of a TSession object private to Abyss.  Other
   methods are for use by user Uri handlers and are declared in the
   external abyss.h header file.
-----------------------------------------------------------------------------*/

void
SessionInit(TSession * const sessionP,
            TConn *    const c);

void
SessionTerm(TSession * const sessionP);

#endif

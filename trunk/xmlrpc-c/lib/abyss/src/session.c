#include <sys/types.h>
#include <stdio.h>

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/abyss.h"
#include "server.h"
#include "conn.h"

#include "session.h"



abyss_bool
SessionRefillBuffer(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Get the next chunk of data from the connection into the buffer.

   I.e. read data from the socket.
-----------------------------------------------------------------------------*/
    struct _TServer * const srvP = sessionP->conn->server->srvP;
    abyss_bool succeeded;
            
    /* Reset our read buffer & flush data from previous reads. */
    ConnReadInit(sessionP->conn);
    
    /* Read more network data into our buffer.  If we encounter a
       timeout, exit immediately.  We're very forgiving about the
       timeout here.  We allow a full timeout per network read, which
       would allow somebody to keep a connection alive nearly
       indefinitely.  But it's hard to do anything intelligent here
       without very complicated code.
    */
    succeeded = ConnRead(sessionP->conn, srvP->timeout);

    return succeeded;
}



size_t
SessionReadDataAvail(TSession * const sessionP) {

    return sessionP->conn->buffersize - sessionP->conn->bufferpos;

}



void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP) {
/*----------------------------------------------------------------------------
   Extract some data which the server has read and buffered for the
   session.  Don't get or wait for any data that has not yet arrived.
   Do not return more than 'max'.

   We return a pointer to the first byte as *outStartP, and the length in
   bytes as *outLenP.  The memory pointed to belongs to the session.
-----------------------------------------------------------------------------*/
    uint32_t const bufferPos = sessionP->conn->bufferpos;

    *outStartP = &sessionP->conn->buffer[bufferPos];

    *outLenP = MIN(max, sessionP->conn->buffersize - bufferPos);

    /* move pointer past the bytes we are returning */
    sessionP->conn->bufferpos += *outLenP;
}



void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP) {
    
    *requestInfoPP = &sessionP->request_info;
}



abyss_bool
SessionLog(TSession * const sessionP) {

    abyss_bool retval;

    if (sessionP->request_info.requestline == NULL)
        retval = FALSE;
    else {
        const char * const user = sessionP->request_info.user;

        char z[1024];
        uint32_t n;

        if (strlen(sessionP->request_info.requestline) > 1024-26-50)
            sessionP->request_info.requestline[1024-26-50] = '\0';

        

        n = sprintf(z, "%d.%d.%d.%d - %s - [",
                    IPB1(sessionP->conn->peerip),
                    IPB2(sessionP->conn->peerip),
                    IPB3(sessionP->conn->peerip),
                    IPB4(sessionP->conn->peerip),
                    user ? user : ""
            );

        DateToLogString(&sessionP->date, &z[n]);

        sprintf(&z[n+26], "] \"%s\" %d %d",
                sessionP->request_info.requestline,
                sessionP->status,
                sessionP->conn->outbytes);

        LogWrite(sessionP->conn->server, z);
        retval = TRUE;
    }
    return retval;
}




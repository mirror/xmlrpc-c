/******************************************************************************

  Copyright information is at the end of the file.
******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef ABYSS_WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#endif  /* ABYSS_WIN32 */

#include "socket.h"

#ifdef ABYSS_WIN32
#define  EINTR      WSAEINTR
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static abyss_bool ABYSS_TRACE_SOCKET;

/*********************************************************************
** Socket
*********************************************************************/

abyss_bool SocketInit()
{
    abyss_bool retval;
#ifdef ABYSS_WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD( 2, 0 );
 
    err = WSAStartup( wVersionRequested, &wsaData );
    retval = ( err == 0 );
#else
    retval = TRUE;
    ABYSS_TRACE_SOCKET = (getenv("ABYSS_TRACE_SOCKET") != NULL);
#endif  /* ABYSS_WIN32 */
    if (ABYSS_TRACE_SOCKET)
        fprintf(stderr, "Abyss socket layer will trace socket traffic "
                "due to ABYSS_TRACE_SOCKET environment variable\n");
    return retval;
}

#define RET(x)  return ((x)!=(-1))

abyss_bool SocketCreate(TSocket *s)
{
    int rc;

    rc =socket(AF_INET,SOCK_STREAM,0);
    if (rc < 0)
        return FALSE;
    else {
        int32_t n=1;
        *s = rc;
        RET(setsockopt(*s,SOL_SOCKET,SO_REUSEADDR,(char*)&n,sizeof(n)));
    }
}

abyss_bool SocketClose(TSocket *s)
{
#ifdef ABYSS_WIN32
    RET(closesocket(*s));
#else
    RET(close(*s));
#endif  /* ABYSS_WIN32 */
}



void
SocketWrite(TSocket *             const socketP,
            const unsigned char * const buffer,
            uint32_t              const len,
            abyss_bool *          const failedP) {

    size_t bytesLeft;
    abyss_bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        ssize_t rc;
        
        rc = send(*socketP, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);
        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



uint32_t SocketRead(TSocket * const socketP, 
                  char *    const buffer, 
                  uint32_t    const len) {
    int rc;
    rc = recv(*socketP, buffer, len, 0);
    if (ABYSS_TRACE_SOCKET) {
        if (rc < 0)
            fprintf(stderr, "Abyss socket: recv() failed.  errno=%d (%s)",
                    errno, strerror(errno));
        else 
            fprintf(stderr, "Abyss socket: read %u bytes: '%.*s'\n",
                    len, (int)len, buffer);
    }
    return rc;
}



uint32_t SocketPeek(TSocket *s, char *buffer, uint32_t len)
{
    int32_t r=recv(*s,buffer,len,MSG_PEEK);

    if (r==(-1))
#ifdef ABYSS_WIN32
        if (SocketError()==WSAEMSGSIZE)
#else
        if (SocketError()==EMSGSIZE)
#endif
            return len;

    return r;
}

abyss_bool SocketConnect(TSocket *s, TIPAddr *addr, uint16_t port)
{
    struct sockaddr_in name;

    name.sin_family=AF_INET;
    name.sin_port=htons(port);
    name.sin_addr=*addr;

    RET(connect(*s,(struct sockaddr *)&name,sizeof(name)));
}

abyss_bool SocketBind(TSocket *s, TIPAddr *addr, uint16_t port)
{
    struct sockaddr_in name;

    name.sin_family=AF_INET;
    name.sin_port=htons(port);
    if (addr)
        name.sin_addr=*addr;
    else
        name.sin_addr.s_addr=INADDR_ANY;

    RET(bind(*s,(struct sockaddr *)&name,sizeof(name)));
}

abyss_bool SocketListen(TSocket *s, uint32_t backlog)
{
    int32_t n=-1;

    /* Disable the naggle algorithm to make persistant connections faster */
    setsockopt(*s, IPPROTO_TCP,TCP_NODELAY,(char *)&n,sizeof(n));

    RET(listen(*s,backlog));
}

abyss_bool SocketAccept(const TSocket *s, TSocket *ns,TIPAddr *ip)
{
    struct sockaddr_in sa;
    socklen_t size=sizeof(sa);
    abyss_bool connected;

    connected = FALSE;
    for (;;) {
        int rc;
        rc = accept(*s,(struct sockaddr *)&sa,&size);
        if (rc >= 0)
        {
            connected = TRUE;
            *ns = rc;
            *ip=sa.sin_addr;
            break;
        }
        else
            if (SocketError()!=EINTR)
                break;
    }   
    return connected;
}

uint32_t SocketWait(TSocket *s,abyss_bool rd,abyss_bool wr,uint32_t timems)
{
    fd_set rfds,wfds;
#ifdef ABYSS_WIN32
    TIMEVAL tv;
#else
    struct timeval tv;
#endif  /* ABYSS_WIN32 */

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (rd)
        FD_SET(*s,&rfds);

    if (wr)
        FD_SET(*s,&wfds);

    tv.tv_sec=timems/1000;
    tv.tv_usec=timems%1000;


    for (;;)
        switch(select((*s)+1,&rfds,&wfds,NULL,
            (timems==TIME_INFINITE?NULL:&tv)))
        {   
        case 0: /* time out */
            return 0;

        case (-1):  /* socket error */
            if (SocketError()==EINTR)
                break;
            
            return 0;

        default:
            if (FD_ISSET(*s,&rfds))
                return 1;
            if (FD_ISSET(*s,&wfds))
                return 2;
            return 0;
        };
}

abyss_bool SocketBlocking(TSocket *s, abyss_bool b)
{
    uint32_t x=b;

    RET(ioctlsocket(*s,FIONBIO,&x));
}

uint32_t SocketAvailableReadBytes(TSocket *s)
{
    uint32_t x;

    if (ioctlsocket(*s,FIONREAD,&x)!=0)
        x=0;

    return x;
}

uint32_t SocketError()
{
#ifdef ABYSS_WIN32
    return WSAGetLastError();
#else
    return errno;
#endif  /* ABYSS_WIN32 */
}



/******************************************************************************
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

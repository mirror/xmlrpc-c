/*******************************************************************************
**
** socket.c
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
*******************************************************************************/

#include "abyss.h"

/*********************************************************************
** Socket
*********************************************************************/

bool SocketInit()
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
 
	wVersionRequested = MAKEWORD( 2, 0 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	return ( err == 0 );
#else
	return TRUE;
#endif	/* _WIN32 */
}

#define RET(x)	return ((x)!=(-1))

bool SocketCreate(TSocket *s)
{
	int32 n=1;

	if ((*s=socket(AF_INET,SOCK_STREAM,0))==(-1))
		return FALSE;

	RET(setsockopt(*s,SOL_SOCKET,SO_REUSEADDR,(char*)&n,sizeof(n)));
}

bool SocketClose(TSocket *s)
{
#ifdef _WIN32
	RET(closesocket(*s));
#else
	RET(close(*s));
#endif	/* _WIN32 */
}

uint32 SocketWrite(TSocket *s, char *buffer, uint32 len)
{
	return send(*s,buffer,len,0);
}

uint32 SocketRead(TSocket *s, char *buffer, uint32 len)
{
	return recv(*s,buffer,len,0);
}

uint32 SocketPeek(TSocket *s, char *buffer, uint32 len)
{
	int32 r=recv(*s,buffer,len,MSG_PEEK);

	if (r==(-1))
#ifdef _WIN32
		if (SocketError()==WSAEMSGSIZE)
#else
		if (SocketError()==EMSGSIZE)
#endif
			return len;

	return r;
}

bool SocketConnect(TSocket *s, TIPAddr *addr, uint16 port)
{
	struct sockaddr_in name;

	name.sin_family=AF_INET;
	name.sin_port=htons(port);
	name.sin_addr=*addr;

	RET(connect(*s,(struct sockaddr *)&name,sizeof(name)));
}

bool SocketBind(TSocket *s, TIPAddr *addr, uint16 port)
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

bool SocketListen(TSocket *s, uint32 backlog)
{
	int32 n=-1;

	/* Disable the naggle algorithm to make persistant connections faster */
	setsockopt(*s, IPPROTO_TCP,TCP_NODELAY,(char *)&n,sizeof(n));

	RET(listen(*s,backlog));
}

bool SocketAccept(TSocket *s, TSocket *ns,TIPAddr *ip)
{
	struct sockaddr_in sa;
	uint32 size=sizeof(sa);

	for (;;)
		if ((*ns=accept(*s,(struct sockaddr *)&sa,&size))!=(-1))
		{
			*ip=sa.sin_addr;
			break;
		}
		else
			if (SocketError()!=EINTR)
				break;
	
	RET(*ns);
}

uint32 SocketWait(TSocket *s,bool rd,bool wr,uint32 timems)
{
	fd_set rfds,wfds;
#ifdef _WIN32
	TIMEVAL tv;
#else
	struct timeval tv;
#endif	/* _WIN32 */

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
		case 0:	/* time out */
			return 0;

		case (-1):	/* socket error */
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

bool SocketBlocking(TSocket *s, bool b)
{
	uint32 x=b;

	RET(ioctlsocket(*s,FIONBIO,&x));
}

uint32 SocketAvailableReadBytes(TSocket *s)
{
	uint32 x;

	if (ioctlsocket(*s,FIONREAD,&x)!=0)
		x=0;

	return x;
}

uint32 SocketError()
{
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif	/* _WIN32 */
}

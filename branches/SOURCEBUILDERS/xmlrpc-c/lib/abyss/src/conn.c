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
*******************************************************************************/

#include <time.h>
#include <string.h>
#include "abyss.h"

/*********************************************************************
** Conn
*********************************************************************/

TConn *ConnAlloc()
{
	return (TConn *)malloc(sizeof(TConn));
}

void ConnFree(TConn *c)
{
	free(c);
}

uint32 ConnJob(TConn *c)
{
	c->connected=TRUE;
	(c->job)(c);
	c->connected=FALSE;
	return 0;
}

bool ConnCreate(TConn *c, TSocket *s, void (*func)(TConn *))
{
	c->socket=*s;
	c->buffersize=c->bufferpos=0;
	c->connected=TRUE;
	c->job=func;
	c->inbytes=c->outbytes=0;
	return ThreadCreate(&(c->thread),(TThreadProc)ConnJob,c);
}

bool ConnProcess(TConn *c)
{
/******* Must check this undef *****/
#ifndef _WIN32
	c->connected=FALSE;
#endif
	return ThreadRun(&(c->thread));
}

bool ConnKill(TConn *c)
{
	c->connected=FALSE;
	return ThreadKill(&(c->thread));
}

void ConnReadInit(TConn *c)
{
	if (c->buffersize>c->bufferpos)
	{
		c->buffersize-=c->bufferpos;
		memmove(c->buffer,c->buffer+c->bufferpos,c->buffersize);
		c->bufferpos=0;
	}
	else
		c->buffersize=c->bufferpos=0;

	c->inbytes=c->outbytes=0;
}

bool ConnRead(TConn *c,uint32 timeout)
{
	while (SocketWait(&(c->socket),TRUE,FALSE,timeout*1000)==1)
	{
		uint32 y,x=SocketAvailableReadBytes(&(c->socket));
		
		/* Avoid lost connections */
		if (x<=0)
			break;
		
		/* Avoid Buffer overflow and lost Connections */
		if (x+c->buffersize>=BUFFER_SIZE)
			x=BUFFER_SIZE-c->buffersize-1;
		
		if ((y=SocketRead(&(c->socket),c->buffer+c->buffersize,x))>0)
			{
				/*int i;
				char d;

				for (i=0;i<y;i++)
				{
					d=*(c->buffer+c->buffersize+i);
					if (d<32)
						printf("[%d]",d);
					else
						printf("%c",d);
				};
				printf("\n");*/

				c->inbytes+=y;
				c->buffersize+=y;
				c->buffer[c->buffersize]='\0';
				return TRUE;
			};

		break;
	};

	return FALSE;
}
			
bool ConnWrite(TConn *c,void *buffer,uint32 size)
{
	c->outbytes+=size;
	return SocketWrite(&(c->socket),buffer,size);
}

bool ConnWriteFromFile(TConn *c,TFile *file,uint64 start,uint64 end,
			void *buffer,uint32 buffersize,uint32 rate)
{
	uint64 y,bytesread=0;
	uint32 waittime;

	if (rate>0)
	{
		if (buffersize>rate)
			buffersize=rate;

		waittime=(1000*buffersize)/rate;
	}
	else
		waittime=0;

	if (!FileSeek(file,start,SEEK_SET))
		return FALSE;

	while (bytesread<=end-start)
	{
		y=(end-start+1)-bytesread;

		if (y>buffersize)
			y=buffersize;

		y=FileRead(file,buffer,y);
		bytesread+=y;

		if (y>0)
			ConnWrite(c,buffer,y);
		else
			break;

		if (waittime)
			ThreadWait(waittime);
	};

	return (bytesread>end-start);
}

bool ConnReadLine(TConn *c,char **z,uint32 timeout)
{
	char *p,*t;
	bool first=TRUE;
	uint32 to,start;

	p=*z=c->buffer+c->bufferpos;
	start=clock();

	for (;;)
	{
		to=(clock()-start)/CLOCKS_PER_SEC;
		if (to>timeout)
			break;

		if (first)
		{
			if (c->bufferpos>=c->buffersize)
				if (!ConnRead(c,timeout-to))
					break;

			first=FALSE;
		}
		else
		{
			if (!ConnRead(c,timeout-to))
				break;
		};

		if (t=strchr(p,LF))
		{
			if ((*p!=LF) && (*p!=CR))
			{
				if (!*(t+1))
					continue;

				p=t;

				if ((*(p+1)==' ') || (*(p+1)=='\t'))
				{
					if (p>*z)
						if (*(p-1)==CR)
							*(p-1)=' ';

					*(p++)=' ';
					continue;
				};
			} else {
			    /* emk - 04 Jan 2001 - Bug fix to leave buffer
			    ** pointing at start of body after reading blank
			    ** line following header. */
			    p=t;
			}

			c->bufferpos+=p+1-*z;

			if (p>*z)
				if (*(p-1)==CR)
					p--;

			*p='\0';
			return TRUE;
		}
	};

	return FALSE;
}

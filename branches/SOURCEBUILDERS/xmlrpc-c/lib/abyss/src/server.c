/*******************************************************************************
**
** server.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#else
/* Check this
#include <sys/io.h>
*/
#endif	/* _WIN32 */
#include <fcntl.h>
#include "abyss.h"

#define BOUNDARY	"##123456789###BOUNDARY"

typedef int (*TQSortProc)(const void *, const void *);

int cmpfilenames(const TFileInfo **f1,const TFileInfo **f2)
{
	if (((*f1)->attrib & A_SUBDIR) && !((*f2)->attrib & A_SUBDIR))
		return (-1);
	if (!((*f1)->attrib & A_SUBDIR) && ((*f2)->attrib & A_SUBDIR))
		return 1;

	return strcmp((*f1)->name,(*f2)->name);
}

int cmpfiledates(const TFileInfo **f1,const TFileInfo **f2)
{
	if (((*f1)->attrib & A_SUBDIR) && !((*f2)->attrib & A_SUBDIR))
		return (-1);
	if (!((*f1)->attrib & A_SUBDIR) && ((*f2)->attrib & A_SUBDIR))
		return 1;

	return ((*f1)->time_write-(*f2)->time_write);
}

bool ServerDirectoryHandler(TSession *r,char *z,TDate *dirdate)
{
	TFileInfo fileinfo,*fi;
	TFileFind findhandle;
	char *p,z1[26],z2[20],z3[9],u,*z4;
	TList list;
	int16 i;
	uint32 k;
	bool text=FALSE;
	bool ascending=TRUE;
	uint16 sort=1;	/* 1=by name, 2=by date */
	struct tm ftm;
	TPool pool;
	TDate date;

	if (r->query)
		if (strcmp(r->query,"plain")==0)
			text=TRUE;
		else if (strcmp(r->query,"name-up")==0)
		{
			sort=1;
			ascending=TRUE;
		}
		else if (strcmp(r->query,"name-down")==0)
		{
			sort=1;
			ascending=FALSE;
		}
		else if (strcmp(r->query,"date-up")==0)
		{
			sort=2;
			ascending=TRUE;
		}
		else if (strcmp(r->query,"date-down")==0)
		{
			sort=2;
			ascending=FALSE;
		}
		else 
		{
			ResponseStatus(r,400);
			return TRUE;
		};

	if (DateCompare(&r->date,dirdate)<0)
		dirdate=&r->date;

	if (p=RequestHeaderValue(r,"If-Modified-Since"))
		if (DateDecode(p,&date))
			if (DateCompare(dirdate,&date)<=0)
			{
				ResponseStatus(r,304);
				ResponseWrite(r);
				return TRUE;
			};

	if (!FileFindFirst(&findhandle,z,&fileinfo))
	{
		ResponseStatusErrno(r);
		return TRUE;
	};

	ListInit(&list);

	if (!PoolCreate(&pool,1024))
	{
		ResponseStatus(r,500);
		return TRUE;
	};

	do
	{
		/* Files which names start with a dot are ignored */
		/* This includes implicitly the ./ and ../ */
		if (*fileinfo.name=='.')
			if (strcmp(fileinfo.name,"..")==0)
			{
				if (strcmp(r->uri,"/")==0)
					continue;
			}
			else
				continue;

		if (fi=(TFileInfo *)PoolAlloc(&pool,sizeof(fileinfo)))
		{
			memcpy(fi,&fileinfo,sizeof(fileinfo));
			if (ListAdd(&list,fi))
				continue;
		};

		ResponseStatus(r,500);
		FileFindClose(&findhandle);
		ListFree(&list);
		PoolFree(&pool);
		return TRUE;

	} while (FileFindNext(&findhandle,&fileinfo));

	FileFindClose(&findhandle);

	/* Send something to the user to show that we are still alive */
	ResponseStatus(r,200);
	ResponseContentType(r,(text?"text/plain":"text/html"));

	if (DateToString(dirdate,z))
		ResponseAddField(r,"Last-Modified",z);
	
	ResponseChunked(r);
	ResponseWrite(r);

	if (r->method!=m_head)
	{
		if (text)
		{
			sprintf(z,"Index of %s" CRLF,r->uri);
			i=strlen(z)-2;
			p=z+i+2;

			while (i>0)
			{
				*(p++)='-';
				i--;
			};

			*p='\0';
			strcat(z,CRLF CRLF
				"Name                      Size      Date-Time             Type" CRLF
				"--------------------------------------------------------------------------------"CRLF);
		}
		else
		{
			sprintf(z,"<HTML><HEAD><TITLE>Index of %s</TITLE></HEAD><BODY>"
					"<H1>Index of %s</H1><PRE>",r->uri,r->uri);
			strcat(z,"Name                      Size      Date-Time             Type<HR WIDTH=100%>"CRLF);
 		};

		HTTPWrite(r,z,strlen(z));

		/* Sort the files */
			qsort(list.item,list.size,sizeof(void *),(TQSortProc)(sort==1?cmpfilenames:cmpfiledates));
		
		/* Write the listing */
		if (ascending)
			i=0;
		else
			i=list.size-1;

		while ((i<list.size) && (i>=0))
		{
			fi=list.item[i];

			if (ascending)
				i++;
			else
				i--;
			
			strcpy(z,fi->name);

			k=strlen(z);

			if (fi->attrib & A_SUBDIR)
			{
				z[k++]='/';
				z[k]='\0';
			};

			if (k>24)
			{
				z[10]='\0';
				strcpy(z1,z);
				strcat(z1,"...");
				strcat(z1,z+k-11);
				k=24;
				p=z1+24;
			}
			else
			{
				strcpy(z1,z);
				
				k++;
				p=z1+k;
				while (k<25)
					z1[k++]=' ';

				z1[25]='\0';
			};

			ftm=*(gmtime(&fi->time_write));
			sprintf(z2,"%02u/%02u/%04u %02u:%02u:%02u",ftm.tm_mday,ftm.tm_mon,
				ftm.tm_year+1900,ftm.tm_hour,ftm.tm_min,ftm.tm_sec);

			if (fi->attrib & A_SUBDIR)
			{
				strcpy(z3,"   --  ");
				z4="Directory";
			}
			else
			{
				if (fi->size<9999)
					u='b';
				else 
				{
					fi->size/=1024;
					if (fi->size<9999)
						u='K';
					else
					{
						fi->size/=1024;
						if (fi->size<9999)
							u='M';
						else
							u='G';
					};
				};

				sprintf(z3,"%5lu %c",fi->size,u);

				if (strcmp(fi->name,"..")==0)
					z4="";
				else
					z4=MIMETypeFromFileName(fi->name);

				if (!z4)
					z4="Unknown";
			};

			if (text)
				sprintf(z,"%s%s %s    %s   %s"CRLF,
					z1,p,z3,z2,z4);
			else
				sprintf(z,"<A HREF=\"%s%s\">%s</A>%s %s    %s   %s"CRLF,
					fi->name,(fi->attrib & A_SUBDIR?"/":""),z1,p,z3,z2,z4);

			HTTPWrite(r,z,strlen(z));
		};
		
		/* Write the tail of the file */
		if (text)
			strcpy(z,SERVER_PLAIN_INFO);
		else
			strcpy(z,"</PRE>" SERVER_HTML_INFO "</BODY></HTML>" CRLF CRLF);

		HTTPWrite(r,z,strlen(z));
	};
	
	HTTPWriteEnd(r);
	/* Free memory and exit */
	ListFree(&list);
	PoolFree(&pool);

	return TRUE;
}

bool ServerFileHandler(TSession *r,char *z,TDate *filedate)
{
	char *mediatype;
	TFile file;
	uint64 filesize,start,end;
	uint16 i;
	TDate date;
	char *p;

	mediatype=MIMETypeGuessFromFile(z);

	if (!FileOpen(&file,z,O_BINARY | O_RDONLY))
	{
		ResponseStatusErrno(r);
		return TRUE;
	};

	if (DateCompare(&r->date,filedate)<0)
		filedate=&r->date;

	if (p=RequestHeaderValue(r,"if-modified-since"))
		if (DateDecode(p,&date))
			if (DateCompare(filedate,&date)<=0)
			{
				ResponseStatus(r,304);
				ResponseWrite(r);
				return TRUE;
			}
			else
				r->ranges.size=0;

	filesize=FileSize(&file);

	switch (r->ranges.size)
	{
	case 0:
		ResponseStatus(r,200);
		break;

	case 1:
		if (!RangeDecode((char *)(r->ranges.item[0]),filesize,&start,&end))
		{
			ListFree(&(r->ranges));
			ResponseStatus(r,200);
			break;
		}
		
		sprintf(z,"bytes %ld-%ld/%ld",start,end,filesize);

		ResponseAddField(r,"Content-range",z);
		ResponseContentLength(r,end-start+1);
		ResponseStatus(r,206);
		break;

	default:
		ResponseContentType(r,"multipart/ranges; boundary=" BOUNDARY);
		ResponseStatus(r,206);
		break;
	};
	
	if (r->ranges.size==0)
	{
		ResponseContentLength(r,filesize);
		ResponseContentType(r,mediatype);
	};
	
	if (DateToString(filedate,z))
		ResponseAddField(r,"Last-Modified",z);

	ResponseWrite(r);

	if (r->method!=m_head)
	{
		if (r->ranges.size==0)
			ConnWriteFromFile(r->conn,&file,0,filesize-1,z,4096,0);
		else if (r->ranges.size==1)
			ConnWriteFromFile(r->conn,&file,start,end,z,4096,0);
		else
			for (i=0;i<=r->ranges.size;i++)
			{
				ConnWrite(r->conn,"--",2);
				ConnWrite(r->conn,BOUNDARY,strlen(BOUNDARY));
				ConnWrite(r->conn,CRLF,2);

				if (i<r->ranges.size)
					if (RangeDecode((char *)(r->ranges.item[i]),filesize,&start,&end))
					{
						/* Entity header, not response header */
						sprintf(z,"Content-type: %s" CRLF
									"Content-range: bytes %ld-%ld/%ld" CRLF
									"Content-length: %lu" CRLF
									CRLF,mediatype,start,end,filesize,end-start+1);

						ConnWrite(r->conn,z,strlen(z));

						ConnWriteFromFile(r->conn,&file,start,end,z,4096,0);
					};
			};
	};

	FileClose(&file);

	return TRUE;
}

bool ServerDefaultHandlerFunc(TSession *r)
{
	char *p,z[4096];
	TFileStat fs;
	uint16 i;
	bool endingslash=FALSE;
	TDate objdate;

	if (!RequestValidURIPath(r))
	{
		ResponseStatus(r,400);
		return TRUE;
	};

	/* Must check for * (asterisk uri) in the future */
	if (r->method==m_options)
	{
		ResponseAddField(r,"Allow","GET, HEAD");
		ResponseContentLength(r,0);
		ResponseStatus(r,200);
		return TRUE;
	};

	if ((r->method!=m_get) && (r->method!=m_head))
	{
		ResponseAddField(r,"Allow","GET, HEAD");
		ResponseStatus(r,405);
		return TRUE;
	};

	strcpy(z,r->server->filespath);
	strcat(z,r->uri);

	p=z+strlen(z)-1;
	if (*p=='/')
	{
		endingslash=TRUE;
		*p='\0';
	};

#ifdef _WIN32
	p=z;
	while (*p)
	{
		if ((*p)=='/')
			*p='\\';

		p++;
	};
#endif	/* _WIN32 */

	if (!FileStat(z,&fs))
	{
		ResponseStatusErrno(r);
		return TRUE;
	};

	if (fs.st_mode & S_IFDIR)
	{
		/* Redirect to the same directory but with the ending slash
		** to avoid problems with some browsers (IE for examples) when
		** they generate relative urls */
		if (!endingslash)
		{
			strcpy(z,r->uri);
			p=z+strlen(z);
			*p='/';
			*(p+1)='\0';
			ResponseAddField(r,"Location",z);
			ResponseStatus(r,302);
			ResponseWrite(r);
			return TRUE;
		};

#ifdef _WIN32
		*p='\\';
#else
		*p='/';
#endif	/* _WIN32 */
		p++;

		i=r->server->defaultfilenames.size;
		while (i-->0)
		{
  			*p='\0';		
  			strcat(z,(char *)(r->server->defaultfilenames.item[i]));
  			if (FileStat(z,&fs))
  				if (!(fs.st_mode & S_IFDIR))
					if (DateFromLocal(&objdate,fs.st_mtime))
						return ServerFileHandler(r,z,&objdate);
					else
						return ServerFileHandler(r,z,NULL);
		};

		*(p-1)='\0';
		
		if (!FileStat(z,&fs))
		{
			ResponseStatusErrno(r);
			return TRUE;
		};

		if (DateFromLocal(&objdate,fs.st_mtime))
			return ServerDirectoryHandler(r,z,&objdate);
		else
			return ServerDirectoryHandler(r,z,NULL);

	}
	else
		if (DateFromLocal(&objdate,fs.st_mtime))
			return ServerFileHandler(r,z,&objdate);
		else
			return ServerFileHandler(r,z,NULL);
}

bool ServerCreate(TServer *srv,char *name,uint16 port,char *filespath,
				  char *logfilename)
{
	srv->name=strdup(name);
	srv->port=port;
	srv->defaulthandler=ServerDefaultHandlerFunc;
	srv->filespath=strdup(filespath);
	srv->keepalivetimeout=15;
	srv->keepalivemaxconn=30;
	srv->timeout=15;
	srv->advertise=TRUE;
#ifdef _UNIX
	srv->pidfile=srv->uid=srv->gid=(-1);
#endif	/* _UNIX */

	ListInit(&srv->handlers);
	ListInitAutoFree(&srv->defaultfilenames);

	if (logfilename)
		return LogOpen(srv,logfilename);
	else
	{
		srv->logfile=(-1);
		return TRUE;
	};
}

void ServerFree(TServer *srv)
{
	free(srv->name);
	free(srv->filespath);
	ListFree(&srv->handlers);
	ListInitAutoFree(&srv->defaultfilenames);
	LogClose(srv);
}

void ServerFunc(TConn *c)
{
	TSession r;
	uint32 i,ka;
	bool treated;
	URIHandler *hl=(URIHandler *)(c->server)->handlers.item;

	ka=c->server->keepalivemaxconn;

	while (ka--)
	{
		RequestInit(&r,c);

		/* Wait to read until timeout */
		if (!ConnRead(c,c->server->keepalivetimeout))
			break;

		if (RequestRead(&r))
		{
			/* Check if it is the last keepalive */
			if (ka==1)
				r.keepalive=FALSE;

			r.cankeepalive=r.keepalive;

			if (r.status==0)
				if (r.versionmajor>=2)
					ResponseStatus(&r,505);
				else if (!RequestValidURI(&r))
					ResponseStatus(&r,400);
				else
				{
					i=c->server->handlers.size;
					treated=FALSE;

					while (i>0)
					{
						i--;
						if ((hl[i])(&r))
						{
							treated=TRUE;
							break;
						};
					};

					if (!treated)
						((URIHandler)(c->server->defaulthandler))(&r);
				};
		};
       		
		HTTPWriteEnd(&r);

		if (!r.done)
			ResponseError(&r);

		SessionLog(&r);

		if (!(r.keepalive && r.cankeepalive))
			break;

		/**************** Must adjust the read buffer *****************/
		ConnReadInit(c);		
	};

	RequestFree(&r);

	SocketClose(&(c->socket));
}

void ServerInit(TServer *srv)
{
	/********* Must check errors from these functions *************/
	if (!SocketInit())
		TraceExit("Can't initialize TCP sockets\n");;

	if (!SocketCreate(&srv->listensock))
		TraceExit("Can't create a socket\n");;

	if (!SocketBind(&srv->listensock,NULL,srv->port))
		TraceExit("Can't bind\n");

	if (!SocketListen(&srv->listensock,MAX_CONN))
		TraceExit("Can't listen\n");
};


#ifdef _FORK
void ServerRun(TServer *srv)
{
	TSocket s,ns;
	TConn c;
	pid_t r;
	TIPAddr ip;

	s=srv->listensock;

	while (1)
	{
		if (SocketAccept(&s,&ns,&ip))
		{
			switch (fork())
			{
			case 0:
				SocketClose(&s);
				c.socket=ns;
				c.buffersize=c.bufferpos=0;
				c.inbytes=c.outbytes=0;
				c.server=srv;
				c.peerip=ip;
				ServerFunc(&c);
			 	exit(0);
			case (-1):
				TraceExit("Fork Error");
			};
			SocketClose(&ns);
		}
		else
			TraceMsg("Socket Error=%d\n", SocketError());
	};
}
#else
void ServerRun(TServer *srv)
{
	uint32 i;
	TSocket s,ns;
	TIPAddr ip;
	TConn c[MAX_CONN];

	for (i=0;i<MAX_CONN;i++)
	{
		c[i].connected=FALSE;
		c[i].server=srv;
	};

	s=srv->listensock;

	while (1)
	{
		for (i=0;i<MAX_CONN;i++)
			if (!c[i].connected)
				break;

		if (i==MAX_CONN)
		{
			ThreadWait(2000);
			continue;
		};

		if (SocketAccept(&s,&ns,&ip))
		{
			if (ConnCreate(c+i,&ns,&ServerFunc))
			{
				c[i].peerip=ip;
				c[i].connected=TRUE;
				ConnProcess(c+i);
			}
			else
				SocketClose(&ns);
		}
		else
			TraceMsg("Socket Error=%d\n", SocketError());
	};
}
#endif	/* _FORK */




bool ServerAddHandler(TServer *srv,URIHandler handler)
{
	return ListAdd(&srv->handlers,handler);
}

void ServerDefaultHandler(TServer *srv,URIHandler handler)
{
	srv->defaulthandler=handler;
}

bool LogOpen(TServer *srv, char *filename)
{
	if (FileOpenCreate(&(srv->logfile),filename,O_WRONLY | O_APPEND))
		if (MutexCreate(&(srv->logmutex)))
			return TRUE;
		else
		{
			FileClose(&(srv->logfile));
			srv->logfile=(-1);
		};

	TraceMsg("Can't open log file '%s'",filename);
	return FALSE;
}

void LogWrite(TServer *srv,char *c)
{
	if ((srv->logfile)==(-1))
		return;

	if (!MutexLock(&(srv->logmutex)))
		return;

	FileWrite(&(srv->logfile),c,strlen(c));
	FileWrite(&(srv->logfile),LBR,strlen(LBR));

	MutexUnlock(&(srv->logmutex));
}

void LogClose(TServer *srv)
{
	if ((srv->logfile)==(-1))
		return;

	FileClose(&(srv->logfile));
	MutexFree(&(srv->logmutex));
}

bool SessionLog(TSession *s)
{
	char z[1024];
	uint32 n;

	if (strlen(s->requestline)>1024-26-50)
		s->requestline[1024-26-50]='\0';

	n=sprintf(z,"%d.%d.%d.%d - %s - [",IPB1(s->conn->peerip),IPB2(s->conn->peerip),IPB3(s->conn->peerip),IPB4(s->conn->peerip),(s->user?s->user:""));

	DateToLogString(&s->date,z+n);

	sprintf(z+n+26,"] \"%s\" %d %d",s->requestline,s->status,s->conn->outbytes);

	LogWrite(s->server,z);
	return TRUE;
}

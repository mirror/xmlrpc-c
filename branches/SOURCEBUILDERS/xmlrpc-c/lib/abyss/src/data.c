/*******************************************************************************
**
** list.c
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

#include <malloc.h>
#include <string.h>
#include "abyss.h"

/*********************************************************************
** List
*********************************************************************/

void ListInit(TList *sl)
{
	sl->item=NULL;
	sl->size=sl->maxsize=0;
	sl->autofree=FALSE;
}

void ListInitAutoFree(TList *sl)
{
	sl->item=NULL;
	sl->size=sl->maxsize=0;
	sl->autofree=TRUE;
}

void ListFree(TList *sl)
{
	uint16 i;

	if (sl->item)
	{
		if (sl->autofree && sl->size)
			for (i=sl->size;i>0;i--)
				free(sl->item[i-1]);
			
		free(sl->item);
	}

	ListInit(sl);
}

bool ListAdd(TList *sl,void *str)
{
	if (sl->size>=sl->maxsize)
	{
		void **newitem;
		
		sl->maxsize+=16;

		if (newitem=realloc(sl->item,(sl->maxsize)*sizeof(void *)))
			sl->item=newitem;
		else
		{
			sl->maxsize-=16;
			return FALSE;
		};
	};

	sl->item[sl->size]=str;
	sl->size++;
	return TRUE;
}

void NextToken(char **c);
char *GetToken(char **c);

bool ListAddFromString(TList *list,char *c)
{
	char *t,*p;

	if (c)
		while (1)
		{
			NextToken(&c);

			while (*c==',')
				c++;

			if (!(t=GetToken(&c)))
				break;

			p=c-2;

			while (*p==',')
				*(p--)='\0';

			if (*t)
				if (!ListAdd(list,t))
					return FALSE;
		};

	return TRUE;
}

bool ListFindString(TList *sl,char *str,uint16 *index)
{
	uint16 i;

	if (sl->item && str)
		for (i=0;i<sl->size;i++)
			if (strcmp(str,(char *)(sl->item[i]))==0)
			{
				*index=i;
				return TRUE;
			};

	return FALSE;
}

/*********************************************************************
** Buffer
*********************************************************************/

bool BufferAlloc(TBuffer *buf,uint32 memsize)
{
	/* ************** Implement the static buffers ***/
	buf->staticid=0;
	buf->data=(void *)malloc(memsize);
	if (buf->data)
	{
		buf->size=memsize;
		return TRUE;
	}
	else
	{
		buf->size=0;
		return FALSE;
	};
}

void BufferFree(TBuffer *buf)
{
	if (buf->staticid)
	{
		/* ************** Implement the static buffers ***/
	}
	else
		free(buf->data);

	buf->size=0;
	buf->staticid=0;
}

bool BufferRealloc(TBuffer *buf,uint32 memsize)
{
	if (buf->staticid)
	{
		TBuffer b;

		if (memsize<=buf->size)
			return TRUE;

		if (BufferAlloc(&b,memsize))
		{
			memcpy(b.data,buf->data,buf->size);
			BufferFree(buf);
			*buf=b;
			return TRUE;
		}
	}
	else
	{
		void *d;
		
		if (d=realloc(buf->data,memsize))
		{
			buf->data=d;
			buf->size=memsize;
			return TRUE;
		}
	}

	return FALSE;
}


/*********************************************************************
** String
*********************************************************************/

bool StringAlloc(TString *s)
{
	s->size=0;
	if (BufferAlloc(&(s->buffer),256))
	{
		*(char *)(s->buffer.data)='\0';
		return TRUE;
	}
	else
		return FALSE;
}

bool StringConcat(TString *s,char *s2)
{
	uint32 len=strlen(s2);

	if (len+s->size+1>s->buffer.size)
		if (!BufferRealloc(&(s->buffer),((len+s->size+1+256)/256)*256))
			return FALSE;
	
	strcat((char *)(s->buffer.data),s2);
	s->size+=len;
	return TRUE;
}

bool StringBlockConcat(TString *s,char *s2,char **ref)
{
	uint32 len=strlen(s2)+1;

	if (len+s->size>s->buffer.size)
		if (!BufferRealloc(&(s->buffer),((len+s->size+1+256)/256)*256))
			return FALSE;
	
	*ref=(char *)(s->buffer.data)+s->size;
	memcpy(*ref,s2,len);
	s->size+=len;
	return TRUE;
}

void StringFree(TString *s)
{
	s->size=0;
	BufferFree(&(s->buffer));
}

char *StringData(TString *s)
{
	return (char *)(s->buffer.data);
}

/*********************************************************************
** Hash
*********************************************************************/

uint16 Hash16(char *s)
{
	uint16 i=0;

	while (*s)
		i+=*(s++);

	return i;
}

/*********************************************************************
** Table
*********************************************************************/

void TableInit(TTable *t)
{
	t->item=NULL;
	t->size=t->maxsize=0;
}

void TableFree(TTable *t)
{
	uint16 i;

	if (t->item)
	{
		if (t->size)
			for (i=t->size;i>0;i--)
			{
				free(t->item[i-1].name);
				free(t->item[i-1].value);
			};
			
		free(t->item);
	}

	TableInit(t);
}

bool TableFindIndex(TTable *t,char *name,uint16 *index)
{
	uint16 i,hash=Hash16(name);

	if ((t->item) && (t->size>0) && (*index<t->size))
	{
		for (i=*index;i<t->size;i++)
			if (hash==t->item[i].hash)
				if (strcmp(t->item[i].name,name)==0)
				{
					*index=i;
					return TRUE;
				};
	};

	return FALSE;
}

bool TableAddReplace(TTable *t,char *name,char *value)
{
	uint16 i=0;

	if (TableFindIndex(t,name,&i))
	{
		free(t->item[i].value);
		if (value)
			t->item[i].value=strdup(value);
		else
		{
			free(t->item[i].name);
			if (--t->size>0)
				t->item[i]=t->item[t->size];
		};

		return TRUE;
	}
	else
		return TableAdd(t,name,value);
}

bool TableAdd(TTable *t,char *name,char *value)
{
	if (t->size>=t->maxsize)
	{
		TTableItem *newitem;
		
		t->maxsize+=16;

		if (newitem=(TTableItem *)realloc(t->item,(t->maxsize)*sizeof(TTableItem)))
			t->item=newitem;
		else
		{
			t->maxsize-=16;
			return FALSE;
		};
	};

	t->item[t->size].name=strdup(name);
	t->item[t->size].value=strdup(value);
	t->item[t->size].hash=Hash16(name);

	t->size++;
	return TRUE;
}

char *TableFind(TTable *t,char *name)
{
	uint16 i=0;

	if (TableFindIndex(t,name,&i))
		return t->item[i].value;
	else
		return NULL;
}

/*********************************************************************
** Pool
*********************************************************************/

TPoolZone *PoolZoneAlloc(uint32 zonesize)
{
	TPoolZone *pz;

	pz=(TPoolZone *)malloc(zonesize+sizeof(TPoolZone));
	if (pz)
	{
		pz->pos=pz->data;
		pz->maxpos=pz->pos+zonesize;
		pz->next=pz->prev=NULL;
	};

	return pz;
}

bool PoolCreate(TPool *p,uint32 zonesize)
{
	p->zonesize=zonesize;
	if (MutexCreate(&p->mutex))
		if (!(p->firstzone=p->currentzone=PoolZoneAlloc(zonesize)))
		{
			MutexFree(&p->mutex);
			return FALSE;
		};
	
	return TRUE;
}

void *PoolAlloc(TPool *p,uint32 size)
{
	TPoolZone *pz,*npz;
	void *x;
	uint32 zonesize;

	if (size==0)
		return NULL;

	if (!MutexLock(&p->mutex))
		return NULL;

	pz=p->currentzone;

	if (pz->pos+size<pz->maxpos)
	{
		x=pz->pos;
		pz->pos+=size;
		MutexUnlock(&p->mutex);
		return x;
	};

	if (size>p->zonesize)
		zonesize=size;
	else
		zonesize=p->zonesize;

	if (npz=PoolZoneAlloc(zonesize))
	{
		npz->prev=pz;
		npz->next=pz->next;
		pz->next=npz;
		p->currentzone=npz; 
		x=npz->data;
		npz->pos=npz->data+size;
	}
	else
		x=NULL;

	MutexUnlock(&p->mutex);
	return x;
}

void PoolFree(TPool *p)
{
	TPoolZone *pz,*npz;

	pz=p->firstzone;

	while (pz)
	{
		npz=pz->next;
		free(pz);
		pz=npz;
	};
}

char *PoolStrdup(TPool *p,char *s)
{
	char *ns;

	if (s)
		if (ns=PoolAlloc(p,strlen(s)+1))
			strcpy(ns,s);
	else
		ns=NULL;

	return ns;
}

void PoolDump(TPool *p)
{
	TPoolZone *pz;

	TraceMsg("first=[%p] current=[%p]",pz=p->firstzone,p->currentzone);

	while (pz)
	{
		TraceMsg("Zone [%p]: data=[%p] pos=[%p] max=[%p]",pz,pz->data,pz->pos,pz->maxpos);
		pz=pz->next;
	};
}
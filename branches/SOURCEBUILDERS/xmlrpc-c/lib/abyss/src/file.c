/*******************************************************************************
**
** file.c
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

#ifdef _WIN32
#include <io.h>
#else
/* Must check this
#include <sys/io.h>
*/
#endif	/* _WIN32 */

#ifndef _WIN32
#include <dirent.h>
#endif	/* _WIN32 */
#include "abyss.h"

/*********************************************************************
** File
*********************************************************************/

bool FileOpen(TFile *f, char *name,uint32 attrib)
{
#ifdef _WIN32
	return ((*f=_open(name,attrib))!=(-1));
#else
	return ((*f=open(name,attrib))!=(-1));
#endif
}

bool FileOpenCreate(TFile *f, char *name,uint32 attrib)
{
#ifdef _WIN32
	return ((*f=_open(name,attrib | O_CREAT,_S_IWRITE | _S_IREAD))!=(-1));
#else
	return ((*f=open(name,attrib | O_CREAT,S_IWRITE | S_IREAD))!=(-1));
#endif
}

bool FileWrite(TFile *f, void *buffer, uint32 len)
{
#ifdef _WIN32
	return (_write(*f,buffer,len)==(int32)len);
#else
	return (write(*f,buffer,len)==(int32)len);
#endif
}

int32 FileRead(TFile *f, void *buffer, uint32 len)
{
#ifdef _WIN32
	return (_read(*f,buffer,len));
#else
	return (read(*f,buffer,len));
#endif
}

bool FileSeek(TFile *f, uint64 pos, uint32 attrib)
{
#ifdef _WIN32
	return (_lseek(*f,pos,attrib)!=(-1));
#else
	return (lseek(*f,pos,attrib)!=(-1));
#endif
}

uint64 FileSize(TFile *f)
{
#ifdef _WIN32
	return (_filelength(*f));
#else
	struct stat fs;

	fstat(*f,&fs);
	return (fs.st_size);
#endif	
}

bool FileClose(TFile *f)
{
#ifdef _WIN32
	return (_close(*f)!=(-1));
#else
	return (close(*f)!=(-1));
#endif
}

bool FileStat(char *filename,TFileStat *filestat)
{
#ifdef _WIN32
	return (_stati64(filename,filestat)!=(-1));
#else
	return (stat(filename,filestat)!=(-1));
#endif
}

bool FileFindFirst(TFileFind *filefind,char *path,TFileInfo *fileinfo)
{
#ifdef _WIN32
	bool ret;
	char *p=path+strlen(path);

	*p='\\';
	*(p+1)='*';
	*(p+2)='\0';
	ret=(((*filefind)=_findfirst(path,fileinfo))!=(-1));
	*p='\0';
	return ret;
#else
	strncpy(filefind->path,path,NAME_MAX);
	filefind->path[NAME_MAX]='\0';
	if (filefind->handle=opendir(path))
		return FileFindNext(filefind,fileinfo);

	return FALSE;
#endif
}

bool FileFindNext(TFileFind *filefind,TFileInfo *fileinfo)
{
#ifdef _WIN32
	return (_findnext(*filefind,fileinfo)!=(-1));
#else
	struct dirent *de;
	/****** Must be changed ***/
	char z[NAME_MAX+1];

	if (de=readdir(filefind->handle))
	{
		struct stat fs;

		strcpy(fileinfo->name,de->d_name);
		strcpy(z,filefind->path);
		strncat(z,"/",NAME_MAX);
		strncat(z,fileinfo->name,NAME_MAX);
		z[NAME_MAX]='\0';
		
		stat(z,&fs);

		if (fs.st_mode & S_IFDIR)
			fileinfo->attrib=A_SUBDIR;
		else
			fileinfo->attrib=0;

		fileinfo->size=fs.st_size;
		fileinfo->time_write=fs.st_mtime;
		
		return TRUE;
	};

	return FALSE;
#endif
}

void FileFindClose(TFileFind *filefind)
{
#ifdef _WIN32
	_findclose(*filefind);
#else
	closedir(filefind->handle);
#endif
}

 

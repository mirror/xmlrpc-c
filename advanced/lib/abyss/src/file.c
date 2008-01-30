/******************************************************************************
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
******************************************************************************/

#include "xmlrpc_config.h"

#define _CRT_SECURE_NO_WARNINGS
    /* Tell msvcrt not to warn about functions that are often misused and
       cause security exposures.
    */

#include <string.h>

#if MSVCRT
#include <io.h>
#endif

#if !MSVCRT
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"
#include "file.h"

abyss_bool const win32 =
#ifdef WIN32
TRUE;
#else
FALSE;
#endif


/*********************************************************************
** File
*********************************************************************/

abyss_bool
FileOpen(TFile *      const f,
         const char * const name,
         uint32_t     const attrib) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return ((*f=_open(name,attrib))!=(-1));
#else
    return ((*f=open(name,attrib))!=(-1));
#endif
}



abyss_bool
FileOpenCreate(TFile *      const f,
               const char * const name,
               uint32_t     const attrib) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return ((*f=_open(name,attrib | O_CREAT,_S_IWRITE | _S_IREAD))!=(-1));
#else
    return ((*f=open(name,attrib | O_CREAT,S_IWRITE | S_IREAD))!=(-1));
#endif
}



abyss_bool
FileWrite(TFile *      const f,
          const void * const buffer,
          uint32_t     const len) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_write(*f,buffer,len)==(int32_t)len);
#else
    return (write(*f,buffer,len)==(int32_t)len);
#endif
}



int32_t
FileRead(const TFile * const fileP,
         void *        const buffer,
         uint32_t      const len) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_read(*fileP, buffer, len));
#else
    return (read(*fileP, buffer, len));
#endif
}



abyss_bool
FileSeek(const TFile * const fileP,
         uint64_t      const pos,
         uint32_t      const attrib) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_lseek(*fileP, (long)pos, attrib)!=(-1));
#else
    return (lseek(*fileP, pos, attrib)!=(-1));
#endif
}



uint64_t
FileSize(const TFile * const fileP) {

#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_filelength(*fileP));
#else
    struct stat fs;

    fstat(*fileP, &fs);
    return (fs.st_size);
#endif  
}



abyss_bool
FileClose(TFile * const f) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_close(*f)!=(-1));
#else
    return (close(*f)!=(-1));
#endif
}



abyss_bool
FileStat(const char * const filename,
         TFileStat *  const filestat) {
#if defined( WIN32 ) && !defined( __BORLANDC__ )
    return (_stati64(filename,filestat)!=(-1));
#else
    return (stat(filename,filestat)!=(-1));
#endif
}



static void
fileFindFirstWin(TFileFind *  const filefind ATTR_UNUSED,
                 const char * const path,
                 TFileInfo *  const fileinfo ATTR_UNUSED,
                 abyss_bool * const retP     ATTR_UNUSED) {
    const char * search;

    xmlrpc_asprintf(&search, "%s\\*", path);

#ifdef WIN32
#ifndef __BORLANDC__
    *retP = (((*filefind) = _findfirst64(search, fileinfo)) != -1);
#else
    *filefind = FindFirstFile(search, &fileinfo->data);
    *retP = *filefind != INVALID_HANDLE_VALUE;
    if (*retP) {
        LARGE_INTEGER li;
        li.LowPart = fileinfo->data.nFileSizeLow;
        li.HighPart = fileinfo->data.nFileSizeHigh;
        strcpy( fileinfo->name, fileinfo->data.cFileName );
        fileinfo->attrib = fileinfo->data.dwFileAttributes;
        fileinfo->size   = li.QuadPart;
        fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
    }
#endif
#endif /* WIN32 */
    xmlrpc_strfree(search);
}



static void
fileFindFirstPosix(TFileFind *  const filefind,
                   const char * const path,
                   TFileInfo *  const fileinfo,
                   abyss_bool * const retP) {
    
#ifndef WIN32
    strncpy(filefind->path, path, NAME_MAX);
    filefind->path[NAME_MAX] = '\0';
    filefind->handle = opendir(path);
    if (filefind->handle)
        *retP = FileFindNext(filefind, fileinfo);
    else
        *retP = FALSE;
#endif
}
    


abyss_bool
FileFindFirst(TFileFind *  const filefind,
              const char * const path,
              TFileInfo *  const fileinfo) {

    abyss_bool ret;

    if (win32)
        fileFindFirstWin(filefind, path, fileinfo, &ret);
    else
        fileFindFirstPosix(filefind, path, fileinfo, &ret);

    return ret;
}



abyss_bool
FileFindNext(TFileFind * const filefind,
             TFileInfo * const fileinfo) {
#ifdef WIN32

#ifndef __BORLANDC__
    return (_findnext64(*filefind,fileinfo)!=(-1));
#else
   abyss_bool ret = FindNextFile( *filefind, &fileinfo->data );
   if( ret )
   {
      LARGE_INTEGER li;
      li.LowPart = fileinfo->data.nFileSizeLow;
      li.HighPart = fileinfo->data.nFileSizeHigh;
      strcpy( fileinfo->name, fileinfo->data.cFileName );
       fileinfo->attrib = fileinfo->data.dwFileAttributes;
       fileinfo->size   = li.QuadPart;
      fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
   }
    return ret;
#endif

#else  /* WIN32 */
    struct dirent *de;
    /****** Must be changed ***/
    char z[NAME_MAX+1];

    de=readdir(filefind->handle);
    if (de)
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
#endif /* WIN32 */
}

void
FileFindClose(TFileFind * const filefind) {
#ifdef WIN32

#ifndef __BORLANDC__
    _findclose(*filefind);
#else
   FindClose( *filefind );
#endif

#else  /* WIN32 */
    closedir(filefind->handle);
#endif /* WIN32 */
}

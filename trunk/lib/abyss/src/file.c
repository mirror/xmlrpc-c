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
#if MSVCRT
    return ((*f=_open(name,attrib))!=(-1));
#else
    return ((*f=open(name,attrib))!=(-1));
#endif
}



abyss_bool
FileOpenCreate(TFile *      const f,
               const char * const name,
               uint32_t     const attrib) {
#if MSVCRT
    return ((*f=_open(name,attrib | O_CREAT,_S_IWRITE | _S_IREAD))!=(-1));
#else
    return ((*f=open(name,attrib | O_CREAT,S_IWRITE | S_IREAD))!=(-1));
#endif
}



abyss_bool
FileWrite(TFile *      const f,
          const void * const buffer,
          uint32_t     const len) {
#if MSVCRT
    return (_write(*f,buffer,len)==(int32_t)len);
#else
    return (write(*f,buffer,len)==(int32_t)len);
#endif
}



int32_t
FileRead(const TFile * const fileP,
         void *        const buffer,
         uint32_t      const len) {
#if MSVCRT
    return (_read(*fileP, buffer, len));
#else
    return (read(*fileP, buffer, len));
#endif
}



abyss_bool
FileSeek(const TFile * const fileP,
         uint64_t      const pos,
         uint32_t      const attrib) {
#if MSVCRT
    return (_lseek(*fileP, (long)pos, attrib)!=(-1));
#else
    return (lseek(*fileP, pos, attrib)!=(-1));
#endif
}



uint64_t
FileSize(const TFile * const fileP) {

#if MSVCRT
    return (_filelength(*fileP));
#else
    struct stat fs;

    fstat(*fileP, &fs);
    return (fs.st_size);
#endif  
}



abyss_bool
FileClose(TFile * const f) {
#if MSVCRT
    return (_close(*f)!=(-1));
#else
    return (close(*f)!=(-1));
#endif
}



abyss_bool
FileStat(const char * const filename,
         TFileStat *  const filestat) {
#if MSVCRT
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

#if MSVCRT
    *retP = (((*filefind) = _findfirst64(search, fileinfo)) != -1);
#else
#ifdef WIN32
    *filefind = FindFirstFile(search, &fileinfo->data);
    *retP = *filefind != NULL;
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
#endif
    xmlrpc_strfree(search);
}



static void
fileFindFirstPosix(TFileFind *  const filefind,
                   const char * const path,
                   TFileInfo *  const fileinfo,
                   abyss_bool * const retP) {
    
#if !MSVCRT
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



static void
fileFindNextWin(TFileFind *  const filefind ATTR_UNUSED,
                TFileInfo *  const fileinfo ATTR_UNUSED,
                abyss_bool * const retvalP  ATTR_UNUSED) {

#if MSVCRT
    *retvalP = _findnext64(*filefind,fileinfo) != -1;
#else
#ifdef WIN32
    abyss_bool found;
    found = FindNextFile(*filefind, &fileinfo->data);
    if (found) {
        LARGE_INTEGER li;
        li.LowPart = fileinfo->data.nFileSizeLow;
        li.HighPart = fileinfo->data.nFileSizeHigh;
        strcpy(fileinfo->name, fileinfo->data.cFileName);
        fileinfo->attrib = fileinfo->data.dwFileAttributes;
        fileinfo->size   = li.QuadPart;
        fileinfo->time_write = fileinfo->data.ftLastWriteTime.dwLowDateTime;
    }
    *retvalP = found;
#endif
#endif
}



static void
fileFindNextPosix(TFileFind *  const filefindP,
                  TFileInfo *  const fileinfoP,
                  abyss_bool * const retvalP) {

#ifndef WIN32
    struct dirent * deP;

    deP = readdir(filefindP->handle);
    if (deP) {
        char z[NAME_MAX+1];
        struct stat fs;

        strcpy(fileinfoP->name, deP->d_name);
        strcpy(z, filefindP->path);
        strncat(z, "/",NAME_MAX);
        strncat(z, fileinfoP->name, NAME_MAX);
        z[NAME_MAX] = '\0';
        
        stat(z, &fs);

        if (fs.st_mode & S_IFDIR)
            fileinfoP->attrib = A_SUBDIR;
        else
            fileinfoP->attrib = 0;

        fileinfoP->size       = fs.st_size;
        fileinfoP->time_write = fs.st_mtime;
        
        *retvalP = TRUE;
    } else
        *retvalP = FALSE;
#endif
}



abyss_bool
FileFindNext(TFileFind * const filefind,
             TFileInfo * const fileinfo) {

    abyss_bool retval;

    if (win32)
        fileFindNextWin(filefind, fileinfo, &retval);
    else
        fileFindNextPosix(filefind, fileinfo, &retval);

    return retval;
}



void
FileFindClose(TFileFind * const filefindP) {
#ifdef WIN32
#if MSVCRT
    _findclose(*filefindP);
#else
   FindClose(*filefindP);
#endif
#else
    closedir(filefindP->handle);
#endif
}

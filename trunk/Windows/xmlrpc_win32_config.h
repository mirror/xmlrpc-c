/* Win32 version of xmlrpc_config.h.

   For other platforms, this is generated automatically, but for Windows,
   someone generates it manually.  Nonetheless, we keep it looking as much
   as possible like the automatically generated one to make it easier to
   maintain (e.g. you can compare the two and see why something builds
   differently for Windows that for some other platform).

   The purpose of this file is to define stuff particular to the build
   environment being used to build Xmlrpc-c.  Xmlrpc-c source files can
   #include this file and have build-environment-independent source code.

   A major goal of this file is to reduce conditional compilation in
   the other source files as much as possible.  Even more, we want to avoid
   having to generate source code particular to to a build environment
   except in this file.   

   This file is NOT meant to be used by any code outside of the
   Xmlrpc-c source tree.  There is a similar file that gets installed
   as <xmlrpc-c/config.h> that performs the same function for Xmlrpc-c
   interface header files that get compiled as part of a user's program.

   Logical macros are 0 or 1 instead of the more traditional defined and
   undefined.  That's so we can distinguish when compiling code between
   "false" and some problem with the code.
*/

#ifndef XMLRPC_CONFIG_H_INCLUDED
#define XMLRPC_CONFIG_H_INCLUDED

/* From xmlrpc_amconfig.h */

#define HAVE_SETGROUPS 0
#define HAVE_ASPRINTF 0
#define HAVE_SETENV 0
#define HAVE_PSELECT 0
#define HAVE_WCSNCMP 1
#define HAVE_LOCALTIME_R 0
#define HAVE_GMTIME_R 0
/* Name of package */
#define PACKAGE "xmlrpc-c"
/*----------------------------------*/

#define HAVE_WCHAR_H 1
#define HAVE_SYS_FILIO_H 0
#define HAVE_SYS_IOCTL_H 0

#define VA_LIST_IS_ARRAY 0

#define HAVE_LIBWWW_SSL 0

/* Used to mark an unused function parameter */
#define ATTR_UNUSED

#define DIRECTORY_SEPARATOR "\\"

#define HAVE_UNICODE_WCHAR 1

/*  Xmlrpc-c code uses __inline__ to declare functions that should
    be compiled as inline code.  GNU C recognizes the __inline__ keyword.
    Others recognize 'inline' or '__inline' or nothing at all to say
    a function should be inlined.

    We could make 'configure' simply do a trial compile to figure out
    which one, but for now, this approximation is easier:
*/
#if (!defined(__GNUC__))
  #if (!defined(__inline__))
    #if (defined(__sgi) || defined(_AIX) || defined(_MSC_VER))
      #define __inline__ __inline
    #else   
      #define __inline__
    #endif
  #endif
#endif

/* MSVCRT means we're using the Microsoft Visual C++ runtime library */

#ifdef _MSC_VER
/* The compiler is Microsoft Visual C++. */
  #define MSVCRT 1
#else
  #define MSVCRT 0
#endif


#define _CRT_SECURE_NO_DEPRECATE

#if !defined (vsnprintf)
  #define vsnprintf _vsnprintf
#endif
#if !defined (snprintf)
  #define snprintf _snprintf
#endif
#if !defined (popen)
  #define popen _popen
#endif
#if !defined (strtoll)
  #define strtoll strtol
#endif
#if !defined (strcasecmp)
  #define strcasecmp(a,b) stricmp((a),(b))
#endif

#endif

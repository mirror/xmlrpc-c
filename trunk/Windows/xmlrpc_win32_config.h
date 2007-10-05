/* Win32 version of xmlrpc_config.h */

#ifndef XMLRPC_CONFIG_H_INCLUDED
#define XMLRPC_CONFIG_H_INCLUDED

#pragma once

/* From xmlrpc_amconfig.h */

/* We have the wcsncmp function.  */
#define HAVE_WCSNCMP 1

/* We have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

#define HAVE_SYS_FILIO_H 0

#define HAVE_SYS_IOCTL_H 0

#define HAVE_SETENV 0

/* We have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Name of package */
#define PACKAGE "xmlrpc-c"


/*
   Logical macros are 0 or 1 instead of the more traditional defined and
   undefined.  That's so we can distinguish when compiling code between
   "false" and some problem with the code.
*/

#define _CRT_SECURE_NO_DEPRECATE

/* va_list is not an array. */
#define VA_LIST_IS_ARRAY 0

/* Used to mark unused variables under GCC... */
#define ATTR_UNUSED

#define HAVE_UNICODE_WCHAR 1

#define DIRECTORY_SEPARATOR "\\"


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

#define __inline__ __inline

#define XMLRPC_HAVE_WCHAR 1

#endif

/* This header was created by editing xmlrpc_config.h until it worked under
** Windows. */
  
/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */
  
/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1
  
/* Define if va_list is actually an array. */
/* #undef VA_LIST_IS_ARRAY */
  
/* Define if we're using a copy of libwww with built-in SSL support. */
/* #undef HAVE_LIBWWW_SSL */

/* Define if you have the setgroups function.  */
/* #undef HAVE_SETGROUPS */

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1
  
/* Name of package */
#define PACKAGE "xmlrpc-c"
  
/* Version number of package */
#define VERSION "0.9.10"

/* Windows-specific includes. */  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
  #if !defined (vsnprintf)
#define vsnprintf _vsnprintf
  #endif
#include <time.h>
#include <WINSOCK.h>
#include <DIRECT.h>   /* for _chdir() */

/* Required for compatability with libWWW headers
** Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Use this to mark unused variables under GCC...
** #define ATTR_UNUSED __attribute__((__unused__))
**  ...otherwise use this */
#define ATTR_UNUSED

/* Define this if your C library provides reasonably complete and correct
** Unicode wchar_t support. */
#define HAVE_UNICODE_WCHAR 1

/* Define if you have the wcsncmp function.  */
#define HAVE_WCSNCMP 1

/* The directory containing our source code.  This is used by some of
** the build-time test suites to locate their test data (because automake
** and autoconf may not compile our code in a separate build directory. */
#define SRCDIR ""

/* Define if we're building the "wininet" xmlrpc_client. */
/* #undef BUILD_WININET_CLIENT 0  */


/* Define if we're building the "libwww" xmlrpc_client. */
/* #undef BUILD_LIBWWW_CLIENT 0 */


/* Define if we're building the "curl" xmlrpc_client. */
/* #undef BUILD_CURL_CLIENT 0 */


/* Define your preferred transport. Use 0L if you are unsure. */
#define XMLRPCDEFAULTTRANSPORT "wininet"


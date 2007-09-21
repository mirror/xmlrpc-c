/* From xmlrpc_amconfig.h */
  
/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the setgroups function.  */
/* #undef HAVE_SETGROUPS */

/* #undef HAVE_ASPRINTF */

/* Define if you have the wcsncmp function.  */
#define HAVE_WCSNCMP 1

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <sys/filio.h> header file.  */
/* #undef HAVE_SYS_FILIO_H */

/* Define if you have the <sys/ioctl.h> header file.  */
/* #undef HAVE_SYS_IOCTL_H 1 */

/* Define if you have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Name of package */
#define PACKAGE "xmlrpc-c"


/* Win32 version of xmlrpc_config.h

   Logical macros are 0 or 1 instead of the more traditional defined and
   undefined.  That's so we can distinguish when compiling code between
   "false" and some problem with the code.
*/

/* Define if va_list is actually an array. */
#define VA_LIST_IS_ARRAY 0
  
/* Define if we're using a copy of libwww with built-in SSL support. */
#define HAVE_LIBWWW_SSL 0

/* Used to mark unused variables under GCC... */
#define ATTR_UNUSED

/* Define this if your C library provides reasonably complete and correct Unicode wchar_t support. */
#define HAVE_UNICODE_WCHAR 1
  
#define DIRECTORY_SEPARATOR "\\"

/* This gives the path to the testdata dir.  It is relative to the  */
/* output directory of cpptest and rpctest.  Thus, you must run     */
/* the test apps from the .\Bin directory for them to work unless   */
/* you modify this defined value to a hardcoded path                */
#define TESTDATADIR "..\\src\\testdata"

  
/* Windows-specific includes. */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
  #if !defined (vsnprintf)
#define vsnprintf _vsnprintf
  #endif
  #if !defined (snprintf)
#define snprintf _snprintf
  #endif
#include <time.h>
#include <WINSOCK.h>
#include <direct.h>  /* for _chdir() */

#define __inline__ __inline

__inline BOOL setenv(const char* name, const char* value, int i) 
{
	return (SetEnvironmentVariable(name, value) != 0) ? TRUE : FALSE;
}

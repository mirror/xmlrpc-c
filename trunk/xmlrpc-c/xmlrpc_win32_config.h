/* This header was created by editing xmlrpc_config.h until it worked under
** Windows. */
  
/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */
  
/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1
  
/* Define if va_list is actually an array. */
/* #undef VA_LIST_IS_ARRAY */
  
/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1
  
/* Name of package */
#define PACKAGE "xmlrpc-c"
  
/* Version number of package */
#define VERSION "0.9.3"

/* Windows-specific includes. */  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
  #if !defined (vsnprintf)
#define vsnprintf _vsnprintf
  #endif
#include <time.h>
#include <WINSOCK.h>

#ifndef XMLRPC_C_UTIL_INT_H_INCLUDED
#define XMLRPC_C_UTIL_INT_H_INCLUDED

#include "util.h"

/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)

/* GNU_PRINTF_ATTR lets the GNU compiler check printf-type
   calls to be sure the arguments match the format string, thus preventing
   runtime segmentation faults and incorrect messages.
*/
#ifdef __GNUC__
#define GNU_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
#define GNU_PRINTF_ATTR(a,b)
#endif


#endif

/* This takes the place of C99 inttypes.h, which at least some Windows
   compilers don't have.  (October 2007).
*/

/* PRId64 is the printf-style format specifier for a long long type, as in
   long long mynumber = 5;
   printf("My number is %" PRId64 ".\n", mynumber

*/

#ifdef _MSC_VER
#  define PRId64 "I64"

#ifndef uint32_t
typedef unsigned int      uint32_t;
#endif
#ifndef uint64_t
typedef unsigned __int64  uint64_t;
#endif
#ifndef  uint
typedef  unsigned int   uint;
#endif

#else
#  include <inttypes.h>
#endif


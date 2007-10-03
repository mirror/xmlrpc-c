/* This takes the place of C99 inttypes.h, which at least some Windows
   compilers don't have.  (October 2007).
*/

/* PRId64 is the printf-style format specifier for a long long type, as in
   long long mynumber = 5;
   printf("My number is %" PRId64 ".\n", mynumber

   Xmlrpc-c uses uint32_t; in Windows compilers we've seen, that is built-in.
*/

#ifdef _MSC_VER
#  define PRId64 "I64"
#else
#  include <inttypes.h>
#endif


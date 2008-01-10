/* This takes the place of C99 inttypes.h, which at least some Windows
   compilers don't have.  (October 2007).
*/

/* PRId64 is the printf-style format specifier for a long long type, as in
   long long mynumber = 5;
   printf("My number is %" PRId64 ".\n", mynumber);

*/

#ifdef _MSC_VER
#  define PRId64 "I64d"
#  define PRIu64 "I64u"

#ifndef uint16_t
typedef unsigned short    uint16_t;
#endif
#ifndef int32_t
typedef int               int32_t;
#endif
#ifndef uint32_t
typedef unsigned int      uint32_t;
#endif
#ifndef uint64_t
typedef unsigned __int64  uint64_t;
#endif
#ifndef  uint
typedef  unsigned int   uint;
#endif
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif
#ifndef int16_t
typedef short int16_t;
#endif

/* Here we define intptr_t (another type that in C99 is defined in
   <inttypes.h>).  The Windows runtime library _uses_ this type
   (functions in <io.h>, for example), but does not provide a header file
   like inttypes.h to define it.  Instead, individual header files that
   use it (e.g. <io.h>) each define it with code like the following.
*/

#ifndef _INTPTR_T_DEFINED
#define _INTPTR_T_DEFINED
  #ifdef _WIN64
    typedef __int64 intptr_t;
  #else
    typedef _W64 int intptr_t;
  #endif
#endif

#else /* _MSC_VER */
  #include <inttypes.h>
#endif


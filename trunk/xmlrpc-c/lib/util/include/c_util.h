#ifndef C_UTIL_H_INCLUDED
#define C_UTIL_H_INCLUDED

/* C language stuff.  Doesn't involve any libraries that aren't part of
   the compiler.
*/

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

/* GNU_PRINTF_ATTR lets the GNU compiler check printf-type
   calls to be sure the arguments match the format string, thus preventing
   runtime segmentation faults and incorrect messages.
*/
#ifdef __GNUC__
#define GNU_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
#define GNU_PRINTF_ATTR(a,b)
#endif

/*----------------------------------------------------------------------------
   We need a special version of va_list in order to pass around the
   variable argument heap by reference, thus allowing a subroutine to
   advance its pointer.

   On some systems (e.g. Gcc for PPC or AMD64), va_list is an array.
   That invites the scourge of array-to-pointer degeneration if you try
   to take its address.  Burying it inside a struct as we do with out
   va_listx type makes it immune.

   Example of what would happen if we used va_list instead of va_listx,
   on a system where va_list is an array:

      void sub2(va_list * argsP) [
          ...
      }

      void sub1(va_list args) {
          sub2(&args);
      }

      This doesn't work.  '&args' is the same thing as 'args', so is
      va_list, not va_list *.  The compiler will even warn you about the
      pointer type mismatch.
-----------------------------------------------------------------------------*/


typedef struct {
/*----------------------------------------------------------------------------
   Same thing as va_list, but in a form that works everywhere.  See above.
-----------------------------------------------------------------------------*/
    va_list v;
} va_listx;


#endif

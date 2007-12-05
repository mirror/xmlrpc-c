#include "xmlrpc_config.h"
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#if MSVCRT
#include <windows.h>
#endif

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"



#if MSVCRT
static void
gettimeofdayWindows(struct timeval * const tvP) {
#define EPOCH_FILETIME 11644473600000000Ui64
    /* Number of micro-seconds between the beginning of the Windows epoch
       (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
    */

    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;

    GetSystemTimeAsFileTime(&ft);
    li.LowPart  = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    t  = li.QuadPart / 10   /* In micro-second intervals */
        - EPOCH_FILETIME;     /* Offset to the Epoch time */
    tvP->tv_sec  = (long)(t / 1000000);
    tvP->tv_usec = (long)(t % 1000000);
}
#endif



void
xmlrpc_gettimeofday(struct timeval * const tvP) {

    assert(tvP);

#if HAVE_GETTIMEOFDAY
    gettimeofday(tvP, NULL);
#else
#if MSVCRT
    gettimeofdayWindows(tvP);
#else
  #error "We don't know how to get the time of day on this system"
#endif
#endif /* HAVE_GETTIMEOFDAY */
}



static bool
isLeapYear(unsigned int const yearOfAd) {

    return
        (yearOfAd % 4) == 0 &&
        ((yearOfAd % 100) != 0 || (yearOfAd % 400) == 0);
}



void
xmlrpc_timegm(const struct tm  * const tmP,
              time_t *           const timeValueP,
              const char **      const errorP) {
/*----------------------------------------------------------------------------
   This does what GNU libc's timegm() does.
-----------------------------------------------------------------------------*/
    if (tmP->tm_year < 70 ||
        tmP->tm_mon  > 11 ||
        tmP->tm_mon  <  0 ||
        tmP->tm_mday > 31 ||
        tmP->tm_min  > 60 ||
        tmP->tm_sec  > 60 ||
        tmP->tm_hour > 24) {

        xmlrpc_asprintf(errorP, "Invalid time specification; a member "
                        "of struct tm is out of range");
    } else {
        static unsigned int const monthDaysNonLeap[12] = 
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        unsigned int totalDays;
        unsigned int year;
        unsigned int month;

        totalDays = 0;  /* initial value */
    
        for (year = 70; year < (unsigned int)tmP->tm_year; ++year)
            totalDays += isLeapYear(1900 + year) ? 366 : 365;
    
        for (month = 0; month < (unsigned int)tmP->tm_mon; ++month)
            totalDays += monthDaysNonLeap[month];

        if (tmP->tm_mon > 1 && isLeapYear(1900 + tmP->tm_year))
            totalDays += 1;

        totalDays += tmP->tm_mday - 1;

        *errorP = NULL;

        *timeValueP = ((totalDays * 24 +
                        tmP->tm_hour) * 60 +
                        tmP->tm_min) * 60 +
                        tmP->tm_sec;
    }
}



void
xmlrpc_localtime(time_t      const datetime,
                 struct tm * const tmP) {
/*----------------------------------------------------------------------------
   Convert datetime from standard to broken-down format in the local
   time zone.

   For Windows, this is not thread-safe.  If you run a version of Abyss
   with multiple threads, you can get arbitrary results here.
-----------------------------------------------------------------------------*/
#if HAVE_LOCALTIME_R
  localtime_r(&datetime, tmP);
#else
  *tmP = *localtime(&datetime);
#endif
}



void
xmlrpc_gmtime(time_t      const datetime,
              struct tm * const resultP) {
/*----------------------------------------------------------------------------
   Convert datetime from standard to broken-down UTC format.

   For Windows, this is not thread-safe.  If you run a version of Abyss
   with multiple threads, you can get arbitrary results here.
-----------------------------------------------------------------------------*/

#if HAVE_GMTIME_R
    gmtime_r(&datetime, resultP);
#else
    *resultP = *gmtime(&datetime);
#endif
}

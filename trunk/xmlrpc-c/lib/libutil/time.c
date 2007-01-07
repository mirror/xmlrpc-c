#include <time.h>

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"



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

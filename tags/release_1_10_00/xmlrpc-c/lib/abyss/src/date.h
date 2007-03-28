#ifndef DATE_H_INCLUDED
#define DATE_H_INCLUDED

#include <time.h>

#include "xmlrpc-c/abyss.h"

void
DateToString(time_t        const datetime,
             const char ** const dateStringP);

void
DateToLogString(time_t        const datetime,
                const char ** const dateStringP);

void
DateDecode(const char * const dateString,
           abyss_bool * const validP,
           time_t *     const datetimeP);

#endif

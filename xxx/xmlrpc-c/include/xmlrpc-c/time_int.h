#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

#include <time.h>
#include "xmlrpc-c/util.h"

void
xmlrpc_timegm(const struct tm  * const brokenTime,
              time_t *           const timeValueP,
              const char **      const errorP);

#endif

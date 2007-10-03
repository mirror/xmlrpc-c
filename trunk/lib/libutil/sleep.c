#include "xmlrpc-c/sleep_int.h"

#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif


void
xmlrpc_millisecond_sleep(unsigned int const milliseconds) {

#ifdef WIN32
    SleepEx(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

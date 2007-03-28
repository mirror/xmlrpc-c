#define _XOPEN_SOURCE 600  /* Get pselect() in <sys/select.h> */

#include <sys/select.h>
#include <signal.h>
#include <time.h>

#include "xmlrpc-c/select_int.h"

int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const struct timespec * const timeoutP,
               sigset_t *              const sigmaskP) {

    return pselect(n, readfdsP, writefdsP, exceptfdsP, timeoutP, sigmaskP);
}

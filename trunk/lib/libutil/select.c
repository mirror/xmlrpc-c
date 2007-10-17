#define _XOPEN_SOURCE 600  /* Get pselect() in <sys/select.h> */

#include <sys/select.h>
#include <signal.h>
#include <time.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/select_int.h"

int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const struct timespec * const timeoutP,
               sigset_t *              const sigmaskP) {

    int retval;

#if HAVE_PSELECT
    retval = pselect(n, readfdsP, writefdsP, exceptfdsP, timeoutP, sigmaskP);
#else
    sigset_t origmask;

    sigprocmask(SIG_SETMASK, sigmaskP, &origmask);
    retval = select(n, readfdsP, writefdsP, exceptfdsP, timeoutP);
    sigprocmask(SIG_SETMASK, &origmask, NULL);
#endif

    return retval;
}

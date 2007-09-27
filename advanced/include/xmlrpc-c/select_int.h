#ifndef SELECT_INT_H_INCLUDED
#define SELECT_INT_H_INCLUDED

#include <sys/select.h>
#include <signal.h>
#include <time.h>

int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const struct timespec * const timeoutP,
               sigset_t *              const sigmaskP);

#endif

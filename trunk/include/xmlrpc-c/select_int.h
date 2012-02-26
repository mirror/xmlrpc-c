#ifndef SELECT_INT_H_INCLUDED
#define SELECT_INT_H_INCLUDED

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <signal.h>

#include "xmlrpc-c/c_util.h"
#include "xmlrpc-c/time_int.h"
#if MSVCRT
#ifndef sigset_t
typedef int sigset_t;
#endif
#endif

/*
  XMLRPC_UTIL_EXPORTED marks a symbol in this file that is exported from
  libxmlrpc_util.

  XMLRPC_BUILDING_UTIL says this compilation is part of libxmlrpc_util, as
  opposed to something that _uses_ libxmlrpc_util.
*/
#ifdef XMLRPC_BUILDING_UTIL
#define XMLRPC_UTIL_EXPORTED XMLRPC_DLLEXPORT
#else
#define XMLRPC_UTIL_EXPORTED
#endif

XMLRPC_UTIL_EXPORTED
int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const xmlrpc_timespec * const timeoutP,
               sigset_t *              const sigmaskP);

#endif

#ifndef LOCK_PLATFORM_H_INCLUDED
#define LOCK_PLATFORM_H_INCLUDED

#include "xmlrpc-c/lock.h"

#ifdef __cplusplus
extern "C" {
#endif

lock *
xmlrpc_lock_create(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

/*********************************************************************
** Thread
*********************************************************************/

#include "xmlrpc-c/abyss.h"  /* for abyss_bool */

typedef struct abyss_thread TThread;

void
ThreadPoolInit(void);

typedef void TThreadProc(void * const userHandleP);
typedef void TThreadDoneFn(void * const userHandleP);

void
ThreadCreate(TThread **      const threadPP,
             void *          const userHandle,
             TThreadProc   * const func,
             TThreadDoneFn * const threadDone,
             abyss_bool      const useSigchld,
             const char **   const errorP);

abyss_bool
ThreadRun(TThread * const threadP);

abyss_bool
ThreadStop(TThread * const threadP);

abyss_bool
ThreadKill(TThread * const threadP);

void
ThreadWaitAndRelease(TThread * const threadP);

void
ThreadExit(int const retValue);

void
ThreadRelease(TThread * const threadP);

abyss_bool
ThreadForks(void);

void
ThreadUpdateStatus(TThread * const threadP);

#ifndef WIN32
void
ThreadHandleSigchld(pid_t const pid);
#endif

/*********************************************************************
** Mutex
*********************************************************************/

typedef struct abyss_mutex TMutex;

abyss_bool
MutexCreate(TMutex ** const mutexP);

abyss_bool
MutexLock(TMutex * const mutexP);

abyss_bool
MutexUnlock(TMutex * const mutexP);

abyss_bool
MutexTryLock(TMutex * const mutexP);

void
MutexDestroy(TMutex * const mutexP);

#endif

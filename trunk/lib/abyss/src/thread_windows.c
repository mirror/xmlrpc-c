/* This code is just a first draft by someone who doesn't know anything
   about Windows.  It has never been compiled.  It is just a framework
   for someone who knows Windows to pick and finish.

   Bryan Henderson redesigned the threading structure for Abyss in
   April 2006 and wrote this file then.  He use the former
   threading code, which did work on Windows, as a basis.
*/


#include <process.h>
#include <windows.h>

#include "xmlrpc_config.h"

#include "mallocvar.h"
#include "xmlrpc-c/string_int.h"

#include "xmlrpc-c/abyss.h"

#include "thread.h"



struct abyss_thread {
    HANDLE handle;
    TThreadProc *   func;
    TThreadDoneFn * threadDone;
};

#define  THREAD_STACK_SIZE (16*1024L)


typedef uint32_t (WINAPI WinThreadProc)(void *);


static WinThreadProc threadRun;

static uint32_t
threadRun(void * const arg) {

    struct abyss_thread * const threadP = arg;

    threadP->func(threadP->userHandle);

    threadP->threadDone(threadP->userHandle);
}



void
ThreadCreate(TThread **      const threadPP,
             void *          const userHandle,
             TThreadProc   * const func,
             TThreadDoneFn * const threadDone,
             abyss_bool      const useSigchld,
             const char **   const errorP) {

    DWORD z;
    TThread * threadP;

    MALLOCVAR(threadP);

    if (threadP == NULL)
        xmlrpc_asprintf(errorP,
                        "Can't allocate memory for thread descriptor.");
    else {
        threadP->userHandle = userHandle;
        threadP->func       = func;
        threadP->threadDone = threadDone;

        threadP->handle = _beginthreadex(NULL, THREAD_STACK_SIZE, func, 
                                         threadP,
                                         CREATE_SUSPENDED, &z);
        if (threadP->handle == NULL)
            xmlrpc_asprintf(errorP, "_beginthreadex() failed.");
        else {
            *errorP = NULL;
            *threadPP = threadP;
        }
        if (*errorP)
            free(threadP);
    }
}



abyss_bool
ThreadRun(TThread * const threadP) {
    return (ResumeThread(threadP->handle) != 0xFFFFFFFF);
}



abyss_bool
ThreadStop(TThread * const threadP) {

    return (SuspendThread(threadP->handle) != 0xFFFFFFFF);
}



abyss_bool
ThreadKill(TThread * const threadP) {
    return (TerminateThread(threadP->handle, 0) != 0);
}



void
ThreadWaitAndRelease(TThread * const threadP) {

    ThreadRelease(threadP);
}



void
ThreadExit(int const retValue) {

    _endthreadex(retValue);
}



void
ThreadRelease(TThread * const threadP) {

    CloseHandle(threadP->handle);
}



abyss_bool
ThreadForks(void) {

    return FALSE;
}



/*********************************************************************
** Mutex
*********************************************************************/

struct abyss_mutex {
    HANDLE winMutex;
};


abyss_bool
MutexCreate(TMutex * const mutexP) {
    
    mutexP->winMutex = CreateMutex(NULL, FALSE, NULL);

    return (mutexP->winMutex != NULL);
}



abyss_bool
MutexLock(TMutex * const mutexP) {

    return (WaitForSingleObject(mutexP->HANDLE, INFINITE) != WAIT_TIMEOUT);
}



abyss_bool
MutexUnlock(TMutex * const mutexP) {
    return ReleaseMutex(mutexP->HANDLE);
}



abyss_bool
MutexTryLock(TMutex * const mutexP) {
    return (WaitForSingleObject(mutexP->HANDLE, 0) != WAIT_TIMEOUT);
}



void
MutexFree(TMutex * const mutexP) {
    CloseHandle(mutexP->HANDLE);
}

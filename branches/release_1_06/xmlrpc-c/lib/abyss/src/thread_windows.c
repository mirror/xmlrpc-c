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
};

#define  THREAD_STACK_SIZE (16*1024L)


typedef uint32_t (WINAPI WinThreadProc)(void *);


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
        threadP->handle = _beginthreadex(NULL, THREAD_STACK_SIZE, func, 
                                         (WinThreadProc)arg,
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



abyss_bool
MutexCreate(TMutex * const mutexP) {
    
    *mutexP = CreateMutex(NULL, FALSE, NULL);

    return (*mutexP != NULL);
}



abyss_bool
MutexLock(TMutex * const mutexP) {

    return (WaitForSingleObject(*mutexP, INFINITE) != WAIT_TIMEOUT);
}



abyss_bool
MutexUnlock(TMutex * const mutexP) {
    return ReleaseMutex(*mutexP);
}



abyss_bool
MutexTryLock(TMutex * const mutexP) {
    return (WaitForSingleObject(*mutexP, 0) != WAIT_TIMEOUT);
}



void
MutexFree(TMutex * const mutexP) {
    CloseHandle(*mutexP);
}

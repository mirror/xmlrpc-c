/*******************************************************************************
**
** thread.c
**
** This file is part of the ABYSS Web server project.
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
*******************************************************************************/

#include "abyss.h"

/*********************************************************************
** Thread
*********************************************************************/

bool ThreadCreate(TThread *t,uint32 (*func)(void *),void *arg)
{
#ifdef _WIN32
	DWORD z;

	*t=CreateThread(NULL,4096,(LPTHREAD_START_ROUTINE)func,(LPVOID)arg,CREATE_SUSPENDED,&z);

	return (*t!=NULL);
#else
#	ifdef _UNIX
#		ifdef _THREAD
	if (pthread_create(t,NULL,(PTHREAD_START_ROUTINE)func,arg)==0)
		return (pthread_detach(*t)==0);

	return FALSE;
#		else
#			ifdef _FORK	
	switch (fork())
	{
	case 0:
		(*func)(arg);
	 	exit(0);
	case (-1):
		return FALSE;
	};
	
	return TRUE;
#			else
	(*func)(arg);
	return TRUE;
#			endif	/* _FORK */
#		endif	/* _THREAD */
#	else
	(*func)(arg);
	return TRUE;
#	endif	/*_UNIX */
#endif	/* _WIN32 */
}

bool ThreadRun(TThread *t)
{
#ifdef _WIN32
	return (ResumeThread(*t)!=0xFFFFFFFF);
#else
	return TRUE;	
#endif	/* _WIN32 */
}
bool ThreadStop(TThread *t)
{
#ifdef _WIN32
	return (SuspendThread(*t)!=0xFFFFFFFF);
#else
	return TRUE;
#endif	/* _WIN32 */
}

bool ThreadKill(TThread *t)
{
#ifdef _WIN32
	return (TerminateThread(*t,0)!=0);
#else
	/*return (pthread_kill(*t)==0);*/
	return TRUE;
#endif	/* _WIN32 */
}

void ThreadWait(uint32 ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms*1000);
#endif	/* _WIN32 */
}

/*********************************************************************
** Mutex
*********************************************************************/

bool MutexCreate(TMutex *m)
{
#ifdef _THREAD
	return (pthread_mutex_init(m, NULL)==0);
#elif defined(_WIN32)
	return ((*m=CreateMutex(NULL,FALSE,NULL))!=NULL);
#else
	return TRUE;
#endif	
}

bool MutexLock(TMutex *m)
{
#ifdef _THREAD
	return (pthread_mutex_lock(m)==0);
#elif defined(_WIN32)
	return (WaitForSingleObject(*m,INFINITE)!=WAIT_TIMEOUT);
#else
	return TRUE;
#endif
}

bool MutexUnlock(TMutex *m)
{
#ifdef _THREAD
	return (pthread_mutex_unlock(m)==0);
#elif defined(_WIN32)
	return ReleaseMutex(*m);
#else
	return TRUE;
#endif
}

bool MutexTryLock(TMutex *m)
{
#ifdef _THREAD
	return (pthread_mutex_trylock(m)==0);
#elif defined(_WIN32)
	return (WaitForSingleObject(*m,0)!=WAIT_TIMEOUT);
#else
	return TRUE;
#endif
}

void MutexFree(TMutex *m)
{
#ifdef _THREAD
	pthread_mutex_destroy(m);
#elif defined(_WIN32)
	CloseHandle(*m);
#else
	;
#endif
}

/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*****************************************************************************
** The following is a simple implementation of posix conditions
*****************************************************************************/

#undef SAFE_MUTEX			/* Avoid safe_mutex redefinitions */
#include "mysys_priv.h"
#if defined(THREAD) && defined(__WIN__)
#include <m_string.h>
#undef getpid
#include <process.h>
#include <sys/timeb.h>

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  cond->waiting=0;
  cond->semaphore=CreateSemaphore(NULL,0,0x7FFFFFFF,NullS);
  if (!cond->semaphore)
    return ENOMEM;
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	return CloseHandle(cond->semaphore) ? 0 : EINVAL;
}


int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  InterlockedIncrement(&cond->waiting);
  LeaveCriticalSection(mutex);
  WaitForSingleObject(cond->semaphore,INFINITE);
  InterlockedDecrement(&cond->waiting);
  EnterCriticalSection(mutex);
  return 0 ;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           struct timespec *abstime)
{
  struct _timeb curtime;
  int result;
  long timeout;
  _ftime(&curtime);
  timeout= ((long) (abstime->tv_sec - curtime.time)*1000L +
		    (long)((abstime->tv_nsec/1000) - curtime.millitm)/1000L);
  if (timeout < 0)				/* Some safety */
    timeout = 0L;
  InterlockedIncrement(&cond->waiting);
  LeaveCriticalSection(mutex);
  result=WaitForSingleObject(cond->semaphore,timeout);
  InterlockedDecrement(&cond->waiting);
  EnterCriticalSection(mutex);

  return result == WAIT_TIMEOUT ? ETIMEDOUT : 0;
}


int pthread_cond_signal(pthread_cond_t *cond)
{
  long prev_count;
  if (cond->waiting)
    ReleaseSemaphore(cond->semaphore,1,&prev_count);
  return 0;
}


int pthread_cond_broadcast(pthread_cond_t *cond)
{
  long prev_count;
  if (cond->waiting)
    ReleaseSemaphore(cond->semaphore,cond->waiting,&prev_count);
  return 0 ;
}


int pthread_attr_init(pthread_attr_t *connect_att)
{
  connect_att->dwStackSize	= 0;
  connect_att->dwCreatingFlag	= 0;
  connect_att->priority		= 0;
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *connect_att,DWORD stack)
{
  connect_att->dwStackSize=stack;
  return 0;
}

int pthread_attr_setprio(pthread_attr_t *connect_att,int priority)
{
  connect_att->priority=priority;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *connect_att)
{
  bzero((gptr) connect_att,sizeof(*connect_att));
  return 0;
}

/****************************************************************************
** Fix localtime_r() to be a bit safer
****************************************************************************/

struct tm *localtime_r(const time_t *timep,struct tm *tmp)
{
  if (*timep == (time_t) -1)			/* This will crash win32 */
  {
    bzero(tmp,sizeof(*tmp));
  }
  else
  {
    struct tm *res=localtime(timep);
    if (!res)                                   /* Wrong date */
    {
      bzero(tmp,sizeof(*tmp));                  /* Keep things safe */
      return 0;
    }
    *tmp= *res;
  }
  return tmp;
}
#endif /* __WIN__ */

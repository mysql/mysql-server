/*
 * -------------------------------------------------------------
 *
 * Module: my_semaphore.c  (Original: semaphore.c from pthreads library)
 *
 * Purpose:
 *	Semaphores aren't actually part of the PThreads standard.
 *	They are defined by the POSIX Standard:
 *
 *		POSIX 1003.1b-1993	(POSIX.1b)
 *
 * -------------------------------------------------------------
 *
 * Pthreads-win32 - POSIX Threads Library for Win32
 * Copyright (C) 1998
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 */

/*
  NEED_SEM is not used in MySQL and should only be needed under
  Windows CE.

  The big changes compared to the original version was to not allocate
  any additional memory in sem_init() but to instead store everthing
  we need in sem_t.

  TODO:
  To get HAVE_CREATESEMAPHORE we have to define the struct
  in my_semaphore.h
*/

#include "mysys_priv.h"
#ifdef __WIN__
#include "my_semaphore.h"
#include <errno.h>

/*
 DOCPUBLIC
      This function initializes an unnamed semaphore. the
      initial value of the semaphore is 'value'

 PARAMETERS
      sem	Pointer to an instance of sem_t

      pshared	If zero, this semaphore may only be shared between
		threads in the same process.
		If nonzero, the semaphore can be shared between
		processes

      value	Initial value of the semaphore counter

 RESULTS
      0		Successfully created semaphore,
     -1		Failed, error in errno

 ERRNO
      EINVAL	'sem' is not a valid semaphore,
      ENOSPC	A required resource has been exhausted,
      ENOSYS	Semaphores are not supported,
      EPERM	The process lacks appropriate privilege

*/

int
sem_init (sem_t *sem, int pshared, unsigned int value)
{
  int result = 0;

  if (pshared != 0)
  {
    /*
      We don't support creating a semaphore that can be shared between
      processes
     */
    result = EPERM;
  }
  else
  {
#ifndef HAVE_CREATESEMAPHORE
    sem->value = value;
    sem->event = CreateEvent(NULL,
			     FALSE,	/* manual reset */
			     FALSE,	/* initial state */
			     NULL);
    if (!sem->event)
      result = ENOSPC;
    else
    {
      if (value)
	SetEvent(sem->event);
      InitializeCriticalSection(&sem->sem_lock_cs);
    }
#else /* HAVE_CREATESEMAPHORE */
    *sem = CreateSemaphore (NULL,	 /* Always NULL */
			    value,	 /* Initial value */
			    0x7FFFFFFFL, /* Maximum value */
			    NULL);	 /* Name */
    if (!*sem)
      result = ENOSPC;
#endif /* HAVE_CREATESEMAPHORE */
  }
  if (result != 0)
  {
    errno = result;
    return -1;
  }
  return 0;
} /* sem_init */


/*
  DOCPUBLIC
       This function destroys an unnamed semaphore.

  PARAMETERS
       sem	Pointer to an instance of sem_t

 RESULTS
       0	Successfully destroyed semaphore,
       -1	Failed, error in errno
 ERRNO
       EINVAL	'sem' is not a valid semaphore,
       ENOSYS	Semaphores are not supported,
       EBUSY	Threads (or processes) are currently blocked on 'sem'
*/

int
sem_destroy (sem_t * sem)
{
  int result = 0;

#ifdef EXTRA_DEBUG
  if (sem == NULL || *sem == NULL)
  {
    errno=EINVAL;
    return;
  }
#endif /* EXTRA_DEBUG */

#ifndef HAVE_CREATESEMAPHORE
  if (! CloseHandle(sem->event))
    result = EINVAL;
  else
    DeleteCriticalSection(&sem->sem_lock_cs);
#else /* HAVE_CREATESEMAPHORE */
  if (!CloseHandle(*sem))
    result = EINVAL;
#endif /* HAVE_CREATESEMAPHORE */
  if (result)
  {
    errno = result;
    return -1;
  }
  *sem=0;					/* Safety */
  return 0;
} /* sem_destroy */


/*
 DOCPUBLIC
      This function tries to wait on a semaphore. If the
      semaphore value is greater than zero, it decreases
      its value by one. If the semaphore value is zero, then
      this function returns immediately with the error EAGAIN

 PARAMETERS
      sem	Pointer to an instance of sem_t

 RESULTS
      0		Successfully decreased semaphore,
      -1	Failed, error in errno

 ERRNO
      EAGAIN	The semaphore was already locked,
      EINVAL	'sem' is not a valid semaphore,
      ENOSYS	Semaphores are not supported,
      EINTR	The function was interrupted by a signal,
      EDEADLK	A deadlock condition was detected.
*/

int
sem_trywait(sem_t * sem)
{
#ifndef HAVE_CREATESEMAPHORE
  /* not yet implemented! */
  int errno = EINVAL;
  return -1;
#else /* HAVE_CREATESEMAPHORE */
#ifdef EXTRA_DEBUG
  if (sem == NULL || *sem == NULL)
  {
    errno=EINVAL;
    return -1;
  }
#endif /* EXTRA_DEBUG */
  if (WaitForSingleObject (*sem, 0) == WAIT_TIMEOUT)
  {
    errno= EAGAIN;
    return -1;
  }
  return 0;
#endif /* HAVE_CREATESEMAPHORE */

}				/* sem_trywait */


#ifndef HAVE_CREATESEMAPHORE

static void
ptw32_decrease_semaphore(sem_t * sem)
{
  EnterCriticalSection(&sem->sem_lock_cs);
  DBUG_ASSERT(sem->value != 0);
  sem->value--;
  if (sem->value != 0)
    SetEvent(sem->event);
  LeaveCriticalSection(&sem->sem_lock_cs);
}

static BOOL
ptw32_increase_semaphore(sem_t * sem, unsigned int n)
{
  BOOL result=FALSE;

  EnterCriticalSection(&sem->sem_lock_cs);
  if (sem->value + n > sem->value)
  {
    sem->value += n;
    SetEvent(sem->event);
    result = TRUE;
  }
  LeaveCriticalSection(&sem->sem_lock_cs);
  return result;
}

#endif /* HAVE_CREATESEMAPHORE */


/*
 ------------------------------------------------------
 DOCPUBLIC
      This function waits on a semaphore. If the
      semaphore value is greater than zero, it decreases
      its value by one. If the semaphore value is zero, then
      the calling thread (or process) is blocked until it can
      successfully decrease the value or until interrupted by
      a signal.

 PARAMETERS
      sem	Pointer to an instance of sem_t

 RESULTS
      0		Successfully decreased semaphore,
     -1		Failed, error in errno

 ERRNO
      EINVAL	'Sem' is not a valid semaphore,
      ENOSYS	Semaphores are not supported,
      EINTR	The function was interrupted by a signal,
      EDEADLK	A deadlock condition was detected.
*/

int
sem_wait(sem_t *sem)
{
  int result;

#ifdef EXTRA_DEBUG
  if (sem == NULL || *sem == NULL)
  {
    errno=EINVAL;
    return -1;
  }
#endif /* EXTRA_DEBUG */

#ifndef HAVE_CREATESEMAPHORE
  result=WaitForSingleObject(sem->event, INFINITE);
#else
  result=WaitForSingleObject(*sem, INFINITE);
#endif
  if (result == WAIT_FAILED || result == WAIT_ABANDONED_0)
    result = EINVAL;
  else if (result == WAIT_TIMEOUT)
    result = ETIMEDOUT;
  else
    result=0;
  if (result)
  {
    errno = result;
    return -1;
  }
#ifndef HAVE_CREATESEMAPHORE
  ptw32_decrease_semaphore(sem);
#endif /* HAVE_CREATESEMAPHORE */
  return 0;
}


/*
 ------------------------------------------------------
 DOCPUBLIC
      This function posts a wakeup to a semaphore. If there
      are waiting threads (or processes), one is awakened;
      otherwise, the semaphore value is incremented by one.

 PARAMETERS
      sem		Pointer to an instance of sem_t

 RESULTS
      0			Successfully posted semaphore,
      -1		Failed, error in errno

 ERRNO
      EINVAL		'sem' is not a valid semaphore,
      ENOSYS		Semaphores are not supported,

*/

int
sem_post (sem_t * sem)
{
#ifdef EXTRA_DEBUG
  if (sem == NULL || *sem == NULL)
  {
    errno=EINVAL;
    return -1;
  }
#endif /* EXTRA_DEBUG */

#ifndef HAVE_CREATESEMAPHORE
  if (! ptw32_increase_semaphore(sem, 1))
#else /* HAVE_CREATESEMAPHORE */
  if (! ReleaseSemaphore(*sem, 1, 0))
#endif /* HAVE_CREATESEMAPHORE */
  {
    errno=EINVAL;
    return -1;
  }
  return 0;
}


/*
 ------------------------------------------------------
 DOCPUBLIC
      This function posts multiple wakeups to a semaphore. If there
      are waiting threads (or processes), n <= count are awakened;
      the semaphore value is incremented by count - n.

 PARAMETERS
      sem		Pointer to an instance of sem_t
      count		Counter, must be greater than zero.

 RESULTS
      0			Successfully posted semaphore,
      -1		Failed, error in errno

 ERRNO
      EINVAL		'sem' is not a valid semaphore or count is less
			than or equal to zero.
*/

int
sem_post_multiple (sem_t * sem, unsigned int count)
{
#ifdef EXTRA_DEBUG
  if (sem == NULL || *sem == NULL || count <= 0)
  {
    errno=EINVAL;
    return -1;
  }
#endif /* EXTRA_DEBUG */
#ifndef HAVE_CREATESEMAPHORE
  if (! ptw32_increase_semaphore (sem, count))
#else /* HAVE_CREATESEMAPHORE */
  if (! ReleaseSemaphore(*sem, count, 0))
#endif /* HAVE_CREATESEMAPHORE */
  {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

int
sem_getvalue (sem_t *sem, unsigned int *sval)
{
  errno = ENOSYS;
  return -1;
}				/* sem_getvalue */

#endif /* __WIN__ */

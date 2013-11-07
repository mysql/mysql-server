/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*****************************************************************************
** The following is a simple implementation of posix conditions
*****************************************************************************/
#if defined(_WIN32)

#undef SAFE_MUTEX			/* Avoid safe_mutex redefinitions */
#include "mysys_priv.h"
#include <m_string.h>
#include <process.h>
#include <sys/timeb.h>


/*
  Windows native condition variables. We use runtime loading / function 
  pointers, because they are not available on XP 
*/

/* Prototypes and function pointers for condition variable functions */
typedef VOID (WINAPI * InitializeConditionVariableProc) 
  (PCONDITION_VARIABLE ConditionVariable);

typedef BOOL (WINAPI * SleepConditionVariableCSProc)
  (PCONDITION_VARIABLE ConditionVariable,
  PCRITICAL_SECTION CriticalSection, 
  DWORD dwMilliseconds);

typedef VOID (WINAPI * WakeAllConditionVariableProc)
 (PCONDITION_VARIABLE ConditionVariable);

typedef VOID (WINAPI * WakeConditionVariableProc)
  (PCONDITION_VARIABLE ConditionVariable);

static InitializeConditionVariableProc my_InitializeConditionVariable;
static SleepConditionVariableCSProc my_SleepConditionVariableCS;
static WakeAllConditionVariableProc my_WakeAllConditionVariable;
static WakeConditionVariableProc my_WakeConditionVariable;


/**
 Indicates if we have native condition variables,
 initialized first time pthread_cond_init is called.
*/

static BOOL have_native_conditions= FALSE;


/**
  Check if native conditions can be used, load function pointers 
*/

static void check_native_cond_availability(void)
{
  HMODULE module= GetModuleHandle("kernel32");

  my_InitializeConditionVariable= (InitializeConditionVariableProc)
    GetProcAddress(module, "InitializeConditionVariable");
  my_SleepConditionVariableCS= (SleepConditionVariableCSProc)
    GetProcAddress(module, "SleepConditionVariableCS");
  my_WakeAllConditionVariable= (WakeAllConditionVariableProc)
    GetProcAddress(module, "WakeAllConditionVariable");
  my_WakeConditionVariable= (WakeConditionVariableProc)
    GetProcAddress(module, "WakeConditionVariable");

  if (my_InitializeConditionVariable)
    have_native_conditions= TRUE;
}



/**
  Convert abstime to milliseconds
*/

static DWORD get_milliseconds(const struct timespec *abstime)
{
  long long millis; 
  union ft64 now;

  if (abstime == NULL)
   return INFINITE;

  GetSystemTimeAsFileTime(&now.ft);

  /*
    Calculate time left to abstime
    - subtract start time from current time(values are in 100ns units)
    - convert to millisec by dividing with 10000
  */
  millis= (abstime->tv.i64 - now.i64) / 10000;
  
  /* Don't allow the timeout to be negative */
  if (millis < 0)
    return 0;

  /*
    Make sure the calculated timeout does not exceed original timeout
    value which could cause "wait for ever" if system time changes
  */
  if (millis > abstime->max_timeout_msec)
    millis= abstime->max_timeout_msec;
  
  if (millis > UINT_MAX)
    millis= UINT_MAX;

  return (DWORD)millis;
}


/*
  Old (pre-vista) implementation using events
*/

static int legacy_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  cond->waiting= 0;
  InitializeCriticalSection(&cond->lock_waiting);
    
  cond->events[SIGNAL]= CreateEvent(NULL,  /* no security */
                                    FALSE, /* auto-reset event */
                                    FALSE, /* non-signaled initially */
                                    NULL); /* unnamed */

  /* Create a manual-reset event. */
  cond->events[BROADCAST]= CreateEvent(NULL,  /* no security */
                                       TRUE,  /* manual-reset */
                                       FALSE, /* non-signaled initially */
                                       NULL); /* unnamed */


  cond->broadcast_block_event= CreateEvent(NULL,  /* no security */
                                           TRUE,  /* manual-reset */
                                           TRUE,  /* signaled initially */
                                           NULL); /* unnamed */
  
  if( cond->events[SIGNAL] == NULL ||
      cond->events[BROADCAST] == NULL ||
      cond->broadcast_block_event == NULL )
    return ENOMEM;
  return 0;
}


static int legacy_cond_destroy(pthread_cond_t *cond)
{
  DeleteCriticalSection(&cond->lock_waiting);

  if (CloseHandle(cond->events[SIGNAL]) == 0 ||
      CloseHandle(cond->events[BROADCAST]) == 0 ||
      CloseHandle(cond->broadcast_block_event) == 0)
    return EINVAL;
  return 0;
}


static int legacy_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           struct timespec *abstime)
{
  int result;
  DWORD timeout; 

  timeout= get_milliseconds(abstime);
  /* 
    Block access if previous broadcast hasn't finished.
    This is just for safety and should normally not
    affect the total time spent in this function.
  */
  WaitForSingleObject(cond->broadcast_block_event, INFINITE);

  EnterCriticalSection(&cond->lock_waiting);
  cond->waiting++;
  LeaveCriticalSection(&cond->lock_waiting);

  LeaveCriticalSection(mutex);
  result= WaitForMultipleObjects(2, cond->events, FALSE, timeout);
  
  EnterCriticalSection(&cond->lock_waiting);
  cond->waiting--;
  
  if (cond->waiting == 0)
  {
    /*
      We're the last waiter to be notified or to stop waiting, so
      reset the manual event. 
    */
    /* Close broadcast gate */
    ResetEvent(cond->events[BROADCAST]);
    /* Open block gate */
    SetEvent(cond->broadcast_block_event);
  }
  LeaveCriticalSection(&cond->lock_waiting);
  
  EnterCriticalSection(mutex);

  return result == WAIT_TIMEOUT ? ETIMEDOUT : 0;
}

static int legacy_cond_signal(pthread_cond_t *cond)
{
  EnterCriticalSection(&cond->lock_waiting);
  
  if(cond->waiting > 0)
    SetEvent(cond->events[SIGNAL]);

  LeaveCriticalSection(&cond->lock_waiting);
  
  return 0;
}


static int legacy_cond_broadcast(pthread_cond_t *cond)
{
  EnterCriticalSection(&cond->lock_waiting);
  /*
     The mutex protect us from broadcasting if
     there isn't any thread waiting to open the
     block gate after this call has closed it.
   */
  if(cond->waiting > 0)
  {
    /* Close block gate */
    ResetEvent(cond->broadcast_block_event); 
    /* Open broadcast gate */
    SetEvent(cond->events[BROADCAST]);
  }

  LeaveCriticalSection(&cond->lock_waiting);  

  return 0;
}


/* 
 Posix API functions. Just choose between native and legacy implementation.
*/

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  /*
    Once initialization is used here rather than in my_init(), to 
    1) avoid  my_init() pitfalls- undefined order in which initialization should
    run
    2) be potentially useful C++ (in static constructors that run before main())
    3) just to simplify the API.
    Also, the overhead of my_pthread_once is very small.
  */
  static my_pthread_once_t once_control= MY_PTHREAD_ONCE_INIT;
  my_pthread_once(&once_control, check_native_cond_availability);

  if (have_native_conditions)
  {
    my_InitializeConditionVariable(&cond->native_cond);
    return 0;
  }
  else 
    return legacy_cond_init(cond, attr);
}


int pthread_cond_destroy(pthread_cond_t *cond)
{
  if (have_native_conditions)
    return 0; /* no destroy function */
  else
    return legacy_cond_destroy(cond);
}


int pthread_cond_broadcast(pthread_cond_t *cond)
{
  if (have_native_conditions)
  {
    my_WakeAllConditionVariable(&cond->native_cond);
    return 0;
  }
  else
    return legacy_cond_broadcast(cond);
}


int pthread_cond_signal(pthread_cond_t *cond)
{
  if (have_native_conditions)
  {
    my_WakeConditionVariable(&cond->native_cond);
    return 0;
  }
  else
    return legacy_cond_signal(cond);
}


int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
  const struct timespec *abstime)
{
  if (have_native_conditions)
  {
    DWORD timeout= get_milliseconds(abstime);
    if (!my_SleepConditionVariableCS(&cond->native_cond, mutex, timeout))
      return ETIMEDOUT;
    return 0;  
  }
  else
    return legacy_cond_timedwait(cond, mutex, abstime);
}


int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  return pthread_cond_timedwait(cond, mutex, NULL);
}


int pthread_attr_init(pthread_attr_t *connect_att)
{
  connect_att->dwStackSize	= 0;
  connect_att->dwCreatingFlag	= 0;
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *connect_att,DWORD stack)
{
  connect_att->dwStackSize=stack;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *connect_att)
{
  memset(connect_att, 0, sizeof(*connect_att));
  return 0;
}

/****************************************************************************
** Fix localtime_r() to be a bit safer
****************************************************************************/

struct tm *localtime_r(const time_t *timep,struct tm *tmp)
{
  if (*timep == (time_t) -1)			/* This will crash win32 */
  {
    memset(tmp, 0, sizeof(*tmp));
  }
  else
  {
    struct tm *res=localtime(timep);
    if (!res)                                   /* Wrong date */
    {
      memset(tmp, 0, sizeof(*tmp));             /* Keep things safe */
      return 0;
    }
    *tmp= *res;
  }
  return tmp;
}
#endif /* __WIN__ */

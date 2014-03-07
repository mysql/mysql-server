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

#include "mysys_priv.h"
#include <m_string.h>
#include <process.h>
#include <sys/timeb.h>
#include <time.h>


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
  Posix API functions using native implementation of condition variables.
*/

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  InitializeConditionVariable(cond);
  return 0;
}


int pthread_cond_destroy(pthread_cond_t *cond)
{
  return 0; /* no destroy function */
}


int pthread_cond_broadcast(pthread_cond_t *cond)
{
  WakeAllConditionVariable(cond);
  return 0;
}


int pthread_cond_signal(pthread_cond_t *cond)
{
  WakeConditionVariable(cond);
  return 0;
}


int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
  const struct timespec *abstime)
{
  DWORD timeout= get_milliseconds(abstime);
  if (!SleepConditionVariableCS(cond, mutex, timeout))
    return ETIMEDOUT;
  return 0;
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


int pthread_attr_getstacksize(pthread_attr_t *connect_att, size_t *stack)
{
  *stack= (size_t)connect_att->dwStackSize;
  return 0;
}


int pthread_attr_destroy(pthread_attr_t *connect_att)
{
  memset(connect_att, 0, sizeof(*connect_att));
  return 0;
}


int pthread_dummy(int ret)
{
  return ret;
}


/****************************************************************************
** Replacements for localtime_r and gmtime_r
****************************************************************************/

struct tm *localtime_r(const time_t *timep,struct tm *tmp)
{
  localtime_s(tmp, timep);
  return tmp;
}


struct tm *gmtime_r(const time_t *clock, struct tm *res)
{
  gmtime_s(res, clock);
  return res;
}
#endif /* _WIN32 */

/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "xplatform/my_xp_cond.h"

#ifdef _WIN32
My_xp_cond_win::My_xp_cond_win()
  :m_cond(static_cast<native_cond_t *>(malloc(sizeof(*m_cond))))
{}


My_xp_cond_win::~My_xp_cond_win()
{
  free(m_cond);
}


native_cond_t *My_xp_cond_win::get_native_cond()
{
  return m_cond;
}


/**
  Convert abstime to milliseconds on Windows.
*/
DWORD My_xp_cond_win::get_milliseconds(const struct timespec *abstime)
{
  if (abstime == NULL)
    return INFINITE;
#ifdef HAVE_STRUCT_TIMESPEC
  /*
    Convert timespec to millis and subtract current time.
    My_xp_util::getsystime() returns time in 100 ns units.
  */
  unsigned long long future = abstime->tv_sec * 1000 + abstime->tv_nsec / 1000000;
  unsigned long long now = My_xp_util::getsystime() / 10000;
  /* Don't allow the timeout to be negative. */
  if (future < now)
    return 0;
  return (DWORD)(future - now);
#else
  long long millis;
  union ft64 now;

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
#endif
}


int My_xp_cond_win::init()
{
  InitializeConditionVariable(m_cond);
  return 0;
};


int My_xp_cond_win::destroy()
{
  return 0;
}


int My_xp_cond_win::timed_wait(native_mutex_t *mutex, const struct timespec *abstime)
{
  DWORD timeout= get_milliseconds(abstime);
  if (!SleepConditionVariableCS(m_cond, mutex, timeout))
    return ETIMEDOUT;

  return 0;
}


int My_xp_cond_win::wait(native_mutex_t *mutex)
{
  return timed_wait(mutex, NULL);
}


int My_xp_cond_win::signal()
{
  WakeConditionVariable(m_cond);
  return 0;
}


int My_xp_cond_win::broadcast()
{
  WakeAllConditionVariable(m_cond);
  return 0;
}
#else
My_xp_cond_pthread::My_xp_cond_pthread()
  :m_cond(static_cast<native_cond_t *>(malloc(sizeof(*m_cond))))
{}


My_xp_cond_pthread::~My_xp_cond_pthread()
{
  free(m_cond);
}

/* purecov: begin deadcode */
native_cond_t *My_xp_cond_pthread::get_native_cond()
{
  return m_cond;
}
/* purecov: end */

int My_xp_cond_pthread::init()
{
  if (m_cond == NULL)
    return -1;

  return pthread_cond_init(m_cond, NULL);
};


int My_xp_cond_pthread::destroy()
{
  return pthread_cond_destroy(m_cond);
}


int My_xp_cond_pthread::timed_wait(native_mutex_t *mutex, const struct timespec *abstime)
{
  return pthread_cond_timedwait(m_cond, mutex, abstime);
}


int My_xp_cond_pthread::wait(native_mutex_t *mutex)
{
  return pthread_cond_wait(m_cond, mutex);
}

/* purecov: begin deadcode */
int My_xp_cond_pthread::signal()
{
  return pthread_cond_signal(m_cond);
}
/* purecov: end */

int My_xp_cond_pthread::broadcast()
{
  return pthread_cond_broadcast(m_cond);
}
#endif

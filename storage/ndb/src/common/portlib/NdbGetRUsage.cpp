/* Copyright (c) 2011, 2014 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <NdbGetRUsage.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef HAVE_MAC_OS_X_THREAD_INFO
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_port.h>

mach_port_t our_mach_task = MACH_PORT_NULL;
#endif

#ifndef _WIN32
#ifndef HAVE_MAC_OS_X_THREAD_INFO
static
Uint64
micros(struct timeval val)
{
  return
    (Uint64)val.tv_sec * (Uint64)1000000 + val.tv_usec;
}
#endif
#endif

/**
 * On Mac OS X we use the mach_task_self call to be able to
 * access thread info, this allocates memory, we only need
 * one global instance per process since a mach task is
 * representing the process, but we need to deallocate at end
 * of process, so we need an End call as well.
 */
extern "C"
void Ndb_GetRUsage_Init(void)
{
#ifdef HAVE_MAC_OS_X_THREAD_INFO
  our_mach_task = mach_task_self();
#endif
}

extern "C"
void Ndb_GetRUsage_End(void)
{
#ifdef HAVE_MAC_OS_X_THREAD_INFO
  if (our_mach_task != MACH_PORT_NULL)
  {
    mach_port_deallocate(our_mach_task, our_mach_task);
  }
#endif
}

extern "C"
int
Ndb_GetRUsage(ndb_rusage* dst)
{
  int res = -1;
#ifdef _WIN32
  FILETIME create_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  dst->ru_minflt = 0;
  dst->ru_majflt = 0;
  dst->ru_nvcsw = 0;
  dst->ru_nivcsw = 0;

  /**
   * GetThreadTimes times are updated once per timer interval, so can't
   * be used for microsecond measurements, but it is good enough for
   * keeping track of CPU usage on a second basis.
   */
  bool ret = GetThreadTimes( GetCurrentThread(),
                             &create_time,
                             &exit_time,
                             &kernel_time,
                             &user_time);
  if (ret)
  {
    /* Successful return */
    res = 0;

    Uint64 tmp = user_time.dwHighDateTime;
    tmp <<= 32;
    tmp += user_time.dwLowDateTime;
    /** 
     * Time reported in microseconds, Windows report it in
     * 100 ns intervals. So we need to divide by 10 the
     * Windows counter.
     */
    dst->ru_utime = tmp / 10;

    tmp = kernel_time.dwHighDateTime;
    tmp <<= 32;
    tmp += kernel_time.dwLowDateTime;
    dst->ru_stime = tmp / 10;
  }
  else
  {
    res = -1;
  }
#elif defined(HAVE_MAC_OS_X_THREAD_INFO)
  mach_port_t thread_port;
  kern_return_t ret_code;
  mach_msg_type_number_t basic_info_count;
  thread_basic_info_data_t basic_info;

  /**
   * mach_thread_self allocates memory so it needs to be
   * released immediately since we don't want to burden
   * the code with keeping track of this value.
   */
  thread_port = mach_thread_self();
  if (thread_port != MACH_PORT_NULL)
  {
    ret_code = thread_info(thread_port,
                           THREAD_BASIC_INFO,
                           (thread_info_t) &basic_info,
                           &basic_info_count);
  
    mach_port_deallocate(our_mach_task, thread_port);

    if (ret_code == KERN_SUCCESS)
    {
      dst->ru_minflt = 0;
      dst->ru_majflt = 0;
      dst->ru_nvcsw = 0;
      dst->ru_nivcsw = 0;

      Uint64 tmp;
      tmp = basic_info.user_time.seconds * 1000000;
      tmp += basic_info.user_time.microseconds;
      dst->ru_utime = tmp;

      tmp = basic_info.system_time.seconds * 1000000;
      tmp += basic_info.system_time.microseconds;
      dst->ru_stime = tmp;

      res = 0;
    }
    else
    {
      res = -1;
    }
  }
  else
  {
    res = -2; /* Report -2 to distinguish error cases for debugging. */
  }
#else
#ifdef HAVE_GETRUSAGE
  struct rusage tmp;
#ifdef RUSAGE_THREAD
  res = getrusage(RUSAGE_THREAD, &tmp);
#elif defined RUSAGE_LWP
  res = getrusage(RUSAGE_LWP, &tmp);
#endif

  if (res == 0)
  {
    dst->ru_utime = micros(tmp.ru_utime);
    dst->ru_stime = micros(tmp.ru_stime);
    dst->ru_minflt = tmp.ru_minflt;
    dst->ru_majflt = tmp.ru_majflt;
    dst->ru_nvcsw = tmp.ru_nvcsw;
    dst->ru_nivcsw = tmp.ru_nivcsw;
  }
#endif
#endif

  if (res != 0)
  {
    bzero(dst, sizeof(* dst));
  }
  return res;
}

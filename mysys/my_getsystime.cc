/* Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_getsystime.cc
  Get time since epoch in 100 nanoseconds units.
  Thus to get the current time we should use the system function
  with the highest possible resolution
*/

#include "my_config.h"

#include <time.h>

#include "my_inttypes.h"
#include "my_sys.h"
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if defined(_WIN32)
#include "mysys/my_static.h"
#endif

#if defined(_WIN32)
typedef VOID(WINAPI *time_fn)(_Out_ LPFILETIME);
static time_fn my_get_system_time_as_file_time= GetSystemTimeAsFileTime;

/**
  Initialise highest available time resolution API on Windows
  @return Initialization result
    @retval FALSE Success
    @retval TRUE  Error. Couldn't initialize environment
  */
bool win_init_get_system_time_as_file_time()
{
  HMODULE h= LoadLibrary("kernel32.dll");
  if (h != nullptr)
  {
    auto pfn = reinterpret_cast<time_fn>(
      GetProcAddress(h, "GetSystemTimePreciseAsFileTime"));
    if (pfn)
      my_get_system_time_as_file_time= pfn;

    return false;
  }

  DWORD error= GetLastError();
  my_message_local(ERROR_LEVEL, 
    "LoadLibrary(\"kernel32.dll\") failed: GetLastError returns %lu",
    error);

  return true;
}
#endif
/**
  Get high-resolution time.

  @remark For windows platforms we need the frequency value of
          the CPU. This is initialized in my_init.c through
          QueryPerformanceFrequency(). On the versions of Windows supported
          by MySQL 5.7 onwwards QueryPerformanceFrequency() is guaranteed to 
          return a non-zero value.

  @retval current high-resolution time.
*/

extern "C" ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
#elif defined(_WIN32)
  LARGE_INTEGER t_cnt;
  QueryPerformanceCounter(&t_cnt);
  return ((t_cnt.QuadPart / query_performance_frequency * 10000000) +
          ((t_cnt.QuadPart % query_performance_frequency) * 10000000 /
            query_performance_frequency) + query_performance_offset);
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;
#endif
}


/**
  Return current time.

  @param  flags   If MY_WME is set, write error if time call fails.

  @retval current time.
*/

time_t my_time(myf flags)
{
  time_t t;
  /*
    The following loop is here beacuse time() may fail on some systems.
    We're using a hardcoded my_message_stderr() here rather than going
    through the hook in my_message_local() because it's far too easy to
    come full circle with any logging function that writes timestamps ...
  */
  while ((t= time(0)) == (time_t) -1)
  {
    if (flags & MY_WME)
      my_message_stderr(0, "time() call failed", MYF(0));
  }
  return t;
}


/**
  Return time in microseconds.

  @remark This function is to be used to measure performance in
          micro seconds.

  @retval Number of microseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
*/
#ifdef _WIN32
#define OFFSET_TO_EPOCH 116444736000000000ULL
#endif

ulonglong my_micro_time()
{
#ifdef _WIN32
  ulonglong newtime;
  my_get_system_time_as_file_time((FILETIME*)&newtime);
  newtime -= OFFSET_TO_EPOCH;
  return (newtime / 10);
#else
  ulonglong newtime;
  struct timeval t;
  /*
  The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {
  }
  newtime = (ulonglong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}

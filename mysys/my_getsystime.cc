/* Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file mysys/my_getsystime.cc
  Get time since epoch in 100 nanoseconds units.
  Thus to get the current time we should use the system function
  with the highest possible resolution
*/

#include <time.h>

#include "my_config.h"
#include "my_inttypes.h"
#include "my_sys.h"
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if defined(_WIN32)
#include "my_static.h"
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
          micro seconds. Note that this value will be affected by NTP on Linux,
          and is subject to drift of approx 1 second per day on Windows.
          It cannot be used for timestamps that may be compared in different
          servers as Windows' QueryPerformanceCounter() generates timestamps
          that are only accurately comparable when produced by the same process.

  @retval Number of microseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
*/

ulonglong my_micro_time()
{
#ifdef _WIN32
  LARGE_INTEGER t_cnt;
  QueryPerformanceCounter(&t_cnt);
  return ((t_cnt.QuadPart / query_performance_frequency * 1000000) +
          ((t_cnt.QuadPart % query_performance_frequency) * 1000000 /
           query_performance_frequency) + query_performance_offset_micros);
#else
  return my_micro_time_ntp();
#endif
}


#ifdef _WIN32
#define OFFSET_TO_EPOCH 116444736000000000ULL
#endif

/**
  Return time in microseconds. The timestamps returned by this function are
  guaranteed to be comparable between different servers as they are synchronized
  to an external time reference (NTP).
  However, they may not be monotonic and, in Windows, their resolution may vary.

  @retval Number of microseconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
*/

ulonglong my_micro_time_ntp()
{
#ifdef _WIN32
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  newtime-= OFFSET_TO_EPOCH;
  return (newtime/10);
#else
  ulonglong newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  newtime= (ulonglong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}

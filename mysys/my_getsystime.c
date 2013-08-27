/* Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

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

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

/* 
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/

#include "mysys_priv.h"
#include "my_static.h"

/**
  Get high-resolution time.

  @remark For windows platforms we need the frequency value of
          the CPU. This is initialized in my_init.c through
          QueryPerformanceFrequency(). If the Windows platform
          doesn't support QueryPerformanceFrequency(), zero is
          returned.

  @retval current high-resolution time.
*/

ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
#elif defined(_WIN32)
  LARGE_INTEGER t_cnt;
  if (query_performance_frequency)
  {
    QueryPerformanceCounter(&t_cnt);
    return ((t_cnt.QuadPart / query_performance_frequency * 10000000) +
            ((t_cnt.QuadPart % query_performance_frequency) * 10000000 /
             query_performance_frequency) + query_performance_offset);
  }
  return 0;
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
  /* The following loop is here beacuse time() may fail on some systems */
  while ((t= time(0)) == (time_t) -1)
  {
    if (flags & MY_WME)
      fprintf(stderr, "%s: Warning: time() call failed\n", my_progname);
  }
  return t;
}


#define OFFSET_TO_EPOCH 116444736000000000ULL

/**
  Return time in microseconds.

  @remark This function is to be used to measure performance in
          micro seconds. As it's not defined whats the start time
          for the clock, this function us only useful to measure
          time between two moments.

  @retval Value in microseconds from some undefined point in time.
*/

ulonglong my_micro_time()
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


/**
  Return time in seconds and timer in microseconds (not different start!)

  @param  time_arg  Will be set to seconds since epoch.

  @remark This function is to be useful when we need both the time and
          microtime. For example in MySQL this is used to get the query
          time start of a query and to measure the time of a query (for
          the slow query log)

  @remark The time source is the same as for my_micro_time(), meaning
          that time values returned by both functions can be intermixed
          in meaningful ways (i.e. for comparison purposes).

  @retval Value in microseconds from some undefined point in time.
*/

/* Difference between GetSystemTimeAsFileTime() and now() */

ulonglong my_micro_time_and_time(time_t *time_arg)
{
#ifdef _WIN32
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  *time_arg= (time_t) ((newtime - OFFSET_TO_EPOCH) / 10000000);
  return (newtime/10);
#else
  ulonglong newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  *time_arg= t.tv_sec;
  newtime= (ulonglong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}


/**
  Returns current time.

  @param  microtime Value from very recent my_micro_time().

  @remark This function returns the current time. The microtime argument
          is only used if my_micro_time() uses a function that can safely
          be converted to the current time.

  @retval current time.
*/

time_t my_time_possible_from_micro(ulonglong microtime __attribute__((unused)))
{
#ifdef _WIN32
  time_t t;
  while ((t= time(0)) == (time_t) -1)
  {}
  return t;
#else
  return (time_t) (microtime / 1000000);
#endif
}


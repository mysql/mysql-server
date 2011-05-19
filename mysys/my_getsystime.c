/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "mysys_priv.h"
#include "my_static.h"

#ifdef __NETWARE__
#include <nks/time.h>
#elif defined(__WIN__)
#define OFFSET_TO_EPOC 116444736000000000LL
static ulonglong query_performance_frequency;
#endif

/*
  return number of nanoseconds since unspecified (but always the same)
  point in the past

  NOTE:
  Thus to get the current time we should use the system function
  with the highest possible resolution

  The value is not not anchored to any specific point in time (e.g. epoch) nor
  is it subject to resetting or drifting by way of adjtime() or settimeofday(),
  and thus it is *NOT* appropriate for getting the current timestamp. It can be
  used for calculating time intervals, though.
*/

ulonglong my_interval_timer()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec*1000000000ULL+tp.tv_nsec;
#elif defined(HAVE_GETHRTIME)
  return gethrtime();
#elif defined(__WIN__)
  LARGE_INTEGER t_cnt;
  if (query_performance_frequency)
  {
    QueryPerformanceCounter(&t_cnt);
    return (t_cnt.QuadPart / query_performance_frequency * 1000000000ULL) +
            ((t_cnt.QuadPart % query_performance_frequency) * 1000000000ULL /
             query_performance_frequency);
  }
  else
  {
    ulonglong newtime;
    GetSystemTimeAsFileTime((FILETIME*)&newtime);
    return newtime*100ULL;
  }
#elif defined(__NETWARE__)
  NXTime_t tm;
  NXGetTime(NX_SINCE_1970, NX_NSECONDS, &tm);
  return (ulonglong)tm;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000000000ULL+tv.tv_usec*1000ULL;
#endif
}

/* Return current time in HRTIME_RESOLUTION (microseconds) since epoch */

my_hrtime_t my_hrtime()
{
  my_hrtime_t hrtime;
#if defined(__WIN__)
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  newtime -= OFFSET_TO_EPOC;
  hrtime.val= newtime/10;
#elif defined(HAVE_CLOCK_GETTIME)
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec*1000000ULL+tp.tv_nsec/1000ULL;
#else
  struct timeval t;
  /* The following loop is here because gettimeofday may fail on some systems */
  while (gettimeofday(&t, NULL) != 0) {}
  hrtime.val= t.tv_sec*1000000ULL + t.tv_usec;
#endif
  return hrtime;
}

void my_time_init()
{
#ifdef __WIN__
  compile_time_assert(sizeof(LARGE_INTEGER) == sizeof(query_performance_frequency));
  if (QueryPerformanceFrequency((LARGE_INTEGER *)&query_performance_frequency) == 0)
    query_performance_frequency= 0;
#endif
}

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


/* 
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/

#include "mysys_priv.h"
#include "my_static.h"

#ifdef __NETWARE__
#include <nks/time.h>
#elif defined(__WIN__)
static ulonglong query_performance_frequency, query_performance_offset;
#elif defined(HAVE_GETHRTIME)
static ulonglong gethrtime_offset;
#endif

/*
  get time since epoc in 100 nanosec units

  NOTE:
  Thus to get the current time we should use the system function
  with the highest possible resolution

  The value is not subject to resetting or drifting by way of adjtime() or
  settimeofday(), and thus it is *NOT* appropriate for getting the current
  timestamp. It can be used for calculating time intervals, though.
  And it's good enough for UUID.
*/

ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
#elif defined(HAVE_GETHRTIME)
  return gethrtime()/100-gethrtime_offset;
#elif defined(__WIN__)
  LARGE_INTEGER t_cnt;
  if (query_performance_frequency)
  {
    QueryPerformanceCounter(&t_cnt);
    return ((t_cnt.QuadPart / query_performance_frequency * 10000000) +
            ((t_cnt.QuadPart % query_performance_frequency) * 10000000 /
             query_performance_frequency) + query_performance_offset);
  }
  return 0;
#elif defined(__NETWARE__)
  NXTime_t tm;
  NXGetTime(NX_SINCE_1970, NX_NSECONDS, &tm);
  return (ulonglong)tm/100;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;
#endif
}

/* Return current time in microseconds since epoch */

my_hrtime_t my_hrtime()
{
  my_hrtime_t hrtime;
#if defined(__WIN__)
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  hrtime.val= newtime/10;
#elif defined(HAVE_GETHRTIME)
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  hrtime.val= t.tv_sec*1000000 + t.tv_usec;
#else
  hrtime.val= my_getsystime()/10;
#endif
  return hrtime;
}

/*
  This function is basically equivalent to

     *interval= my_getsystime()/10;
     *timestamp= my_time();

   but it avoids calling OS time functions twice, if possible.
*/
void my_diff_and_hrtime(my_timediff_t *interval, my_hrtime_t *timestamp)
{
  interval->val= my_getsystime() / 10;
#if defined(__WIN__) || defined(HAVE_GETHRTIME)
  {
    my_hrtime_t t= my_hrtime();
    timestamp->val= t.val;
  }
#else
  timestamp->val= interval->val;
#endif
}

void my_time_init()
{
#ifdef __WIN__
#define OFFSET_TO_EPOC ((__int64) 134774 * 24 * 60 * 60 * 1000 * 1000 * 10)
  FILETIME ft;
  LARGE_INTEGER li, t_cnt;
  DBUG_ASSERT(sizeof(LARGE_INTEGER) == sizeof(query_performance_frequency));
  if (QueryPerformanceFrequency((LARGE_INTEGER *)&query_performance_frequency) == 0)
    query_performance_frequency= 0;
  else
  {
    GetSystemTimeAsFileTime(&ft);
    li.LowPart=  ft.dwLowDateTime;
    li.HighPart= ft.dwHighDateTime;
    query_performance_offset= li.QuadPart-OFFSET_TO_EPOC;
    QueryPerformanceCounter(&t_cnt);
    query_performance_offset-= (t_cnt.QuadPart /
                                query_performance_frequency * 10000000 +
                                t_cnt.QuadPart %
                                query_performance_frequency * 10000000 /
                                query_performance_frequency);
  }
#elif defined(HAVE_GETHRTIME)
  gethrtime_offset= gethrtime();
#endif
}

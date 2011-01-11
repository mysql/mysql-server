/* Copyright (C) 2004 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

/* 
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/

#include "mysys_priv.h"
#include "my_static.h"

ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
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
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;
#endif
}


/*
  Return current time

  SYNOPSIS
    my_time()
    flags	If MY_WME is set, write error if time call fails

*/

time_t my_time(myf flags __attribute__((unused)))
{
  time_t t;
#ifdef HAVE_GETHRTIME
  (void) my_micro_time_and_time(&t);
  return t;
#else
  /* The following loop is here beacuse time() may fail on some systems */
  while ((t= time(0)) == (time_t) -1)
  {
    if (flags & MY_WME)
      fprintf(stderr, "%s: Warning: time() call failed\n", my_progname);
  }
  return t;
#endif
}


/*
  Return time in micro seconds

  SYNOPSIS
    my_micro_time()

  NOTES
    This function is to be used to measure performance in micro seconds.
    As it's not defined whats the start time for the clock, this function
    us only useful to measure time between two moments.

    For windows platforms we need the frequency value of the CUP. This is
    initalized in my_init.c through QueryPerformanceFrequency().

    If Windows platform doesn't support QueryPerformanceFrequency() we will
    obtain the time via GetClockCount, which only supports milliseconds.

  RETURN
    Value in microseconds from some undefined point in time
*/

ulonglong my_micro_time()
{
#if defined(__WIN__)
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  return (newtime/10);
#elif defined(HAVE_GETHRTIME)
  return gethrtime()/1000;
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
#endif  /* defined(__WIN__) */
}


/*
  Return time in seconds and timer in microseconds (not different start!)

  SYNOPSIS
    my_micro_time_and_time()
    time_arg		Will be set to seconds since epoch (00:00:00 UTC,
                        January 1, 1970)

  NOTES
    This function is to be useful when we need both the time and microtime.
    For example in MySQL this is used to get the query time start of a query
    and to measure the time of a query (for the slow query log)

  IMPLEMENTATION
    Value of time is as in time() call.
    Value of microtime is same as my_micro_time(), which may be totally
    unrealated to time()

  RETURN
    Value in microseconds from some undefined point in time
*/

#define DELTA_FOR_SECONDS 500000000LL  /* Half a second */

/* Difference between GetSystemTimeAsFileTime() and now() */
#define OFFSET_TO_EPOCH 116444736000000000ULL

ulonglong my_micro_time_and_time(time_t *time_arg)
{
#if defined(__WIN__)
  ulonglong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  *time_arg= (time_t) ((newtime - OFFSET_TO_EPOCH) / 10000000);
  return (newtime/10);
#elif defined(HAVE_GETHRTIME)
  /*
    Solaris has a very slow time() call. We optimize this by using the very
    fast gethrtime() call and only calling time() every 1/2 second
  */
  static hrtime_t prev_gethrtime= 0;
  static time_t cur_time= 0;
  hrtime_t cur_gethrtime;

  mysql_mutex_lock(&THR_LOCK_time);
  cur_gethrtime= gethrtime();
  /*
    Due to bugs in the Solaris (x86) implementation of gethrtime(),
    the time returned by it might not be monotonic. Don't use the
    cached time(2) value if this is a case.
  */
  if ((prev_gethrtime > cur_gethrtime) ||
      ((cur_gethrtime - prev_gethrtime) > DELTA_FOR_SECONDS))
  {
    cur_time= time(0);
    prev_gethrtime= cur_gethrtime;
  }
  *time_arg= cur_time;
  mysql_mutex_unlock(&THR_LOCK_time);
  return cur_gethrtime/1000;
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
#endif  /* defined(__WIN__) */
}


/*
  Returns current time

  SYNOPSIS
    my_time_possible_from_micro()
    microtime		Value from very recent my_micro_time()

  NOTES
    This function returns the current time. The microtime argument is only used
    if my_micro_time() uses a function that can safely be converted to the
    current time.

  RETURN
    current time
*/

time_t my_time_possible_from_micro(ulonglong microtime __attribute__((unused)))
{
#if defined(__WIN__)
  time_t t;
  while ((t= time(0)) == (time_t) -1)
  {}
  return t;
#elif defined(HAVE_GETHRTIME)
  return my_time(0);                            /* Cached time */
#else
  return (time_t) (microtime / 1000000);
#endif  /* defined(__WIN__) */
}


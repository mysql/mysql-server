/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_utils.c
 *
 */

#include <hrt_utils.h>
#include <assert.h>

/*
 * High-Resolution Time Utilities -- Implementation
 */

int
hrt_rtnow(hrt_rtstamp* x)
{
    int ret;

#if (HRT_REALTIME_METHOD==HRT_USE_CLOCK_GETTIME)
#  if (defined(_POSIX_MONOTONIC_CLOCK) && (_POSIX_MONOTONIC_CLOCK+0 >= 0))
#    define REAL_CLOCK_TYPE CLOCK_MONOTONIC
#  else
#    define REAL_CLOCK_TYPE CLOCK_REALTIME
#  endif
    ret = clock_gettime(REAL_CLOCK_TYPE, &x->time);
#elif (HRT_REALTIME_METHOD==HRT_USE_GETTIMEOFDAY)
    ret = gettimeofday(&x->time, NULL);
#elif (HRT_REALTIME_METHOD==HRT_USE_TIMES)
    {
        clock_t r = times(NULL);
        if (r == -1) {
            ret = r;
        } else {
            ret = 0;
            x->time = r;
        }
    }
#elif (HRT_REALTIME_METHOD==HRT_USE_ANSI_TIME)
    {
        time_t r = time(NULL);
        if (r == -1) {
            ret = r;
        } else {
            ret = 0;
            x->time = r;
        }
    }
#endif

    return ret;
}

int
hrt_ctnow(hrt_ctstamp* x)
{
    int ret;

#if (HRT_CPUTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    ret = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &x->time);
#elif (HRT_CPUTIME_METHOD==HRT_USE_GETRUSAGE)
    ret = getrusage(RUSAGE_SELF, &x->time);
#elif (HRT_CPUTIME_METHOD==HRT_USE_TIMES)
    {
        clock_t r = times(&x->time);
        ret = (r == -1 ? r : 0);
    }
#elif (HRT_CPUTIME_METHOD==HRT_USE_ANSI_CLOCK)
    {
        clock_t r = clock();
        if (r == -1) {
            ret = r;
        } else {
            ret = 0;
            x->time = r;
        }
    }
#endif

    return ret;
}

int
hrt_tnow(hrt_tstamp* x)
{
    int ret;
    ret = hrt_rtnow(&x->rtstamp);
    if (ret != 0)
        return ret;
    ret = hrt_ctnow(&x->ctstamp);
    return ret;
}

#if (HRT_REALTIME_METHOD==HRT_USE_CLOCK_GETTIME      \
     || HRT_CPUTIME_METHOD==HRT_USE_CLOCK_GETTIME)
static inline double
timespec_diff(const struct timespec *y, const struct timespec *x)
{
    return (((y->tv_sec - x->tv_sec) * 1000000.0)
            + ((y->tv_nsec - x->tv_nsec) / 1000.0));
}
#endif

#if (HRT_REALTIME_METHOD==HRT_USE_GETTIMEOFDAY       \
     || HRT_CPUTIME_METHOD==HRT_USE_GETRUSAGE)
static inline double
timeval_diff(const struct timeval *y, const struct timeval *x)
{
    return (((y->tv_sec - x->tv_sec) * 1000000.0)
            + (y->tv_usec - x->tv_usec));
}
#endif

#if (HRT_REALTIME_METHOD==HRT_USE_TIMES              \
     || HRT_CPUTIME_METHOD==HRT_USE_TIMES            \
     || HRT_CPUTIME_METHOD==HRT_USE_ANSI_CLOCK)
static inline double
clock_t_diff(clock_t y, clock_t x)
{
    /* ignored: losing precision if clock_t is specified as long double */
    return (double)(y - x);
}
#endif

double
hrt_rtmicros(const hrt_rtstamp* y, const hrt_rtstamp* x)
{
#if (HRT_REALTIME_METHOD==HRT_USE_TIMES)
    static long clock_ticks = 0;
    if (clock_ticks == 0) {
        clock_ticks = sysconf(_SC_CLK_TCK);
        assert (clock_ticks > 0);
    }
#endif

#if (HRT_REALTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    return timespec_diff(&y->time, &x->time);
#elif (HRT_REALTIME_METHOD==HRT_USE_GETTIMEOFDAY)
    return timeval_diff(&y->time, &x->time);
#elif (HRT_REALTIME_METHOD==HRT_USE_TIMES)
    return ((clock_t_diff(y->time, x->time) * 1000000.0) / clock_ticks);
#elif (HRT_REALTIME_METHOD==HRT_USE_ANSI_TIME)
    return (difftime(y->time, x->time) * 1000000.0);
#endif
}

double
hrt_ctmicros(const hrt_ctstamp* y, const hrt_ctstamp* x)
{
#if (HRT_CPUTIME_METHOD==HRT_USE_TIMES)
    static long clock_ticks = 0;
    if (clock_ticks == 0) {
        clock_ticks = sysconf(_SC_CLK_TCK);
        assert (clock_ticks > 0);
    }
#endif

#if (HRT_CPUTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    return timespec_diff(&y->time, &x->time);
#elif (HRT_CPUTIME_METHOD==HRT_USE_GETRUSAGE)
    return (timeval_diff(&y->time.ru_utime, &x->time.ru_utime)
            + timeval_diff(&y->time.ru_stime, &x->time.ru_stime));
#elif (HRT_CPUTIME_METHOD==HRT_USE_TIMES)
    return (((clock_t_diff(y->time.tms_utime, x->time.tms_utime)
              + clock_t_diff(y->time.tms_stime, x->time.tms_stime))
             * 1000000.0) / clock_ticks);
#elif (HRT_CPUTIME_METHOD==HRT_USE_ANSI_CLOCK)
    return ((clock_t_diff(y->time, x->time) * 1000000.0) / CLOCKS_PER_SEC);
#endif
}

void
hrt_rtnull(hrt_rtstamp* x)
{
#if (HRT_REALTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    x->time.tv_sec = 0;
    x->time.tv_nsec = 0;
#elif (HRT_REALTIME_METHOD==HRT_USE_GETTIMEOFDAY)
    x->time.tv_sec = 0;
    x->time.tv_usec = 0;
#elif (HRT_REALTIME_METHOD==HRT_USE_TIMES)
    x->time = 0;
#elif (HRT_REALTIME_METHOD==HRT_USE_ANSI_TIME)
    x->time = 0;
#endif
}

void
hrt_ctnull(hrt_ctstamp* x)
{
#if (HRT_CPUTIME_METHOD==HRT_USE_CLOCK_GETTIME)
    x->time.tv_sec = 0;
    x->time.tv_nsec = 0;
#elif (HRT_CPUTIME_METHOD==HRT_USE_GETRUSAGE)
    x->time.ru_utime.tv_sec = 0;
    x->time.ru_utime.tv_usec = 0;
    x->time.ru_stime.tv_sec = 0;
    x->time.ru_stime.tv_usec = 0;
#elif (HRT_CPUTIME_METHOD==HRT_USE_TIMES)
    x->time.tms_utime = 0;
    x->time.tms_stime = 0;
#elif (HRT_CPUTIME_METHOD==HRT_USE_ANSI_CLOCK)
    x->time = 0;
#endif
}

void
hrt_tnull(hrt_tstamp* x)
{
    hrt_rtnull(&x->rtstamp);
    hrt_ctnull(&x->ctstamp);
}

/*
   Copyright (c) 2010, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_utils.h
 *
 */

#include "ndb_config.h"

#ifndef _utils_hrt_utils
#define _utils_hrt_utils
#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-Resolution Time Measurement Utilities
 *
 * This module provides functions for measuring the system's real time
 * and the current process's cpu time.
 *
 * In the Unix universe, various system functions exist of different
 * resolution for measuring resources, such as real and cpu time.
 * While ANSI C provides for functions time() and clock(), they are
 * of limited resolution and use.  Unix standardization efforts
 * ("POSIX" et al) over time specified a number of functions, namely:
 * clock_gettime(), gettimeofday(), getrusage(), and times().
 *
 * While these functions have different characteristics, they are not
 * equally available on all systems.  Therefore, determining the best
 * time function available for various systems becomes a task; and it
 * then introduces dependencies.
 *
 * This module provides
 * - an abstraction from the chosen function for measuring times,
 * - a default method of measuring real and cpu times by selecting
 *   the function with highest resolution available, and
 * - functions to calculate the amount of time between measurements
 *   at microsecond resolution (but not necessarily accuracy).
 *
 * The choice of the measurement function is controlled by the macros
 *   HRT_REALTIME_METHOD for real time and
 *   HRT_CPUTIME_METHOD  for cpu time.
 * If these macros haven't been defined, a default is chosen.
 *
 * Supported values for these macros are (by descending accuracy)
 * HRT_REALTIME_METHOD:
 *     HRT_USE_CLOCK_GETTIME
 *     HRT_USE_WINDOWS
 *     HRT_USE_GETTIMEOFDAY
 *     HRT_USE_TIMES
 *     HRT_USE_ANSI_TIME
 * HRT_CPUTIME_METHOD:
 *     HRT_USE_CLOCK_GETTIME
 *     HRT_USE_WINDOWS
 *     HRT_USE_GETRUSAGE
 *     HRT_USE_TIMES
 *     HRT_USE_ANSI_CLOCK
 *
 * Some information on the individual methods is given below; for
 * detailed information, however, consult the system's man pages.
 */

#include <time.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#ifdef _WIN32
#include <processthreadsapi.h>
#include <profileapi.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * Method definitions for measuring real and cpu times.
 */

/**
 * Use: clock_gettime()		[SUSv2, POSIX.1-2001, #include <time.h>
 *                               _POSIX_C_SOURCE >= 199309L]
 * Real time and, possibly, CPU time in nanosecond resolution (but not
 * necessarily accuracy).  Optional support for clock types:
 *   CLOCK_REALTIME             (systemwide realtime clock)
 *   CLOCK_MONOTONIC            (realtime clock, cannot be set)
 *   CLOCK_PROCESS_CPUTIME_ID   (per-process timer)
 * On SMP systems, may return bogus results if a process is migrated
 * to another CPU.
 * Inclusion of child's and grandchild's time reported as unreliable.
 */
#define HRT_USE_CLOCK_GETTIME 1

/**
 * Use: getrusage()	[SVr4, 4.3BSD, POSIX.1-2001, #include <sys/time.h>]
 * CPU time in microsecond resolution (but not necessarily accuracy).
 * Inclusion of child's and grandchild's time reported as unreliable.
 */
#define HRT_USE_GETRUSAGE 2

/**
 * Use: gettimeofday()	[SVr4, 4.3BSD, POSIX.1-2001, #include <sys/time.h>
 *    			 POSIX.1-2008 marks gettimeofday() as obsolete]
 * Real time in microsecond resolution (but not necessarily accuracy).
 * On some architectures, can be done completely in userspace using the
 * vdso/vsyscall method avoiding the syscall overhead.
 */
#define HRT_USE_GETTIMEOFDAY 3

/**
 * Use: times()		[SVr4, 4.3BSD, POSIX.1-2001, #include <sys/times.h>]
 * Real and CPU time in centi- or millisecond resolution, typically.
 * Reports clock ticks that have elapsed since an arbitrary point in the
 * past.  The number of clock ticks per second can be obtained using
 * sysconf(_SC_CLK_TCK); the symbol CLK_TCK (defined in <time.h>) has
 * been marked obsolescent/obsolete since POSIX.1-1996.
 * Clock tick values may overflow the possible range of type clock_t.
 * Additional limitations on some systems when times() can return -1.
 * On some systems, times() returns the cpu, not real time clock ticks.
 * Inclusion of child's and grandchild's time reported as unreliable.
 */
#define HRT_USE_TIMES 4

/**
 * Use: time()		[SVr4, 4.3BSD, POSIX.1-2001, C89, C99, SVID, AT&T
 *                       #include <time.h>]
 * Real time in seconds since the Epoch (January 1, 1970, w/o leap seconds).
 * Will roll over in 2038 as long as time_t is defined as a 32 bit int.
 */
#define HRT_USE_ANSI_TIME 5

/**
 * Use: clock()		[C89, C99, POSIX.1-2001, #include <time.h>]
 * CPU time in microseconds resolution (but not necessarily accuracy).
 * Returns the CPU time as a clock_t value or -1 if not available.
 * To get the number of seconds used, divide by CLOCKS_PER_SEC; POSIX
 * requires CLOCKS_PER_SEC=1000000 independent of the actual resolution.
 * On a 32-bit system where CLOCKS_PER_SEC equals 1000000 this function
 * will return the same value approximately every 72 minutes.
 * Some implementations also include the cpu times of any child processes
 * whose status has been collected via wait().
 */
#define HRT_USE_ANSI_CLOCK 6

/**
 * Use: QueryPerformanceCounter() for real time
 * Use: GetProcessTimes() for CPU time
 *
 * These are the preferred timers on Windows systems.
 */
#define HRT_USE_WINDOWS 7

/*
 * Default method selection of measuring real and cpu times.
 */

#ifdef HRT_REALTIME_METHOD
#if !(HRT_REALTIME_METHOD == HRT_USE_CLOCK_GETTIME || \
      HRT_REALTIME_METHOD == HRT_USE_GETTIMEOFDAY ||  \
      HRT_REALTIME_METHOD == HRT_USE_TIMES ||         \
      HRT_REALTIME_METHOD == HRT_USE_WINDOWS ||       \
      HRT_REALTIME_METHOD == HRT_USE_ANSI_TIME)
#error "unsupported HRT_REALTIME_METHOD: " HRT_REALTIME_METHOD
#endif
#elif defined(_WIN32)
#define HRT_REALTIME_METHOD HRT_USE_WINDOWS
#else
#if HAVE_CLOCK_GETTIME
#define HRT_REALTIME_METHOD HRT_USE_CLOCK_GETTIME
#else
#define HRT_REALTIME_METHOD HRT_USE_GETTIMEOFDAY
#endif
#endif

#ifdef HRT_CPUTIME_METHOD
#if !(HRT_CPUTIME_METHOD == HRT_USE_CLOCK_GETTIME || \
      HRT_CPUTIME_METHOD == HRT_USE_GETRUSAGE ||     \
      HRT_CPUTIME_METHOD == HRT_USE_TIMES ||         \
      HRT_CPUTIME_METHOD == HRT_USE_WINDOWS ||       \
      HRT_CPUTIME_METHOD == HRT_USE_ANSI_CLOCK)
#error "unsupported HRT_CPUTIME_METHOD: " HRT_CPUTIME_METHOD
#endif
#elif defined(_WIN32)
#define HRT_CPUTIME_METHOD HRT_USE_WINDOWS
#else
#if HAVE_CLOCK_GETTIME
#define HRT_CPUTIME_METHOD HRT_USE_CLOCK_GETTIME
#else
#define HRT_CPUTIME_METHOD HRT_USE_GETRUSAGE
#endif
#endif

/*
 * Timestamp types for real and cpu time.
 */

/**
 * A snapshot of the system's real time count.
 */
typedef struct {
#if (HRT_REALTIME_METHOD == HRT_USE_CLOCK_GETTIME)
  struct timespec time;
#elif (HRT_REALTIME_METHOD == HRT_USE_GETTIMEOFDAY)
  struct timeval time;
#elif (HRT_REALTIME_METHOD == HRT_USE_TIMES)
  clock_t time;
#elif (HRT_REALTIME_METHOD == HRT_USE_ANSI_TIME)
  time_t time;
#elif (HRT_REALTIME_METHOD == HRT_USE_WINDOWS)
  LARGE_INTEGER time;
#endif
} hrt_rtstamp;

/**
 * A snapshot of this process's cpu time count.
 */
typedef struct {
#if (HRT_CPUTIME_METHOD == HRT_USE_CLOCK_GETTIME)
  struct timespec time;
#elif (HRT_CPUTIME_METHOD == HRT_USE_GETRUSAGE)
  struct rusage time;
#elif (HRT_CPUTIME_METHOD == HRT_USE_TIMES)
  struct tms time;
#elif (HRT_CPUTIME_METHOD == HRT_USE_ANSI_CLOCK)
  clock_t time;
#elif (HRT_CPUTIME_METHOD == HRT_USE_WINDOWS)
  LARGE_INTEGER time;
#endif
} hrt_ctstamp;

/**
 * A snapshot of the system's real and this process's cpu time count.
 */
typedef struct hrt_tstamp {
  hrt_rtstamp rtstamp;
  hrt_ctstamp ctstamp;
} hrt_tstamp;

/*
 * Functions for time snapshots.
 */

/**
 * Stores a snapshot of the system's real time count.
 *
 * Returns zero if and only if the operation succeeded; otherwise,
 * a system- and method-specific error code is returned.
 */
extern int hrt_rtnow(hrt_rtstamp *x);

/**
 * Stores a snapshot of the process's cpu time count.
 *
 * Returns zero if and only if the operation succeeded; otherwise,
 * a system- and method-specific error code is returned.
 */
extern int hrt_ctnow(hrt_ctstamp *x);

/**
 * Stores a snapshot of the system's real and this process's cpu time count.
 *
 * Returns zero if and only if the operation succeeded; otherwise,
 * a system- and method-specific error code is returned.
 */
extern int hrt_tnow(hrt_tstamp *x);

/**
 * Returns the time amount between two real timestamps in microseconds
 * (i.e., y - x).
 */
extern double hrt_rtmicros(const hrt_rtstamp *y, const hrt_rtstamp *x);

/**
 * Returns the time amount between two cpu timestamps in microseconds
 * (i.e., y - x).
 */
extern double hrt_ctmicros(const hrt_ctstamp *y, const hrt_ctstamp *x);

/*
 * Functions for Debugging.
 */

/**
 * Nulls a snapshot of the system's real time count.
 */
extern void hrt_rtnull(hrt_rtstamp *x);

/**
 * Nulls a snapshot of the process's cpu time count.
 */
extern void hrt_ctnull(hrt_ctstamp *x);

/**
 * Nulls a snapshot of the system's real and this process's cpu time count.
 */
extern void hrt_tnull(hrt_tstamp *x);

#ifdef __cplusplus
}
#endif
#endif

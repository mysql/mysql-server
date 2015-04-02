/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <NdbTick.h>

#define NANOSEC_PER_SEC  1000000000
#define MICROSEC_PER_SEC 1000000
#define MILLISEC_PER_SEC 1000
#define MICROSEC_PER_MILLISEC 1000
#define NANOSEC_PER_MILLISEC  1000000
#define NANOSEC_PER_MICROSEC  1000

Uint64 NdbDuration::tick_frequency = 0;
static bool isMonotonic = true;
static bool isInited = false;


#ifdef HAVE_CLOCK_GETTIME
static clockid_t NdbTick_clk_id;
#endif

void NdbTick_Init()
{
  isInited = true;

#ifdef HAVE_CLOCK_GETTIME
  struct timespec tick_time;

  NdbDuration::tick_frequency = NANOSEC_PER_SEC;
  /**
   * Always try to use a MONOTONIC clock.
   * On older Solaris (< S10) CLOCK_MONOTONIC
   * is not available, CLOCK_HIGHRES is a good replacement.
   * If failed, or not available, warn about it.
   */
#if defined(CLOCK_MONOTONIC)
  NdbTick_clk_id = CLOCK_MONOTONIC;
  if (clock_gettime(NdbTick_clk_id, &tick_time) == 0)
    return;
#elif defined(CLOCK_HIGHRES)
  NdbTick_clk_id = CLOCK_HIGHRES;
  if (clock_gettime(NdbTick_clk_id, &tick_time) == 0)
    return;
#endif

  /**
   * Fall through: Fallback to use CLOCK_REALTIME.
   */
  isMonotonic = false;
  NdbTick_clk_id = CLOCK_REALTIME;
  if (clock_gettime(NdbTick_clk_id, &tick_time) == 0)
    return;

  fprintf(stderr, "Failed to use CLOCK_REALTIME for clock_gettime,"
          " errno=%u.  Aborting\n", errno);
  fflush(stderr);
  abort();

#elif defined(_WIN32)
  /**
   * QueryPerformance API is available since Windows 2000 Server.
   * This is a sensible min. requirement, so we refuse to start
   * if Performance-counters are not supported.
   */
  LARGE_INTEGER perf_frequency;
  BOOL res = QueryPerformanceFrequency(&perf_frequency);
  if (!res)
  {
    fprintf(stderr, "BEWARE: A suitable monotonic timer was not available on "
                    "this platform. ('QueryPerformanceFrequency()' failed)."
                    "This is not a suitable platform for this SW.\n");
    fflush(stderr);
    abort();
  }
  LARGE_INTEGER unused;
  res = QueryPerformanceCounter(&unused);
  if (!res)
  {
    fprintf(stderr, "BEWARE: A suitable monotonic timer was not available on "
                    "this platform. ('QueryPerformanceCounter()' failed)."
                    "This is not a suitable platform for this SW.\n");
    fflush(stderr);
    abort();
  }
  NdbDuration::tick_frequency = (Uint64)(perf_frequency.QuadPart);
  assert(NdbDuration::tick_frequency != 0);

#else
  /* Considder to deprecate platforms not supporting monotonic counters */
  //#error "A monotonic counter was not available on this platform"

  // gettimeofday() resolution is usec
  NdbDuration::tick_frequency = MICROSEC_PER_SEC;

  /* gettimeofday() is not guaranteed to be monotonic */
  isMonotonic = false;
#endif
}

bool NdbTick_IsMonotonic()
{
  assert(isInited);
  return isMonotonic;
}

const NDB_TICKS NdbTick_getCurrentTicks(void)
{
  assert(isInited);

#if defined(HAVE_CLOCK_GETTIME)
  struct timespec tick_time;
  const int res = clock_gettime(NdbTick_clk_id, &tick_time);
  /**
   * The only possible errors returned from clock_gettime()
   * are EINVAL in case of invalid clk_id arg, or EFAULT if
   * timespec arg is an invalid pointer.
   * As we test the clk_id in NdbTick_Init() at startup,
   * and is in control of the tp-arg ourself, it should be
   * safe to assume that errors wil never be returned.
   */
  assert(res==0);
  (void)res;

#ifndef NDBUG
  if (unlikely(res != 0))
  {
    fprintf(stderr, "clock_gettime(%u, tp) failed, errno=%d\n", 
            NdbTick_clk_id, errno);
#ifdef CLOCK_MONOTONIC
    fprintf(stderr, "CLOCK_MONOTONIC=%u\n", CLOCK_MONOTONIC);
#endif
#ifdef  CLOCK_HIGHRES
    fprintf(stderr, "CLOCK_HIGHRES=%u\n", CLOCK_HIGHRES);
#endif
    fprintf(stderr, "CLOCK_REALTIME=%u\n", CLOCK_REALTIME);
    fprintf(stderr, "NdbTick_clk_id = %u\n", NdbTick_clk_id);
    abort();
  }
#endif

  const Uint64 val =
    ((Uint64)tick_time.tv_sec)  * ((Uint64)NANOSEC_PER_SEC) +
    ((Uint64)tick_time.tv_nsec);

#elif defined(_WIN32)
  LARGE_INTEGER t_cnt;
  const BOOL res = QueryPerformanceCounter(&t_cnt);
  /**
   * We tested support of QPC in NdbTick_Init().
   * Thus, it should not fail later.
   */
  assert(res!=0);
  const Uint64 val = (Uint64)(t_cnt.QuadPart);
  assert(val != 0);

#else
  struct timeval tick_time;
  const int res = gettimeofday(&tick_time, 0);
  /**
   * The only possible errors returned from gettimeofday()
   * are EFAULT or EINVAL which is related to incorrect
   * arguments. As we are in control of these ourself,
   * it is safe to assume that errors are never returned.
   */
  require(res==0);
  (void)res;

  const Uint64 val =
    ((Uint64)tick_time.tv_sec) * ((Uint64)MICROSEC_PER_SEC) +
    ((Uint64)tick_time.tv_usec);
#endif

  {
    const NDB_TICKS ticks(val);
    return ticks;
  }
}


const NDB_TICKS NdbTick_AddMilliseconds(NDB_TICKS ticks, Uint64 ms)
{
  assert(isInited);
  assert(NdbTick_IsValid(ticks));
  assert(NdbDuration::tick_frequency >= MILLISEC_PER_SEC);
  ticks.t += (ms * (NdbDuration::tick_frequency/MILLISEC_PER_SEC));
  return ticks;
}

/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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


#ifdef HAVE_CLOCK_GETTIME
#ifdef CLOCK_MONOTONIC
static clockid_t NdbTick_clk_id = CLOCK_MONOTONIC;
#else
static clockid_t NdbTick_clk_id = CLOCK_REALTIME;
#endif

void NdbTick_Init(int need_monotonic)
{
  struct timespec tick_time;

  NdbDuration::tick_frequency = NANOSEC_PER_SEC;
  if (!need_monotonic)
    NdbTick_clk_id = CLOCK_REALTIME;

  if (clock_gettime(NdbTick_clk_id, &tick_time) == 0)
    return;
#ifdef CLOCK_MONOTONIC
  fprintf(stderr, "Failed to use CLOCK_MONOTONIC for clock_gettime,"
          " errno= %u\n", errno);
  fflush(stderr);
  NdbTick_clk_id = CLOCK_REALTIME;
  if (clock_gettime(NdbTick_clk_id, &tick_time) == 0)
    return;
#endif
  fprintf(stderr, "Failed to use CLOCK_REALTIME for clock_gettime,"
          " errno=%u.  Aborting\n", errno);
  fflush(stderr);
  abort();
}

const NDB_TICKS NdbTick_getCurrentTicks(void)
{
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
    fprintf(stderr, "CLOCK_REALTIME=%u\n", CLOCK_REALTIME);
    fprintf(stderr, "NdbTick_clk_id = %u\n", NdbTick_clk_id);
    abort();
  }
#endif

  {
    const NDB_TICKS ticks
    (((Uint64)tick_time.tv_sec)  * ((Uint64)NANOSEC_PER_SEC) +
      (Uint64)tick_time.tv_nsec);

    return ticks;
  }
}

#else
void NdbTick_Init(int need_monotonic)
{
#ifdef _WIN32
  // GetSystemTimeAsFileTime() return ticks as 100ns's
  NdbDuration::tick_frequency = NANOSEC_PER_SEC/100;

#else
  // gettimeofday() resolution is usec
  NdbDuration::tick_frequency = MICROSEC_PER_SEC;
#endif
}


const NDB_TICKS NdbTick_getCurrentTicks(void)
{
  Uint64 val;

#ifdef _WIN32
  ulonglong time;
  GetSystemTimeAsFileTime((FILETIME*)&time);
  val = (Uint64)time;  // 'time' is in 100ns
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

  val =  
    ((Uint64)tick_time.tv_sec) * ((Uint64)MICROSEC_PER_SEC) +
    ((Uint64)tick_time.tv_usec);
#endif

  {
    const NDB_TICKS ticks(val);
    return ticks;
  }
}
#endif


const NDB_TICKS NdbTick_AddMilliseconds(NDB_TICKS ticks, Uint64 ms)
{
  assert(NdbTick_IsValid(ticks));
  assert(NdbDuration::tick_frequency >= MILLISEC_PER_SEC);
  ticks.t += (ms * (NdbDuration::tick_frequency/MILLISEC_PER_SEC));
  return ticks;
}

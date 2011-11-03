/* Copyright (c) 2008 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_timer.cc
  Performance schema timers (implementation).
*/

#include "my_global.h"
#include "pfs_timer.h"
#include "my_rdtsc.h"

enum_timer_name wait_timer= TIMER_NAME_CYCLE;
MY_TIMER_INFO pfs_timer_info;

static ulonglong cycle_v0;
static ulonglong nanosec_v0;
static ulonglong microsec_v0;
static ulonglong millisec_v0;
static ulonglong tick_v0;

static ulong cycle_to_pico; /* 1000 at 1 GHz, 333 at 3GHz, 250 at 4GHz */
static ulong nanosec_to_pico; /* In theory, 1 000 */
static ulong microsec_to_pico; /* In theory, 1 000 000 */
static ulong millisec_to_pico; /* In theory, 1 000 000 000, fits in uint32 */
static ulonglong tick_to_pico; /* 1e10 at 100 Hz, 1.666e10 at 60 Hz */

static inline ulong round_to_ulong(double value)
{
  return (ulong) (value + 0.5);
}

static inline ulonglong round_to_ulonglong(double value)
{
  return (ulonglong) (value + 0.5);
}

void init_timers(void)
{
  double pico_frequency= 1.0e12;

  my_timer_init(&pfs_timer_info);

  cycle_v0= my_timer_cycles();
  nanosec_v0= my_timer_nanoseconds();
  microsec_v0= my_timer_microseconds();
  millisec_v0= my_timer_milliseconds();
  tick_v0= my_timer_ticks();

  if (pfs_timer_info.cycles.frequency > 0)
    cycle_to_pico= round_to_ulong(pico_frequency/
                                  (double)pfs_timer_info.cycles.frequency);
  else
    cycle_to_pico= 0;

  if (pfs_timer_info.nanoseconds.frequency > 0)
    nanosec_to_pico= round_to_ulong(pico_frequency/
                                    (double)pfs_timer_info.nanoseconds.frequency);
  else
    nanosec_to_pico= 0;

  if (pfs_timer_info.microseconds.frequency > 0)
    microsec_to_pico= round_to_ulong(pico_frequency/
                                     (double)pfs_timer_info.microseconds.frequency);
  else
    microsec_to_pico= 0;

  if (pfs_timer_info.milliseconds.frequency > 0)
    millisec_to_pico= round_to_ulong(pico_frequency/
                                     (double)pfs_timer_info.milliseconds.frequency);
  else
    millisec_to_pico= 0;

  if (pfs_timer_info.ticks.frequency > 0)
    tick_to_pico= round_to_ulonglong(pico_frequency/
                                     (double)pfs_timer_info.ticks.frequency);
  else
    tick_to_pico= 0;
}

ulonglong get_timer_value(enum_timer_name timer_name)
{
  ulonglong result;

  switch (timer_name) {
  case TIMER_NAME_CYCLE:
    result= (my_timer_cycles() - cycle_v0) * cycle_to_pico;
    break;
  case TIMER_NAME_NANOSEC:
    result= (my_timer_nanoseconds() - nanosec_v0) * nanosec_to_pico;
    break;
  case TIMER_NAME_MICROSEC:
    result= (my_timer_microseconds() - microsec_v0) * microsec_to_pico;
    break;
  case TIMER_NAME_MILLISEC:
    result= (my_timer_milliseconds() - millisec_v0) * millisec_to_pico;
    break;
  case TIMER_NAME_TICK:
    result= (my_timer_ticks() - tick_v0) * tick_to_pico;
    break;
  default:
    result= 0;
    DBUG_ASSERT(false);
  }
  return result;
}


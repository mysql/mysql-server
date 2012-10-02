/* Copyright (c) 2008, 2012, Oracle and/or its affiliates. All rights reserved.

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

enum_timer_name idle_timer= TIMER_NAME_MICROSEC;
enum_timer_name wait_timer= TIMER_NAME_CYCLE;
enum_timer_name stage_timer= TIMER_NAME_NANOSEC;
enum_timer_name statement_timer= TIMER_NAME_NANOSEC;
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

/* Indexed by enum enum_timer_name */
static struct time_normalizer to_pico_data[FIRST_TIMER_NAME + COUNT_TIMER_NAME]=
{
  { 0, 0}, /* unused */
  { 0, 0}, /* cycle */
  { 0, 0}, /* nanosec */
  { 0, 0}, /* microsec */
  { 0, 0}, /* millisec */
  { 0, 0}  /* tick */
};

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

  to_pico_data[TIMER_NAME_CYCLE].m_v0= cycle_v0;
  to_pico_data[TIMER_NAME_CYCLE].m_factor= cycle_to_pico;

  to_pico_data[TIMER_NAME_NANOSEC].m_v0= nanosec_v0;
  to_pico_data[TIMER_NAME_NANOSEC].m_factor= nanosec_to_pico;

  to_pico_data[TIMER_NAME_MICROSEC].m_v0= microsec_v0;
  to_pico_data[TIMER_NAME_MICROSEC].m_factor= microsec_to_pico;

  to_pico_data[TIMER_NAME_MILLISEC].m_v0= millisec_v0;
  to_pico_data[TIMER_NAME_MILLISEC].m_factor= millisec_to_pico;

  to_pico_data[TIMER_NAME_TICK].m_v0= tick_v0;
  to_pico_data[TIMER_NAME_TICK].m_factor= tick_to_pico;

  /*
    Depending on the platform and build options,
    some timers may not be available.
    Pick best replacements.
  */

  /*
    For STAGE and STATEMENT, a timer with a fixed frequency is better.
    The prefered timer is nanosecond, or lower resolutions.
  */

  if (nanosec_to_pico != 0)
  {
    /* Normal case. */
    stage_timer= TIMER_NAME_NANOSEC;
    statement_timer= TIMER_NAME_NANOSEC;
  }
  else if (microsec_to_pico != 0)
  {
    /* Windows. */
    stage_timer= TIMER_NAME_MICROSEC;
    statement_timer= TIMER_NAME_MICROSEC;
  }
  else if (millisec_to_pico != 0)
  {
    /* Robustness, no known cases. */
    stage_timer= TIMER_NAME_MILLISEC;
    statement_timer= TIMER_NAME_MILLISEC;
  }
  else if (tick_to_pico != 0)
  {
    /* Robustness, no known cases. */
    stage_timer= TIMER_NAME_TICK;
    statement_timer= TIMER_NAME_TICK;
  }
  else
  {
    /* Robustness, no known cases. */
    stage_timer= TIMER_NAME_CYCLE;
    statement_timer= TIMER_NAME_CYCLE;
  }

  /*
    For IDLE, a timer with a fixed frequency is critical,
    as the CPU clock may slow down a lot if the server is completely idle.
    The prefered timer is microsecond, or lower resolutions.
  */

  if (microsec_to_pico != 0)
  {
    /* Normal case. */
    idle_timer= TIMER_NAME_MICROSEC;
  }
  else if (millisec_to_pico != 0)
  {
    /* Robustness, no known cases. */
    idle_timer= TIMER_NAME_MILLISEC;
  }
  else if (tick_to_pico != 0)
  {
    /* Robustness, no known cases. */
    idle_timer= TIMER_NAME_TICK;
  }
  else
  {
    /* Robustness, no known cases. */
    idle_timer= TIMER_NAME_CYCLE;
  }
}

ulonglong get_timer_raw_value(enum_timer_name timer_name)
{
  switch (timer_name)
  {
  case TIMER_NAME_CYCLE:
    return my_timer_cycles();
  case TIMER_NAME_NANOSEC:
    return my_timer_nanoseconds();
  case TIMER_NAME_MICROSEC:
    return my_timer_microseconds();
  case TIMER_NAME_MILLISEC:
    return my_timer_milliseconds();
  case TIMER_NAME_TICK:
    return my_timer_ticks();
  default:
    DBUG_ASSERT(false);
  }
  return 0;
}

ulonglong get_timer_raw_value_and_function(enum_timer_name timer_name, timer_fct_t *fct)
{
  switch (timer_name)
  {
  case TIMER_NAME_CYCLE:
    *fct= my_timer_cycles;
    return my_timer_cycles();
  case TIMER_NAME_NANOSEC:
    *fct= my_timer_nanoseconds;
    return my_timer_nanoseconds();
  case TIMER_NAME_MICROSEC:
    *fct= my_timer_microseconds;
    return my_timer_microseconds();
  case TIMER_NAME_MILLISEC:
    *fct= my_timer_milliseconds;
    return my_timer_milliseconds();
  case TIMER_NAME_TICK:
    *fct= my_timer_ticks;
    return my_timer_ticks();
  default:
    *fct= NULL;
    DBUG_ASSERT(false);
  }
  return 0;
}

ulonglong get_timer_pico_value(enum_timer_name timer_name)
{
  ulonglong result;

  switch (timer_name)
  {
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

time_normalizer* time_normalizer::get(enum_timer_name timer_name)
{
  uint index= static_cast<uint> (timer_name);

  DBUG_ASSERT(index >= FIRST_TIMER_NAME);
  DBUG_ASSERT(index <= LAST_TIMER_NAME);

  return & to_pico_data[index];
}

void time_normalizer::to_pico(ulonglong start, ulonglong end,
                              ulonglong *pico_start, ulonglong *pico_end, ulonglong *pico_wait)
{
  if (start == 0)
  {
    *pico_start= 0;
    *pico_end= 0;
    *pico_wait= 0;
  }
  else
  {
    *pico_start= (start - m_v0) * m_factor;
    if (end == 0)
    {
      *pico_end= 0;
      *pico_wait= 0;
    }
    else
    {
      *pico_end= (end - m_v0) * m_factor;
      *pico_wait= (end - start) * m_factor;
    }
  }
}


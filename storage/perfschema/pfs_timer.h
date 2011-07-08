/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_TIMER_H
#define PFS_TIMER_H

/**
  @file storage/perfschema/pfs_timer.h
  Performance schema timers (declarations).
*/
#include <my_rdtsc.h>
#include "pfs_column_types.h"

/** Conversion factor, from micro seconds to pico seconds. */
#define MICROSEC_TO_PICOSEC 1000000

/**
  A time normalizer.
  A time normalizer consist of a transformation that
  converts raw timer values (expressed in the timer unit)
  to normalized values, expressed in picoseconds.
*/
struct time_normalizer
{
  /**
    Get a time normalizer for a given timer.
    @param timer_name the timer name
    @return the normalizer for the timer
  */
  static time_normalizer* get(enum_timer_name timer_name);

  /** Timer value at server statup. */
  ulonglong m_v0;
  /** Conversion factor from timer values to pico seconds. */
  ulonglong m_factor;

  /**
    Convert a wait from timer units to pico seconds.
    @param wait a wait, expressed in timer units
    @return the wait, expressed in pico seconds
  */
  inline ulonglong wait_to_pico(ulonglong wait)
  {
    return wait * m_factor;
  }

  /**
    Convert a time from timer units to pico seconds.
    @param t a time, expressed in timer units
    @return the time, expressed in pico seconds
  */
  inline ulonglong time_to_pico(ulonglong t)
  {
    return (t == 0 ? 0 : (t - m_v0) * m_factor);
  }

  /**
    Convert start / end times from timer units to pico seconds.
    @param start start time, expressed in timer units
    @param end end time, expressed in timer units
    @param[out] pico_start start time, expressed in pico seconds
    @param[out] pico_end end time, expressed in pico seconds
    @param[out] pico_wait wait time, expressed in pico seconds
  */
  void to_pico(ulonglong start, ulonglong end,
               ulonglong *pico_start, ulonglong *pico_end, ulonglong *pico_wait);
};

/**
  Idle timer.
  The timer used to measure all idle events.
*/
extern enum_timer_name idle_timer;
/**
  Wait timer.
  The timer used to measure all wait events.
*/
extern enum_timer_name wait_timer;
/**
  Stage timer.
  The timer used to measure all stage events.
*/
extern enum_timer_name stage_timer;
/**
  Statement timer.
  The timer used to measure all statement events.
*/
extern enum_timer_name statement_timer;
/**
  Timer information data.
  Characteristics about each suported timer.
*/
extern MY_TIMER_INFO pfs_timer_info;

/** Initialize the timer component. */
void init_timers();

extern "C"
{
  /** A timer function. */
  typedef ulonglong (*timer_fct_t)(void);
}

/**
  Get a timer value, in pico seconds.
  @param timer_name the timer to use
  @return timer value, in pico seconds
*/
ulonglong get_timer_pico_value(enum_timer_name timer_name);
/**
  Get a timer value, in timer units.
  @param timer_name the timer to use
  @return timer value, in timer units
*/
ulonglong get_timer_raw_value(enum_timer_name timer_name);
/**
  Get a timer value and function, in timer units.
  This function is useful when code needs to call the same timer several times.
  The returned timer function can be invoked directly, which avoids having to
  resolve the timer by name for each call.
  @param timer_name the timer to use
  @param[out] fct the timer function
  @return timer value, in timer units
*/
ulonglong get_timer_raw_value_and_function(enum_timer_name timer_name, timer_fct_t *fct);

#endif


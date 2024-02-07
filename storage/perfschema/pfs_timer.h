/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_TIMER_H
#define PFS_TIMER_H

/**
  @file storage/perfschema/pfs_timer.h
  Performance schema timers (declarations).
*/

#include "my_config.h"
#include "my_inttypes.h"
#include "my_rdtsc.h"

#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_histogram.h"

/** Conversion factor, from micro seconds to pico seconds. */
#define MICROSEC_TO_PICOSEC 1000000

/** Conversion factor, from nano seconds to pico seconds. */
#define NANOSEC_TO_PICOSEC 1000

#ifndef MY_CONFIG_H
/* my_config.h MUST be included before testing HAVE_XXX flags. */
#error "This build is broken"
#endif

/*
  HAVE_SYS_TIMES_H:
  - cmakedefine from config.h.cmake
  - testable after #include "my_config.h"

  HAVE_GETHRTIME:
  - cmakedefine from config.h.cmake
  - testable after #include "my_config.h"

  HAVE_CLOCK_GETTIME:
  - cmakedefine from config.h.cmake
  - testable after #include "my_config.h"

  HAVE_CLOCK_REALTIME:
  - cmakedefine from config.h.cmake
  - testable after #include "my_config.h"
  - not to be confused with CLOCK_REALTIME,
    which is set in #include <times.h>

  __APPLE__:
  __MACH__:
*/

/*
  See my_timer_nanoseconds() in mysys/my_rdtsc.cc
  This logic matches my_timer_nanoseconds(),
  to find out at compile time if a nanosecond
  timer is available or not.
*/

#if defined(HAVE_SYS_TIMES_H) && defined(HAVE_GETHRTIME)
#define HAVE_NANOSEC_TIMER
/*
  Testing HAVE_CLOCK_REALTIME instead of CLOCK_REALTIME
*/
#elif defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_REALTIME)
#define HAVE_NANOSEC_TIMER
#elif defined(__APPLE__) && defined(__MACH__)
#define HAVE_NANOSEC_TIMER
#endif

#ifdef HAVE_NANOSEC_TIMER
/* Use NANOSECOND for statements and the like. */
#define USED_TIMER_NAME TIMER_NAME_NANOSEC
#define USED_TIMER my_timer_nanoseconds
#else
/* Otherwise use MICROSECOND for statements and the like. */
#define USED_TIMER_NAME TIMER_NAME_MICROSEC
#define USED_TIMER my_timer_microseconds
#endif

ulonglong inline get_idle_timer() { return USED_TIMER(); }

ulonglong inline get_wait_timer() { return my_timer_cycles(); }

ulonglong inline get_stage_timer() { return USED_TIMER(); }

ulonglong inline get_statement_timer() { return USED_TIMER(); }

ulonglong inline get_transaction_timer() { return USED_TIMER(); }

ulonglong inline get_thread_cpu_timer() { return my_timer_thread_cpu(); }

/**
  A time normalizer.
  A time normalizer consist of a transformation that
  converts raw timer values (expressed in the timer unit)
  to normalized values, expressed in picoseconds.
*/
struct time_normalizer {
  /**
    Get a time normalizer for the statement timer.
    @return the normalizer for the timer
  */
  static time_normalizer *get_idle();
  static time_normalizer *get_wait();
  static time_normalizer *get_stage();
  static time_normalizer *get_statement();
  static time_normalizer *get_transaction();

  /** Timer value at server startup. */
  ulonglong m_v0;
  /** Conversion factor from timer values to pico seconds. */
  ulonglong m_factor;
  /** Histogram bucket timers, expressed in timer unit. */
  ulonglong m_bucket_timer[NUMBER_OF_BUCKETS + 1];

  /**
    Convert a wait from timer units to pico seconds.
    @param wait a wait, expressed in timer units
    @return the wait, expressed in pico seconds
  */
  inline ulonglong wait_to_pico(ulonglong wait) const {
    return wait * m_factor;
  }

  /**
    Convert a time from timer units to pico seconds.
    @param t a time, expressed in timer units
    @return the time, expressed in pico seconds
  */
  inline ulonglong time_to_pico(ulonglong t) const {
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
  void to_pico(ulonglong start, ulonglong end, ulonglong *pico_start,
               ulonglong *pico_end, ulonglong *pico_wait) const;

  ulong bucket_index(ulonglong t);
};

/**
  Timer information data.
  Characteristics about each supported timer.
*/
extern MY_TIMER_INFO pfs_timer_info;

/** Initialize the timer component. */
void init_timers();

#endif

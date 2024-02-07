/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_TICK_H
#define NDB_TICK_H

#include <assert.h>
#include <ndb_types.h>
#include <time.h>

void NdbTick_Init();

/**
 * NDB_TICKS is a high resolution monotonic timer representing
 * timer 'ticks' from some epoch start like boot time, 1/1 -1970 or
 * whatever.
 * Its actual resolution and duration of a 'tick' is platform
 * dependent. Make no assumption about it representing a specific time.
 * Functions are provided to compare ticks and calculate time
 * interval between ticks
 *
 * NOTE: Even if the platform specific implementation of 'ticks'
 *       should be in nanoseconds, the 64bit NDB_TICK will not wrap until
 *       ~585 years has passed. So it should be pretty safe....
 */
typedef struct NDB_TICKS {
  Uint64 t;

 public:
  NDB_TICKS() { t = 0; }

  /**
   * Provide functionality for fetch and reconstruct of tick value.
   * Useful when a 'tick' is sent as part of a signal, or when
   * the clock is used to generate a pseudo random number.
   */
  Uint64 getUint64() const { return t; }

  explicit NDB_TICKS(Uint64 val) { t = val; }

} NDB_TICKS;

/**
 * Returns whether the 'ticks' are provided by a monotonic timer.
 * Must be called after NdbTick_Init()
 */
bool NdbTick_IsMonotonic();

#ifndef _WIN32
int NdbTick_GetMonotonicClockId(clockid_t *clk);
#endif

/**
 * Returns number of 'ticks' since some
 * platforms dependent epoch start.
 */
const NDB_TICKS NdbTick_getCurrentTicks(void);

/**
 * Add specified number of milliseconds to a 'ticks' value.
 */
const NDB_TICKS NdbTick_AddMilliseconds(NDB_TICKS ticks, Uint64 ms);

static void NdbTick_Invalidate(NDB_TICKS *ticks);
static int NdbTick_IsValid(NDB_TICKS ticks);

/**
 * Compare ticks and return an integer greater than,
 * equal to, or less than 0, if the  'tick value' in t1
 * is greater than, equal to, or less than the t2 tick
 * respectively.
 */
static int NdbTick_Compare(NDB_TICKS t1, NDB_TICKS t2);

/**
 * Get time elapsed between start and end time.
 */
static const class NdbDuration NdbTick_Elapsed(NDB_TICKS start, NDB_TICKS end);

/**
 * Returns the current millisecond since some epoch start.
 *
 * Treat this function as deprecated. Elapsed time intervals
 * should be calculated by using the pattern
 * start/end = NdbTick_getCurrentTicks() and
 * elapsed = NdbTick_Elapsed...(start,end).
 *
 * All usage except in test utilities, should be considered
 * a bug.
 */
static Uint64 NdbTick_CurrentMillisecond(void);

class NdbDuration {
 public:
  Uint64 seconds() const;
  Uint64 milliSec() const;
  Uint64 microSec() const;
  Uint64 nanoSec() const;

 private:
  Uint64 t;
  static Uint64 tick_frequency;

  friend const NdbDuration NdbTick_Elapsed(NDB_TICKS start, NDB_TICKS end);

  friend Uint64 NdbTick_CurrentMillisecond(void);

  friend const NDB_TICKS NdbTick_AddMilliseconds(NDB_TICKS ticks, Uint64 ms);

  friend void NdbTick_Init();

  NdbDuration(Uint64 ticks) : t(ticks) {}
};  // class NdbDuration

/******************************************************
 * Implementation of NdbTick_foo functions.
 ******************************************************/
inline void NdbTick_Invalidate(NDB_TICKS *ticks) { ticks->t = 0; }

static inline int NdbTick_IsValid(NDB_TICKS ticks) { return (ticks.t != 0); }

static inline int NdbTick_Compare(NDB_TICKS t1, NDB_TICKS t2) {
  assert(NdbTick_IsValid(t1));
  assert(NdbTick_IsValid(t2));
  return (t1.t > t2.t) ? 1 : (t1.t < t2.t) ? -1 : 0;
}

static inline const NdbDuration NdbTick_Elapsed(NDB_TICKS start,
                                                NDB_TICKS end) {
  assert(NdbTick_IsValid(start));
  assert(NdbTick_IsValid(end));

  if (end.t >= start.t) {
    return NdbDuration(end.t - start.t);
  }

  /**
   * Clock has ticked backwards!
   * We protect against backward leaping timers by returning 0
   * if detected. This is less harmful than returning a huge
   * Uint64 which would be the result of that subtraction.
   * Even the monotonic clock is known buggy
   * on some older BIOS and virtualized platforms.
   */
  else if (NdbTick_IsMonotonic()) {
    /* Don't accept more than 10ms 'noise' if monotonic */
    assert(NdbDuration(start.t - end.t).milliSec() <= 10);
  }

  return NdbDuration(0);
}

static inline Uint64 NdbTick_CurrentMillisecond(void) {
  const Uint64 ticks = NdbTick_getCurrentTicks().t;
  if (ticks < (UINT_MAX64 / 1000))
    return ((ticks * 1000) / NdbDuration::tick_frequency);  // Best precision
  else
    return (ticks / (NdbDuration::tick_frequency / 1000));  // Avoids overflow,
}

/******************************************************
 * Implementation of NdbDuration methods.
 *
 * In order to avoid precision loss, we multiply ticks
 * by the scale factor before dividing by the frequency.
 ******************************************************/
inline Uint64 NdbDuration::seconds() const { return (t / tick_frequency); }

inline Uint64 NdbDuration::milliSec() const {
  assert(t < (UINT_MAX64 / 1000));  // Overflow?
  return ((t * 1000) / tick_frequency);
}

/**
 * To avoid overflow in intermediate results when
 * multiplying 'tick' (t) with micro- or nanoScale
 * factor below, we handle the tick conversion in a
 * 'second' and a 'fraction' (of seconds) part.
 */
inline Uint64 NdbDuration::microSec() const {
  static const Uint64 microScale = 1000 * 1000;

  const Uint64 seconds = (t / tick_frequency);
  const Uint64 fraction = (t % tick_frequency);
  const Uint64 microsec = ((fraction * microScale) / tick_frequency);

  return microsec + (seconds * microScale);
}

inline Uint64 NdbDuration::nanoSec() const {
  static const Uint64 nanoScale = 1000 * 1000 * 1000;

  const Uint64 seconds = (t / tick_frequency);
  const Uint64 fraction = (t % tick_frequency);
  const Uint64 nanosec = ((fraction * nanoScale) / tick_frequency);

  return nanosec + (seconds * nanoScale);
}

#endif

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

#ifndef TIMER_HPP
#define TIMER_HPP

/**
 *  @class Timer
 *  @brief A timer class that can't be fooled by NTP:ing the system clock to
 *         old time. The 'time' used by this class is 'ticks' since some
 *         platform specific epoch start. Typically retrieved by
 *         NdbTick_getCurrentTicks().
 */
class Timer {
 public:
  Timer() { m_delay = 10; }

  Timer(Uint64 delay_time) { m_delay = delay_time; }

  /**
   *  Set/Get alarm time of timer
   */
  inline void setDelay(Uint64 delay_time) { m_delay = delay_time; }
  inline Uint64 getDelay() const { return m_delay; }

  /**
   *  Start timer
   */
  inline void reset(NDB_TICKS now) { m_start_time = now; }

  inline bool check(NDB_TICKS now) {
    /**
     *  Protect against time moving backwards.
     *  In that case use 'backtick' as new start.
     */
    if (NdbTick_Compare(m_start_time, now) > 0) {
      m_start_time = now;
      return false;
    }

    /**
     *  Standard alarm check
     */
    if (NdbTick_Elapsed(m_start_time, now).milliSec() > m_delay) return true;

    /**
     *  Time progressing, but it is not alarm time yet
     */
    return false;
  }

 private:
  NDB_TICKS m_start_time;
  Uint64 m_delay;
};

#endif

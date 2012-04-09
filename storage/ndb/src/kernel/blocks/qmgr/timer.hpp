/*
   Copyright (C) 2003, 2005, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

/**
 *  @class Timer
 *  @brief A timer class that can't be fooled by NTP:ing the system clock to old time
 */
class Timer {
public:
  Timer() {
    m_delay = 10;
  };

  Timer(NDB_TICKS delay_time) {
    m_delay = delay_time;
  }

  /**
   *  Set/Get alarm time of timer
   */
  inline void       setDelay(NDB_TICKS delay_time) { m_delay = delay_time;  }
  inline NDB_TICKS  getDelay() const               { return m_delay; }

  /**
   *  Start timer
   */
  inline void reset(NDB_TICKS now) { 
    m_current_time = now;
    m_alarm_time = m_current_time + m_delay;
  }
  
  inline bool check(NDB_TICKS now) {
    /**
     *  Standard alarm check
     */
    if (now > m_alarm_time) return true;

    /**
     *  Time progressing, but it is not alarm time yet
     */
    if (now >= m_current_time) return false;

    /**
     *  Time has moved backwards
     */
    reset(now);
    return false;
  }

private:
  NDB_TICKS m_current_time;
  NDB_TICKS m_alarm_time;
  NDB_TICKS m_delay;
};

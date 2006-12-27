/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  inline NDB_TICKS  getDelay()                     { return m_delay; }

  /**
   *  Start timer
   */
  inline void reset() { 
    m_current_time = NdbTick_CurrentMillisecond();
    m_alarm_time = m_current_time + m_delay;
  }
  
  /**
   *  Check for alarm
   */ 
  inline bool check() { return check(NdbTick_CurrentMillisecond()); }

  inline bool check(NDB_TICKS check_time) {
    /**
     *  Standard alarm check
     */
    if (check_time > m_alarm_time) return true;

    /**
     *  Time progressing, but it is not alarm time yet
     */
    if (check_time >= m_current_time) return false;

    /**
     *  Time has moved backwards
     */
    reset();
    return false;
  }    

private:
  NDB_TICKS m_current_time;
  NDB_TICKS m_alarm_time;
  NDB_TICKS m_delay;
};

/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_timer.h"

#include <algorithm>  // std::max
#include <chrono>
#include <sstream>
#include <thread>

#include "health_monitor_log.h"  // on_of()

namespace hm {

/**
  Mode Timer - Start
  - Reset state
  - Start countdown
*/
void Mode_Timer::start() {
  m_timer = 0;
  m_started = true;
  m_running = true;
  m_expired = false;
  m_canceled = false;
}

/**
  Mode Timer - Cancel
  - Reset everything
  - Set canceled flag
*/
void Mode_Timer::cancel() {
  reset();
  m_canceled = true;  // used for logging
}

/**
  Mode Timer - Reset
  - Reset timer state
*/
void Mode_Timer::reset() {
  m_timer = 0;
  m_started = false;
  m_running = false;
  m_expired = false;
  m_canceled = false;
}

/**
  Mode Timer - Set timeout
  - Set the timeout value then adjust if necessary
*/
void Mode_Timer::set_timeout(int timeout) {
  m_timeout = timeout;
  adjust_timeout();
}

/**
  Mode Timer - Adjust timeout
  - Round up timeout to the next interval
*/
void Mode_Timer::adjust_timeout() {
  const int rem = m_timeout % m_interval;
  if (rem > 0) m_timeout = m_timeout - rem + m_interval;
}

/**
  Mode Timer - Tick
  - Advance the countdown timer
  - If expired, must be stopped by the caller
*/
void Mode_Timer::tick() {
  m_timer += m_interval;
  if (m_timer >= m_timeout) {
    m_expired = true;
  }
}

/**
  Mode Timer - Report status
  - Return true if it's time to issue a status message
*/
bool Mode_Timer::report_status() const {
  return (m_timer % m_report_interval == 0);
}

/**
  Mode Timer - Format status
  - Return formatted string with timer settings
*/
std::string Mode_Timer::format_status() const {
  std::stringstream profile;
  profile << "timer="
          << "'" << m_name << "'";
  profile << ", interval=" << m_interval << "s";
  profile << ", report interval=" << m_report_interval << "s";
  profile << ", timeout=" << m_timeout << "s";
  profile << ", running=" << on_off(m_running);
  profile << ", started=" << on_off(m_started);
  profile << ", expired=" << on_off(m_expired);
  profile << ", canceled=" << on_off(m_canceled);
  profile << ", remaining=" << remaining() << "s";
  return profile.str();
}

/**
  Mode Timer - Remaining time
  - Return seconds remaining or 0.
  - The timer can continue running beyond the timeout.
*/
int Mode_Timer::remaining() const {
  return (m_timeout >= m_timer) ? (m_timeout - m_timer) : 0;
}

/**
  Retry Timer - Wait
  - Wait with exponential backoff 1(0), 2, 4, 8, ...
  @return true to try again, false if max attempts reached
*/
bool Retry_Timer::wait() {
  // Total backoff time is given by
  //     backoff = 1 * (1 - 2^^max_retries) / (1 - 2)
  // Total retry time is given by
  //     backoff + lock_wait_timeout * max_retries
  // If max_retries = 5 and lock_wait_timeout = 5 then total = 56 seconds.
  // The first attempt is actually 0 seconds, so total is 55 seconds.
  if (++m_retry_count == 1) return true;  // no wait
  if (m_retry_count > m_max_retries) return false;
  m_wait_ms = 1 << (m_retry_count - 1);
  m_wait_ms *= 1000;
  std::this_thread::sleep_for(std::chrono::milliseconds(m_wait_ms));
  return true;
}

void Retry_Timer::reset() {
  m_retry_count = 0;
  m_wait_ms = 0;
  m_next_wait_ms = 0;
}

/**
  Retry Timer - Print status
  - Return formatted string with timer settings
*/
std::string Retry_Timer::format_status() const {
  std::stringstream profile;
  profile << "retries=" << m_max_retries;
  profile << ", count=" << m_retry_count;
  profile << ", wait=" << m_wait_ms << "ms";
  profile << ", next wait=" << m_next_wait_ms << "ms";
  profile << ", remaining=" << remaining();
  return profile.str();
}

/**
  Cycle Timer - Advance the timer using the default interval
*/
void Cycle_Timer::tick() {
  m_timer += m_interval;
  if (m_timer >= m_timeout) {
    m_expired = true;
  }
}

/**
  Cycle Timer - Advance the timer using the given interval
*/
void Cycle_Timer::tick(int interval) {
  m_timer += interval;
  if (m_timer >= m_timeout) {
    m_expired = true;
  }
}

/**
  Cycle Timer - Reset
*/
void Cycle_Timer::reset() {
  m_timer = 0;
  m_expired = false;
}

/**
  Cycle Timer - Set interval
*/
void Cycle_Timer::set_interval(int interval) {
  m_interval = interval;
  reset();
}

/**
  Cycle Timer - Set timeout
*/
void Cycle_Timer::set_timeout(int timeout) {
  m_timeout = timeout;
  adjust_timeout();
  reset();
}

/**
  Cycle Timer - Round up timeout to the next interval
*/
void Cycle_Timer::adjust_timeout() {
  const int rem = m_timeout % m_interval;
  if (rem > 0) m_timeout = m_timeout - rem + m_interval;
}

/**
  Cycle Timer - Return seconds remaining or 0
*/
int Cycle_Timer::remaining() const {
  return (m_timeout >= m_timer) ? (m_timeout - m_timer) : 0;
}

/**
  Cycle Timer - Return formatted string with timer settings
*/
std::string Cycle_Timer::format_status() const {
  std::stringstream profile;
  profile << "timer="
          << "'" << m_name << "'";
  profile << ", interval=" << m_interval << "s";
  profile << ", timeout=" << m_timeout << "s";
  profile << ", expired=" << on_off(m_expired);
  profile << ", remaining=" << remaining() << "s";
  return profile.str();
}

}  // namespace hm

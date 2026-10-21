/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <string>

namespace hm {

// clang-format off

static const int k_default_timer_interval{10};
static const int k_default_timer_timeout{60};
static const int k_default_report_interval{30};

/**
  Server mode timer used for fallback mode, recovery mode, etc.
  - Time-based interval, driven by the collection cycle, i.e. one tick per cycle
  - Timer interval (tick) is the base monitoring interval of the collector (10s)
  - Reporting interval is the interval of issuing status messages, and should
    be a multiple of the timer interval

 NOTE: This timer assumes a constant Collector base interval.
       If the base interval is dynamic, then the Mode_Timer must be modified
       to use the current Collector base interval for the tick interval.
*/
class Mode_Timer {
 public:
  Mode_Timer() = default;

  Mode_Timer(std::string &name, const int interval)
      : m_name{name},
        m_interval{interval} {}

  void start();
  void stop() { m_running = false; }
  void cancel();
  void reset();
  void set_timeout(int timeout);
  void adjust_timeout();
  void tick();
  bool report_status() const;
  std::string format_status() const;

  bool canceled() const { return m_canceled; }
  void set_canceled(bool canceled) { m_canceled = canceled; }
  bool expired() const { return m_expired; }
  int interval() const { return m_interval; }
  void set_interval(int interval) { m_interval = interval; }
  int report_interval() const { return m_report_interval; }
  void set_report_interval(int interval) { m_report_interval = interval; }
  int remaining() const;
  bool running() const { return m_running; }
  bool started() const { return m_started; }
  int timeout() const { return m_timeout; }
  std::string name() const { return m_name; }
  void set_name(const std::string name) { m_name = name; }
  std::string running_msg() const { return m_running_msg; }
  void set_running_msg(const std::string &msg) { m_running_msg = msg; }

 private:
  std::string m_name;
  int m_timer{0};             // time counter
  int m_interval{k_default_timer_interval}; // timer interval, usually the collector base interval
  int m_report_interval{k_default_report_interval}; // interval for reporting status, multiple of interval
  int m_timeout{0};          // length of the timer in seconds
  bool m_running{false};     // timer is started, advancing and not expired
  bool m_started{false};     // timer initialized, may or may not be running or expired
  bool m_expired{false};     // timer counter > timeout
  bool m_canceled{false};    // state and counter reset, timeout and interval preserved
  std::string m_running_msg; // timer-specific running message
};

/**
  Retry timer with backoff.
  - Used for retrying operations e.g. setting SRO, terminating connections, etc.
  - Driven by a sleep wait interval until max retries reached
  - Unrelated to the Collector base interval
*/
class Retry_Timer {
 public:
  explicit Retry_Timer(int max_retries)
      : m_max_retries{max_retries} {}
  bool wait();
  void reset();
  int max_retries() const { return m_max_retries; }
  void set_max_retries(int max_retries) { m_max_retries = max_retries; }
  int retry_count() const { return m_retry_count; }
  int remaining() const { return m_max_retries - m_retry_count; }
  int next_wait() const { return m_next_wait_ms; }
  std::string format_status() const;

 private:
  int m_max_retries{0};
  int m_retry_count{0};
  int m_wait_ms{0};
  int m_next_wait_ms{0};
};

/**
  Cycle-driven timer.
  - Interval = time between ticks
  - Timeout = total time before expiration
*/
class Cycle_Timer {
 public:
  Cycle_Timer() = default;

  Cycle_Timer(int interval, int timeout)
      : m_interval{interval},
        m_timeout{timeout} {}

  void tick();
  void tick(int interval);
  void reset();
  void set_name(const std::string &name) { m_name = name; }
  void set_interval(int interval);
  void set_timeout(int timeout);
  void adjust_timeout();
  int interval() const { return m_interval; }
  int timeout() const { return m_timeout; }
  bool expired() const { return m_expired; }
  int remaining() const;
  std::string format_status() const;

 private:
  std::string m_name{"Cycle_Timer"};
  int m_timer{0};                           // time counter
  int m_interval{k_default_timer_interval}; // timer interval, usually the collector base interval
  int m_timeout{k_default_timer_timeout};   // length of the timer in seconds
  bool m_expired{false};                    // timer counter > timeout
};

// clang-format on

}  // namespace hm

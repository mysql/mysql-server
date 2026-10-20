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

class Disk_Collector;

namespace ds {

class Storage_State {
 public:
  enum State {
    UNDEF,
    START,
    STOP,
    NORMAL,
    WARNING,
    LOW_LIMIT,
    CRITICAL,
    RECOVERY_1,
    RECOVERY_2,
    ERROR
  };

  Storage_State() = default;
  Storage_State(const char *state_name, State state)
      : m_name(state_name), m_state(state) {}

  virtual ~Storage_State() = default;
  virtual void enter() {}
  virtual void check_levels() {}
  virtual void exit() {}
  static void set_collector(Disk_Collector *collector) { dc = collector; }
  static void get_params();
  static void get_levels();
  enum State state() const { return m_state; }

  static bool normal_level();
  static bool warning_level();
  static bool low_level();
  static bool critical_level();
  static bool disk_error();

  static void set_normal_state();
  static void set_warning_state();
  static void set_low_limit_state();
  static void set_critical_state();
  static void set_error_state();

  static Disk_Collector *dc;
  static size_t available_mb;
  // Storage thresholds, updated when the monitored filesystem or settings
  // change
  static size_t reserve_mb;
  static size_t critical_level_mb;
  static size_t warn_level_1_mb;
  static size_t warn_level_2_mb;
  static size_t warn_level_3_mb;
  static size_t max_warn_level_mb;
  static size_t recovery_level_mb;

 private:
  const std::string m_name;
  State m_state{UNDEF};
};

/**
  Start State

  The Disk Collector process thread is starting but has not yet checked the
  storage status.
*/
class Start_State : public Storage_State {
 public:
  Start_State() : Storage_State("Start", State::START) {}
  Start_State(const Start_State &) = delete;
  Start_State &operator=(const Start_State &) = delete;

  void check_levels() override;
};

/**
  Stop State

  The Disk Collector process thread is stopping. Clean up any remaining tasks.
*/
class Stop_State : public Storage_State {
 public:
  Stop_State() : Storage_State("Stop", State::STOP) {}
  void check_levels() override;
};

/**
  Normal State

  Available space is above low space limit.
*/
class Normal_State : public Storage_State {
 public:
  Normal_State() : Storage_State("Normal", State::NORMAL) {}
  void check_levels() override;
};

/**
  Warning State

  Available space is below one or more warning levels, but above
  the low space limit.
*/
class Warning_State : public Storage_State {
 public:
  Warning_State() : Storage_State("Warning", State::WARNING) {}
  void enter() override;
  static bool check_warning_level();
  void check_levels() override;
};

/**
  Low Limit State

  Available space is below the low space limit but still above
  the critical space limit. Entering the low limit state starts
  the fallback timer.
*/
class Low_Limit_State : public Storage_State {
 public:
  Low_Limit_State() : Storage_State("Low Limit", State::LOW_LIMIT) {}
  void enter() override;
  void check_levels() override;
  void exit() override;
};

/**
  Critical State

  Available space is below the absolute lowest storage limit.
  Fallback mode is triggered immediately.
*/
class Critical_State : public Storage_State {
 public:
  Critical_State() : Storage_State("Critical", State::CRITICAL) {}
  void enter() override;
  void exit() override;
  void check_levels() override;
};

/**
  Error State

  The Disk Collector is running but there is an error that prevents it from
  collecting storage data.
*/
class Error_State : public Storage_State {
 public:
  Error_State() : Storage_State("Error", State::ERROR) {}
  void enter() override;
  void check_levels() override;
};

}  // namespace ds
}  // namespace hm

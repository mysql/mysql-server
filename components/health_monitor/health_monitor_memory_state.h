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

#include <cstdint>
#include <string>

namespace hm {

class Memory_Collector;

namespace ms {

class Memory_State {
 public:
  enum State { UNDEF, START, STOP, NORMAL, WARNING, ERROR };

  Memory_State() = default;
  Memory_State(const char *state_name, State state)
      : m_name(state_name), m_state(state) {}

  virtual ~Memory_State() = default;
  virtual void enter() {}
  virtual void check_levels() {}
  virtual void exit() {}
  static void set_collector(Memory_Collector *collector) { mc = collector; }
  static void get_params();
  static void get_levels();
  enum State state() const { return m_state; }

  static Memory_Collector *mc;
  static uint64_t available_mb;
  /* Memory thresholds, set for each settings change */
  static uint64_t low_space_limit_mb;
  static uint64_t critical_level_mb;
  static uint64_t warn_level_1_mb;
  static uint64_t warn_level_2_mb;
  static uint64_t warn_level_3_mb;
  static uint64_t max_warn_level_mb;

 private:
  const std::string m_name;
  State m_state{UNDEF};
};

/**
  Start State

  The Memory Collector process thread is starting but has not yet checked the
  memory levels.
*/
class Start_State : public Memory_State {
 public:
  Start_State() : Memory_State("Start", State::START) {}
  Start_State(const Start_State &) = delete;
  Start_State &operator=(const Start_State &) = delete;

  void check_levels() override;
};

/**
  Stop State

  The Memory Collector process thread is stopping. Clean up any remaining tasks.
*/
class Stop_State : public Memory_State {
 public:
  Stop_State() : Memory_State("Stop", State::STOP) {}
  Stop_State(const Stop_State &) = delete;
  Stop_State &operator=(const Stop_State &) = delete;

  void check_levels() override;
};

/**
  Normal State

  Available memory is above warning levels.
*/
class Normal_State : public Memory_State {
 public:
  Normal_State() : Memory_State("Normal", State::NORMAL) {}
  void check_levels() override;
};

/**
  Warning State

  Available memory is below one or more warning levels.
*/
class Warning_State : public Memory_State {
 public:
  Warning_State() : Memory_State("Warning", State::WARNING) {}
  void enter() override;
  static bool check_warning_level();
  void check_levels() override;
};

/**
  Error State

  The Memory Collector is running but there is an error that prevents it from
  collecting memory data.
*/
class Error_State : public Memory_State {
 public:
  Error_State() : Memory_State("Error", State::ERROR) {}
  void enter() override;
  void check_levels() override;
};

}  // namespace ms
}  // namespace hm

/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

class System_Collector;

namespace ss {

class System_State {
 public:
  enum State { UNDEF, START, STOP, NORMAL, WARNING, ERROR };

  System_State() = default;
  System_State(const char *state_name, State state)
      : m_name(state_name), m_state(state) {}

  virtual ~System_State() = default;
  virtual void enter() {}
  virtual void check_levels() {}
  virtual void exit() {}
  static void set_collector(System_Collector *collector) { sc = collector; }
  enum State state() const { return m_state; }

  static System_Collector *sc;

 private:
  const std::string m_name;
  State m_state{UNDEF};
};

/**
  Start State

  The System Collector process thread is starting.
*/
class Start_State : public System_State {
 public:
  Start_State() : System_State("Start", State::START) {}
  void check_levels() override;
};

/**
  Stop State

  The System Collector process thread is stopping. Clean up any remaining tasks.
*/
class Stop_State : public System_State {
 public:
  Stop_State() : System_State("Stop", State::STOP) {}
  void check_levels() override;
};

/**
  Normal State

  Typical operating run state.
*/
class Normal_State : public System_State {
 public:
  Normal_State() : System_State("Normal", State::NORMAL) {}
  void check_levels() override;
};

/**
  Warning State

  The System Collector is running but there was an error or condition that
  might need attention.
*/
class Warning_State : public System_State {
 public:
  Warning_State() : System_State("Warning", State::WARNING) {}
  void enter() override;  // cppcheck-suppress uselessOverride
  void check_levels() override;
};

/**
  Error State

  The System Collector is running but there is an error that prevents it from
  collecting system stats.
*/
class Error_State : public System_State {
 public:
  Error_State() : System_State("Error", State::ERROR) {}
  void enter() override;
  void check_levels() override;
};

}  // namespace ss
}  // namespace hm

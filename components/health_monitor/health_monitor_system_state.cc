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

#include "health_monitor_log.h"  // error messages
#include "health_monitor_system.h"

namespace hm {
using namespace ss;

/* Instantiate System_State static member variables */
System_Collector *System_State::sc{nullptr};

/**
  Start State - Check levels
*/
void Start_State::check_levels() {
  if (sc->m_sysinfo_error) {
    sc->set_error_state();
  } else if (sc->m_sysinfo_warning) {
    sc->set_warning_state();
  } else {
    sc->set_normal_state();
  }
}

/**
  Stop State - Check levels
*/
void Stop_State::check_levels() {
  if (sc->m_sysinfo_error) {
    sc->set_error_state();
  } else if (sc->m_sysinfo_warning) {
    sc->set_warning_state();
  } else {
    sc->log_status();
  }
}

/**
  Normal State - Check levels
*/
void Normal_State::check_levels() {
  if (sc->m_sysinfo_error) {
    sc->set_error_state();
  } else if (sc->m_sysinfo_warning) {
    sc->set_warning_state();
  } else {
    sc->log_status();
  }
}

/**
  Warning State - Enter
  Issue a warning immediately upon entering the warning state.
*/
void Warning_State::enter() {
  // sc->log_warning();
}

/**
  Warning State - Check levels
*/
void Warning_State::check_levels() {
  if (sc->m_sysinfo_error) {
    sc->set_error_state();
  } else if (sc->m_sysinfo_warning) {
    // Still in warning state, so nothing to do except issue status
    sc->log_status();
  } else {
    sc->set_normal_state();
  }
}

/**
  Error State - Enter
*/
void Error_State::enter() { sc->log_warning_error_condition(); }

/**
  Error State - Check levels
*/
void Error_State::check_levels() {
  if (sc->m_sysinfo_error) {
    // Still in error state, so nothing to do except issue status
    sc->log_status();
  } else if (sc->m_sysinfo_warning) {
    sc->set_warning_state();
  } else {
    sc->set_normal_state();
  }
}

}  // namespace hm
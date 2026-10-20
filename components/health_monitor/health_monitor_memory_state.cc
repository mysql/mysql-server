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

#include "health_monitor_log.h"  // error messages
#include "health_monitor_memory.h"

namespace hm {
using namespace ms;

/* Instantiate Memory_State static member variables */
Memory_Collector *Memory_State::mc{nullptr};
uint64_t Memory_State::available_mb{0};
uint64_t Memory_State::warn_level_1_mb{0};
uint64_t Memory_State::warn_level_2_mb{0};
uint64_t Memory_State::warn_level_3_mb{0};
uint64_t Memory_State::max_warn_level_mb{0};

/**
  Memory State - Get params
  Get the memory threshold params from the memory collector.
*/
void Memory_State::get_params() {
  available_mb = mc->m_avail_mb;
  warn_level_1_mb = mc->m_warn_level_1_mb;
  warn_level_2_mb = mc->m_warn_level_2_mb;
  warn_level_3_mb = mc->m_warn_level_3_mb;
  max_warn_level_mb = mc->m_max_warn_level_mb;
}

/**
  Memory State - Get levels
  Get the memory levels from the memory collector.
*/
void Memory_State::get_levels() { available_mb = mc->m_avail_mb; }

/**
  Start State - Check levels
*/
void Start_State::check_levels() {
  if (mc->m_sysinfo_error) {
    mc->set_error_state();
  } else if (available_mb < max_warn_level_mb) {
    mc->set_warning_state();
  } else {
    mc->log_status();
  }
}

/**
  Stop State - Check levels
*/
void Stop_State::check_levels() {
  if (mc->m_sysinfo_error) {
    mc->set_error_state();
  } else if (available_mb < max_warn_level_mb) {
    mc->set_warning_state();
  } else {
    mc->log_status();
  }
}

/**
  Normal State - Check levels
*/
void Normal_State::check_levels() {
  if (mc->m_sysinfo_error) {
    mc->set_error_state();
  } else if (available_mb < max_warn_level_mb) {
    mc->set_warning_state();
  } else {
    mc->log_status();
  }
}

/**
  Warning State - Enter
  Issue a warning immediately upon entering the warning state.
*/
void Warning_State::enter() {
  mc->m_last_warn_level_mb = Collector::no_warn_level;
  check_warning_level();
}

/**
  Warning State - Check warning levels
    - warning levels can be in any order
    - 0 is a valid warning level
*/
bool Warning_State::check_warning_level() {
  uint64_t level_mb = Collector::no_warn_level;
  int errmsg{0};

  if (available_mb < warn_level_1_mb) {
    if (warn_level_1_mb < level_mb) {
      level_mb = warn_level_1_mb;
      errmsg = ER_HEALTH_WARNING_MEMORY_USAGE_LEVEL_1;
    }
  }
  if (available_mb < warn_level_2_mb) {
    if (warn_level_2_mb < level_mb) {
      level_mb = warn_level_2_mb;
      errmsg = ER_HEALTH_WARNING_MEMORY_USAGE_LEVEL_2;
    }
  }
  if (available_mb < warn_level_3_mb) {
    if (warn_level_3_mb < level_mb) {
      level_mb = warn_level_3_mb;
      errmsg = ER_HEALTH_WARNING_MEMORY_USAGE_LEVEL_3;
    }
  }

  const bool warning_state = (level_mb != Collector::no_warn_level);

  if (level_mb == mc->m_last_warn_level_mb) {
    /* No change from the last level, warning level or not */
    mc->log_status();
  } else {
    /* Either at a new warning level or no longer within warning range */
    mc->m_last_warn_level_mb = level_mb;
    if (warning_state) {
      mc->log_warning_level(errmsg);
    }
  }
  return warning_state;
}

/**
  Warning State - Check levels
  Issue warnings when the warning level changes.
*/
void Warning_State::check_levels() {
  if (mc->m_sysinfo_error) {
    mc->set_error_state();
  } else if (!check_warning_level()) {
    /* Warning state cleared, back to normal */
    mc->set_normal_state();
    mc->log_resource_levels();
  }
}

/**
  Error State - Enter
*/
void Error_State::enter() { mc->log_warning_error_condition(); }

/**
  Error State - Check levels
*/
void Error_State::check_levels() {
  if (mc->m_sysinfo_error) {
    /* Still in error state, so no levels to check */
    mc->log_status();
  } else if (available_mb < max_warn_level_mb) {
    mc->set_warning_state();
  } else {
    mc->set_normal_state();
  }
}

}  // namespace hm
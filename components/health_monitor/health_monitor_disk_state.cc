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

#include "health_monitor_disk.h"
#include "health_monitor_log.h"  // error messages

namespace hm {
using namespace ds;

/* Instantiate Storage_State static member variables */
Disk_Collector *Storage_State::dc{nullptr};
size_t Storage_State::available_mb{0};
size_t Storage_State::reserve_mb{0};
size_t Storage_State::critical_level_mb{0};
size_t Storage_State::warn_level_1_mb{0};
size_t Storage_State::warn_level_2_mb{0};
size_t Storage_State::warn_level_3_mb{0};
size_t Storage_State::max_warn_level_mb{0};
size_t Storage_State::recovery_level_mb{0};

/**
  Storage State - Get params
  Get the storage threshold params from the disk collector.
  These are set once a valid mount point is established.
*/
void Storage_State::get_params() {
  available_mb = dc->m_avail_mb;
  reserve_mb = dc->m_reserve_mb;
  critical_level_mb = dc->m_critical_level_mb;
  warn_level_1_mb = dc->m_warn_level_1_mb;
  warn_level_2_mb = dc->m_warn_level_2_mb;
  warn_level_3_mb = dc->m_warn_level_3_mb;
  max_warn_level_mb = dc->m_max_warn_level_mb;
  recovery_level_mb = dc->recovery().level_mb();
}

/**
  Storage State - Get levels
  Get the storage levels from the disk collector.
*/
void Storage_State::get_levels() { available_mb = dc->m_avail_mb; }

/**
  Storage State - Storage levels

  Storage levels are intended to be logically sequential such that
  normal > warning 1 > warning 2 > warning 3 > low > critical, however
  they can be defined in any order. To ensure logical consistency,
  each level has a primary definition with zero or more qualifiers.

  For example, low level *always* means that available space is below the
  reserve level, but must also be above the critical level, even if the critical
  level is set to be higher than the reserve, as some test cases do.
*/
bool Storage_State::normal_level() {
  // Normal level is above all warning and low space levels
  return (available_mb > max_warn_level_mb) && (available_mb > reserve_mb) &&
         (available_mb > critical_level_mb);
}

bool Storage_State::warning_level() {
  // Warning level is below the maximum warning level (usually warning level 1),
  // but above the low space and critical levels
  return (available_mb < max_warn_level_mb) && (available_mb > reserve_mb) &&
         (available_mb > critical_level_mb);
}

bool Storage_State::low_level() {
  // Low level is below the reserve level but above the critical level
  // Warning levels are informational only
  return (available_mb < reserve_mb) && (available_mb > critical_level_mb);
}

bool Storage_State::critical_level() {
  // Critical level is the absolute lowest level
  return available_mb < critical_level_mb;
}

bool Storage_State::disk_error() { return dc->m_disk_error; }

/**
  Storage State - Transitions
*/
void Storage_State::set_normal_state() { dc->set_normal_state(); }
void Storage_State::set_warning_state() { dc->set_warning_state(); }
void Storage_State::set_low_limit_state() { dc->set_low_limit_state(); }
void Storage_State::set_critical_state() { dc->set_critical_state(); }
void Storage_State::set_error_state() { dc->set_error_state(); }

/**
  Storage State - Modes

  The Disk Collector will be in one of the following modes:
  - Normal: No action required
  - Fallback: Server is in read-only mode after storage depletion
  - Recovery: Server is in read-only mode after storage depletion, but
              is now preparing to restore normal operation
*/

/**
  Storage State - Disk Collector wrappers
*/
// clang-format off
Fallback_State &fallback() { return Storage_State::dc->fallback(); }
void start_fallback_timer() { Storage_State::dc->start_fallback_timer(); }
void stop_fallback_timer() { Storage_State::dc->stop_fallback_timer(); }
void cancel_fallback_timer() { Storage_State::dc->cancel_fallback_timer(); }
bool fallback_eligible(bool print_reason = true) { return Storage_State::dc->fallback_eligible(print_reason); }
bool check_fenced_server_state() { return Storage_State::dc->check_fenced_server_state(); }
bool forced_fallback_eligible() { return Storage_State::dc->forced_fallback_eligible(); }
Mode_Timer &fallback_timer() { return Storage_State::dc->fallback_timer(); }
void set_fallback_mode(bool force) { Storage_State::dc->set_fallback_mode(force); }
void process_expired_fallback_timer() { Storage_State::dc->process_expired_fallback_timer(); }

Recovery_State &recovery() { return Storage_State::dc->recovery(); }
void check_recovery_status() { Storage_State::dc->check_recovery_status(); }
// clang-format on

/**
  Start State - Check levels
*/
void Start_State::check_levels() {
  if (disk_error()) {
    set_error_state();
  } else if (normal_level()) {
    set_normal_state();
  } else if (warning_level()) {
    set_warning_state();
  } else if (low_level()) {
    set_low_limit_state();
  } else if (critical_level()) {
    set_critical_state();
  } else {
    dc->log_status();
  }
}

/**
  Stop State - Check levels
*/
void Stop_State::check_levels() {
  if (disk_error()) {
    set_error_state();
  } else if (normal_level()) {
    set_normal_state();
  } else if (warning_level()) {
    set_warning_state();
  } else if (low_level()) {
    set_low_limit_state();
  } else if (critical_level()) {
    set_critical_state();
  } else {
    dc->log_status();
  }
}

/**
  Normal State - Check levels
*/
void Normal_State::check_levels() {
  if (disk_error()) {
    set_error_state();
    return;
  }
  bool server_state_changed{false};
  if (fallback().fenced()) {
    server_state_changed = check_fenced_server_state();
    check_recovery_status();
  }
  if (normal_level()) {
    if (!recovery().in_progress() && !server_state_changed) {
      dc->log_status();
    }
  } else if (warning_level()) {
    set_warning_state();
  } else if (low_level()) {
    set_low_limit_state();
  } else if (critical_level()) {
    set_critical_state();
  }
}

/**
  Warning State - Enter
  Issue a warning immediately upon entering the warning state.
*/
void Warning_State::enter() {
  dc->m_last_warn_level_mb = Collector::no_warn_level;
  // Set the appropriate warning level
  check_warning_level();
}

/**
  Warning State - Check warning levels
    - Warning levels can be in any order
    - 0 is a valid warning level
  @return true if in warning state, false if above or below warning range
*/
bool Warning_State::check_warning_level() {
  // Below warning range
  if (available_mb < reserve_mb) return false;

  // In warning range or above
  size_t level_mb = Collector::no_warn_level;
  int errmsg{0};

  if (available_mb < warn_level_1_mb) {
    if (warn_level_1_mb < level_mb) {
      level_mb = warn_level_1_mb;
      errmsg = ER_HEALTH_WARNING_DISK_USAGE_LEVEL_1;
    }
  }
  if (available_mb < warn_level_2_mb) {
    if (warn_level_2_mb < level_mb) {
      level_mb = warn_level_2_mb;
      errmsg = ER_HEALTH_WARNING_DISK_USAGE_LEVEL_2;
    }
  }
  if (available_mb < warn_level_3_mb) {
    if (warn_level_3_mb < level_mb) {
      level_mb = warn_level_3_mb;
      errmsg = ER_HEALTH_WARNING_DISK_USAGE_LEVEL_3;
    }
  }

  const bool warning_state = (level_mb != Collector::no_warn_level);

  if (level_mb == dc->m_last_warn_level_mb) {
    // No change from the last level
    if (!recovery().in_progress()) {
      dc->log_status();
    }
  } else {
    // Either at a new warning level or no longer within warning range
    dc->m_last_warn_level_mb = level_mb;
    if (warning_state) {
      dc->log_warning_level(errmsg);
    }
  }
  return warning_state;
}

/**
  Warning State - Check levels
  Issue warnings when the warning level changes.
*/
void Warning_State::check_levels() {
  if (disk_error()) {
    set_error_state();
    return;
  }
  if (fallback().fenced()) {
    check_fenced_server_state();
    check_recovery_status();
  }
  if (normal_level()) {
    set_normal_state();
    // Warning state cleared, update status
    // Low limit state and critical state issue separate log messages
    dc->log_resource_levels();
  } else if (warning_level()) {
    // Still in warning range, adjust warning level if necessary
    check_warning_level();
  } else if (low_level()) {
    set_low_limit_state();
  } else if (critical_level()) {
    set_critical_state();
  }
}

/**
  Low Limit State - Enter
  Start fallback timer countdown.
*/
void Low_Limit_State::enter() {
  if (fallback_eligible(true)) {
    if (!fallback().fenced() && !fallback().super_read_only_retry_failed()) {
      start_fallback_timer();
    }
  }
  dc->log_warning_low_limit();
}

/**
  Low Limit State - Exit
  Cancel the fallback timer, clear Low Limit state variables.
*/
void Low_Limit_State::exit() {
  if (fallback_timer().running()) {
    cancel_fallback_timer();
  }
  fallback().set_super_read_only_force_failed(false);
}

/**
  Low Limit State - Check levels
  Fallback timer was started in ::enter() and is counting down.
  Issue warnings every 10 seconds.
  Fallback timer will be canceled in ::exit() before transitioning to new
  state.
*/
void Low_Limit_State::check_levels() {
  if (disk_error()) {
    set_error_state();
    return;
  }
  // Check the server state, sync the fallback state if necessary.
  bool server_state_changed{false};
  if (fallback().fenced()) {
    server_state_changed = check_fenced_server_state();
    check_recovery_status();
  }
  // Storage is at low level, fence the server if eligible.
  if (low_level()) {
    if (fallback().fenced()) {
      // Issue routine status if server state hasn't changed.
      if (!server_state_changed) {
        dc->log_status();
      }
      return;
    }
    // The fallback timer is running but the server is no longer eligible to be
    // fenced. Cancel the timer and issue status explaining why.
    if (!fallback_eligible()) {
      if (fallback_timer().running()) {
        cancel_fallback_timer();
      }
      dc->log_status();
      return;
    }
    // The fallback timer is running and the server is eligible to be fenced.
    // Check if timer has expired, otherwise issue a timer countdown message.
    if (fallback_timer().running()) {
      if (fallback_timer().expired()) {
        process_expired_fallback_timer();
      } else {
        if (fallback_timer().report_status()) {
          dc->log_warning_low_limit();
        }
      }
      return;
    }
    // The server is not fenced but is now eligible to be fenced. Re-enter the
    // Low Limit state and re-evaluate. This can happen when an OOD secondary
    // is promoted to primary.
    set_low_limit_state();
    return;
  }
  // Handle other state transitions
  if (normal_level()) {
    set_normal_state();
  } else if (warning_level()) {
    set_warning_state();
  } else if (critical_level()) {
    set_critical_state();
  }
}

/**
  Critical State - Enter
  If the server hasn't been fenced yet, then fence immediately.
  Force SUPER_READ_ONLY if necessary.
*/
void Critical_State::enter() {
  if (fallback_eligible(true)) {
    const bool ok_to_force{forced_fallback_eligible()};
    if (!fallback().super_read_only_was_set_by_hm() || ok_to_force) {
      set_fallback_mode(ok_to_force);
    }
  }
  dc->log_status();
}

/**
  Critical Limit State - Exit
  Clear Critical state variables.
*/
void Critical_State::exit() {
  fallback().set_super_read_only_force_failed(false);
}

/**
  Critical State - Check levels
  Server is (or should be) fenced, issue regular warnings.
*/
void Critical_State::check_levels() {
  if (disk_error()) {
    set_error_state();
    return;
  }
  // Check the server state, sync the fallback state if necessary.
  bool server_state_changed{false};
  if (fallback().fenced()) {
    server_state_changed = check_fenced_server_state();
    check_recovery_status();
  }
  // Storage is at the critical level, fence the server immediately if eligible.
  if (critical_level()) {
    if (fallback().fenced()) {
      // Issue routine status if server state hasn't changed.
      if (!server_state_changed) {
        dc->log_status();
      }
      return;
    }
    // The server is not fenced but is now eligible to be fenced. Re-enter the
    // Critical state and re-evaluate. This can happen when an OOD secondary
    // is promoted to primary.
    set_critical_state();
    return;
  }
  // Handle other state transitions
  if (normal_level()) {
    set_normal_state();
  } else if (warning_level()) {
    set_warning_state();
  } else if (low_level()) {
    set_low_limit_state();
  }
}

/**
  Error State - Enter
*/
void Error_State::enter() {
  // The failed sample logs the path-specific transition and error reason.
}

/**
  Error State - Check levels
*/
void Error_State::check_levels() {
  if (disk_error()) {
    // Reconcile owned server modes without using unavailable storage input
    if (fallback().fenced()) {
      check_fenced_server_state();
    }
    return;
  }
  if (fallback().fenced()) {
    check_recovery_status();
  }
  if (normal_level()) {
    set_normal_state();
  } else if (warning_level()) {
    set_warning_state();
  } else if (low_level()) {
    set_low_limit_state();
  } else if (critical_level()) {
    set_critical_state();
  }
}

}  // namespace hm

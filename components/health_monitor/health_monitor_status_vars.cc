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

#include "health_monitor_status_vars.h"

#include "health_monitor.h"
#include "health_monitor_disk.h"
#include "health_monitor_disk_forecaster.h"
#include "health_monitor_log.h"
#include "health_monitor_option_usage.h"

namespace hm {

const std::string k_offline_mode_disk_full{"offline_mode_disk_full"};
const std::string k_super_read_only_disk_full{"super_read_only_disk_full"};
const std::string k_recovery_cycles{"recovery_cycles"};
const std::string k_option_tracker_usage{"option_tracker_usage:Health Monitor"};
const std::string k_disk_forecaster_run_mode{"disk_forecaster_run_mode"};
const std::string k_disk_forecaster_test_state{"disk_forecaster_test_state"};

/**
  Initialize the Health Monitor status variables.
*/
// cppcheck-suppress unusedFunction
// clang-format off
int Status_vars::initialize_all() {
  // Disk collector status variables
  offline_mode_disk_full.var.name = k_offline_mode_disk_full.c_str();
  offline_mode_disk_full.var.value = reinterpret_cast<char *>(Health_Monitor::disk_collector()->offline_mode_set_ptr());
  offline_mode_disk_full.var.type = SHOW_BOOL;
  offline_mode_disk_full.var.scope = SHOW_SCOPE_GLOBAL;

  super_read_only_disk_full.var.name = k_super_read_only_disk_full.c_str();
  super_read_only_disk_full.var.value = reinterpret_cast<char *>(Health_Monitor::disk_collector()->super_read_only_set_ptr());
  super_read_only_disk_full.var.type = SHOW_BOOL;
  super_read_only_disk_full.var.scope = SHOW_SCOPE_GLOBAL;

  disk_recovery_count.var.name = k_recovery_cycles.c_str();
  disk_recovery_count.var.value = reinterpret_cast<char *>(Health_Monitor::disk_collector()->recovery_count_ptr());
  disk_recovery_count.var.type = SHOW_INT;
  disk_recovery_count.var.scope = SHOW_SCOPE_GLOBAL;

  option_tracker_usage.var.name = k_option_tracker_usage.c_str();
  option_tracker_usage.var.value = reinterpret_cast<char *>(&Option_Usage::usage_count);
  option_tracker_usage.var.type = SHOW_LONGLONG;
  option_tracker_usage.var.scope = SHOW_SCOPE_GLOBAL;

  disk_forecaster_run_mode.var.name = k_disk_forecaster_run_mode.c_str();
  disk_forecaster_run_mode.var.value = const_cast<char *>(Health_Monitor::disk_forecaster()->run_mode_name_ptr());
  disk_forecaster_run_mode.var.type = SHOW_CHAR;
  disk_forecaster_run_mode.var.scope = SHOW_SCOPE_GLOBAL;

  disk_forecaster_test_state.var.name = k_disk_forecaster_test_state.c_str();
  disk_forecaster_test_state.var.value = const_cast<char *>(Health_Monitor::disk_forecaster()->test_state_name_ptr());
  disk_forecaster_test_state.var.type = SHOW_CHAR;
  disk_forecaster_test_state.var.scope = SHOW_SCOPE_GLOBAL;
  return 0;
}
// clang-format on

/**
  Register the Health Monitor status variables with the server.
*/
// cppcheck-suppress unusedFunction
int Status_vars::register_all() {
  if (m_registered) {
    return 0;
  }
  if (register_status_var(
          Health_Monitor::status_vars()->offline_mode_disk_full) != 0) {
    return 1;
  }
  if (register_status_var(
          Health_Monitor::status_vars()->super_read_only_disk_full) != 0) {
    return 1;
  }
  if (register_status_var(Health_Monitor::status_vars()->disk_recovery_count) !=
      0) {
    return 1;
  }
  if (register_status_var(
          Health_Monitor::status_vars()->option_tracker_usage) != 0) {
    return 1;
  }
  if (register_status_var(
          Health_Monitor::status_vars()->disk_forecaster_run_mode) != 0) {
    return 1;
  }
  if (register_status_var(
          Health_Monitor::status_vars()->disk_forecaster_test_state) != 0) {
    return 1;
  }
  m_registered = true;
  return 0;
}

/**
  Unregister the Health Monitor status variables.
*/
// cppcheck-suppress unusedFunction
void Status_vars::unregister_all() {
  if (Health_Monitor::status_vars()->m_registered) {
    unregister_status_var(
        Health_Monitor::status_vars()->offline_mode_disk_full);
    unregister_status_var(
        Health_Monitor::status_vars()->super_read_only_disk_full);
    unregister_status_var(Health_Monitor::status_vars()->disk_recovery_count);
    unregister_status_var(Health_Monitor::status_vars()->option_tracker_usage);
    unregister_status_var(
        Health_Monitor::status_vars()->disk_forecaster_run_mode);
    unregister_status_var(
        Health_Monitor::status_vars()->disk_forecaster_test_state);
  }
  m_registered = false;
}

/**
  Register one status variable.
*/
int Status_vars::register_status_var(Statvar &statvar) {
  SHOW_VAR statvars[]{{statvar.var},
                      {Health_Monitor::status_vars()->null_entry.var}};
  if (statvar_register_srv->register_variable(statvars) != 0) {
    log_error("Health Monitor: Failed to register status variable '%s'",
              statvar.var.name);
    return 1;
  }
  statvar.registered = true;
  return 0;
}

/**
  Unregister one status variable.
*/
int Status_vars::unregister_status_var(Statvar &statvar) {
  SHOW_VAR statvars[]{{statvar.var},
                      {Health_Monitor::status_vars()->null_entry.var}};
  if (statvar_register_srv->unregister_variable(statvars) != 0) {
    log_error("Health Monitor: Failed to unregister status variable '%s'",
              statvar.var.name);
    return 1;
  }
  statvar.registered = false;
  return 0;
}

}  // namespace hm

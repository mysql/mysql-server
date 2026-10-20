/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_disk_forecaster.h"

#include <cstring>  // std::strncpy

#include "health_monitor_system_vars.h"

namespace hm {

// -----------------------------------------------------------------------------
// Set mode
// -----------------------------------------------------------------------------

void Disk_Forecaster::set_run_mode(Dfx_Run_Mode mode) {
  set_is_new_run_mode((m_run_mode != mode));
  // The run mode can be set internally, so always set both
  // Disk_Forecaster::m_run_mode and System_vars::disk_forecast_run_mode
  m_run_mode = mode;
  System_vars::disk_forecast_run_mode().val = static_cast<unsigned long>(mode);

  // Update the status variable 'disk_forecast_run_mode'
  std::string mode_str = run_mode_name();
  std::strncpy(m_dfx_run_mode_name, mode_str.c_str(),
               sizeof(m_dfx_run_mode_name) - 1);
  m_dfx_run_mode_name[sizeof(m_dfx_run_mode_name) - 1] = '\0';
}

void Disk_Forecaster::set_test_state(Dfx_Test_State state) {
  m_test_state = state;
  // Update status var buffer, null-terminate
  std::string state_str = test_state_name();
  std::strncpy(m_dfx_test_state_name, state_str.c_str(),
               sizeof(m_dfx_test_state_name) - 1);
  m_dfx_test_state_name[sizeof(m_dfx_test_state_name) - 1] = '\0';
}

// -----------------------------------------------------------------------------
// Mode names
// -----------------------------------------------------------------------------

const std::array<std::string, 4> Disk_Forecaster::RUN_MODE_NAMES = {
    "NONE", "NORMAL", "TEST", "STANDBY"};
const std::array<std::string, 3> Disk_Forecaster::TEST_STATE_NAMES = {
    "NONE", "IDLE", "RUNNING"};
const std::array<std::string, 4> Disk_Forecaster::FORECAST_STATUS_NAMES = {
    "NONE", "NORMAL", "WARNING", "CRITICAL"};

// Status variable string buffers
char Disk_Forecaster::m_dfx_run_mode_name[32]{};
char Disk_Forecaster::m_dfx_test_state_name[32]{};

// Name buffers for PLUGIN_VAR_ENUM system variables
const char *Disk_Forecaster::dfx_run_mode_names[5] = {
    RUN_MODE_NAMES[0].c_str(), RUN_MODE_NAMES[1].c_str(),
    RUN_MODE_NAMES[2].c_str(), RUN_MODE_NAMES[3].c_str(), nullptr};

const std::string &Disk_Forecaster::run_mode_name() {
  auto idx = static_cast<size_t>(m_run_mode);
  if (idx < RUN_MODE_NAMES.size()) {
    return RUN_MODE_NAMES[idx];
  }
  static const std::string unknown = "UNKNOWN";
  return unknown;
}

const std::string &Disk_Forecaster::test_state_name() {
  auto idx = static_cast<size_t>(m_test_state);
  if (idx < TEST_STATE_NAMES.size()) {
    return TEST_STATE_NAMES[idx];
  }
  static const std::string unknown = "UNKNOWN";
  return unknown;
}

const std::string &Disk_Forecaster::forecast_status_name(Dfx_Status status) {
  auto idx = static_cast<size_t>(status);
  if (idx < FORECAST_STATUS_NAMES.size()) {
    return FORECAST_STATUS_NAMES[idx];
  }
  static const std::string unknown = "UNKNOWN";
  return unknown;
}

}  // namespace hm

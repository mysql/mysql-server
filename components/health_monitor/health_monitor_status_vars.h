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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include "component_status.h"

namespace hm {

extern const std::string k_offline_mode_disk_full;
extern const std::string k_super_read_only_disk_full;
extern const std::string k_recovery_cycles;
extern const std::string k_option_tracker_usage;
extern const std::string k_disk_forecaster_test_state;

class Statvar {
 public:
  Statvar() = default;
  SHOW_VAR var{nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_UNDEF};
  bool registered{false};
};

class Status_vars {
 public:
  Status_vars() = default;

  int initialize_all();
  int register_all();
  void unregister_all();
  static int register_status_var(Statvar &statvar);
  static int unregister_status_var(Statvar &statvar);

  // True if the Health Monitor set OFFLINE_MODE=ON to fence the server.
  // False if
  //   - the Health Monitor set OFFLINE_MODE=OFF after storage recovery, or
  //   - OFFLINE_MODE was set externally to the Health Monitor.
  Statvar offline_mode_disk_full;

  // True if the Health Monitor set SUPER_READ_ONLY=ON to fence the server.
  // False if
  //   - the Health Monitor set SUPER_READ_ONLY=OFF after storage recovery, or
  //   - SUPER_READ_ONLY was set externally to the Health Monitor.
  Statvar super_read_only_disk_full;

  // The number of storage depletion and recovery cycles, or the number of times
  // that available disk space recovered after the Health Monitor put the server
  // into SUPER_READ_ONLY and OFFLINE_MODE.
  // Max cycles given by `health_monitor.disk_recovery_max_cycles`.
  Statvar disk_recovery_count;

  // Option tracker usage
  Statvar option_tracker_usage;

  // Disk Forecaster run mode.
  // - NORMAL:  Normal processing, metric collection, disk usage forecasting
  // - TEST:    A unit test or simulation is being run
  // - STANDBY: Thread is active but processing is paused
  Statvar disk_forecaster_run_mode;

  // Disk Forecaster test state.
  // - IDLE:    No test is running (normal operation)
  // - RUNNING: Unit test or simulation is running
  Statvar disk_forecaster_test_state;

  // Null terminator for the SHOW_VAR array
  Statvar null_entry;
  bool m_registered{false};
};

}  // namespace hm

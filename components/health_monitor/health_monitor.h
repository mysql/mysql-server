/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "component_service.h"
#include "health_monitor_constants.h"

namespace hm {

class Health_Monitor;
class Disk_Collector;
class Disk_Forecaster;
class Memory_Collector;
class System_Collector;
class Log;
class Periodic_Reporter;
class Status_vars;
class Supervisor;
class System_vars;
class Tables;
class Triggered_Reporter;

extern std::unique_ptr<Health_Monitor> health_monitor;
extern std::unique_ptr<Disk_Collector> disk_collector;
extern std::unique_ptr<Disk_Forecaster> disk_forecaster;
extern std::unique_ptr<Memory_Collector> memory_collector;
extern std::unique_ptr<System_Collector> system_collector;
extern std::unique_ptr<Log> log;
extern std::unique_ptr<Triggered_Reporter> memory_reporter;
extern std::unique_ptr<Periodic_Reporter> periodic_reporter;
extern std::unique_ptr<Status_vars> status_vars;
extern std::unique_ptr<Supervisor> supervisor;
extern std::unique_ptr<Tables> tables;

class Health_Monitor {
 public:
  Health_Monitor() = default;

  [[nodiscard]] const char *name() const { return m_name.c_str(); }
  [[nodiscard]] const char *sysvar_prefix() const {
    return m_sysvar_prefix.c_str();
  }

  int collectors_init();
  int collectors_deinit();
  int collectors_start();
  int collectors_stop();

  static int log_init();
  static int log_deinit();

  static int reporters_init();
  static int reporters_deinit();

  static int status_vars_init();
  static int status_vars_deinit();
  int status_vars_register();
  int status_vars_unregister();

  static int supervisor_init();
  static int supervisor_deinit();

  static int system_vars_init();
  // No need for system_vars_deinit() since System_vars has no state
  int system_vars_register();
  int system_vars_unregister();
  static int system_vars_update();

  static int tables_init();
  static int tables_deinit();
  int tables_register();
  int tables_unregister();

  int metrics_register();
  int metrics_unregister();

  static void server_utils_init();
  static void server_utils_deinit();

  static int option_usage_tracker_init();
  static int option_usage_tracker_deinit();

  void set_server_shutting_down(bool shutdown);
  [[nodiscard]] bool initialized() const {
    return m_hm_initialized.load(std::memory_order_acquire);
  }
  static bool available() noexcept;

  mysql_service_status_t component_init();
  mysql_service_status_t component_deinit();

 private:
  const std::string m_name{"Health Monitor"};
  const std::string m_sysvar_prefix{"health_monitor"};
  static std::atomic<bool> m_hm_initialized;
  static std::atomic<bool> m_server_shutting_down;
  bool m_status_vars_registered{false};
  bool m_system_vars_registered{false};
  bool m_collectors_initialized{false};
  bool m_collectors_started{false};
  bool m_tables_initialized{false};
  bool m_tables_registered{false};
  bool m_metrics_registered{false};
};

}  // namespace hm

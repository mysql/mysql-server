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

/**
  Own the objects used by one Health Monitor component instance.

  Component initialization and deinitialization control their lifetime.
  Accessors return borrowed pointers, which can be null during initialization
  and teardown. Callers must not retain them beyond the component lifetime.
  Accessors do not synchronize access with component deinitialization.
*/
class Health_Monitor {
 public:
  static mysql_service_status_t component_init();
  static mysql_service_status_t component_deinit();

  [[nodiscard]] static Health_Monitor *instance() noexcept {
    return m_instance;
  }
  [[nodiscard]] static Disk_Collector *disk_collector() noexcept {
    return m_instance != nullptr ? m_instance->m_disk_collector.get() : nullptr;
  }
  [[nodiscard]] static Disk_Forecaster *disk_forecaster() noexcept {
    return m_instance != nullptr ? m_instance->m_disk_forecaster.get()
                                 : nullptr;
  }
  [[nodiscard]] static Memory_Collector *memory_collector() noexcept {
    return m_instance != nullptr ? m_instance->m_memory_collector.get()
                                 : nullptr;
  }
  [[nodiscard]] static System_Collector *system_collector() noexcept {
    return m_instance != nullptr ? m_instance->m_system_collector.get()
                                 : nullptr;
  }
  [[nodiscard]] static Log *log() noexcept {
    return m_instance != nullptr ? m_instance->m_log.get() : nullptr;
  }
  [[nodiscard]] static Periodic_Reporter *periodic_reporter() noexcept {
    return m_instance != nullptr ? m_instance->m_periodic_reporter.get()
                                 : nullptr;
  }
  [[nodiscard]] static Status_vars *status_vars() noexcept {
    return m_instance != nullptr ? m_instance->m_status_vars.get() : nullptr;
  }
  [[nodiscard]] static Supervisor *supervisor() noexcept {
    return m_instance != nullptr ? m_instance->m_supervisor.get() : nullptr;
  }
  [[nodiscard]] static Tables *tables() noexcept {
    return m_instance != nullptr ? m_instance->m_tables.get() : nullptr;
  }
  [[nodiscard]] static Triggered_Reporter *memory_reporter() noexcept {
    return m_instance != nullptr ? m_instance->m_memory_reporter.get()
                                 : nullptr;
  }

  [[nodiscard]] const char *name() const { return m_name.c_str(); }
  [[nodiscard]] const char *sysvar_prefix() const {
    return m_sysvar_prefix.c_str();
  }

  static int system_vars_update();

  void set_server_shutting_down(bool shutdown);
  [[nodiscard]] bool initialized() const {
    return m_hm_initialized.load(std::memory_order_acquire);
  }
  static bool available() noexcept;

 private:
  Health_Monitor();
  ~Health_Monitor();
  Health_Monitor(const Health_Monitor &) = delete;
  Health_Monitor &operator=(const Health_Monitor &) = delete;
  Health_Monitor(Health_Monitor &&) = delete;
  Health_Monitor &operator=(Health_Monitor &&) = delete;

  mysql_service_status_t initialize();
  mysql_service_status_t deinitialize();

  int collectors_init();
  int collectors_deinit();
  int collectors_start();
  int collectors_stop();

  int log_init();
  int log_deinit();

  int reporters_init();
  int reporters_deinit();

  int status_vars_init();
  int status_vars_deinit();
  int status_vars_register();
  int status_vars_unregister();

  int supervisor_init();
  int supervisor_deinit();

  int system_vars_init();
  // No need for system_vars_deinit() since System_vars has no state
  int system_vars_register();
  int system_vars_unregister();

  int tables_init();
  int tables_deinit();
  int tables_register();
  int tables_unregister();

  int metrics_register();
  int metrics_unregister();

  void server_utils_init();
  void server_utils_deinit();

  int option_usage_tracker_init();
  int option_usage_tracker_deinit();

  // The component entry points create and destroy this instance explicitly.
  // A raw pointer avoids a destructor running after component services unload.
  static Health_Monitor *m_instance;

  std::unique_ptr<Disk_Collector> m_disk_collector;
  std::unique_ptr<Disk_Forecaster> m_disk_forecaster;
  std::unique_ptr<Memory_Collector> m_memory_collector;
  std::unique_ptr<System_Collector> m_system_collector;
  std::unique_ptr<Log> m_log;
  std::unique_ptr<Periodic_Reporter> m_periodic_reporter;
  std::unique_ptr<Status_vars> m_status_vars;
  std::unique_ptr<Supervisor> m_supervisor;
  std::unique_ptr<Tables> m_tables;
  std::unique_ptr<Triggered_Reporter> m_memory_reporter;

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

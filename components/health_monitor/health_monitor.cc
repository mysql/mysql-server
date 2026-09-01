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

#include "health_monitor.h"

#include "component_status.h"
#include "component_sysvar.h"

#include "health_monitor_disk.h"
#include "health_monitor_disk_forecaster.h"
#include "health_monitor_log.h"
#include "health_monitor_memory.h"
#include "health_monitor_metrics.h"
#include "health_monitor_option_usage.h"
#include "health_monitor_periodic_reporter.h"
#include "health_monitor_server.h"
#include "health_monitor_status_vars.h"
#include "health_monitor_supervisor.h"
#include "health_monitor_system.h"
#include "health_monitor_system_vars.h"
#include "health_monitor_tables.h"
#include "health_monitor_triggered_reporter.h"

#include <mysql/components/services/psi_metric.h>
#include <mysql/psi/mysql_metric.h>

REQUIRES_MYSQL_COND_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_MUTEX_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;
REQUIRES_PSI_METRIC_SERVICE_PLACEHOLDER;
REQUIRES_PSI_THREAD_SERVICE_PLACEHOLDER;

namespace hm {

std::unique_ptr<Health_Monitor> health_monitor{nullptr};
std::unique_ptr<Disk_Collector> disk_collector{nullptr};
std::unique_ptr<Disk_Forecaster> disk_forecaster{nullptr};
std::unique_ptr<Memory_Collector> memory_collector{nullptr};
std::unique_ptr<System_Collector> system_collector{nullptr};
std::unique_ptr<Log> log{nullptr};
std::unique_ptr<Periodic_Reporter> periodic_reporter{nullptr};
std::unique_ptr<Status_vars> status_vars{nullptr};
std::unique_ptr<Supervisor> supervisor{nullptr};
std::unique_ptr<Tables> tables{nullptr};
std::unique_ptr<Triggered_Reporter> memory_reporter{nullptr};

/**
  Initialize the Health Monitor collectors.
*/
int Health_Monitor::collectors_init() {
  if (m_collectors_initialized) {
    return 0;
  }
  disk_collector = std::make_unique<Disk_Collector>();
  if (disk_collector == nullptr) {
    return 1;
  }
  disk_collector->initialize();

  disk_forecaster = std::make_unique<Disk_Forecaster>();
  if (disk_forecaster == nullptr) {
    return 1;
  }
  disk_forecaster->initialize();

  memory_collector = std::make_unique<Memory_Collector>();
  if (memory_collector == nullptr) {
    return 1;
  }

  system_collector = std::make_unique<System_Collector>();
  if (system_collector == nullptr) {
    return 1;
  }

  m_collectors_initialized = true;
  return 0;
}

/**
  Deinitialize the Health Monitor collectors.
*/
int Health_Monitor::collectors_deinit() {
  if (m_collectors_started) {
    return 1;
  }
  disk_collector.reset();
  disk_forecaster.reset();
  memory_collector.reset();
  system_collector.reset();
  m_collectors_initialized = false;
  return 0;
}

/**
  Start the Health Monitor collectors.
  @return
  0 - Supervisor and one or more collectors started successfully
  0 - Supervisor started and no collectors are enabled
  1 - Supervisor or all enabled collectors failed to start
*/
int Health_Monitor::collectors_start() {
  if (m_collectors_started) {
    return 0;
  }

  // Disk Collector
  const bool disk_enabled{System_vars::disk_running().val};
  int ret_disk = 0;
  if (disk_enabled) {
    ret_disk = disk_collector->start();
  } else {
    disk_collector->log_collector_status(true);
  }

  // Disk Forecaster
  const bool diskf_enabled{System_vars::disk_forecast_running().val};
  int ret_diskf = 0;
  if (diskf_enabled) {
    ret_diskf = disk_forecaster->start();
  } else {
    disk_forecaster->log_collector_status(true);
  }

  // Memory Collector
  const bool memory_enabled{System_vars::memory_running().val};
  int ret_memory = 0;
  if (memory_enabled) {
    ret_memory = memory_collector->start();
  } else {
    memory_collector->log_collector_status(true);
  }

  // System Collector
  const bool system_enabled{System_vars::system_running().val};
  int ret_system = 0;
  if (system_enabled) {
    ret_system = system_collector->start();
  } else {
    system_collector->log_collector_status(true);
  }

  // Memory Reporter
  const bool memory_rpt_enabled{System_vars::memory_reporting().val};
  int ret_memory_rpt = 0;
  if (memory_rpt_enabled) {
    ret_memory_rpt = memory_reporter->start();
  } else {
    memory_reporter->log_collector_status(true);
  }

  // Periodic Reporter
  const bool periodic_rpt_enabled{System_vars::periodic_reporting().val};
  int ret_periodic_rpt = 0;
  if (periodic_rpt_enabled) {
    ret_periodic_rpt = periodic_reporter->start();
  } else {
    periodic_reporter->log_collector_status(true);
  }

  // Supervisor
  const int ret_supervisor{supervisor->start()};
  if (ret_supervisor != 0) {
    return 1;
  }

  const bool any_enabled{disk_enabled || diskf_enabled || memory_enabled ||
                         system_enabled || memory_rpt_enabled ||
                         periodic_rpt_enabled};
  const bool any_started{(disk_enabled && ret_disk == 0) ||
                         (diskf_enabled && ret_diskf == 0) ||
                         (memory_enabled && ret_memory == 0) ||
                         (system_enabled && ret_system == 0) ||
                         (memory_rpt_enabled && ret_memory_rpt == 0) ||
                         (periodic_rpt_enabled && ret_periodic_rpt == 0)};

  // All enabled collectors failed to start
  if (any_enabled && !any_started) {
    return 1;
  }

  // One or more collectors was started
  m_collectors_started = true;
  return 0;
}

/**
  Shut down the collectors.
*/
int Health_Monitor::collectors_stop() {
  if (supervisor) {
    supervisor->stop();
  }
  if (periodic_reporter) {
    periodic_reporter->stop();
  }
  if (memory_reporter) {
    memory_reporter->stop();
  }
  if (disk_collector) {
    disk_collector->stop();
  }
  if (disk_forecaster) {
    disk_forecaster->stop();
  }
  if (memory_collector) {
    memory_collector->stop();
  }
  if (system_collector) {
    system_collector->stop();
  }
  m_collectors_started = false;
  return 0;
}

/**
  Initialize the log manager.
*/
int Health_Monitor::log_init() {
  if (log == nullptr) {
    log = std::make_unique<Log>();
    if (log == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the log manager.
*/
int Health_Monitor::log_deinit() {
  if (log != nullptr) {
    log.reset();
  }
  return 0;
}

/**
  Initialize the report manager.
*/
int Health_Monitor::reporters_init() {
  if (memory_reporter == nullptr) {
    memory_reporter = std::make_unique<Triggered_Reporter>(
        "Memory Reporter", "REPORTER", "memory_reporter", "health_mem_rep",
        "Health Monitor memory reporter", &System_vars::memory_reporting(),
        &System_vars::memory_report_definition());
    if (memory_reporter == nullptr) {
      return 1;
    }
  }
  if (periodic_reporter == nullptr) {
    periodic_reporter = std::make_unique<Periodic_Reporter>(
        "Periodic Reporter", "REPORTER", "periodic_reporter", "health_per_rep",
        "Health Monitor periodic reporter", &System_vars::periodic_reporting(),
        &System_vars::periodic_report_definition());
    if (periodic_reporter == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the reporters.
*/
int Health_Monitor::reporters_deinit() {
  if (periodic_reporter != nullptr) {
    periodic_reporter.reset();
  }
  if (memory_reporter != nullptr) {
    memory_reporter.reset();
  }
  return 0;
}

/**
  Initialize the status variable manager and initialize (but do not register)
  all status variables.
*/
int Health_Monitor::status_vars_init() {
  if (status_vars == nullptr) {
    status_vars = std::make_unique<Status_vars>();
    if (status_vars == nullptr) {
      return 1;
    }
  }
  return status_vars->initialize_all();
}

/**
  Deinitialize the status variable manager after all status variables
  have been deregistered.
*/
int Health_Monitor::status_vars_deinit() {
  if (status_vars != nullptr) {
    status_vars.reset();
  }
  return 0;
}

/**
  Register all status variables.
*/
int Health_Monitor::status_vars_register() {
  if (m_status_vars_registered) {
    return 0;
  }
  if (status_vars->register_all() != 0) {
    return 1;
  }
  m_status_vars_registered = true;
  return 0;
}

/**
  Deregister all status variables.
*/
int Health_Monitor::status_vars_unregister() {
  if (status_vars != nullptr) {
    status_vars->unregister_all();
  }
  m_status_vars_registered = false;
  return 0;
}

/**
  Initialize the task supervisor.
*/
int Health_Monitor::supervisor_init() {
  if (supervisor == nullptr) {
    supervisor = std::make_unique<Supervisor>();
    if (supervisor == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the task supervisor.
*/
int Health_Monitor::supervisor_deinit() {
  if (supervisor != nullptr) {
    supervisor.reset();
  }
  return 0;
}

/**
  Initialize but do not register all system variables.
*/
int Health_Monitor::system_vars_init() { return System_vars::initialize_all(); }

/**
  Register all system variables.
*/
int Health_Monitor::system_vars_register() {
  if (m_system_vars_registered) {
    return 0;
  }
  if (System_vars::register_all() != 0) {
    return 1;
  }
  m_system_vars_registered = true;
  return 0;
}

/**
  Deregister all system variables.
*/
int Health_Monitor::system_vars_unregister() {
  System_vars::unregister_all();
  m_system_vars_registered = false;
  return 0;
}

/**
  Notify collectors of pending system variable updates.

  This thread currently holds the LOCK_global_system_variables. The update
  process will signal the thread to stop, apply the new settings, and then
  restart it.

  To avoid deadlocks, the update will be deferred for threads executing SQL
  queries or accessing global system variables until they complete their
  processing and release the LOCK_global_system_variables lock.
*/
int Health_Monitor::system_vars_update() {
  bool signal_supervisor{false};

  // Defer
  if (disk_collector) {
    disk_collector->mark_settings_pending();
    // An active disk thread signals after its current processing cycle.
    if (disk_collector->Thread::running() != System_vars::disk_running().val) {
      signal_supervisor = true;
    }
  }
  if (disk_forecaster) {
    disk_forecaster->mark_settings_pending();
    if (disk_forecaster->Thread::running() !=
        System_vars::disk_forecast_running().val) {
      signal_supervisor = true;
    }
  }
  if (memory_reporter) {
    memory_reporter->mark_settings_pending();
    if (memory_reporter->Thread::running() !=
        System_vars::memory_reporting().val) {
      signal_supervisor = true;
    }
  }
  if (periodic_reporter) {
    periodic_reporter->mark_settings_pending();
    if (periodic_reporter->Thread::running() !=
        System_vars::periodic_reporting().val) {
      signal_supervisor = true;
    }
  }
  if (memory_collector) {
    memory_collector->mark_settings_pending();
    signal_supervisor = true;
  }
  if (system_collector) {
    system_collector->mark_settings_pending();
    signal_supervisor = true;
  }
  if (signal_supervisor && supervisor) {
    supervisor->notify_settings_update();
  }
  return 0;
}

/**
  Initialize the table manager.
*/
int Health_Monitor::tables_init() {
  if (tables == nullptr) {
    tables = std::make_unique<Tables>();
    if (tables == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the table manager after all tables have been deregistered.
*/
int Health_Monitor::tables_deinit() {
  if (tables != nullptr) {
    tables.reset();
  }
  return 0;
}

/**
  Register all Health Monitor tables in the Performance Schema.
*/
int Health_Monitor::tables_register() {
  if (m_tables_registered) {
    return 0;
  }
  if (tables->register_all() != 0) {
    return 1;
  }
  m_tables_registered = true;
  return 0;
}

/**
  Deregister all Health Monitor tables.
*/
int Health_Monitor::tables_unregister() {
  if (tables != nullptr) {
    tables->unregister_all();
  }
  m_tables_registered = false;
  return 0;
}

/**
  Register metric sources exposed by the component.
*/
int Health_Monitor::metrics_register() {
  if (m_metrics_registered) {
    return 0;
  }
  mysql_meter_register(health_meter, get_health_meter_count());
  m_metrics_registered = true;
  return 0;
}

/**
  Deregister metric sources exposed by the component.
*/
int Health_Monitor::metrics_unregister() {
  mysql_meter_unregister(health_meter, get_health_meter_count());
  m_metrics_registered = false;
  return 0;
}

/**
  Initialize the server utility functions.
*/
void Health_Monitor::server_utils_init() {
  mysql_service_status_t const ret =
      Server::register_group_member_status_listeners();
  if (ret != 0) {
    log_info("%s: Failed to register group member status listeners",
             health_monitor->name());
  } else {
    log_info("%s: Registered group member status listeners",
             health_monitor->name());
  }
}

/**
  Deinitialize the server utility functions.
*/
void Health_Monitor::server_utils_deinit() {
  mysql_service_status_t const ret =
      Server::unregister_group_member_status_listeners();
  if (ret != 0) {
    log_info("%s: Failed to unregister group member status listeners",
             health_monitor->name());
  } else {
    log_info("%s: Unregistered group member status listeners",
             health_monitor->name());
  }
}

/**
  Register option usage tracker.
*/
int Health_Monitor::option_usage_tracker_init() {
  int ret{0};
  if (Option_Usage::usage_register()) {
    log_info("%s: Failed to register with option usage tracker",
             health_monitor->name());
    ret = 1;
  } else {
    log_info("%s: Registered with option usage tracker",
             health_monitor->name());
  }
  return ret;
}

/**
  Deregister option usage tracker.
*/
int Health_Monitor::option_usage_tracker_deinit() {
  int ret{0};
  if (Option_Usage::usage_deregister()) {
    log_info("%s: Failed to unregister from option usage tracker",
             health_monitor->name());
    ret = 1;
  } else {
    log_info("%s: Unregistered from option usage tracker",
             health_monitor->name());
  }
  return ret;
}

/**
  Health monitor initialization:
  - Initialize the logging service
  - Initialize the report manager
  - Register system and status variables
  - Start enabled collectors
  - Initialize Health Monitor tables in the Performance Schema
  - Register metric sources for telemetry
*/
mysql_service_status_t Health_Monitor::component_init() {
  m_hm_initialized.store(false, std::memory_order_release);

  if (log_init() != 0) {
    goto error;
  }
  log_info("%s: Starting...", health_monitor->name());

  log_info("%s: Initializing task supervisor", health_monitor->name());
  if (supervisor_init() != 0) {
    goto error;
  }

  // System variables must be registered before initializing the collectors
  if (system_vars_init() != 0) {
    goto error;
  }

  log_info("%s: Registering server system variables", health_monitor->name());
  if (system_vars_register() != 0) {
    goto error;
  }

  log_info("%s: Initializing reporters", health_monitor->name());
  if (reporters_init() != 0) {
    goto error;
  }

  log_info("%s: Initializing collectors", health_monitor->name());
  if (collectors_init() != 0) {
    goto error;
  }

  // Status variables must be registered after the collectors are initialized
  if (status_vars_init() != 0) {
    goto error;
  }
  log_info("%s: Registering status variables", health_monitor->name());
  if (status_vars_register() != 0) {
    goto error;
  }

  log_info("%s: Starting collectors", health_monitor->name());
  if (collectors_start() != 0) {
    goto error;
  }

  // Tables can only be registered once a successful component initialization is
  // assured. Tables created during component initialization cannot be deleted,
  // i.e. cleaned up, until after the component is fully installed.
  // See Bug#33973047 "Health Monitor crashes due to full disk".

  log_info("%s: Registering Performance Schema tables", health_monitor->name());

  if (tables_init() != 0) {
    goto error;
  }
  if (tables_register() != 0) {
    goto error;
  }

  log_info("%s: Registering metric sources", health_monitor->name());
  if (metrics_register() != 0) {
    goto error;
  }

  log_info("%s: Initializing server utilities", health_monitor->name());
  server_utils_init();

  log_info("%s: Registering option usage tracker", health_monitor->name());
  if (option_usage_tracker_init() != 0) {
    goto error;
  }

  m_hm_initialized.store(true, std::memory_order_release);
  // Process system variable updates queued during component initialization
  supervisor->notify_settings_update();
  log_info("Health Monitor: Started");
  return 0;

error:
  if (log) {
    log_error("Health Monitor: Failed to start");
  }
  component_deinit();
  return 1;
}

// clang-format off

std::atomic<bool> Health_Monitor::m_hm_initialized{false};
std::atomic<bool> Health_Monitor::m_server_shutting_down{false};

/**
 Notify all Collectors that the server is shutting down and to stop work
 immediately. Although the Collector threads will be eventually be terminated by
 component_deinit(), this ensures that Health Monitor will not issue queries or
 make system variable updates during shutdown.

 @param[in] shutdown  New value of the shutdown flag
*/
void Health_Monitor::set_server_shutting_down(bool shutdown) {
  m_server_shutting_down.store(shutdown, std::memory_order_release);
#ifndef NDEBUG
  if (m_server_shutting_down) {
    log_info("%s: Server shutdown detected", health_monitor->name());
  }
#endif
}

/**
  The Health Monitor is avaiable if it is initialized and the server is not
  shutting down.
*/
bool Health_Monitor::available() noexcept {
  return m_hm_initialized.load(std::memory_order_acquire) &&
        !m_server_shutting_down.load(std::memory_order_acquire);
}

/**
  Health monitor deinitialization.
*/
mysql_service_status_t Health_Monitor::component_deinit() {
  log_info("%s: Stopping...", health_monitor->name());
  collectors_stop();
  log_info("%s: Deinitializing task supervisor", health_monitor->name());
  supervisor_deinit();
  log_info("%s: Unregistering system variables", health_monitor->name());
  system_vars_unregister();
  log_info("%s: Unregistering status variables", health_monitor->name());
  status_vars_unregister();
  log_info("%s: Unregistering tables", health_monitor->name());
  tables_unregister();
  log_info("%s: Unregistering metric sources", health_monitor->name());
  metrics_unregister();
  log_info("%s: Deinitalizing server utilities", health_monitor->name());
  server_utils_deinit();
  log_info("%s: Unregistering option usage tracker", health_monitor->name());
  option_usage_tracker_deinit(); // ignore errors
  log_info("%s: Deinitializing collectors", health_monitor->name());
  collectors_deinit();
  log_info("%s: Deinitializing status variable manager", health_monitor->name());
  status_vars_deinit();
  log_info("%s: Deinitializing reporters", health_monitor->name());
  reporters_deinit();
  log_info("%s: Deinitializing table manager", health_monitor->name());
  tables_deinit();
  log_info("Health Monitor: Stopped");
  log_deinit();
  return 0;
}

/**
  Initialize the Health Monitor component.
*/
mysql_service_status_t health_monitor_component_init() {
  health_monitor = std::make_unique<Health_Monitor>();
  return health_monitor->component_init();
}

/**
  Deinitialize the Health Monitor component.
*/
mysql_service_status_t health_monitor_component_deinit() {
  health_monitor->component_deinit();
  health_monitor.reset();
  return 0;
}

REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register, sysvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister, sysvar_unregister_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_system_variable_reader, sysvar_reader_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_system_variable_update_string, sysvar_update_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_global_variable_attributes, sysvar_attributes_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins, log_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(log_builtins_string, log_string_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_charset, charset_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_charset_converter, string_converter_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_table_v1, table_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_bigint_v1, bigint_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_decimal_v1, decimal_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_double_v1, double_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_enum_v1, enum_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_integer_v1, integer_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_string_v2, string_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_timestamp_v2, timestamp_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(registry, registry_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration, registry_reg_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration, statvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_status_variable_string, status_variable_reader_srv);

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result, cmd_query_result_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info, cmd_field_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info, cmd_error_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata, cmd_field_meta_srv);

} // namespace hm

namespace {

IMPLEMENTS_SERVICE_EVENT_TRACKING_LIFECYCLE(health_monitor_component);

BEGIN_COMPONENT_PROVIDES(health_monitor_component)
  PROVIDES_SERVICE_EVENT_TRACKING_LIFECYCLE(health_monitor_component),
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(health_monitor_component)
  REQUIRES_SERVICE_AS(component_sys_variable_register, hm::sysvar_register_srv),
  REQUIRES_SERVICE_AS(component_sys_variable_unregister, hm::sysvar_unregister_srv),
  REQUIRES_SERVICE_AS(mysql_system_variable_reader, hm::sysvar_reader_srv),
  REQUIRES_SERVICE_AS(mysql_system_variable_update_string, hm::sysvar_update_srv),
  REQUIRES_SERVICE_AS(mysql_global_variable_attributes, hm::sysvar_attributes_srv),

  REQUIRES_SERVICE_AS(log_builtins, hm::log_srv),
  REQUIRES_SERVICE_AS(log_builtins_string, hm::log_string_srv),

  REQUIRES_SERVICE_AS(mysql_charset, hm::charset_srv),
  REQUIRES_SERVICE_AS(mysql_string_charset_converter, hm::string_converter_srv),
  REQUIRES_SERVICE_AS(mysql_string_factory, hm::string_factory_srv),

  REQUIRES_SERVICE_AS(pfs_plugin_table_v1, hm::table_srv),
  REQUIRES_SERVICE_AS(pfs_plugin_column_bigint_v1, hm::bigint_srv),
  REQUIRES_SERVICE_AS(pfs_plugin_column_decimal_v1, hm::decimal_srv),
  // NOLINTNEXTLINE
  REQUIRES_SERVICE_AS(pfs_plugin_column_double_v1, hm::double_srv),
  // NOLINTNEXTLINE
  REQUIRES_SERVICE_AS(pfs_plugin_column_enum_v1, hm::enum_srv),
  REQUIRES_SERVICE_AS(pfs_plugin_column_integer_v1, hm::integer_srv),
  REQUIRES_SERVICE_AS(pfs_plugin_column_string_v2, hm::string_srv),
  REQUIRES_SERVICE_AS(pfs_plugin_column_timestamp_v2, hm::timestamp_srv),

  REQUIRES_SERVICE_AS(registry, hm::registry_srv),
  REQUIRES_SERVICE_AS(registry_registration, hm::registry_reg_srv),

  REQUIRES_SERVICE_AS(status_variable_registration, hm::statvar_register_srv),
  REQUIRES_SERVICE_AS(mysql_status_variable_string, hm::status_variable_reader_srv),

  REQUIRES_MYSQL_COND_SERVICE,
  REQUIRES_MYSQL_MUTEX_SERVICE,
  REQUIRES_MYSQL_RWLOCK_SERVICE,
  REQUIRES_PSI_MEMORY_SERVICE,
  REQUIRES_PSI_METRIC_SERVICE,
  REQUIRES_PSI_THREAD_SERVICE,

  REQUIRES_SERVICE_AS(mysql_command_factory, hm::cmd_factory_srv),
  REQUIRES_SERVICE_AS(mysql_command_thread, hm::cmd_thread_srv),
  REQUIRES_SERVICE_AS(mysql_command_options, hm::cmd_options_srv),
  REQUIRES_SERVICE_AS(mysql_command_query, hm::cmd_query_srv),
  REQUIRES_SERVICE_AS(mysql_command_query_result, hm::cmd_query_result_srv),
  REQUIRES_SERVICE_AS(mysql_command_field_info, hm::cmd_field_info_srv),
  REQUIRES_SERVICE_AS(mysql_command_error_info, hm::cmd_error_info_srv),
  REQUIRES_SERVICE_AS(mysql_command_field_metadata, hm::cmd_field_meta_srv),
END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(health_monitor_component)
  METADATA("mysql.author", "Oracle Corporation"),
  METADATA("health_monitor_component", "1"),
END_COMPONENT_METADATA();

DECLARE_COMPONENT(health_monitor_component, "mysql:health_monitor_component")
  hm::health_monitor_component_init,
  hm::health_monitor_component_deinit
END_DECLARE_COMPONENT();

// Components contained in this library, currently one component
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(health_monitor_component)
END_DECLARE_LIBRARY_COMPONENTS
// clang-format on

}  // namespace

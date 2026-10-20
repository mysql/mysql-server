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

#include <new>

#include "mysql/components/util/debug_execute_if.h"

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

#ifndef NDEBUG
REQUIRES_SERVICE_PLACEHOLDER(mysql_debug_keyword_service);
#endif

REQUIRES_MYSQL_COND_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_MUTEX_SERVICE_PLACEHOLDER;
REQUIRES_MYSQL_RWLOCK_SERVICE_PLACEHOLDER;
REQUIRES_PSI_MEMORY_SERVICE_PLACEHOLDER;
REQUIRES_PSI_METRIC_SERVICE_PLACEHOLDER;
REQUIRES_PSI_THREAD_SERVICE_PLACEHOLDER;

namespace hm {

Health_Monitor *Health_Monitor::m_instance{nullptr};

Health_Monitor::Health_Monitor() = default;
Health_Monitor::~Health_Monitor() = default;

/**
  Create the component owner before publishing any of its objects to callbacks.
  On failure, release partially initialized objects while services are
  available.
*/
mysql_service_status_t Health_Monitor::component_init() {
  if (m_instance != nullptr) return 1;

  try {
    m_instance = new Health_Monitor();
    if (m_instance->initialize() == 0) return 0;
  } catch (...) {
    if (log() != nullptr) {
      log_error("Health Monitor: Failed to initialize component objects");
    }
  }
  component_deinit();
  return 1;
}

/**
  Stop workers and unregister callbacks before destroying the component owner.
*/
mysql_service_status_t Health_Monitor::component_deinit() {
  if (m_instance == nullptr) return 0;

  const auto result = m_instance->deinitialize();
  delete m_instance;
  m_instance = nullptr;
  return result;
}

/**
  Initialize the Health Monitor collectors.
*/
int Health_Monitor::collectors_init() {
  if (m_collectors_initialized) {
    return 0;
  }
  m_disk_collector = std::make_unique<Disk_Collector>();
  if (m_disk_collector == nullptr) {
    return 1;
  }
  m_disk_collector->initialize();
  DBUG_EXECUTE_IF("health_monitor_fail_collectors_init", return 1;);
  DBUG_EXECUTE_IF("health_monitor_throw_collectors_init",
                  throw std::bad_alloc(););

  m_disk_forecaster = std::make_unique<Disk_Forecaster>();
  if (m_disk_forecaster == nullptr) {
    return 1;
  }
  m_disk_forecaster->initialize();

  m_memory_collector = std::make_unique<Memory_Collector>();
  if (m_memory_collector == nullptr) {
    return 1;
  }

  m_system_collector = std::make_unique<System_Collector>();
  if (m_system_collector == nullptr) {
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
  m_disk_collector.reset();
  m_disk_forecaster.reset();
  m_memory_collector.reset();
  m_system_collector.reset();
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
    ret_disk = m_disk_collector->start();
  } else {
    m_disk_collector->log_collector_status(true);
  }

  // Disk Forecaster
  const bool diskf_enabled{System_vars::disk_forecast_running().val};
  int ret_diskf = 0;
  if (diskf_enabled) {
    ret_diskf = m_disk_forecaster->start();
  } else {
    m_disk_forecaster->log_collector_status(true);
  }

  // Memory Collector
  const bool memory_enabled{System_vars::memory_running().val};
  int ret_memory = 0;
  if (memory_enabled) {
    ret_memory = m_memory_collector->start();
  } else {
    m_memory_collector->log_collector_status(true);
  }

  // System Collector
  const bool system_enabled{System_vars::system_running().val};
  int ret_system = 0;
  if (system_enabled) {
    ret_system = m_system_collector->start();
  } else {
    m_system_collector->log_collector_status(true);
  }

  // Memory Reporter
  const bool memory_rpt_enabled{System_vars::memory_reporting().val};
  int ret_memory_rpt = 0;
  if (memory_rpt_enabled) {
    ret_memory_rpt = m_memory_reporter->start();
  } else {
    m_memory_reporter->log_collector_status(true);
  }

  // Periodic Reporter
  const bool periodic_rpt_enabled{System_vars::periodic_reporting().val};
  int ret_periodic_rpt = 0;
  if (periodic_rpt_enabled) {
    ret_periodic_rpt = m_periodic_reporter->start();
  } else {
    m_periodic_reporter->log_collector_status(true);
  }

  // Supervisor
  const int ret_supervisor{m_supervisor->start()};
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
  if (m_supervisor) {
    m_supervisor->stop();
  }
  if (m_periodic_reporter) {
    m_periodic_reporter->stop();
  }
  if (m_memory_reporter) {
    m_memory_reporter->stop();
  }
  if (m_disk_collector) {
    m_disk_collector->stop();
  }
  if (m_disk_forecaster) {
    m_disk_forecaster->stop();
  }
  if (m_memory_collector) {
    m_memory_collector->stop();
  }
  if (m_system_collector) {
    m_system_collector->stop();
  }
  m_collectors_started = false;
  return 0;
}

/**
  Initialize the log manager.
*/
int Health_Monitor::log_init() {
  if (m_log == nullptr) {
    m_log = std::make_unique<Log>();
    if (m_log == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the log manager.
*/
int Health_Monitor::log_deinit() {
  if (m_log != nullptr) {
    m_log.reset();
  }
  return 0;
}

/**
  Initialize the report manager.
*/
int Health_Monitor::reporters_init() {
  if (m_memory_reporter == nullptr) {
    m_memory_reporter = std::make_unique<Triggered_Reporter>(
        "Memory Reporter", "REPORTER", "memory_reporter", "health_mem_rep",
        "Health Monitor memory reporter", &System_vars::memory_reporting(),
        &System_vars::memory_report_definition());
    if (m_memory_reporter == nullptr) {
      return 1;
    }
  }
  if (m_periodic_reporter == nullptr) {
    m_periodic_reporter = std::make_unique<Periodic_Reporter>(
        "Periodic Reporter", "REPORTER", "periodic_reporter", "health_per_rep",
        "Health Monitor periodic reporter", &System_vars::periodic_reporting(),
        &System_vars::periodic_report_definition());
    if (m_periodic_reporter == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the reporters.
*/
int Health_Monitor::reporters_deinit() {
  if (m_periodic_reporter != nullptr) {
    m_periodic_reporter.reset();
  }
  if (m_memory_reporter != nullptr) {
    m_memory_reporter.reset();
  }
  return 0;
}

/**
  Initialize the status variable manager and initialize (but do not register)
  all status variables.
*/
int Health_Monitor::status_vars_init() {
  if (m_status_vars == nullptr) {
    m_status_vars = std::make_unique<Status_vars>();
    if (m_status_vars == nullptr) {
      return 1;
    }
  }
  return m_status_vars->initialize_all();
}

/**
  Deinitialize the status variable manager after all status variables
  have been deregistered.
*/
int Health_Monitor::status_vars_deinit() {
  if (m_status_vars != nullptr) {
    m_status_vars.reset();
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
  if (m_status_vars->register_all() != 0) {
    return 1;
  }
  m_status_vars_registered = true;
  return 0;
}

/**
  Deregister all status variables.
*/
int Health_Monitor::status_vars_unregister() {
  if (m_status_vars != nullptr) {
    m_status_vars->unregister_all();
  }
  m_status_vars_registered = false;
  return 0;
}

/**
  Initialize the task supervisor.
*/
int Health_Monitor::supervisor_init() {
  if (m_supervisor == nullptr) {
    m_supervisor = std::make_unique<Supervisor>();
    if (m_supervisor == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the task supervisor.
*/
int Health_Monitor::supervisor_deinit() {
  if (m_supervisor != nullptr) {
    m_supervisor.reset();
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
  if (Health_Monitor::disk_collector()) {
    Health_Monitor::disk_collector()->mark_settings_pending();
    // An active disk thread signals after its current processing cycle.
    if (Health_Monitor::disk_collector()->Thread::running() !=
        System_vars::disk_running().val) {
      signal_supervisor = true;
    }
  }
  if (Health_Monitor::disk_forecaster()) {
    Health_Monitor::disk_forecaster()->mark_settings_pending();
    if (Health_Monitor::disk_forecaster()->Thread::running() !=
        System_vars::disk_forecast_running().val) {
      signal_supervisor = true;
    }
  }
  if (Health_Monitor::memory_reporter()) {
    Health_Monitor::memory_reporter()->mark_settings_pending();
    if (Health_Monitor::memory_reporter()->Thread::running() !=
        System_vars::memory_reporting().val) {
      signal_supervisor = true;
    }
  }
  if (Health_Monitor::periodic_reporter()) {
    Health_Monitor::periodic_reporter()->mark_settings_pending();
    if (Health_Monitor::periodic_reporter()->Thread::running() !=
        System_vars::periodic_reporting().val) {
      signal_supervisor = true;
    }
  }
  if (Health_Monitor::memory_collector()) {
    Health_Monitor::memory_collector()->mark_settings_pending();
    signal_supervisor = true;
  }
  if (Health_Monitor::system_collector()) {
    Health_Monitor::system_collector()->mark_settings_pending();
    signal_supervisor = true;
  }
  if (signal_supervisor && Health_Monitor::supervisor()) {
    Health_Monitor::supervisor()->notify_settings_update();
  }
  return 0;
}

/**
  Initialize the table manager.
*/
int Health_Monitor::tables_init() {
  if (m_tables == nullptr) {
    m_tables = std::make_unique<Tables>();
    if (m_tables == nullptr) {
      return 1;
    }
  }
  return 0;
}

/**
  Deinitialize the table manager after all tables have been deregistered.
*/
int Health_Monitor::tables_deinit() {
  if (m_tables != nullptr) {
    m_tables.reset();
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
  if (m_tables->register_all() != 0) {
    return 1;
  }
  m_tables_registered = true;
  return 0;
}

/**
  Deregister all Health Monitor tables.
*/
int Health_Monitor::tables_unregister() {
  if (m_tables != nullptr) {
    m_tables->unregister_all();
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
    log_info("%s: Failed to register group member status listeners", name());
  } else {
    log_info("%s: Registered group member status listeners", name());
  }
}

/**
  Deinitialize the server utility functions.
*/
void Health_Monitor::server_utils_deinit() {
  mysql_service_status_t const ret =
      Server::unregister_group_member_status_listeners();
  if (ret != 0) {
    log_info("%s: Failed to unregister group member status listeners", name());
  } else {
    log_info("%s: Unregistered group member status listeners", name());
  }
}

/**
  Register option usage tracker.
*/
int Health_Monitor::option_usage_tracker_init() {
  int ret{0};
  if (Option_Usage::usage_register()) {
    log_info("%s: Failed to register with option usage tracker", name());
    ret = 1;
  } else {
    log_info("%s: Registered with option usage tracker", name());
  }
  return ret;
}

/**
  Deregister option usage tracker.
*/
int Health_Monitor::option_usage_tracker_deinit() {
  int ret{0};
  if (Option_Usage::usage_deregister()) {
    log_info("%s: Failed to unregister from option usage tracker", name());
    ret = 1;
  } else {
    log_info("%s: Unregistered from option usage tracker", name());
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
mysql_service_status_t Health_Monitor::initialize() {
  m_hm_initialized.store(false, std::memory_order_release);
  m_server_shutting_down.store(false, std::memory_order_release);

  if (log_init() != 0) {
    goto error;
  }
  log_info("%s: Starting...", name());

  log_info("%s: Initializing task supervisor", name());
  if (supervisor_init() != 0) {
    goto error;
  }

  // System variables must be registered before initializing the collectors
  if (system_vars_init() != 0) {
    goto error;
  }

  log_info("%s: Registering server system variables", name());
  if (system_vars_register() != 0) {
    goto error;
  }

  log_info("%s: Initializing reporters", name());
  if (reporters_init() != 0) {
    goto error;
  }

  log_info("%s: Initializing collectors", name());
  if (collectors_init() != 0) {
    goto error;
  }

  // Status variables must be registered after the collectors are initialized
  if (status_vars_init() != 0) {
    goto error;
  }
  log_info("%s: Registering status variables", name());
  if (status_vars_register() != 0) {
    goto error;
  }

  log_info("%s: Starting collectors", name());
  if (collectors_start() != 0) {
    goto error;
  }

  // Tables can only be registered once a successful component initialization is
  // assured. Tables created during component initialization cannot be deleted,
  // i.e. cleaned up, until after the component is fully installed.
  // See Bug#33973047 "Health Monitor crashes due to full disk".

  log_info("%s: Registering Performance Schema tables", name());

  if (tables_init() != 0) {
    goto error;
  }
  if (tables_register() != 0) {
    goto error;
  }

  log_info("%s: Registering metric sources", name());
  if (metrics_register() != 0) {
    goto error;
  }

  log_info("%s: Initializing server utilities", name());
  server_utils_init();

  log_info("%s: Registering option usage tracker", name());
  if (option_usage_tracker_init() != 0) {
    goto error;
  }

  m_hm_initialized.store(true, std::memory_order_release);
  // Process system variable updates queued during component initialization
  m_supervisor->notify_settings_update();
  log_info("Health Monitor: Started");
  return 0;

error:
  if (m_log) {
    log_error("Health Monitor: Failed to start");
  }
  return 1;
}

// clang-format off

std::atomic<bool> Health_Monitor::m_hm_initialized{false};
std::atomic<bool> Health_Monitor::m_server_shutting_down{false};

/**
 Notify all Collectors that the server is shutting down and to stop work
 immediately. Although the Collector threads will eventually be terminated by
 component_deinit(), this ensures that Health Monitor will not issue queries or
 make system variable updates during shutdown.

 @param[in] shutdown  New value of the shutdown flag
*/
void Health_Monitor::set_server_shutting_down(bool shutdown) {
  m_server_shutting_down.store(shutdown, std::memory_order_release);
#ifndef NDEBUG
  if (m_server_shutting_down) {
    log_info("%s: Server shutdown detected", name());
  }
#endif
}

/**
  The Health Monitor is available if it is initialized and the server is not
  shutting down.
*/
bool Health_Monitor::available() noexcept {
  return m_hm_initialized.load(std::memory_order_acquire) &&
        !m_server_shutting_down.load(std::memory_order_acquire);
}

/**
  Health monitor deinitialization.
*/
mysql_service_status_t Health_Monitor::deinitialize() {
  m_hm_initialized.store(false, std::memory_order_release);
  if (m_log == nullptr) return 0;
  log_info("%s: Stopping...", name());
  collectors_stop();
  log_info("%s: Deinitializing task supervisor", name());
  supervisor_deinit();
  log_info("%s: Unregistering system variables", name());
  system_vars_unregister();
  log_info("%s: Unregistering status variables", name());
  status_vars_unregister();
  log_info("%s: Unregistering tables", name());
  tables_unregister();
  log_info("%s: Unregistering metric sources", name());
  metrics_unregister();
  log_info("%s: Deinitalizing server utilities", name());
  server_utils_deinit();
  log_info("%s: Unregistering option usage tracker", name());
  option_usage_tracker_deinit(); // ignore errors
  log_info("%s: Deinitializing collectors", name());
  collectors_deinit();
  log_info("%s: Deinitializing status variable manager", name());
  status_vars_deinit();
  log_info("%s: Deinitializing reporters", name());
  reporters_deinit();
  log_info("%s: Deinitializing table manager", name());
  tables_deinit();
  log_info("Health Monitor: Stopped");
  log_deinit();
  return 0;
}

/**
  Initialize the Health Monitor component.
*/
mysql_service_status_t health_monitor_component_init() {
  return Health_Monitor::component_init();
}

/**
  Deinitialize the Health Monitor component.
*/
mysql_service_status_t health_monitor_component_deinit() {
  return Health_Monitor::component_deinit();
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
#ifndef NDEBUG
  REQUIRES_SERVICE(mysql_debug_keyword_service),
#endif
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

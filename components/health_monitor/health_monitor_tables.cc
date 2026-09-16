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

#include "health_monitor_tables.h"

#include <functional>

#include "health_monitor.h"
#include "health_monitor_collector.h"
#include "health_monitor_stat_buffer.h"
#include "table_disk_forecast.h"
#include "table_disk_forecast_summary.h"
#include "table_disk_metric.h"
// #include "table_disk_metric_aggregate.h"
#include "table_health_block_device.h"
#include "table_health_process_memory.h"
#include "table_health_system_memory.h"
#include "table_system_cpu_stats.h"
#include "table_system_cpu_stats_history.h"
#include "table_system_disk_stats.h"
#include "table_system_disk_stats_history.h"
#include "table_system_memory_stats.h"
#include "table_system_memory_stats_history.h"
#include "table_system_process_stats.h"
#include "table_system_process_stats_history.h"

using namespace hm;

// clang-format off

/**
  Define and initialize all Health Monitor tables in the Performance Schema.
*/
mysql_service_status_t Tables::register_all() {
  mysql_service_status_t rc{1};
  if (table_srv != nullptr) {
    // Create the table definitions
    m_tables.push_back(table_disk_forecast_init(Health_Monitor::disk_forecaster()->disk_forecast_buffer()));
    m_tables.push_back(table_disk_forecast_summary_init(Health_Monitor::disk_forecaster()->disk_forecast_summary_buffer()));
    m_tables.push_back(table_disk_metric_init(Health_Monitor::disk_forecaster()->disk_metric_buffer()));
    #if 0
    // Deferred until later release
    m_tables.push_back(table_disk_metric_aggregate_init(Health_Monitor::disk_forecaster()->disk_metric_aggregate_buffer()));
    #endif
    m_tables.push_back(table_health_block_device_init(Health_Monitor::disk_collector()->buffer()));
    m_tables.push_back(table_health_process_memory_init(Health_Monitor::memory_collector()->proc_buffer()));
    m_tables.push_back(table_health_system_memory_init(Health_Monitor::memory_collector()->sys_buffer()));
    m_tables.push_back(table_system_cpu_stats_init());
    m_tables.push_back(table_system_cpu_stats_history_init(Health_Monitor::system_collector()->sys_cpu_buffer()));
    m_tables.push_back(table_system_disk_stats_init());
    m_tables.push_back(table_system_disk_stats_history_init(Health_Monitor::system_collector()->sys_disk_buffer()));
    m_tables.push_back(table_system_memory_stats_init());
    m_tables.push_back(table_system_memory_stats_history_init(Health_Monitor::system_collector()->sys_memory_buffer()));
    m_tables.push_back(table_system_process_stats_init());
    m_tables.push_back(table_system_process_stats_history_init(Health_Monitor::system_collector()->sys_process_buffer()));
    // Register the tables in the Performance Schema
    // Assign a shared status buffer to each
    rc = table_srv->add_tables(m_tables.data(), m_tables.size());
  }
  m_initialized = (rc == 0);
  return rc;
}
//clang-format on

/**
  Uninitialize all Health Monitor tables within the Performance Schema.
*/
mysql_service_status_t Tables::unregister_all() {
  mysql_service_status_t rc{1};
  // Delete table definitions in the Performance schema
  if (m_initialized) {
    if (table_srv != nullptr && !m_tables.empty()) {
      rc = table_srv->delete_tables(m_tables.data(), m_tables.size());
    }
  }
  // Release the status buffers for each table
  table_disk_forecast_deinit();
  table_disk_forecast_summary_deinit();
  table_disk_metric_deinit();
  #if 0
  // Deferred until later release
  table_disk_metric_aggregate_deinit();
  #endif
  table_health_block_device_deinit();
  table_health_process_memory_deinit();
  table_health_system_memory_deinit();
  table_system_cpu_stats_history_deinit();
  table_system_disk_stats_history_deinit();
  table_system_memory_stats_history_deinit();
  table_system_process_stats_history_deinit();
  m_initialized = false;
  return rc;
}

//}  // namespace hm

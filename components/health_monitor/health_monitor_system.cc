/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_system.h"

#include <sstream>

#include <mysql/components/library_mysys/system_info.h>

#include "health_monitor.h"
#include "health_monitor_cpu_mapping.h"
#include "health_monitor_log.h"
#include "health_monitor_memory_mapping.h"
#include "health_monitor_process_stats_mapping.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_storage_mapping.h"
#include "health_monitor_system_vars.h"

namespace hm {
using namespace ss;

System_Collector::System_Collector()
    : Collector("System Collector", "System", "system_collector", "health_sys",
                "Health Monitor system info collector") {
  mysql_rwlock_register("health", &m_rwlock_info, 1);

  // clang-format off

  PSI_memory_info all_memory_info[] = {
      {&m_memory_key_cpu_stats, "health_monitor::system_cpu_stats", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor system CPU stats data buffer"},
      {&m_memory_key_cpu_stats_history, "health_monitor::system_cpu_stats_history", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor system CPU stats history data buffer"},
      {&m_memory_key_disk_stats, "health_monitor::system_disk_stats", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor disk stats data buffer"},
      {&m_memory_key_disk_stats_history, "health_monitor::system_disk_stats_history", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor disk stats history data buffer"},
      {&m_memory_key_memory_stats, "health_monitor::system_memory_stats", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor system memory stats data buffer"},
      {&m_memory_key_memory_stats_history, "health_monitor::system_memory_stats_history", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor system memory stats history data buffer"},
      {&m_memory_key_process_stats, "health_monitor::process_stats", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor process stats data buffer"},
      {&m_memory_key_process_stats_history, "health_monitor::process_stats_history", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor process stats history data buffer"}};

  PSI_MEMORY_CALL(register_memory)("health", all_memory_info, 8);

  // Initialize the stat buffers with the registered instrumentation keys
  m_sys_cpu_buffer = std::make_shared<Stat_Buffer<System_CPU_Stats>>(m_rwlock_key, m_memory_key_cpu_stats_history);
  m_sys_disk_buffer = std::make_shared<Stat_Buffer<System_Disk_Stats>>(m_rwlock_key, m_memory_key_disk_stats_history);
  m_sys_memory_buffer = std::make_shared<Stat_Buffer<System_Memory_Stats>>(m_rwlock_key, m_memory_key_memory_stats_history);
  m_sys_process_buffer = std::make_shared<Stat_Buffer<System_Process_Stats>>(m_rwlock_key, m_memory_key_process_stats_history);

  // Set the local parameters then size the stat buffers accordingly
  set_params();
  set_buffer_size();

  // Initialize system stats collector state
  System_State::set_collector(this);
  set_start_state();
}

/**
  Tasks before starting the collection loop.
*/
void System_Collector::pre_process() {
  set_start_state();
  log_collector_status(true);  // show settings
}

/**
  Collect the current system stats and write to the stat buffer.
*/
void System_Collector::process() {
  if (!Health_Monitor::available()) return;

  const time_t timestamp = time(nullptr);

  if (!m_rows_per_sample_set) {
    if (set_rows_per_sample()) {
      set_buffer_size();
    }
    m_rows_per_sample_set = true;
  }

  // System CPU stats history
  if (m_cpu_stats_history) {
    std::vector<System_CPU_Stats> sys_cpu_stats;
    m_sysinfo_error = (get_cpu_stats(sys_cpu_stats, timestamp) != 0);
    if (!m_sysinfo_error && !sys_cpu_stats.empty()) {
      for (const System_CPU_Stats &cpu_stats : sys_cpu_stats) {
        m_sys_cpu_buffer->write(cpu_stats);
      }
    }
  }

  // System disk stats history
  if (m_disk_stats_history) {
    std::vector<System_Disk_Stats> sys_disk_stats;
    m_sysinfo_error = (get_disk_stats(sys_disk_stats, timestamp) != 0);
    if (!m_sysinfo_error && !sys_disk_stats.empty()) {
      for (const System_Disk_Stats &disk_stats : sys_disk_stats) {
        m_sys_disk_buffer->write(disk_stats);
      }
    }
  }

  // System memory stats history
  if (m_memory_stats_history) {
    System_Memory_Stats sys_memory_stats;
    m_sysinfo_error = (get_memory_stats(sys_memory_stats, timestamp) != 0);
    if (!m_sysinfo_error && sys_memory_stats.m_total_bytes > 0) {
      m_sys_memory_buffer->write(sys_memory_stats);
    }
  }

  // Process stats history
  if (m_process_stats_history) {
    std::vector<System_Process_Stats> sys_process_stats;
    m_sysinfo_error = (get_process_stats(sys_process_stats, timestamp) != 0);
    // Write only first row, which is an aggregate of stats for the process
    if (!m_sysinfo_error && !sys_process_stats.empty()) {
      m_sys_process_buffer->write(sys_process_stats.front());
    }
  }

  // Evaluate System_Collector state
  m_current_state->check_levels();
}

/**
  Final tasks before exiting the thread.
*/
void System_Collector::post_process() {
  set_stop_state();
  log_collector_status(false);  // no settings
}

/**
  Resize the stat buffers if necessary. Account for the expected number of rows
  per sample. Must be inside collector lock.
*/
void System_Collector::set_buffer_size() {
  set_cpu_buffer_size();
  set_disk_buffer_size();
  set_memory_buffer_size();
  set_process_buffer_size();
}

void System_Collector::set_cpu_buffer_size() {
  uint32_t new_size = m_retention / sample_interval();
  if (new_size == 0) new_size = 1;
  new_size *= m_cpu_rows_per_sample;

  if (new_size != m_sys_cpu_buffer->max_size()) {
    m_sys_cpu_buffer->resize(new_size);
  }
}

void System_Collector::set_disk_buffer_size() {
  uint32_t new_size = m_retention / sample_interval();
  if (new_size == 0) new_size = 1;
  new_size *= m_disk_rows_per_sample;

  if (new_size != m_sys_disk_buffer->max_size()) {
    m_sys_disk_buffer->resize(new_size);
  }
}

void System_Collector::set_memory_buffer_size() {
  uint32_t new_size = m_retention / sample_interval();
  if (new_size == 0) new_size = 1;
  new_size *= m_memory_rows_per_sample;

  if (new_size != m_sys_memory_buffer->max_size()) {
    m_sys_memory_buffer->resize(new_size);
  }
}

void System_Collector::set_process_buffer_size() {
  uint32_t new_size = m_retention / sample_interval();
  if (new_size == 0) new_size = 1;
  new_size *= m_process_rows_per_sample;

  if (new_size != m_sys_process_buffer->max_size()) {
    m_sys_process_buffer->resize(new_size);
  }
}

/**
  Set the number of rows per sample for each statistic type.
  The number of rows for cpu and disk stats should remain effectively constant.
  @return true if any rows per sample changed
*/
bool System_Collector::set_rows_per_sample() {
  // Take initial readings to establish the number of rows per sample
  int changed{0};
  std::vector<System_CPU_Stats> cpu_stats;
  if (get_cpu_stats(cpu_stats) == 0 && !cpu_stats.empty()) {
    if (cpu_stats.size() > m_cpu_rows_per_sample) {
      m_cpu_rows_per_sample = cpu_stats.size();
      changed++;
    }
  }
  std::vector<System_Disk_Stats> disk_stats;
  if (get_disk_stats(disk_stats) == 0 && !disk_stats.empty()) {
    if (disk_stats.size() > m_disk_rows_per_sample) {
      m_disk_rows_per_sample = disk_stats.size();
      changed++;
    }
  }
  return (changed > 0);
}

/**
  Return true if the collector settings have changed.
*/
bool System_Collector::settings_changed() {
  m_settings_changed =
      (System_vars::status_interval().val != m_status_interval ||
       System_vars::system_running().val != m_running ||
       System_vars::system_sample_interval().val != sample_interval() ||
       System_vars::system_retention().val != m_retention ||
       System_vars::system_cpu_stats_history().val != m_cpu_stats_history ||
       System_vars::system_disk_stats_history().val != m_disk_stats_history ||
       System_vars::system_memory_stats_history().val != m_memory_stats_history ||
       System_vars::system_process_stats_history().val != m_process_stats_history);
  return m_settings_changed;
}

/**
  Set collection parameters from the system variables.
*/
void System_Collector::set_params() {
  m_running = System_vars::system_running().val;
  set_sample_interval(System_vars::system_sample_interval().val);
  m_retention = System_vars::system_retention().val;
  m_status_interval = System_vars::status_interval().val;
  m_cpu_stats_history = System_vars::system_cpu_stats_history().val;
  m_disk_stats_history = System_vars::system_disk_stats_history().val;
  m_memory_stats_history = System_vars::system_memory_stats_history().val;
  m_process_stats_history = System_vars::system_process_stats_history().val;
  set_buffer_size();
}

/**
  Return health_monitor.system_running.
*/
bool System_Collector::running() const {
  return System_vars::system_running().val;
}

/**
  Get the system CPU stats.
  @param[in,out] sys_cpu_stats  Stats for each CPU
  @return 0 success, 1 failure
*/
int System_Collector::get_cpu_stats(
    std::vector<System_CPU_Stats> &sys_cpu_stats) {
  const time_t timestamp = time(nullptr);
  return get_cpu_stats(sys_cpu_stats, timestamp);
}

/**
  Get the system CPU stats.
  @param[out] sys_cpu_stats  Stats for each CPU
  @param[in] timestamp  Timestamp to use for the record
  @return 0 success, 1 failure
*/
int System_Collector::get_cpu_stats(
    std::vector<System_CPU_Stats> &sys_cpu_stats, const time_t &timestamp) {
  const auto snapshot = mysql::system_info::query_host_cpu();
  return convert_host_cpu_snapshot(snapshot, timestamp, sys_cpu_stats) ? 0 : 1;
}

/**
  Get the system CPU statistics and return them in a local stat buffer.
  @return stat_buffer with one or more rows of stats for each CPU
*/
std::shared_ptr<Stat_Buffer<System_CPU_Stats>>
System_Collector::get_cpu_stats_buffer() {
  // Create a buffer to return
  std::shared_ptr<Stat_Buffer<System_CPU_Stats>> stats_buffer =
      std::make_shared<Stat_Buffer<System_CPU_Stats>>(true, m_memory_key_cpu_stats);
  if (!m_running) return stats_buffer;

  // Get the stats for each CPU
  std::vector<System_CPU_Stats> cpu_stats;
  m_sysinfo_error = get_cpu_stats(cpu_stats) != 0;

  // Write to the stats buffer
  if (!m_sysinfo_error && !cpu_stats.empty()) {
    stats_buffer->resize(cpu_stats.size());
    for (System_CPU_Stats const &stats : cpu_stats) {
      stats_buffer->write(stats);
    }
  }
  return stats_buffer;
}

/**
  Get storage device stats.
  @param[in,out] sys_disk_stats Stats for each available storage device
  @return 0 success, 1 failure
*/
int System_Collector::get_disk_stats(
    std::vector<System_Disk_Stats> &sys_disk_stats) {
  const time_t timestamp = time(nullptr);
  return get_disk_stats(sys_disk_stats, timestamp);
}

/**
  Get storage statistics for all available devices.
  @param[out] sys_disk_stats  Stats for each available storage device
  @param[in]  timestamp       Timestamp to use for the record
  @return 0 success or unavailable, 1 failure
*/
int System_Collector::get_disk_stats(
    std::vector<System_Disk_Stats> &sys_disk_stats, const time_t &timestamp) {
  const auto snapshot = mysql::system_info::query_storage_devices();
  const auto result =
      convert_storage_snapshot(snapshot, timestamp, sys_disk_stats);
  return result == mysql::system_info::Result_state::failed ? 1 : 0;
}

/**
  Get the system disk statistics and return them in a local stat buffer.
  @return stat_buffer with one or more rows of stats for each storage device
*/
std::shared_ptr<Stat_Buffer<System_Disk_Stats>>
System_Collector::get_disk_stats_buffer() {
  // Create a buffer to return
  std::shared_ptr<Stat_Buffer<System_Disk_Stats>> stats_buffer =
      std::make_shared<Stat_Buffer<System_Disk_Stats>>(true, m_memory_key_disk_stats);
  if (!m_running) return stats_buffer;

  // Get the stats for each block device
  std::vector<System_Disk_Stats> diskstats;
  m_sysinfo_error = (get_disk_stats(diskstats) != 0);

  // Write to the disk stats buffer
  if (!m_sysinfo_error) {
    stats_buffer->resize(diskstats.size());
    for (System_Disk_Stats const &stats : diskstats) {
      stats_buffer->write(stats);
    }
  }
  return stats_buffer;
}

/**
  Get the system memory stats.
  @param[in,out] mem_stats System memory info
  @return 0 success, 1 failure
*/
int System_Collector::get_memory_stats(System_Memory_Stats &mem_stats) {
  const time_t timestamp = time(nullptr);
  return get_memory_stats(mem_stats, timestamp);
}

/**
  Get the system memory stats.
  @param[out] mem_stats  System memory stats
  @param[in]  timestamp  Timestamp to use for the record
  @return 0 success, 1 failure
*/
int System_Collector::get_memory_stats(System_Memory_Stats &mem_stats,
                                       const time_t &timestamp) {
  const auto snapshot = mysql::system_info::query_host_memory();
  return convert_host_memory_snapshot(snapshot, timestamp, mem_stats) ? 0 : 1;
}

/**
  Get the system memory statistics and return them in a local stat buffer.
  @return stat_buffer with one record of memory stats
*/
std::shared_ptr<Stat_Buffer<System_Memory_Stats>>
System_Collector::get_memory_stats_buffer() {
  // Create and resize a buffer to return
  std::shared_ptr<Stat_Buffer<System_Memory_Stats>> stats_buffer =
      std::make_shared<Stat_Buffer<System_Memory_Stats>>(true, m_memory_key_memory_stats);
  if (!m_running) return stats_buffer;

  // Get the system memory stats
  System_Memory_Stats memory_stats;
  m_sysinfo_error = (get_memory_stats(memory_stats) != 0);

  // Write to the stats buffer
  if (!m_sysinfo_error) {
    stats_buffer->resize(1);
    stats_buffer->write(memory_stats);
  }
  return stats_buffer;
}

/**
  Get the mysqld process stats.
  @param[out] sys_process_stats  Array of stats for each thread
  @return 0 success, 1 failure
*/
int System_Collector::get_process_stats(
    std::vector<System_Process_Stats> &sys_process_stats) {
  const time_t timestamp = time(nullptr);
  return get_process_stats(sys_process_stats, timestamp);
}

/**
  Get the mysqld process stats.
  @param[out] sys_process_stats  Array of stats for each thread
  @param[in] timestamp  Timestamp to use for the record
  @return 0 success, 1 failure
*/
int System_Collector::get_process_stats(
    std::vector<System_Process_Stats> &sys_process_stats,
    const time_t &timestamp) {
  const auto snapshot = mysql::system_info::query_process_threads();
  const auto conversion =
      convert_process_threads_snapshot(snapshot, timestamp, sys_process_stats);
  const bool log_error =
      update_process_threads_error_state(conversion, m_process_stats_error);
  if (conversion == Process_threads_conversion_result::failure) {
    if (log_error)
      log_warning("%s: query_process_threads() failed", name());
    return 1;
  }

  if (conversion == Process_threads_conversion_result::degraded) {
    if (log_error) {
      log_warning("%s: query_process_threads() optional capability failed",
                  name());
    }
  }
  return 0;
}

/**
  Get the mysqld process statistics and return them in a local stat buffer.
  @return stat_buffer with one or more rows of process stats
*/
std::shared_ptr<Stat_Buffer<System_Process_Stats>>
System_Collector::get_process_stats_buffer() {
  // Create a stats buffer to return
  std::shared_ptr<Stat_Buffer<System_Process_Stats>> stats_buffer =
      std::make_shared<Stat_Buffer<System_Process_Stats>>(true, m_memory_key_process_stats);
  if (!m_running) return stats_buffer;

  // Get stats for each thread in the process
  std::vector<System_Process_Stats> process_stats;
  m_sysinfo_error = (get_process_stats(process_stats) != 0);

  // Write to the stats buffer
  if (!m_sysinfo_error && !process_stats.empty()) {
    stats_buffer->resize(process_stats.size());
    for (System_Process_Stats const &stats : process_stats) {
      stats_buffer->write(stats);
    }
  }
  return stats_buffer;
}

/**
  System Collector settings formatted string.
*/
std::string System_Collector::format_settings() {
  std::stringstream profile;
  profile << "running" << eq << on_off(m_running);
  profile << ", sample interval" << eq << sample_interval() << "s";
  profile << ", retention" << eq << m_retention << "s";
  profile << ", status interval" << eq << m_status_interval;
  return profile.str();
}

/**
  System Collector status formatted string.
*/
std::string System_Collector::format_status_info() {
  std::stringstream profile;
  profile << "status info";
  return profile.str();
}

/**
  Collector state tag.
*/
std::string System_Collector::tag_collector_state() {
  if (m_current_state == nullptr) return "";
  switch (m_current_state->state()) {
    case System_State::START:
      return k_started;
      break;
    case System_State::STOP:
      return k_stopped;
      break;
    default:
      return "";
      break;
  }
}

/**
  Error condition warning.
*/
void System_Collector::log_warning_error_condition() {
  log_warning("%s",
              build_collector_message(k_error, k_system_error, "", true)
                  .c_str());
}

/**
  System status, depending on state.
*/
void System_Collector::log_resource_status() {
  if (m_current_state == nullptr) return;
  switch (m_current_state->state()) {
    case System_State::State::START:
    case System_State::State::STOP:
      log_collector_status(true);
      break;
    case System_State::State::NORMAL:
      if (m_settings_changed) {
        log_resource_levels();
        m_settings_changed = false;
      }
      break;
    case System_State::State::WARNING:
      m_settings_changed = false;
      break;
    case System_State::State::ERROR:
      log_warning_error_condition();
      break;
    default:
      log_resource_levels();
      break;
  }
}

/**
  Set the next state.
*/
void System_Collector::set_state(System_State &new_state) {
  if (m_current_state != nullptr) {
    m_current_state->exit();
  }
  m_current_state = &new_state;
  m_current_state->enter();
}

// clang-format on

}  // namespace hm

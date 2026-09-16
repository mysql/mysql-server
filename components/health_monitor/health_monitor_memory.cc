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

#include "health_monitor_memory.h"

#include <iomanip>  // setprecision()
#include <sstream>

#include "health_monitor.h"
#include "health_monitor_log.h"
#include "health_monitor_memory_mapping.h"
#include "health_monitor_process_memory_mapping.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_system_vars.h"
#include "health_monitor_triggered_reporter.h"
#include "health_monitor_util.h"

namespace hm {
using namespace ms;

Memory_Collector::Memory_Collector()
    : Collector("Memory Collector", "Memory", "memory_collector", "health_mem",
                "Health Monitor memory collector") {
  mysql_rwlock_register("health", &m_rwlock_info, 1);
  PSI_MEMORY_CALL(register_memory)("health", &m_memory_info, 1);
  // Initialize the stat buffers with the registered instrumentation keys
  m_proc_buffer = std::make_shared<Stat_Buffer<Process_Memory_Info>>(
      m_rwlock_key, m_memory_key);
  m_sys_buffer = std::make_shared<Stat_Buffer<System_Memory_Info>>(
      m_rwlock_key, m_memory_key);
  // Set the local parameters then size the stat buffer accordingly
  set_params();
  set_buffer_size();
  // Initialize memory collector states
  Memory_State::set_collector(this);
  set_start_state();
}

/**
  Tasks before starting the collection loop.
*/
void Memory_Collector::pre_process() {
  set_start_state();
  log_collector_status(true);  // show settings
}

/**
  Collect the current memory stats and write to the stat buffer.
*/
void Memory_Collector::process() {
  if (!Health_Monitor::available()) return;

  // Get the current system and process memory info
  m_sysinfo_error = (get_memory_info(m_info) != 0);

  if (!m_sysinfo_error) {
    const time_t timestamp = time(nullptr);
    publish_memory_records(
        m_info, timestamp,
        // Write to 'health_process_memory'
        [this](const Process_Memory_Info &record) {
          m_proc_buffer->write(record);
        },
        // Write to 'health_system_memory'
        [this](const System_Memory_Info &record) {
          m_sys_buffer->write(record);
        });
    // Update thresholds if the settings have changed
    if (m_refresh_memory_profile) {
      set_memory_profile(m_info);
    }
    update_per_sample_totals(m_info);
  }
  // Check the available memory against the memory thresholds
  check_memory_levels();
}

/**
  Final tasks before exiting the thread.
*/
void Memory_Collector::post_process() {
  set_stop_state();
  log_collector_status(false);  // no settings
}

/**
  Resize the stat buffer if necessary. Must be inside collector lock.
*/
void Memory_Collector::set_buffer_size() {
  unsigned int new_size = m_retention / sample_interval();
  if (new_size == 0) new_size = 1;
  // proc and sys buffers are the same size
  if (new_size != m_proc_buffer->max_size()) {
    m_proc_buffer->resize(new_size);
    m_sys_buffer->resize(new_size);
  }
}

/**
  Indicate if the Memory Collector settings have changed.
  @return true if settings have changed
*/
bool Memory_Collector::settings_changed() {
  m_settings_changed =
      (System_vars::status_interval().val != m_status_interval ||
       System_vars::memory_running().val != m_running ||
       System_vars::memory_sample_interval().val != sample_interval() ||
       System_vars::memory_retention().val != m_retention ||
       System_vars::memory_warn_level_1().val != m_warn_level_1_mb ||
       System_vars::memory_warn_level_2().val != m_warn_level_2_mb ||
       System_vars::memory_warn_level_3().val != m_warn_level_3_mb);
  return m_settings_changed;
}

/**
  Set collection parameters from the system variables.
*/
void Memory_Collector::set_params() {
  m_running = System_vars::memory_running().val;
  set_sample_interval(System_vars::memory_sample_interval().val);
  m_retention = System_vars::memory_retention().val;
  m_status_interval = System_vars::status_interval().val;
  m_warn_level_1_mb = System_vars::memory_warn_level_1().val;
  m_warn_level_2_mb = System_vars::memory_warn_level_2().val;
  m_warn_level_3_mb = System_vars::memory_warn_level_3().val;
  set_buffer_size();
  // Refresh memory thresholds on next collection
  m_refresh_memory_profile = true;
}

/**
  Return health_monitor.memory_running
*/
bool Memory_Collector::running() const {
  return System_vars::memory_running().val;
}

/**
  Get the current system and process memory info
  @param[out] memory_info  System and process memory info
  @return 0 success, 1 failure
*/
int Memory_Collector::get_memory_info(Memory_Info &memory_info) {
  Memory_Info converted;
  if (get_system_memory_info(converted.sys) != 0) return 1;

  bool process_memory_unavailable{false};
  if (get_process_memory_info(converted.proc, process_memory_unavailable) !=
          0 &&
      !process_memory_unavailable) {
    return 1;
  }
  converted.m_process_memory_available = !process_memory_unavailable;

  memory_info = std::move(converted);
  return 0;
}

/**
  Get the physical memory used by the current process.
  @param[out] process_info  Process memory info
  @param[out] unavailable   Whether the process memory API is unavailable
  @return 0 success, 1 failure
*/
int Memory_Collector::get_process_memory_info(Process_Memory_Info &process_info,
                                              bool &unavailable) {
  unavailable = false;
  const auto snapshot = mysql::system_info::query_process_memory();
  if (convert_process_memory_snapshot(snapshot, process_info)) return 0;

  unavailable = process_memory_snapshot_is_unavailable(snapshot);
  return 1;
}

/**
  Get memory stats for the system.
  @param[out] sys_memory_info  System memory info
  @return 0 success, 1 failure
*/
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
int Memory_Collector::get_system_memory_info(
    System_Memory_Info &sys_memory_info) {
  const auto snapshot = mysql::system_info::query_host_memory();
  return convert_host_memory_snapshot(snapshot, sys_memory_info) ? 0 : 1;
}

/**
  Update totals used for status and warning messages, convert to MiB.
  @param[in] memory_info  System memory info
*/
void Memory_Collector::update_per_sample_totals(
    const Memory_Info &memory_info) {
  if (memory_info.sys.m_total_bytes == 0) {
    return;
  }
  m_avail_mb = static_cast<uint64_t>(memory_info.sys.m_avail_bytes / megabyte);
  if (memory_info.m_process_memory_available) {
    m_process_mb = memory_info.proc.m_vmrss_bytes / megabyte;
  }

  if (memory_info.sys.m_total_bytes > 0) {
    double used_pct = static_cast<double>(memory_info.sys.m_total_bytes -
                                          memory_info.sys.m_avail_bytes) /
                      memory_info.sys.m_total_bytes;
    used_pct *= 100.0;  // using double literal for precision
    m_used_pct = static_cast<float>(used_pct);
  } else {
    m_used_pct = 0.0F;
  }
  // Detect significant changes in available memory
  if (m_last_avail_mb == 0) {
    m_last_avail_mb = m_avail_mb;
  } else if (m_last_avail_mb > 0) {
    const double diff_pct = utl::change_percent(m_last_avail_mb, m_avail_mb);
    if (diff_pct >= m_delta_pct) {
      m_force_status = true;
      m_last_avail_mb = m_avail_mb;
    }
  }
}

/**
  Update the memory warning thresholds, convert to MiB.
  @param[in] memory_info  System memory info
*/
void Memory_Collector::set_memory_profile(const Memory_Info &memory_info) {
  m_total_mb = memory_info.sys.m_total_bytes / megabyte;
  m_max_warn_level_mb = std::max(m_warn_level_1_mb, m_warn_level_2_mb);
  m_max_warn_level_mb = std::max(m_max_warn_level_mb, m_warn_level_3_mb);
  m_last_warn_level_mb = no_warn_level;
  // Refresh the threshold parameters in the memory states
  set_state_params();
  // Reset the warning state after updating thresholds
  set_normal_state();
  m_refresh_memory_profile = false;
  m_force_status = true;
}

/**
  Memory Collector settings formatted string.
*/
std::string Memory_Collector::format_settings() {
  std::stringstream profile;
  profile << "running" << eq << on_off(m_running);
  profile << ", warning levels" << eq << m_warn_level_1_mb;
  profile << "/" << m_warn_level_2_mb << "/" << m_warn_level_3_mb << " MiB";
  profile << ", sample interval" << eq << sample_interval() << "s";
  profile << ", retention" << eq << m_retention << "s";
  profile << ", max samples" << eq << m_proc_buffer->max_size();
  profile << ", status interval" << eq << m_status_interval;
  return profile.str();
}

/**
  Memory info formatted string.
*/
std::string Memory_Collector::format_status_info() {
  std::stringstream profile;
  profile << "avail" << eq << format_mb(m_avail_mb, -1, true);
  profile << ", total" << eq << format_mb(m_total_mb, -1, true);
  profile << std::setprecision(1) << std::fixed;
  profile << ", used" << eq << m_used_pct << "%%";
  if (m_info.m_process_memory_available) {
    profile << ", mysqld" << eq << format_mb(m_process_mb, -1, true);
  }
  profile << ", warnings" << eq << format_mb(m_warn_level_1_mb, -1, true);
  profile << "/" << format_mb(m_warn_level_2_mb, -1, true);
  profile << "/" << format_mb(m_warn_level_3_mb, -1, true);
  return profile.str();
}

/**
  Collector state tag.
*/
std::string Memory_Collector::tag_collector_state() {
  if (m_current_state == nullptr) return "";
  switch (m_current_state->state()) {
    case Memory_State::START:
      return k_started;
      break;
    case Memory_State::STOP:
      return k_stopped;
      break;
    default:
      return "";
      break;
  }
}

/**
  Low memory warning level 1/2/3.
*/
void Memory_Collector::log_warning_level(int) {
  int level{0};
  if (m_last_warn_level_mb == m_warn_level_1_mb) {
    level = 1;
  } else if (m_last_warn_level_mb == m_warn_level_2_mb) {
    level = 2;
  } else if (m_last_warn_level_mb == m_warn_level_3_mb) {
    level = 3;
  }
  std::string state = k_warning_level + " " + std::to_string(level);
  state += " (" + format_mb(m_last_warn_level_mb, -1, true) + ")";
  log_warning("%s",
              build_resource_message(state, tag_mode(), "", true).c_str());
  // Emit memory
  Health_Monitor::memory_reporter()->signal();
}

/**
  Error condition warning.
*/
void Memory_Collector::log_warning_error_condition() {
  log_warning(
      "%s", build_collector_message(k_error, k_system_error, "", true).c_str());
}

/**
  Memory status, depending on state.
*/
void Memory_Collector::log_resource_status() {
  if (m_current_state == nullptr) return;
  switch (m_current_state->state()) {
    case Memory_State::State::START:
    case Memory_State::State::STOP:
      log_collector_status(true);
      break;
    case Memory_State::State::NORMAL:
      if (status_message_timer().expired() || m_settings_changed ||
          m_force_status) {
        log_resource_levels();
        m_settings_changed = false;
        m_force_status = false;
      }
      break;
    case Memory_State::State::WARNING:
      if (status_message_timer().expired() || m_settings_changed ||
          m_force_status) {
        log_warning_level();
        m_settings_changed = false;
        m_force_status = false;
      }
      break;
    case Memory_State::State::ERROR:
      log_warning_error_condition();
      break;
    default:
      log_resource_levels();
      break;
  }
}

/**
  Set threshold params for all memory states.
*/
void Memory_Collector::set_state_params() { Memory_State::get_params(); }

/**
  Set the next state based on the available memory.
*/
void Memory_Collector::set_state(Memory_State &new_state) {
  if (m_current_state != nullptr) {
    m_current_state->exit();
  }
  m_current_state = &new_state;
  m_current_state->enter();
}

/**
  Check the available memory against the memory thresholds.
  If available space < any warning level, issue warning message.
  Otherwise issue a status message.
*/
void Memory_Collector::check_memory_levels() const {
  Memory_State::get_levels();
  m_current_state->check_levels();
}

}  // namespace hm

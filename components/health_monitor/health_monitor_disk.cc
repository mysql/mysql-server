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

#include "health_monitor_disk.h"

#include <iomanip>  // setprecision
#include <iostream>

#include "health_monitor.h"
#include "health_monitor_log.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_status_vars.h"
#include "health_monitor_supervisor.h"
#include "health_monitor_system_vars.h"
#include "health_monitor_util.h"  // TODO temporary

namespace hm {
using namespace ds;

// clang-format off

/**
  Disk Collector constructor. Initialize the stat buffer and instrumentation.
*/
Disk_Collector::Disk_Collector()
    : Collector("Disk Collector", "Disk", "disk_collector", "health_disk",
                "Health Monitor disk collector") {
  mysql_rwlock_register("health", &m_rwlock_info, 1);
  PSI_MEMORY_CALL(register_memory)("health", &m_memory_info, 1);
  // Initialize the stat buffer with the registered instrumentation keys
  m_buffer = std::make_shared<Stat_Buffer<Disk_Info>>(m_rwlock_key, m_memory_key);
  //m_buffer = std::make_shared<Disk_Stats_Buffer>(m_rwlock_key, m_memory_key);
}

/**
  Initialize the Disk Collector.
  Performs initialization after construction.
  Call this immediately after creating the collector.
*/
void Disk_Collector::initialize() {
  // Set the local parameters then size the stat buffer accordingly
  set_params();
  set_buffer_size();
  // Initialize fallback and recovery timers
  initialize_timers();
  // Initialize disk collector states
  Storage_State::set_collector(this);
  set_start_state();
}

/**
  Initialize the fallback and recovery timers.
*/
void Disk_Collector::initialize_timers() {
  // Fallback timer is the countdown to fencing the server
  fallback_timer().set_name(k_fallback_timer);
  fallback_timer().set_interval(base_interval()); // Collector base interval
  fallback_timer().set_report_interval(k_fallback_report_interval);
  fallback_timer().set_running_msg("until " + k_fallback_mode);
  // Recovery timer 1 is the countdown to setting OFFLINE_MODE = OFF
  recovery_timer_1().set_name(k_recovery_timer_1);
  recovery_timer_1().set_interval(base_interval());
  recovery_timer_1().set_report_interval(k_recovery_report_interval);
  recovery_timer_1().set_running_msg("until " + k_recovery_1_mode + " = OFF");
  // Recovery timer 2 is the countdown to setting SUPER_READ_ONLY = OFF
  recovery_timer_2().set_name(k_recovery_timer_2);
  recovery_timer_2().set_interval(base_interval());
  recovery_timer_2().set_report_interval(k_recovery_report_interval);
  recovery_timer_2().set_running_msg("until " + k_recovery_2_mode + " = OFF");
}

/**
  Advance the timers driven by the collection interval.
*/
void Disk_Collector::advance_timers() {
  // Timers driven by the collection cycle must be advanced every interval.
  // Advancing all timers at once rather than at various points in the
  // processing stack ensures consistent timing.
  if (fallback_timer().running()) {
    fallback_timer().tick();
  }
  if (recovery_timer_1().running()) {
    recovery_timer_1().tick();
  }
  if (recovery_timer_2().running()) {
    recovery_timer_2().tick();
  }
}

/**
  Put the collector into the START state.
*/
void Disk_Collector::pre_process() {
  set_start_state();
  log_collector_status(true);  // show settings
}

/**
  Query and evaluate filesystem usage at the base interval. Publish a
  Performance Schema record only when the sample interval expires and publish
  a disk metric record when its metric buffer is writable. Check the available
  space against the storage limits and take appropriate action if necessary.
*/
void Disk_Collector::process() {
  if (!Health_Monitor::available()) {
    return;
  }
  // Get the storage levels
  m_disk_error = !resolve_monitored_path() ||
                 (get_disk_info(m_monitored, m_info,
                                m_filesystem_sample_state,
                                m_filesystem_change) != 0);
  if (m_disk_forecaster != nullptr) {
    m_disk_forecaster->set_disk_input_available(!m_disk_error);
  }
  // Publish records for this sample when each destination is ready
  if (!m_disk_error) {
    m_new_volume = (m_filesystem_change == Filesystem_change::initial ||
                    m_filesystem_change == Filesystem_change::identity_changed);
    m_new_size = (m_filesystem_change == Filesystem_change::capacity_changed);
    if (m_new_volume || m_new_size) m_refresh_storage_profile = true;

    // Sample timer expired, write to table buffer
    const bool write_stat = sample_timer().expired();
    // Update disk metrics if available
    const bool write_metric = m_disk_metric_buffer &&
                              m_disk_metric_buffer->writeable();
    publish_filesystem_records(
        m_info, time(nullptr), write_stat, write_metric, m_filesystem_change,
        [this]() {
          if (m_disk_forecaster != nullptr) {
            m_disk_forecaster->reset_filesystem_series();
          }
        },
        [this](const Disk_Info &record) { m_buffer->write(record); },
        [this](const Disk_Info &record) {m_disk_metric_buffer->write(record);
        });
    // Update warning and fallback thresholds if the settings have changed
    if (m_refresh_storage_profile) {
      set_storage_profile(m_info);
    }
    update_per_sample_totals(m_info);
  }
  // Advance fallback and recovery timers
  advance_timers();
  // Check the available space against the storage thresholds, react as necessary
  check_storage_levels();
  // If the system variables were updated during exclusive processing such as
  // fallback mode, then signal the task supervisor to apply the changes
  if (settings_pending()) {
    Health_Monitor::supervisor()->notify_settings_update();
  }
}

/**
  Resolve the automatic monitored path before filesystem collection.
  @return true if a path is available, false otherwise
*/
bool Disk_Collector::resolve_monitored_path() {
  if (!m_configured_monitored.empty()) return true;

  if (m_monitored.empty() &&
      (!m_unavailable_path.has_value() ||
       m_input_failure != Input_failure::datadir_lookup)) {
    log_warning(
        "%s: Error: Unable to read @@GLOBAL.datadir for automatic disk "
        "monitoring",
        name());
    m_unavailable_path = std::string{};
    m_input_failure = Input_failure::datadir_lookup;
  }

  std::string monitored;
  uint64_t generation{0};
  if (!System_vars::select_disk_monitored_path(m_configured_monitored,
                                                monitored, generation)) {
    if (!m_unavailable_path.has_value() ||
        m_input_failure != Input_failure::datadir_lookup) {
      log_warning(
          "%s: Error: Unable to read @@GLOBAL.datadir for automatic disk "
          "monitoring",
          name());
    }
    m_unavailable_path = std::string{};
    m_input_failure = Input_failure::datadir_lookup;
    return false;
  }

  if (m_monitored != monitored || m_monitored_generation != generation) {
    m_monitored = std::move(monitored);
    m_monitored_generation = generation;
    m_filesystem_sample_state = Filesystem_sample_state{};
    m_filesystem_change = Filesystem_change::initial;
    m_refresh_storage_profile = true;
    if (m_disk_forecaster != nullptr) {
      m_disk_forecaster->set_settings_pending(true);
      Health_Monitor::supervisor()->notify_settings_update();
    }
  }
  return true;
}

/**
  Put the collector into the STOP state, which will cancel active timers,
  issue status messages, etc.
*/
void Disk_Collector::post_process() {
  if (m_disk_forecaster != nullptr) {
    m_disk_forecaster->set_disk_input_available(false);
  }
  set_stop_state();
  log_collector_status(false);  // no settings
}

/**
  Resize the stat buffer if necessary. Must be inside collector lock.
*/
void Disk_Collector::set_buffer_size() {
  unsigned int new_buffer_size = m_retention / sample_interval();
  if (new_buffer_size == 0) {
    new_buffer_size = 1;
  }
  if (new_buffer_size != m_buffer->max_size()) {
    m_buffer->resize(new_buffer_size);
  }
}

/**
  Indicate if the Disk Collector settings have changed.
  @return true if settings have changed
*/
bool Disk_Collector::settings_changed() {
  m_settings_changed =
      (System_vars::status_interval().val != m_status_interval ||
       System_vars::disk_fallback_enable().val != fallback().enabled() ||
       System_vars::disk_fallback_force().val != fallback().force_enabled() ||
       System_vars::disk_low_space_critical_level().val != m_low_space_critical_level ||
       System_vars::disk_low_space_duration().val != m_low_space_duration ||
       System_vars::disk_low_space_level().val != m_low_space_level ||
       System_vars::disk_low_space_percent().val != m_low_space_pct ||
       System_vars::disk_monitored().val != m_configured_monitored ||
       (System_vars::disk_monitored().val[0] == '\0' &&
        System_vars::disk_monitored_generation() != m_monitored_generation) ||
       System_vars::disk_sample_interval().val != sample_interval() ||
       System_vars::disk_retention().val != m_retention ||
       System_vars::disk_running().val != m_running ||
       System_vars::disk_warn_level_1().val != m_warn_level_1_pct ||
       System_vars::disk_warn_level_2().val != m_warn_level_2_pct ||
       System_vars::disk_warn_level_3().val != m_warn_level_3_pct ||
       System_vars::disk_recovery_enable().val != recovery().enabled() ||
       System_vars::disk_recovery_level().val != recovery().level_pct() ||
       System_vars::disk_recovery_time_1().val != recovery().time_1() ||
       System_vars::disk_recovery_time_2().val != recovery().time_2() ||
       System_vars::disk_recovery_max_cycles().val != recovery().max_cycles() ||
       System_vars::disk_recovery_increment().val != recovery().increment() ||
       System_vars::disk_recovery_window().val != recovery().window());
  return m_settings_changed;
}

/**
  Set collection parameters from the system variables.
*/
void Disk_Collector::set_params() {
  m_running = System_vars::disk_running().val;
  // Set a monitoring (aka base) interval that is faster than the sample interval
  set_collector_intervals(k_disk_base_interval, System_vars::disk_sample_interval().val);
  m_retention = System_vars::disk_retention().val;
  m_status_interval = System_vars::status_interval().val;
  const std::string configured_monitored = System_vars::disk_monitored().val;
  std::string monitored;
  if (!System_vars::select_disk_monitored_path(
          configured_monitored, monitored, m_monitored_generation)) {
    monitored.clear();
  }
  m_configured_monitored = configured_monitored;
  m_monitored = monitored;
  set_buffer_size();
  // Warnings
  m_warn_level_1_pct = System_vars::disk_warn_level_1().val;
  m_warn_level_2_pct = System_vars::disk_warn_level_2().val;
  m_warn_level_3_pct = System_vars::disk_warn_level_3().val;
  // Fallback
  fallback().set_enabled(System_vars::disk_fallback_enable().val);
  fallback().set_force_enabled(System_vars::disk_fallback_force().val);
  m_low_space_duration = System_vars::disk_low_space_duration().val;
  fallback_timer().set_timeout(m_low_space_duration);
  m_low_space_level = System_vars::disk_low_space_level().val;
  m_low_space_critical_level = System_vars::disk_low_space_critical_level().val;
  m_low_space_pct = System_vars::disk_low_space_percent().val;
  // Recovery
  recovery().reset();
  recovery().set_enabled(System_vars::disk_recovery_enable().val);
  recovery().set_level_pct(System_vars::disk_recovery_level().val);
  recovery().set_time_1(System_vars::disk_recovery_time_1().val);
  recovery().set_time_2(System_vars::disk_recovery_time_2().val);
  recovery().set_max_cycles(System_vars::disk_recovery_max_cycles().val);
  recovery().set_increment(System_vars::disk_recovery_increment().val);

  // Reset the active recovery window, if any
  const uint64_t orig_recovery_window{recovery().window()};
  recovery().set_window(System_vars::disk_recovery_window().val);
  if ((recovery().window() != orig_recovery_window) && recovery().count() > 0) {
    set_recovery_window(true);
  }
  // Initialize the recovery timers
  recovery_timer_1().set_timeout(recovery().time_1());
  recovery_timer_2().set_timeout(recovery().time_2());
  if (recovery().count() > 0) {
    set_recovery_window(true);
  }
  // Refresh storage thresholds and cancel timers on next collection
  m_refresh_storage_profile = true;
}

/**
  Return health_monitor.disk_running
*/
bool Disk_Collector::running() const { return System_vars::disk_running().val; }

/**
  Get available free space for a monitored filesystem.
  @param[in] target_path  A valid path
  @param[out] disk_info  Storage info for the filesystem
  @param[in,out] sample_state Last successful filesystem sample state
  @param[out] change Current sample classification
  @return 0 success, 1 failure
*/
int Disk_Collector::get_disk_info(const std::string &target_path,
                                  Disk_Info &disk_info,
                                  Filesystem_sample_state &sample_state,
                                  Filesystem_change &change) {
  const auto snapshot = mysql::system_info::query_filesystem(target_path);
  if (!accept_filesystem_snapshot(snapshot, sample_state, disk_info, change)) {
    if (!m_unavailable_path.has_value() ||
        *m_unavailable_path != target_path ||
        m_input_failure != Input_failure::filesystem) {
      std::error_code error;
      if (snapshot.identity.state() == mysql::system_info::Result_state::failed) {
        error = snapshot.identity.error();
      } else if (snapshot.usage.state() == mysql::system_info::Result_state::failed) {
        error = snapshot.usage.error();
      }
      if (error) {
        log_warning(
            "%s: Error: Filesystem check for '%s' failed with error %d: %s'",
            name(), target_path.c_str(), error.value(),
            error.message().c_str());
      } else {
        log_warning(
            "%s: Error: Filesystem check for '%s' returned an incomplete"
            " or invalid sample",
            name(), target_path.c_str());
      }
    }
    m_unavailable_path = target_path;
    m_input_failure = Input_failure::filesystem;
    return 1;
  }

  if (m_unavailable_path.has_value() &&
      (m_input_failure == Input_failure::datadir_lookup ||
       *m_unavailable_path == target_path)) {
    log_filesystem_recovery(target_path);
  }
  m_unavailable_path.reset();
  return 0;
}

/**
  Update totals that change for each sample, convert to MiB.
*/
void Disk_Collector::update_per_sample_totals(const Disk_Info &disk_info) {
  if (disk_info.total_bytes == 0) {
    return;
  }

  m_avail_mb = static_cast<uint64_t>(disk_info.avail_bytes / megabyte);
  double used_pct =
      static_cast<double>(disk_info.total_bytes - disk_info.avail_bytes) /
      disk_info.total_bytes;
  used_pct *= 100.0;

  // Used percentage is displayed in the Performance Schema tables
  m_used_pct = static_cast<float>(used_pct);

  // Detect significant changes in available storage
  if (m_last_avail_mb == 0) {
    m_last_avail_mb = m_avail_mb;
  } else {
    const double diff_pct = utl::change_percent(m_last_avail_mb, m_avail_mb);
    if (diff_pct >= m_delta_pct) {
      m_force_status = true;
      m_last_avail_mb = m_avail_mb;
    }
  }
}

/**
  Update disk info for display only. Do not write to the stat buffer, do not
  check storage levels, do not change collector state.
*/
void Disk_Collector::update_disk_info() {
  Filesystem_sample_state sample_state = m_filesystem_sample_state;
  Filesystem_change change = m_filesystem_change;
  m_disk_error =
      (get_disk_info(m_monitored, m_info, sample_state, change) != 0);
  if (!m_disk_error) {
    m_info.timestamp = time(nullptr);
    update_per_sample_totals(m_info);
  }
}

/**
  Set the storage threshold parameters for the monitored filesystem.
  This requires that a disk sample has been collected.
  - Convert units to MiB.
  - Adjust levels to be relative to the total space.
  @param[in] disk_info  Current block device info
*/
void Disk_Collector::set_storage_profile(const Disk_Info &disk_info) {
  m_total_space_mb = disk_info.total_bytes / megabyte;

  // The 'reserve' level = min(low_space_level, low_space_percent)
  // Operating below the reserve level for too long triggers fallback mode
  const uint64_t low_space_percent_mb =
      (m_total_space_mb * m_low_space_pct) / 100;
  m_reserve_mb = std::min(m_low_space_level, low_space_percent_mb);

  // The 'critical level' defines the absolute low storage limit for the
  // MySQL server. If available storage drops below the critical level, then
  // fallback mode is triggered immediately.
  //
  // The configured critical level may not be suitable for small volumes, such
  // as those used for local testing.
  //
  // If the critical level is configured to be greather than the size of
  // the volume, then adjust the critical level to be 1/2 the reserve level
  // computed above.
  if (m_low_space_critical_level >= m_total_space_mb &&
      m_low_space_critical_level != k_max_level) {  // ignore test values
    m_critical_level_mb = m_reserve_mb / 2;
    log_info("%s: Critical level adjusted to %s MiB", name(),
             format_mb(m_critical_level_mb, 0, false).c_str());
  } else {
    m_critical_level_mb = m_low_space_critical_level;
  }

  // The 'operative space' = total space - reserve level
  m_operative_space_mb = m_total_space_mb - m_reserve_mb;

  // Warning levels are defined as a percentage of the space above
  // the reserve level
  m_warn_level_1_mb = (m_operative_space_mb * m_warn_level_1_pct) / 100;
  m_warn_level_1_mb += m_reserve_mb;
  m_warn_level_2_mb = (m_operative_space_mb * m_warn_level_2_pct) / 100;
  m_warn_level_2_mb += m_reserve_mb;
  m_warn_level_3_mb = (m_operative_space_mb * m_warn_level_3_pct) / 100;
  m_warn_level_3_mb += m_reserve_mb;
  // The largest warning level defines the top of the warning range
  m_max_warn_level_mb = std::max(m_warn_level_1_mb, m_warn_level_2_mb);
  m_max_warn_level_mb = std::max(m_max_warn_level_mb, m_warn_level_3_mb);
  m_last_warn_level_mb = no_warn_level;

  // The 'recovery level' is defined as a percentage of the total space above
  // the reserve level
  uint64_t level_mb = (m_operative_space_mb * recovery().level_pct()) / 100;
  level_mb += m_reserve_mb;
  recovery().set_level_mb(level_mb);

  // Any settings change cancels all running timers
  cancel_fallback_process();
  cancel_recovery_process();

  // Refresh the threshold parameters in the storage states
  set_state_params();

  // If this is a new volume or the volume size has changed
  // - Restore normal operating mode if the server has been fenced
  // - Re-arm the fallback and recovery modes
  if (m_new_volume || m_new_size) {
    // Notify if the monitored volume or size of the volume has changed
    if (m_new_volume) {
      log_volume_change();
    }
    if (m_new_size) {
      log_volume_size_change();
    }
    m_status_counter = 1;
    // Immediately restore normal operating mode
    if (fallback().fenced()) {
      log_warning_status("Restoring normal operating mode");
      clear_fallback_mode();
    }
    // Re-arm the fallback and recovery modes
    reset_fallback_and_recovery();
    m_new_volume = false;
    m_new_size = false;
    // Reset the state machine, let it reassess the new volume
    set_start_state();
  }

  m_refresh_storage_profile = false;
  m_force_status = true;
}

/**
  Start the fallback timer
*/
void Disk_Collector::start_fallback_timer() { fallback_timer().start(); }

/**
  Stop the fallback timer
*/
void Disk_Collector::stop_fallback_timer() { fallback_timer().reset(); }

/**
  Cancel the fallback timer:
  - stop the fallback timer
  - set the timer state to canceled
  - issue a warning that timer was canceled
  - restore the timer state
*/
void Disk_Collector::cancel_fallback_timer() {
  fallback_timer().cancel();
  log_timer_status(fallback_timer());
}

/**
  Fallback eligible: Server is eligible to be placed in fallback mode if:
  - fallback is enabled
  - Health Monitor is not already in fallback mode
  - server is not a group replication secondary
  - SUPER_READ_ONLY is not already set (i.e. externally)
  @param reason  Reason for ineligibility
  @return true if eligible
*/
bool Disk_Collector::fallback_eligible(std::string &reason) {
  // Fencing the server has been disabled
  if (!fallback().enabled()) {
    reason = "Fallback mode is disabled";
    return false;
  }
  // We don't fence group replication secondaries
  if (Server::is_group_replication_secondary()) {
    reason = "Server is a group replication secondary";
    return false;
  }
  // Check if SUPER_READ_ONLY is already set but not by the Health Monitor
  bool error{false};
  if (!fallback().super_read_only_was_set_by_hm() &&
      (Server::get_super_read_only(error) && !error)) {
    reason = "SUPER_READ_ONLY is already ON";
    return false;
  }
  // It's ok if OFFLINE_MODE is already ON
  reason = "";
  return true;
}

/**
  Fallback eligible: Print reason if ineligible.
  @param print_reason  Print the reason why the server cannot be fenced
  @return true if eligible
*/
bool Disk_Collector::fallback_eligible(bool print_reason) {
  std::string reason;
  bool const eligible = fallback_eligible(reason);
  if (print_reason) {
    // Print the reason warning if it has not been printed for the
    // current state
    if (!reason.empty()) {
      if (reason != fallback().last_reason() ||
          m_current_state != fallback().last_state()) {
        log_warning_status(reason);
        fallback().set_last_reason(reason);
        fallback().set_last_state(m_current_state);
      }
    }
  }
  return eligible;
}

/**
  Return true if forced fallback is allowed.
  Criteria:
    - health_monitor.disk_fallback_force = true
    - forced fallback has not been attempted
    - SUPER_READ_ONLY has not been set by the Health Monitor
  Assumes:
    - health_monitor.disk_fallback_enable = true
    - available storage <= health_monitor.disk_low_space_critical_level
*/
bool Disk_Collector::forced_fallback_eligible() {
  return (fallback().force_enabled() && !fallback().force_attempted() &&
          !fallback().super_read_only_was_set_by_hm());
}

/**
  Set the server fallback mode, OFFLINE_MODE and SUPER_READ_ONLY.
  @param force  If true then terminate queries blocking SUPER_READ_ONLY
*/
void Disk_Collector::set_fallback_mode(bool force) {
  // SUPER_READ_ONLY will already be set on a group replication secondary
  if (Server::is_group_replication_secondary()) {
    log_warning_status(
        "Fallback mode canceled: Server is a group replication secondary, " +
        k_super_read_only + " and " + k_offline_mode + " will not be set");
    return;
  }

  // Initialize the recovery timers
  recovery_timer_1().reset();
  recovery_timer_2().reset();

  // Set OFFLINE_MODE and SUPER_READ_ONLY, retry if necessary
  constexpr int k_max_retries{3};
  bool olm_externally_set{false};  // true if OFFLINE_MODE set externally
  bool sro_externally_set{false};  // true if SUPER_READ_ONLY set externally
  bool const offline_mode_set{
      set_offline_mode(k_max_retries, olm_externally_set)};
  bool super_read_only_set{
      set_super_read_only(k_max_retries, sro_externally_set)};

  // Repeated attempts to set SUPER_READ_ONLY failed due to a blocking condition
  // such as an ongoing LOAD DATA or global table lock. Terminate queries on
  // foreground threads holding a GLOBAL lock, then try SUPER_READ_ONLY again.
  //
  // Note:
  // - MySQL client programs may automatically reconnect and resume the current
  //   statement, so the queries are killed first instead of connections.
  // - Large transactions such as LOAD DATA may require a long rollback time
  //   during which the blocking global lock remains held. In such cases,
  //   SUPER_READ_ONLY = ON will still fail, but the offending query has ended.
  if (!super_read_only_set && force) {
    constexpr int k_max_groups{5};
    int group_size{0};

    // Only do this once
    fallback().set_force_attempted(true);

    // Get list of foreground session IDs holding a GLOBAL lock
    Processlist_Ids plist_ids{m_server.get_fg_sessions_with_global_lock()};
    if (plist_ids.size() <= k_max_groups) {
      group_size = 1;
    } else {
      group_size = static_cast<int>(plist_ids.size() / k_max_groups);
    }
    std::string plist_size = std::to_string(plist_ids.size());

    // First pass: Terminate foreground queries in groups of group_size
    log_warning_status("Terminating foreground queries");
    log_warning_status("Foreground sessions: " + plist_size);

    super_read_only_set = terminate_queries(plist_ids, group_size);

    // Second pass: Terminate foreground connections
    if (!super_read_only_set) {
      // Refresh the session list to exclude threads with queries that were
      // terminated in the first pass and that may be rolling back. This also
      // avoids triggering the client to reconnect and restart the query.
      plist_ids = m_server.get_fg_sessions_with_global_lock();
      if (plist_ids.size() <= k_max_groups) {
        group_size = 1;
      } else {
        group_size = static_cast<int>(plist_ids.size() / k_max_groups);
      }
      plist_size = std::to_string(plist_ids.size());

      log_warning_status(k_super_read_only + " failed");
      log_warning_status("Terminating foreground sessions");
      log_warning_status("Foreground sessions: " + plist_size);
      super_read_only_set = terminate_connections(plist_ids, group_size);
    }

    if (super_read_only_set) {
      log_warning_status(k_super_read_only + " succeeded");
      log_warning_status(k_super_read_only + " = ON");
    } else {
      log_warning_status(k_super_read_only + " failed");
    }
  }

  // Update the state of the fallback process.
  // Important notes:
  // - When the Health Monitor sets SUPER_READ_ONLY and OFFLINE_MODE, it also
  //   sets the 'super_read_only_disk_full' and 'offline_mode_disk_full' status
  //   variables.
  // - If the system variables are later cleared, either externally or by the
  //   Health Monitor, then the status variables will also be cleared.
  // - 'super_read_only_was_set_by_hm' and 'offline_mode_was_set_by_hm' track
  //   whether the Health Monitor had previously set the system variables. They
  //   are used to ensure that we only clear variables that we set.
  fallback().set_offline_mode_was_set_by_hm(offline_mode_set &&
                                            !olm_externally_set);
  fallback().set_super_read_only_was_set_by_hm(super_read_only_set &&
                                               !sro_externally_set);
  fallback().set_super_read_only_retry_failed(!super_read_only_set);
  fallback().set_super_read_only_force_failed(!super_read_only_set && force);
  // Bump the fallback counter if either SUPER_READ_ONLY or OFFLINE_MODE was set
  // by the Health Monitor.
  if (fallback().fenced()) {
    fallback().count()++;
  }
  // Update status vars 'offline_mode_disk_full' and 'super_read_only_disk_full'
  update_fallback_status_vars();
}

/**
  Clear OFFLINE_MODE and SUPER_READ_ONLY, reset all fallback state variables.
  This is called when the Health Monitor settings change or a new or resized
  volume is detected.
*/
void Disk_Collector::clear_fallback_mode() {
  // Don't change SUPER_READ_ONLY on a group replication secondary
  if (Server::is_group_replication_secondary()) {
    log_warning_status(
        "Server is a group replication secondary, no change to " +
        k_super_read_only);
    if (fallback().offline_mode_was_set_by_hm()) {
      // Clear OFFLINE_MODE if the node has not been ejected from the group
      if (Server::is_member_online_with_group_majority()) {
        clear_offline_mode();
      }
    }
  } else {
    // Clear SUPER_READ_ONLY and OFFLINE_MODE
    clear_super_read_only(k_fallback_retries);
    clear_offline_mode();
  }
  // Reset fallback state and update status variables
  fallback().reset();
  update_fallback_status_vars();
}

/**
  Check if the state of a fenced server changed:
  - HA designation changes from Primary to Secondary
  - SUPER_READ_ONLY or OFFLINE_MODE were cleared externally

  If the server state changed, then sync the fallback state accordingly.

  This is independent of the recovery process, changes in storage levels or
  updates to the Health Monitor settings.
  @return true if the server state changed
*/
bool Disk_Collector::check_fenced_server_state() {
  // Case 1: Fenced primary demoted to secondary
  if (Server::is_group_replication_secondary()) {
    log_warning_status("This is now a group replication secondary");
    // No change to SUPER_READ_ONLY, as it will be managed by group replication.
    // Clear OFFLINE_MODE if a) the Health Monitor set it, and b) the node has
    // not been ejected from the group.
    if (fallback().offline_mode_was_set_by_hm()) {
      if (Server::is_member_online_with_group_majority()) {
        clear_offline_mode();
      }
    }
    fallback().reset();
    update_fallback_status_vars();
    if (recovery().in_progress()) {
      cancel_recovery_process();
    }
    return true;
  }
  // Case 2: Server was fenced but either OFFLINE_MODE or SUPER_READ_ONLY
  // was cleared externally. Sync the status variables and release ownership
  // so the Health Monitor cannot later clear an externally restored mode.
  bool changed{false};
  bool error{false};
  // OFFLINE_MODE was cleared externally
  if (m_status_offline_mode_disk_full) {
    if (!Server::get_offline_mode(error) && !error) {
      m_status_offline_mode_disk_full = false;
      fallback().set_offline_mode_was_set_by_hm(false);
      log_warning_status("Detected OFFLINE_MODE is OFF");
      log_warning_status(msg_setting_offline_mode_disk_full());
      changed = true;
    }
  }
  // SUPER_READ_ONLY was cleared externally
  if (m_status_super_read_only_disk_full) {
    if (!Server::get_super_read_only(error) && !error) {
      m_status_super_read_only_disk_full = false;
      fallback().set_super_read_only_was_set_by_hm(false);
      log_warning_status("Detected SUPER_READ_ONLY is OFF");
      log_warning_status(msg_setting_super_read_only_disk_full());
      changed = true;
    }
  }
  if (!fallback().fenced() && recovery().in_progress()) {
    cancel_recovery_process();
  }
  return changed;
}

/**
  Process expired fallback timer.
  - Issue warning that the server will be fenced
  - Stop the timer
  - Fence the server
*/
void Disk_Collector::process_expired_fallback_timer() {
  log_timer_status(fallback_timer());
  stop_fallback_timer();
  // Go to fallback mode but do not force SUPER_READ_ONLY, that's only
  // done at the critical level
  set_fallback_mode(false);
  // Timer countdown complete, server fenced
  fallback().last_reason().clear();
}

/**
  Reset fallback and recovery modes, cancel all timers.
  Invoked after the monitored filesystem capacity changes.
*/
void Disk_Collector::reset_fallback_and_recovery() {
  fallback().hard_reset();
  if (fallback_timer().started()) {
    fallback_timer().reset();
  }
  recovery().hard_reset();
  if (recovery_timer_1().started()) {
    recovery_timer_1().reset();
  }
  if (recovery_timer_2().started()) {
    recovery_timer_2().reset();
  }
}

/**
  Cancel the ongoing fallback process.
  Leave fallback state unchanged.
*/
void Disk_Collector::cancel_fallback_process() {
  if (fallback_timer().running()) {
    cancel_fallback_timer();
  }
  fallback().set_force_attempted(false);
  fallback().last_reason().clear();
}

/**
  Attempt to set OFFLINE_MODE. Retry if necessary.
  @param max_retries  Number of attempts to make
  @param already_set  True if OFFLINE_MODE was already set
  @returns True if OFFLINE_MODE was set
*/
bool Disk_Collector::set_offline_mode(int max_retries, bool &already_set) {
  bool error{false};
  already_set = Server::get_offline_mode(error);
  if (error) {
    return false;
  }
  if (already_set) {
    log_warning_status("OFFLINE_MODE is already ON");
    return true;
  }

  bool offline_mode_set{false};
  Retry_Timer retry(max_retries);

  while (retry.wait()) {  // no wait on first attempt
    offline_mode_set = !Server::set_offline_mode();
    if (offline_mode_set) {
      log_warning_status("Setting " + k_offline_mode + " = ON");
      break;
    }
    auto attempt{std::to_string(retry.remaining()) + " " + k_remaining};
    log_warning_status(k_offline_mode + " failed, " + attempt);
    // Update storage totals without changing state
    update_disk_info();
  }
  return offline_mode_set;
}

/**
  Clear OFFLINE_MODE.
  @returns True if OFFLINE_MODE = OFF
*/
bool Disk_Collector::clear_offline_mode() {
  // Only clear OFFLINE_MODE if it was set by the Health Monitor
  if (!fallback().offline_mode_was_set_by_hm()) {
    return true;
  }
  bool const offline_mode_cleared = !Server::clear_offline_mode();

  if (offline_mode_cleared) {
    log_warning_status("Setting " + k_offline_mode + " = OFF", false);
  } else {
    log_warning_status(k_offline_mode + " failed", false);
  }
  // Update storage totals without changing state
  update_disk_info();
  return offline_mode_cleared;
}

/**
  Set SUPER_READ_ONLY. Retry if necessary.
  Setting SUPER_READ_ONLY also sets READ_ONLY.
  @param max_retries  Number of attempts to make
  @param already_set  True if SUPER_READ_ONLY already set
  @returns True if SUPER_READ_ONLY is set
*/
bool Disk_Collector::set_super_read_only(int max_retries, bool &already_set) {
  bool error{false};
  already_set = Server::get_super_read_only(error);
  if (error) {
    return false;
  }
  if (already_set) {
    log_warning_status("SUPER_READ_ONLY is already ON");
    return true;
  }

  bool super_read_only_set{false};
  Retry_Timer retry(max_retries);

  while (retry.wait()) {  // no wait on first attempt
    super_read_only_set = !Server::set_super_read_only();
    if (super_read_only_set) {
      log_warning_status("Setting " + k_super_read_only + " = ON", false);
      break;
    }
    auto attempt{std::to_string(retry.remaining()) + " " + k_remaining};
    log_warning_status(k_super_read_only + " failed, " + attempt, false);
    // Update storage totals without changing state
    update_disk_info();
  }
  return super_read_only_set;
}

/**
  Clear READ_ONLY and SUPER_READ_ONLY. Retry if necessary.

  @param max_retries   Number of attempts to make
  @returns True if READ_ONLY and SUPER_READ_ONLY = OFF
*/
bool Disk_Collector::clear_super_read_only(int max_retries) {
  // Only clear SUPER_READ_ONLY if it was set by the Health Monitor
  if (!fallback().super_read_only_was_set_by_hm()) {
    return true;
  }
  bool super_read_only_cleared{false};
  Retry_Timer retry(max_retries);

  while (retry.wait()) {  // no wait on first attempt
    super_read_only_cleared = !Server::clear_super_read_only();
    if (super_read_only_cleared) {
      log_warning_status("Setting " + k_super_read_only + " = OFF", false);
      break;
    }
    auto attempt{std::to_string(retry.remaining()) + " " + k_remaining};
    log_warning_status(k_super_read_only + " failed, " + attempt, false);
    // Update storage totals without changing state
    update_disk_info();
  }
  return super_read_only_cleared;
}

/**
  Terminate queries or connections until SUPER_READ_ONLY can be set.

  @param plist_ids   Processlist ids of connections to terminate
  @param group_size  Terminate connections/queries in groups of this size
  @param query_only  Terminate the queries, not the connections
  @returns true if SUPER_READ_ONLY was set, false otherwise
*/
bool Disk_Collector::terminate_and_retry(const Processlist_Ids &plist_ids,
                                         int group_size, bool query_only) {
  std::string errmsg;
  const std::string kill_type{query_only ? "KILL QUERY " : "KILL CONNECTION "};
  int pid_count{0};

  // Terminate a group of queries/connections, then retry SUPER_READ_ONLY
  for (auto const &pid : plist_ids) {
    std::string const kill_cmd = kill_type + pid;
    log_warning_status(kill_cmd, false);
    const int rc = (query_only ? m_server.kill_query(pid, errmsg)
                               : m_server.kill_connection(pid, errmsg));
    std::ostringstream message;
    if (rc != 0) {
      message << "Error: '" << kill_cmd << "' returned '" << errmsg << "'";
      log_warning_status(message.str(), false);
    }
    if (++pid_count % group_size == 0 || pid == plist_ids.back()) {
      message.clear();
      message << "Retry " << k_super_read_only;
      log_warning_status(message.str(), false);
      constexpr int max_retries{3};
      bool already_set{false};  // ignored
      if (set_super_read_only(max_retries, already_set)) {
        return true;
      }
      // Update disk totals without changing state
      update_disk_info();
      log_warning_status("", false);
    }
  }
  return false;
}

/**
  Recovery eligible. Server is eligible to recover from fallback mode if:
  - Recovery is enabled
  - HA role is Primary or N/A
  @param reason  Message string giving reason for ineligibility
  @return true if eligible to recover from fallback mode
*/
bool Disk_Collector::recovery_eligible(std::string &reason) {
  if (!recovery().enabled()) {
    reason = "Recovery is disabled";
    return false;
  }
  // Don't attempt recovery on group replication secondaries.
  if (Server::is_group_replication_secondary()) {
    reason = "Server is a group replication secondary";
    return false;
  }
  // Make sure server wasn't fenced by group replication.
  if (Server::is_group_replication_primary()) {
    if (!Server::is_member_online_with_group_majority()) {
      reason = "Server is not online and in a group majority, ";
      if (recovery().in_progress()) {
        reason += "recovery from fallback mode canceled";
      } else {
        reason += "cannot restore normal operating mode";
      }
      return false;
    }
  }
  reason = "";
  return true;
}

/**
  Check the recovery status after the server has been fenced.
  This is only called if the server was fenced by the Health Monitor.

  Recovery from fallback mode is done in two phases. If storage rises above
  the recovery level, then:
  - after a delay of 'recovery_timer_1' set OFFLINE_MODE = OFF, then
  - after an additional delay of 'recovery_timer_2', set SUPER_READ_ONLY = OFF.

  Increase the recovery delays by 'disk_recovery_increment' for each recovery
  cycle.
*/
void Disk_Collector::check_recovery_status() {
  // The recovery process requires that the server is fenced.
  if (!fallback().fenced() && !recovery().in_progress()) {
    return;
  }
  // Recovery is in progress but available storage dropped below the recovery
  // level. Cancel recovery.
  if (below_recovery_level()) {
    if (recovery().in_progress()) {
      // Storage dropped below the recovery limit, cancel all timers
      log_warning_status("Available space (" + format_level(m_avail_mb) + ")" +
                         " is below recovery level (" +
                         format_level(recovery().level_mb()) + ")");
      cancel_recovery_process();
    }
    return;
  }
  // Recovery is in progress and available storage is above the recovery level,
  // but the server is no longer eligible for recovery. This can happen if a
  // primary server is demoted to secondary. Cancel recovery.
  std::string reason;
  if (recovery().in_progress()) {
    if (!recovery_eligible(reason)) {
      log_warning_status(reason);
      cancel_recovery_process();
      return;
    }
  }
  // Storage is above the recovery level, but max recovery cycles are exceeded.
  if (max_recovery_cycles() && !recovery().in_progress()) {
    if (!recovery().cycle_warning()) {
      std::string msg{"Maximum recovery cycles reached "};
      msg +=
          "(" + std::to_string(recovery().count()) + ") within the recovery ";
      msg += "window (" + std::to_string(recovery().window()) + " seconds)";
      log_warning_status(msg);
      recovery().set_cycle_warning(true);  // warn once
    }
    return;
  }
  // Storage is above the recovery level, but the server is no longer eligible
  // for recovery.
  if (!recovery_eligible(reason)) {
    if (!recovery().ineligible_warning()) {
      log_warning_status(reason);
      recovery().set_ineligible_warning(true);  // warn once
    }
    return;
  }
  // Storage is above the recovery level and the server is fenced and otherwise
  // eligible for recovery. Start or process the recovery timers:
  // - Recovery timer 1 expires: OFFLINE_MODE = OFF, start recovery timer 2
  // - Recovery timer 2 expires; SUPER_READ_ONLY = OFF, clear fallback state

  // Initialize the recovery process
  if (!recovery().in_progress()) {
    recovery().reset();
    recovery().set_in_progress(true);
    // Adjust recovery timeouts according to the current recovery cycle
    set_recovery_timeout_1();
    set_recovery_timeout_2();
    log_warning_status("Available space (" + format_level(m_avail_mb) + ")" +
                       " is above recovery level (" +
                       format_level(recovery().level_mb()) + ")");
  }

  // Start the OFFLINE_MODE timer
  if (!recovery_timer_1().started()) {
    // If the Health Monitor didn't set OFFLINE_MODE, then nothing to do
    if (!offline_mode_disk_full()) {
      if (!recovery().offline_warning()) {
        log_warning_status("OFFLINE_MODE will not be changed");
        recovery().set_offline_warning(true);
      }
      // Fall through to the SUPER_READ_ONLY timer
      // ...
    } else {
      start_recovery_timer_1();
      log_timer_status(recovery_timer_1());
      recovery().set_offline_warning(false);
      return;
    }
  }
  // Process the running OFFLINE_MODE timer
  if (recovery_timer_1().running()) {
    // If timer 1 is running, then the Health Monitor set OFFLINE_MODE = ON.
    // Cancel the timer if OFFLINE_MODE was cleared externally.
    if (!offline_mode_disk_full()) {
      if (!recovery().offline_warning()) {
        log_warning_status("OFFLINE_MODE will not be changed");
      }
      cancel_recovery_timer_1();
      // Fall through to SUPER_READ_ONLY timer
      // ...
    } else {
      // Timer still running, issue status message
      if (!recovery_timer_1().expired()) {
        if (recovery_timer_1().report_status()) {
          log_timer_status(recovery_timer_1(), true);
        }
        return;
      }
      // OFFLINE_MODE timer expired, clear OFFLINE_MODE
      stop_recovery_timer_1();
      log_timer_status(recovery_timer_1());
      clear_offline_mode();
      fallback().set_offline_mode_was_set_by_hm(false);
      // Sync status var 'offline_mode_disk_full' with new server state
      update_offline_mode_disk_full();
      // Recovery attempts are only counted if server state was changed
      std::string message{"Recovery count: "};
      message += std::to_string(++recovery().count()) + ", Remaining: ";
      message +=
          std::to_string(recovery().max_cycles() - recovery().count());
      log_warning_status(message);
      // This is the first recovery, set the recovery expiration time
      if (recovery().count() == 1) {
        set_recovery_window(false);
      }
      // Fall through to SUPER_READ_ONLY
      // ...
    }
  }

  // Start the SUPER_READ_ONLY timer
  if (!recovery_timer_2().started()) {
    // If the Health Monitor didn't set SUPER_READ_ONLY, then nothing to do
    if (!super_read_only_disk_full()) {
      log_warning_status("SUPER_READ_ONLY will not be changed");
      complete_recovery_process();
      return;
    }
    start_recovery_timer_2();
    log_timer_status(recovery_timer_2());
    return;
  }

  // Process the running SUPER_READ_ONLY timer
  if (recovery_timer_2().running()) {
    // If timer 2 is running, then the Health Monitor set SUPER_READ_ONLY = ON.
    // Cancel the timer if SUPER_READ_ONLY was cleared externally.
    if (!super_read_only_disk_full()) {
      log_warning_status("SUPER_READ_ONLY will not be changed");
      cancel_recovery_timer_2();
      complete_recovery_process();
      return;
    }
    // Timer still running, issue status message
    if (!recovery_timer_2().expired()) {
      if (recovery_timer_2().report_status()) {
        log_timer_status(recovery_timer_2(), true);
      }
      return;
    }
    // SUPER_READ_ONLY timer expired, clear SUPER_READ_ONLY, end recovery
    stop_recovery_timer_2();
    log_timer_status(recovery_timer_2());
    clear_super_read_only(k_fallback_retries);
    // Clear status vars, reset fallback and recovery states, reset timers
    complete_recovery_process();
  }
}

/**
  Complete the recovery process:
  - Reset the fallback and recovery states
  - Reset the recovery timers
  - Clear the status variables
  - Issue messages with the current values of SUPER_READ_ONLY and OFFLINE_MODE
*/
void Disk_Collector::complete_recovery_process() {
  // Show the current values of OFFLINE_MODE and SUPER_READ_ONLY
  std::string message = Server::get_offline_mode_str();
  if (!message.empty()) {
    log_warning_status(message);
  }
  message = Server::get_super_read_only_str();
  if (!message.empty()) {
    log_warning_status(message);
  }
  // Clear the fallback state. Note that even if SUPER_READ_ONLY = OFF failed,
  // we still clear fallback().super_read_only_was_set_by_hm() and
  // 'super_read_only_disk_full', as there is no more action to take.
  fallback().reset();
  update_fallback_status_vars();
  m_force_status = true;
  log_warning_status("Restore normal operating mode complete");
  recovery_timer_1().reset();
  recovery_timer_2().reset();
  recovery().reset();
}

/**
  Cancel the recovery process:
  - Reset the recovery timers
  - Issue log messages indicating that recovery has been canceled
*/
void Disk_Collector::cancel_recovery_process() {
  if (recovery_timer_1().started()) {
    cancel_recovery_timer_1();
  } else {
    recovery_timer_1().reset();
  }
  if (recovery_timer_2().started()) {
    cancel_recovery_timer_2();
  } else {
    recovery_timer_2().reset();
  }
  recovery().reset();
}

/**
  Check if the max recovery cycle count has been reached.
  @return True if recovery cycles have reached the maximum within
          the recovery expiration window
*/
bool Disk_Collector::max_recovery_cycles() {
  if (recovery().expiration() > 0) {
    return (recovery().count() >= recovery().max_cycles()) &&
           (recovery().expiration() > time(nullptr));
  }
  return false;
}

/**
  Set the end of the recovery window.
  param[in] print_status  If true, issue log message
*/
void Disk_Collector::set_recovery_window(bool print_status) {
  const std::chrono::seconds window(recovery().window());
  recovery().set_expiration(std::time(nullptr) + window.count());
  if (print_status) {
    std::string msg{"Recovery window ends " +
                    std::to_string(recovery().window())};
    msg += " seconds from now";
    log_info_message(msg);
  }
}

/**
  Check the recovery window.
  If sufficient time has elapsed since the last recovery, then reset
  the recovery cycle count.
*/
void Disk_Collector::check_recovery_window() {
  if (time(nullptr) > recovery().expiration()) {
    std::string msg{"Recovery window (" + std::to_string(recovery().window())};
    msg += " seconds) has expired, resetting recovery cycle count ";
    msg += "(" + std::to_string(recovery().count()) + ")";
    log_info_message(msg);
    recovery().set_count(0);
    recovery().set_expiration(0);
  }
}

/**
  Set the timeout for recovery timer 1.
*/
void Disk_Collector::set_recovery_timeout_1() {
  // Timeout increases for each recovery cycle
  recovery_timer_1().set_timeout(recovery().time_1() +
                                 recovery().increment() * recovery().count());
}

/**
  Start the recovery_1 timer:
  - reset status update message counter
  - assumes timeout has been set
*/
void Disk_Collector::start_recovery_timer_1() { recovery_timer_1().start(); }

/**
  Stop the recovery_1 timer:
  - set the recovery_1 timer to OFF
  - renable status update messages
*/
void Disk_Collector::stop_recovery_timer_1() { recovery_timer_1().stop(); }

/**
  Cancel the recovery_1 timer:
  - stop the recovery_1 timer
  - set the timer state to canceled
  - issue a warning that timer was canceled
*/
void Disk_Collector::cancel_recovery_timer_1() {
  recovery_timer_1().cancel();
  log_timer_status(recovery_timer_1());
}

/**
  Set the timeout for recovery timer 2.

*/
void Disk_Collector::set_recovery_timeout_2() {
  // Timeout increases for each recovery cycle
  recovery_timer_2().set_timeout(recovery().time_2() +
                                 recovery().increment() * recovery().count());
}

/**
  Start the recovery_2 timer:
  - reset status update message counter
  - assumes timeout has been set
*/
void Disk_Collector::start_recovery_timer_2() { recovery_timer_2().start(); }

/**
  Stop the recovery_2 timer:
  - set the recovery_2 timer to OFF
  - renable status update messages
*/
void Disk_Collector::stop_recovery_timer_2() { recovery_timer_2().stop(); }

/**
  Cancel the recovery_2 timer:
  - stop the recovery_2 timer
  - set the timer state to canceled
  - issue a warning that timer was canceled
*/
void Disk_Collector::cancel_recovery_timer_2() {
  recovery_timer_2().cancel();
  log_timer_status(recovery_timer_2());
}

/**
  Update the 'offline_mode_disk_full' status variable.
*/
void Disk_Collector::update_offline_mode_disk_full() {
  if (m_status_offline_mode_disk_full !=
      fallback().offline_mode_was_set_by_hm()) {
    m_status_offline_mode_disk_full = fallback().offline_mode_was_set_by_hm();
    log_warning_status(msg_setting_offline_mode_disk_full());
  }
}

/**
  Update the 'super_read_only_disk_full' status variable.
*/
void Disk_Collector::update_super_read_only_disk_full() {
  if (m_status_super_read_only_disk_full !=
      fallback().super_read_only_was_set_by_hm()) {
    m_status_super_read_only_disk_full =
        fallback().super_read_only_was_set_by_hm();
    log_warning_status(msg_setting_super_read_only_disk_full());
  }
}

/**
  Update fallback status variables, log changes.
*/
void Disk_Collector::update_fallback_status_vars() {
  update_offline_mode_disk_full();
  update_super_read_only_disk_full();
}

/**
  Log messages for updates to status variables 'super_read_only_disk_full' and
  'offline_mode_disk_full'.
  @returns "Setting status" message
*/
std::string Disk_Collector::msg_setting_super_read_only_disk_full() const {
  std::string msg{"Setting status "};
  msg += "'" + k_super_read_only_disk_full + "'";
  msg += " = " + true_false(m_status_super_read_only_disk_full);
  return msg;
}

std::string Disk_Collector::msg_setting_offline_mode_disk_full() const {
  std::string msg{"Setting status "};
  msg += "'" + k_offline_mode_disk_full + "'";
  msg += " = " + true_false(m_status_offline_mode_disk_full);
  return msg;
}

/**
  Disk Collector settings formatted string.
*/
std::string Disk_Collector::format_settings() {
  std::stringstream profile;
  profile << "running" << eq << on_off(m_running);
  profile << ", volume monitored" << eq << "'" << m_monitored << "'";
  profile << ", base interval" << eq << base_interval() << "s";
  profile << ", sample interval" << eq << sample_interval() << "s";
  profile << ", retention" << eq << m_retention << "s";
  profile << ", max samples" << eq << m_buffer->max_size();
  profile << ", status interval" << eq << m_status_interval;
  profile << ", low level" << eq << m_low_space_level << " MiB";
  profile << ", low percent" << eq << m_low_space_pct << "%%";
  profile << ", critical level" << eq << m_low_space_critical_level << " MiB";
  // Note if critical level had to be adjusted for the volume size
  if (m_critical_level_mb != 0 &&
      m_low_space_critical_level != m_critical_level_mb) {
    profile << " (" << m_critical_level_mb << " MiB adj)";
  }
  profile << ", warning levels" << eq << m_warn_level_1_pct;
  profile << "/" << m_warn_level_2_pct << "/" << m_warn_level_3_pct << "%%";
  profile << ", fallback (enable" << eq << on_off(fallback().enabled());
  profile << ", force" << eq << on_off(fallback().force_enabled()) << ")";
  profile << ", low space duration" << eq << m_low_space_duration << "s";
  // Note if low space duration had to be adjusted to the base interval
  if (m_low_space_duration != fallback_timer().timeout()) {
    profile << " (" << fallback_timer().timeout() << "s adj)";
  }
  profile << ", recovery (enable" << eq << on_off(recovery().enabled());
  profile << ", level" << eq << recovery().level_pct() << "%%";
  profile << ", time 1" << eq << recovery().time_1() << "s";
  profile << ", time 2" << eq << recovery().time_2() << "s";
  profile << ", max cycles" << eq << recovery().max_cycles();
  profile << ", increment" << eq << recovery().increment();
  profile << ", window" << eq << recovery().window() << ")";
  return profile.str();
}

/**
  Storage info formatted string.
*/
std::string Disk_Collector::format_status_info() {
  std::stringstream profile;
  profile << "volume" << eq << "'" << m_monitored << "'";
  profile << ", avail" << eq << format_mb(m_avail_mb, -1, true);
  profile << ", total" << eq << format_mb(m_total_space_mb, -1, true);
  profile << std::setprecision(1) << std::fixed;
  profile << ", used" << eq << m_used_pct << "%%";
  profile << ", reserve" << eq << format_mb(m_reserve_mb, -1, true);
  profile << ", critical" << eq << format_mb(m_critical_level_mb, -1, true);
  profile << ", recovery" << eq << format_mb(recovery().level_mb(), -1, true);
  profile << ", warnings" << eq << format_mb(m_warn_level_1_mb, -1, true);
  profile << "/" << format_mb(m_warn_level_2_mb, -1, true);
  profile << "/" << format_mb(m_warn_level_3_mb, -1, true);
  // Group Replication role, primary or secondary
  if (Server::gr_status().valid) {
    profile << ", gr" << eq << Server::gr_status().role();
  }
  return profile.str();
}

/**
  Collector state tag: Start, Stop, etc.
*/
std::string Disk_Collector::tag_collector_state() {
  if (m_current_state == nullptr) {
    return "";
  }
  switch (m_current_state->state()) {
    case Storage_State::START:
      return k_started;
      break;
    case Storage_State::STOP:
      return k_stopped;
      break;
    default:
      return "";
      break;
  }
}

/**
  Resource state tag: Normal Level, Warning Level, etc.
*/
std::string Disk_Collector::tag_resource_state() { return storage_status(); }

/**
  Mode string: Fallback, Timer, etc
*/
std::string Disk_Collector::tag_mode() {
  std::string mode_status_msg;
  if (recovery().in_progress()) {
    mode_status_msg = tag_recovery();
  } else {
    if (fallback().fenced()) {
      mode_status_msg = tag_fallback();
    }
  }
  return mode_status_msg;
}

/**
  Fallback mode string: "Fallback".
*/
std::string Disk_Collector::tag_fallback() { return k_fallback; }

/**
  Recovery mode string: "Recovery".
*/
std::string Disk_Collector::tag_recovery() { return k_recovery; }

/**
  Build storage state string.
*/
std::string Disk_Collector::storage_status() const {
  if (m_current_state == nullptr) {
    return "";
  }
  std::string state;
  switch (m_current_state->state()) {
    case Storage_State::NORMAL:
      state = k_normal_level;
      break;
    case Storage_State::WARNING:
      state = warning_status();
      break;
    case Storage_State::LOW_LIMIT:
      state = k_low_level + " (" + format_mb(m_reserve_mb, -1, true) + ")";
      break;
    case Storage_State::CRITICAL:
      state = k_critical_level + " (" +
              format_mb(m_critical_level_mb, -1, true) + ")";
      break;
    default:
      break;
  }
  return state;
}

/**
  Timer status with optional message: Started, Running, Expired, ...
*/
std::string Disk_Collector::timer_status(Mode_Timer &timer,
                                         const std::string &msg) const {
  std::string timer_state{timer.name()};
  if (timer.running()) {
    std::string const countdown{std::to_string(timer.remaining()) +
                                " seconds "};
    timer_state += ": " + k_running + ": " + countdown + timer.running_msg();
  } else if (timer.expired()) {
    timer_state += ": " + k_expired;
  } else if (timer.canceled()) {
    timer_state += ": " + k_canceled;
  }
  if (!msg.empty()) {
    timer_state += " " + msg;
  }
  return timer_state;
}

/**
  Warning level string: Warning Level 1, Warning Level 2, ...
*/
std::string Disk_Collector::warning_status() const {
  int level{0};
  if (m_last_warn_level_mb == m_warn_level_1_mb) {
    level = 1;
  } else if (m_last_warn_level_mb == m_warn_level_2_mb) {
    level = 2;
  } else if (m_last_warn_level_mb == m_warn_level_3_mb) {
    level = 3;
  }
  std::string warn_state =  // non-const to allow move
      k_warning_level + " " + std::to_string(level) + " (" +
      format_mb(m_last_warn_level_mb, -1, true) + ")";
  return warn_state;
}

/**
  Log informational message with storage state, storage levels and message.
*/
void Disk_Collector::log_info_message(const std::string &msg, bool levels) {
  log_info("%s",
           build_resource_message(storage_status(), tag_mode(), msg, levels)
               .c_str());
}

/**
  Log informational message with storage state, no levels and message.
*/
void Disk_Collector::log_info_message(const std::string &msg) {
  log_info_message(msg, false);
}

/**
  Log warning with storage state, fallback state, storage levels and message.
*/
void Disk_Collector::log_warning_status(const std::string &msg, bool levels) {
  log_warning("%s",
              build_resource_message(storage_status(), tag_mode(), msg, levels)
                  .c_str());
}

/**
  Log warning with storage state and message.
*/
void Disk_Collector::log_warning_status(const std::string &msg) {
  log_warning_status(msg, false);
}

/**
  Low space warning level 1/2/3.
*/
void Disk_Collector::log_warning_level([[maybe_unused]] int level) {
  log_warning("%s",
              build_resource_message(warning_status(), tag_mode(), "", true)
                  .c_str());
}

/**
  Low space warning with timer state.
  Only use the "Fallback" tag if the server is fenced or in the process
  of being fenced.
*/
void Disk_Collector::log_warning_low_limit() {
  std::string fallback_state;
  if (fallback_timer().running()) {
    fallback_state = timer_status(fallback_timer(), "");
  } else {
    if (fallback().fenced()) {
      fallback_state = tag_fallback();
    }
  }
  log_warning("%s",
              build_resource_message(storage_status(), fallback_state, "", true)
                  .c_str());
}

/**
  Error condition warning.
*/
void Disk_Collector::log_warning_error_condition() {
  std::string reason;
  if (m_unavailable_path.has_value()) {
    reason = m_input_failure == Input_failure::datadir_lookup
                 ? "Data Directory Unavailable"
                 : "Filesystem Unavailable";
  }
  log_warning("%s",
              build_collector_message("Error", reason, "", true).c_str());
}

/**
  Log filesystem recovery after an unavailable transition.
*/
void Disk_Collector::log_filesystem_recovery(const std::string &target_path) {
  log_info("%s: Filesystem check for '%s' is available again", name(),
           target_path.c_str());
}

/**
  Timer status, no levels.
*/
void Disk_Collector::log_timer_status(Mode_Timer &timer) {
  log_timer_status(timer, "", false);
}

/**
  Timer status with optional storage levels.
*/
void Disk_Collector::log_timer_status(Mode_Timer &timer, bool levels) {
  log_timer_status(timer, "", levels);
}

/**
  Timer status with optional message and storage levels.
*/
void Disk_Collector::log_timer_status(Mode_Timer &timer, const std::string &msg,
                                      bool levels) {
  const std::string storage_state{storage_status()};
  const std::string timer_state{timer_status(timer, msg)};
  log_warning(
      "%s",
      build_resource_message(storage_state, tag_mode(), timer_state, levels)
          .c_str());
}

/**
  The monitored disk has changed.
*/
void Disk_Collector::log_volume_change() {
  std::string const state =
      "New Volume Detected (" + format_mb(m_total_space_mb, -1, true) + ")";
  log_info("%s", build_collector_message(state, "", "", true).c_str());
}

/**
  Size of the monitored disk has changed.
*/
void Disk_Collector::log_volume_size_change() {
  std::string const state = "New Volume Size Detected (" +
                            format_mb(m_total_space_mb, -1, true) + ")";
  log_info("%s", build_collector_message(state, "", "", true).c_str());
}

/**
  Log the current storage levels, no message.
*/
void Disk_Collector::log_resource_levels() { log_info_message("", true); }

/**
  Collector or storage status with levels, depending on state.
*/
void Disk_Collector::log_resource_status() {
  if (m_current_state == nullptr) {
    return;
  }
  switch (m_current_state->state()) {
    case Storage_State::State::START:
    case Storage_State::State::STOP:
      log_collector_status(true);
      break;
    case Storage_State::State::NORMAL:
      if (status_message_timer().expired() || m_settings_changed ||
          m_force_status) {
        log_resource_levels();
        m_settings_changed = false;
        m_force_status = false;
      }
      break;
    case Storage_State::State::WARNING:
      if (status_message_timer().expired() || m_settings_changed ||
          m_force_status) {
        log_warning_level();
        m_settings_changed = false;
        m_force_status = false;
      }
      break;
    case Storage_State::State::LOW_LIMIT:
      log_warning_low_limit();
      break;
    case Storage_State::State::CRITICAL:
      log_warning_status("", true);
      break;
    case Storage_State::State::ERROR:
      log_warning_error_condition();
      break;
    default:
      log_resource_levels();
      break;
  }
}

/**
  Set threshold params for all storage states.
*/
void Disk_Collector::set_state_params() { Storage_State::get_params(); }

/**
  Set the next state based on the available space.
*/
void Disk_Collector::set_state(Storage_State &new_state) {
  if (m_current_state != nullptr) {
    m_current_state->exit();
  }
  m_current_state = &new_state;
  m_current_state->enter();
}

/**
  Check the available space against the storage thresholds:
    - available space < critical low space level, trigger fallback mode
    - available space < reserve level, start fallback timer
    - fallback timer >= low space duration, trigger fallback mode
    - available space > reserve level, cancel fallback timer if running
    - available space < a warning level, issue warning message
  Otherwise, if status duration has expired, issue a status message.
*/
void Disk_Collector::check_storage_levels() {
  // check_levels() can be query-intensive, verify the server state
  if (!Health_Monitor::available()) {
    return;
  }
  // Check the recovery status before processing the new storage levels
  if (recovery().count() > 0) {
    check_recovery_window();
  }
  // Set the new storage levels for the state machine
  Storage_State::get_levels();
  // Process the new storage levels
  m_current_state->check_levels();
}

/**
  Set the time series buffer.

  @param buffer      Disk time series buffer
  @param forecaster  Disk Forecaster associated with the buffer
*/
void Disk_Collector::set_disk_metric_buffer(Disk_Metric_Buffer buffer,
                                            Disk_Forecaster *forecaster) {
  m_disk_metric_buffer = std::move(buffer);
  m_disk_forecaster = forecaster;
}

}  // namespace hm

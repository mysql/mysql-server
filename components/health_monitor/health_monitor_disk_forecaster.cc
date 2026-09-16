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

#include <iomanip>  // setprecision
#include <sstream>

#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include "health_monitor.h"
#include "health_monitor_configuration.h"
#include "health_monitor_disk.h"
#include "health_monitor_log.h"
#include "health_monitor_server.h"
#include "health_monitor_supervisor.h"
#include "health_monitor_system_vars.h"
#include "health_monitor_util.h"

namespace hm {
Disk_Forecaster::Disk_Forecaster()
    : Collector("Disk Forecaster",
                "Disk",  // same resource name as the Disk Collector
                "disk_forecaster", "health_dfx",
                "Health Monitor disk forecaster") {
  // Memory instrumentation
  mysql_rwlock_register("health", m_rwlock_info, 5);
  PSI_MEMORY_CALL(register_memory)("health", m_memory_info, 4);

  // Metric and forecast buffers
  m_disk_metric_buffer = std::make_shared<Metric_Buffer<Disk_Metric>>(
      m_metric_rwlock_key, m_metric_memory_key);
#if 0
  // Aggregation is deferred
  m_disk_metric_aggregate_buffer =
      std::make_shared<Stat_Buffer<Disk_Metric_Aggregate>>(
          m_aggregate_rwlock_key, m_aggregate_memory_key);
#endif
  m_disk_forecast_buffer = std::make_shared<Stat_Buffer<Disk_Forecast>>(
      m_forecast_rwlock_key, m_forecast_memory_key);
  m_disk_forecast_summary_buffer =
      std::make_shared<Stat_Buffer<Disk_Forecast_Summary>>(
          m_forecast_summary_rwlock_key, m_forecast_summary_memory_key);

  // Forecast model lock
  mysql_rwlock_init(m_forecast_model_rwlock_key, &m_model_lock);

  // Set initial run mode and test state status vars
  set_run_mode(
      static_cast<Dfx_Run_Mode>(System_vars::disk_forecast_run_mode().val));
  set_test_state(Dfx_Test_State::NONE);
}

// -----------------------------------------------------------------------------
// Processing
// -----------------------------------------------------------------------------

/**
  Initialize the Disk Forecaster.
  Performs initialization after construction.
  Call this immediately after creating the collector.
*/
void Disk_Forecaster::initialize() {
  // Set the local parameters then size the stat buffer accordingly
  set_params();
  set_disk_metric_buffer_size();
  // set_disk_metric_aggregate_buffer_size(); // deferred
  set_disk_forecast_buffer_size();
  initialize_timers();
  // Share the time series buffer with the Disk Collector
  Health_Monitor::disk_collector()->set_disk_metric_buffer(disk_metric_buffer(),
                                                           this);
}

/**
  Initialize the forecast and report timers.
*/
void Disk_Forecaster::initialize_timers() {
  forecast_timer().set_name("Forecast Timer");
  forecast_timer().set_interval(base_interval());  // tick() duration
  forecast_timer().set_timeout(m_forecast_interval_seconds);

  report_timer().set_name("Forecast Report Timer");
  report_timer().set_interval(base_interval());  // tick() duration
  report_timer().set_timeout(m_report_interval_seconds);

  short_report_timer().set_name("Forecast Status Timer");
  short_report_timer().set_interval(base_interval());  // tick() duration
  short_report_timer().set_timeout(m_short_report_interval_seconds);
}

/**
  Advance the timers driven by the base interval.
*/
void Disk_Forecaster::advance_cycle_timers() {
  if (forecast_timer().expired()) {
    forecast_timer().reset();
  }
  forecast_timer().tick();

  if (report_timer().expired()) {
    report_timer().reset();
  }
  report_timer().tick();

  if (short_report_timer().expired()) {
    short_report_timer().reset();
  }
  short_report_timer().tick();
}

/**
  Issue status before starting the collection cycle.
*/
void Disk_Forecaster::pre_process() {
  m_first_normal_cycle = true;
  log_collector_status("Started", true);
}

/**
  Main processing loop, 10 second interval.
  Get disk metrics every minute, generate forecasts, issue forecast reports.
*/
void Disk_Forecaster::process() {
  auto processing_guard = std::unique_lock<std::mutex>(m_processing_mutex);
  auto settings_guard = std::unique_lock<std::mutex>(m_settings_mutex);

  if (stop_requested()) {
    return;
  }
  if (!Health_Monitor::available()) {
    return;
  }

  if (is_new_run_mode()) {
    log_info_message("Run Mode: " + run_mode_name(), false);

    switch (run_mode()) {
      case Dfx_Run_Mode::TEST:
        // Pause metric collection
        disk_metric_buffer()->set_writeable(false);
        m_first_normal_cycle = true;
        process_test_cycle();
        break;
      case Dfx_Run_Mode::NORMAL:
        break;
      case Dfx_Run_Mode::STANDBY:
        // Pause metric collection
        disk_metric_buffer()->set_writeable(false);
        m_first_normal_cycle = true;
        break;
      case Dfx_Run_Mode::NONE:
        m_first_normal_cycle = true;
        break;
    }

    set_is_new_run_mode(false);
  }

  // Normal operation: collect disk metrics, generate forecasts
  if (run_mode() == Dfx_Run_Mode::NORMAL) {
    process_normal_cycle();
  }

  // Check if settings have changed
  if (settings_pending()) {
    settings_guard.unlock();
    processing_guard.unlock();
    Health_Monitor::supervisor()->notify_settings_update();
  }
}

/**
  Apply pending settings unless a Disk Forecaster processing cycle is active.

  This method is called from the Supervisor thread.

  If a cycle is active, leave the settings pending and return so the Supervisor
  can update other collectors. The Disk Forecaster thread signals the Supervisor
  after the cycle completes.

  @return true if settings were applied, false if processing is active
*/
bool Disk_Forecaster::update_settings() {
  auto command_guard = std::unique_lock<std::mutex>(m_command_lock);
  auto processing_guard =
      std::unique_lock<std::mutex>(m_processing_mutex, std::try_to_lock);
  if (!processing_guard.owns_lock()) return false;

  // Clear notification being processed.
  consume_settings_pending();

  if (settings_changed() || static_settings_changed()) {
    // Request shutdown of the Disk Forecaster while holding the processing
    // mutex. If the Disk Forecaster is waiting for this mutex, it will see the
    // stop request before starting another forecasting cycle.
    request_thread_stop();
    processing_guard.unlock();
    // A concurrent SET can change the run state while join_thread() waits for
    // the Disk Forecaster thread to exit.
    join_thread();
    set_params();
    // Use the run state applied by set_params(). A later SET will remain
    // pending for the next pass.
    const bool next_run_state = m_running;
    initialize_sample_timers();
    log_collector_status(std::string("Settings"), true);
    if (next_run_state) {
      start_thread();
    }
  } else if (dynamic_settings_changed()) {
    set_params();
    log_collector_status(std::string("Settings (live)"), true);
  }
  return true;
}

// clang-format off

/**
  Normal processing cycle.
  Get disk metrics every minute, generate forecasts, issue forecast reports.
*/
void Disk_Forecaster::process_normal_cycle() {
  if (!m_disk_input_available.load(std::memory_order_acquire)) {
    m_first_normal_cycle = true;
    return;
  }

  if (!disk_metric_buffer()->writeable()) {
    Disk_Metric record;
    {
      // Lock the metric buffer and model to guard against filesystem changes
      rwlock_guard metric_lock(disk_metric_buffer()->get_rwlock(), Lock_Read, __FILE__, __LINE__);
      const auto count = disk_metric_buffer()->count();
      const bool has_record = (count != 0) &&
                              (disk_metric_buffer()->read_no_lock(record, count-1) == 0);
      rwlock_guard model_lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__, __LINE__);
      if (has_record) update_storage_totals(record);
      // For reporting
      set_total_metric_count(disk_metric_buffer()->total_written());
    }
  }

  // Enable disk metric writes from the Disk Collector
  if (sample_timer().expired()) {
    disk_metric_buffer()->set_writeable(true);
  }

  // The first process call occurs immediately and represents no elapsed time
  if (m_first_normal_cycle) {
    m_first_normal_cycle = false;
  } else {
    advance_cycle_timers();
  }

  // Generate forecasts if the forecast interval has elapsed or if
  // the monitored filesystem capacity changed
  bool force_immediate_reports{false};
  const bool forecast_timer_expired = forecast_timer().expired();
  if (forecast_timer_expired || volume_size_changed()) {
    if (!ready_to_forecast(true)) {
      return;
    }

    // Pause writes from the Disk Collector while processing metrics
    bool writeable_save{disk_metric_buffer()->writeable()};
    disk_metric_buffer()->set_writeable(false);

    // Block external DML, e.g. TRUNCATE
    if (disk_metric_buffer()->lock_read() != 0) {
      disk_metric_buffer()->set_writeable(writeable_save);
      return;
    }

    // Update the forecast model with the most recent metrics then generate the
    // forecasts
    std::vector<Disk_Forecast> forecasts;
    Disk_Forecast_Summary summary;
    if (generate_forecast(forecasts, summary) == 0) {
      update_forecast_tables(forecasts, summary);
      // Save the last forecast for reporting and status
      save_forecast_status(forecasts, summary);
      // Issue reports now, override the report timers
      if (volume_size_changed()) {
        force_immediate_reports = true;
        set_volume_size_changed(false);
      }
    }

    // Allow external DML
    disk_metric_buffer()->unlock();

    // Restore writeable state
    disk_metric_buffer()->set_writeable(writeable_save);

    if (forecast_timer_expired) {
      forecast_timer().reset();
    }
  }

  // Write disk usage summary and most recent forecasts to the error log
  const bool status_timer_expired = short_report_timer().expired();
  if (status_timer_expired) {
    short_report_timer().reset();
  }
  if (status_timer_expired || force_immediate_reports) {
    auto snapshot = status_snapshot();
    log_forecast_status(snapshot.forecasts, snapshot.summary);
  }

  // Write forecast report to diagnostic log, ignore if HA secondary
  const bool report_timer_expired = report_timer().expired();
  if (report_timer_expired || force_immediate_reports) {
    if (Server::is_group_replication_secondary()) {
      return;
    }
    auto snapshot = status_snapshot();
    generate_report(snapshot.forecasts, snapshot.summary);
    if (report_timer_expired) {
      report_timer().reset();
    }
  }
}

/**
  Issue a status message after stopping the collection cycle.
*/
void Disk_Forecaster::post_process() { log_collector_status("Stopped", false); }

// -----------------------------------------------------------------------------
// Data buffers
// -----------------------------------------------------------------------------

/**
  Resize the disk metric time series buffer if sample interval changes.
  Must be inside collector lock.
*/
void Disk_Forecaster::set_disk_metric_buffer_size() {
  unsigned int new_buffer_size = m_retention / sample_interval();
  // Default is 14 days = 20,160 records
  if (new_buffer_size == 0) {
    new_buffer_size = 1;
  }
  // New buffer size
  if (new_buffer_size != disk_metric_buffer()->max_size()) {
    disk_metric_buffer()->resize(new_buffer_size);
  }
}

/**
  Set the disk metric aggregated buffer size to include rows for all 3
  aggregation tiers.
  Must be inside collector lock.
*/
void Disk_Forecaster::set_disk_metric_aggregate_buffer_size() {
  size_t total_rows{k_tier_1_rows + k_tier_2_rows + k_tier_3_rows};
  disk_metric_aggregate_buffer()->resize(total_rows);
}

/**
  Set the sizes for  the disk forecast and disk forecast summary buffers.
  Must be inside collector lock.
*/
void Disk_Forecaster::set_disk_forecast_buffer_size() {
  size_t max_forecasts{k_forecast_retention_seconds /
                       k_forecast_interval_seconds};
  // Each forecast has 3 records (1h, 24h, 7d)
  size_t forecast_rows{max_forecasts * 3};
  disk_forecast_buffer()->resize(forecast_rows);
  // One forecast summary per forecast
  disk_forecast_summary_buffer()->resize(max_forecasts);
}

// clang-format off

/**
  Indicate if the dynamic Disk Forecast settings have changed.
  Dynamic settings can be applied without restarting the collector thread.
  @return true if settings have changed
*/
bool Disk_Forecaster::dynamic_settings_changed() {
  std::scoped_lock settings_guard{m_settings_mutex};

  m_dynamic_settings_changed =
      (System_vars::disk_forecast_run_mode().val != static_cast<unsigned long>(m_run_mode) ||
       System_vars::disk_forecast_test_filename().val != m_test_filename);
  return m_dynamic_settings_changed;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

/**
  Indicate if the static Disk Forecast settings have changed.
  Changes to static settings require that the collector thread be restarted.
  @return true if settings have changed
*/
bool Disk_Forecaster::static_settings_changed() {
  m_static_settings_changed =
      (System_vars::status_interval().val != m_status_interval ||
       System_vars::disk_forecast_running().val != m_running ||
       configuration::report_interval_seconds(
           System_vars::disk_forecast_report_interval().val) !=
           m_report_interval_seconds ||
       System_vars::disk_forecast_sample_interval().val != sample_interval() ||
       System_vars::disk_monitored().val != m_configured_volume ||
       (System_vars::disk_monitored().val[0] == '\0' &&
        System_vars::disk_monitored_generation() != m_volume_generation));
  return m_static_settings_changed;
}

/**
  Set collection parameters from the system variables.
*/
void Disk_Forecaster::set_params() {
  std::scoped_lock settings_guard{m_settings_mutex};

  if (m_running != System_vars::disk_forecast_running().val) {
    // The only way to clear the test state is to set disk_forecast_running
    set_test_state(Dfx_Test_State::IDLE);
  }
  m_running = System_vars::disk_forecast_running().val;
  m_configured_volume = System_vars::disk_monitored().val;
  if (!System_vars::select_disk_monitored_path(
          m_configured_volume, m_volume, m_volume_generation)) {
    m_volume.clear();
  }

  // Base interval: How often the thread wakes up (default 10s)
  // Sample interval: How often to collect disk metrics (default 60s)
  set_collector_intervals(k_disk_forecast_base_interval_seconds,
                          System_vars::disk_forecast_sample_interval().val);

  // Time series retention is not configurable, set here for clarity
  m_retention = k_metric_retention_seconds; // default 14 days

  // Restart the report timer if the report interval has changed
  const int report_interval_seconds = configuration::report_interval_seconds(
      System_vars::disk_forecast_report_interval().val);
  if (m_report_interval_seconds != report_interval_seconds) {
    m_report_interval_seconds = report_interval_seconds;
    report_timer().set_timeout(m_report_interval_seconds);
    report_timer().reset();
    // Timer interval remains base_interval()
  }
  m_status_interval = System_vars::status_interval().val;

  auto new_mode = static_cast<Dfx_Run_Mode>(System_vars::disk_forecast_run_mode().val);
  // The new run mode will execute in the next process cycle
  if (m_run_mode != new_mode) {
    set_run_mode(new_mode);
  }
  m_test_filename = System_vars::disk_forecast_test_filename().val;
}

// clang-format on

/**
  Return health_monitor.disk_forecast_running.
*/
bool Disk_Forecaster::running() const {
  return System_vars::disk_forecast_running().val;
}

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------

std::string Disk_Forecaster::format_settings() {
  std::scoped_lock settings_guard{m_settings_mutex};

  std::stringstream profile;
  const std::string eq{"="};
  profile << "running" << eq << on_off(running());
  profile << ", volume" << eq << "'" << volume() << "'";
  profile << ", base interval" << eq << base_interval() << "s";
  profile << ", sample interval" << eq << sample_interval() << "s";
  profile << ", report interval" << eq << report_interval() << "m";
  profile << ", run mode" << eq << run_mode_name();
  profile << ", test filename" << eq << test_filename();
  return profile.str();
}

std::string Disk_Forecaster::format_status_info() {
  const auto snapshot = status_snapshot();
  std::stringstream profile;
  profile << "running" << eq << on_off(running());
  profile << ", volume" << eq << "'" << volume() << "'";
  profile << ", used" << eq << format_mb(snapshot.used_mb, -1, true);
  profile << ", total" << eq << format_mb(snapshot.total_mb, -1, true);
  profile << std::setprecision(1) << std::fixed;
  profile << ", used" << eq << snapshot.used_pct << "%%";
  profile << ", metrics" << eq << snapshot.metric_count;
  // profile << ", trend" << eq << "stable";  // m_trend; // TODO
  profile << ", last forecast" << eq;
  if (snapshot.last_forecast_time == 0) {
    profile << "n/a";
  } else {
    profile << fmt::format_time(snapshot.last_forecast_time);
  }
  return profile.str();
}

/**
  Informational message with collector state and optional forecast status.
*/
void Disk_Forecaster::log_info_message(const std::string &msg, bool levels) {
  log_info("%s", build_resource_message("", "", msg, levels).c_str());
}

/**
  Informational message with collector state and forecast status.
*/
void Disk_Forecaster::log_info_message(const std::string &msg) {
  log_info_message(msg, true);
}

/**
  Collector or forecast status.
*/
void Disk_Forecaster::log_resource_status() { log_collector_status(true); }

/**
  Forecast status, no message.
*/
void Disk_Forecaster::log_resource_levels() { log_info_message("", true); }

}  // namespace hm

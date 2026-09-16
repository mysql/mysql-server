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

#pragma once

#include <atomic>
#include <mutex>

#include "fx_holt_winters_forecaster.h"
#include "fx_sample.h"
#include "health_monitor_collector.h"
#include "health_monitor_constants.h"
#include "health_monitor_disk_bits.h"
#include "health_monitor_metric_buffer.h"

namespace hm {

struct Disk_forecast_reset_context;

class Disk_Forecaster : public Collector {
 public:
  struct Status_snapshot {
    uint64_t total_mb{0};
    uint64_t used_mb{0};
    double used_pct{0.0};
    size_t metric_count{0};
    time_t last_forecast_time{0};
    std::vector<Disk_Forecast> forecasts;
    Disk_Forecast_Summary summary;
  };

  Disk_Forecaster();
  virtual ~Disk_Forecaster() override { mysql_rwlock_destroy(&m_model_lock); }

  // Disable copy and move operations
  Disk_Forecaster(const Disk_Forecaster &) = delete;
  Disk_Forecaster &operator=(const Disk_Forecaster &) = delete;
  Disk_Forecaster(Disk_Forecaster &&) = delete;
  Disk_Forecaster &operator=(Disk_Forecaster &&) = delete;

  // ---------------------------------------------------------------------------
  // Operational Mode and State Definitions
  // ---------------------------------------------------------------------------
  // A note on terminology:
  // - mode:  Refers to a configuration or operational regime that persists
  //          until changed by explicit action.
  // - state: Refers to the current condition or status of a process, and can
  //          change as a result of internal events.
  // ---------------------------------------------------------------------------

  // Run_Mode - Disk Forecaster operational mode.
  // Configurable as system variable.
  enum class Dfx_Run_Mode {
    NONE,    // system not initialized
    NORMAL,  // active normal operation, collecting live data (default)
    TEST,    // run unit test from preloaded disk metrics
    STANDBY  // forecaster thread is alive but not processing data
  };
  static const std::array<std::string, 4> RUN_MODE_NAMES;

  // Test_State - Current state of the test process, distinct from the run mode.
  // Set internally, visible as status variable.
  enum class Dfx_Test_State {
    NONE,    // no test is running or configured (default)
    IDLE,    // no test is running, ready to accept a new test command
    RUNNING  // test currently executing, processing input test data
  };
  static const std::array<std::string, 3> TEST_STATE_NAMES;

  // For system variable registration
  static const char *dfx_run_mode_names[5];  // 4 names + nullptr

 private:
  // Forecaster status variables
  static char m_dfx_run_mode_name[32];    // disk_forecaster_run_mode
  static char m_dfx_test_state_name[32];  // disk_forecaster_test_state

  // ---------------------------------------------------------------------------
  // Process
  // ---------------------------------------------------------------------------
 public:
  void initialize();
  void initialize_timers();
  void advance_cycle_timers();
  // Metric collection and forecasting
  void pre_process() override;
  void process_normal_cycle();
  void process() override;
  void post_process() override;

  void set_disk_input_available(bool available) {
    m_disk_input_available.store(available, std::memory_order_release);
  }

  // ---------------------------------------------------------------------------
  // Operational modes
  // ---------------------------------------------------------------------------
  [[nodiscard]] Dfx_Run_Mode run_mode() const { return m_run_mode; }
  [[nodiscard]] bool is_new_run_mode() const { return m_is_new_run_mode; }
  [[nodiscard]] Dfx_Test_State test_state() const { return m_test_state; }

  void set_run_mode(Dfx_Run_Mode mode);
  void set_is_new_run_mode(bool is_new) { m_is_new_run_mode = is_new; }
  void set_test_state(Dfx_Test_State state);

  const std::string &run_mode_name();
  const std::string &test_state_name();

  // Accessor for status variable 'disk_forecaster_run_mode'
  [[nodiscard]] const char *run_mode_name_ptr() const {
    return m_dfx_run_mode_name;
  }
  // Accessor for status variable 'disk_forecaster_test_state'
  [[nodiscard]] const char *test_state_name_ptr() const {
    return m_dfx_test_state_name;
  }

  // Forecast depletion ETA classifications
  static const std::array<std::string, 4> FORECAST_STATUS_NAMES;
  const std::string &forecast_status_name(Dfx_Status status);

 private:
  Dfx_Run_Mode m_run_mode{Dfx_Run_Mode::NORMAL};
  bool m_is_new_run_mode{false};
  Dfx_Test_State m_test_state{Dfx_Test_State::NONE};
  std::string m_test_filename;  // disk metrics input file

  // ---------------------------------------------------------------------------
  // Settings
  // ---------------------------------------------------------------------------
 public:
  [[nodiscard]] bool running() const override;
  [[nodiscard]] int report_interval() const {
    return m_report_interval_seconds / 60;
  }
  std::string test_filename() { return m_test_filename; }
  std::string volume() { return m_volume; }  // from Disk Collector
  bool dynamic_settings_changed() override;
  bool static_settings_changed() override;
  void set_params() final;
  bool update_settings() override;

 private:
  // Prevents settings application from blocking the Supervisor during a cycle.
  std::mutex m_processing_mutex;
  // Serializes processing with live settings checks and updates.
  std::mutex m_settings_mutex;
  bool m_static_settings_changed{false};
  bool m_dynamic_settings_changed{false};

  // ---------------------------------------------------------------------------
  // Data buffers
  // ---------------------------------------------------------------------------
  void set_disk_metric_buffer_size();
  void set_disk_metric_aggregate_buffer_size();
  void set_disk_forecast_buffer_size();

 public:
  Disk_Metric_Buffer disk_metric_buffer() { return m_disk_metric_buffer; }
  Disk_Metric_Aggregate_Buffer disk_metric_aggregate_buffer() {
    return m_disk_metric_aggregate_buffer;
  }
  Disk_Forecast_Buffer disk_forecast_buffer() { return m_disk_forecast_buffer; }
  Disk_Forecast_Summary_Buffer disk_forecast_summary_buffer() {
    return m_disk_forecast_summary_buffer;
  }

 private:
  Disk_Metric_Buffer m_disk_metric_buffer;
  Disk_Metric_Aggregate_Buffer m_disk_metric_aggregate_buffer;
  Disk_Forecast_Buffer m_disk_forecast_buffer;
  Disk_Forecast_Summary_Buffer m_disk_forecast_summary_buffer;

 protected:
  // ---------------------------------------------------------------------------
  // Logging
  // ---------------------------------------------------------------------------
  void log_info_message(const std::string &msg, bool levels);
  void log_info_message(const std::string &msg);
  void log_resource_status() override;
  void log_resource_levels() override;

  std::string format_settings() override;
  std::string format_status_info() override;

  // ---------------------------------------------------------------------------
  // Timing and interval settings
  // ---------------------------------------------------------------------------
  Cycle_Timer &forecast_timer() { return m_forecast_timer; }
  Cycle_Timer &report_timer() { return m_report_timer; }
  Cycle_Timer &short_report_timer() { return m_short_report_timer; }

 private:
  Cycle_Timer m_forecast_timer;
  Cycle_Timer m_report_timer;
  Cycle_Timer m_short_report_timer;
  bool m_first_normal_cycle{true};

 public:
  [[nodiscard]] static constexpr int default_sample_interval() {
    return k_time_series_interval_seconds;
  }
  [[nodiscard]] static constexpr int default_report_interval() {
    return k_report_interval_seconds;
  }

 private:
  // Base interval (internal, fixed)
  //   - frequency that the Collector calls process(), in seconds
  //   - not configurable
  static constexpr int k_disk_forecast_base_interval_seconds{10};

  // Time series interval (internal, configurable, aka sample_interval)
  //   - frequency of storing time series samples, in seconds
  //   - always a multiple of the base interval
  //   - default: 60 seconds
  static constexpr int k_time_series_interval_seconds{60};

  // Data retention interval (internal, fixed) - Disk_Metric table
  //   - duration raw time series records are retained, in seconds
  //   - default: 14 days
  //   - rows: 14 days * 1440/day = 20,160 disk metrics
  //   - size: 20,160 metrics * 24 bytes/metric = 484 KB
  static constexpr size_t k_metric_retention_seconds{seconds_per_day * 14};

  // Forecast interval (internal, fixed)
  //   - frequency that forecasts are made
  //   - default: 1 hour
  static constexpr int k_forecast_interval_seconds{seconds_per_hour};
  int m_forecast_interval_seconds{k_forecast_interval_seconds};

  // Forecast report interval (configurable)
  //   - frequency that the forecast report is written to the diagnostics log
  //   - default 4 hours
  static constexpr int k_report_interval_seconds{seconds_per_hour * 4};
  int m_report_interval_seconds{k_report_interval_seconds};

  // Forecast status interval (internal, fixed)
  // - Frequency that forecast messages are written to the error log
  // - Default 1 hour
  int m_short_report_interval_seconds{seconds_per_hour};

  // Disk Collector input availability for NORMAL mode
  std::atomic<bool> m_disk_input_available{false};

  // Forecast record retention (internal, fixed)
  //   - how long forecast records are retained in the Disk_Forecast table
  //   - default: 14 days @ 1 forecast per hour
  //   - rows: 14 days * 24 fx/day * 3 (1h/1d/7d) = 1008 records
  //   - size: 1008 records * 52 bytes/record = 52 KB
  static constexpr int k_forecast_retention_seconds{seconds_per_day * 14};

  // ---------------------------------------------------------------------------
  // Data aggregation
  // ---------------------------------------------------------------------------
  // M = 1440 minutes per day
  // W = aggregation window in minutes (5, 15 or 60)
  //                                        Intervals/Day
  // | Aggregation Tier       | Days/Tier |     (M/W)    | Rows = Days × M/W |
  // |------------------------|-----------|--------------|-------------------|
  // | 5-minute (Days 8–30)   |     23    |     288      |  23 × 288 = 6,624 |
  // | 15-minute (Days 31–90) |     60    |      96      |  60 ×  96 = 5,760 |
  // | 1-hour (Days 91–365)   |    275    |      24      | 275 ×  24 = 6,600 |
  // |                                                        Total = 18,984 |
  static constexpr int k_tier_1_days{23};
  static constexpr int k_tier_2_days{60};
  static constexpr int k_tier_3_days{275};
  static constexpr int k_tier_1_rows{minutes_per_day / k_tier_1_days};
  static constexpr int k_tier_2_rows{minutes_per_day / k_tier_2_days};
  static constexpr int k_tier_3_rows{minutes_per_day / k_tier_3_days};

  // Current storage in MiB
  uint64_t m_total_mb{};            // filesystem capacity
  uint64_t m_last_total_mb{};       // to detect if volume changed
  uint64_t m_used_mb{};             // current used space, MiB
  double m_used_pct{};              // current used space, percent of total
  std::string m_configured_volume;  // configured monitored path
  std::string m_volume;             // effective monitored path
  uint64_t m_volume_generation{0};
  bool m_volume_size_changed{};  // true if volume size changed

  [[nodiscard]] bool volume_size_changed() const {
    rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Read, __FILE__,
                      __LINE__);
    return m_volume_size_changed;
  }
  void set_volume_size_changed(bool change) {
    rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__,
                      __LINE__);
    m_volume_size_changed = change;
  }

  // ---------------------------------------------------------------------------
  // Forecasting
  // ---------------------------------------------------------------------------
  Holt_Winters_Forecaster &forecaster() { return m_forecaster; }
  Dfx_Status compute_status(double current_usage_pct, bool has_depletion_eta,
                            double days_to_full, double usage_rate_pct_per_day);
  Disk_Forecast_Summary compute_summary(const Holt_Winters_State &model,
                                        double volume_size_mb,
                                        double current_usage_mb);
  int generate_forecast(std::vector<Disk_Forecast> &disk_forecasts,
                        Disk_Forecast_Summary &summary);
  bool ready_to_forecast(bool log_warning = true);
  void update_forecast_tables(std::vector<Disk_Forecast> &forecasts,
                              Disk_Forecast_Summary &summary);
  void update_storage_totals(const Disk_Metric &disk_metric);

 public:
  void reset_filesystem_series();
  static void reset_filesystem_series(Disk_forecast_reset_context &context);
  Status_snapshot status_snapshot() const;

 private:
  void set_total_metric_count(size_t metric_count) {
    m_total_metric_count = metric_count;
  }
  void save_forecast_status(const std::vector<Disk_Forecast> &forecasts,
                            const Disk_Forecast_Summary &summary) {
    rwlock_guard lock(Disk_Forecaster::model_lock(), Lock_Write, __FILE__,
                      __LINE__);
    m_last_forecasts = forecasts;
    m_last_forecast_summary = summary;
    m_last_forecast_time = summary.timestamp;
  }
  static mysql_rwlock_t *model_lock();

 private:
  Holt_Winters_Forecaster m_forecaster;  // forecast model
  std::mutex m_series_mutex;             // serializes time series operations

  // Disk forecaster locking scheme.
  //
  // m_series_mutex serializes operations that act on the complete time series,
  // such as a filesystem reset and running in TEST mode.
  //
  // The metric buffer R/W lock protects metric rows, counters and logical
  // positions. The model R/W lock protects the Holt-Winters model and
  // the reporting state. Forecast and summary buffers use their own locks.
  //
  // Acquire locks in this order:
  //   m_series_mutex
  //   metric buffer R/W lock
  //   forecast model R/W lock
  //
  // - The metric and model locks block forecasting during a filesystem reset.
  // - The m_series_mutex prevents a reset from interrupting TEST mode.
  // - Functions with a `_no_lock` suffix require the caller to hold the
  //   applicable lock and must not acquire it again.

  static mysql_rwlock_t m_model_lock;  // R/W lock for the forecast model
  size_t m_total_metric_count{};
  time_t m_last_forecast_time{};
  std::vector<Disk_Forecast> m_last_forecasts;  // most recent set of forecasts
  Disk_Forecast_Summary m_last_forecast_summary;  // and summary for reporting

  // ---------------------------------------------------------------------------
  // Reporting
  // ---------------------------------------------------------------------------
 protected:
  std::string generate_report(std::vector<Disk_Forecast> &forecasts,
                              Disk_Forecast_Summary &summary);
  void log_forecast_status(std::vector<Disk_Forecast> &forecasts,
                           Disk_Forecast_Summary &summary);
  std::string format_disk_forecast(const Disk_Forecast &dfx);
  std::string format_forecast_summary(const Disk_Forecast_Summary &dfxs);

  // ---------------------------------------------------------------------------
  // Testing
  // ---------------------------------------------------------------------------
  int initialize_test();
  int process_test_cycle();
  int load_unit_test_data();
  int read_unit_test_file(std::vector<Disk_Metric> &disk_metrics);
  void run_unit_test();

  // clang-format off

  // ---------------------------------------------------------------------------
  // Instrumentation
  // ---------------------------------------------------------------------------
  // Mutexes
  PSI_rwlock_key m_metric_rwlock_key{};
  PSI_rwlock_key m_aggregate_rwlock_key{};
  PSI_rwlock_key m_forecast_rwlock_key{};
  PSI_rwlock_key m_forecast_summary_rwlock_key{};
  PSI_rwlock_key m_forecast_model_rwlock_key{};
  PSI_rwlock_info m_rwlock_info[5] = {
      {&m_metric_rwlock_key, "disk_forecaster_metric", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING, PSI_DOCUMENT_ME},
      {&m_aggregate_rwlock_key, "disk_forecaster_aggregate", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING, PSI_DOCUMENT_ME},
      {&m_forecast_rwlock_key, "disk_forecaster_forecast", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING, PSI_DOCUMENT_ME},
      {&m_forecast_summary_rwlock_key, "disk_forecaster_forecast_summary", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING, PSI_DOCUMENT_ME},
      {&m_forecast_model_rwlock_key, "disk_forecaster_forecast_model", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING, PSI_DOCUMENT_ME}
  };
  // Memory
  PSI_memory_key m_metric_memory_key{};
  PSI_memory_key m_aggregate_memory_key{};
  PSI_memory_key m_forecast_memory_key{};
  PSI_memory_key m_forecast_summary_memory_key{};
  PSI_memory_info m_memory_info[4] = {
      {&m_metric_memory_key, "disk_forecaster_metric", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor disk metrics buffer"},
      {&m_aggregate_memory_key, "disk_forecaster_aggregate", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor aggregated metrics buffer"},
      {&m_forecast_memory_key, "disk_forecaster_forecast", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor forecast buffer"},
      {&m_forecast_summary_memory_key, "disk_forecaster_forecast_summary", PSI_FLAG_ONLY_GLOBAL_STAT, PSI_VOLATILITY_PROVISIONING, "Health Monitor forecast summary buffer"}
  };

  // clang-format on
};

}  // namespace hm

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

#include <optional>

#include "health_monitor_collector.h"
#include "health_monitor_disk_bits.h"
#include "health_monitor_disk_forecaster.h"
#include "health_monitor_disk_state.h"
#include "health_monitor_filesystem_sample.h"
#include "health_monitor_server.h"
#include "health_monitor_timer.h"

namespace hm {
// clang-format off

/**
  Fallback State

  "Fallback" is the process of fencing the server when disk space is critically
  low. The Fallback_State class manages the state of the fallback process, such
  as tracking whether the Health Monitor set or attempted to set SUPER_READ_ONLY
  and/or OFFLINE_MODE.

  The fallback state does not indicate the actual value of the system variables,
  only whether or not the Health Monitor set them. This allows the Health
  Monitor to detect when the system variables are changed externally and sync
  with the server state.
*/
class Fallback_State {
 public:
  Fallback_State() = default;

  void reset() {
    m_offline_mode_was_set_by_hm = false;
    m_super_read_only_was_set_by_hm = false;
    m_super_read_only_retry_failed = false;
    m_super_read_only_force_failed = false;
    m_force_attempted = false;
    m_last_reason.clear();
    m_last_state = nullptr;
  }

  void hard_reset() {
    reset();
    m_count = 0;
  }

  [[nodiscard]] bool enabled() const { return m_enabled; }
  void set_enabled(bool value) { m_enabled = value; }

  [[nodiscard]] bool force_enabled() const { return m_force_enabled; }
  void set_force_enabled(bool value) { m_force_enabled = value; }

  // The server is considered fenced if the Health Monitor set either
  // SUPER_READ_ONLY or OFFLINE_MODE.
  [[nodiscard]] bool fenced() const {
    return m_offline_mode_was_set_by_hm || m_super_read_only_was_set_by_hm;
  }

  [[nodiscard]] bool offline_mode_was_set_by_hm() const { return m_offline_mode_was_set_by_hm; }
  bool set_offline_mode_was_set_by_hm(bool value) {
    m_offline_mode_was_set_by_hm = value;
    return m_offline_mode_was_set_by_hm;
  }

  [[nodiscard]] bool super_read_only_was_set_by_hm() const { return m_super_read_only_was_set_by_hm; }
  bool set_super_read_only_was_set_by_hm(bool value) {
    m_super_read_only_was_set_by_hm = value;
    return m_super_read_only_was_set_by_hm;
  }

  [[nodiscard]] bool super_read_only_retry_failed() const {
    return m_super_read_only_retry_failed;
  }
  void set_super_read_only_retry_failed(bool value) {
    m_super_read_only_retry_failed = value;
  }

  [[nodiscard]] bool super_read_only_force_failed() const {
    return m_super_read_only_force_failed;
  }
  void set_super_read_only_force_failed(bool value) {
    m_super_read_only_force_failed = value;
  }

  [[nodiscard]] bool force_attempted() const { return m_force_attempted; }
  void set_force_attempted(bool value) { m_force_attempted = value; }

  [[nodiscard]] int count() const { return m_count; }
  int &count() { return m_count; }  // support ++/--
  void set_count(int value) { m_count = value; }

  std::string &last_reason() { return m_last_reason; }  // support '='
  [[nodiscard]] const std::string &last_reason() const { return m_last_reason; }
  void set_last_reason(const std::string &value) { m_last_reason = value; }

  [[nodiscard]] const ds::Storage_State *last_state() const { return m_last_state; }
  void set_last_state(ds::Storage_State *state) { m_last_state = state; }

 private:
  bool m_enabled{false};                       // system variable 'disk_fallback_enable'
  bool m_force_enabled{false};                 // system variable 'disk_fallback_force_enable'
  bool m_offline_mode_was_set_by_hm{false};    // true if the Health Monitor set OFFLINE_MODE
  bool m_super_read_only_was_set_by_hm{false}; // true if the Health Monitor set SUPER_READ_ONLY
  bool m_super_read_only_retry_failed{false};  // true if unable to set SUPER_READ_ONLY (unforced)
  bool m_super_read_only_force_failed{false};  // true if unable to set SUPER_READ_ONLY (forced)
  bool m_force_attempted{false};               // true if SUPER_READ_ONLY (forced) was attempted
  std::string m_last_reason;                   // last reason why server is ineligible for fencing
  ds::Storage_State *m_last_state{nullptr};    // last state the reason was printed
  int m_count{0};                              // the number of times the server was fenced
};

/**
  Recovery State

  "Recovery" is the process of unfencing the server and restoring normal
  operating mode by setting SUPER_READ_ONLY = OFF and OFFLINE_MODE = OFF.
  The recovery process is triggered when storage levels rise above the
  configured recovery level after the server has been fenced.

  The Recovery_State class manages the state of the recovery process, such as
  tracking the recovery timers and the number of recovery cycles.
*/
class Recovery_State {
 public:
  Recovery_State() = default;

  void reset() {
    m_in_progress = false;
    m_ineligible_warning = false;
    m_offline_warning = false;
    m_cycle_warning = false;
  }

  void hard_reset() {
    reset();
    m_count = 0;
    m_expiration = 0;
  }

  [[nodiscard]] bool enabled() const { return m_enabled; }
  void set_enabled(bool value) { m_enabled = value; }

  [[nodiscard]] int level_pct() const { return m_level_pct; }
  void set_level_pct(int value) { m_level_pct = value; }

  [[nodiscard]] int time_1() const { return m_time_1; }
  void set_time_1(int value) { m_time_1 = value; }

  [[nodiscard]] int time_2() const { return m_time_2; }
  void set_time_2(int value) { m_time_2 = value; }

  [[nodiscard]] int max_cycles() const { return m_max_cycles; }
  void set_max_cycles(int value) { m_max_cycles = value; }

  [[nodiscard]] int increment() const { return m_increment; }
  void set_increment(int value) { m_increment = value; }

  [[nodiscard]] uint64_t window() const { return m_window; }
  void set_window(uint64_t value) { m_window = value; }

  [[nodiscard]] uint64_t level_mb() const { return m_level_mb; }
  void set_level_mb(uint64_t value) { m_level_mb = value; }

  int count() const { return m_count; }
  int &count() { return m_count; }  // support ++/--
  void set_count(int value) { m_count = value; }

  [[nodiscard]] bool in_progress() const { return m_in_progress; }
  void set_in_progress(bool value) { m_in_progress = value; }

  [[nodiscard]] bool ineligible_warning() const { return m_ineligible_warning; }
  void set_ineligible_warning(bool value) { m_ineligible_warning = value; }

  [[nodiscard]] bool offline_warning() const { return m_offline_warning; }
  void set_offline_warning(bool value) { m_offline_warning = value; }

  [[nodiscard]] bool cycle_warning() const { return m_cycle_warning; }
  void set_cycle_warning(bool value) { m_cycle_warning = value; }

  [[nodiscard]] time_t expiration() const { return m_expiration; }
  void set_expiration(time_t value) { m_expiration = value; }

  // Public for use as status variable 'disk_recovery_count'
  int m_count{0};  // number of times recovery timer 1 was started

 private:
  // System variables
  bool m_enabled{false}; // system variable 'disk_recovery_enable'
  int m_level_pct{0};    // system variable 'disk_level_pct', percent above reserve level to set OFFLINE_MODE = OFF
  int m_time_1{0};       // system variable 'disk_recovery_time_1', seconds before setting OFFLINE_MODE = OFF
  int m_time_2{0};       // system variable 'disk_recovery_time_2', seconds before setting SUPER_READ_ONLY = OFF
  int m_max_cycles{0};   // system variable 'disk_recovery_max_cycles', maximum allowed recovery cycles
  int m_increment{0};    // system variable 'disk_recovery_increment', seconds to increase recovery timer each cycle
  uint64_t m_window{0};  // system variable 'disk_recovery_window', lifetime of the recovery cycle count
  // Recovery state
  uint64_t m_level_mb{0};           // min MiB above reserve level to restore normal operating mode
  bool m_in_progress{false};        // true if either recovery timer is running
  bool m_ineligible_warning{false}; // true if server ineligible for recovery warning issued
  bool m_offline_warning{false};    // true if OFFLINE_MODE warning issued
  bool m_cycle_warning{false};      // true if recovery cycles exhausted warning issued
  time_t m_expiration{0};           // the end of the recovery cycle window
};

/**
  Disk Collector

  The Disk Collector monitors space on the filesystem path specified by 
  `health_monitor.disk_monitored`. It collects filesystem usage, detects
  filesystem and capacity changes, publishes samples and supplies disk metrics
  to the Disk Forecaster.

  The collector classifies available space as normal, warning, low or critical.
  When fencing is enabled, low and critical states can cause the collector to
  set OFFLINE_MODE and SUPER_READ_ONLY. The collector records which modes it
  set so that when disk space recovers, the Health Monitor clears only changes
  that it made.

  If the monitored path or filesystem is unavailable, then the Disk Collector
  suspends sample collection and retries on subsequent collection cycles.
*/
class Disk_Collector : public Collector {
 public:
  Disk_Collector();
  void initialize();

 private:
  void initialize_timers();
  void advance_timers();

 protected:
  // Sample collection
  void pre_process() override;
  void process() override;
  void post_process() override;

 public:
  // Settings
  void set_buffer_size() final;
  bool settings_changed() override;
  void set_params() final;
  [[nodiscard]] bool running() const override;

  // Storage info
  std::shared_ptr<Stat_Buffer<Disk_Info>> buffer() { return m_buffer; }
  int get_disk_info(const std::string &target_path, Disk_Info &disk_info,
                    Filesystem_sample_state &sample_state,
                    Filesystem_change &change);
  bool resolve_monitored_path();
  void update_disk_info();
  void update_per_sample_totals(const Disk_Info &disk_info);
  void set_storage_profile(const Disk_Info &disk_info);

  // Fallback
  Fallback_State &fallback() { return m_fallback; }
  Mode_Timer &fallback_timer() { return m_fallback_timer; }
  void start_fallback_timer();
  void stop_fallback_timer();
  void cancel_fallback_timer();

  bool fallback_eligible(std::string &reason);
  bool fallback_eligible(bool print_reason = false);
  bool forced_fallback_eligible();
  void set_fallback_mode(bool force);
  void clear_fallback_mode();
  bool check_fenced_server_state();
  void process_expired_fallback_timer();
  void reset_fallback_and_recovery();
  void cancel_fallback_process();

  bool set_offline_mode(int max_retries, bool &already_set);
  bool clear_offline_mode();
  bool set_super_read_only(int max_retries, bool &already_set);
  bool clear_super_read_only(int max_retries);

  bool terminate_and_retry(const Processlist_Ids &plist_ids, int group_size,
                           bool query_only);
  bool terminate_queries(const Processlist_Ids &plist_ids, int group_size) {
    return terminate_and_retry(plist_ids, group_size, true);
  }
  bool terminate_connections(const Processlist_Ids &plist_ids, int group_size) {
    return terminate_and_retry(plist_ids, group_size, false);
  }

  // Recovery
  Recovery_State &recovery() { return m_recovery; }
  bool recovery_eligible(std::string &reason);
  void check_recovery_status();
  [[nodiscard]] bool above_recovery_level() const { return m_avail_mb >= m_recovery.level_mb(); }
  [[nodiscard]] bool below_recovery_level() const { return m_avail_mb < m_recovery.level_mb(); }
  void complete_recovery_process();
  void cancel_recovery_process();
  bool max_recovery_cycles();
  void set_recovery_window(bool print_status);
  void check_recovery_window();

  Mode_Timer &recovery_timer_1() { return m_recovery_timer_1; }
  void set_recovery_timeout_1();
  void start_recovery_timer_1();
  void stop_recovery_timer_1();
  void cancel_recovery_timer_1();

  Mode_Timer &recovery_timer_2() { return m_recovery_timer_2; }
  void set_recovery_timeout_2();
  void start_recovery_timer_2();
  void stop_recovery_timer_2();
  void cancel_recovery_timer_2();

  // Status variables
  bool *super_read_only_set_ptr() { return &m_status_super_read_only_disk_full; }
  bool *offline_mode_set_ptr() { return &m_status_offline_mode_disk_full; }
  [[nodiscard]] bool offline_mode_disk_full() const { return m_status_offline_mode_disk_full; }
  bool super_read_only_disk_full() const { return m_status_super_read_only_disk_full; }
  void update_super_read_only_disk_full();
  void update_offline_mode_disk_full();
  void update_fallback_status_vars();
  [[nodiscard]] std::string msg_setting_super_read_only_disk_full() const;
  [[nodiscard]] std::string msg_setting_offline_mode_disk_full() const;
  int *recovery_count_ptr() { return &m_recovery.m_count; }
  [[nodiscard]] int recovery_count() const { return m_recovery.m_count; }

  // Collector state
  static void set_state_params();
  void set_state(ds::Storage_State &new_state);
  void set_start_state() { set_state(m_start_state); }
  void set_stop_state() { set_state(m_stop_state); }
  void set_normal_state() { set_state(m_normal_state); }
  void set_warning_state() { set_state(m_warning_state); }
  void set_low_limit_state() { set_state(m_low_limit_state); }
  void set_critical_state() { set_state(m_critical_state); }
  void set_error_state() { set_state(m_error_state); }
  void check_storage_levels();

  ds::Storage_State *m_current_state{nullptr};
  ds::Start_State m_start_state;
  ds::Stop_State m_stop_state;
  ds::Normal_State m_normal_state;
  ds::Warning_State m_warning_state;
  ds::Low_Limit_State m_low_limit_state;
  ds::Critical_State m_critical_state;
  ds::Error_State m_error_state;

  // Time series collection
  void set_disk_metric_buffer(Disk_Metric_Buffer buffer,
                              Disk_Forecaster *forecaster);

  // Logging
  std::string format_settings() override;
  std::string format_status_info() override;
  std::string tag_collector_state() override;
  std::string tag_resource_state() override;
  std::string tag_mode() override;
  std::string tag_fallback();
  std::string tag_recovery();
  [[nodiscard]] std::string storage_status() const;
  std::string timer_status(Mode_Timer &timer, const std::string &msg) const;
  [[nodiscard]] std::string warning_status() const;

  void log_info_message(const std::string &msg, bool levels);
  void log_info_message(const std::string &msg);
  void log_warning_status(const std::string &msg, bool levels);
  void log_warning_status(const std::string &msg);
  void log_warning_level([[maybe_unused]] int level);
  void log_warning_level() { log_warning_level(0); }
  void log_warning_low_limit();
  void log_warning_error_condition();
  void log_filesystem_recovery(const std::string &target_path);
  void log_timer_status(Mode_Timer &timer);
  void log_timer_status(Mode_Timer &timer, bool levels);
  void log_timer_status(Mode_Timer &timer, const std::string &msg, bool levels);
  void log_volume_change();
  void log_volume_size_change();
  void log_resource_status() override;
  void log_resource_levels() override;

  // System variables
  std::string m_configured_monitored;  // configured monitored path
  std::string m_monitored;             // effective monitored path
  uint64_t m_monitored_generation{0};  // version of the monitored path
  int m_low_space_duration{0};    // seconds, fallback timer duration
  uint64_t m_low_space_level{0};  // MiB below which starts fallback timer
  int m_low_space_pct{0};         // percent of total space
  uint64_t m_low_space_critical_level{0};  // MiB below which triggers fallback mode
  int m_warn_level_1_pct{0};      // percent above reserve level, first warning level
  int m_warn_level_2_pct{0};      // percent above reserve level, second warning level
  int m_warn_level_3_pct{0};      // percent above reserve level, final warning level

  std::atomic<bool> m_settings_changed{false};

  // Storage thresholds in MiB
  uint64_t m_total_space_mb{0};      // filesystem capacity
  uint64_t m_operative_space_mb{0};  // total space - reserve
  uint64_t m_warn_level_1_mb{0};     // MiB above reserve level
  uint64_t m_warn_level_2_mb{0};
  uint64_t m_warn_level_3_mb{0};
  uint64_t m_reserve_mb{0};          // min of low space level and percent
  uint64_t m_critical_level_mb{0};
  bool m_refresh_storage_profile{false}; // true if mount point or settings change

  // Current storage levels in MiB
  uint64_t m_avail_mb{0};          // current available space
  uint64_t m_last_avail_mb{0};     // previous available space

  float m_used_pct{0};             // current used space, percent of total
  int m_delta_pct{k_delta_pct};    // pct change in available space to trigger status message
  uint64_t m_max_warn_level_mb{no_warn_level};  // top of the warning range, usually warn_level_1
  uint64_t m_last_warn_level_mb{no_warn_level}; // previous warning level
  Disk_Info m_info;                // current storage stats, raw

  // Fallback process
  Fallback_State m_fallback;       // state of the fallback process
  Mode_Timer m_fallback_timer;     // countdown timer to fallback mode

  // Recovery process
  Recovery_State m_recovery;       // state of the recovery process
  Mode_Timer m_recovery_timer_1;   // countdown to setting OFFLINE_MODE = OFF
  Mode_Timer m_recovery_timer_2;   // countdown to setting SUPER_READ_ONLY = OFF

  // Status variable 'offline_mode_disk_full'
  // True if OFFLINE_MODE was set by the Health Monitor
  bool m_status_offline_mode_disk_full{false};

  // Status variable 'super_read_only_disk_full'
  // True if SUPER_READ_ONLY was set by the Health Monitor
  bool m_status_super_read_only_disk_full{false};

  // A note on the different intervals:
  // - The base interval is the time between resource checks
  // - The sample interval is the time between stored samples, and is always
  //   a multiple of the base interval
  // - The report interval is the time between resource status reports,
  //   and is a multiple of the sample interval
  static constexpr int k_disk_base_interval{10};
  // Reporting interval while the fallback timer is running (10 sec)
  static constexpr int k_fallback_report_interval{k_disk_base_interval};
  // Reporting interval while a recovery timer is running (10 sec)
  static constexpr int k_recovery_report_interval{k_disk_base_interval};
  // Number of attempts to set SUPER_READ_ONLY
  static constexpr int k_fallback_retries{5};
  // Percent change in disk space to trigger status message
  static constexpr int k_delta_pct{10};

  const std::string k_sample_timer{"Disk Sample Timer"};
  const std::string k_fallback_mode{k_super_read_only};
  const std::string k_fallback{"Fallback"};
  const std::string k_fallback_timer{"Fallback Timer"};
  const std::string k_fallback_error{"Fallback Error"};
  const std::string k_recovery{"Recovery"};
  const std::string k_recovery_error{"Recovery Error"};
  const std::string k_recovery_timer_1{"Timer 1"};
  const std::string k_recovery_timer_2{"Timer 2"};
  const std::string k_recovery_1_mode{k_offline_mode};
  const std::string k_recovery_2_mode{k_super_read_only};
  const std::string k_normal_level{"Normal Level"};
  const std::string k_low_level{"Low Level"};
  const std::string k_warning_level{"Warning Level"};
  const std::string k_critical_level{"Critical Level"};
  const std::string k_running{"Running"};
  const std::string k_expired{"Expired"};
  const std::string k_canceled{"Canceled"};

  // Filesystem state
  Filesystem_sample_state m_filesystem_sample_state;
  Filesystem_change m_filesystem_change{Filesystem_change::initial};
  bool m_new_volume{false};  // true if the monitored disk changes
  bool m_new_size{false};    // true if the size of the current monitored disk changes
  bool m_disk_error{false};
  enum class Input_failure { filesystem, datadir_lookup };
  std::optional<std::string> m_unavailable_path;
  Input_failure m_input_failure{Input_failure::filesystem};

 private:
  std::shared_ptr<Stat_Buffer<Disk_Info>> m_buffer;
  Disk_Metric_Buffer m_disk_metric_buffer;
  Disk_Forecaster *m_disk_forecaster{nullptr};

  PSI_rwlock_key m_rwlock_key{0};
  PSI_rwlock_info m_rwlock_info{&m_rwlock_key, "disk_collector",
                                PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                                PSI_DOCUMENT_ME};
  PSI_memory_key m_memory_key{0};
  PSI_memory_info m_memory_info{
      &m_memory_key, "disk_collector", PSI_FLAG_ONLY_GLOBAL_STAT,
      PSI_VOLATILITY_PROVISIONING, "Health Monitor disk metric buffer"};

  Server m_server;
};

// clang-format on

}  // namespace hm

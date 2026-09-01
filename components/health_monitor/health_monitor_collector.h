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
#include <cstdint>
#include <limits>
#include <string>

#include "health_monitor_thread.h"
#include "health_monitor_timer.h"

namespace hm {

class System_vars;
template <typename T>
class Stat_Buffer;
class mutex_guard;

class Collector : public Thread {
 public:
  Collector() = default;
  Collector(const char *name, const char *resource_name, const char *instr_name,
            const char *os_name, const char *description);
  ~Collector() override = default;
  Collector(const Collector &) = delete;
  Collector &operator=(const Collector &) = delete;
  Collector(Collector &&) = delete;
  Collector &operator=(Collector &&) = delete;

  [[nodiscard]] const char *resource_name() const {
    return m_resource_name.c_str();
  }

  // Sample collection
  void run_process() override;
  void process() override {}

  // Settings
  virtual void set_buffer_size() {}
  virtual bool settings_changed() { return false; }
  virtual bool dynamic_settings_changed() { return false; }
  virtual bool static_settings_changed() { return false; }
  void mark_settings_pending();
  [[nodiscard]] bool settings_pending() const {
    return m_settings_pending.load(std::memory_order_acquire);
  }
  void set_settings_pending(bool pending) {
    m_settings_pending.store(pending, std::memory_order_release);
  }
  virtual void set_params() {}
  virtual bool update_settings();
  [[nodiscard]] int base_interval() const { return m_base_interval; }
  [[nodiscard]] int sample_interval() const { return m_sample_interval; }

 private:
  void set_base_interval(int base_interval);

 public:
  void set_sample_interval(int sample_interval);
  void set_collector_intervals(int base_interval, int sample_interval);

  // Logging
  virtual void log_collector_status(bool settings);
  virtual void log_collector_status(const std::string &state_msg,
                                    bool settings);
  virtual void log_resource_status() = 0;  // defined by collector
  virtual void log_status();
  virtual void log_resource_levels();

 protected:
  void consume_settings_pending();
  void apply_pending_settings();

  // Collector timers
  Cycle_Timer &sample_timer() { return m_sample_timer; }
  Cycle_Timer &status_message_timer() { return m_status_timer; }
  void initialize_sample_timers();
  void advance_sample_timers();

 public:
  virtual std::string build_collector_message(const std::string &status,
                                              const std::string &mode,
                                              const std::string &message,
                                              bool settings);

  virtual std::string build_resource_message(const std::string &status,
                                             const std::string &mode,
                                             const std::string &message,
                                             bool levels);
  static std::string format_level(uint64_t mbytes);
  static std::string format_mb(uint64_t mbytes, int prec, bool show_units);
  virtual std::string format_settings() { return ""; }
  virtual std::string format_status_info() { return ""; }
  virtual std::string tag_collector_state() { return "Started"; }
  virtual std::string tag_resource_state() { return ""; }
  virtual std::string tag_mode() { return ""; }

  // Constants and conversions
  static constexpr uint64_t no_warn_level{std::numeric_limits<uint64_t>::max()};

  const std::string k_started{"Started"};
  const std::string k_stopped{"Stopped"};
  const std::string k_offline_mode{"OFFLINE_MODE"};
  const std::string k_super_read_only{"SUPER_READ_ONLY"};
  const std::string k_invalid_path{"Invalid Path"};
  const std::string k_error{"ERROR"};
  const std::string k_system_error{"SYSTEM ERROR"};
  const std::string k_failed{"failed"};
  const std::string k_remaining{"attempts remaining"};

  // The base interval is the frequency that the Collector checks the resource,
  // i.e. the rate at which process() is called.
  // The sample interval is the frequency of storing resource info.
  // By default, the base interval == the sample interval, however, a collector
  // may choose a shorter base interval in order to monitor the resource at a
  // rate higher than sample collection and storage.
  // In that case, then the collector must check the sample_timer() to determine
  // when to store the sample.
  static constexpr int k_min_interval{1};
  static constexpr int k_max_interval{86400};  // 1 day
  static constexpr int k_default_base_interval{10};
  static constexpr int k_default_sample_interval{k_default_base_interval * 6};

  // clang-format off
 protected:
  // Resource name for logging such as 'Disk', 'Memory', 'System', etc.
  const std::string m_resource_name;
  // Current settings from system variables
  bool m_running{false};  // enable/disable sample collection

 private:
  int m_base_interval{k_default_base_interval};     // monitoring interval, in seconds
  int m_sample_interval{k_default_sample_interval}; // sample storage interval, in seconds
  bool m_custom_base_interval{false};               // true if base interval was set separately

 public:
  int m_retention{0};              // how long samples are retained, in seconds
  // true if system variable changes are pending
  std::atomic<bool> m_settings_pending{false};
  int m_status_interval{0};        // number of samples between status updates
  int m_status_counter{0};         // countdown of samples to the next status
  bool m_verbose_log{false};       // use a less compact log message format
  bool m_force_status{false};      // force status message wrt status counter

  Cycle_Timer m_sample_timer;  // timer for sample collection
  Cycle_Timer m_status_timer;  // timer for status messages
  // clang-format on
};

}  // namespace hm

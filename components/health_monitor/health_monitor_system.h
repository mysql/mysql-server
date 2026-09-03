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

#pragma once

#include "health_monitor_collector.h"
#include "health_monitor_system_bits.h"
#include "health_monitor_system_state.h"

namespace hm {

using System_CPU_Stats_Buffer = std::shared_ptr<Stat_Buffer<System_CPU_Stats>>;
using System_Disk_Stats_Buffer =
    std::shared_ptr<Stat_Buffer<System_Disk_Stats>>;
using System_Process_Stats_Buffer =
    std::shared_ptr<Stat_Buffer<System_Process_Stats>>;
using System_Memory_Stats_Buffer =
    std::shared_ptr<Stat_Buffer<System_Memory_Stats>>;

/**
  System Collector

  The System Collector collects and publishes CPU, disk I/O, memory, and process
  statistics. It does not issue resource warnings or take corrective action
  based on these values.
*/
class System_Collector : public Collector {
 public:
  System_Collector();

 protected:
  // Sample collection
  void pre_process() override;
  void process() override;
  void post_process() override;

 public:
  // Settings
  void set_buffer_size() final;
  void set_cpu_buffer_size();
  void set_disk_buffer_size();
  void set_memory_buffer_size();
  void set_process_buffer_size();
  bool set_rows_per_sample();
  bool settings_changed() override;
  void set_params() final;
  bool running() const override;

  // CPU stats
  std::shared_ptr<Stat_Buffer<System_CPU_Stats>> sys_cpu_buffer() {
    return m_sys_cpu_buffer;
  }
  static int get_cpu_stats(std::vector<System_CPU_Stats> &sys_cpu_stats);
  static int get_cpu_stats(std::vector<System_CPU_Stats> &sys_cpu_stats,
                           const time_t &timestamp);
  std::shared_ptr<Stat_Buffer<System_CPU_Stats>> get_cpu_stats_buffer();

  // Disk stats
  std::shared_ptr<Stat_Buffer<System_Disk_Stats>> sys_disk_buffer() {
    return m_sys_disk_buffer;
  }
  static int get_disk_stats(std::vector<System_Disk_Stats> &sys_disk_stats);
  static int get_disk_stats(std::vector<System_Disk_Stats> &sys_disk_stats,
                            const time_t &timestamp);
  std::shared_ptr<Stat_Buffer<System_Disk_Stats>> get_disk_stats_buffer();

  // Memory stats
  std::shared_ptr<Stat_Buffer<System_Memory_Stats>> sys_memory_buffer() {
    return m_sys_memory_buffer;
  }
  static int get_memory_stats(System_Memory_Stats &mem_stats);
  static int get_memory_stats(System_Memory_Stats &mem_stats,
                              const time_t &timestamp);
  std::shared_ptr<Stat_Buffer<System_Memory_Stats>> get_memory_stats_buffer();

  // Process stats
  std::shared_ptr<Stat_Buffer<System_Process_Stats>> sys_process_buffer() {
    return m_sys_process_buffer;
  }
  int get_process_stats(std::vector<System_Process_Stats> &sys_process_stats);
  int get_process_stats(std::vector<System_Process_Stats> &sys_process_stats,
                        const time_t &timestamp);
  std::shared_ptr<Stat_Buffer<System_Process_Stats>> get_process_stats_buffer();

  // Collector state
  void set_state(ss::System_State &new_state);
  void set_start_state() { set_state(m_start_state); }
  void set_stop_state() { set_state(m_stop_state); }
  void set_normal_state() { set_state(m_normal_state); }
  void set_warning_state() { set_state(m_warning_state); }
  void set_error_state() { set_state(m_error_state); }

  ss::System_State *m_current_state{nullptr};
  ss::Start_State m_start_state;
  ss::Stop_State m_stop_state;
  ss::Normal_State m_normal_state;
  ss::Warning_State m_warning_state;
  ss::Error_State m_error_state;

  // Logging
  void log_resource_status() override;
  void log_warning_error_condition();
  std::string format_settings() override;
  std::string format_status_info() override;
  std::string tag_collector_state() override;

  // System variables
  bool m_process_stats_history{true};
  bool m_cpu_stats_history{true};
  bool m_disk_stats_history{true};
  bool m_memory_stats_history{true};
  std::atomic<bool> m_settings_changed{false};

  // Sample sizes
  bool m_rows_per_sample_set{false};
  uint32_t m_cpu_rows_per_sample{1};
  uint32_t m_disk_rows_per_sample{1};
  uint32_t m_memory_rows_per_sample{1};
  uint32_t m_process_rows_per_sample{1};

  // Error states
  bool m_sysinfo_error{false};
  bool m_sysinfo_warning{false};
  bool m_cpu_stats_error{false};
  bool m_disk_stats_error{false};
  bool m_memory_stats_error{false};
  bool m_process_stats_error{false};

 private:
  std::shared_ptr<Stat_Buffer<System_CPU_Stats>> m_sys_cpu_buffer{nullptr};
  std::shared_ptr<Stat_Buffer<System_Disk_Stats>> m_sys_disk_buffer{nullptr};
  std::shared_ptr<Stat_Buffer<System_Memory_Stats>> m_sys_memory_buffer{
      nullptr};
  std::shared_ptr<Stat_Buffer<System_Process_Stats>> m_sys_process_buffer{
      nullptr};

  PSI_rwlock_key m_rwlock_key{0};
  PSI_rwlock_info m_rwlock_info{&m_rwlock_key, "system_collector",
                                PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                                PSI_DOCUMENT_ME};
  PSI_memory_key m_memory_key_cpu_stats{0};
  PSI_memory_key m_memory_key_cpu_stats_history{0};
  PSI_memory_key m_memory_key_disk_stats{0};
  PSI_memory_key m_memory_key_disk_stats_history{0};
  PSI_memory_key m_memory_key_memory_stats{0};
  PSI_memory_key m_memory_key_memory_stats_history{0};
  PSI_memory_key m_memory_key_process_stats{0};
  PSI_memory_key m_memory_key_process_stats_history{0};
};

}  // namespace hm

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

#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

#include "health_monitor_collector.h"
#include "health_monitor_memory_state.h"

namespace hm {

struct Process_Memory_Info {
  Process_Memory_Info() = default;

  time_t m_timestamp{0};
  std::string m_name;
  uint64_t m_pid{0};
  uint64_t m_vmrss_bytes{0};
  uint64_t m_vmdata_bytes{0};
  uint64_t m_vmswap_bytes{0};
  uint64_t m_page_faults{0};
  bool m_residency_available{false};
  bool m_details_available{false};
  bool m_page_faults_available{false};
};

struct System_Memory_Info {
  System_Memory_Info() = default;

  time_t m_timestamp{0};
  uint64_t m_total_bytes{0};
  uint64_t m_free_bytes{0};
  uint64_t m_avail_bytes{0};
  uint64_t m_cached_bytes{0};
  uint64_t m_swap_total_bytes{0};
  uint64_t m_swap_free_bytes{0};
  bool m_cache_available{false};
  bool m_swap_capacity_available{false};
};

/**
  Memory Info
  Record structure for health_process_memory and health_system_memory.
*/
struct Memory_Info {
  Memory_Info() = default;
  Process_Memory_Info proc;
  System_Memory_Info sys;
  bool m_process_memory_available{false};
};

/**
  Timestamp and publish the records for one memory sample.

  A 'sample' is one-time memory observation. 'Publication' means making its
  record(s) visible through writers provided by the caller.

  The caller provides writers to store the records in their respective buffers.

  @param[in,out] memory_info Memory sample to publish
  @param[in] timestamp Timestamp applied to published records
  @param[in] write_process Writer for a process memory record
  @param[in] write_system Writer for a system memory record
*/
template <typename Process_writer, typename System_writer>
void publish_memory_records(Memory_Info &memory_info, time_t timestamp,
                            Process_writer &&write_process,
                            System_writer &&write_system) {
  memory_info.sys.m_timestamp = timestamp;
  if (memory_info.m_process_memory_available) {
    memory_info.proc.m_timestamp = timestamp;
    std::forward<Process_writer>(write_process)(memory_info.proc);
  }
  std::forward<System_writer>(write_system)(memory_info.sys);
}

/**
  Memory Collector

  The Memory Collector monitors server memory usage, publishes memory
  statistics, and emits warnings when available memory falls below the
  configured thresholds. Unlike the Disk Collector, the Memory Collector
  does not take corrective action when memory is low.
*/
class Memory_Collector : public Collector {
 public:
  Memory_Collector();

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
  bool running() const override;

  // Memory info
  std::shared_ptr<Stat_Buffer<Process_Memory_Info>> proc_buffer() {
    return m_proc_buffer;
  }
  std::shared_ptr<Stat_Buffer<System_Memory_Info>> sys_buffer() {
    return m_sys_buffer;
  }
  int get_memory_info(Memory_Info &memory_info);
  int get_process_memory_info(Process_Memory_Info &process_info,
                              bool &unavailable);
  int get_system_memory_info(System_Memory_Info &sys_memory_info);
  void update_per_sample_totals(const Memory_Info &memory_info);
  void set_memory_profile(const Memory_Info &memory_info);

  // Collector state
  static void set_state_params();
  void set_state(ms::Memory_State &new_state);
  void set_start_state() { set_state(m_start_state); }
  void set_stop_state() { set_state(m_stop_state); }
  void set_normal_state() { set_state(m_normal_state); }
  void set_warning_state() { set_state(m_warning_state); }
  void set_error_state() { set_state(m_error_state); }
  void check_memory_levels() const;

  ms::Memory_State *m_current_state{nullptr};
  ms::Start_State m_start_state;
  ms::Stop_State m_stop_state;
  ms::Normal_State m_normal_state;
  ms::Warning_State m_warning_state;
  ms::Error_State m_error_state;

  // Logging
  void log_resource_status() override;
  std::string format_settings() override;
  std::string format_status_info() override;
  std::string tag_collector_state() override;

  void log_warning_level(int);
  void log_warning_level() { log_warning_level(0); }
  void log_warning_error_condition();

  // System variables
  uint64_t m_warn_level_1_mb{0};
  uint64_t m_warn_level_2_mb{0};
  uint64_t m_warn_level_3_mb{0};
  std::atomic<bool> m_settings_changed{false};

  // Current memory levels in MiB
  uint64_t m_total_mb{0};       // total memory im mb
  uint64_t m_avail_mb{0};       // previous available memory
  uint64_t m_last_avail_mb{0};  // last availale memory
  float m_used_pct{0};          // used memory, pct of total
  int m_delta_pct{10};          // pct change to trigger status message
  uint64_t m_process_mb{0};     // memory used by mysqld
  uint64_t m_max_warn_level_mb{no_warn_level};
  uint64_t m_last_warn_level_mb{no_warn_level};
  bool m_refresh_memory_profile{false};  // true if settings change

  bool m_sysinfo_error{false};
  Memory_Info m_info;  // memory stats, raw

  // Constants
  const std::string k_warning_level{"Warning Level"};

 private:
  std::shared_ptr<Stat_Buffer<Process_Memory_Info>> m_proc_buffer;
  std::shared_ptr<Stat_Buffer<System_Memory_Info>> m_sys_buffer;

  PSI_rwlock_key m_rwlock_key{0};
  PSI_rwlock_info m_rwlock_info{&m_rwlock_key, "memory_collector",
                                PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                                PSI_DOCUMENT_ME};
  PSI_memory_key m_memory_key{0};
  PSI_memory_info m_memory_info{
      &m_memory_key, "memory_collector", PSI_FLAG_ONLY_GLOBAL_STAT,
      PSI_VOLATILITY_PROVISIONING, "Health Monitor system memory data buffer"};
};

}  // namespace hm

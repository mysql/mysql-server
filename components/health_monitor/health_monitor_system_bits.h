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

#include <cstdint>
#include <ctime>
#include <string>

namespace hm {

struct System_CPU_Stats {
  time_t m_timestamp{0};
  std::string m_cpu;
  uint64_t m_user_ms{0};
  uint64_t m_nice_ms{0};
  uint64_t m_system_ms{0};
  uint64_t m_idle_ms{0};
  uint64_t m_iowait_ms{0};
  uint64_t m_irq_ms{0};
  uint64_t m_softirq_ms{0};
  uint64_t m_steal_ms{0};
  uint64_t m_guest_ms{0};
  uint64_t m_guest_nice_ms{0};
  bool m_extended_times_available{false};
};

struct System_Disk_Stats {
  System_Disk_Stats() = default;

  time_t m_timestamp{0};
  std::string m_device_name;
  uint64_t m_total_bytes{0};
  uint64_t m_sector_size_bytes{0};
  uint64_t m_reads{0};
  uint64_t m_read_bytes{0};
  uint64_t m_read_time_ms{0};
  uint64_t m_writes{0};
  uint64_t m_write_bytes{0};
  uint64_t m_write_time_ms{0};
  uint64_t m_flushes{0};
  uint64_t m_flush_time_ms{0};
  bool m_partition{false};
  std::string m_parent_disk_name;
  bool m_device_kind_available{false};
  bool m_capacity_available{false};
  bool m_hierarchy_available{false};
  bool m_read_write_available{false};
  bool m_flush_available{false};
};

struct System_Memory_Stats {
  System_Memory_Stats() = default;

  time_t m_timestamp{0};
  uint64_t m_total_bytes{0};
  uint64_t m_free_bytes{0};
  uint64_t m_avail_bytes{0};
  uint64_t m_used_bytes{0};  // derived
  uint64_t m_buffers_bytes{0};
  uint64_t m_cached_bytes{0};
  uint64_t m_slab_bytes{0};
  uint64_t m_swap_total_bytes{0};
  uint64_t m_swap_free_bytes{0};
  uint64_t m_swap_used_bytes{0};  // derived
  uint64_t m_swap_in{0};
  uint64_t m_swap_out{0};
  bool m_breakdown_available{false};
  bool m_swap_capacity_available{false};
  bool m_swap_activity_available{false};
};

struct System_Process_Stats {
  System_Process_Stats() = default;

  time_t m_timestamp{0};
  uint64_t m_pid{0};
  std::string m_process_name;
  uint64_t m_tid{0};
  std::string m_thread_name;
  char m_state{'\0'};
  uint64_t m_utime_ms{0};
  uint64_t m_stime_ms{0};
  uint64_t m_cutime_ms{0};
  uint64_t m_cstime_ms{0};
  uint32_t m_num_threads{0};
  uint64_t m_vsize_bytes{0};
  uint64_t m_rss_bytes{0};
  uint64_t m_rsslim_bytes{0};
  uint32_t m_processor{0};
  uint64_t m_delayacct_blkio_ms{0};
  uint64_t m_guest_time_ms{0};
  uint64_t m_cguest_time_ms{0};
  uint64_t m_read_bytes{0};
  uint64_t m_write_bytes{0};
  bool m_cpu_available{false};
  bool m_runtime_available{false};
  bool m_extended_cpu_available{false};
  bool m_scheduler_available{false};
  bool m_io_available{false};
};

}  // namespace hm

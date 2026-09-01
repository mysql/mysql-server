/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "health_monitor_process_stats_mapping.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hm {
namespace {

bool is_available(const mysql::system_info::Result_state state) {
  return state == mysql::system_info::Result_state::available;
}

bool is_failed(const mysql::system_info::Result_state state) {
  return state == mysql::system_info::Result_state::failed;
}

bool convert_milliseconds(std::chrono::milliseconds value, uint64_t &output) {
  if (value.count() < 0) return false;
  output = static_cast<uint64_t>(value.count());
  return true;
}

bool set_state(const mysql::system_info::Thread_state state, char &output) {
  switch (state) {
    case mysql::system_info::Thread_state::running:
      output = 'R';
      return true;
    case mysql::system_info::Thread_state::sleeping:
      output = 'S';
      return true;
    case mysql::system_info::Thread_state::waiting:
      output = 'D';
      return true;
    case mysql::system_info::Thread_state::stopped:
      output = 'T';
      return true;
    case mysql::system_info::Thread_state::zombie:
      output = 'Z';
      return true;
    case mysql::system_info::Thread_state::unknown:
      output = '?';
      return true;
  }
  return false;
}

template <typename Snapshot>
bool is_aligned(const mysql::system_info::Thread_inventory &inventory,
                const Snapshot &snapshot) {
  const auto &threads = inventory.threads;
  if (threads.empty() || snapshot.threads.size() != threads.size())
    return false;

  for (size_t index = 0; index < threads.size(); ++index) {
    if (snapshot.threads[index].tid != threads[index].tid) return false;
  }
  return true;
}

bool is_valid_inventory(const mysql::system_info::Thread_inventory &inventory) {
  if (inventory.threads.empty()) return false;
  std::unordered_set<uint64_t> thread_ids;
  for (const auto &thread : inventory.threads) {
    if (thread.tid == 0 || !thread_ids.insert(thread.tid).second) return false;
    char state;
    if (!set_state(thread.state, state)) return false;
  }
  return true;
}

bool valid_cpu_times(const mysql::system_info::Thread_cpu_snapshot &cpu) {
  for (const auto &thread : cpu.threads) {
    uint64_t value;
    if (!convert_milliseconds(thread.user, value) ||
        !convert_milliseconds(thread.system, value)) {
      return false;
    }
  }
  return true;
}

bool valid_extended_cpu_times(
    const mysql::system_info::Thread_extended_cpu_snapshot &cpu) {
  for (const auto &thread : cpu.threads) {
    uint64_t value;
    if (!convert_milliseconds(thread.child_user, value) ||
        !convert_milliseconds(thread.child_system, value) ||
        !convert_milliseconds(thread.guest, value) ||
        !convert_milliseconds(thread.child_guest, value)) {
      return false;
    }
  }
  return true;
}

bool valid_scheduler(
    const mysql::system_info::Thread_scheduler_snapshot &scheduler) {
  for (const auto &thread : scheduler.threads) {
    uint64_t value;
    if (!convert_milliseconds(thread.block_io_delay, value)) return false;
  }
  return true;
}

bool set_required_record(const mysql::system_info::Process_identity &identity,
                         const mysql::system_info::Thread_identity &thread,
                         const mysql::system_info::Thread_cpu_times &cpu,
                         time_t timestamp, System_Process_Stats &output) {
  output.m_timestamp = timestamp;
  output.m_pid = identity.pid;
  output.m_process_name = identity.name;
  output.m_tid = thread.tid;
  output.m_thread_name = thread.name;
  if (!set_state(thread.state, output.m_state) ||
      !convert_milliseconds(cpu.user, output.m_utime_ms) ||
      !convert_milliseconds(cpu.system, output.m_stime_ms)) {
    return false;
  }
  output.m_cpu_available = true;
  return true;
}

void set_runtime(const mysql::system_info::Process_runtime_info &runtime,
                 System_Process_Stats &output) {
  output.m_num_threads = runtime.thread_count;
  output.m_vsize_bytes = runtime.virtual_bytes;
  output.m_rss_bytes = runtime.resident_bytes;
  output.m_rsslim_bytes = runtime.resident_limit_bytes;
  output.m_runtime_available = true;
}

void set_extended_cpu(
    const mysql::system_info::Thread_extended_cpu_times &extended_cpu,
    System_Process_Stats &output) {
  output.m_cutime_ms = static_cast<uint64_t>(extended_cpu.child_user.count());
  output.m_cstime_ms = static_cast<uint64_t>(extended_cpu.child_system.count());
  output.m_guest_time_ms = static_cast<uint64_t>(extended_cpu.guest.count());
  output.m_cguest_time_ms =
      static_cast<uint64_t>(extended_cpu.child_guest.count());
  output.m_extended_cpu_available = true;
}

void set_scheduler(const mysql::system_info::Thread_scheduler_info &scheduler,
                   const bool process_leader, System_Process_Stats &output) {
  output.m_processor = scheduler.last_cpu;
  if (process_leader) {
    output.m_delayacct_blkio_ms =
        static_cast<uint64_t>(scheduler.block_io_delay.count());
  }
  output.m_scheduler_available = true;
}

void set_io(const mysql::system_info::Thread_io_counters &io,
            System_Process_Stats &output) {
  output.m_read_bytes = io.read_bytes;
  output.m_write_bytes = io.write_bytes;
  output.m_io_available = true;
}

}  // namespace

bool update_process_threads_error_state(
    const Process_threads_conversion_result result, bool &error_state) {
  if (result == Process_threads_conversion_result::success) {
    error_state = false;
    return false;
  }
  if (error_state) return false;
  error_state = true;
  return true;
}

Process_threads_conversion_result convert_process_threads_snapshot(
    const mysql::system_info::Process_threads_snapshot &snapshot,
    const time_t timestamp, std::vector<System_Process_Stats> &output) {
  if (!is_available(snapshot.identity.state()) ||
      !is_available(snapshot.threads.state()) ||
      !is_available(snapshot.cpu.state())) {
    return Process_threads_conversion_result::failure;
  }

  const auto &identity = snapshot.identity.value();
  const auto &inventory = snapshot.threads.value();
  if (identity.pid == 0 || identity.name.empty() ||
      !is_valid_inventory(inventory) ||
      !is_aligned(inventory, snapshot.cpu.value()) ||
      !valid_cpu_times(snapshot.cpu.value())) {
    return Process_threads_conversion_result::failure;
  }

  if ((is_available(snapshot.extended_cpu.state()) &&
       (!is_aligned(inventory, snapshot.extended_cpu.value()) ||
        !valid_extended_cpu_times(snapshot.extended_cpu.value()))) ||
      (is_available(snapshot.scheduler.state()) &&
       (!is_aligned(inventory, snapshot.scheduler.value()) ||
        !valid_scheduler(snapshot.scheduler.value()))) ||
      (is_available(snapshot.io.state()) &&
       !is_aligned(inventory, snapshot.io.value()))) {
    return Process_threads_conversion_result::failure;
  }

  std::vector<System_Process_Stats> converted;
  converted.reserve(inventory.threads.size());
  for (size_t index = 0; index < inventory.threads.size(); ++index) {
    System_Process_Stats record;
    if (!set_required_record(identity, inventory.threads[index],
                             snapshot.cpu.value().threads[index], timestamp,
                             record)) {
      return Process_threads_conversion_result::failure;
    }
    if (is_available(snapshot.runtime.state()) &&
        record.m_tid == identity.pid) {
      set_runtime(snapshot.runtime.value(), record);
    }
    if (is_available(snapshot.extended_cpu.state())) {
      set_extended_cpu(snapshot.extended_cpu.value().threads[index], record);
    }
    if (is_available(snapshot.scheduler.state())) {
      set_scheduler(snapshot.scheduler.value().threads[index],
                    record.m_tid == identity.pid, record);
    }
    if (is_available(snapshot.io.state())) {
      set_io(snapshot.io.value().threads[index], record);
    }
    converted.push_back(std::move(record));
  }

  output.insert(output.end(), std::make_move_iterator(converted.begin()),
                std::make_move_iterator(converted.end()));
  if (is_failed(snapshot.runtime.state()) ||
      is_failed(snapshot.extended_cpu.state()) ||
      is_failed(snapshot.scheduler.state()) || is_failed(snapshot.io.state())) {
    return Process_threads_conversion_result::degraded;
  }
  return Process_threads_conversion_result::success;
}

System_process_stats_row make_system_process_stats_row(
    const System_Process_Stats &record) {
  System_process_stats_row row;
  row.m_timestamp = record.m_timestamp;
  row.m_pid = record.m_pid;
  row.m_process_name = record.m_process_name;
  row.m_tid = record.m_tid;
  row.m_thread_name = record.m_thread_name;
  row.m_state = record.m_state;
  row.m_utime_ms = record.m_utime_ms;
  row.m_stime_ms = record.m_stime_ms;
  row.m_cutime_ms = record.m_cutime_ms;
  row.m_cstime_ms = record.m_cstime_ms;
  row.m_num_threads = record.m_num_threads;
  row.m_vsize_bytes = record.m_vsize_bytes;
  row.m_rss_bytes = record.m_rss_bytes;
  row.m_rsslim_bytes = record.m_rsslim_bytes;
  row.m_processor = record.m_processor;
  row.m_delayacct_blkio_ms = record.m_delayacct_blkio_ms;
  row.m_guest_time_ms = record.m_guest_time_ms;
  row.m_cguest_time_ms = record.m_cguest_time_ms;
  row.m_read_bytes = record.m_read_bytes;
  row.m_write_bytes = record.m_write_bytes;
  row.m_cpu_available = record.m_cpu_available;
  row.m_runtime_available = record.m_runtime_available;
  row.m_extended_cpu_available = record.m_extended_cpu_available;
  row.m_scheduler_available = record.m_scheduler_available;
  row.m_io_available = record.m_io_available;
  return row;
}

bool system_process_stats_column_is_null(const System_process_stats_row &row,
                                         const unsigned int index) {
  switch (index) {
    case 6:  // UTIME_MS
    case 7:  // STIME_MS
      return !row.m_cpu_available;
    case 8:   // CUTIME_MS
    case 9:   // CSTIME_MS
    case 16:  // GUEST_TIME_MS
    case 17:  // CGUEST_TIME_MS
      return !row.m_extended_cpu_available;
    case 10:  // NUM_THREADS
    case 11:  // VSIZE_BYTES
    case 12:  // RSS_BYTES
    case 13:  // RSSLIM_BYTES
      return !row.m_runtime_available;
    case 14:  // PROCESSOR
    case 15:  // DELAYACCT_BLKIO_MS
      return !row.m_scheduler_available;
    case 18:  // READ_BYTES
    case 19:  // WRITE_BYTES
      return !row.m_io_available;
    default:
      assert(false);
      return false;
  }
}

}  // namespace hm

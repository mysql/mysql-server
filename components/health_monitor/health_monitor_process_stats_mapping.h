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

#pragma once

#include <mysql/components/library_mysys/system_info.h>

#include <cstdint>
#include <ctime>
#include <utility>
#include <vector>

#include "health_monitor_system_bits.h"

namespace hm {

enum class Process_threads_conversion_result { success, degraded, failure };

/**
  Update process thread warning suppression after conversion.

  @param[in] result Conversion result
  @param[in,out] error_state Existing warning suppression state
  @return true when the caller must log this result
*/
bool update_process_threads_error_state(
    Process_threads_conversion_result result, bool &error_state);

/**
  Convert process threads snapshot into multiple Health Monitor process records.
  - Process_identity, Thread_inventory and Thread_cpu_times are required.
  - Runtime, extended CPU, scheduler and I/O capabilities are optional.

  A failed optional capability still produces the required rows. For example,
  if I/O collection fails, the thread rows still contain identity, state and
  CPU time, however, READ_BYTES and WRITE_BYTES remain NULL, and the
  corresponding metrics are not delivered.

  @param[in] snapshot Process threads snapshot to convert
  @param[in] timestamp Timestamp for every converted record
  @param[in,out] output Destination records, unchanged on failure
  @return complete success, 'degraded' success or conversion failure
*/
Process_threads_conversion_result convert_process_threads_snapshot(
    const mysql::system_info::Process_threads_snapshot &snapshot,
    time_t timestamp, std::vector<System_Process_Stats> &output);

struct System_process_stats_row {
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

/**
  Convert a process thread record into a Performance Schema table row.

  @param[in] record process thread record to convert
  @return Converted table row
*/
System_process_stats_row make_system_process_stats_row(
    const System_Process_Stats &record);

/**
  Determine whether a process thread metric column is NULL.

  @param[in] row Table row
  @param[in] index Performance Schema column index
  @return true when the column is NULL
*/
bool system_process_stats_column_is_null(const System_process_stats_row &row,
                                         unsigned int index);

enum class Process_thread_metric {
  user_time,
  system_time,
  child_user_time,
  child_system_time,
  thread_count,
  virtual_bytes,
  resident_bytes,
  resident_limit_bytes,
  processor,
  block_io_delay,
  guest_time,
  child_guest_time,
  read_bytes,
  write_bytes
};

/**
  Determine whether a process thread metric is available.

  @param[in] record process thread record
  @param[in] metric Metric to inspect
  @return true when the selected metric is available
*/
inline bool process_thread_metric_available(
    const System_Process_Stats &record, const Process_thread_metric metric) {
  switch (metric) {
    case Process_thread_metric::user_time:
    case Process_thread_metric::system_time:
      return record.m_cpu_available;
    case Process_thread_metric::child_user_time:
    case Process_thread_metric::child_system_time:
    case Process_thread_metric::guest_time:
    case Process_thread_metric::child_guest_time:
      return record.m_extended_cpu_available;
    case Process_thread_metric::thread_count:
    case Process_thread_metric::virtual_bytes:
    case Process_thread_metric::resident_bytes:
    case Process_thread_metric::resident_limit_bytes:
      return record.m_runtime_available;
    case Process_thread_metric::processor:
    case Process_thread_metric::block_io_delay:
      return record.m_scheduler_available;
    case Process_thread_metric::read_bytes:
    case Process_thread_metric::write_bytes:
      return record.m_io_available;
  }
  return false;
}

/**
  Select the newest record that contains a process thread metric.

  Records are read by descending index. The search is limited to the newest
  timestamp so an unavailable current sample cannot fall back to stale data.

  @param[in] count Number of records available to read
  @param[in] metric Metric that must be available
  @param[in] reader Function that copies the record at an index
  @param[out] output Selected record, unchanged when no record is selected
  @return true when a record was selected
*/
template <typename Reader>
bool select_latest_process_thread_metric(const int count,
                                         const Process_thread_metric metric,
                                         Reader &&reader,
                                         System_Process_Stats &output) {
  bool have_timestamp = false;
  time_t timestamp = 0;
  for (int index = count - 1; index >= 0; --index) {
    System_Process_Stats candidate;
    if (!reader(index, candidate)) return false;
    if (!have_timestamp) {
      timestamp = candidate.m_timestamp;
      have_timestamp = true;
    } else if (timestamp != candidate.m_timestamp) {
      break;
    }
    if (process_thread_metric_available(candidate, metric)) {
      output = std::move(candidate);
      return true;
    }
  }
  return false;
}

/**
  Deliver one process thread metric.

  The delivery function receives the metric value. It is not called when the
  selected metric is unavailable. A value of zero remains an available
  measurement.

  @param[in] record process thread record
  @param[in] metric Metric to deliver
  @param[in] delivery Delivery function
  @return true when the delivery function was called
*/
template <typename Delivery>
bool deliver_process_thread_metric(const System_Process_Stats &record,
                                   const Process_thread_metric metric,
                                   Delivery &&delivery) {
  if (!process_thread_metric_available(record, metric)) return false;

  uint64_t value{0};
  switch (metric) {
    case Process_thread_metric::user_time:
      value = record.m_utime_ms;
      break;
    case Process_thread_metric::system_time:
      value = record.m_stime_ms;
      break;
    case Process_thread_metric::child_user_time:
      value = record.m_cutime_ms;
      break;
    case Process_thread_metric::child_system_time:
      value = record.m_cstime_ms;
      break;
    case Process_thread_metric::thread_count:
      value = record.m_num_threads;
      break;
    case Process_thread_metric::virtual_bytes:
      value = record.m_vsize_bytes;
      break;
    case Process_thread_metric::resident_bytes:
      value = record.m_rss_bytes;
      break;
    case Process_thread_metric::resident_limit_bytes:
      value = record.m_rsslim_bytes;
      break;
    case Process_thread_metric::processor:
      value = record.m_processor;
      break;
    case Process_thread_metric::block_io_delay:
      value = record.m_delayacct_blkio_ms;
      break;
    case Process_thread_metric::guest_time:
      value = record.m_guest_time_ms;
      break;
    case Process_thread_metric::child_guest_time:
      value = record.m_cguest_time_ms;
      break;
    case Process_thread_metric::read_bytes:
      value = record.m_read_bytes;
      break;
    case Process_thread_metric::write_bytes:
      value = record.m_write_bytes;
      break;
  }

  std::forward<Delivery>(delivery)(value);
  return true;
}

}  // namespace hm

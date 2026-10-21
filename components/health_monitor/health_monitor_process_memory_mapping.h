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
#include <utility>

#include "health_monitor_memory.h"

namespace hm {

/**
  Convert a process memory snapshot into a Health Monitor memory record.

  Identity and residency must have available System_result values. The details
  and page_faults fields are optional. Optional fields without available values
  become SQL NULL.

  @param[in] snapshot Process memory snapshot to convert
  @param[in,out] output Destination record, unchanged on failure
  @return true on success, false on failure
*/
bool convert_process_memory_snapshot(
    const mysql::system_info::Process_memory_snapshot &snapshot,
    Process_Memory_Info &output);

/**
  Check whether the process memory row must be omitted as unavailable.

  The collector omits the row only when every capability is unavailable.
  Failed capabilities remain failures.

  @param[in] snapshot Process memory snapshot
  @return true when the complete row is unavailable
*/
bool process_memory_snapshot_is_unavailable(
    const mysql::system_info::Process_memory_snapshot &snapshot);

struct Health_process_memory_row {
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

/**
  Convert a process memory record into a Performance Schema table row.

  @param[in] record Process memory record to convert
  @return Converted table row
*/
Health_process_memory_row make_health_process_memory_row(
    const Process_Memory_Info &record);

/**
  Determine whether a process memory column is NULL.

  @param[in] row Table row
  @param[in] index Performance Schema column index
  @return true when the column is NULL
*/
bool health_process_memory_column_is_null(const Health_process_memory_row &row,
                                          unsigned int index);

enum class Process_memory_metric {
  resident_bytes,
  data_bytes,
  swap_bytes,
  page_faults
};

/**
  Deliver one available process memory metric.

  The delivery function receives the metric value and process ID. It is not
  called when the metric is unavailable. A value of zero is a valid available
  measurement.

  @param[in] record process memory record
  @param[in] metric Metric to deliver
  @param[in] delivery Delivery function
  @return true when the delivery function was called
*/
template <typename Delivery>
bool deliver_process_memory_metric(const Process_Memory_Info &record,
                                   const Process_memory_metric metric,
                                   Delivery &&delivery) {
  uint64_t value{0};
  switch (metric) {
    case Process_memory_metric::resident_bytes:
      if (!record.m_residency_available) return false;
      value = record.m_vmrss_bytes;
      break;
    case Process_memory_metric::data_bytes:
      if (!record.m_details_available) return false;
      value = record.m_vmdata_bytes;
      break;
    case Process_memory_metric::swap_bytes:
      if (!record.m_details_available) return false;
      value = record.m_vmswap_bytes;
      break;
    case Process_memory_metric::page_faults:
      if (!record.m_page_faults_available) return false;
      value = record.m_page_faults;
      break;
  }

  std::forward<Delivery>(delivery)(value, record.m_pid);
  return true;
}

}  // namespace hm

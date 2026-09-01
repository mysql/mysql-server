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

#include <ctime>

#include "health_monitor_memory.h"
#include "health_monitor_system_bits.h"

namespace hm {

/**
  Convert a host memory snapshot into a System Memory Stats record.

  The snapshot must have a valid System_result to be converted.
  - 'Unavailable' for a value means there is no equivalent on this platform.
  - 'Failed' means that the collection attempt failed.

  For example, swap_activity is available on Linux but not macOS, and would be
  represented as NULL in the memory tables.

  @param[in] snapshot Memory snapshot to convert
  @param[in] timestamp Timestamp for the converted record
  @param[in,out] output Destination record, no change on failure
  @return true on success, false on failure
*/
bool convert_host_memory_snapshot(
    const mysql::system_info::Host_memory_snapshot &snapshot, time_t timestamp,
    System_Memory_Stats &output);

/**
  Convert a host memory snapshot into a Memory Collector record.

  The snapshot must have a valid System_result to be converted.
  - 'Unavailable' for a value means there is no equivalent on this platform.
  - 'Failed' means that the collection attempt failed.

  If the record is complete but a value is unavailable, then that value is
  represented as NULL in the table.

  @param[in] snapshot Host memory snapshot to convert
  @param[in,out] output Destination record, no change on failure
  @return true on success, false on failure
*/
bool convert_host_memory_snapshot(
    const mysql::system_info::Host_memory_snapshot &snapshot,
    System_Memory_Info &output);

struct Health_system_memory_row {
  time_t m_timestamp{0};
  uint64_t m_total_bytes{0};
  uint64_t m_free_bytes{0};
  uint64_t m_avail_bytes{0};
  uint64_t m_cached_bytes{0};
  uint64_t m_swap_total_bytes{0};
  uint64_t m_swap_free_bytes{0};
  float m_use_percent{0};
  bool m_cache_available{false};
  bool m_swap_capacity_available{false};
};

/**
  Convert a system memory record into a Performance Schema table row.

  @param[in] record System-memory record to convert
  @return Converted table row
*/
Health_system_memory_row make_health_system_memory_row(
    const System_Memory_Info &record);

/**
  Determine whether a nullable memory column is NULL.

  @param[in] row Table row
  @param[in] index Performance Schema column index
  @return true when the column is NULL
*/
bool health_system_memory_column_is_null(const Health_system_memory_row &row,
                                         unsigned int index);

}  // namespace hm

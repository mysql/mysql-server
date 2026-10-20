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
#include <string>
#include <vector>

#include "health_monitor_system_bits.h"

namespace hm {

struct System_Disk_Table_Row {
  time_t m_timestamp{0};
  std::string m_device_name;
  uint64_t m_reads{0};
  uint64_t m_read_bytes{0};
  uint64_t m_read_time_ms{0};
  uint64_t m_writes{0};
  uint64_t m_write_bytes{0};
  uint64_t m_write_time_ms{0};
  uint64_t m_flushes{0};
  uint64_t m_flush_time_ms{0};
  bool m_read_write_available{false};
  bool m_flush_available{false};
};

/**
  Convert a storage snapshot into Health Monitor disk records.

  This function merges five storage capabilities:
  - Device inventory and disk/partition classification
  - Capacity and sector size
  - Read/write counters
  - Flush counters
  - Partition-to-disk hierarchy

  Each capability contains rows keyed by a device name such as `sda` or `sda1`.
  These rows are then joined into one System_Disk_Stats record per device.

  For example:
    inventory:  sda = disk
    capacity:   sda = 1 TB
    read_write: sda = 100 reads, 50 writes
    flush:      unavailable
  produces an sda record with inventory, capacity and read/write counts, but
  flush availability is false because it's not available on the platform.

  @param[in] snapshot Storage snapshot to convert
  @param[in] timestamp Timestamp to apply to every converted record
  @param[in,out] output Destination records, unchanged unless data is available
  @return Storage conversion result state
*/
mysql::system_info::Result_state convert_storage_snapshot(
    const mysql::system_info::Storage_snapshot &snapshot, time_t timestamp,
    std::vector<System_Disk_Stats> &output);

/**
  Convert a disk record into a Performance Schema table row.

  @param[in] record Disk record to convert
  @return Converted table row
*/
System_Disk_Table_Row make_system_disk_table_row(
    const System_Disk_Stats &record);

/**
  Determine whether disk table column is NULL.

  @param[in] row Table row
  @param[in] index Performance Schema column index
  @return true when the column is NULL
*/
bool system_disk_table_column_is_null(const System_Disk_Table_Row &row,
                                      unsigned int index);

/**
  Determine whether a read/write metric can use a disk record.

  @param[in] record Disk record
  @param[in] partition Device classification
  @return true when the record is eligible
*/
bool system_disk_read_write_metric_eligible(const System_Disk_Stats &record,
                                            bool partition);

/**
  Determine whether a flush metric can use a disk record.

  @param[in] record Disk record
  @param[in] partition Device classification
  @return true when the record is eligible
*/
bool system_disk_flush_metric_eligible(const System_Disk_Stats &record,
                                       bool partition);

}  // namespace hm

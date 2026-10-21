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

#include "health_monitor_storage_mapping.h"

#include <cassert>
#include <chrono>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace hm {
namespace {

using mysql::system_info::Result_state;
using mysql::system_info::Storage_device_kind;

bool valid_name(const std::string_view name) { return !name.empty(); }

template <typename Row, typename Name>
bool valid_rows(const std::vector<Row> &rows, Name name,
                const bool allow_empty = false) {
  if (rows.empty()) return allow_empty;

  std::unordered_set<std::string_view> names;
  names.reserve(rows.size());
  for (const auto &row : rows) {
    const std::string_view device = name(row);
    if (!valid_name(device) || !names.insert(device).second) return false;
  }
  return true;
}

bool convert_milliseconds(const std::chrono::milliseconds value,
                          uint64_t &output) {
  if (value.count() < 0) return false;
  output = static_cast<uint64_t>(value.count());
  return true;
}

bool has_row_producing_failure(
    const mysql::system_info::Storage_snapshot &snapshot) {
  return snapshot.inventory.state() == Result_state::failed ||
         snapshot.capacities.state() == Result_state::failed ||
         snapshot.read_write.state() == Result_state::failed ||
         snapshot.flush.state() == Result_state::failed;
}

}  // namespace

Result_state convert_storage_snapshot(
    const mysql::system_info::Storage_snapshot &snapshot,
    const time_t timestamp, std::vector<System_Disk_Stats> &output) {
  std::map<std::string, System_Disk_Stats> records;

  if (snapshot.inventory.has_value()) {
    const auto &devices = snapshot.inventory.value().devices;
    if (!valid_rows(devices, [](const auto &device) -> const std::string & {
          return device.name;
        })) {
      return Result_state::failed;
    }
    for (const auto &device : devices) {
      if (device.kind != Storage_device_kind::disk &&
          device.kind != Storage_device_kind::partition) {
        return Result_state::failed;
      }
      auto &record = records[device.name];
      record.m_device_name = device.name;
      record.m_device_kind_available = true;
      record.m_partition = device.kind == Storage_device_kind::partition;
    }
  }

  if (snapshot.capacities.has_value()) {
    const auto &devices = snapshot.capacities.value().devices;
    if (!valid_rows(devices, [](const auto &device) -> const std::string & {
          return device.device;
        })) {
      return Result_state::failed;
    }
    for (const auto &device : devices) {
      if (device.sector_size_bytes == 0) return Result_state::failed;
      auto &record = records[device.device];
      record.m_device_name = device.device;
      record.m_total_bytes = device.capacity_bytes;
      record.m_sector_size_bytes = device.sector_size_bytes;
      record.m_capacity_available = true;
    }
  }

  if (snapshot.read_write.has_value()) {
    const auto &devices = snapshot.read_write.value().devices;
    if (!valid_rows(devices, [](const auto &device) -> const std::string & {
          return device.device;
        })) {
      return Result_state::failed;
    }
    for (const auto &device : devices) {
      auto &record = records[device.device];
      record.m_device_name = device.device;
      record.m_reads = device.read_count;
      record.m_read_bytes = device.read_bytes;
      record.m_writes = device.write_count;
      record.m_write_bytes = device.write_bytes;
      if (!convert_milliseconds(device.read_time, record.m_read_time_ms) ||
          !convert_milliseconds(device.write_time, record.m_write_time_ms)) {
        return Result_state::failed;
      }
      record.m_read_write_available = true;
    }
  }

  if (snapshot.flush.has_value()) {
    const auto &devices = snapshot.flush.value().devices;
    if (!valid_rows(devices, [](const auto &device) -> const std::string & {
          return device.device;
        })) {
      return Result_state::failed;
    }
    for (const auto &device : devices) {
      auto &record = records[device.device];
      record.m_device_name = device.device;
      record.m_flushes = device.flush_count;
      if (!convert_milliseconds(device.flush_time, record.m_flush_time_ms)) {
        return Result_state::failed;
      }
      record.m_flush_available = true;
    }
  }

  if (snapshot.hierarchy.has_value()) {
    const auto &relationships = snapshot.hierarchy.value().relationships;
    if (!valid_rows(
            relationships,
            [](const auto &relationship) -> const std::string & {
              return relationship.device;
            },
            true)) {
      return Result_state::failed;
    }
    for (const auto &relationship : relationships) {
      if (!valid_name(relationship.parent_device) ||
          relationship.device == relationship.parent_device ||
          !records.contains(relationship.device) ||
          !records.contains(relationship.parent_device) ||
          !records[relationship.device].m_device_kind_available ||
          !records[relationship.device].m_partition ||
          !records[relationship.parent_device].m_device_kind_available ||
          records[relationship.parent_device].m_partition) {
        return Result_state::failed;
      }
      records[relationship.device].m_parent_disk_name =
          relationship.parent_device;
      records[relationship.device].m_hierarchy_available = true;
    }
    for (auto &[name, record] : records) {
      (void)name;
      if (!record.m_device_kind_available) continue;
      if (record.m_partition && !record.m_hierarchy_available) {
        return Result_state::failed;
      }
      if (!record.m_partition) record.m_hierarchy_available = true;
    }
  }

  if (records.empty()) {
    return has_row_producing_failure(snapshot) ? Result_state::failed
                                               : Result_state::unavailable;
  }

  std::vector<System_Disk_Stats> converted;
  converted.reserve(records.size());
  for (auto &[name, record] : records) {
    (void)name;
    record.m_timestamp = timestamp;
    converted.push_back(std::move(record));
  }

  output.insert(output.end(), std::make_move_iterator(converted.begin()),
                std::make_move_iterator(converted.end()));
  return Result_state::available;
}

System_Disk_Table_Row make_system_disk_table_row(
    const System_Disk_Stats &record) {
  System_Disk_Table_Row row;
  row.m_timestamp = record.m_timestamp;
  row.m_device_name = record.m_device_name;
  row.m_reads = record.m_reads;
  row.m_read_bytes = record.m_read_bytes;
  row.m_read_time_ms = record.m_read_time_ms;
  row.m_writes = record.m_writes;
  row.m_write_bytes = record.m_write_bytes;
  row.m_write_time_ms = record.m_write_time_ms;
  row.m_flushes = record.m_flushes;
  row.m_flush_time_ms = record.m_flush_time_ms;
  row.m_read_write_available = record.m_read_write_available;
  row.m_flush_available = record.m_flush_available;
  return row;
}

bool system_disk_table_column_is_null(const System_Disk_Table_Row &row,
                                      const unsigned int index) {
  switch (index) {
    case 2:  // READ_COUNT
    case 3:  // READ_BYTES
    case 4:  // READ_TIME_MS
    case 5:  // WRITE_COUNT
    case 6:  // WRITE_BYTES
    case 7:  // WRITE_TIME_MS
      return !row.m_read_write_available;
    case 8:  // FLUSHES
    case 9:  // FLUSH_TIME_MS
      return !row.m_flush_available;
    default:
      assert(false);
      return false;
  }
}

bool system_disk_read_write_metric_eligible(const System_Disk_Stats &record,
                                            const bool partition) {
  return record.m_device_kind_available && record.m_partition == partition &&
         record.m_read_write_available;
}

bool system_disk_flush_metric_eligible(const System_Disk_Stats &record,
                                       const bool partition) {
  return record.m_device_kind_available && record.m_partition == partition &&
         record.m_flush_available;
}

}  // namespace hm

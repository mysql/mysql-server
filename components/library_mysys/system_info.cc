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

#include "mysql/components/library_mysys/system_info.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "components/library_mysys/my_system_api/system_info_platform.h"

namespace mysql::system_info {
namespace {

[[nodiscard]] bool valid_processor_label(std::string_view label) {
  return label.size() > 3 && label.starts_with("cpu") &&
         std::all_of(label.begin() + 3, label.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

[[nodiscard]] bool valid_storage_device_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." &&
         name.find('/') == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

template <typename Row, typename Name>
[[nodiscard]] bool valid_storage_rows(const std::vector<Row> &rows, Name name,
                                      bool allow_empty = false) {
  if (rows.empty()) return allow_empty;
  std::unordered_set<std::string_view> names;
  names.reserve(rows.size());
  return std::all_of(rows.begin(), rows.end(), [&](const Row &row) {
    const std::string_view device = name(row);
    return valid_storage_device_name(device) && names.insert(device).second;
  });
}

template <typename Cpu>
[[nodiscard]] bool valid_cpu_rows(const std::vector<Cpu> &cpus) {
  if (cpus.empty() || cpus.front().cpu != "cpu") return false;

  std::unordered_set<std::string_view> labels;
  labels.reserve(cpus.size());
  for (size_t i = 0; i < cpus.size(); ++i) {
    if ((i != 0 && !valid_processor_label(cpus[i].cpu)) ||
        !labels.insert(cpus[i].cpu).second) {
      return false;
    }
  }
  return true;
}

template <typename Thread>
[[nodiscard]] bool valid_thread_rows(const std::vector<Thread> &threads) {
  if (threads.empty()) return false;
  std::unordered_set<uint64_t> tids;
  tids.reserve(threads.size());
  return std::all_of(threads.begin(), threads.end(), [&](const Thread &thread) {
    return thread.tid > 0 && tids.insert(thread.tid).second;
  });
}

template <typename Thread>
[[nodiscard]] bool thread_rows_align(
    const std::vector<Thread_identity> &inventory,
    const std::vector<Thread> &threads) {
  return inventory.size() == threads.size() &&
         std::equal(inventory.begin(), inventory.end(), threads.begin(),
                    [](const Thread_identity &left, const Thread &right) {
                      return left.tid == right.tid;
                    });
}

}  // namespace

bool internal::cpu_ticks_to_milliseconds(
    uint64_t ticks, uint64_t ticks_per_second,
    std::chrono::milliseconds &milliseconds) {
  if (ticks_per_second == 0 ||
      ticks_per_second > std::numeric_limits<uint64_t>::max() / 1000U) {
    return true;
  }

  const uint64_t whole_seconds = ticks / ticks_per_second;
  const uint64_t remaining_ticks = ticks % ticks_per_second;
  const uint64_t maximum =
      static_cast<uint64_t>(std::chrono::milliseconds::max().count());
  if (whole_seconds > maximum / 1000U) return true;

  const uint64_t whole_milliseconds = whole_seconds * 1000U;
  const uint64_t fractional_milliseconds =
      remaining_ticks * 1000U / ticks_per_second;
  if (fractional_milliseconds > maximum - whole_milliseconds) return true;

  milliseconds =
      std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(
          whole_milliseconds + fractional_milliseconds)};
  return false;
}

Filesystem_snapshot internal::validate_filesystem_snapshot(
    Filesystem_snapshot snapshot) {
  if (snapshot.usage.has_value()) {
    const auto &usage = snapshot.usage.value();
    if (usage.capacity_bytes == 0 ||
        usage.available_bytes > usage.capacity_bytes) {
      snapshot.usage = System_result<Filesystem_usage>::failure(
          std::make_error_code(std::errc::invalid_argument));
    }
  }
  return snapshot;
}

Filesystem_snapshot query_filesystem(std::string_view path) {
  return internal::validate_filesystem_snapshot(
      internal::query_filesystem_platform(path));
}

Storage_snapshot internal::validate_storage_snapshot(
    Storage_snapshot snapshot) {
  const auto error = std::make_error_code(std::errc::invalid_argument);
  if (snapshot.inventory.has_value()) {
    const auto &devices = snapshot.inventory.value().devices;
    const bool valid = valid_storage_rows(
        devices, [](const Storage_device &device) -> const std::string & {
          return device.name;
        });
    const bool valid_kinds =
        std::all_of(devices.begin(), devices.end(), [](const auto &device) {
          return device.kind != Storage_device_kind::unknown;
        });
    if (!valid || !valid_kinds) {
      snapshot.inventory = System_result<Storage_inventory>::failure(error);
    }
  }
  if (snapshot.capacities.has_value()) {
    const auto &devices = snapshot.capacities.value().devices;
    const bool valid = valid_storage_rows(
        devices,
        [](const Storage_device_capacity &device) -> const std::string & {
          return device.device;
        });
    const bool valid_sectors = std::all_of(
        devices.begin(), devices.end(),
        [](const auto &device) { return device.sector_size_bytes > 0; });
    if (!valid || !valid_sectors) {
      snapshot.capacities = System_result<Storage_capacities>::failure(error);
    }
  }
  if (snapshot.inventory.has_value() && snapshot.capacities.has_value()) {
    const auto &inventory = snapshot.inventory.value().devices;
    const auto &capacities = snapshot.capacities.value().devices;
    const bool align =
        inventory.size() == capacities.size() &&
        std::equal(inventory.begin(), inventory.end(), capacities.begin(),
                   [](const Storage_device &left,
                      const Storage_device_capacity &right) {
                     return left.name == right.device;
                   });
    if (!align) {
      snapshot.capacities = System_result<Storage_capacities>::failure(error);
    }
  }
  if (snapshot.hierarchy.has_value()) {
    const auto &relationships = snapshot.hierarchy.value().relationships;
    bool valid = snapshot.inventory.has_value() &&
                 valid_storage_rows(
                     relationships,
                     [](const Storage_device_relationship &relationship)
                         -> const std::string & { return relationship.device; },
                     true);
    if (valid) {
      valid = std::all_of(
          relationships.begin(), relationships.end(),
          [](const auto &relationship) {
            return valid_storage_device_name(relationship.parent_device) &&
                   relationship.device != relationship.parent_device;
          });
    }
    if (valid) {
      const auto &devices = snapshot.inventory.value().devices;
      std::unordered_map<std::string_view, Storage_device_kind> kinds;
      kinds.reserve(devices.size());
      for (const auto &device : devices) {
        kinds.emplace(device.name, device.kind);
      }
      valid = std::all_of(relationships.begin(), relationships.end(),
                          [&](const auto &relationship) {
                            const auto child = kinds.find(relationship.device);
                            return child != kinds.end() &&
                                   child->second ==
                                       Storage_device_kind::partition &&
                                   kinds.contains(relationship.parent_device);
                          });
    }
    if (!valid) {
      snapshot.hierarchy = System_result<Storage_hierarchy>::failure(error);
    }
  }
  if (snapshot.read_write.has_value() &&
      !valid_storage_rows(
          snapshot.read_write.value().devices,
          [](const Storage_read_write_counters &device) -> const std::string & {
            return device.device;
          })) {
    snapshot.read_write =
        System_result<Storage_read_write_snapshot>::failure(error);
  }
  if (snapshot.flush.has_value() &&
      !valid_storage_rows(
          snapshot.flush.value().devices,
          [](const Storage_flush_counters &device) -> const std::string & {
            return device.device;
          })) {
    snapshot.flush = System_result<Storage_flush_snapshot>::failure(error);
  }
  return snapshot;
}

Storage_snapshot query_storage_devices() {
  return internal::validate_storage_snapshot(
      internal::query_storage_devices_platform());
}

Host_memory_snapshot internal::validate_host_memory_snapshot(
    Host_memory_snapshot snapshot) {
  if (snapshot.memory.has_value()) {
    const auto &memory = snapshot.memory.value();
    if (memory.total_bytes == 0 || memory.free_bytes > memory.total_bytes ||
        memory.available_bytes > memory.total_bytes) {
      snapshot.memory = System_result<Host_memory_info>::failure(
          std::make_error_code(std::errc::invalid_argument));
    }
  }
  if (snapshot.swap_capacity.has_value()) {
    const auto &swap = snapshot.swap_capacity.value();
    if (swap.free_bytes > swap.total_bytes) {
      snapshot.swap_capacity = System_result<Swap_capacity_info>::failure(
          std::make_error_code(std::errc::invalid_argument));
    }
  }
  return snapshot;
}

Host_memory_snapshot query_host_memory() {
  return internal::validate_host_memory_snapshot(
      internal::query_host_memory_platform());
}

Host_cpu_snapshot internal::validate_host_cpu_snapshot(
    Host_cpu_snapshot snapshot) {
  const auto error = std::make_error_code(std::errc::invalid_argument);
  if (snapshot.times.has_value() &&
      !valid_cpu_rows(snapshot.times.value().cpus)) {
    snapshot.times = System_result<Cpu_times_snapshot>::failure(error);
  }
  if (snapshot.extended_times.has_value() &&
      !valid_cpu_rows(snapshot.extended_times.value().cpus)) {
    snapshot.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
  }
  if (snapshot.times.has_value() && snapshot.extended_times.has_value()) {
    const auto &times = snapshot.times.value().cpus;
    const auto &extended = snapshot.extended_times.value().cpus;
    const bool labels_match =
        times.size() == extended.size() &&
        std::equal(times.begin(), times.end(), extended.begin(),
                   [](const Cpu_times &left, const Cpu_extended_times &right) {
                     return left.cpu == right.cpu;
                   });
    if (!labels_match) {
      snapshot.extended_times =
          System_result<Cpu_extended_snapshot>::failure(error);
    }
  }
  return snapshot;
}

Host_cpu_snapshot query_host_cpu() {
  return internal::validate_host_cpu_snapshot(
      internal::query_host_cpu_platform());
}

Process_memory_snapshot internal::validate_process_memory_snapshot(
    Process_memory_snapshot snapshot) {
  if (snapshot.identity.has_value()) {
    const auto &identity = snapshot.identity.value();
    if (identity.pid == 0 || identity.name.empty()) {
      snapshot.identity = System_result<Process_identity>::failure(
          std::make_error_code(std::errc::invalid_argument));
    }
  }
  return snapshot;
}

Process_memory_snapshot query_process_memory() {
  return internal::validate_process_memory_snapshot(
      internal::query_process_memory_platform());
}

Process_threads_snapshot internal::validate_process_threads_snapshot(
    Process_threads_snapshot snapshot) {
  const auto error = std::make_error_code(std::errc::invalid_argument);
  if (snapshot.identity.has_value()) {
    const auto &identity = snapshot.identity.value();
    if (identity.pid == 0 || identity.name.empty()) {
      snapshot.identity = System_result<Process_identity>::failure(error);
    }
  }
  if (snapshot.threads.has_value() &&
      !valid_thread_rows(snapshot.threads.value().threads)) {
    snapshot.threads = System_result<Thread_inventory>::failure(error);
  }
  if (snapshot.runtime.has_value() &&
      snapshot.runtime.value().thread_count == 0) {
    snapshot.runtime = System_result<Process_runtime_info>::failure(error);
  }
  if (snapshot.cpu.has_value() &&
      !valid_thread_rows(snapshot.cpu.value().threads)) {
    snapshot.cpu = System_result<Thread_cpu_snapshot>::failure(error);
  }
  if (snapshot.extended_cpu.has_value() &&
      !valid_thread_rows(snapshot.extended_cpu.value().threads)) {
    snapshot.extended_cpu =
        System_result<Thread_extended_cpu_snapshot>::failure(error);
  }
  if (snapshot.scheduler.has_value() &&
      !valid_thread_rows(snapshot.scheduler.value().threads)) {
    snapshot.scheduler =
        System_result<Thread_scheduler_snapshot>::failure(error);
  }
  if (snapshot.io.has_value() &&
      !valid_thread_rows(snapshot.io.value().threads)) {
    snapshot.io = System_result<Thread_io_snapshot>::failure(error);
  }

  if (snapshot.threads.has_value()) {
    const auto &inventory = snapshot.threads.value().threads;
    if (snapshot.cpu.has_value() &&
        !thread_rows_align(inventory, snapshot.cpu.value().threads)) {
      snapshot.cpu = System_result<Thread_cpu_snapshot>::failure(error);
    }
    if (snapshot.extended_cpu.has_value() &&
        !thread_rows_align(inventory, snapshot.extended_cpu.value().threads)) {
      snapshot.extended_cpu =
          System_result<Thread_extended_cpu_snapshot>::failure(error);
    }
    if (snapshot.scheduler.has_value() &&
        !thread_rows_align(inventory, snapshot.scheduler.value().threads)) {
      snapshot.scheduler =
          System_result<Thread_scheduler_snapshot>::failure(error);
    }
    if (snapshot.io.has_value() &&
        !thread_rows_align(inventory, snapshot.io.value().threads)) {
      snapshot.io = System_result<Thread_io_snapshot>::failure(error);
    }
  }
  return snapshot;
}

Process_threads_snapshot query_process_threads() {
  return internal::validate_process_threads_snapshot(
      internal::query_process_threads_platform());
}

}  // namespace mysql::system_info

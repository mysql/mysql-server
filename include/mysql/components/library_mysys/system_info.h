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

#include <cassert>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mysql::system_info {

/** State of one system information capability. */
enum class Result_state { available, unavailable, failed };

/**
  Result of collecting one system information capability.

  An available result contains one complete value. An unavailable result means
  that the platform has no supported equivalent. A failed result means that
  collection was attempted but failed.

  @tparam T Collected capability type
*/
template <typename T>
class System_result {
 public:
  /** Construct an unavailable result. */
  System_result() = default;

  /**
    Construct an available result.

    @param value Complete capability value
    @return Available result containing value
  */
  [[nodiscard]] static System_result success(T value) {
    System_result result;
    result.m_state = Result_state::available;
    result.m_value.emplace(std::move(value));
    return result;
  }

  /** @return An unavailable result. */
  [[nodiscard]] static System_result unavailable() { return System_result{}; }

  /**
    Construct a failed result.

    @param error Collection error
    @return Failed result containing error
  */
  [[nodiscard]] static System_result failure(std::error_code error) {
    System_result result;
    result.m_state = Result_state::failed;
    result.m_error = error;
    return result;
  }

  /** @return Current capability state. */
  [[nodiscard]] Result_state state() const noexcept { return m_state; }

  /** @return true if this result contains an available value. */
  [[nodiscard]] bool has_value() const noexcept {
    return m_state == Result_state::available;
  }

  /**
    Access the available value.

    @pre has_value() is true.
  */
  [[nodiscard]] const T &value() const & {
    assert(has_value());
    return *m_value;
  }

  /**
    Access the available value.

    @pre has_value() is true.
  */
  [[nodiscard]] T &value() & {
    assert(has_value());
    return *m_value;
  }

  /**
    Move the value out of an available temporary result.

    @pre has_value() is true.
  */
  [[nodiscard]] T value() && {
    assert(has_value());
    return std::move(*m_value);
  }

  /**
    Copy the value out of an available const temporary result.

    @pre has_value() is true.
  */
  [[nodiscard]] T value() const && {
    assert(has_value());
    return *m_value;
  }

  /**
    Access the collection error.

    @pre state() is Result_state::failed.
  */
  [[nodiscard]] const std::error_code &error() const &noexcept {
    assert(m_state == Result_state::failed);
    return m_error;
  }

  /**
    Move the error out of a failed temporary result.

    @pre state() is Result_state::failed.
  */
  [[nodiscard]] std::error_code error() &&noexcept {
    assert(m_state == Result_state::failed);
    return std::move(m_error);
  }

  /**
    Copy the error out of a failed const temporary result.

    @pre state() is Result_state::failed.
  */
  [[nodiscard]] std::error_code error() const &&noexcept {
    assert(m_state == Result_state::failed);
    return m_error;
  }

 private:
  std::optional<T> m_value;
  std::error_code m_error;
  Result_state m_state{Result_state::unavailable};
};

/** Stable identity of the filesystem that contains a monitored path. */
struct Filesystem_identity {
  std::string filesystem_id;
  std::string mount_point;
  std::string mount_source;
  std::string filesystem_type;
};

/** Capacity and space available to unprivileged processes, in bytes. */
struct Filesystem_usage {
  uint64_t capacity_bytes{0};
  uint64_t available_bytes{0};
};

/** Filesystem identity and usage collected for one monitored path. */
struct Filesystem_snapshot {
  System_result<Filesystem_identity> identity;
  System_result<Filesystem_usage> usage;
};

/** Kind of storage device represented by an inventory entry. */
enum class Storage_device_kind {
  disk,
  partition,
  volume,
  virtual_device,
  unknown
};

/** Identity and kind of one storage device. */
struct Storage_device {
  std::string name;
  Storage_device_kind kind{Storage_device_kind::unknown};
};

/** Storage devices visible at one collection point. */
struct Storage_inventory {
  std::vector<Storage_device> devices;
};

/** Capacity and hardware sector size of one storage device, in bytes. */
struct Storage_device_capacity {
  std::string device;
  uint64_t capacity_bytes{0};
  uint64_t sector_size_bytes{0};
};

/** Storage device capacities collected at one collection point. */
struct Storage_capacities {
  std::vector<Storage_device_capacity> devices;
};

/** Child and parent relationship between two storage devices. */
struct Storage_device_relationship {
  std::string device;
  std::string parent_device;
};

/** Storage device relationships collected at one collection point. */
struct Storage_hierarchy {
  std::vector<Storage_device_relationship> relationships;
};

/** Cumulative read and write counters for one storage device. */
struct Storage_read_write_counters {
  std::string device;
  uint64_t read_count{0};
  uint64_t read_bytes{0};
  std::chrono::milliseconds read_time{0};
  uint64_t write_count{0};
  uint64_t write_bytes{0};
  std::chrono::milliseconds write_time{0};
};

/** Storage read and write counters collected at one collection point. */
struct Storage_read_write_snapshot {
  std::vector<Storage_read_write_counters> devices;
};

/** Cumulative flush counters for one storage device. */
struct Storage_flush_counters {
  std::string device;
  uint64_t flush_count{0};
  std::chrono::milliseconds flush_time{0};
};

/** Storage flush counters collected at one collection point. */
struct Storage_flush_snapshot {
  std::vector<Storage_flush_counters> devices;
};

/** Storage capabilities collected at one collection point. */
struct Storage_snapshot {
  System_result<Storage_inventory> inventory;
  System_result<Storage_capacities> capacities;
  System_result<Storage_hierarchy> hierarchy;
  System_result<Storage_read_write_snapshot> read_write;
  System_result<Storage_flush_snapshot> flush;
};

/** Portable host physical memory values, in bytes. */
struct Host_memory_info {
  uint64_t total_bytes{0};
  uint64_t free_bytes{0};
  uint64_t available_bytes{0};
};

/** Optional host memory breakdown values, in bytes. */
struct Host_memory_breakdown {
  uint64_t buffer_bytes{0};
  uint64_t cache_bytes{0};
  uint64_t slab_bytes{0};
};

/** Host swap capacity values, in bytes. */
struct Swap_capacity_info {
  uint64_t total_bytes{0};
  uint64_t free_bytes{0};
};

/** Cumulative host swap activity, in bytes. */
struct Swap_activity_info {
  uint64_t bytes_in{0};
  uint64_t bytes_out{0};
};

/** Host memory capabilities collected at one collection point. */
struct Host_memory_snapshot {
  System_result<Host_memory_info> memory;
  System_result<Host_memory_breakdown> breakdown;
  System_result<Swap_capacity_info> swap_capacity;
  System_result<Swap_activity_info> swap_activity;
};

/** Portable cumulative CPU times for one aggregate or logical CPU. */
struct Cpu_times {
  std::string cpu;
  std::chrono::milliseconds user{0};
  std::chrono::milliseconds nice{0};
  std::chrono::milliseconds system{0};
  std::chrono::milliseconds idle{0};
};

/** Portable CPU times collected at one collection point. */
struct Cpu_times_snapshot {
  std::vector<Cpu_times> cpus;
};

/** Optional cumulative CPU times without portable equivalents. */
struct Cpu_extended_times {
  std::string cpu;
  std::chrono::milliseconds io_wait{0};
  std::chrono::milliseconds irq{0};
  std::chrono::milliseconds soft_irq{0};
  std::chrono::milliseconds steal{0};
  std::chrono::milliseconds guest{0};
  std::chrono::milliseconds guest_nice{0};
};

/** Extended CPU times collected at one collection point. */
struct Cpu_extended_snapshot {
  std::vector<Cpu_extended_times> cpus;
};

/** Host CPU capabilities collected at one collection point. */
struct Host_cpu_snapshot {
  System_result<Cpu_times_snapshot> times;
  System_result<Cpu_extended_snapshot> extended_times;
};

/** Identity of the current process. */
struct Process_identity {
  uint64_t pid{0};
  std::string name;
};

/** Resident memory of the current process, in bytes. */
struct Process_memory_residency {
  uint64_t resident_bytes{0};
};

/** Optional memory details of the current process, in bytes. */
struct Process_memory_details {
  uint64_t data_bytes{0};
  uint64_t swap_bytes{0};
};

/** Cumulative major page faults of the current process. */
struct Process_page_faults {
  uint64_t major_faults{0};
};

/** Process memory capabilities collected at one collection point. */
struct Process_memory_snapshot {
  System_result<Process_identity> identity;
  System_result<Process_memory_residency> residency;
  System_result<Process_memory_details> details;
  System_result<Process_page_faults> page_faults;
};

/** Portable execution state of a process thread. */
enum class Thread_state {
  running,
  sleeping,
  waiting,
  stopped,
  zombie,
  unknown
};

/** Identity, name and state of one process thread. */
struct Thread_identity {
  uint64_t tid{0};
  std::string name;
  Thread_state state{Thread_state::unknown};
};

/** Process threads visible at one collection point. */
struct Thread_inventory {
  std::vector<Thread_identity> threads;
};

/** Process-wide runtime values for the current process. */
struct Process_runtime_info {
  uint32_t thread_count{0};
  uint64_t virtual_bytes{0};
  uint64_t resident_bytes{0};
  uint64_t resident_limit_bytes{0};
};

/** Portable cumulative CPU times for one process thread. */
struct Thread_cpu_times {
  uint64_t tid{0};
  std::chrono::milliseconds user{0};
  std::chrono::milliseconds system{0};
};

/** Portable thread CPU times collected at one collection point. */
struct Thread_cpu_snapshot {
  std::vector<Thread_cpu_times> threads;
};

/** Optional cumulative CPU times for one process thread. */
struct Thread_extended_cpu_times {
  uint64_t tid{0};
  std::chrono::milliseconds child_user{0};
  std::chrono::milliseconds child_system{0};
  std::chrono::milliseconds guest{0};
  std::chrono::milliseconds child_guest{0};
};

/** Extended thread CPU times collected at one collection point. */
struct Thread_extended_cpu_snapshot {
  std::vector<Thread_extended_cpu_times> threads;
};

/** Optional scheduler values for one process thread. */
struct Thread_scheduler_info {
  uint64_t tid{0};
  uint32_t last_cpu{0};
  std::chrono::milliseconds block_io_delay{0};
};

/** Thread scheduler values collected at one collection point. */
struct Thread_scheduler_snapshot {
  std::vector<Thread_scheduler_info> threads;
};

/** Cumulative I/O byte counters for one process thread. */
struct Thread_io_counters {
  uint64_t tid{0};
  uint64_t read_bytes{0};
  uint64_t write_bytes{0};
};

/** Thread I/O counters collected at one collection point. */
struct Thread_io_snapshot {
  std::vector<Thread_io_counters> threads;
};

/** Process thread capabilities collected at one collection point. */
struct Process_threads_snapshot {
  System_result<Process_identity> identity;
  System_result<Thread_inventory> threads;
  System_result<Process_runtime_info> runtime;
  System_result<Thread_cpu_snapshot> cpu;
  System_result<Thread_extended_cpu_snapshot> extended_cpu;
  System_result<Thread_scheduler_snapshot> scheduler;
  System_result<Thread_io_snapshot> io;
};

/**
  Collect identity and usage for the filesystem containing a path.

  @param path Path whose filesystem is queried
  @return Filesystem capability results
*/
[[nodiscard]] Filesystem_snapshot query_filesystem(std::string_view path);

/** @return Storage capabilities collected at one collection point. */
[[nodiscard]] Storage_snapshot query_storage_devices();

/** @return Host memory capabilities collected at one collection point. */
[[nodiscard]] Host_memory_snapshot query_host_memory();

/** @return Host CPU capabilities collected at one collection point. */
[[nodiscard]] Host_cpu_snapshot query_host_cpu();

/** @return Current process memory capabilities. */
[[nodiscard]] Process_memory_snapshot query_process_memory();

/** @return Current process thread capabilities. */
[[nodiscard]] Process_threads_snapshot query_process_threads();

}  // namespace mysql::system_info

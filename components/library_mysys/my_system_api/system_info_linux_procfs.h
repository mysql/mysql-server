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

#include <functional>
#include <istream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "mysql/components/library_mysys/system_info.h"

namespace mysql::system_info::internal {

/** Linux block device identity parsed from /proc/partitions. */
struct Linux_block_device {
  uint64_t major{0};
  uint64_t minor{0};
  std::string name;
};

/** Linux storage I/O capabilities parsed from /proc/diskstats. */
struct Linux_diskstats_snapshot {
  System_result<Storage_read_write_snapshot> read_write;
  System_result<Storage_flush_snapshot> flush;
};

/** Linux procfs and sysfs roots used by storage collection. */
struct Linux_storage_paths {
  std::string proc_partitions;
  std::string proc_diskstats;
  std::string sys_class_block;
};

/** Assemble Linux storage capabilities from the supplied proc and sys roots. */
[[nodiscard]] Storage_snapshot query_linux_storage_devices(
    const Linux_storage_paths &paths,
    const std::function<void()> &after_metadata = {});

/** Check that a Linux device name is one safe path component. */
[[nodiscard]] bool valid_linux_storage_device_name(std::string_view name);

/** Read Linux block device identities from partitions input. */
[[nodiscard]] System_result<std::vector<Linux_block_device>>
read_linux_partitions(std::istream &input);

/** Read one Linux block device capacity and hardware sector size. */
[[nodiscard]] System_result<Storage_device_capacity>
read_linux_storage_capacity(std::istream &size_input,
                            std::istream &sector_size_input,
                            std::string_view device);

/** Resolve a partition relationship from its canonical sysfs path. */
[[nodiscard]] System_result<Storage_device_relationship>
linux_partition_relationship(std::string_view canonical_path,
                             std::string_view device);

/** Select the device queue attribute used for hardware sector size. */
[[nodiscard]] System_result<std::string> linux_storage_sector_size_path(
    std::string_view canonical_path, bool partition);

/** Read Linux block device I/O capabilities from diskstats input. */
[[nodiscard]] Linux_diskstats_snapshot read_linux_diskstats(
    std::istream &input, const std::unordered_set<std::string> &whole_devices);

/** Read the most specific matching mount from Linux mountinfo input. */
[[nodiscard]] System_result<Filesystem_identity> read_linux_mount_info(
    std::istream &input, std::string_view device_id,
    std::string_view target_path);

/** Read host memory capabilities from Linux meminfo input. */
[[nodiscard]] Host_memory_snapshot read_linux_meminfo(std::istream &input);

/** Read swap activity from Linux vmstat input. */
[[nodiscard]] System_result<Swap_activity_info> read_linux_vmstat(
    std::istream &input);

/** Read host CPU capabilities from Linux stat input. */
[[nodiscard]] Host_cpu_snapshot read_linux_cpu_stat(std::istream &input,
                                                    uint64_t ticks_per_second);

/** Read process identity and memory from Linux status input. */
[[nodiscard]] Process_memory_snapshot read_linux_process_status(
    std::istream &input);

/** Read process thread capabilities from one Linux stat record. */
[[nodiscard]] Process_threads_snapshot read_linux_thread_stat(
    std::istream &input, uint64_t expected_tid, uint64_t process_pid,
    uint64_t ticks_per_second, uint64_t page_size);

/** Read process thread I/O counters from one Linux io record. */
[[nodiscard]] System_result<Thread_io_counters> read_linux_thread_io(
    std::istream &input, uint64_t tid);

}  // namespace mysql::system_info::internal

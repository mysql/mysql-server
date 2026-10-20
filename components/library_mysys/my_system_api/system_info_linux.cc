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

#include "components/library_mysys/my_system_api/system_info_platform.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "components/library_mysys/my_system_api/system_info_linux_procfs.h"
#include "components/library_mysys/my_system_api/system_info_utils.h"

namespace mysql::system_info::internal {
namespace {

/**
  Linux sources used by the platform queries in this file.

  Source                         Contents
  -----------------------------  -------------------------------------------
  stat()                         Filesystem device major and minor numbers
  /proc/self/mountinfo           Filesystem and mount identity
  statvfs()                      Filesystem capacity and available space
  /proc/partitions               Block device inventory
  /sys/class/block               Device capacity and partition relationships
  /proc/diskstats                Block device I/O counters
  /proc/meminfo                  Host memory and swap capacity
  /proc/vmstat                   Host paging activity
  /proc/stat                     Host CPU times
  /proc/self/status              Process identity and memory use
  getrusage(RUSAGE_SELF)         Process major page faults
  /proc/self/task                Current process thread IDs
  `/proc/self/task/<tid>/stat`   Thread state, CPU and runtime data
  `/proc/self/task/<tid>/io`     Thread I/O counters

  This file opens the native sources and reports operating system errors. The
  readers in `system_info_linux_procfs.cc` validate and convert their text
  records into typed system information snapshots.
*/

/** Parse a positive thread ID from a /proc/self/task directory name. */
[[nodiscard]] bool parse_tid(std::string_view text, uint64_t &tid) {
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), tid);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size() && tid > 0;
}

/** Return `errno`, or a general I/O error when no value was recorded. */
[[nodiscard]] std::error_code io_error() {
  auto error = errno_code();
  if (!error) error = std::make_error_code(std::errc::io_error);
  return error;
}

}  // namespace

/**
  Collect Linux filesystem identity and space metadata for one path.

  Source and field                  Contents
  --------------------------------  -----------------------------------------
  stat() st_dev                     Filesystem device major and minor numbers
  /proc/self/mountinfo major:minor  Filesystem ID and mount point
  statvfs() f_blocks                Total number of filesystem blocks
  statvfs() f_bavail                Blocks available to this caller
  statvfs() f_frsize                Size of each block in bytes

  The device number from stat() joins the path to a mount record in the
  calling process's mount namespace. The canonical path is used to select the
  most specific matching mount point.

  The `identity` and `usage` capabilities use separate queries. Failure of one
  does not prevent collection of the other.
*/
Filesystem_snapshot query_filesystem_platform(std::string_view path) {
  Filesystem_snapshot result;
  // An empty path cannot identify a filesystem or provide its space usage
  if (path.empty()) {
    const auto error = std::make_error_code(std::errc::invalid_argument);
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  const std::string path_string{path};
  std::error_code canonical_error;
  // Resolve symbolic links and relative components before matching mount
  // points from the current process's mount namespace.
  const auto canonical_path =
      std::filesystem::canonical(path_string, canonical_error);
  struct stat path_status {};
  if (canonical_error || stat(path_string.c_str(), &path_status) != 0) {
    const auto error = canonical_error ? canonical_error : errno_code();
    result.identity = System_result<Filesystem_identity>::failure(error);
  } else {
    // mountinfo identifies a filesystem with the major and minor values from
    // the device number returned by stat().
    const std::string device_id = std::to_string(major(path_status.st_dev)) +
                                  ":" +
                                  std::to_string(minor(path_status.st_dev));
    std::ifstream mount_info{"/proc/self/mountinfo"};
    if (!mount_info.is_open()) {
      auto error = errno_code();
      if (!error) error = std::make_error_code(std::errc::io_error);
      result.identity = System_result<Filesystem_identity>::failure(error);
    } else {
      result.identity = read_linux_mount_info(mount_info, device_id,
                                              canonical_path.generic_string());
      // A stream error invalidates the result even if the parser read a
      // matching mount record first.
      if (mount_info.bad()) {
        auto error = errno_code();
        if (!error) error = std::make_error_code(std::errc::io_error);
        result.identity = System_result<Filesystem_identity>::failure(error);
      }
    }
  }

  // Collect space usage independently of filesystem identity.
  struct statvfs usage {};
  if (statvfs(path_string.c_str(), &usage) != 0) {
    result.usage = System_result<Filesystem_usage>::failure(errno_code());
    return result;
  }
  const uint64_t block_size = usage.f_frsize;
  // Reject incomplete values and calculations that cannot fit in bytes.
  if (block_size == 0 || usage.f_blocks == 0 ||
      usage.f_bavail > usage.f_blocks ||
      multiply_overflows(usage.f_blocks, block_size) ||
      multiply_overflows(usage.f_bavail, block_size)) {
    result.usage = System_result<Filesystem_usage>::failure(
        std::make_error_code(std::errc::value_too_large));
    return result;
  }
  // f_bavail reports blocks available to an unprivileged caller.
  result.usage = System_result<Filesystem_usage>::success(
      {usage.f_blocks * block_size, usage.f_bavail * block_size});
  return result;
}

/**
  Collect Linux storage device metadata from procfs and sysfs.

  Source                    Capabilities
  ------------------------  -----------------------------------------------
  /proc/partitions          `inventory`
  /sys/class/block          `capacities` and `hierarchy`
  /proc/diskstats           `read_write` and `flush`

  `query_linux_storage_devices()` joins these records by device name. It also
  accounts for devices that disappear while the sources are being read.
*/
Storage_snapshot query_storage_devices_platform() {
  return query_linux_storage_devices(
      {"/proc/partitions", "/proc/diskstats", "/sys/class/block"});
}

/**
  Collect Linux host memory from procfs.

  Capability       Linux source and fields
  ---------------  --------------------------------------------------------
  `memory`         /proc/meminfo: MemTotal, MemFree and MemAvailable
  `breakdown`      /proc/meminfo: Buffers, Cached and Slab
  `swap_capacity`  /proc/meminfo: SwapTotal and SwapFree
  `swap_activity`  /proc/vmstat: pgpgin and pgpgout

  Linux provides the memory breakdown and paging counters that have no direct
  mapping in the macOS implementation. Values from /proc/meminfo are parsed
  as independent capabilities, but an error reading the file fails all three.
  /proc/vmstat is read separately, so its result is independent of the memory
  and swap capacity results.
*/
Host_memory_snapshot query_host_memory_platform() {
  Host_memory_snapshot result;
  // A file error affects every capability read from /proc/meminfo.
  std::ifstream meminfo{"/proc/meminfo"};
  if (!meminfo.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.memory = System_result<Host_memory_info>::failure(error);
    result.breakdown = System_result<Host_memory_breakdown>::failure(error);
    result.swap_capacity = System_result<Swap_capacity_info>::failure(error);
  } else {
    auto parsed = read_linux_meminfo(meminfo);
    // A stream error invalidates every value parsed from the file.
    if (meminfo.bad()) {
      auto error = errno_code();
      if (!error) error = std::make_error_code(std::errc::io_error);
      result.memory = System_result<Host_memory_info>::failure(error);
      result.breakdown = System_result<Host_memory_breakdown>::failure(error);
      result.swap_capacity = System_result<Swap_capacity_info>::failure(error);
    } else {
      result.memory = std::move(parsed.memory);
      result.breakdown = std::move(parsed.breakdown);
      result.swap_capacity = std::move(parsed.swap_capacity);
    }
  }

  // Paging activity comes from a separate procfs file and remains independent.
  std::ifstream vmstat{"/proc/vmstat"};
  if (!vmstat.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.swap_activity = System_result<Swap_activity_info>::failure(error);
  } else {
    result.swap_activity = read_linux_vmstat(vmstat);
    // A read error takes precedence over any counters parsed before it.
    if (vmstat.bad()) {
      auto error = errno_code();
      if (!error) error = std::make_error_code(std::errc::io_error);
      result.swap_activity = System_result<Swap_activity_info>::failure(error);
    }
  }
  return result;
}

/**
  Collect cumulative Linux CPU times from /proc/stat.

  Capability        Fields
  ----------------  -------------------------------------------------------
  `times`           user, nice, system and idle
  `extended_times`  iowait, irq, softirq, steal, guest and guest_nice

  Linux reports these values in clock ticks for the aggregate `cpu` row and
  each logical processor. _SC_CLK_TCK supplies the number of ticks per
  second so the parser can convert each counter to milliseconds.

  The macOS processor record provides the four portable CPU times but no
  values with the same semantics as the Linux extended fields.
*/
Host_cpu_snapshot query_host_cpu_platform() {
  Host_cpu_snapshot result;
  // Both CPU capabilities require the system clock tick rate.
  errno = 0;
  const long ticks_per_second = sysconf(_SC_CLK_TCK);
  if (ticks_per_second <= 0) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::invalid_argument);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
    return result;
  }

  // Both capabilities come from the same /proc/stat stream.
  std::ifstream stat{"/proc/stat"};
  if (!stat.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
    return result;
  }

  result = read_linux_cpu_stat(stat, static_cast<uint64_t>(ticks_per_second));
  // A stream error invalidates both results even if rows were parsed first.
  if (stat.bad()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
  }
  return result;
}

/**
  Collect Linux process memory metadata.

  Capability       Linux source and fields
  ---------------  --------------------------------------------------------
  `identity`       /proc/self/status: Name and Pid
  `residency`      /proc/self/status: VmRSS
  `details`        /proc/self/status: VmData and VmSwap
  `page_faults`    getrusage(): ru_majflt

  Linux provides process data and swap values in /proc/self/status. The
  macOS process record used by this library provides resident memory but no
  values with the same semantics for those details.

  The status file and getrusage() are independent queries. Failure of one
  does not prevent collection of the other.
*/
Process_memory_snapshot query_process_memory_platform() {
  Process_memory_snapshot result;

  // A file error affects every capability read from /proc/self/status.
  std::ifstream status{"/proc/self/status"};
  if (!status.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.identity = System_result<Process_identity>::failure(error);
    result.residency = System_result<Process_memory_residency>::failure(error);
    result.details = System_result<Process_memory_details>::failure(error);
  } else {
    auto parsed = read_linux_process_status(status);
    // A stream error invalidates every value parsed from the file.
    if (status.bad()) {
      auto error = errno_code();
      if (!error) error = std::make_error_code(std::errc::io_error);
      result.identity = System_result<Process_identity>::failure(error);
      result.residency =
          System_result<Process_memory_residency>::failure(error);
      result.details = System_result<Process_memory_details>::failure(error);
    } else {
      result.identity = std::move(parsed.identity);
      result.residency = std::move(parsed.residency);
      result.details = std::move(parsed.details);
    }
  }

  // Collect major page faults independently of the status file.
  struct rusage usage {};
  errno = 0;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.page_faults = System_result<Process_page_faults>::failure(error);
  } else if (!std::in_range<uint64_t>(usage.ru_majflt)) {
    // Reject a negative count or a value that cannot fit the public type.
    result.page_faults = System_result<Process_page_faults>::failure(
        std::make_error_code(std::errc::value_too_large));
  } else {
    result.page_faults = System_result<Process_page_faults>::success(
        {static_cast<uint64_t>(usage.ru_majflt)});
  }
  return result;
}

/**
  Collect Linux thread metadata for the current process.

  Source                           Contents
  -------------------------------  -----------------------------------------
  /proc/self/status                Process identity
  `/proc/self/task/<tid>`          Candidate thread IDs
  `/proc/self/task/<tid>/stat`     Name, state, CPU, scheduler and runtime
  `/proc/self/task/<tid>/io`       Thread I/O counters
  sysconf(_SC_CLK_TCK)             Clock ticks per second
  sysconf(_SC_PAGESIZE)            Memory page size in bytes

  Linux exposes thread metadata as procfs records. In contrast, macOS returns
  thread ports from task_threads() and reads their metadata with
  thread_info().

  A thread can exit after its directory is enumerated and before either file
  is read. A missing file omits that thread because this is a normal removal
  race. Other read errors fail the capabilities that depend on the file.

  Each capability represents one consistent set of sampled threads. If a
  required record is invalid for any retained thread, that capability fails
  rather than returning a partial set.
*/
Process_threads_snapshot query_process_threads_platform() {
  Process_threads_snapshot result;
  const uint64_t process_pid = static_cast<uint64_t>(getpid());

  // Process identity is collected independently of the thread records.
  std::ifstream status{"/proc/self/status"};
  if (!status.is_open()) {
    result.identity = System_result<Process_identity>::failure(io_error());
  } else {
    auto identity = read_linux_process_status(status).identity;
    if (status.bad()) {
      result.identity = System_result<Process_identity>::failure(io_error());
    } else if (!identity.has_value() || identity.value().pid != process_pid) {
      // The status record must describe the process queried by this function.
      result.identity = System_result<Process_identity>::failure(
          std::make_error_code(std::errc::invalid_argument));
    } else {
      result.identity = std::move(identity);
    }
  }

  // CPU records require the clock tick rate and runtime memory requires the
  // page size. Preserve separate errors because the capabilities differ.
  errno = 0;
  const long ticks = sysconf(_SC_CLK_TCK);
  const auto ticks_error = ticks > 0 ? std::error_code{} : io_error();
  errno = 0;
  const long page_size = sysconf(_SC_PAGESIZE);
  const auto page_error = page_size > 0 ? std::error_code{} : io_error();

  // Enumerate candidate thread IDs from numeric directory names.
  std::error_code enumeration_error;
  std::filesystem::directory_iterator entries{"/proc/self/task",
                                              enumeration_error};
  std::vector<uint64_t> tids;
  for (std::filesystem::directory_iterator end;
       !enumeration_error && entries != end;
       entries.increment(enumeration_error)) {
    std::error_code type_error;
    if (!entries->is_directory(type_error)) {
      if (type_error) enumeration_error = type_error;
      continue;
    }
    uint64_t tid{0};
    const auto name = entries->path().filename().string();
    if (parse_tid(name, tid)) tids.push_back(tid);
  }
  // Use stable order and guard against duplicate directory entries.
  std::sort(tids.begin(), tids.end());
  tids.erase(std::unique(tids.begin(), tids.end()), tids.end());

  // An enumeration failure affects every capability built from thread files.
  const auto fail_all = [&](std::error_code error) {
    result.threads = System_result<Thread_inventory>::failure(error);
    result.runtime = System_result<Process_runtime_info>::failure(error);
    result.cpu = System_result<Thread_cpu_snapshot>::failure(error);
    result.extended_cpu =
        System_result<Thread_extended_cpu_snapshot>::failure(error);
    result.scheduler = System_result<Thread_scheduler_snapshot>::failure(error);
    result.io = System_result<Thread_io_snapshot>::failure(error);
  };
  if (enumeration_error) {
    fail_all(enumeration_error);
    return result;
  }
  // A running process must contain at least one thread.
  if (tids.empty()) {
    fail_all(std::make_error_code(std::errc::no_such_process));
    return result;
  }

  // Accumulate each capability separately. A validity flag records whether
  // every retained thread supplied the data required by that capability.
  Thread_inventory inventory;
  Thread_cpu_snapshot cpu;
  Thread_extended_cpu_snapshot extended_cpu;
  Thread_scheduler_snapshot scheduler;
  Thread_io_snapshot io;
  bool inventory_valid = true;
  bool cpu_valid = ticks > 0;
  bool extended_cpu_valid = ticks > 0;
  bool scheduler_valid = ticks > 0;
  bool io_valid = true;
  bool runtime_seen = false;
  bool runtime_valid = page_size > 0;
  Process_runtime_info runtime;
  std::error_code stat_error;
  std::error_code thread_io_error;

  for (const uint64_t tid : tids) {
    const std::string base = "/proc/self/task/" + std::to_string(tid);

    // Read the positional stat record before the independent I/O record.
    errno = 0;
    std::ifstream stat{base + "/stat"};
    if (!stat.is_open()) {
      const auto error = io_error();
      // A missing file means the thread exited after directory enumeration.
      if (error == std::errc::no_such_file_or_directory) continue;
      // Other errors fail every capability that depends on the stat record.
      if (!stat_error) stat_error = error;
      inventory_valid = false;
      cpu_valid = false;
      extended_cpu_valid = false;
      scheduler_valid = false;
      if (tid == process_pid) runtime_valid = false;
      continue;
    }
    errno = 0;
    auto parsed = read_linux_thread_stat(
        stat, tid, process_pid, ticks > 0 ? static_cast<uint64_t>(ticks) : 0,
        page_size > 0 ? static_cast<uint64_t>(page_size) : 0);
    // Save errno immediately because later library calls can replace it.
    const int stat_read_errno = errno;
    if (stat.bad()) {
      const std::error_code error =
          stat_read_errno == 0
              ? std::make_error_code(std::errc::io_error)
              : std::error_code{stat_read_errno, std::generic_category()};
      // A read can detect the same removal race as opening the file.
      if (error == std::errc::no_such_file_or_directory) continue;
      if (!stat_error) stat_error = error;
      inventory_valid = false;
      cpu_valid = false;
      extended_cpu_valid = false;
      scheduler_valid = false;
      if (tid == process_pid) runtime_valid = false;
      continue;
    }
    // `threads` contains the shared TID, name and state fields. Without those
    // fields, no capability can safely retain this thread.
    if (!parsed.threads.has_value()) {
      inventory_valid = false;
      cpu_valid = false;
      extended_cpu_valid = false;
      scheduler_valid = false;
      if (tid == process_pid) runtime_valid = false;
      continue;
    }

    // Read I/O separately. Its errors affect only `io` unless a missing file
    // shows that the thread disappeared.
    errno = 0;
    std::ifstream io_file{base + "/io"};
    if (!io_file.is_open()) {
      const auto error = io_error();
      // Omit a thread that disappeared between the stat and I/O reads.
      if (error == std::errc::no_such_file_or_directory) continue;
      // Other errors fail only the `io` capability.
      if (!thread_io_error) thread_io_error = error;
      io_valid = false;
    }
    System_result<Thread_io_counters> parsed_io;
    if (io_file.is_open()) {
      errno = 0;
      parsed_io = read_linux_thread_io(io_file, tid);
      const int io_read_errno = errno;
      if (io_file.bad()) {
        const std::error_code error =
            io_read_errno == 0
                ? std::make_error_code(std::errc::io_error)
                : std::error_code{io_read_errno, std::generic_category()};
        // Omit a thread that disappeared while its I/O record was being read.
        if (error == std::errc::no_such_file_or_directory) continue;
        if (!thread_io_error) thread_io_error = error;
        io_valid = false;
      } else if (!parsed_io.has_value()) {
        io_valid = false;
      }
    }

    // The common thread record is present. Append each capability only when
    // its parser returned a complete value for this thread.
    inventory.threads.push_back(
        std::move(parsed.threads.value().threads.front()));
    if (parsed.cpu.has_value()) {
      cpu.threads.push_back(std::move(parsed.cpu.value().threads.front()));
    } else {
      cpu_valid = false;
    }
    if (parsed.extended_cpu.has_value()) {
      extended_cpu.threads.push_back(
          std::move(parsed.extended_cpu.value().threads.front()));
    } else {
      extended_cpu_valid = false;
    }
    if (parsed.scheduler.has_value()) {
      scheduler.threads.push_back(
          std::move(parsed.scheduler.value().threads.front()));
    } else {
      scheduler_valid = false;
    }
    if (parsed_io.has_value()) {
      io.threads.push_back(std::move(parsed_io).value());
    }
    // Process runtime fields come only from the main thread record, whose TID
    // is equal to the process PID.
    if (tid == process_pid) {
      runtime_seen = true;
      if (parsed.runtime.has_value()) {
        runtime = std::move(parsed.runtime).value();
      } else {
        runtime_valid = false;
      }
    }
  }

  // No enumerated thread produced a valid common record.
  if (inventory.threads.empty()) {
    fail_all(std::make_error_code(std::errc::no_such_process));
    return result;
  }
  const auto invalid = std::make_error_code(std::errc::invalid_argument);
  const size_t sampled_threads = inventory.threads.size();

  // Publish a capability only when it contains one record for every retained
  // thread. Report the first source error when available.
  result.threads =
      inventory_valid
          ? System_result<Thread_inventory>::success(std::move(inventory))
          : System_result<Thread_inventory>::failure(stat_error ? stat_error
                                                                : invalid);
  result.cpu = cpu_valid && cpu.threads.size() == sampled_threads
                   ? System_result<Thread_cpu_snapshot>::success(std::move(cpu))
                   : System_result<Thread_cpu_snapshot>::failure(
                         ticks_error ? ticks_error
                                     : (stat_error ? stat_error : invalid));
  result.extended_cpu =
      extended_cpu_valid && extended_cpu.threads.size() == sampled_threads
          ? System_result<Thread_extended_cpu_snapshot>::success(
                std::move(extended_cpu))
          : System_result<Thread_extended_cpu_snapshot>::failure(
                ticks_error ? ticks_error
                            : (stat_error ? stat_error : invalid));
  result.scheduler =
      scheduler_valid && scheduler.threads.size() == sampled_threads
          ? System_result<Thread_scheduler_snapshot>::success(
                std::move(scheduler))
          : System_result<Thread_scheduler_snapshot>::failure(
                ticks_error ? ticks_error
                            : (stat_error ? stat_error : invalid));
  result.io = io_valid && io.threads.size() == sampled_threads
                  ? System_result<Thread_io_snapshot>::success(std::move(io))
                  : System_result<Thread_io_snapshot>::failure(
                        thread_io_error ? thread_io_error : invalid);
  // Runtime is a process value and must be present in the main thread record
  result.runtime =
      runtime_seen && runtime_valid
          ? System_result<Process_runtime_info>::success(std::move(runtime))
          : System_result<Process_runtime_info>::failure(
                ticks_error ? ticks_error
                            : (page_error ? page_error : invalid));
  return result;
}

}  // namespace mysql::system_info::internal

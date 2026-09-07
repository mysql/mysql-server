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

[[nodiscard]] bool parse_tid(std::string_view text, uint64_t &tid) {
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), tid);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size() && tid > 0;
}

[[nodiscard]] std::error_code io_error() {
  auto error = errno_code();
  if (!error) error = std::make_error_code(std::errc::io_error);
  return error;
}

}  // namespace

Filesystem_snapshot query_filesystem_platform(std::string_view path) {
  Filesystem_snapshot result;
  if (path.empty()) {
    const auto error = std::make_error_code(std::errc::invalid_argument);
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  const std::string path_string{path};
  std::error_code canonical_error;
  const auto canonical_path =
      std::filesystem::canonical(path_string, canonical_error);
  struct stat path_status {};
  if (canonical_error || stat(path_string.c_str(), &path_status) != 0) {
    const auto error = canonical_error ? canonical_error : errno_code();
    result.identity = System_result<Filesystem_identity>::failure(error);
  } else {
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
      if (mount_info.bad()) {
        auto error = errno_code();
        if (!error) error = std::make_error_code(std::errc::io_error);
        result.identity = System_result<Filesystem_identity>::failure(error);
      }
    }
  }

  struct statvfs usage {};
  if (statvfs(path_string.c_str(), &usage) != 0) {
    result.usage = System_result<Filesystem_usage>::failure(errno_code());
    return result;
  }
  const uint64_t block_size = usage.f_frsize;
  if (block_size == 0 || usage.f_blocks == 0 ||
      usage.f_bavail > usage.f_blocks ||
      multiply_overflows(usage.f_blocks, block_size) ||
      multiply_overflows(usage.f_bavail, block_size)) {
    result.usage = System_result<Filesystem_usage>::failure(
        std::make_error_code(std::errc::value_too_large));
    return result;
  }
  result.usage = System_result<Filesystem_usage>::success(
      {usage.f_blocks * block_size, usage.f_bavail * block_size});
  return result;
}

Storage_snapshot query_storage_devices_platform() {
  return query_linux_storage_devices(
      {"/proc/partitions", "/proc/diskstats", "/sys/class/block"});
}

Host_memory_snapshot query_host_memory_platform() {
  Host_memory_snapshot result;
  std::ifstream meminfo{"/proc/meminfo"};
  if (!meminfo.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.memory = System_result<Host_memory_info>::failure(error);
    result.breakdown = System_result<Host_memory_breakdown>::failure(error);
    result.swap_capacity = System_result<Swap_capacity_info>::failure(error);
  } else {
    auto parsed = read_linux_meminfo(meminfo);
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

  std::ifstream vmstat{"/proc/vmstat"};
  if (!vmstat.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.swap_activity = System_result<Swap_activity_info>::failure(error);
  } else {
    result.swap_activity = read_linux_vmstat(vmstat);
    if (vmstat.bad()) {
      auto error = errno_code();
      if (!error) error = std::make_error_code(std::errc::io_error);
      result.swap_activity = System_result<Swap_activity_info>::failure(error);
    }
  }
  return result;
}

Host_cpu_snapshot query_host_cpu_platform() {
  Host_cpu_snapshot result;
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
  if (stat.bad()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
  }
  return result;
}

Process_memory_snapshot query_process_memory_platform() {
  Process_memory_snapshot result;

  std::ifstream status{"/proc/self/status"};
  if (!status.is_open()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.identity = System_result<Process_identity>::failure(error);
    result.residency = System_result<Process_memory_residency>::failure(error);
    result.details = System_result<Process_memory_details>::failure(error);
  } else {
    auto parsed = read_linux_process_status(status);
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

  struct rusage usage {};
  errno = 0;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.page_faults = System_result<Process_page_faults>::failure(error);
  } else if (!std::in_range<uint64_t>(usage.ru_majflt)) {
    result.page_faults = System_result<Process_page_faults>::failure(
        std::make_error_code(std::errc::value_too_large));
  } else {
    result.page_faults = System_result<Process_page_faults>::success(
        {static_cast<uint64_t>(usage.ru_majflt)});
  }
  return result;
}

Process_threads_snapshot query_process_threads_platform() {
  Process_threads_snapshot result;
  const uint64_t process_pid = static_cast<uint64_t>(getpid());

  std::ifstream status{"/proc/self/status"};
  if (!status.is_open()) {
    result.identity = System_result<Process_identity>::failure(io_error());
  } else {
    auto identity = read_linux_process_status(status).identity;
    if (status.bad()) {
      result.identity = System_result<Process_identity>::failure(io_error());
    } else if (!identity.has_value() || identity.value().pid != process_pid) {
      result.identity = System_result<Process_identity>::failure(
          std::make_error_code(std::errc::invalid_argument));
    } else {
      result.identity = std::move(identity);
    }
  }

  errno = 0;
  const long ticks = sysconf(_SC_CLK_TCK);
  const auto ticks_error = ticks > 0 ? std::error_code{} : io_error();
  errno = 0;
  const long page_size = sysconf(_SC_PAGESIZE);
  const auto page_error = page_size > 0 ? std::error_code{} : io_error();

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
  std::sort(tids.begin(), tids.end());
  tids.erase(std::unique(tids.begin(), tids.end()), tids.end());

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
  if (tids.empty()) {
    fail_all(std::make_error_code(std::errc::no_such_process));
    return result;
  }

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
    errno = 0;
    std::ifstream stat{base + "/stat"};
    if (!stat.is_open()) {
      const auto error = io_error();
      if (error == std::errc::no_such_file_or_directory) continue;
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
    const int stat_read_errno = errno;
    if (stat.bad()) {
      const std::error_code error =
          stat_read_errno == 0
              ? std::make_error_code(std::errc::io_error)
              : std::error_code{stat_read_errno, std::generic_category()};
      if (error == std::errc::no_such_file_or_directory) continue;
      if (!stat_error) stat_error = error;
      inventory_valid = false;
      cpu_valid = false;
      extended_cpu_valid = false;
      scheduler_valid = false;
      if (tid == process_pid) runtime_valid = false;
      continue;
    }
    if (!parsed.threads.has_value()) {
      inventory_valid = false;
      cpu_valid = false;
      extended_cpu_valid = false;
      scheduler_valid = false;
      if (tid == process_pid) runtime_valid = false;
      continue;
    }

    errno = 0;
    std::ifstream io_file{base + "/io"};
    if (!io_file.is_open()) {
      const auto error = io_error();
      if (error == std::errc::no_such_file_or_directory) continue;
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
        if (error == std::errc::no_such_file_or_directory) continue;
        if (!thread_io_error) thread_io_error = error;
        io_valid = false;
      } else if (!parsed_io.has_value()) {
        io_valid = false;
      }
    }

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
    if (tid == process_pid) {
      runtime_seen = true;
      if (parsed.runtime.has_value()) {
        runtime = std::move(parsed.runtime).value();
      } else {
        runtime_valid = false;
      }
    }
  }

  if (inventory.threads.empty()) {
    fail_all(std::make_error_code(std::errc::no_such_process));
    return result;
  }
  const auto invalid = std::make_error_code(std::errc::invalid_argument);
  const size_t sampled_threads = inventory.threads.size();
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
  result.runtime =
      runtime_seen && runtime_valid
          ? System_result<Process_runtime_info>::success(std::move(runtime))
          : System_result<Process_runtime_info>::failure(
                ticks_error ? ticks_error
                            : (page_error ? page_error : invalid));
  return result;
}

}  // namespace mysql::system_info::internal

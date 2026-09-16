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

#include "components/library_mysys/my_system_api/system_info_macos.h"
#include "components/library_mysys/my_system_api/system_info_utils.h"

#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace mysql::system_info::internal {
namespace {

class Mach_error_category final : public std::error_category {
 public:
  [[nodiscard]] const char *name() const noexcept override { return "mach"; }

  [[nodiscard]] std::string message(int error) const override {
    const char *const description =
        mach_error_string(static_cast<mach_error_t>(error));
    return description != nullptr ? description : "unknown Mach error";
  }
};

[[nodiscard]] const std::error_category &mach_category() {
  static const Mach_error_category category;
  return category;
}

[[nodiscard]] System_result<Process_identity> query_process_identity(
    pid_t pid) {
  if (pid <= 0) {
    return System_result<Process_identity>::failure(
        std::make_error_code(std::errc::invalid_argument));
  }

  std::array<char, PROC_PIDPATHINFO_MAXSIZE> process_name{};
  errno = 0;
  const int name_length =
      proc_name(pid, process_name.data(), process_name.size());
  if (name_length <= 0 ||
      static_cast<size_t>(name_length) > process_name.size()) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    return System_result<Process_identity>::failure(error);
  }

  const auto end = std::find(process_name.begin(), process_name.end(), '\0');
  std::string name{process_name.begin(), end};
  if (name.empty()) {
    return System_result<Process_identity>::failure(
        std::make_error_code(std::errc::invalid_argument));
  }
  return System_result<Process_identity>::success(
      {static_cast<uint64_t>(pid), std::move(name)});
}

[[nodiscard]] bool thread_disappeared(kern_return_t error) {
  return error == KERN_INVALID_ARGUMENT || error == KERN_TERMINATED ||
         error == MACH_SEND_INVALID_DEST;
}

[[nodiscard]] kern_return_t deallocate_thread_port(void *, mach_port_t port) {
  return mach_port_deallocate(mach_task_self(), port);
}

[[nodiscard]] kern_return_t deallocate_thread_array(void *,
                                                    vm_address_t address,
                                                    vm_size_t size) {
  return vm_deallocate(mach_task_self(), address, size);
}

[[nodiscard]] macOS_thread_deallocator native_thread_deallocator() {
  return {nullptr, deallocate_thread_port, deallocate_thread_array};
}

}  // namespace

std::error_code mach_error_code(int error) { return {error, mach_category()}; }

macOS_thread_resources::macOS_thread_resources(
    thread_act_array_t threads, mach_msg_type_number_t thread_count,
    macOS_thread_deallocator deallocator)
    : m_threads(threads),
      m_thread_count(thread_count),
      m_deallocator(deallocator) {}

macOS_thread_resources::~macOS_thread_resources() {
  if (!m_released) (void)release();
}

std::error_code macOS_thread_resources::release() {
  if (m_released) return m_release_error;
  m_released = true;
  if (m_threads == nullptr) return m_release_error;

  if (m_deallocator.deallocate_port == nullptr) {
    m_release_error = std::make_error_code(std::errc::invalid_argument);
  } else {
    for (mach_msg_type_number_t index = 0; index < m_thread_count; ++index) {
      const kern_return_t result = m_deallocator.deallocate_port(
          m_deallocator.context, m_threads[index]);
      if (result != KERN_SUCCESS && !m_release_error) {
        m_release_error = mach_error_code(result);
      }
    }
  }

  const uint64_t bytes =
      static_cast<uint64_t>(m_thread_count) * sizeof(*m_threads);
  kern_return_t memory_result = KERN_SUCCESS;
  if (m_deallocator.deallocate_memory == nullptr ||
      bytes > std::numeric_limits<vm_size_t>::max()) {
    memory_result = KERN_INVALID_ARGUMENT;
  } else {
    memory_result = m_deallocator.deallocate_memory(
        m_deallocator.context, reinterpret_cast<vm_address_t>(m_threads),
        static_cast<vm_size_t>(bytes));
  }
  if (memory_result != KERN_SUCCESS && !m_release_error) {
    m_release_error = mach_error_code(memory_result);
  }
  return m_release_error;
}

Thread_state macos_thread_state(int run_state) {
  switch (run_state) {
    case TH_STATE_RUNNING:
      return Thread_state::running;
    case TH_STATE_WAITING:
      return Thread_state::sleeping;
    case TH_STATE_UNINTERRUPTIBLE:
      return Thread_state::waiting;
    case TH_STATE_STOPPED:
      // The native states differ, but neither is runnable. The portable
      // Thread_state model has no useful distinction between them.
      [[fallthrough]];
    case TH_STATE_HALTED:
      return Thread_state::stopped;
    default:
      return Thread_state::unknown;
  }
}

bool macos_time_value_to_milliseconds(int64_t seconds, int64_t microseconds,
                                      std::chrono::milliseconds &milliseconds) {
  constexpr int64_t kMillisecondsPerSecond = 1000;
  constexpr int64_t kMicrosecondsPerSecond = 1000000;
  if (seconds < 0 || microseconds < 0 ||
      microseconds >= kMicrosecondsPerSecond ||
      seconds > std::numeric_limits<int64_t>::max() / kMillisecondsPerSecond) {
    return false;
  }

  const int64_t second_milliseconds = seconds * kMillisecondsPerSecond;
  const int64_t fractional_milliseconds = microseconds / 1000;
  if (fractional_milliseconds >
      std::numeric_limits<int64_t>::max() - second_milliseconds) {
    return false;
  }
  const int64_t whole_milliseconds =
      second_milliseconds + fractional_milliseconds;
  milliseconds = std::chrono::milliseconds{whole_milliseconds};
  return true;
}

std::string copy_bounded_macos_thread_name(const char *name, size_t size) {
  const char *const end = std::find(name, name + size, '\0');
  return {name, end};
}

std::error_code map_macos_thread_samples(
    std::vector<macOS_thread_sample> samples,
    std::error_code disappearance_error, Thread_inventory &inventory,
    Thread_cpu_snapshot &cpu) {
  Thread_inventory mapped_inventory;
  Thread_cpu_snapshot mapped_cpu;
  samples.erase(std::remove_if(samples.begin(), samples.end(),
                               [](const auto &sample) {
                                 return sample.sample_state ==
                                        macOS_thread_sample_state::disappeared;
                               }),
                samples.end());
  if (samples.empty()) {
    return disappearance_error
               ? disappearance_error
               : std::make_error_code(std::errc::invalid_argument);
  }
  std::sort(samples.begin(), samples.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.tid < rhs.tid; });
  mapped_inventory.threads.reserve(samples.size());
  mapped_cpu.threads.reserve(samples.size());
  uint64_t previous_tid{0};
  for (auto &sample : samples) {
    if (sample.tid == 0 || sample.tid == previous_tid) {
      return std::make_error_code(std::errc::invalid_argument);
    }

    std::chrono::milliseconds user;
    std::chrono::milliseconds system;
    if (!macos_time_value_to_milliseconds(sample.user_seconds,
                                          sample.user_microseconds, user) ||
        !macos_time_value_to_milliseconds(sample.system_seconds,
                                          sample.system_microseconds, system)) {
      return std::make_error_code(std::errc::invalid_argument);
    }
    previous_tid = sample.tid;
    mapped_inventory.threads.push_back({sample.tid, std::move(sample.name),
                                        macos_thread_state(sample.run_state)});
    mapped_cpu.threads.push_back({sample.tid, user, system});
  }
  inventory = std::move(mapped_inventory);
  cpu = std::move(mapped_cpu);
  return {};
}

void complete_macos_thread_collection(macOS_thread_resources &resources,
                                      std::error_code collection_error,
                                      Thread_inventory inventory,
                                      Thread_cpu_snapshot cpu,
                                      Process_threads_snapshot &result) {
  const auto cleanup_error = resources.release();
  const auto error = cleanup_error ? cleanup_error : collection_error;
  if (error) {
    result.threads = System_result<Thread_inventory>::failure(error);
    result.cpu = System_result<Thread_cpu_snapshot>::failure(error);
    return;
  }
  result.threads =
      System_result<Thread_inventory>::success(std::move(inventory));
  result.cpu = System_result<Thread_cpu_snapshot>::success(std::move(cpu));
}

Filesystem_snapshot query_filesystem_platform(std::string_view path) {
  Filesystem_snapshot result;
  if (path.empty()) {
    const auto error = std::make_error_code(std::errc::invalid_argument);
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  struct statfs information {};
  const std::string path_string{path};
  if (statfs(path_string.c_str(), &information) != 0) {
    const auto error = errno_code();
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  const std::string filesystem_id = std::to_string(information.f_fsid.val[0]) +
                                    ":" +
                                    std::to_string(information.f_fsid.val[1]);
  result.identity = System_result<Filesystem_identity>::success(
      {filesystem_id, information.f_mntonname, information.f_mntfromname,
       information.f_fstypename});

  if (information.f_bsize <= 0 || information.f_blocks == 0 ||
      information.f_bavail > information.f_blocks ||
      multiply_overflows(information.f_blocks, information.f_bsize) ||
      multiply_overflows(information.f_bavail, information.f_bsize)) {
    result.usage = System_result<Filesystem_usage>::failure(
        std::make_error_code(std::errc::value_too_large));
    return result;
  }

  result.usage = System_result<Filesystem_usage>::success(
      {information.f_blocks * static_cast<uint64_t>(information.f_bsize),
       information.f_bavail * static_cast<uint64_t>(information.f_bsize)});
  return result;
}

Storage_snapshot query_storage_devices_platform() {
  Storage_snapshot result;
  result.inventory = System_result<Storage_inventory>::unavailable();
  result.capacities = System_result<Storage_capacities>::unavailable();
  result.hierarchy = System_result<Storage_hierarchy>::unavailable();
  result.read_write = System_result<Storage_read_write_snapshot>::unavailable();
  result.flush = System_result<Storage_flush_snapshot>::unavailable();
  return result;
}

Host_memory_snapshot query_host_memory_platform() {
  Host_memory_snapshot result;
  result.breakdown = System_result<Host_memory_breakdown>::unavailable();
  result.swap_activity = System_result<Swap_activity_info>::unavailable();

  uint64_t total_bytes{0};
  size_t total_size = sizeof(total_bytes);
  if (sysctlbyname("hw.memsize", &total_bytes, &total_size, nullptr, 0) != 0) {
    result.memory = System_result<Host_memory_info>::failure(errno_code());
  } else {
    const host_t host = mach_host_self();
    vm_size_t page_size{0};
    vm_statistics64_data_t statistics{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const kern_return_t page_result = host_page_size(host, &page_size);
    const kern_return_t statistics_result =
        page_result == KERN_SUCCESS
            ? host_statistics64(host, HOST_VM_INFO64,
                                reinterpret_cast<host_info64_t>(&statistics),
                                &count)
            : page_result;
    mach_port_deallocate(mach_task_self(), host);

    // macOS available memory is free plus inactive pages from
    // host_statistics64(); it is not equivalent to Linux MemAvailable.
    const uint64_t available_pages =
        static_cast<uint64_t>(statistics.free_count) +
        static_cast<uint64_t>(statistics.inactive_count);
    if (statistics_result != KERN_SUCCESS) {
      result.memory = System_result<Host_memory_info>::failure(
          mach_error_code(statistics_result));
    } else if (multiply_overflows(statistics.free_count, page_size) ||
               multiply_overflows(available_pages, page_size)) {
      result.memory = System_result<Host_memory_info>::failure(
          std::make_error_code(std::errc::value_too_large));
    } else {
      result.memory = System_result<Host_memory_info>::success(
          {total_bytes,
           static_cast<uint64_t>(statistics.free_count) * page_size,
           available_pages * page_size});
    }
  }

  xsw_usage swap{};
  size_t swap_size = sizeof(swap);
  if (sysctlbyname("vm.swapusage", &swap, &swap_size, nullptr, 0) != 0) {
    result.swap_capacity =
        System_result<Swap_capacity_info>::failure(errno_code());
  } else if (swap.xsu_used > swap.xsu_total) {
    result.swap_capacity = System_result<Swap_capacity_info>::failure(
        std::make_error_code(std::errc::invalid_argument));
  } else {
    const uint64_t free_bytes = swap.xsu_total - swap.xsu_used;
    result.swap_capacity = System_result<Swap_capacity_info>::success(
        {swap.xsu_total, free_bytes});
  }
  return result;
}

Host_cpu_snapshot query_host_cpu_platform() {
  Host_cpu_snapshot result;
  result.extended_times = System_result<Cpu_extended_snapshot>::unavailable();

  errno = 0;
  const long ticks_per_second = sysconf(_SC_CLK_TCK);
  if (ticks_per_second <= 0) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::invalid_argument);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    return result;
  }

  const host_t host = mach_host_self();
  natural_t processor_count{0};
  processor_info_array_t processor_information{nullptr};
  mach_msg_type_number_t processor_information_count{0};
  const kern_return_t information_result =
      host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &processor_count,
                          &processor_information, &processor_information_count);
  const kern_return_t host_deallocation_result =
      mach_port_deallocate(mach_task_self(), host);

  const auto deallocate_information = [&]() {
    if (processor_information == nullptr) return KERN_SUCCESS;
    return vm_deallocate(mach_task_self(),
                         reinterpret_cast<vm_address_t>(processor_information),
                         static_cast<vm_size_t>(processor_information_count) *
                             sizeof(integer_t));
  };

  if (information_result != KERN_SUCCESS) {
    deallocate_information();
    result.times = System_result<Cpu_times_snapshot>::failure(
        mach_error_code(information_result));
    return result;
  }
  if (host_deallocation_result != KERN_SUCCESS) {
    deallocate_information();
    result.times = System_result<Cpu_times_snapshot>::failure(
        mach_error_code(host_deallocation_result));
    return result;
  }

  const bool count_overflows =
      processor_count > std::numeric_limits<mach_msg_type_number_t>::max() /
                            PROCESSOR_CPU_LOAD_INFO_COUNT;
  const mach_msg_type_number_t expected_count =
      count_overflows ? 0 : processor_count * PROCESSOR_CPU_LOAD_INFO_COUNT;
  if (processor_count == 0 || processor_information == nullptr ||
      count_overflows || processor_information_count != expected_count) {
    const kern_return_t deallocation_result = deallocate_information();
    result.times = deallocation_result == KERN_SUCCESS
                       ? System_result<Cpu_times_snapshot>::failure(
                             std::make_error_code(std::errc::invalid_argument))
                       : System_result<Cpu_times_snapshot>::failure(
                             mach_error_code(deallocation_result));
    return result;
  }

  const auto *loads =
      reinterpret_cast<processor_cpu_load_info_t>(processor_information);
  std::array<uint64_t, CPU_STATE_MAX> aggregate{};
  Cpu_times_snapshot times;
  times.cpus.reserve(static_cast<size_t>(processor_count) + 1U);
  bool valid = true;
  for (natural_t processor = 0; processor < processor_count; ++processor) {
    for (size_t state = 0; state < CPU_STATE_MAX; ++state) {
      const uint64_t ticks = loads[processor].cpu_ticks[state];
      if (add_overflows(aggregate[state], ticks)) {
        valid = false;
        break;
      }
      aggregate[state] += ticks;
    }
    if (!valid) break;

    Cpu_times cpu;
    cpu.cpu = "cpu" + std::to_string(processor);
    valid =
        !cpu_ticks_to_milliseconds(loads[processor].cpu_ticks[CPU_STATE_USER],
                                   static_cast<uint64_t>(ticks_per_second),
                                   cpu.user) &&
        !cpu_ticks_to_milliseconds(loads[processor].cpu_ticks[CPU_STATE_NICE],
                                   static_cast<uint64_t>(ticks_per_second),
                                   cpu.nice) &&
        !cpu_ticks_to_milliseconds(loads[processor].cpu_ticks[CPU_STATE_SYSTEM],
                                   static_cast<uint64_t>(ticks_per_second),
                                   cpu.system) &&
        !cpu_ticks_to_milliseconds(loads[processor].cpu_ticks[CPU_STATE_IDLE],
                                   static_cast<uint64_t>(ticks_per_second),
                                   cpu.idle);
    if (!valid) break;
    times.cpus.push_back(std::move(cpu));
  }

  Cpu_times aggregate_cpu;
  aggregate_cpu.cpu = "cpu";
  valid = valid &&
          !cpu_ticks_to_milliseconds(aggregate[CPU_STATE_USER],
                                     static_cast<uint64_t>(ticks_per_second),
                                     aggregate_cpu.user) &&
          !cpu_ticks_to_milliseconds(aggregate[CPU_STATE_NICE],
                                     static_cast<uint64_t>(ticks_per_second),
                                     aggregate_cpu.nice) &&
          !cpu_ticks_to_milliseconds(aggregate[CPU_STATE_SYSTEM],
                                     static_cast<uint64_t>(ticks_per_second),
                                     aggregate_cpu.system) &&
          !cpu_ticks_to_milliseconds(aggregate[CPU_STATE_IDLE],
                                     static_cast<uint64_t>(ticks_per_second),
                                     aggregate_cpu.idle);
  if (valid) times.cpus.insert(times.cpus.begin(), std::move(aggregate_cpu));

  const kern_return_t deallocation_result = deallocate_information();
  if (deallocation_result != KERN_SUCCESS) {
    result.times = System_result<Cpu_times_snapshot>::failure(
        mach_error_code(deallocation_result));
  } else if (!valid) {
    result.times = System_result<Cpu_times_snapshot>::failure(
        std::make_error_code(std::errc::value_too_large));
  } else {
    result.times = System_result<Cpu_times_snapshot>::success(std::move(times));
  }
  return result;
}

Process_memory_snapshot query_process_memory_platform() {
  Process_memory_snapshot result;
  const pid_t pid = getpid();
  result.identity = query_process_identity(pid);

  struct proc_taskinfo task_information {};
  errno = 0;
  const int task_size = proc_pidinfo(
      pid, PROC_PIDTASKINFO, 0, &task_information, sizeof(task_information));
  if (task_size != sizeof(task_information)) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.residency = System_result<Process_memory_residency>::failure(error);
  } else {
    result.residency = System_result<Process_memory_residency>::success(
        {task_information.pti_resident_size});
  }

  result.details = System_result<Process_memory_details>::unavailable();
  result.page_faults = System_result<Process_page_faults>::unavailable();
  return result;
}

Process_threads_snapshot query_process_threads_platform() {
  Process_threads_snapshot result;
  result.runtime = System_result<Process_runtime_info>::unavailable();
  result.extended_cpu =
      System_result<Thread_extended_cpu_snapshot>::unavailable();
  result.scheduler = System_result<Thread_scheduler_snapshot>::unavailable();
  result.io = System_result<Thread_io_snapshot>::unavailable();

  result.identity = query_process_identity(getpid());

  thread_act_array_t threads{nullptr};
  mach_msg_type_number_t thread_count{0};
  const kern_return_t enumeration_result =
      task_threads(mach_task_self(), &threads, &thread_count);
  macOS_thread_resources thread_list{threads, thread_count,
                                     native_thread_deallocator()};
  if (enumeration_result != KERN_SUCCESS) {
    complete_macos_thread_collection(
        thread_list, mach_error_code(enumeration_result), {}, {}, result);
    return result;
  }
  if (threads == nullptr || thread_count == 0) {
    complete_macos_thread_collection(
        thread_list, std::make_error_code(std::errc::invalid_argument), {}, {},
        result);
    return result;
  }

  std::vector<macOS_thread_sample> samples;
  samples.reserve(thread_count);
  std::error_code collection_error;
  std::error_code disappearance_error;
  for (mach_msg_type_number_t index = 0; index < thread_count; ++index) {
    macOS_thread_sample sample;

    thread_identifier_info_data_t identifier{};
    mach_msg_type_number_t identifier_count = THREAD_IDENTIFIER_INFO_COUNT;
    kern_return_t native_result = thread_info(
        threads[index], THREAD_IDENTIFIER_INFO,
        reinterpret_cast<thread_info_t>(&identifier), &identifier_count);
    if (native_result != KERN_SUCCESS) {
      if (thread_disappeared(native_result)) {
        disappearance_error = mach_error_code(native_result);
        sample.sample_state = macOS_thread_sample_state::disappeared;
        samples.push_back(std::move(sample));
        continue;
      }
      collection_error = mach_error_code(native_result);
      break;
    }
    if (identifier_count != THREAD_IDENTIFIER_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    thread_extended_info_data_t extended{};
    mach_msg_type_number_t extended_count = THREAD_EXTENDED_INFO_COUNT;
    native_result = thread_info(threads[index], THREAD_EXTENDED_INFO,
                                reinterpret_cast<thread_info_t>(&extended),
                                &extended_count);
    if (native_result != KERN_SUCCESS) {
      if (thread_disappeared(native_result)) {
        disappearance_error = mach_error_code(native_result);
        sample.sample_state = macOS_thread_sample_state::disappeared;
        samples.push_back(std::move(sample));
        continue;
      }
      collection_error = mach_error_code(native_result);
      break;
    }
    if (extended_count != THREAD_EXTENDED_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    thread_basic_info_data_t basic{};
    mach_msg_type_number_t basic_count = THREAD_BASIC_INFO_COUNT;
    native_result =
        thread_info(threads[index], THREAD_BASIC_INFO,
                    reinterpret_cast<thread_info_t>(&basic), &basic_count);
    if (native_result != KERN_SUCCESS) {
      if (thread_disappeared(native_result)) {
        disappearance_error = mach_error_code(native_result);
        sample.sample_state = macOS_thread_sample_state::disappeared;
        samples.push_back(std::move(sample));
        continue;
      }
      collection_error = mach_error_code(native_result);
      break;
    }
    if (basic_count != THREAD_BASIC_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    sample.tid = identifier.thread_id;
    sample.name = copy_bounded_macos_thread_name(extended.pth_name,
                                                 sizeof(extended.pth_name));
    sample.run_state = basic.run_state;
    sample.user_seconds = basic.user_time.seconds;
    sample.user_microseconds = basic.user_time.microseconds;
    sample.system_seconds = basic.system_time.seconds;
    sample.system_microseconds = basic.system_time.microseconds;
    samples.push_back(std::move(sample));
  }

  Thread_inventory inventory;
  Thread_cpu_snapshot cpu;
  if (!collection_error) {
    collection_error = map_macos_thread_samples(
        std::move(samples), disappearance_error, inventory, cpu);
  }

  complete_macos_thread_collection(thread_list, collection_error,
                                   std::move(inventory), std::move(cpu),
                                   result);
  return result;
}

}  // namespace mysql::system_info::internal

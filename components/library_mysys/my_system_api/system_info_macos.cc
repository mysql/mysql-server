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

/**
  macOS metadata read by this implementation.

  Source                                       Contents
  -------------------------------------------  -------------------------------
  statfs()                                     Filesystem identity and space
  sysctlbyname("hw.memsize")                   Physical memory in bytes
  host_statistics64(HOST_VM_INFO64)            Free and inactive memory pages
  sysctlbyname("vm.swapusage")                 Total and used swap space
  host_processor_info(PROCESSOR_CPU_LOAD_INFO) CPU time for each processor
  proc_name()                                  Process name
  proc_pidinfo(PROC_PIDTASKINFO)               Resident process memory
  task_threads()                               Current process thread ports
  thread_info(THREAD_IDENTIFIER_INFO)          Thread ID unique across system
  thread_info(THREAD_EXTENDED_INFO)            Thread name
  thread_info(THREAD_BASIC_INFO)               Thread state and CPU time

  The collectors validate and convert these native records into typed system
  information snapshots. Capabilities without a suitable macOS source remain
  unavailable.
*/

/** Preserve Mach return codes and their native error descriptions. */
class Mach_error_category final : public std::error_category {
 public:
  [[nodiscard]] const char *name() const noexcept override { return "mach"; }

  [[nodiscard]] std::string message(int error) const override {
    const char *const description =
        mach_error_string(static_cast<mach_error_t>(error));
    return description != nullptr ? description : "unknown Mach error";
  }
};

/** Return the shared error category used for Mach return codes. */
[[nodiscard]] const std::error_category &mach_category() {
  static const Mach_error_category category;
  return category;
}

/**
  Read the process name returned by proc_name().

  libproc writes at most the supplied buffer size and can return a name without
  a trailing null byte. The bounded search therefore does not assume that the
  buffer contains a null-terminated C string.
*/
[[nodiscard]] System_result<Process_identity> query_process_identity(
    pid_t pid) {
  // A process ID must be positive before it can be passed to libproc.
  if (pid <= 0) {
    return System_result<Process_identity>::failure(
        std::make_error_code(std::errc::invalid_argument));
  }

  // proc_name() returns the number of bytes written or zero on failure. Some
  // failures do not set errno, so use a general I/O error then.
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

  // Search only within the supplied buffer because libproc need not append a
  // null byte when the name fills it.
  const auto end = std::find(process_name.begin(), process_name.end(), '\0');
  std::string name{process_name.begin(), end};
  if (name.empty()) {
    return System_result<Process_identity>::failure(
        std::make_error_code(std::errc::invalid_argument));
  }
  return System_result<Process_identity>::success(
      {static_cast<uint64_t>(pid), std::move(name)});
}

/**
  Return true when a Mach error can mean that an enumerated thread exited.

  A thread can disappear after task_threads() returns its port. These errors
  omit that thread from the snapshot. Other errors stop the collection.
*/
[[nodiscard]] bool thread_disappeared(kern_return_t error) {
  return error == KERN_INVALID_ARGUMENT || error == KERN_TERMINATED ||
         error == MACH_SEND_INVALID_DEST;
}

/** Release one thread send right returned by task_threads(). */
[[nodiscard]] kern_return_t deallocate_thread_port(void *, mach_port_t port) {
  return mach_port_deallocate(mach_task_self(), port);
}

/** Release the out of line thread array returned by task_threads(). */
[[nodiscard]] kern_return_t deallocate_thread_array(void *,
                                                    vm_address_t address,
                                                    vm_size_t size) {
  return vm_deallocate(mach_task_self(), address, size);
}

/** Use the native Mach functions to release thread collection resources. */
[[nodiscard]] macOS_thread_deallocator native_thread_deallocator() {
  return {nullptr, deallocate_thread_port, deallocate_thread_array};
}

}  // namespace

/** Convert a Mach return code to `std::error_code`. */
std::error_code mach_error_code(int error) { return {error, mach_category()}; }

/** Take ownership of the thread ports and array returned by task_threads(). */
macOS_thread_resources::macOS_thread_resources(
    thread_act_array_t threads, mach_msg_type_number_t thread_count,
    macOS_thread_deallocator deallocator)
    : m_threads(threads),
      m_thread_count(thread_count),
      m_deallocator(deallocator) {}

/** Release owned Mach resources if the collector has not released them. */
macOS_thread_resources::~macOS_thread_resources() {
  if (!m_released) (void)release();
}

/**
  Release the thread ports and allocated array returned by task_threads().

  Each thread port is a 'send right' owned by the caller. The array itself is
  memory allocated by Mach. Release all ports even if one release fails, then
  release the array. The first error is retained. Repeated calls return the
  error without releasing anything again.
*/
std::error_code macOS_thread_resources::release() {
  // Make release idempotent, including when the first attempt reports an error.
  if (m_released) return m_release_error;
  m_released = true;
  if (m_threads == nullptr) return m_release_error;

  // Release every thread send right. Keep the first error but continue so one
  // bad port does not prevent the remaining ports from being released.
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

  // The thread count determines the size of the array allocated by Mach.
  // Release it even if releasing a port failed.
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

/**
  Map a native Mach thread state to the portable thread state.

  Mach state                Thread_state
  ------------------------  ----------------------
  TH_STATE_RUNNING          running
  TH_STATE_WAITING          sleeping
  TH_STATE_UNINTERRUPTIBLE  waiting
  TH_STATE_STOPPED          stopped
  TH_STATE_HALTED           stopped
  any other value           unknown
*/
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

/**
  Convert a Mach `time_value_t` value to whole milliseconds.

  Microseconds must be in the range 0 through 999999. The conversion discards
  any fraction smaller than one millisecond. The output remains unchanged if a
  component is negative or the converted value would overflow.
*/
bool macos_time_value_to_milliseconds(int64_t seconds, int64_t microseconds,
                                      std::chrono::milliseconds &milliseconds) {
  constexpr int64_t kMillisecondsPerSecond = 1000;
  constexpr int64_t kMicrosecondsPerSecond = 1000000;
  // Validate each component before multiplying seconds by 1000
  if (seconds < 0 || microseconds < 0 ||
      microseconds >= kMicrosecondsPerSecond ||
      seconds > std::numeric_limits<int64_t>::max() / kMillisecondsPerSecond) {
    return false;
  }

  const int64_t second_milliseconds = seconds * kMillisecondsPerSecond;
  const int64_t fractional_milliseconds = microseconds / 1000;
  // Check the final addition separately so the output changes only on success
  if (fractional_milliseconds >
      std::numeric_limits<int64_t>::max() - second_milliseconds) {
    return false;
  }
  const int64_t whole_milliseconds =
      second_milliseconds + fractional_milliseconds;
  milliseconds = std::chrono::milliseconds{whole_milliseconds};
  return true;
}

/**
  Copy a thread name from a fixed size native buffer.

  Stop at the first null byte. If no null byte is present, copy the complete
  buffer without reading beyond it.
*/
std::string copy_bounded_macos_thread_name(const char *name, size_t size) {
  const char *const end = std::find(name, name + size, '\0');
  return {name, end};
}

/**
  Build matching thread inventory and CPU snapshots from native samples.

  Samples for threads that disappeared during collection are omitted. The
  remaining samples are sorted by thread ID so both snapshots use the same
  stable order. Every thread must have a nonzero unique ID and valid user and
  system CPU times.

  The output snapshots remain unchanged on failure. If every enumerated thread
  disappeared, return the recorded Mach error.
*/
std::error_code map_macos_thread_samples(
    std::vector<macOS_thread_sample> samples,
    std::error_code disappearance_error, Thread_inventory &inventory,
    Thread_cpu_snapshot &cpu) {
  // Build into local snapshots so the caller's outputs change only after every
  // surviving sample has been validated.
  Thread_inventory mapped_inventory;
  Thread_cpu_snapshot mapped_cpu;
  // A thread can exit after enumeration. Remove those samples when other
  // threads remain.
  samples.erase(std::remove_if(samples.begin(), samples.end(),
                               [](const auto &sample) {
                                 return sample.sample_state ==
                                        macOS_thread_sample_state::disappeared;
                               }),
                samples.end());
  if (samples.empty()) {
    // Preserve the native error when every enumerated thread disappeared
    return disappearance_error
               ? disappearance_error
               : std::make_error_code(std::errc::invalid_argument);
  }
  // Sorting once keeps Thread_inventory and Thread_cpu_snapshot aligned and
  // places duplicate thread IDs next to each other.
  std::sort(samples.begin(), samples.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.tid < rhs.tid; });
  mapped_inventory.threads.reserve(samples.size());
  mapped_cpu.threads.reserve(samples.size());
  uint64_t previous_tid{0};
  for (auto &sample : samples) {
    // Reject zero and duplicate IDs because each row must identify one thread.
    if (sample.tid == 0 || sample.tid == previous_tid) {
      return std::make_error_code(std::errc::invalid_argument);
    }

    // THREAD_BASIC_INFO reports CPU time as seconds and microseconds.
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
  // Publish both snapshots together after every row succeeds
  inventory = std::move(mapped_inventory);
  cpu = std::move(mapped_cpu);
  return {};
}

/**
  Release native thread resources before publishing the collected snapshots.

  A cleanup error takes precedence over a collection error. The `threads` and
  `cpu` capabilities succeed together only after collection, mapping and
  cleanup all succeed.
*/
void complete_macos_thread_collection(macOS_thread_resources &resources,
                                      std::error_code collection_error,
                                      Thread_inventory inventory,
                                      Thread_cpu_snapshot cpu,
                                      Process_threads_snapshot &result) {
  // Cleanup is part of successful collection because the thread ports and
  // allocated array belong to this call.
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

/**
  Collect filesystem metadata from the macOS `statfs` record.

  Field          Contents
  -------------  -----------------------------------------------------------
  f_fsid         Two-part filesystem ID
  f_mntonname    Directory where the filesystem is mounted
  f_mntfromname  Mounted filesystem or source
  f_fstypename   Filesystem type
  f_bsize        Filesystem block size in bytes
  f_blocks       Total number of filesystem blocks
  f_bavail       Blocks available to a nonprivileged process

  `f_blocks` and `f_bavail` are multiplied by `f_bsize` to produce byte
  values. Identity and usage are separate capabilities, so valid identity
  data remains available if the numeric usage fields are invalid.
*/
Filesystem_snapshot query_filesystem_platform(std::string_view path) {
  Filesystem_snapshot result;
  // An empty path cannot identify a filesystem or provide its usage
  if (path.empty()) {
    const auto error = std::make_error_code(std::errc::invalid_argument);
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  // statfs() returns identity and usage in the same record. If the query
  // fails, neither capability has usable data.
  struct statfs information {};
  const std::string path_string{path};
  if (statfs(path_string.c_str(), &information) != 0) {
    const auto error = errno_code();
    result.identity = System_result<Filesystem_identity>::failure(error);
    result.usage = System_result<Filesystem_usage>::failure(error);
    return result;
  }

  // Keep both parts of f_fsid so the ID identifies the mounted filesystem
  const std::string filesystem_id = std::to_string(information.f_fsid.val[0]) +
                                    ":" +
                                    std::to_string(information.f_fsid.val[1]);
  result.identity = System_result<Filesystem_identity>::success(
      {filesystem_id, information.f_mntonname, information.f_mntfromname,
       information.f_fstypename});

  // Validate the block counts and both multiplications before converting usage
  // to bytes. This does not invalidate the identity collected above.
  if (information.f_bsize <= 0 || information.f_blocks == 0 ||
      information.f_bavail > information.f_blocks ||
      multiply_overflows(information.f_blocks, information.f_bsize) ||
      multiply_overflows(information.f_bavail, information.f_bsize)) {
    result.usage = System_result<Filesystem_usage>::failure(
        std::make_error_code(std::errc::value_too_large));
    return result;
  }

  // f_bavail is used because the public API reports space available to a
  // nonprivileged process, not all blocks that the filesystem considers free.
  result.usage = System_result<Filesystem_usage>::success(
      {information.f_blocks * static_cast<uint64_t>(information.f_bsize),
       information.f_bavail * static_cast<uint64_t>(information.f_bsize)});
  return result;
}

/**
  Report macOS storage device capabilities as unavailable.

  statfs() provides mounted filesystem data. On Linux, procfs and sysfs also
  provide a storage device inventory, partition relationships and cumulative
  block I/O counters.

  However, statfs() does not provide equivalent macOS data, so this library does
  not create storage device records from filesystem metadata.
*/
Storage_snapshot query_storage_devices_platform() {
  Storage_snapshot result;
  // These capabilities are unsupported rather than failed because no native
  // storage device collection was attempted.
  result.inventory = System_result<Storage_inventory>::unavailable();
  result.capacities = System_result<Storage_capacities>::unavailable();
  result.hierarchy = System_result<Storage_hierarchy>::unavailable();
  result.read_write = System_result<Storage_read_write_snapshot>::unavailable();
  result.flush = System_result<Storage_flush_snapshot>::unavailable();
  return result;
}

/**
  Collect macOS host memory from sysctl and Mach records.

  Source and field                  Contents
  --------------------------------  -----------------------------------------
  hw.memsize                        Physical memory in bytes
  HOST_VM_INFO64 free_count         Free memory pages
  HOST_VM_INFO64 inactive_count     Inactive memory pages
  host_page_size()                  Size of one memory page in bytes
  vm.swapusage xsu_total            Total swap space in bytes
  vm.swapusage xsu_used             Used swap space in bytes

  Free memory is free_count * page_size. Here, available memory is defined as
  (free_count + inactive_count) * page_size. Unlike Linux MemAvailable, this
  is a simple sum of two page counts.

  Linux reads the memory breakdown from `Buffers`, `Cached` and `Slab` in
  /proc/meminfo. It reads paging activity from `pgpgin` and `pgpgout` in
  /proc/vmstat. The macOS records used here do not provide values with matching
  semantics, so the `breakdown` and `swap_activity` capabilities remain
  unavailable.

  The `memory` and `swap_capacity` capabilities use independent macOS queries,
  so the failure of one does not prevent collection of the other.
*/
Host_memory_snapshot query_host_memory_platform() {
  Host_memory_snapshot result;
  // macOS has no supported source for these capabilities.
  result.breakdown = System_result<Host_memory_breakdown>::unavailable();
  result.swap_activity = System_result<Swap_activity_info>::unavailable();

  // Read the installed physical memory size directly in bytes.
  uint64_t total_bytes{0};
  size_t total_size = sizeof(total_bytes);
  if (sysctlbyname("hw.memsize", &total_bytes, &total_size, nullptr, 0) != 0) {
    result.memory = System_result<Host_memory_info>::failure(errno_code());
  } else {
    // Read the native page size first, then request the 64-bit virtual memory
    // counters needed for free and available memory.
    const host_t host = mach_host_self();
    vm_size_t page_size{0};
    vm_statistics64_data_t statistics{};
    // host_statistics64() measures the buffer count in integer_t elements.
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const kern_return_t page_result = host_page_size(host, &page_size);
    const kern_return_t statistics_result =
        page_result == KERN_SUCCESS
            ? host_statistics64(host, HOST_VM_INFO64,
                                reinterpret_cast<host_info64_t>(&statistics),
                                &count)
            : page_result;
    // Release the send right returned by mach_host_self() after the calls.
    mach_port_deallocate(mach_task_self(), host);

    // For this API, macOS available memory = free + inactive pages. It is
    // not equivalent to Linux MemAvailable.
    const uint64_t available_pages =
        static_cast<uint64_t>(statistics.free_count) +
        static_cast<uint64_t>(statistics.inactive_count);
    if (statistics_result != KERN_SUCCESS) {
      result.memory = System_result<Host_memory_info>::failure(
          mach_error_code(statistics_result));
    } else if (multiply_overflows(statistics.free_count, page_size) ||
               multiply_overflows(available_pages, page_size)) {
      // A page count that cannot be represented in bytes invalidates memory.
      result.memory = System_result<Host_memory_info>::failure(
          std::make_error_code(std::errc::value_too_large));
    } else {
      result.memory = System_result<Host_memory_info>::success(
          {total_bytes,
           static_cast<uint64_t>(statistics.free_count) * page_size,
           available_pages * page_size});
    }
  }

  // Collect swap capacity even if the physical memory query failed.
  // vm.swapusage reports byte counts, so free swap is total minus used.
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

/**
  Collect cumulative CPU times from Mach PROCESSOR_CPU_LOAD_INFO.

  Field             Cpu_times value
  ----------------  ---------------------------------------
  CPU_STATE_USER    Time spent running user code
  CPU_STATE_NICE    Time spent running low priority user code
  CPU_STATE_SYSTEM  Time spent running kernel code
  CPU_STATE_IDLE    Idle time

  host_processor_info() returns one processor_cpu_load_info record for each
  logical processor. Each field is a cumulative clock tick count. The values
  are converted to milliseconds with _SC_CLK_TCK.

  The collector sums each field to produce the leading `cpu` record, followed
  by `cpu0`, `cpu1` and the remaining logical processors. Mach allocates the
  returned array, so it must be released with vm_deallocate() before the
  snapshot is published.

  PROCESSOR_CPU_LOAD_INFO has no counters that match the extended CPU
  capability, so `extended_times` remains unavailable.
*/
Host_cpu_snapshot query_host_cpu_platform() {
  Host_cpu_snapshot result;
  // The Mach record provides only the four portable CPU time values
  result.extended_times = System_result<Cpu_extended_snapshot>::unavailable();

  // Read the clock tick rate used by the native cumulative counters
  errno = 0;
  const long ticks_per_second = sysconf(_SC_CLK_TCK);
  if (ticks_per_second <= 0) {
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::invalid_argument);
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    return result;
  }

  // host_processor_info() returns an array allocated by Mach and its length in
  // integer_t elements. Release the host send right after the query.
  const host_t host = mach_host_self();
  natural_t processor_count{0};
  processor_info_array_t processor_information{nullptr};
  mach_msg_type_number_t processor_information_count{0};
  const kern_return_t information_result =
      host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &processor_count,
                          &processor_information, &processor_information_count);
  const kern_return_t host_deallocation_result =
      mach_port_deallocate(mach_task_self(), host);

  // Convert the returned element count to bytes when releasing the array.
  const auto deallocate_information = [&]() {
    if (processor_information == nullptr) return KERN_SUCCESS;
    return vm_deallocate(mach_task_self(),
                         reinterpret_cast<vm_address_t>(processor_information),
                         static_cast<vm_size_t>(processor_information_count) *
                             sizeof(integer_t));
  };

  // Attempt to release any returned array before reporting a query error.
  if (information_result != KERN_SUCCESS) {
    deallocate_information();
    result.times = System_result<Cpu_times_snapshot>::failure(
        mach_error_code(information_result));
    return result;
  }
  // Failure to release the host send right also makes the collection fail.
  if (host_deallocation_result != KERN_SUCCESS) {
    deallocate_information();
    result.times = System_result<Cpu_times_snapshot>::failure(
        mach_error_code(host_deallocation_result));
    return result;
  }

  // The returned count measures integer_t elements, not processor records.
  // Each logical processor must contribute one complete record.
  // Check for overflow before calculating the expected element count.
  const bool count_overflows =
      processor_count > std::numeric_limits<mach_msg_type_number_t>::max() /
                            PROCESSOR_CPU_LOAD_INFO_COUNT;
  const mach_msg_type_number_t expected_count =
      count_overflows ? 0 : processor_count * PROCESSOR_CPU_LOAD_INFO_COUNT;

  // Reject an empty or incomplete array before reading its positional fields.
  if (processor_count == 0 || processor_information == nullptr ||
      count_overflows || processor_information_count != expected_count) {
    // Mach may have allocated an array even when its metadata is invalid.
    const kern_return_t deallocation_result = deallocate_information();

    // Report invalid metadata unless releasing the array produced a Mach error.
    result.times = deallocation_result == KERN_SUCCESS
                       ? System_result<Cpu_times_snapshot>::failure(
                             std::make_error_code(std::errc::invalid_argument))
                       : System_result<Cpu_times_snapshot>::failure(
                             mach_error_code(deallocation_result));
    return result;
  }

  // Build the values for each logical processor while accumulating the
  // counters used for the leading aggregate record.
  const auto *loads =
      reinterpret_cast<processor_cpu_load_info_t>(processor_information);
  std::array<uint64_t, CPU_STATE_MAX> aggregate{};
  Cpu_times_snapshot times;
  times.cpus.reserve(static_cast<size_t>(processor_count) + 1U);
  bool valid = true;

  for (natural_t processor = 0; processor < processor_count; ++processor) {
    // Sum all four native counters and reject an aggregate that would overflow.
    for (size_t state = 0; state < CPU_STATE_MAX; ++state) {
      const uint64_t ticks = loads[processor].cpu_ticks[state];
      if (add_overflows(aggregate[state], ticks)) {
        valid = false;
        break;
      }
      aggregate[state] += ticks;
    }
    if (!valid) break;

    // Convert this processor's 4 clock tick counters to milliseconds.
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

  // Convert the accumulated counters and place the aggregate `cpu` record
  // before the records for individual logical processors.
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

  // Release the array allocated by Mach before publishing the completed
  // snapshot.
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

/**
  Collect macOS process memory from a libproc task record.

  Source and field                     Contents
  -----------------------------------  --------------------------------------
  proc_name()                          Process name
  getpid()                             Process ID
  PROC_PIDTASKINFO pti_resident_size   Resident memory in bytes

  proc_pidinfo(PROC_PIDTASKINFO) fills a proc_taskinfo record. The record
  contains other memory and fault counters, but they do not provide the public
  data memory, swapped memory or major page fault values, so the `details` and
  `page_faults` capabilties are unavailable.

  Identity and residency are collected independently.
*/
Process_memory_snapshot query_process_memory_platform() {
  Process_memory_snapshot result;
  const pid_t pid = getpid();
  // Process identity is independent of the process memory record
  result.identity = query_process_identity(pid);

  // proc_pidinfo() returns the number of bytes written. Require one complete
  // proc_taskinfo record so a partial result is never published.
  struct proc_taskinfo task_information {};
  errno = 0;
  const int task_size = proc_pidinfo(
      pid, PROC_PIDTASKINFO, 0, &task_information, sizeof(task_information));
  if (task_size != sizeof(task_information)) {
    // Some libproc failures do not set errno, so use a general I/O error.
    auto error = errno_code();
    if (!error) error = std::make_error_code(std::errc::io_error);
    result.residency = System_result<Process_memory_residency>::failure(error);
  } else {
    result.residency = System_result<Process_memory_residency>::success(
        {task_information.pti_resident_size});
  }

  // macOS has no equivalent system API for these values.
  result.details = System_result<Process_memory_details>::unavailable();
  result.page_faults = System_result<Process_page_faults>::unavailable();
  return result;
}

/**
  Collect current process thread metadata from Mach thread records.

  Source                         Field used
  -----------------------------  -------------------------------------------
  task_threads()                 Thread port array and thread count
  THREAD_IDENTIFIER_INFO         64-bit thread_id
  THREAD_EXTENDED_INFO           Fixed size pth_name buffer
  THREAD_BASIC_INFO              run_state
  THREAD_BASIC_INFO              user_time seconds and microseconds
  THREAD_BASIC_INFO              system_time seconds and microseconds

  task_threads() returns a Mach allocated array of thread send rights. Each
  right is queried with the three thread_info() flavors above.

  CPU times are converted to whole milliseconds.

  A thread can exit between enumeration and a thread_info() call. Such a
  thread is omitted when at least one valid thread remains.

  For any other Mach errors, the `threads` and `cpu` capabilities will fail.

  Every thread right and the allocated array must be released before the
  snapshots are published.

  The runtime, extended CPU, scheduler and thread I/O capabilities are not
  available in this implementation. Process identity is collected independently.

  The count supplied for each thread_info() flavor is measured in natural_t
  elements and must match the complete native record.
*/
Process_threads_snapshot query_process_threads_platform() {
  Process_threads_snapshot result;
  // No supported macOS source supplies these optional capabilities
  result.runtime = System_result<Process_runtime_info>::unavailable();
  result.extended_cpu =
      System_result<Thread_extended_cpu_snapshot>::unavailable();
  result.scheduler = System_result<Thread_scheduler_snapshot>::unavailable();
  result.io = System_result<Thread_io_snapshot>::unavailable();

  // Process identity does not depend on thread enumeration or mapping.
  result.identity = query_process_identity(getpid());

  /**
    Enumerate the threads in the current process.

    How it works:
    - task_threads() returns an array of thread ports and the number of entries.
    - Each port gives the caller a 'send right' to use with thread_info().
    - The caller must release each send right with mach_port_deallocate() and
      release the separately allocated array with vm_deallocate().

    Releasing these resources drops only the caller's references. It does not
    stop or otherwise change the threads.
  */
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
  // A successful enumeration must contain at least one thread.
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
  // Collect all fields for one thread before advancing to the next port. An
  // exited thread is recorded for later omission. Any other error stops the
  // collection.
  for (mach_msg_type_number_t index = 0; index < thread_count; ++index) {
    macOS_thread_sample sample;

    // THREAD_IDENTIFIER_INFO supplies the ID that aligns every output record
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
    // The count is measured in natural_t elements and must describe the full
    // thread_identifier_info record.
    if (identifier_count != THREAD_IDENTIFIER_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    // THREAD_EXTENDED_INFO supplies the fixed size thread name buffer.
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
    // Reject a short or unexpectedly shaped extended information record.
    if (extended_count != THREAD_EXTENDED_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    // THREAD_BASIC_INFO supplies the native run state and cumulative CPU times.
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
    // Reject a short or unexpectedly shaped basic information record.
    if (basic_count != THREAD_BASIC_INFO_COUNT) {
      collection_error = std::make_error_code(std::errc::invalid_argument);
      break;
    }

    // Copy the 3 native records into one sample so inventory and CPU data
    // stay associated with the same thread ID.
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
  // Sort, validate and convert the samples only if native collection completed.
  if (!collection_error) {
    collection_error = map_macos_thread_samples(
        std::move(samples), disappearance_error, inventory, cpu);
  }

  // Release all Mach resources, then publish both capabilities together.
  complete_macos_thread_collection(thread_list, collection_error,
                                   std::move(inventory), std::move(cpu),
                                   result);
  return result;
}

}  // namespace mysql::system_info::internal

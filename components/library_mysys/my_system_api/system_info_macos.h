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

#include <mach/mach.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include "mysql/components/library_mysys/system_info.h"

namespace mysql::system_info::internal {

/** Result of collecting one native macOS thread. */
enum class macOS_thread_sample_state { complete, disappeared };

/** Native macOS values collected for one thread. */
struct macOS_thread_sample {
  macOS_thread_sample_state sample_state{macOS_thread_sample_state::complete};
  uint64_t tid{0};
  std::string name;
  int run_state{0};
  int64_t user_seconds{0};
  int64_t user_microseconds{0};
  int64_t system_seconds{0};
  int64_t system_microseconds{0};
};

/** Callbacks used to release native macOS thread resources. */
struct macOS_thread_deallocator {
  void *context{nullptr};
  kern_return_t (*deallocate_port)(void *context, mach_port_t port){nullptr};
  kern_return_t (*deallocate_memory)(void *context, vm_address_t address,
                                     vm_size_t size){nullptr};
};

/** Own native thread ports and the Mach allocated thread array. */
class macOS_thread_resources {
 public:
  macOS_thread_resources(thread_act_array_t threads,
                         mach_msg_type_number_t thread_count,
                         macOS_thread_deallocator deallocator);
  macOS_thread_resources(const macOS_thread_resources &) = delete;
  macOS_thread_resources &operator=(const macOS_thread_resources &) = delete;
  ~macOS_thread_resources();

  /** Release every owned right and the thread array exactly once. */
  [[nodiscard]] std::error_code release();

 private:
  thread_act_array_t m_threads;
  mach_msg_type_number_t m_thread_count;
  macOS_thread_deallocator m_deallocator;
  bool m_released{false};
  std::error_code m_release_error;
};

/** Map a macOS native thread run state to the public thread state. */
[[nodiscard]] Thread_state macos_thread_state(int run_state);

/** Convert a macOS time value to whole milliseconds. */
[[nodiscard]] bool macos_time_value_to_milliseconds(
    int64_t seconds, int64_t microseconds,
    std::chrono::milliseconds &milliseconds);

/** Copy a possibly unterminated native thread name without exceeding size. */
[[nodiscard]] std::string copy_bounded_macos_thread_name(const char *name,
                                                         size_t size);

/** Build aligned thread inventory and CPU snapshots from native samples. */
[[nodiscard]] std::error_code map_macos_thread_samples(
    std::vector<macOS_thread_sample> samples,
    std::error_code disappearance_error, Thread_inventory &inventory,
    Thread_cpu_snapshot &cpu);

/** Release native resources and publish only after successful cleanup. */
void complete_macos_thread_collection(macOS_thread_resources &resources,
                                      std::error_code collection_error,
                                      Thread_inventory inventory,
                                      Thread_cpu_snapshot cpu,
                                      Process_threads_snapshot &result);

}  // namespace mysql::system_info::internal

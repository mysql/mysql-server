/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_IO_THREAD_AFFINITY_H
#define MYSQLROUTER_IO_THREAD_AFFINITY_H

#include <bitset>
#include <system_error>
#include <thread>

#if defined(__linux__)
#include <sched.h>  // CPU_SETSIZE
#elif defined(__FreeBSD__)
#include <sys/types.h>  // must be before sys/cpuset.h for cpusetid_t

#include <sys/cpuset.h>  // CPU_SETSIZE
#elif defined(_WIN32)
#include <Windows.h>
#endif

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/io_component_export.h"

/**
 * manage thread affinity.
 */
class IO_COMPONENT_EXPORT ThreadAffinity {
 public:
#if defined(__linux__) || defined(__FreeBSD__)
  static constexpr const int max_cpus = CPU_SETSIZE;
#elif defined(_WIN32)
  static constexpr const int max_cpus = 64;
#else
  static constexpr const int max_cpus = 1024;
#endif

#if defined(_WIN32)
  using native_handle_type = HANDLE;
#else
  using native_handle_type = pthread_t;
#endif

  ThreadAffinity(native_handle_type thread_id) noexcept
      : thread_id_{thread_id} {}

  // get current thread's native handle
  static native_handle_type current_thread_handle() noexcept;

  // get CPU affinity
  stdx::expected<std::bitset<max_cpus>, std::error_code> affinity()
      const noexcept;

  // set CPU affinity
  stdx::expected<void, std::error_code> affinity(
      std::bitset<ThreadAffinity::max_cpus> cpus) const noexcept;

 private:
  native_handle_type thread_id_;
};

#endif

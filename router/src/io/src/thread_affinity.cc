/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/thread_affinity.h"

#include "my_compiler.h"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <pthread.h>  // pthread_self(), pthread_setaffinity_np (linux), pthread_mach_thread_np
#endif

#if defined(__linux__)
#include <sched.h>  // CPU_SET
#elif defined(__FreeBSD__)
#include <sys/types.h>  // cpuset_t, must be before sys/cpuset.h

#include <pthread_np.h>  // pthread_setaffinity_np
#include <sys/cpuset.h>  // CPU_SET
#elif defined(_WIN32)
#include <Windows.h>
#elif defined(__sun)
#include <sys/processor.h>  // processor_affinity
#include <sys/procset.h>    // procset_t
#elif defined(__APPLE__)
#include <mach/thread_act.h>  // thread_policy_set
#endif

#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

// see /sql/resourcegroups/platform/thread_attrs_api*.cc

// windows has
// #include <winbase.h>
//
// - SetThreadAffinityMask()
//
// solaris
// #include <sys/processor.h>  // processor_affinity
// #include <sys/procset.h>    // procset_t

#if defined(__linux__)
using thread_affinity_cpu_set_type = cpu_set_t;
#elif defined(__FreeBSD__)
using thread_affinity_cpu_set_type = cpuset_t;
#endif

ThreadAffinity::native_handle_type
ThreadAffinity::current_thread_handle() noexcept {
#if defined(_WIN32)
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}

stdx::expected<std::bitset<ThreadAffinity::max_cpus>, std::error_code>
ThreadAffinity::affinity() const noexcept {
#if defined(__linux__) || defined(__FreeBSD__)
  thread_affinity_cpu_set_type cpuset;

  if (0 != pthread_getaffinity_np(thread_id_, sizeof(cpuset), &cpuset)) {
    return stdx::make_unexpected(net::impl::socket::last_error_code());
  }

  std::bitset<CPU_SETSIZE> cpus;
  for (int ndx{}; ndx < CPU_COUNT(&cpuset); ++ndx) {
    if (CPU_ISSET(ndx, &cpuset)) {
      cpus.set(ndx);
    }
  }

  return cpus;
#elif defined(_WIN32)
  // win32 has no GetThreadAffinityMask(), but SetAffinityMask() returns the old
  // mask.
  DWORD_PTR cur_mask =
      SetThreadAffinityMask(thread_id_, GetCurrentProcessorNumber());

  if (cur_mask == 0) {
    return stdx::make_unexpected(
        std::error_code(GetLastError(), std::generic_category()));
  }

  // set affinity mask to original value.
  SetThreadAffinityMask(thread_id_, cur_mask);

  // create bitmask from old_mask.
  return {std::in_place, cur_mask};
#else
  return stdx::make_unexpected(make_error_code(std::errc::not_supported));
#endif
}

// Doxygen gets confused by [[maybe_unused]]

/**
 @cond
*/
stdx::expected<void, std::error_code> ThreadAffinity::affinity(
    std::bitset<ThreadAffinity::max_cpus> cpus
    [[maybe_unused]]) const noexcept {
#if defined(__linux__) || defined(__FreeBSD__)
  thread_affinity_cpu_set_type cpuset;

  CPU_ZERO(&cpuset);

  for (size_t ndx{}; ndx < cpus.size(); ++ndx) {
    if (cpus.test(ndx)) {
      CPU_SET(ndx, &cpuset);
    }
  }

  if (0 != pthread_setaffinity_np(thread_id_, sizeof(cpuset), &cpuset)) {
    return stdx::make_unexpected(net::impl::socket::last_error_code());
  }

  return {};
#elif defined(_WIN32)
  DWORD_PTR new_mask = cpus.to_ullong();
  if (new_mask == 0) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }
  DWORD_PTR old_mask = SetThreadAffinityMask(thread_id_, new_mask);
  if (old_mask == 0) {
    return stdx::make_unexpected(
        std::error_code(GetLastError(), std::generic_category()));
  }

  return {};
#elif defined(__APPLE__)
  // macosx only allows to set the affinity to a "tag"
  //
  // threads with the same tag, run on the same core.
#if 0
  // to be tested.
  if (cpus.count() != 1) {
    return stdx::make_unexpected(make_error_code(std::errc::not_supported));
  }

  int affinity_tag = reinterpret_cast<int>(cpus.to_ulong());

  auto mach_thread_id = pthread_mach_thread_np(pthread_self());
  thread_affinity_policy policy = {affinity_tag};
  thread_policy_set(mach_thread_id, THREAD_AFFINITY_POLICY,
                    reinterpret_cast<thread_policy_t>(&policy), 1);
#else
  return stdx::make_unexpected(make_error_code(std::errc::not_supported));
#endif
#elif defined(__sun)
  // - processor_bind(), pset_bind()
  return stdx::make_unexpected(make_error_code(std::errc::not_supported));
#else
  return stdx::make_unexpected(make_error_code(std::errc::not_supported));
#endif
}

/**
 @endcond
*/

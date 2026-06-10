/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file components/library_mysys/my_system.cc Functions to retrieve system
  information like total physical memory and total number of logical CPUs
  adhering to limits set by cgroups
*/

#include <cassert>
#include <cstdint>
#include <optional>
#include <thread>

#include "my_config.h"  // HAVE_UNISTD_H
#include "my_system_api/my_system_api.h"
#include "mysql/components/library_mysys/my_system.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace {
bool *should_use_container_config = nullptr;
ulonglong configured_memory{0};

inline bool string_to_bool(const std::string &input) {
  using namespace std::string_literals;
  if (input == "ON"s) {
    return true;
  }
  return false;
}

inline std::optional<ulonglong> string_to_ull(const std::string &input) {
  if (input[0] == '-') {
    return std::nullopt;
  }

  try {
    errno = 0;
    ulonglong result = std::stoull(input);

    if (errno != 0) return std::nullopt;
    return result;
  } catch (...) {
  }
  return std::nullopt;
}
}  // anonymous namespace

bool has_container_resource_limits() {
#ifndef _WIN32
  return does_cgroup_limit_resources();
#endif /* !_WIN32 */

  return false;
}

bool init_container_aware(const std::string &is_container_aware) noexcept {
  return init_container_aware(string_to_bool(is_container_aware));
}

bool init_container_aware(const bool is_container_aware) noexcept {
  should_use_container_config = new bool{is_container_aware};

#ifndef _WIN32
  if (should_use_container_config && *should_use_container_config &&
      !is_running_in_cgroup()) {
    return false;
  }
  return true;
#endif /* !_WIN32 */

  return !is_container_aware;
}

void deinit_container_aware() noexcept {
  delete should_use_container_config;
  should_use_container_config = nullptr;
}

/**
  Get the total physical memory accessible to the server. Tries to read cgroup
  limits first. If failed or no limits were found, then tries to read from the
  system using corresponding system APIs.
  @return  total physical memory available to the server, or 0 on failure
*/
static inline uint64_t total_physical_memory() noexcept {
  try {
    uint64_t mem = 0;
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    mem = ms.ullTotalPhys;
#elif defined(HAVE_UNISTD_H) /* _WIN32 */
    long pages = sysconf(_SC_PHYS_PAGES);
    long pagesize = sysconf(_SC_PAGESIZE);
    if (pages > 0 && pagesize > 0) {
      mem = static_cast<uint64_t>(pages) * static_cast<uint64_t>(pagesize);
    }
    assert(should_use_container_config);
    if (should_use_container_config && *should_use_container_config) {
      const uint64_t cgroup_mem = my_cgroup_mem_limit();
      /* Use cgroup_mem if sysconf fails */
      if (cgroup_mem != 0 && (mem == 0 || cgroup_mem <= mem)) {
        return cgroup_mem;
      }
    }
#else
#error "Missing implementation of sysconf or GlobalMemoryStatusEx"
#endif /* HAVE_UNISTD_H */
    assert(mem != 0);
    return mem;
  } catch (...) {
    return 0;
  }
}

bool init_my_physical_memory(const std::string &memory) {
  const auto server_memory = string_to_ull(memory);
  assert(server_memory.has_value());
  return init_my_physical_memory(server_memory.value());
}

bool init_my_physical_memory(ulonglong memory) {
  if (memory == 0) {
    /* No limit. Physical memory available should be returned */
    return true;
  }
  if (memory <= total_physical_memory()) {
    configured_memory = memory;
    return true;
  }

  return false;
}

uint64_t my_physical_memory() noexcept {
  assert(should_use_container_config);
  if (configured_memory != 0) {
    return configured_memory;
  }
  return total_physical_memory();
}

uint32_t my_num_vcpus() noexcept {
  assert(should_use_container_config);
  if (const uint32_t n_vcpus = my_system_num_vcpus(); n_vcpus != 0) {
    return n_vcpus;
  }

  return std::thread::hardware_concurrency();
}

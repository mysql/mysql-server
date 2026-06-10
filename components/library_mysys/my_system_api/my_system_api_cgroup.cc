/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <unistd.h>
#include <cassert>
#include <climits>
#include <cstdint>
#include <fstream>
#include <functional>
#include <optional>
#include <string_view>

#include "my_config.h"  // HAVE_UNISTD_H
#include "my_system_api.h"

/**
  @file components/library_mysys/my_system_api/my_system_api_cgroup.cc
  Functions to retrieve total physical memory and total number of logical CPUs
  available to the server by reading the limits set by cgroups
*/

namespace my_system_cgroup {
/**
  Determine cgroup memory given memory limits from self and root cgroups

  In some scenarios, it is possible to find memory limits in both self cgroup
  (read and parsed from /proc/self/cgroup) as well as root cgroup.

  @param[in]  self_memory Memory discovered from our own cgroup
  @param[in]  root_memory Memory discovered from the root cgroup
  @return Final memory to be considered when determining cgroup memory limit
  or nullopt otherwise.
  @note Return value of 0 indicates no limits
*/
std::optional<uint64_t> get_cgroup_memory(
    const std::optional<uint64_t> &self_memory,
    const std::optional<uint64_t> &root_memory) {
  if (!self_memory && !root_memory) {
    return std::nullopt;
  }

  if (self_memory && root_memory) {
    /* Both are present, consider minimum ignoring 0 */
    const uint64_t self = *self_memory, root = *root_memory;

    if (self == 0 || root == 0) {
      return std::max(self, root);
    }
    return std::min(self, root);
  }

  if (self_memory) {
    return *self_memory;
  }
  return *root_memory;
}
} /* namespace my_system_cgroup */

namespace {
/** Path containing information about which cgroup we belong to */
constexpr std::string_view cgroup_info{"/proc/self/cgroup"};

/** cgroup v1 pattern to extract cgroup path from cgroup_info */
constexpr std::string_view v1_info_prefix{":memory:"};
/** cgroup v1 prefix to read memory limits */
const std::string v1_mem_prefix{"/sys/fs/cgroup/memory"};
/** cgroup v1 suffix to read memory limits */
const std::string v1_mem_suffix{"/memory.limit_in_bytes"};

/** cgroup v2 pattern to extract cgroup path from cgroup_info */
constexpr std::string_view v2_info_prefix{"0::"};
/** cgroup v2 prefix to read memory limits */
const std::string v2_mem_prefix{"/sys/fs/cgroup"};
/** cgroup v2 suffix to read memory limits */
const std::string v2_mem_suffix{"/memory.max"};

/**
  Utility: Read the first line from the file specified in path and copy its
  contents into the arguments passed
  @param[in]   path  Path to file
  @param[out]  args  Pass the arguments that you expect to read from the file
  in the order of their appearance in the file
  @return true if able to read and parse the file, false otherwise
*/
template <typename... Args>
bool read_line_from_file(const std::string_view &path, Args &...args) {
  std::ifstream file(path.data());
  if (!file.is_open()) {
    return false;
  }

  (file >> ... >> args);

  /* Unable to parse contents or hit error */
  if (file.fail() || file.bad()) {
    return false;
  }
  return true;
}

/**
  Determine which cgroup we belong to. This is common for cgroup v1 and v2.
  @return path to the cgroup  or std::nullopt on failure
*/
std::optional<std::string> get_cgroup_path(std::string_view pattern) {
  std::ifstream file(cgroup_info.data());
  if (!file.is_open()) {
    return std::nullopt;
  }

  /*
    The contents of cgroup_info differ in cgroup v1 and cgroup v2. The line
    matching the pattern looks like:
    cgroup v1: '8:memory:/path/to/cgroup'
    cgroup v2: '0::/path/to/cgroup'
    Substring after the pattern is the required cgroup path
  */
  for (std::string line; std::getline(file, line);) {
    if (const size_t match_pos = line.find(pattern);
        match_pos != std::string::npos) {
      return line.substr(match_pos + pattern.length());
    }
  }
  return std::nullopt;
}

static inline bool is_valid_path(const std::string_view &path) {
  if (path.empty() || path.front() != '/' || path.back() == '/') {
    return false;
  }

  /* check if any directory in path is empty or not absolute */
  size_t i = 1;
  while (i < path.size()) {
    size_t j = path.find('/', i);
    if (j == std::string_view::npos) {
      j = path.size();
    }

    const std::string_view dir = path.substr(i, j - i);
    if (dir.empty() || dir == "." || dir == "..") {
      return false;
    }
    i = j + 1;
  }

  return true;
}

/**
  Read memory limits from cgroups. This wrapper is common for both cgroup v1, v2

  @param[in]  info_prefix  Pattern used to extract cgroup from proc/self/cgroup
  @param[in]  mem_prefix   cgroup path prefix to identify memory controller
  @param[in]  mem_suffix   cgroup path suffix to identify file with memory limit
  @param[in]  with_root    True if limits are to be read from root cgroup, false
  otherwise (read cgroup from info_prefix)
  @param[in]  v2_default_handler Function to handle default memory in cgroup v2

  @return memory read from cgroup on success, 0 if default (unlimited), nullopt
  otherwise
  @note Default handler is required cgroup v2 and not v1 because as v1 writes 0
  or fixed value which can still be parsed, but v2 writes string 'max' which
  requires special handling
*/
std::optional<uint64_t> cgroup_memory(
    const std::string_view &info_prefix, const std::string &mem_prefix,
    const std::string &mem_suffix, const bool with_root,
    std::function<bool(const std::string_view &)> v2_default_handler =
        nullptr) {
  std::string cgroup_path;
  if (!with_root) {
    const auto cgroup = get_cgroup_path(info_prefix);
    if (!cgroup) {
      return std::nullopt;
    }

    /* Build path from cgroup */
    cgroup_path = mem_prefix + *cgroup + mem_suffix;
  } else {
    cgroup_path = mem_prefix + mem_suffix;
  }

  if (!is_valid_path(cgroup_path)) {
    return std::nullopt;
  }

  uint64_t memory;
  if (!read_line_from_file(cgroup_path, memory)) {
    if (v2_default_handler && v2_default_handler(cgroup_path)) {
      return 0;
    }
    return std::nullopt;
  }
  return memory;
}

/**
  Determine if the cgroup v1 memory is the default value and return 0 if it is.
  Return the input if it is not the default value or if the default could not be
  determined.
  @param[in]  memory  memory read from cgroup v1 paths
  @return 0 if memory value is default, input memory value otherwise
*/
uint64_t handle_v1_memory_default(uint64_t memory) {
#ifdef HAVE_UNISTD_H
  const long page_size = sysconf(_SC_PAGESIZE);
  assert(page_size > 0);

  /* Default value of memory limit in cgroup v1 */
  const uint64_t default_limit = LONG_MAX - (LONG_MAX % page_size);

  /* Treat default value as no limits and return 0 */
  return (memory == default_limit) ? 0 : memory;
#endif
  return memory;
}

std::optional<uint64_t> cgroup_v1_memory() {
  const auto self =
      cgroup_memory(v1_info_prefix, v1_mem_prefix, v1_mem_suffix, false);

  const auto root =
      cgroup_memory(v1_info_prefix, v1_mem_prefix, v1_mem_suffix, true);

  const auto memory = my_system_cgroup::get_cgroup_memory(self, root);
  if (memory) {
    return handle_v1_memory_default(*memory);
  }
  return std::nullopt;
}

/**
  Identify if cgroup v2 memory is the default or if could not be determined
  @param[in]   path  Path to the memory v2 cgroup path
  @return true if memory value is default, false otherwise
*/
bool handle_v2_memory_default(const std::string_view &path) {
  std::ifstream file(path.data());
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  if (!std::getline(file, line)) {
    return false;
  }
  return line.find("max") != std::string::npos;
}

std::optional<uint64_t> cgroup_v2_memory() {
  const auto self = cgroup_memory(v2_info_prefix, v2_mem_prefix, v2_mem_suffix,
                                  false, handle_v2_memory_default);

  const auto root = cgroup_memory(v2_info_prefix, v2_mem_prefix, v2_mem_suffix,
                                  true, handle_v2_memory_default);

  return my_system_cgroup::get_cgroup_memory(self, root);
}
} /* namespace */

bool is_running_in_cgroup() {
  return (cgroup_v1_memory().has_value() || cgroup_v2_memory().has_value());
}

bool does_cgroup_limit_resources() {
  /* Value less than 1 indicates no limits are set */
  if (const auto v2_mem = cgroup_v2_memory(); v2_mem.has_value()) {
    return (v2_mem.value() >= 1);
  }
  if (const auto v1_mem = cgroup_v1_memory(); v1_mem.has_value()) {
    return (v1_mem.value() >= 1);
  }
  return false;
}

uint64_t my_cgroup_mem_limit() {
  if (const auto v2_mem = cgroup_v2_memory(); v2_mem.has_value()) {
    return v2_mem.value();
  }

  if (const auto v1_mem = cgroup_v1_memory(); v1_mem.has_value()) {
    return v1_mem.value();
  }
  return 0;
}

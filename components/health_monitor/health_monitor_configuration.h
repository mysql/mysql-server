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

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>

namespace hm::configuration {

constexpr char k_disk_monitored_default[]{""};
constexpr bool k_disk_fallback_enable_default{false};
// FN_REFLEN from my_io.h, which components cannot include
constexpr size_t k_max_monitored_path_length{512};

/** Convert a Disk Forecaster report interval from minutes to seconds. */
constexpr int report_interval_seconds(int minutes) { return minutes * 60; }

/** Return true if a string meets the configured length and content limits. */
inline bool valid_configuration_string(const char *value, size_t length,
                                       size_t maximum_length,
                                       bool allow_empty) {
  return value != nullptr && length <= maximum_length &&
         (allow_empty || length != 0) &&
         std::memchr(value, '\0', length) == nullptr;
}

/** Return true if a monitored path is empty or a valid absolute path. */
inline bool valid_monitored_path(const char *path, size_t length) {
  if (!valid_configuration_string(path, length, k_max_monitored_path_length - 1,
                                  true)) {
    return false;
  }
  return length == 0 ||
         std::filesystem::path(std::string{path, length}).is_absolute();
}

}  // namespace hm::configuration

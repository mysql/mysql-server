/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/routing_guidelines_version.h"

#include <algorithm>
#include <charconv>  // from_chars
#include <string>

#include "mysql/harness/string_utils.h"  // split_string

namespace mysqlrouter {

std::string to_string(const RoutingGuidelinesVersion &version) {
  return std::to_string(version.major) + '.' + std::to_string(version.minor);
}

RoutingGuidelinesVersion get_routing_guidelines_supported_version() {
  return kSupportedRoutingGuidelinesVersions.back();
}

RoutingGuidelinesVersion routing_guidelines_version_from_string(
    const std::string &version_string) {
  const auto parts = mysql_harness::split_string(version_string, '.');

  auto is_valid_number = [](const std::string &str) {
    for (const char c : str) {
      if (!std::isdigit(c)) {
        return false;
      }
    }
    return true;
  };

  if (parts.size() != 2 ||
      !std::all_of(std::begin(parts), std::end(parts), is_valid_number)) {
    throw std::runtime_error(
        "Invalid routing guidelines version format. Expected <major>.<minor> "
        "got " +
        version_string);
  }

  std::uint32_t major{0};
  std::uint32_t minor{0};

  auto [ptr1, ec1] = std::from_chars(parts[0].data(),
                                     parts[0].data() + parts[0].size(), major);
  auto [ptr2, ec2] = std::from_chars(parts[1].data(),
                                     parts[1].data() + parts[1].size(), minor);

  if (ec1 != std::errc() || ec2 != std::errc()) {
    throw std::runtime_error(
        "Invalid routing guidelines version format. Expected <major>.<minor> "
        "got " +
        version_string);
  }

  return RoutingGuidelinesVersion{major, minor};
}

bool routing_guidelines_version_is_compatible(
    const mysqlrouter::RoutingGuidelinesVersion &supported,
    const mysqlrouter::RoutingGuidelinesVersion &available) {
  return (available <= supported) && (supported.major - available.major <= 1);
}

}  // namespace mysqlrouter

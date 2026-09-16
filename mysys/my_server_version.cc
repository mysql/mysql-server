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
  @file mysys/my_server_version.cc Functions to convert server version strings
  into the encoded version ID used by My_server_version
*/

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>

#include "my_server_version.h"

namespace {
uint parse_next_part(std::string &version, bool &is_valid, char delimiter) {
  uint version_part = 0;
  const auto pos = version.find(delimiter);
  const std::string part =
      (pos != std::string::npos) ? version.substr(0, pos) : version;

  try {
    size_t parsed_length = 0;
    const auto parsed_component = std::stoul(part, &parsed_length, 10);

    if (parsed_length != part.length() || parsed_component >= 100) {
      is_valid = false;
      return 0;
    }

    version_part = static_cast<uint>(parsed_component);
  } catch (...) {
    is_valid = false;
    return 0;
  }

  if (pos != std::string::npos) {
    /* pos + 1 to skip the delimiter*/
    version.erase(0, pos + 1);
  } else {
    version.clear();
  }

  return version_part;
}

}  // namespace

uint My_server_version::version_string_to_id(
    const std::string &version_string) {
  bool is_valid = true;
  std::string version = version_string;

  const uint major = parse_next_part(version, is_valid, '.');
  const uint minor = parse_next_part(version, is_valid, '.');
  const uint patch = parse_next_part(version, is_valid, '-');

  return is_valid ? encode_version(major, minor, patch) : 0;
}

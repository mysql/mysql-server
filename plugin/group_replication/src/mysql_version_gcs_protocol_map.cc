/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include <assert.h>
#include <cerrno>
#include <cinttypes>
#include <limits>
#include <regex>

/*
 * +--------------------------------------+
 * | MySQL version | GCS protocol version |
 * +======================================+
 * | 5.7.14        | 1                    |
 * | 8.0.16        | 2                    |
 * | 8.0.27        | 3                    |
 * +--------------------------------------+
 */

static Member_version const version_5_7_14(0x050714);
static Member_version const version_8_0_16(0x080016);
static Member_version const version_8_0_27(0x080027);

/*
 * Protocol 1 maps to version 5.7.14.
 * Protocol 2 maps to version 8.0.16.
 * Protocol 3 maps to version 8.0.27.
 *
 * When you update this function, remember to update the convert_to_gcs_protocol
 * function accordingly.
 */
Member_version convert_to_mysql_version(
    Gcs_protocol_version const &gcs_protocol) {
  switch (gcs_protocol) {
    case Gcs_protocol_version::V1:
      return version_5_7_14;
    case Gcs_protocol_version::V2:
      return version_8_0_16;
    case Gcs_protocol_version::V3:
      return version_8_0_27;
    case Gcs_protocol_version::UNKNOWN:
    case Gcs_protocol_version::V4:
    case Gcs_protocol_version::V5:
      /* This should not happen... */
      assert(false && "GCS protocol should have been V1 or V2, or V3");
      break;
  }
  return Member_version(0x000000);
}

/*
 * Versions in the domain [5.7.14; 8.0.16[ map to protocol 1.
 * Versions in the domain [8.0.16; 8.0.22[ map to protocol 2.
 * Versions in the domain [8.0.27; my-version] map to protocol 3.
 *
 * When you update this function, remember to update the
 * convert_to_mysql_version function accordingly.
 */
Gcs_protocol_version convert_to_gcs_protocol(
    Member_version const &mysql_version, Member_version const &my_version) {
  if (version_5_7_14 <= mysql_version && mysql_version < version_8_0_16) {
    return Gcs_protocol_version::V1;
  } else if (version_8_0_16 <= mysql_version &&
             mysql_version < version_8_0_27) {
    return Gcs_protocol_version::V2;
  } else if (version_8_0_27 <= mysql_version && mysql_version <= my_version) {
    return Gcs_protocol_version::V3;
  } else {
    return Gcs_protocol_version::UNKNOWN;
  }
}

static std::string const one_or_two_digit_number_regex = "([0-9]{1,2})";

#ifndef NDEBUG
static bool is_one_or_two_digit_number(const std::string &s) {
  return std::regex_match(s, std::regex(one_or_two_digit_number_regex));
}
#endif

bool valid_mysql_version_string(char const *version_str) {
  std::string const dot_regex = "\\.";
  std::regex mysql_version_regex(one_or_two_digit_number_regex + dot_regex +
                                 one_or_two_digit_number_regex + dot_regex +
                                 one_or_two_digit_number_regex);
  return std::regex_match(version_str, mysql_version_regex);
}

/* Requires that str is_one_or_two_digit_number. */
static unsigned int convert_to_base_16_number(char const *const str) {
  assert(is_one_or_two_digit_number(std::string(str)));

  int constexpr BASE_16 = 16;
  char *end = nullptr;
  unsigned int base_16_number = 0;

  // Convert.
  auto converted_number = std::strtoumax(str, &end, BASE_16);

#ifndef NDEBUG
  /* Paranoid check; this should always be OK because
   * valid_mysqld_version_string is a precondition. */
  bool const would_overflow =
      (converted_number > std::numeric_limits<decltype(base_16_number)>::max());
  assert(!would_overflow);
#endif

  base_16_number = static_cast<decltype(base_16_number)>(converted_number);

  return base_16_number;
}

/* Requires that str be a valid_mysql_version_string. */
Member_version convert_to_member_version(char const *str) {
  assert(valid_mysql_version_string(str));

  std::string version_str(str);
  Member_version version(0);
  auto const major_dot_index = version_str.find('.');
  auto const minor_dot_index = version_str.find('.', major_dot_index + 1);

  // Get major version.
  auto major_str = version_str.substr(0, major_dot_index);
  auto major = convert_to_base_16_number(major_str.c_str());

  // Get minor version.
  auto minor_str = version_str.substr(major_dot_index + 1,
                                      minor_dot_index - 1 - major_dot_index);
  auto minor = convert_to_base_16_number(minor_str.c_str());

  // Get patch version.
  auto patch_str = version_str.substr(minor_dot_index + 1);
  auto patch = convert_to_base_16_number(patch_str.c_str());

  return Member_version((major << 16) | (minor << 8) | patch);
}

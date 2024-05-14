/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_STRING_TRIM_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_STRING_TRIM_H_

#include <string>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

namespace helper {

inline void left(std::string *to_trim) {
  std::size_t pos = to_trim->find_first_not_of(" ");

  to_trim->erase(0, pos);
}

inline void right(std::string *to_trim) {
  auto size = to_trim->length();
  for (; size > 0; --size) {
    if ((*to_trim)[size - 1] != ' ') break;
  }

  if (size == to_trim->length()) return;

  to_trim->erase(size);
}

inline void trim(std::string *to_trim) {
  left(to_trim);
  right(to_trim);
}

inline std::string make_left(const std::string &to_trim) {
  std::size_t pos = to_trim.find_first_not_of(" ");

  if (std::string::npos == pos) return {};

  return to_trim.substr(pos);
}

inline std::string make_right(const std::string &to_trim) {
  auto size = to_trim.length();
  for (; size > 0; --size) {
    if (to_trim[size - 1] != ' ') break;
  }

  if (size == to_trim.length()) return to_trim;

  if (0 == size) return {};

  return to_trim.substr(0, size);
}

inline std::string make_trim(const std::string &to_trim) {
  return make_right(make_left(to_trim));
}

}  // namespace helper

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_STRING_TRIM_H_

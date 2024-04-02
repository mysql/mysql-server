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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_MYSQL_NUMERIC_VALUE_CC_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_MYSQL_NUMERIC_VALUE_CC_

#include "helper/mysql_numeric_value.h"
#include "helper/container/generic.h"

namespace helper {

DataTypeInText get_type_inside_text(const std::string &value) {
  using namespace helper::container;

  auto it = value.begin();

  if (it == value.end()) return kDataString;

  if (has(std::string("+-"), *it)) {
    ++it;
  }

  int numbers = 0;
  while (it != value.end()) {
    if (!(*it >= '0' && *it <= '9')) break;
    ++it;
    ++numbers;
  }

  if (it == value.end() && numbers) return kDataInteger;

  if (!has(std::string("eE."), *it)) return kDataString;

  if (*it == '.') {
    while (it != value.end()) {
      if (!(*it >= '0' && *it <= '9')) break;
      ++numbers;
      ++it;
    }

    if (it == value.end()) {
      if (numbers)
        return kDataFloat;
      else
        return kDataString;
    }
  }

  if (!has(std::string("Ee"), *it)) return kDataString;
  ++it;

  if (it == value.end()) return kDataString;

  if (!has(std::string("+-"), *it)) return kDataString;
  ++it;

  if (it == value.end()) return kDataString;

  numbers = 0;
  while (it != value.end()) {
    if (!(*it >= '0' && *it <= '9')) break;
    ++numbers;
    ++it;
  }

  if (0 == numbers) return kDataString;

  if (it != value.end()) return kDataString;

  return kDataFloat;
}

}  // namespace helper

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_HELPER_MYSQL_NUMERIC_VALUE_CC_

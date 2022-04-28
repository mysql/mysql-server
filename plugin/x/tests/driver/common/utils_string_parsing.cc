/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/tests/driver/common/utils_string_parsing.h"

namespace aux {

const std::string ALLOWED_HEX_CHARACTERS = "0123456789abcdef";

int replace_all(std::string &input, const std::string &to_find,
                const std::string &change_to, int limit) {
  int replaced = 0;
  if (to_find.empty()) return 0;

  size_t position = input.find(to_find);

  while (std::string::npos != position && limit--) {
    input.replace(position, to_find.size(), change_to);
    position = input.find(to_find, position + change_to.size());
    ++replaced;
  }

  return replaced;
}

void trim(std::string &value, const std::string &whitespace) {
  std::size_t first = value.find_first_not_of(whitespace);
  std::size_t last = value.find_last_not_of(whitespace);

  if (first == std::string::npos) {
    value = "";
    return;
  }

  if (last == std::string::npos) {
    value = value.substr(first);
    return;
  }

  value = value.substr(first, last - first + 1);
}

}  // namespace aux

/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "utils_string_parsing.h"

namespace aux {

const std::string ALLOWED_HEX_CHARACTERS = "0123456789abcdef";

void replace_all(std::string &input, const std::string &to_find,
                 const std::string &change_to) {
  if (to_find.empty()) return;

  size_t position = input.find(to_find);

  while (std::string::npos != position) {
    input.replace(position, to_find.size(), change_to);
    position = input.find(to_find, position + change_to.size());
  }
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

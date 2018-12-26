/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_STRING_PARSING_H_
#define PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_STRING_PARSING_H_

#include <stdio.h>
#include <stdexcept>
#include <string>

namespace aux {

extern const std::string ALLOWED_HEX_CHARACTERS;

template <typename Input_type, typename Output_type>
void unhex(const Input_type &input, Output_type &output) {
  std::size_t position = 0;
  std::size_t distance = std::distance(input.begin(), input.end());

  while (position < distance) {
    int iteration = 2;
    unsigned char decimal_value = 0;

    while (iteration-- > 0) {
      unsigned char data = tolower(input.at(position++));

      std::size_t pos = ALLOWED_HEX_CHARACTERS.find(data);

      if (std::string::npos == pos) {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp),
                 "Invalid hexadecimal character %c at position %zu", data, pos);
        throw std::logic_error(tmp);
      }

      decimal_value = static_cast<unsigned char>(decimal_value << 4) |
                      static_cast<unsigned char>(pos);
    }

    output.push_back(decimal_value);
  }
}

template <typename Container_type>
void split(Container_type &result, const std::string &input,
           const std::string &delimiters, const bool compress_delimiters) {
  std::string copy_of_intput = input;
  std::size_t begin = 0;

  bool force_insert = true;

  while (std::string::npos != begin) {
    std::size_t end = input.find_first_of(delimiters, begin);
    std::string token;

    if (std::string::npos == end) {
      token = input.substr(begin);
      force_insert = true;
    } else {
      token = input.substr(begin, end - begin);
      ++end;
    }

    if ((force_insert || !(token.empty())) || !compress_delimiters) {
      result.push_back(token);
    }

    force_insert = false;
    begin = end;
  }
}

int replace_all(std::string &input, const std::string &to_find,
                const std::string &change_to);

void trim(std::string &value, const std::string &whitespace = " \t");

}  // namespace aux

#endif  // PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_STRING_PARSING_H_

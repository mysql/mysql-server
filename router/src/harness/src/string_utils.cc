/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/string_utils.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace mysql_harness {
std::vector<std::string> split_string(const std::string_view &data,
                                      const char delimiter, bool allow_empty) {
  if (data.empty()) return {};

  std::vector<std::string> result;
  size_t cur = 0;

  for (size_t end = data.find(delimiter, cur); end != std::string_view::npos;
       end = data.find(delimiter, cur)) {
    auto token = data.substr(cur, end - cur);
    if (!token.empty() || allow_empty) {
      result.emplace_back(token);
    }

    cur = end + 1;  // skip this delimiter
  }

  auto token = data.substr(cur);
  if (!token.empty() || allow_empty) {
    result.emplace_back(token);
  }

  return result;
}

void left_trim(std::string &str) {
  str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), ::isspace));
}

void right_trim(std::string &str) {
  str.erase(std::find_if_not(str.rbegin(), str.rend(), ::isspace).base(),
            str.end());
}

void trim(std::string &str) {
  left_trim(str);
  right_trim(str);
}

}  // namespace mysql_harness

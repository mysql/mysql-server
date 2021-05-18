/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <sstream>

using std::string;

namespace mysql_harness {
std::vector<string> split_string(const string &data, const char delimiter,
                                 bool allow_empty) {
  std::stringstream ss(data);
  std::string token;
  std::vector<string> result;

  if (data.empty()) {
    return {};
  }

  while (std::getline(ss, token, delimiter)) {
    if (token.empty() && !allow_empty) {
      // Skip empty
      continue;
    }
    result.push_back(token);
  }

  // When last character is delimiter, it denotes an empty token
  if (allow_empty && data.back() == delimiter) {
    result.push_back("");
  }

  return result;
}

void left_trim(string &str) {
  str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), ::isspace));
}

void right_trim(string &str) {
  str.erase(std::find_if_not(str.rbegin(), str.rend(), ::isspace).base(),
            str.end());
}

void trim(string &str) {
  left_trim(str);
  right_trim(str);
}

}  // namespace mysql_harness

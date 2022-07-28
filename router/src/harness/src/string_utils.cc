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
#include <optional>
#include <sstream>
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

namespace {

/** @brief Finds n-th occurence of character c in string s
 *
 * @param s string to search in
 * @param c character to search for
 * @param n which occurence of c in s we are looking for
 *
 * @return position of the nth occurence of character c in string c if found
 * @return nullopt if not found
 */
static std::optional<size_t> find_nth(const std::string_view &s, const char c,
                                      size_t n) {
  if (n == 0) return {};

  size_t result = 0;
  while (n-- != 0) {
    result = s.find(c, result);
    if (result == std::string::npos) return {};
    result++;
  }
  return result - 1;
}
}  // namespace

std::string limit_lines(const std::string &str, const size_t limit,
                        const std::string &replace_with) {
  size_t num_lines = std::count(str.begin(), str.end(), '\n');
  if (!str.empty() && str.back() != '\n') num_lines++;

  if (num_lines > limit) {
    size_t begin_lines = limit / 2 + limit % 2;
    size_t end_lines = limit - begin_lines;

    std::string result, line;
    std::istringstream iss_content(str);
    while ((begin_lines-- != 0) && std::getline(iss_content, line)) {
      result += line + "\n";
    }

    result += replace_with;

    if (end_lines > 0) {
      auto pos_end = find_nth(str, '\n', num_lines - end_lines);
      if (pos_end) {
        iss_content.seekg(*pos_end + 1);
        while (std::getline(iss_content, line)) {
          result += line + "\n";
        }
      }
    }

    return result;
  }

  return str;
}

}  // namespace mysql_harness

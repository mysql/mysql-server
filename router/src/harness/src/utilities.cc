/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "utilities.h"

#include <algorithm>  // replace
#include <cassert>
#include <cstdarg>
#include <cstring>  // vsnprintf
#include <string>

#include "mysql/harness/utility/string.h"  // wrap_string

namespace mysql_harness::utility {

std::string dirname(const std::string &path) {
  std::string::size_type pos = path.rfind('/');
  if (pos != std::string::npos)
    return std::string(path, 0, pos);
  else
    return ".";
}

std::string basename(const std::string &path) {
  std::string::size_type pos = path.rfind('/');
  if (pos != std::string::npos)
    return std::string(path, pos + 1);
  else
    return path;
}

void strip(std::string *str, const char *chars) {
  str->erase(str->find_last_not_of(chars) + 1);
  str->erase(0, str->find_first_not_of(chars));
}

std::string strip_copy(std::string str, const char *chars) {
  strip(&str, chars);
  return str;
}

std::string string_format(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_next;
  va_copy(args_next, args);

  int size = vsnprintf(nullptr, 0, format, args);
  std::vector<char> buf(static_cast<size_t>(size) + 1);
  va_end(args);

  vsnprintf(buf.data(), buf.size(), format, args_next);
  va_end(args_next);

  return std::string(buf.begin(), buf.end() - 1);
}

std::vector<std::string> wrap_string(const std::string &to_wrap, size_t width,
                                     size_t indent_size) {
  size_t curr_pos = 0;
  size_t wrap_pos = 0;
  size_t prev_pos = 0;
  std::string work{to_wrap};
  std::vector<std::string> res{};
  auto indent = std::string(indent_size, ' ');
  auto real_width = width - indent_size;

  size_t str_size = work.size();
  if (str_size < real_width) {
    res.push_back(indent + work);
  } else {
    work.erase(std::remove(work.begin(), work.end(), '\r'), work.end());
    std::replace(work.begin(), work.end(), '\t', ' ');
    str_size = work.size();

    do {
      curr_pos = prev_pos + real_width;

      // respect forcing newline
      wrap_pos = work.find("\n", prev_pos);
      if (wrap_pos == std::string::npos || wrap_pos > curr_pos) {
        // No new line found till real_width
        wrap_pos = work.find_last_of(" ", curr_pos);
      }
      if (wrap_pos != std::string::npos) {
        assert(wrap_pos - prev_pos != std::string::npos);
        res.push_back(indent + work.substr(prev_pos, wrap_pos - prev_pos));
        prev_pos = wrap_pos + 1;  // + 1 to skip space
      } else {
        break;
      }
    } while (str_size - prev_pos > real_width ||
             work.find("\n", prev_pos) != std::string::npos);
    res.push_back(indent + work.substr(prev_pos));
  }

  return res;
}

bool ends_with(const std::string &str, const std::string &suffix) {
  auto suffix_size = suffix.size();
  auto str_size = str.size();
  return (str_size >= suffix_size &&
          str.compare(str_size - suffix_size, str_size, suffix) == 0);
}

bool starts_with(const std::string &str, const std::string &prefix) {
  auto prefix_size = prefix.size();
  auto str_size = str.size();
  return (str_size >= prefix_size && str.compare(0, prefix_size, prefix) == 0);
}

}  // namespace mysql_harness::utility

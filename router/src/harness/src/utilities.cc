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

#include "utilities.h"

#include <string.h>
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>

using std::string;
using std::vector;

namespace mysql_harness {

namespace utility {

string dirname(const string &path) {
  string::size_type pos = path.rfind('/');
  if (pos != string::npos)
    return string(path, 0, pos);
  else
    return string(".");
}

string basename(const string &path) {
  string::size_type pos = path.rfind('/');
  if (pos != string::npos)
    return string(path, pos + 1);
  else
    return path;
}

void strip(string *str, const char *chars) {
  str->erase(str->find_last_not_of(chars) + 1);
  str->erase(0, str->find_first_not_of(chars));
}

string strip_copy(string str, const char *chars) {
  strip(&str, chars);
  return str;
}

string string_format(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_next;
  va_copy(args_next, args);

  int size = vsnprintf(nullptr, 0, format, args);
  std::vector<char> buf(static_cast<size_t>(size) + 1);
  va_end(args);

  vsnprintf(buf.data(), buf.size(), format, args_next);
  va_end(args_next);

  return string(buf.begin(), buf.end() - 1);
}

vector<string> wrap_string(const string &to_wrap, size_t width,
                           size_t indent_size) {
  size_t curr_pos = 0;
  size_t wrap_pos = 0;
  size_t prev_pos = 0;
  string work{to_wrap};
  vector<string> res{};
  auto indent = string(indent_size, ' ');
  auto real_width = width - indent_size;

  size_t str_size = work.size();
  if (str_size < real_width) {
    res.push_back(indent + work);
  } else {
    work.erase(std::remove(work.begin(), work.end(), '\r'), work.end());
    std::replace(work.begin(), work.end(), '\t', ' '), work.end();
    str_size = work.size();

    do {
      curr_pos = prev_pos + real_width;

      // respect forcing newline
      wrap_pos = work.find("\n", prev_pos);
      if (wrap_pos == string::npos || wrap_pos > curr_pos) {
        // No new line found till real_width
        wrap_pos = work.find_last_of(" ", curr_pos);
      }
      if (wrap_pos != string::npos) {
        assert(wrap_pos - prev_pos != string::npos);
        res.push_back(indent + work.substr(prev_pos, wrap_pos - prev_pos));
        prev_pos = wrap_pos + 1;  // + 1 to skip space
      } else {
        break;
      }
    } while (str_size - prev_pos > real_width ||
             work.find("\n", prev_pos) != string::npos);
    res.push_back(indent + work.substr(prev_pos));
  }

  return res;
}

#ifndef _WIN32
std::string get_message_error(int errcode) {
  return std::string(strerror(errcode));
}
#endif

}  // namespace utility

}  // namespace mysql_harness

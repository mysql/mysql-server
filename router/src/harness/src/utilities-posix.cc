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

#include "utilities.h"

#include <fnmatch.h>
#include <regex.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

namespace mysql_harness {

namespace utility {

bool matches_glob(const std::string &word, const std::string &pattern) {
  return (fnmatch(pattern.c_str(), word.c_str(), 0) == 0);
}

void sleep_seconds(unsigned int seconds) { sleep(seconds); }

bool regex_pattern_matches(const std::string &s, const std::string &pattern) {
  regex_t regex;
  auto r = regcomp(&regex, pattern.c_str(), REG_EXTENDED);
  if (r) {
    throw std::runtime_error("Error compiling regex pattern: " + pattern);
  }
  r = regexec(&regex, s.c_str(), 0, nullptr, 0);
  regfree(&regex);
  return (r == 0);
}

}  // namespace utility

}  // namespace mysql_harness

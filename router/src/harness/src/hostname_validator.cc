/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "hostname_validator.h"
#include "harness_assert.h"

#ifdef _WIN32
#define USE_STD_REGEX  // flag that C++11 regex is fully supported
#endif

#ifdef USE_STD_REGEX
#include <regex>
#else
#include <regex.h>
#endif

namespace mysql_harness {

bool is_valid_hostname(const char *hostname) {
  // WARNING! This is minimalistic validation, it doesn't catch all cornercases.
  //          Please see notes in Doxygen function description.

  const char re_text[] = "^[-._a-z0-9]+$";
  bool is_valid;
#ifdef USE_STD_REGEX
  is_valid = std::regex_match(hostname, std::regex(re_text, std::regex::icase));
#else
  regex_t re;
  harness_assert(!regcomp(&re, re_text, REG_EXTENDED | REG_ICASE | REG_NOSUB));
  is_valid = !regexec(&re, hostname, 0, nullptr, 0);
  regfree(&re);
#endif

  return is_valid;
}

}  // namespace mysql_harness

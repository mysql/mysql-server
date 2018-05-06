/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/x/src/xpl_regex.h"

#include <cstring>
#include "include/my_dbug.h"

namespace xpl {
namespace {

inline void check_result(const int err MY_ATTRIBUTE((unused))) {
  DBUG_ASSERT(err == 0);
}

const struct Regex_finalizer {
  Regex_finalizer() {}
  ~Regex_finalizer() { my_regex_end(); }
} regex_finalizer;

}  // namespace

Regex::Regex(const char *const pattern) {
  memset(&m_re, 0, sizeof(m_re));
  int err = my_regcomp(&m_re, pattern,
                       (MY_REG_EXTENDED | MY_REG_ICASE | MY_REG_NOSUB),
                       &my_charset_utf8mb4_general_ci);
  // Workaround for unused variable
  check_result(err);
}

Regex::~Regex() { my_regfree(&m_re); }

bool xpl::Regex::match(const char *value) const {
  return my_regexec(&m_re, value, static_cast<size_t>(0), nullptr, 0) == 0;
}

}  // namespace xpl

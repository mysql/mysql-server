/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>

#include "include/my_dbug.h"

namespace xpl {

Regex::Regex(const char *const pattern)
    : m_status{U_ZERO_ERROR},
      m_pattern{icu::RegexPattern::compile(
          icu::UnicodeString::fromUTF8(pattern), UREGEX_CASE_INSENSITIVE,
          m_parse_error, m_status)} {
  DBUG_ASSERT(U_SUCCESS(m_status));
}

bool xpl::Regex::match(const char *value) const {
  if (!U_SUCCESS(m_status)) return false;

  UErrorCode match_status{U_ZERO_ERROR};

  /* Initializing RegexMatcher object with RegexPattern
   * can by done only by calling an RegexPattern method
   * that returns RegexMatcher pointer.
   *
   * RegexMatcher is not reentrant thus we need create an
   * instance per thread or like in current solution,
   * instance per xpl::Regex::match call.
   *
   * Other possibility would be to create RegexMatcher on stack
   * and parse the text patter each time that xpl::Regex::match
   * is called.
   */
  std::unique_ptr<icu::RegexMatcher> regexp{
      m_pattern->matcher(icu::UnicodeString::fromUTF8(value), match_status)};

  if (!U_SUCCESS(match_status)) return false;

  return regexp->find();
}

}  // namespace xpl

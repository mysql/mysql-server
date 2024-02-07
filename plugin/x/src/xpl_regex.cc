/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/x/src/xpl_regex.h"

#include <assert.h>
#include <memory>

namespace xpl {

Regex::Regex(const char *const pattern)
    : m_status{U_ZERO_ERROR},
      m_pattern{icu::RegexPattern::compile(
          icu::UnicodeString::fromUTF8(pattern), UREGEX_CASE_INSENSITIVE,
          m_parse_error, m_status)} {
  assert(U_SUCCESS(m_status));
}

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

bool Regex::match(const char *value) const {
  if (!U_SUCCESS(m_status)) return false;

  UErrorCode match_status{U_ZERO_ERROR};
  icu::UnicodeString value_as_utf8{icu::UnicodeString::fromUTF8(value)};
  std::unique_ptr<icu::RegexMatcher> regexp{
      m_pattern->matcher(value_as_utf8, match_status)};

  if (!U_SUCCESS(match_status)) return false;

  return regexp->matches(match_status);
}

bool Regex::match_groups(const char *value, Group_list *groups,
                         const bool skip_empty_group) const {
  UErrorCode status{m_status};
  icu::UnicodeString value_from_utf8{icu::UnicodeString::fromUTF8(value)};
  std::unique_ptr<icu::RegexMatcher> matcher{
      m_pattern->matcher(value_from_utf8, status)};
  if (!matcher || !matcher->matches(status)) return false;

  std::string tmp;
  for (int32_t g = 0; g <= matcher->groupCount(); ++g) {
    matcher->group(g, status).toUTF8String(tmp);
    if (U_FAILURE(status)) return false;
    if (skip_empty_group && tmp.empty()) continue;
    groups->push_back(tmp);
    tmp.clear();
  }
  return true;
}

}  // namespace xpl

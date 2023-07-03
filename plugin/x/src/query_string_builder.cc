/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/query_string_builder.h"

#include <assert.h>
#include <cstdint>

#include <mutex>  // NOLINT(build/c++11)

#include "my_sys.h"  // escape_string_for_mysql NOLINT(build/include_subdir)
#include "mysql/plugin.h"

namespace xpl {

CHARSET_INFO *Query_string_builder::m_charset = nullptr;
std::once_flag Query_string_builder::m_charset_initialized;

void Query_string_builder::init_charset() {
  m_charset = get_charset_by_csname("utf8mb4", MY_CS_PRIMARY, MYF(MY_WME));
}

Query_string_builder::Query_string_builder(size_t reserve)
    : m_in_quoted(false), m_in_identifier(false) {
  std::call_once(m_charset_initialized, init_charset);
  assert(m_charset != nullptr);

  m_str.reserve(reserve);
}

Query_string_builder &Query_string_builder::quote_identifier(const char *s,
                                                             size_t length) {
  m_str.append("`");
  escape_identifier(s, length);
  m_str.append("`");
  return *this;
}

Query_string_builder &Query_string_builder::quote_identifier_if_needed(
    const char *s, size_t length) {
  bool need_quote = false;
  if (length > 0 && isalpha(s[0])) {
    for (size_t i = 1; i < length; i++)
      if (!isalnum(s[i]) && s[i] != '_') {
        need_quote = true;
        break;
      }
  } else {
    need_quote = true;
  }
  if (need_quote)
    return quote_identifier(s, length);
  else
    return put(s, length);
}

namespace {
inline void escape_char(const char *s, const size_t length, const char escape,
                        ngs::PFS_string *buff) {
  size_t str_pos = buff->size();
  // resize the buffer to fit the original size + worst case length of s
  buff->resize(str_pos + length * 2);

  char *cursor_out = &(*buff)[str_pos];
  const char *cursor_in = s;

  for (size_t idx = 0; idx < length; ++idx) {
    if (*cursor_in == escape) *cursor_out++ = escape;
    *cursor_out++ = *cursor_in++;
  }
  buff->resize(str_pos + (cursor_out - &(*buff)[str_pos]));
}

}  // namespace

Query_string_builder &Query_string_builder::escape_identifier(const char *s,
                                                              size_t length) {
  escape_char(s, length, '`', &m_str);
  return *this;
}

Query_string_builder &Query_string_builder::escape_string(const char *s,
                                                          size_t length) {
  size_t str_pos = m_str.size();
  // resize the buffer to fit the original size + worst case length of s
  m_str.resize(str_pos + 2 * length + 1);

  size_t r = escape_string_for_mysql(m_charset, &m_str[str_pos], 2 * length + 1,
                                     s, length);
  m_str.resize(str_pos + r);

  return *this;
}

Query_string_builder &Query_string_builder::escape_json_string(const char *s,
                                                               size_t length) {
  escape_char(s, length, '\'', &m_str);
  return *this;
}

Query_string_builder &Query_string_builder::quote_string(const char *s,
                                                         size_t length) {
  m_str.append("'");
  escape_string(s, length);
  m_str.append("'");

  return *this;
}

Query_string_builder &Query_string_builder::quote_json_string(const char *s,
                                                              size_t length) {
  m_str.append("'");
  escape_json_string(s, length);
  m_str.append("'");

  return *this;
}

Query_string_builder &Query_string_builder::put(const char *s, size_t length) {
  if (m_in_quoted)
    escape_string(s, length);
  else if (m_in_identifier)
    escape_identifier(s, length);
  else
    m_str.append(s, length);

  return *this;
}

Query_formatter Query_string_builder::format() {
  return Query_formatter(m_str, *m_charset);
}

}  // namespace xpl

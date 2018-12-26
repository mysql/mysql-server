/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#include "query_string_builder.h"
#include "mysql/plugin.h"
#include "my_sys.h" // escape_string_for_mysql

using namespace xpl;

charset_info_st* Query_string_builder::m_charset = NULL;
my_thread_once_t Query_string_builder::m_charset_initialized = MY_THREAD_ONCE_INIT;

void Query_string_builder::init_charset()
{
  m_charset = get_charset_by_csname("utf8mb4", MY_CS_PRIMARY, MYF(MY_WME));
}


Query_string_builder::Query_string_builder(size_t reserve)
  : m_in_quoted(false), m_in_identifier(false)
{
  my_thread_once(&m_charset_initialized, init_charset);
  DBUG_ASSERT(m_charset != NULL);

  m_str.reserve(reserve);
}


Query_string_builder::~Query_string_builder()
{
}


Query_string_builder &Query_string_builder::quote_identifier(const char *s, size_t length)
{
  m_str.append("`");
  escape_identifier(s, length);
  m_str.append("`");
  return *this;
}


Query_string_builder &Query_string_builder::quote_identifier_if_needed(const char *s, size_t length)
{
  bool need_quote = false;
  if (length > 0 && isalpha(s[0]))
  {
    for (size_t i = 1; i < length; i++)
      if (!isalnum(s[i]) && s[i] != '_')
      {
        need_quote = true;
        break;
      }
  }
  else
    need_quote = true;

  if (need_quote)
    return quote_identifier(s, length);
  else
    return put(s, length);
}


Query_string_builder &Query_string_builder::escape_identifier(const char *s, size_t length)
{
  size_t str_pos = m_str.size();
  // resize the buffer to fit the original size + worst case length of s
  m_str.resize(str_pos + length*2);

  char* cursor_out = &m_str[str_pos];
  const char* cursor_in = s;

  for (size_t idx = 0; idx < length; ++idx)
  {
    if (*cursor_in == '`')
      *cursor_out++ = '`';
    *cursor_out++ = *cursor_in++;
  }
  m_str.resize(str_pos + (cursor_out - &m_str[str_pos]));
  return *this;
}


Query_string_builder &Query_string_builder::escape_string(const char *s, size_t length)
{
  size_t str_pos = m_str.size();
  // resize the buffer to fit the original size + worst case length of s
  m_str.resize(str_pos + 2*length+1);

  size_t r = escape_string_for_mysql(m_charset, &m_str[str_pos], 2*length+1, s, length);
  m_str.resize(str_pos + r);

  return *this;
}


Query_string_builder &Query_string_builder::quote_string(const char *s, size_t length)
{
  m_str.append("'");
  escape_string(s, length);
  m_str.append("'");

  return *this;
}


Query_string_builder &Query_string_builder::put(const char *s, size_t length)
{
  if (m_in_quoted)
    escape_string(s, length);
  else if (m_in_identifier)
    escape_identifier(s, length);
  else
    m_str.append(s, length);

  return *this;
}


Query_formatter Query_string_builder::format()
{
  return Query_formatter(m_str, *m_charset);
}

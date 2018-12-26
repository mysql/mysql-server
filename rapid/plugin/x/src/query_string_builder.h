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

#ifndef _QUERY_STRING_BUILDER_H_
#define _QUERY_STRING_BUILDER_H_

#include <string>
#include <string.h>
#include <stdint.h>

#include <ngs/thread.h>
#include <ngs/memory.h>

#include <query_formatter.h>

struct charset_info_st;

namespace xpl
{

class Query_string_builder
{
public:
  Query_string_builder(size_t reserve = 256);
  ~Query_string_builder();

  Query_string_builder &bquote()
  {
    m_str.push_back('\'');
    m_in_quoted = true;
    return *this;
  }

  Query_string_builder &equote()
  {
    m_str.push_back('\'');
    m_in_quoted = false;
    return *this;
  }

  Query_string_builder &bident()
  {
    m_str.push_back('`');
    m_in_identifier = true;
    return *this;
  }

  Query_string_builder &eident()
  {
    m_str.push_back('`');
    m_in_identifier = false;
    return *this;
  }

  Query_string_builder &quote_identifier_if_needed(const char *s, size_t length);
  Query_string_builder &quote_identifier(const char *s, size_t length);
  Query_string_builder &quote_string(const char *s, size_t length);

  Query_string_builder &quote_identifier_if_needed(const std::string &s)
  {
    return quote_identifier_if_needed(s.data(), s.length());
  }

  Query_string_builder &quote_identifier(const std::string &s)
  {
    return quote_identifier(s.data(), s.length());
  }

  Query_string_builder &quote_string(const std::string &s)
  {
    return quote_string(s.data(), s.length());
  }

  Query_string_builder &escape_identifier(const char *s, size_t length);
  Query_string_builder &escape_string(const char *s, size_t length);

  Query_string_builder &dot() { return put(".", 1); }

  Query_string_builder &put(const int64_t i) { return put(ngs::to_string(i)); }
  Query_string_builder &put(const uint64_t u) { return put(ngs::to_string(u)); }

//  NOTE: Commented for coverage. Uncomment when needed.
//  Query_string_builder &put(const int32_t i) { return put(ngs::to_string(i)); }

  Query_string_builder &put(const uint32_t u) { return put(ngs::to_string(u)); }
  Query_string_builder &put(const float f) { return put(ngs::to_string(f)); }
  Query_string_builder &put(const double d) { return put(ngs::to_string(d)); }

  Query_string_builder &put(const char *s, size_t length);

  Query_formatter format();

  Query_string_builder &put(const char *s) { return put(s, strlen(s)); }

  Query_string_builder &put(const std::string &s)
  {
    return put(s.data(), s.length());
  }

  Query_string_builder &put(const ngs::PFS_string &s)
  {
    return put(s.data(), s.length());
  }

  void clear()
  {
    m_str.clear();
  }

  void reserve(size_t bytes)
  {
    m_str.reserve(bytes);
  }

  const ngs::PFS_string &get() const { return m_str; }

private:
  ngs::PFS_string m_str;
  bool m_in_quoted;
  bool m_in_identifier;

  static void init_charset();
  static my_thread_once_t  m_charset_initialized;
  static charset_info_st *m_charset;
};

} // namespace xpl

#endif

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

#ifndef _QUERY_FORMATTER_H_
#define _QUERY_FORMATTER_H_


#include <string>
#include <string.h>
#include <stdint.h>
#include <stdexcept>
#include <sstream>
#include "ngs/memory.h"
#include "ngs_common/to_string.h"


struct charset_info_st;

namespace xpl
{

  class Query_formatter
  {
  public:
    Query_formatter(ngs::PFS_string &query, charset_info_st &charser);

    template <typename Value_type>
    class No_escape
    {
    public:
      No_escape(const Value_type &value)
        : m_value(value)
      {
      }

      const Value_type &m_value;
    };

    Query_formatter &operator % (const char *value);
    Query_formatter &operator % (const No_escape<const char *> &value);
    Query_formatter &operator % (const std::string &value);
    Query_formatter &operator % (const No_escape<std::string> &value);


    template<typename Value_type>
    Query_formatter &operator % (const Value_type &value)
    {
      return put(value);
    }

  private:
    template<typename Value_type>
    Query_formatter & put(const Value_type &value)
    {
      validate_next_tag();
      std::string string_value = ngs::to_string(value);
      put_value(string_value.c_str(), string_value.length());

      return *this;
    }

    template<typename Value_type>
    Query_formatter & put_fp(const Value_type &value)
    {
      std::stringstream stream;
      validate_next_tag();
      stream << value;
      std::string string_value = stream.str();
      put_value(string_value.c_str(), string_value.length());

      return *this;
    }

    void put_value(const char *value, const std::size_t length);
    void put_value_and_escape(const char *value, const std::size_t length);
    void validate_next_tag();

    ngs::PFS_string      &m_query;
    charset_info_st &m_charset;
    std::size_t      m_last_tag_position;
  };

  template<>
  inline  Query_formatter& Query_formatter::operator%<double>(const double &value)
  {
    return put_fp(value);
  }

  template<>
  inline  Query_formatter& Query_formatter::operator%<float>(const float &value)
  {
    return put_fp(value);
  }

} // namespace xpl


#endif // _QUERY_FORMATTER_H_

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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef NGS_STRING_FORMATTER_H_
#define NGS_STRING_FORMATTER_H_

#include "ngs/memory.h"
#include "my_global.h"
#include "violite.h"
#include <sstream>
#include <vector>


namespace ngs {

class String_formatter {
public:
  template<typename Element_type, typename Separator_type>
  String_formatter &join(const std::vector<Element_type> &elements_array, const Separator_type &separator) {
    if (elements_array.empty())
      return *this;

    const uint32 num_elements_without_last = elements_array.size() - 1;
    uint32 index_of_element  = 0;

    while(index_of_element < num_elements_without_last) {
      m_stream << elements_array[index_of_element] << separator;
      ++index_of_element;
    }

    m_stream << elements_array[index_of_element];

    return *this;
  }

  template<typename Value_type>
  String_formatter &append(const Value_type &value) {
    m_stream << value;

    return *this;
  }

  std::string get_result() {
    return m_stream.str();
  }

private:
  std::stringstream m_stream;
};

template<typename Element_type, typename Separator_type>
std::string join(const std::vector<Element_type> &elements_array, const Separator_type &separator) {

  return String_formatter().join(elements_array, separator).get_result();
}

} // namespace ngs

#endif // NGS_STRING_FORMATTER_H_

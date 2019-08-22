/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_SET_VARIABLE_ADAPTOR_H_
#define PLUGIN_X_SRC_CAPABILITIES_SET_VARIABLE_ADAPTOR_H_

#include <cassert>
#include <string>
#include <vector>

#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/set_variable.h"

#include "my_dbug.h"  // NOLINT(build/include_subdir)

namespace xpl {

template <typename Enum>
class Set_variable_adaptor {
 public:
  Set_variable_adaptor(const Set_variable &variable,
                       const std::initializer_list<Enum> &label_map)
      : m_variable{variable}, m_label_map{label_map} {
    DBUG_ASSERT(m_variable.get_labels().size() - 1 == label_map.size());
  }

  bool is_allowed_value(const std::string &val) const {
    const auto id = get_id(val);
    return id < m_variable.get_labels().size() - 1
               ? m_variable.get_value() & (static_cast<ulonglong>(1) << id)
               : false;
  }

  void get_allowed_values(std::vector<std::string> *values) const {
    values->clear();
    for (ulonglong i = 0; i < m_variable.get_labels().size() - 1; ++i) {
      if (m_variable.get_value() & (static_cast<ulonglong>(1) << i))
        values->push_back(to_lower(m_variable.get_labels()[i]));
    }
  }

  Enum get_value(const std::string &val) const {
    return m_label_map[get_id(val)];
  }

 private:
  std::vector<const char *>::size_type get_id(const std::string &val) const {
    const auto label = to_upper(val);
    const auto i = std::find_if(
        m_variable.get_labels().begin(), m_variable.get_labels().end() - 1,
        [&label](const char *l) { return std::strcmp(label.c_str(), l) == 0; });
    return std::distance(m_variable.get_labels().begin(), i);
  }

  const Set_variable &m_variable;
  const std::vector<Enum> m_label_map;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_SET_VARIABLE_ADAPTOR_H_

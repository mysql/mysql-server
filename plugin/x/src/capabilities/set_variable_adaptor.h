/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include <assert.h>
#include <cassert>
#include <string>
#include <vector>

#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/variables/set_variable.h"

namespace xpl {

template <typename Enum>
class Set_variable_adaptor {
 public:
  Set_variable_adaptor(const Set_variable &variable,
                       const std::initializer_list<Enum> &label_map)
      : m_variable{variable}, m_label_map{label_map} {
    assert(m_variable.get_labels_count() == label_map.size());
  }

  bool is_allowed_value(const std::string &val) const {
    const auto id = get_id(val);
    return id < m_variable.get_labels_count()
               ? m_variable.get_value() & (static_cast<ulonglong>(1) << id)
               : false;
  }

  void get_allowed_values(std::vector<std::string> *values) const {
    values->clear();
    const auto &value = m_variable.get_value();
    std::vector<std::string> labels;
    m_variable.get_labels(&labels);
    for (ulonglong i = 0; i < labels.size(); ++i) {
      if (value & (static_cast<ulonglong>(1) << i))
        values->push_back(to_lower(labels[i]));
    }
  }

  Enum get_value(const std::string &val) const {
    return m_label_map[get_id(val)];
  }

 private:
  std::vector<std::string>::size_type get_id(const std::string &val) const {
    std::vector<std::string> labels;
    m_variable.get_labels(&labels);
    const auto i = std::find(labels.begin(), labels.end(), to_upper(val));
    return std::distance(labels.begin(), i);
  }

  const Set_variable &m_variable;
  const std::vector<Enum> m_label_map;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_SET_VARIABLE_ADAPTOR_H_

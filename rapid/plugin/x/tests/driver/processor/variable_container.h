/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_
#define X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_

#include <list>
#include <map>
#include <string>

#include "common/utils_string_parsing.h"

class Variable_container {
 public:
  using Container = std::map<std::string, std::string>;

 public:
  Variable_container() = default;
  explicit Variable_container(const Container &variables)
      : m_variables(variables) {}

  void replace(std::string *s) {
    for (std::map<std::string, std::string>::const_iterator sub =
             m_variables.begin();
         sub != m_variables.end(); ++sub) {
      std::string tmp(sub->second);

      aux::replace_all(*s, sub->first, tmp);
    }
  }

  void set(const std::string &key, const std::string &value) {
    m_variables[key] = value;
  }

  std::string get(const std::string &key) const {
    if (!is_present(key))
      return "";

    return m_variables.at(key);
  }

  bool is_present(const std::string &key) const {
    return m_variables.count(key);
  }

  std::string unreplace(const std::string &in, bool clear) {
    std::string s = in;
    for (std::list<std::string>::const_iterator sub = m_to_unreplace.begin();
         sub != m_to_unreplace.end(); ++sub) {
      aux::replace_all(s, m_variables[*sub], *sub);
    }
    if (clear) m_to_unreplace.clear();
    return s;
  }

  void clear_unreplace() { m_to_unreplace.clear(); }
  void push_unreplace(const std::string &value) {
    m_to_unreplace.push_back(value);
  }

 private:
  Container m_variables;
  std::list<std::string> m_to_unreplace;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_

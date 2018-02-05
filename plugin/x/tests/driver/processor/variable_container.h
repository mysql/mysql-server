/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_
#define X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "plugin/x/tests/driver/common/utils_string_parsing.h"
#include "plugin/x/tests/driver/processor/variable.h"

class Variable_container {
 public:
  using Container_initialization = std::map<std::string, std::string>;

  Variable_container() = default;
  explicit Variable_container(const Container_initialization &variables) {
    for (const auto &kv : variables) {
      set(kv.first, kv.second);
    }
  }

  void replace(std::string *s) {
    for (Container::const_iterator sub = m_variables.begin();
         sub != m_variables.end(); ++sub) {
      std::string tmp(sub->second->get_value());

      aux::replace_all(*s, sub->first, tmp);
    }
  }

  void make_special_variable(const std::string &key,
                             Variable_interface *value) {
    m_variables[key].reset(value);
  }

  bool set(const std::string &key, const std::string &value) {
    auto &variable = m_variables[key];

    if (!variable) variable.reset(new Variable_string());

    return variable->set_value(value);
  }

  std::string get(const std::string &key) const {
    if (!is_present(key)) return "";

    return m_variables.at(key)->get_value();
  }

  bool is_present(const std::string &key) const {
    return m_variables.count(key);
  }

  std::string unreplace(const std::string &in, bool clear) {
    std::string s = in;
    for (std::list<std::string>::const_iterator sub = m_to_unreplace.begin();
         sub != m_to_unreplace.end(); ++sub) {
      aux::replace_all(s, m_variables[*sub]->get_value(), *sub);
    }
    if (clear) m_to_unreplace.clear();
    return s;
  }

  void clear_unreplace() { m_to_unreplace.clear(); }
  void push_unreplace(const std::string &value) {
    m_to_unreplace.push_back(value);
  }

 private:
  using Container = std::map<std::string, std::unique_ptr<Variable_interface>>;

  Container m_variables;
  std::list<std::string> m_to_unreplace;
};

#endif  // X_TESTS_DRIVER_PROCESSOR_VARIABLE_CONTAINER_H_

/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_ROW_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_ROW_H_

#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_object_bridge.h"

namespace shcore {
namespace polyglot {

class Row : public Object_bridge {
 public:
  Row(const std::vector<std::string> &names, const jit_executor::db::IRow &row);

  std::string class_name() const override { return "Row"; }

  shcore::Value get_field(const std::string &field) const;

  bool has_member(const std::string &prop) const override;
  shcore::Value get_member(const std::string &prop) const override;
  shcore::Value get_member(size_t index) const override;

  size_t length() const override { return m_value_array.size(); }
  bool is_indexed() const override { return true; }

  shcore::Dictionary_t as_object();

  Value call(const std::string &name, const Argument_list &args) override;

 private:
  const std::vector<std::string> *methods() const override {
    return &m_methods;
  }
  const std::vector<std::string> *properties() const override {
    return &m_names;
  }

  static std::vector<std::string> m_methods;
  std::vector<std::string> m_names;
  std::vector<shcore::Value> m_value_array;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_ROW_H_
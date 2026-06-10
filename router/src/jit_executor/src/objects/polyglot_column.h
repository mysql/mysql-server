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

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_COLUMN_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_COLUMN_H_

#include <memory>
#include <string>

#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_object_bridge.h"

namespace shcore {
namespace polyglot {

class Column : public Object_bridge {
 public:
  Column(const std::shared_ptr<jit_executor::db::IColumn> &meta,
         const shcore::Value &type);

  std::string class_name() const override { return "Column"; }

  using Object_bridge::get_member;
  shcore::Value get_member(const std::string &prop) const override;

  const std::string &get_schema_name() const { return m_column->get_schema(); }

  const std::string &get_table_name() const {
    return m_column->get_table_name();
  }

  const std::string &get_table_label() const {
    return m_column->get_table_label();
  }

  const std::string &get_column_name() const {
    return m_column->get_column_name();
  }

  const std::string &get_column_label() const {
    return m_column->get_column_label();
  }

  shcore::Value get_type() const { return m_type; }

  uint32_t get_length() const { return m_column->get_length(); }

  int get_fractional_digits() const { return m_column->get_fractional(); }

  bool is_number_signed() const {
    return m_column->is_numeric() ? !m_column->is_unsigned() : false;
  }

  //   std::string get_collation_name() const { return
  //   m_column->get_collation_name(); }

  //   std::string get_character_set_name() const { return
  //   m_column->get_charset_name(); }

  bool is_zerofill() const { return m_column->is_zerofill(); }

  std::string get_flags() const { return m_column->get_flags(); }

  bool is_binary() const { return m_column->is_binary(); }
  bool is_numeric() const { return m_column->is_numeric(); }

 private:
  std::vector<std::string> *properties() const override {
    return &m_properties;
  }

  static std::vector<std::string> m_properties;
  std::shared_ptr<jit_executor::db::IColumn> m_column;
  shcore::Value m_type;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_COLUMN_H_
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

#include "objects/polyglot_column.h"

#include <memory>
#include <string>

#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_object_bridge.h"

namespace shcore {
namespace polyglot {
namespace {
const constexpr char *k_schema_name = "schemaName";
const constexpr char *k_table_name = "tableName";
const constexpr char *k_table_label = "tableLabel";
const constexpr char *k_column_name = "columnName";
const constexpr char *k_column_label = "columnLabel";
const constexpr char *k_type = "type";
const constexpr char *k_length = "length";
const constexpr char *k_fractional_digits = "fractionalDigits";
const constexpr char *k_number_signed = "numberSigned";
// TODO(rennox): add character set support
//  const constexpr char *k_collation_name = "collationName";
//  const constexpr char *k_character_set_name = "characterSetName";
const constexpr char *k_zero_fill = "zeroFill";
const constexpr char *k_flags = "flags";
}  // namespace

std::vector<std::string> Column::m_properties = {
    k_schema_name,   k_table_name, k_table_label, k_column_name,
    k_column_label,  k_type,       k_length,      k_fractional_digits,
    k_number_signed, k_zero_fill,  k_flags};

Column::Column(const std::shared_ptr<jit_executor::db::IColumn> &meta,
               const shcore::Value &type)
    : m_column{meta}, m_type{type} {}

shcore::Value Column::get_member(const std::string &prop) const {
  if (prop == k_schema_name) return shcore::Value(m_column->get_schema());
  if (prop == k_table_name) return shcore::Value(m_column->get_table_name());
  if (prop == k_table_label) return shcore::Value(m_column->get_table_label());
  if (prop == k_column_name) return shcore::Value(m_column->get_column_name());
  if (prop == k_column_label)
    return shcore::Value(m_column->get_column_label());
  if (prop == k_type) return m_type;
  if (prop == k_length) return shcore::Value(m_column->get_length());
  if (prop == k_fractional_digits)
    return shcore::Value(m_column->get_fractional());
  if (prop == k_number_signed) return shcore::Value(is_number_signed());
  // TODO(rennox): Add support for collation
  // if (prop == k_collation_name) return
  // shcore::Value(m_column->get_collation_name()); if (prop ==
  // k_character_set_name) return shcore::Value(m_column->get_charset_name());
  if (prop == k_zero_fill) return shcore::Value(m_column->is_zerofill());
  if (prop == k_flags) return shcore::Value(m_column->get_flags());
  return Object_bridge::get_member(prop);
}

}  // namespace polyglot
}  // namespace shcore

/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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
#include "objects/polyglot_result.h"

#include <iostream>
#include <string>
#include <vector>

#include "database/column.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "objects/polyglot_column.h"

namespace shcore {
namespace polyglot {
namespace {
const constexpr char *k_fetch_one = "fetchOne";
const constexpr char *k_fetch_all = "fetchAll";
const constexpr char *k_fetch_one_object = "fetchOneObject";
const constexpr char *k_next_result = "nextResult";
}  // namespace

std::vector<std::string> PolyResult::m_methods = {
    k_fetch_one, k_fetch_all, k_fetch_one_object, k_next_result};

PolyResult::PolyResult(const std::shared_ptr<jit_executor::db::IResult> &result)
    : m_result{result} {}

void PolyResult::reset_column_cache() {
  m_column_names.clear();
  m_columns.reset();
}

void PolyResult::update_column_cache() const {
  if (!m_columns) {
    m_columns = shcore::make_array();

    for (auto &column_meta : m_result->get_metadata()) {
      std::string type_name = database::type_to_dbstring(
          column_meta->get_type(), column_meta->get_length());

      // TODO (rennox): Add constant supports for type constants
      // shcore::Value data_type = mysqlsh::Constant::get_constant(
      //     get_protocol(), "Type", shcore::str_upper(type_name),
      //     shcore::Argument_list());
      m_columns->push_back(shcore::Value(
          std::make_shared<Column>(column_meta, shcore::Value(type_name))));

      m_column_names.push_back(column_meta->get_column_label());
    }
  }
}

std::vector<std::string> PolyResult::get_column_names() const {
  update_column_cache();
  return m_column_names;
}

std::shared_ptr<Row> PolyResult::fetch_one() const {
  std::shared_ptr<Row> ret_val;

  auto columns = get_column_names();
  if (m_result && !columns.empty()) {
    const jit_executor::db::IRow *row = m_result->fetch_one();
    if (row) {
      ret_val = std::make_shared<Row>(columns, *row);
    }
  }

  return ret_val;
}

shcore::Array_t PolyResult::fetch_all() const {
  auto array = shcore::make_array();

  while (auto object = fetch_one()) {
    array->push_back(shcore::Value(object));
  }

  return array;
}

shcore::Dictionary_t PolyResult::fetch_one_object() const {
  auto object = fetch_one();
  return object ? object->as_object() : nullptr;
}

bool PolyResult::next_result() {
  reset_column_cache();
  return m_result->next_resultset();
}

Value PolyResult::call(const std::string &name, const Argument_list &args) {
  if (name == k_fetch_one) return shcore::Value(fetch_one());
  if (name == k_fetch_all) return shcore::Value(fetch_all());
  if (name == k_fetch_one_object) return shcore::Value(fetch_one_object());
  if (name == k_next_result) return shcore::Value(next_result());

  return Object_bridge::call(name, args);
}

}  // namespace polyglot
}  // namespace shcore

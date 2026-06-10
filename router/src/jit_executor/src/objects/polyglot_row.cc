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

#include "objects/polyglot_row.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_exceptions.h"
#include "objects/polyglot_date.h"

namespace shcore {
namespace polyglot {

namespace {
class Invalid_member_exception : public Jit_executor_exception {
 public:
  Invalid_member_exception(const char *msg)
      : Jit_executor_exception("InvalidMemberException", msg) {}
};

std::vector<shcore::Value> get_row_values(const jit_executor::db::IRow &row) {
  using jit_executor::db::Type;
  // using shcore::Date;
  using shcore::Value;
  std::vector<Value> value_array;

  for (uint32_t i = 0, c = row.num_fields(); i < c; i++) {
    Value v;

    if (row.is_null(i)) {
      v = Value::Null();
    } else {
      switch (row.get_type(i)) {
        case Type::Null:
          v = Value::Null();
          break;

        case Type::String:
          v = Value(row.get_string(i));
          break;

        case Type::Integer:
          v = Value(row.get_int(i));
          break;

        case Type::UInteger:
          v = Value(row.get_uint(i));
          break;

        case Type::Float:
          v = Value(row.get_float(i));
          break;

        case Type::Double:
          v = Value(row.get_double(i));
          break;

        case Type::Decimal:
          v = Value(row.get_as_string(i));
          break;

        case Type::Date:
        case Type::DateTime:
          v = Value(std::make_shared<Date>(Date::unrepr(row.get_string(i))));
          break;

        case Type::Time:
          v = Value(std::make_shared<Date>(Date::unrepr(row.get_string(i))));
          break;

        case Type::Bit:
          v = Value(std::get<0>(row.get_bit(i)));
          break;

        case Type::Bytes:
        case Type::Vector:
        case Type::Geometry:
          v = Value(row.get_string(i), true);
          break;

        case Type::Json:
        case Type::Enum:
        case Type::Set:
          v = Value(row.get_string(i));
          break;
      }
    }

    value_array.emplace_back(std::move(v));
  }

  return value_array;
}

const constexpr char *k_get_field = "getField";
const constexpr char *k_length = "length";
}  // namespace

std::vector<std::string> Row::m_methods = {k_get_field};

Row::Row(const std::vector<std::string> &names,
         const jit_executor::db::IRow &row)
    : m_names(names) {
  assert(row.num_fields() == m_names.size());

  m_value_array = get_row_values(row);
}

shcore::Dictionary_t Row::as_object() {
  auto ret_val = shcore::make_dict();

  for (size_t index = 0; index < m_names.size(); index++) {
    ret_val->emplace(m_names.at(index), m_value_array.at(index));
  }

  return ret_val;
}

shcore::Value Row::get_field(const std::string &name) const {
  auto iter = std::find(m_names.begin(), m_names.end(), name);
  if (iter != m_names.end())
    return m_value_array[iter - m_names.begin()];
  else
    throw Invalid_member_exception(
        std::string("Field " + name + " does not exist").c_str());
}

bool Row::has_member(const std::string &prop) const {
  if (prop == k_length) return true;
  return Object_bridge::has_member(prop);
}

shcore::Value Row::get_member(const std::string &prop) const {
  if (prop == k_length) {
    return shcore::Value((int)m_value_array.size());
  } else {
    auto it = std::find(m_names.begin(), m_names.end(), prop);
    if (it != m_names.end()) return m_value_array[it - m_names.begin()];
  }

  return Object_bridge::get_member(prop);
}

Value Row::call(const std::string &name, const Argument_list &args) {
  if (name == k_get_field) return get_field(args[0].as_string());

  return Object_bridge::call(name, args);
}

shcore::Value Row::get_member(size_t index) const {
  if (index < m_value_array.size())
    return m_value_array[index];
  else
    return Object_bridge::get_member(index);
}

}  // namespace polyglot
}  // namespace shcore

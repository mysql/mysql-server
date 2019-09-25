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

#include "plugin/x/src/json_generator.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

namespace {

std::string escape_special_chars(std::string str) {
  for (std::size_t pos = 0; pos < str.size(); ++pos) {
    if (str[pos] == '\'' || str[pos] == '\\' || str[pos] == '\"')
      str.insert(pos++, 1, '\\');
  }
  return str;
}

}  // namespace

void Json_generator::generate(const Mysqlx::Datatypes::Object &arg) const {
  m_qb->put("{");
  generate_json_for_each(arg.fld());
  m_qb->put("}");
}

void Json_generator::generate(const Mysqlx::Datatypes::Array &arg) const {
  m_qb->put("[");
  generate_json_for_each(arg.value());
  m_qb->put("]");
}

void Json_generator::generate(
    const Mysqlx::Datatypes::Scalar::Octets &arg) const {
  switch (arg.content_type()) {
    case CT_PLAIN:
      m_qb->quote_string(arg.value());
      break;

    case CT_GEOMETRY:
      throw Error(ER_X_EXPR_BAD_TYPE_VALUE,
                  "GEOMETRY octet type is not supported in this context");

    case CT_JSON:
      m_qb->put(arg.value());
      break;

    case CT_XML:
      m_qb->quote_string(arg.value());
      break;

    default:
      throw Error(
          ER_X_EXPR_BAD_TYPE_VALUE,
          "Invalid content type for Mysqlx::Datatypes::Scalar::Octets " +
              to_string(arg.content_type()));
  }

}

void Json_generator::handle_object_field(
    const Mysqlx::Datatypes::Object::ObjectField &arg) const {
  m_qb->put("\"").put(arg.key()).put("\":");
  generate(arg.value());
}

void Json_generator::handle_string_scalar(
    const Mysqlx::Datatypes::Scalar &string_scalar) const {
  m_qb->put("\"");
  m_qb->put(escape_special_chars(string_scalar.v_string().value()));
  m_qb->put("\"");
}

void Json_generator::handle_bool_scalar(
    const Mysqlx::Datatypes::Scalar &bool_scalar) const {
  m_qb->put((bool_scalar.v_bool() ? "true" : "false"));
}

}  // namespace xpl

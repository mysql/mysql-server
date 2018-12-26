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

#include "plugin/x/tests/driver/json_to_any_handler.h"

bool Json_to_any_handler::Key(const char *str, rapidjson::SizeType length,
                              bool copy) {
  typedef ::Mysqlx::Datatypes::Object_ObjectField Field;
  Field *f = m_stack.top()->mutable_obj()->add_fld();
  f->set_key(str, length);
  m_stack.push(f->mutable_value());
  return true;
}

bool Json_to_any_handler::Null() {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_NULL);
  return true;
}

bool Json_to_any_handler::Bool(bool b) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_BOOL)->set_v_bool(b);
  return true;
}

bool Json_to_any_handler::Int(int i) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_SINT)->set_v_signed_int(i);
  return true;
}

bool Json_to_any_handler::Uint(unsigned u) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_UINT)->set_v_unsigned_int(u);
  return true;
}

bool Json_to_any_handler::Int64(int64_t i) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_SINT)->set_v_signed_int(i);
  return true;
}

bool Json_to_any_handler::Uint64(uint64_t u) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_UINT)->set_v_unsigned_int(u);
  return true;
}

bool Json_to_any_handler::Double(double d, bool) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_DOUBLE)->set_v_double(d);
  return true;
}

bool Json_to_any_handler::String(const char *str, rapidjson::SizeType length,
                                 bool) {
  get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_STRING)
      ->mutable_v_string()
      ->set_value(str, length);
  return true;
}

bool Json_to_any_handler::StartObject() {
  Any *any = m_stack.top();
  if (any->has_type() && any->type() == ::Mysqlx::Datatypes::Any_Type_ARRAY)
    m_stack.push(any->mutable_array()->add_value());
  m_stack.top()->set_type(::Mysqlx::Datatypes::Any_Type_OBJECT);
  m_stack.top()->mutable_obj();
  return true;
}

bool Json_to_any_handler::EndObject(rapidjson::SizeType /*member_count*/) {
  m_stack.pop();
  return true;
}

bool Json_to_any_handler::StartArray() {
  m_stack.top()->set_type(::Mysqlx::Datatypes::Any_Type_ARRAY);
  m_stack.top()->mutable_array();
  return true;
}

bool Json_to_any_handler::EndArray(rapidjson::SizeType /*element_count*/) {
  m_stack.pop();
  return true;
}

Json_to_any_handler::Scalar *Json_to_any_handler::get_scalar(
    Scalar::Type scalar_type) {
  Any *any = m_stack.top();
  if (any->has_type() && any->type() == ::Mysqlx::Datatypes::Any_Type_ARRAY)
    any = any->mutable_array()->add_value();
  else
    m_stack.pop();
  any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
  Scalar *s = any->mutable_scalar();
  s->set_type(scalar_type);
  return s;
}

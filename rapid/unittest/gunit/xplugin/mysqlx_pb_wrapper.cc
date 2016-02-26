/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "mysqlx_pb_wrapper.h"
#include <cctype>


namespace xpl
{

namespace test
{


Identifier::Identifier(const std::string& name, const std::string& schema_name)
{
  if (name.empty() == false)
    set_name(name);

  if (schema_name.empty() == false)
    set_schema_name(schema_name);
}


Document_path::Document_path(const Path &path)
{
  for (Path::const_iterator i = path.begin(); i != path.end(); ++i)
  {
    Mysqlx::Expr::DocumentPathItem* item = Add();

    item->set_type(i->first);
    if (isdigit(i->second[0]))
      item->set_index(atoi(i->second.c_str()));
    else
      item->set_value(i->second);
  }
}


ColumnIdentifier::ColumnIdentifier(const std::string& name,
                                   const std::string& table_name,
                                   const std::string& schema_name,
                                   const Document_path::Path* path)
{
  if (name.empty() == false)
    set_name(name);

  if (table_name.empty() == false)
    set_table_name(table_name);

  if (schema_name.empty() == false)
    set_schema_name(schema_name);

  if (path)
    *mutable_document_path() = Document_path(*path);
}


Scalar::Scalar(int value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_SINT);
  set_v_signed_int(value);
}


Scalar::Scalar(unsigned int value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_UINT);
  set_v_unsigned_int(value);
}


Scalar::Scalar(bool value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_BOOL);
  set_v_bool(value);
}


Scalar::Scalar(float value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_FLOAT);
  set_v_float(value);
}


Scalar::Scalar(double value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_DOUBLE);
  set_v_double(value);
}


Scalar::Scalar(const char* value, unsigned type)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  set_allocated_v_octets(new Scalar_Octets(value, type));
}


Scalar::Scalar(Scalar_Octets* value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  set_allocated_v_octets(value);
}


Scalar::Scalar(Scalar_String* value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_STRING);
  set_allocated_v_string(value);
}


Scalar::Scalar(Scalar::Null value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_NULL);
}


Expr::Expr()
{
}


void Expr::initialize(Mysqlx::Expr::Expr& expr, Operator* oper)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OPERATOR);
  expr.set_allocated_operator_(oper);
}


void Expr::initialize(
  Mysqlx::Expr::Expr& expr, FunctionCall* func)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_FUNC_CALL);
  expr.set_allocated_function_call(func);
}


void Expr::initialize(Mysqlx::Expr::Expr& expr, ColumnIdentifier* id)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_IDENT);
  expr.set_allocated_identifier(id);
}


void Expr::initialize(Mysqlx::Expr::Expr& expr, Object* obj)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OBJECT);
  expr.set_allocated_object(obj);
}


void Expr::initialize(Mysqlx::Expr::Expr& expr, Array* arr)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_ARRAY);
  expr.set_allocated_array(arr);
}


Expr* Expr::make_variable(const std::string& name)
{
  Expr* expr = new Expr();
  expr->set_type(Mysqlx::Expr::Expr_Type_VARIABLE);
  expr->set_variable(name);
  return expr;
}


Expr* Expr::make_placeholder(const ::google::protobuf::uint32 &value)
{
  Expr* expr = new Expr();
  expr->set_type(Mysqlx::Expr::Expr_Type_PLACEHOLDER);
  expr->set_position(value);
  return expr;
}


Mysqlx::Datatypes::Any* Expr::make_any(Mysqlx::Datatypes::Scalar* scalar)
{
  Mysqlx::Datatypes::Any* any = new Mysqlx::Datatypes::Any;
  any->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
  any->set_allocated_scalar(scalar);
  return any;
}


Object::Object(const Values &values)
{
  for (Values::const_iterator i = values.begin(); i != values.end(); ++i)
  {
    Mysqlx::Expr::Object_ObjectField* item = add_fld();
    item->set_key(i->first);
    item->set_allocated_value(i->second);
  }
}

} // namespace test
} //namespace xpl

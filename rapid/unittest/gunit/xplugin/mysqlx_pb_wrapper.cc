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
#include "ngs_common/to_string.h"


namespace xpl
{

namespace test
{


Identifier::Identifier(const std::string &name, const std::string &schema_name)
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
    Mysqlx::Expr::DocumentPathItem *item = Add();

    item->set_type(i->first);
	if (i->first == Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX)
      item->set_index(ngs::stoi(i->second));
    else
      item->set_value(i->second);
  }
}


Document_path::Path::Path(const std::string &value)
{
  add_member(value);
}


Document_path::Path &Document_path::Path::add_member(const std::string &value)
{
  push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, value));
  return *this;
}

Document_path::Path &Document_path::Path::add_index(int index)
{
  push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX,
                           ngs::to_string(index)));
  return *this;
}

Document_path::Path &Document_path::Path::add_asterisk()
{
  push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK, ""));
  return *this;
}


Document_path::Path &Document_path::Path::add_double_asterisk()
{
  push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::DOUBLE_ASTERISK, ""));
  return *this;
}


ColumnIdentifier::ColumnIdentifier(const std::string &name,
                                   const std::string &table_name,
                                   const std::string &schema_name,
                                   const Document_path::Path *path)
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


ColumnIdentifier::ColumnIdentifier(const Document_path &path,
                                   const std::string &name,
                                   const std::string &table_name,
                                   const std::string &schema_name)
{
  if (!name.empty())
    set_name(name);

  if (!table_name.empty())
    set_table_name(table_name);

  if (!schema_name.empty())
    set_schema_name(schema_name);

  mutable_document_path()->CopyFrom(path);
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


Scalar::Scalar(const char *value, unsigned type)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  set_allocated_v_octets(new Scalar::Octets(value, type));
}


Scalar::Scalar(Scalar::Octets *value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  set_allocated_v_octets(value);
}


Scalar::Scalar(const Scalar::Octets &value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_OCTETS);
  mutable_v_octets()->CopyFrom(value);
}


Scalar::Scalar(Scalar::String *value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_STRING);
  set_allocated_v_string(value);
}


Scalar::Scalar(const Scalar::String &value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_STRING);
  mutable_v_string()->CopyFrom(value);
}


Scalar::Scalar(Scalar::Null value)
{
  set_type(Mysqlx::Datatypes::Scalar_Type_V_NULL);
}


Scalar::String::String(const std::string &value)
{
  set_value(value);
}


Scalar::Octets::Octets(const std::string &value, unsigned type)
{
  set_value(value);
  set_content_type(type);
}


Any::Any(Scalar *scalar)
{
  set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
  set_allocated_scalar(scalar);
}


Any::Any(const Scalar &scalar)
{
  set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
  mutable_scalar()->CopyFrom(scalar);
}


Any::Any(const Object &obj)
{
  set_type(Mysqlx::Datatypes::Any_Type_OBJECT);
  mutable_obj()->CopyFrom(obj);
}


Any::Any(const Array &array)
{
  set_type(Mysqlx::Datatypes::Any_Type_ARRAY);
  mutable_array()->CopyFrom(array);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Scalar &value)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_LITERAL);
  expr.mutable_literal()->CopyFrom(value);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, Operator *oper)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OPERATOR);
  expr.set_allocated_operator_(oper);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Operator &oper)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OPERATOR);
  expr.mutable_operator_()->CopyFrom(oper);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Identifier &ident)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_IDENT);
  expr.mutable_operator_()->CopyFrom(ident);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, FunctionCall *func)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_FUNC_CALL);
  expr.set_allocated_function_call(func);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const FunctionCall &func)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_FUNC_CALL);
  expr.mutable_function_call()->CopyFrom(func);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, ColumnIdentifier *id)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_IDENT);
  expr.set_allocated_identifier(id);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const ColumnIdentifier &id)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_IDENT);
  expr.mutable_identifier()->CopyFrom(id);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, Object *obj)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OBJECT);
  expr.set_allocated_object(obj);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Object &obj)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_OBJECT);
  expr.mutable_object()->CopyFrom(obj);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, Array *arr)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_ARRAY);
  expr.set_allocated_array(arr);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Array &arr)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_ARRAY);
  expr.mutable_array()->CopyFrom(arr);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Placeholder &ph)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_PLACEHOLDER);
  expr.set_position(ph.value);
}


void Expr::initialize(Mysqlx::Expr::Expr &expr, const Variable &var)
{
  expr.set_type(Mysqlx::Expr::Expr_Type_VARIABLE);
  expr.set_variable(var.value);
}


Object::Object(const Values &values)
{
  for (Values::const_iterator i = values.begin(); i != values.end(); ++i)
  {
    ::Mysqlx::Expr::Object_ObjectField *item = add_fld();
    item->set_key(i->first);
    item->mutable_value()->CopyFrom(i->second);
  }
}


Any::Array::Scalar_values &Any::Array::Scalar_values::operator () (const Scalar &value)
{
  push_back(value);
  return *this;
}


Any::Array::Array(const Scalar &value)
{
  this->operator()(value);
}


Any::Array::Array(const Object &value)
{
  this->operator()(value);
}


Any::Array::Array(const Scalar_values &values)
{
  for (Scalar_values::const_iterator i = values.begin(); i != values.end(); ++i)
    this->operator()(*i);
}


Any::Array &Any::Array::operator () (const Scalar &value)
{
  ::Mysqlx::Datatypes::Any *any = add_value();
  any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
  any->mutable_scalar()->CopyFrom(value);
  return *this;
}


Any::Array &Any::Array::operator () (const Object &value)
{
  ::Mysqlx::Datatypes::Any *any = add_value();
  any->set_type(::Mysqlx::Datatypes::Any_Type_OBJECT);
  any->mutable_obj()->CopyFrom(value);
  return *this;
}


Any::Object::Scalar_fields::Scalar_fields(const std::string &key, const Scalar &value)
{
  (*this)[key] = value;
}


Any::Object::Scalar_fields &Any::Object::Scalar_fields::operator () (const std::string &key, const Scalar &value)
{
  (*this)[key] = value;
  return *this;
}


Any::Object::Fields::Fields(const std::string &key, const Any &value)
{
  (*this)[key] = value;
}


Any::Object::Fields &Any::Object::Fields::operator () (const std::string &key, const Any &value)
{
  (*this)[key] = value;
  return *this;
}


Any::Object::Object(const Scalar_fields &values)
{
  for (Scalar_fields::const_iterator i = values.begin(); i != values.end(); ++i)
  {
    Mysqlx::Datatypes::Object_ObjectField *item = add_fld();
    item->set_key(i->first);
    item->set_allocated_value(new Any(i->second));
  }
}


Object::Values::Values(const std::string &key, const Expr &value)
{
  (*this)[key] = value;
}


Object::Values &Object::Values::operator() (const std::string &key, const Expr &value)
{
  (*this)[key] = value;
  return *this;
}


Any::Object::Object(const Fields &values)
{
  for (Fields::const_iterator i = values.begin(); i != values.end(); ++i)
  {
    Mysqlx::Datatypes::Object_ObjectField *item = add_fld();
    item->set_key(i->first);
    item->mutable_value()->CopyFrom(i->second);
  }
}


Object::Object(const std::string &key, Expr *value)
{
  Mysqlx::Expr::Object_ObjectField *item = add_fld();
  item->set_key(key);
  item->set_allocated_value(value);
}


Object::Object(const std::string &key, const Expr &value)
{
  Mysqlx::Expr::Object_ObjectField *item = add_fld();
  item->set_key(key);
  item->mutable_value()->CopyFrom(value);
}


Column::Column(const std::string &name, const std::string &alias)
{
  if (!name.empty())
    set_name(name);
  if (!alias.empty())
    set_alias(alias);
}


Column::Column(const Document_path &path,
               const std::string &name, const std::string &alias)
{
  mutable_document_path()->CopyFrom(path);
  if (!name.empty())
    set_name(name);
  if (!alias.empty())
    set_alias(alias);
}


Collection::Collection(const std::string &name, const std::string &schema)
{
  if (!name.empty())
    set_name(name);
  if (!schema.empty())
    set_schema(schema);
}


Projection::Projection(const Expr &source, const std::string &alias)
{
  mutable_source()->CopyFrom(source);
  if (!alias.empty())
    set_alias(alias);
}


Order::Order(const Expr &expr, const ::Mysqlx::Crud::Order_Direction dir)
{
  mutable_expr()->CopyFrom(expr);
  set_direction(dir);
}


Limit::Limit(const uint64_t row_count, const uint64_t offset)
{
  if (row_count > 0)
    set_row_count(row_count);
  if (offset > 0)
    set_offset(offset);
}

} // namespace test
} // namespace xpl

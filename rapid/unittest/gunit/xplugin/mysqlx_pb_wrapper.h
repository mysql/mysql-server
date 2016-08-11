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

#ifndef _XPL_MYSQLX_PB_WRAPPER_H_
#define _XPL_MYSQLX_PB_WRAPPER_H_

#include "ngs_common/protocol_protobuf.h"
#include <string>


namespace xpl
{

namespace test
{

class Operator;
class FunctionCall;
class Object;
class Array;


class Identifier : public Mysqlx::Expr::Identifier
{
public:
  Identifier(const std::string &name = "", const std::string &schema_name = "");
};


class Document_path : public ::google::protobuf::RepeatedPtrField< ::Mysqlx::Expr::DocumentPathItem >
{
public:
  typedef std::pair<Mysqlx::Expr::DocumentPathItem::Type, std::string>
    Doc_path_item;
  typedef std::vector<Doc_path_item> Path;

  Document_path(const Path &path);
};


class ColumnIdentifier : public Mysqlx::Expr::ColumnIdentifier
{
public:

  ColumnIdentifier(const std::string &name = "",
                   const std::string &table_name = "",
                   const std::string &schema_name = "",
                   const Document_path::Path *path = NULL);
};


class Scalar : public Mysqlx::Datatypes::Scalar
{
public:
  struct Null { };

  struct String : public Mysqlx::Datatypes::Scalar_String
  {
    String(const std::string &value);
  };

  struct Octets : public Mysqlx::Datatypes::Scalar_Octets
  {
    Octets(const std::string &value, unsigned type);
  };

  Scalar() {}
  Scalar(int value);
  Scalar(unsigned int value);
  Scalar(bool value);
  Scalar(float value);
  Scalar(double value);
  Scalar(const char *value, unsigned type = 0);
  Scalar(Scalar::Octets *value);
  Scalar(const Scalar::Octets &value);
  Scalar(Scalar::String *value);
  Scalar(const Scalar::String &value);
  Scalar(Null value);
};


class Any : public Mysqlx::Datatypes::Any
{
public:
  class Object;
  class Array : public Mysqlx::Datatypes::Array
  {
  public:
    class Scalar_values : private std::vector<Scalar>
    {
    public:
      Scalar_values() {}
      Scalar_values &operator () (const Scalar &value);
      friend class Array;
    };

    Array(const Scalar_values &values);
    Array(const Scalar &value);
    Array(const Object &value);
    Array &operator () (const Scalar &value);
    Array &operator () (const Object &value);
    Array() {}
  };


  class Object : public Mysqlx::Datatypes::Object
  {
  public:
    class Scalar_fields : private std::map<std::string, Scalar>
    {
    public:
      Scalar_fields() {}
      Scalar_fields(const std::string &key, const Scalar &value);
      Scalar_fields &operator () (const std::string &key, const Scalar &value);
      friend class Object;
    };

    class Fields : private std::map<std::string, Any>
    {
    public:
      Fields() {}
      Fields(const std::string &key, const Any &value);
      Fields &operator () (const std::string &key, const Any &value);
      friend class Object;
    };

    Object(const Scalar_fields &values);
    Object(const Fields &values);
    Object() {}
  };

  Any() {}
  Any(Scalar *scalar);
  Any(const Scalar &scalar);
  Any(const Object &obj);
  Any(const Array &array);
};


class Expr : public Mysqlx::Expr::Expr
{
public:
  Expr() {}

  template<typename T>
  Expr(T value)
  {
    Expr::initialize(*this, value);
  }

  template<typename T>
  static void initialize(Mysqlx::Expr::Expr &expr, T value)
  {
    Scalar *scalar = new Scalar(value);

    expr.set_type(Mysqlx::Expr::Expr_Type_LITERAL);
    expr.set_allocated_literal(scalar);
  }

  static void initialize(Mysqlx::Expr::Expr &expr, Operator *oper);
  static void initialize(Mysqlx::Expr::Expr &expr, FunctionCall *func);
  static void initialize(Mysqlx::Expr::Expr &expr, ColumnIdentifier *id);
  static void initialize(Mysqlx::Expr::Expr &expr, Object *obj);
  static void initialize(Mysqlx::Expr::Expr &expr, Array *arr);

  static Expr *make_variable(const std::string &name);
  static Expr *make_placeholder(const ::google::protobuf::uint32 &value);
  static Mysqlx::Datatypes::Any *make_any(Mysqlx::Datatypes::Scalar *scalar);
};


class Operator : public Mysqlx::Expr::Operator
{
public:
  Operator(const std::string &name)
  {
    set_name(name);
  }

  template<typename T1>
  Operator(const std::string &name, T1 param1)
  {
    set_name(name);
    add_param(param1);
  }

  template<typename T1, typename T2>
  Operator(const std::string &name, T1 param1, T2 param2)
  {
    set_name(name);
    add_param(param1);
    add_param(param2);
  }

  template<typename T1, typename T2, typename T3>
  Operator(const std::string &name, T1 param1, T2 param2, T3 param3)
  {
    set_name(name);
    add_param(param1);
    add_param(param2);
    add_param(param3);
  }

  template<typename T1, typename T2, typename T3, typename T4>
  Operator(const std::string &name, T1 param1, T2 param2, T3 param3, T4 param4)
  {
    set_name(name);
    add_param(param1);
    add_param(param2);
    add_param(param3);
    add_param(param4);
  }

  template<typename T>
  void add_param(T value)
  {
    Expr::initialize(*Mysqlx::Expr::Operator::add_param(), value);
  }

  void add_param(Expr *value)
  {
    Mysqlx::Expr::Operator::mutable_param()->AddAllocated(value);
  }
};


class FunctionCall : public Mysqlx::Expr::FunctionCall
{
public:
  FunctionCall(Identifier *name)
  {
    set_allocated_name(name);
  }

  template<typename T1>
  FunctionCall(Identifier *name, T1 param1)
  {
    initialize(name, param1);
  }

  template<typename T1, typename T2>
  FunctionCall(Identifier *name, T1 param1, T2 param2)
  {
    initialize(name, param1);
    add_param(param2);
  }

  template<typename T1>
  FunctionCall(const std::string &name, T1 param1)
  {
    initialize(new Identifier(name), param1);
  }

  template<typename T1, typename T2>
  FunctionCall(const std::string &name, T1 param1, T2 param2)
  {
    initialize(new Identifier(name), param1);
    add_param(param2);
  }

  template<typename T1, typename T2, typename T3>
  FunctionCall(const std::string &name, T1 param1, T2 param2, T3 param3)
  {
    initialize(new Identifier(name), param1);
    add_param(param2);
    add_param(param3);
  }

private:
  template<typename T1>
  void initialize(Identifier *name, T1 param1)
  {
    set_allocated_name(name);
    add_param(param1);
  }

  template<typename T>
  void add_param(T value)
  {
    Expr::initialize(*Mysqlx::Expr::FunctionCall::add_param(), value);
  }

  void add_param(Expr *value)
  {
    Mysqlx::Expr::FunctionCall::mutable_param()->AddAllocated(value);
  }
};


class Object : public Mysqlx::Expr::Object
{
public:
  typedef std::map<std::string, Expr*> Values;
  explicit Object(const Values &values);
  Object() {}
};


class Array : public Mysqlx::Expr::Array
{
public:
  Array() {}

  template<int size>
  explicit Array(Expr *(&values)[size])
  {
    for (Expr **i = values; i != values+size; ++i)
      mutable_value()->AddAllocated(*i);
  }
};

} // namespace test
} // namespace xpl


#endif // _XPL_MYSQLX_PB_WRAPPER_H_

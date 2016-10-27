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


typedef ::Mysqlx::Expr::DocumentPathItem Document_path_item;


class Document_path
    : public ::google::protobuf::RepeatedPtrField<Document_path_item>
{
public:
  class Path
      : private std::vector<std::pair<Document_path_item::Type, std::string> >
  {
  public:
    Path() {}
    explicit Path(const std::string &value);
    Path &add_member(const std::string &value);
    Path &add_index(int index);
    Path &add_asterisk();
    Path &add_double_asterisk();
    friend class Document_path;
  };

  Document_path(const Path &path);
};


class ColumnIdentifier : public Mysqlx::Expr::ColumnIdentifier
{
public:
  ColumnIdentifier(const std::string &name = "",
                   const std::string &table_name = "",
                   const std::string &schema_name = "",
                   const Document_path::Path *path = NULL);
  ColumnIdentifier(const Document_path &path,
                   const std::string &name = "",
                   const std::string &table_name = "",
                   const std::string &schema_name = "");
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


class Placeholder
{
public:
  explicit Placeholder(const ::google::protobuf::uint32 v) : value(v) {}
  const ::google::protobuf::uint32 value;
};


class Variable
{
public:
  Variable(const std::string &name) : value(name) {}
  const std::string value;
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

  static void initialize(Mysqlx::Expr::Expr &expr, const Scalar &value);
  static void initialize(Mysqlx::Expr::Expr &expr, Operator *oper);
  static void initialize(Mysqlx::Expr::Expr &expr, const Operator &oper);
  static void initialize(Mysqlx::Expr::Expr &expr, const Identifier &ident);
  static void initialize(Mysqlx::Expr::Expr &expr, FunctionCall *func);
  static void initialize(Mysqlx::Expr::Expr &expr, const FunctionCall &func);
  static void initialize(Mysqlx::Expr::Expr &expr, ColumnIdentifier *id);
  static void initialize(Mysqlx::Expr::Expr &expr, const ColumnIdentifier &id);
  static void initialize(Mysqlx::Expr::Expr &expr, Object *obj);
  static void initialize(Mysqlx::Expr::Expr &expr, const Object &obj);
  static void initialize(Mysqlx::Expr::Expr &expr, Array *arr);
  static void initialize(Mysqlx::Expr::Expr &expr, const Array &arr);
  static void initialize(Mysqlx::Expr::Expr &expr, const Placeholder &ph);
  static void initialize(Mysqlx::Expr::Expr &expr, const Variable &var);
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

  void add_param(Expr *value)
  {
    mutable_param()->AddAllocated(value);
  }

  void add_param(const Expr &value)
  {
    mutable_param()->Add()->CopyFrom(value);
  }
};


class FunctionCall : public Mysqlx::Expr::FunctionCall
{
public:
  FunctionCall(Identifier *name)
  {
    set_allocated_name(name);
  }

  FunctionCall(const Identifier &name)
  {
    mutable_name()->CopyFrom(name);
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

  FunctionCall(const std::string &name)
  {
    set_allocated_name(new Identifier(name));
  }

  template<typename T1>
  FunctionCall(const Identifier &name, T1 param1)
  {
    mutable_name()->CopyFrom(name);
    add_param(param1);
  }

  template<typename T1, typename T2>
  FunctionCall(const Identifier &name, T1 param1, T2 param2)
  {
    mutable_name()->CopyFrom(name);
    add_param(param1);
    add_param(param2);
  }

  template<typename T1, typename T2, typename T3>
  FunctionCall(const Identifier &name, T1 param1, T2 param2, T3 param3)
  {
    mutable_name()->CopyFrom(name);
    add_param(param1);
    add_param(param2);
    add_param(param3);
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
    mutable_param()->AddAllocated(value);
  }
};


class Object : public Mysqlx::Expr::Object
{
public:
  class Values : private std::map<std::string, Expr>
   {
   public:
     Values() {}
     Values(const std::string &key, const Expr &value);
     Values &operator() (const std::string &key, const Expr &value);
     friend class Object;
   };

  Object(const Values &values);
  Object() {}
  Object(const std::string &key, Expr *value);
  Object(const std::string &key, const Expr &value);
};


class Array : public Mysqlx::Expr::Array
{
public:
  Array() {}

  template<int size>
  Array(Expr (&values)[size])
  {
    for (Expr *i = values; i != values+size; ++i)
      mutable_value()->Add()->CopyFrom(*i);
  }
};


class Column: public ::Mysqlx::Crud::Column
{
public:
  Column(const std::string &name, const std::string &alias = "");
  Column(const Document_path &path,
         const std::string &name = "", const std::string &alias = "");
};


class Collection : public ::Mysqlx::Crud::Collection
{
public:
  Collection(const std::string &name, const std::string &schema = "");
};


typedef ::Mysqlx::Crud::DataModel Data_model;


class Projection : public ::Mysqlx::Crud::Projection
{
public:
  Projection(const Expr &source, const std::string &alias = "");
};


class Order : public ::Mysqlx::Crud::Order
{
public:
  Order(const Expr &expr, const ::Mysqlx::Crud::Order_Direction dir = ::Mysqlx::Crud::Order_Direction_ASC);
};


class Limit : public ::Mysqlx::Crud::Limit
{
public:
  Limit(const uint64_t row_count = 0, const uint64_t offset = 0);
};


template<typename B, typename T>
class RepeatedPtrField : public ::google::protobuf::RepeatedPtrField<B>
{
public:
  RepeatedPtrField() {}
  RepeatedPtrField(const T &arg) { add(arg); }
  RepeatedPtrField &operator()(const T &arg) { return add(arg); }
  RepeatedPtrField &add(const T &arg) { *this->Add() = arg; return *this; }
};

typedef RepeatedPtrField<Mysqlx::Datatypes::Scalar, Scalar> Expression_args;
typedef RepeatedPtrField<Mysqlx::Crud::Order, Order> Order_list;
typedef Expr Filter;

} // namespace test
} // namespace xpl


#endif // _XPL_MYSQLX_PB_WRAPPER_H_

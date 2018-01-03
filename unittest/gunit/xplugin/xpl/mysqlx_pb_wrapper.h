/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XPLUGIN_MYSQLX_PB_WRAPPER_H_
#define XPLUGIN_MYSQLX_PB_WRAPPER_H_

#include <cstddef>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

namespace xpl {
namespace test {

class Operator;
class FunctionCall;
class Object;
class Array;

template <typename T>
class Wrapper {
 public:
  using Base = T;
  operator const T&() const { return m_base; }

 protected:
  T m_base;
};

template <typename T, typename B = typename T::Base>
class RepeatedPtrField
    : public Wrapper<::google::protobuf::RepeatedPtrField<B>> {
 public:
  RepeatedPtrField() = default;
  RepeatedPtrField(const std::initializer_list<T> &list) {
    for (const T &e : list) *this->m_base.Add() = e;
  }
};

class Identifier : public Wrapper<::Mysqlx::Expr::Identifier> {
 public:
  Identifier(const std::string &name = "",
             const std::string &schema_name = "");  // NOLINT(runtime/explicit)
};

class Document_path_item : public Wrapper<::Mysqlx::Expr::DocumentPathItem> {
 public:
  Document_path_item(const Base::Type type) {  // NOLINT(runtime/explicit)
    m_base.set_type(type);
  }
  Document_path_item(const int index) {  // NOLINT(runtime/explicit)
    m_base.set_type(Base::ARRAY_INDEX);
    m_base.set_index(index);
  }
  Document_path_item(const char *value) {  // NOLINT(runtime/explicit)
    m_base.set_type(Base::MEMBER);
    m_base.set_value(value);
  }
};

using Document_path = RepeatedPtrField<Document_path_item>;

class ColumnIdentifier : public Wrapper<::Mysqlx::Expr::ColumnIdentifier> {
 public:
  ColumnIdentifier(const std::string &name = "",
                   const std::string &table_name = "",
                   const std::string &schema_name =
                       "");  // NOLINT(runtime/explicit)
  ColumnIdentifier(const Document_path &path, const std::string &name = "",
                   const std::string &table_name = "",
                   const std::string &schema_name =
                       "");  // NOLINT(runtime/explicit)
};

class Scalar : public Wrapper<::Mysqlx::Datatypes::Scalar> {
 public:
  struct Null {};

  struct String : public Wrapper<::Mysqlx::Datatypes::Scalar_String> {
    String(const std::string &value);  // NOLINT(runtime/explicit)
  };

  struct Octets : public Wrapper<::Mysqlx::Datatypes::Scalar_Octets> {
    Octets(const std::string &value,
           const unsigned type = 0);  // NOLINT(runtime/explicit)
  };

  Scalar() = default;
  Scalar(const int value);                             // NOLINT(runtime/explicit)
  Scalar(const unsigned int value);                    // NOLINT(runtime/explicit)
  Scalar(const bool value);                            // NOLINT(runtime/explicit)
  Scalar(const float value);                           // NOLINT(runtime/explicit)
  Scalar(const double value);                          // NOLINT(runtime/explicit)
  Scalar(const char *value, unsigned type = 0);  // NOLINT(runtime/explicit)
  Scalar(const Scalar::Octets &value);           // NOLINT(runtime/explicit)
  Scalar(const Scalar::String &value);           // NOLINT(runtime/explicit)
  Scalar(Null value);                            // NOLINT(runtime/explicit)
};

class Any : public Wrapper<::Mysqlx::Datatypes::Any> {
 public:
  class Array;

  class Object : public Wrapper<::Mysqlx::Datatypes::Object> {
   public:
    struct Fld;
    Object() = default;
    Object(const std::initializer_list<Fld> &list);
  };

  Any() = default;
  template <typename T>
  Any(const T &v)             // NOLINT(runtime/explicit)
      : Any(Scalar(v)) {}
  Any(const Scalar &scalar);  // NOLINT(runtime/explicit)
  Any(const Object &obj);     // NOLINT(runtime/explicit)
  Any(const Array &array);    // NOLINT(runtime/explicit)

};

struct Any::Object::Fld {
  std::string key;
  Any value;
};

class Any::Array : public Wrapper<::Mysqlx::Datatypes::Array> {
 public:
  Array() = default;
  Array(const std::initializer_list<Any> &list) {
    for (const Any &e : list) m_base.add_value()->CopyFrom(e);
  }
};

class Placeholder {
 public:
  explicit Placeholder(const ::google::protobuf::uint32 v) : value(v) {}
  const ::google::protobuf::uint32 value;
};

class Variable {
 public:
  Variable(const std::string &name)  // NOLINT(runtime/explicit)
      : value(name) {}
  const std::string value;
};

class Expr : public Wrapper<::Mysqlx::Expr::Expr> {
 public:
  Expr() = default;
  template <typename T>
  Expr(const T &value)               // NOLINT(runtime/explicit)
      : Expr(Scalar(value)) {}
  Expr(const Scalar &value);         // NOLINT(runtime/explicit)
  Expr(const Operator &oper);        // NOLINT(runtime/explicit)
  Expr(const Identifier &ident);     // NOLINT(runtime/explicit)
  Expr(const FunctionCall &func);    // NOLINT(runtime/explicit)
  Expr(const ColumnIdentifier &id);  // NOLINT(runtime/explicit)
  Expr(const Object &obj);           // NOLINT(runtime/explicit)
  Expr(const Array &arr);            // NOLINT(runtime/explicit)
  Expr(const Placeholder &ph);       // NOLINT(runtime/explicit)
  Expr(const Variable &var);         // NOLINT(runtime/explicit)
};

class Operator : public Wrapper<::Mysqlx::Expr::Operator> {
 public:
  Operator(const std::string &name) {  // NOLINT(runtime/explicit)
    m_base.set_name(name);
  }

  template <typename... T>
  Operator(const std::string &name,
           T &&... params) {  // NOLINT(runtime/explicit)
    m_base.set_name(name);
    add_param(std::forward<T>(params)...);
  }

  template <typename... T>
  void add_param(const Expr &first, T &&... rest) {
    add_param(first);
    add_param(std::forward<T>(rest)...);
  }

  void add_param(const Expr &value) { *m_base.add_param() = value; }
};

class FunctionCall : public Wrapper<::Mysqlx::Expr::FunctionCall> {
 public:
  FunctionCall(const std::string &name) {  // NOLINT(runtime/explicit)
    m_base.mutable_name()->CopyFrom(Identifier(name));
  }

  template <typename... T>
  FunctionCall(const Identifier &name,
               T &&... params) {  // NOLINT(runtime/explicit)
    m_base.mutable_name()->CopyFrom(name);
    add_param(std::forward<T>(params)...);
  }

  template <typename... T>
  FunctionCall(const std::string &name,
               T &&... params) {  // NOLINT(runtime/explicit)
    m_base.mutable_name()->CopyFrom(Identifier(name));
    add_param(std::forward<T>(params)...);
  }

 private:
  template <typename... T>
  void add_param(const Expr &first, T &&... rest) {
    add_param(first);
    add_param(std::forward<T>(rest)...);
  }

  void add_param(const Expr &value) { *m_base.add_param() = value; }
};

class Object : public Wrapper<::Mysqlx::Expr::Object> {
 public:
  struct Fld {
    std::string key;
    Expr value;
  };

  Object() = default;
  Object(const std::string &key, Expr *value);
  Object(const std::initializer_list<Fld> &list);
};

class Array : public Wrapper<::Mysqlx::Expr::Array> {
 public:
  Array() = default;
  Array(const std::initializer_list<Expr> &list) {
    for (const auto &e : list) {
      *m_base.mutable_value()->Add() = e;
    }
  }
};

class Column : public Wrapper<::Mysqlx::Crud::Column> {
 public:
  Column(const std::string &name,
         const std::string &alias = "");  // NOLINT(runtime/explicit)
  Column(const Document_path &path, const std::string &name = "",
         const std::string &alias = "");  // NOLINT(runtime/explicit)
};

class Collection : public Wrapper<::Mysqlx::Crud::Collection> {
 public:
  Collection(const std::string &name,
             const std::string &schema = "");  // NOLINT(runtime/explicit)
};

using Data_model = ::Mysqlx::Crud::DataModel;

class Projection : public Wrapper<::Mysqlx::Crud::Projection> {
 public:
  Projection(const Expr &source,
             const std::string &alias = "");  // NOLINT(runtime/explicit)
};

class Order : public Wrapper<::Mysqlx::Crud::Order> {
 public:
  using Direction = Base::Direction;
  Order(const Expr &expr,
        const Direction dir = Base::Order::ASC);  // NOLINT(runtime/explicit)
};

class Limit : public Wrapper<::Mysqlx::Crud::Limit> {
 public:
  Limit(const uint64_t row_count = 0,
        const uint64_t offset = 0);  // NOLINT(runtime/explicit)
};

class Update_operation : public Wrapper<::Mysqlx::Crud::UpdateOperation> {
 public:
  using Update_type = Base::UpdateType;
  Update_operation(const Update_type &update_type,
                   const ColumnIdentifier &source, const Expr &value);
  Update_operation(const Update_type &update_type,
                   const ColumnIdentifier &source);
  Update_operation(const Update_type &update_type, const Document_path &source,
                   const Expr &value);
  Update_operation(const Update_type &update_type, const Document_path &source);
};

using Filter = Expr;
using Group = Expr;
using Grouping_criteria = Expr;
using Column_projection_list = RepeatedPtrField<Column>;
using Grouping_list = RepeatedPtrField<Group>;
using Expression_args = RepeatedPtrField<Scalar>;
using Field_list = RepeatedPtrField<Expr>;
using Order_list = RepeatedPtrField<Order>;
using Projection_list = RepeatedPtrField<Projection>;
using Value_list = RepeatedPtrField<Expr>;
using Operation_list = RepeatedPtrField<Update_operation>;

class Row_list : public Wrapper<::google::protobuf::RepeatedPtrField<
                     ::Mysqlx::Crud::Insert_TypedRow>> {
 public:
  Row_list() = default;
  Row_list(const std::initializer_list<Value_list> &list) {
    for (const auto &v : list) *this->m_base.Add()->mutable_field() = v;
  }
  Base::size_type size() const { return m_base.size(); }
};

}  // namespace test
}  // namespace xpl

#endif  // XPLUGIN_MYSQLX_PB_WRAPPER_H_

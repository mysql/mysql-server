/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XARGUMENT_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XARGUMENT_H_

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xcl {

class Argument_value {
 public:
  using Arguments = std::vector<Argument_value>;
  using Object = std::map<std::string, Argument_value>;

  enum class Type {
    TInteger,
    TUInteger,
    TNull,
    TDouble,
    TFloat,
    TBool,
    TString,
    TOctets,
    TDecimal,
    TArray,
    TObject
  };

  enum class String_type { TString, TOctets, TDecimal };

  class Argument_visitor {
   public:
    virtual ~Argument_visitor() = default;

    virtual void visit() = 0;
    virtual void visit(const int64_t value) = 0;
    virtual void visit(const uint64_t value) = 0;
    virtual void visit(const double value) = 0;
    virtual void visit(const float value) = 0;
    virtual void visit(const bool value) = 0;
    virtual void visit(const Object &value) = 0;
    virtual void visit(const Arguments &value) = 0;
    virtual void visit(const std::string &value,
                       const Argument_value::String_type st) = 0;
  };

 public:
  Argument_value() { set(); }

  explicit Argument_value(const std::string &s, const String_type string_type =
                                                    String_type::TString) {
    set(s, get_type(string_type));
  }

  template <typename Value_type>
  explicit Argument_value(const Value_type n) {
    set(n);
  }

  template <typename Value_type>
  Argument_value &operator=(const Value_type &value) {
    m_string.clear();
    m_object.clear();
    m_array.clear();
    set(value);

    return *this;
  }

  Type type() const { return m_type; }

  void accept(Argument_visitor *visitor) const {
    switch (m_type) {
      case Type::TInteger:
        visitor->visit(m_value.i);
        return;

      case Type::TUInteger:
        visitor->visit(m_value.ui);
        return;

      case Type::TNull:
        visitor->visit();
        return;

      case Type::TDouble:
        visitor->visit(m_value.d);
        return;

      case Type::TFloat:
        visitor->visit(m_value.f);
        return;

      case Type::TBool:
        visitor->visit(m_value.b);
        return;

      case Type::TString:
        visitor->visit(m_string, String_type::TString);
        return;

      case Type::TOctets:
        visitor->visit(m_string, String_type::TOctets);
        return;

      case Type::TDecimal:
        visitor->visit(m_string, String_type::TDecimal);
        return;

      case Type::TArray:
        visitor->visit(m_array);
        return;

      case Type::TObject:
        visitor->visit(m_object);
        return;
    }
  }

 private:
  Type get_type(const String_type type) const {
    switch (type) {
      case String_type::TString:
        return Type::TString;

      case String_type::TOctets:
        return Type::TOctets;

      case String_type::TDecimal:
        return Type::TDecimal;
    }

    return Type::TNull;
  }

  void set() { m_type = Type::TNull; }

  void set(const std::string &s, Type type = Type::TString) {
    m_type = type;
    m_string = s;
  }

  void set(const bool n) {
    m_type = Type::TBool;
    m_value.b = n;
  }

  void set(const float n) {
    m_type = Type::TFloat;
    m_value.f = n;
  }

  void set(const double n) {
    m_type = Type::TDouble;
    m_value.d = n;
  }

  void set(const int64_t n) {
    m_type = Type::TInteger;
    m_value.i = n;
  }

  void set(const uint64_t n) {
    m_type = Type::TUInteger;
    m_value.ui = n;
  }

  void set(const Arguments &arguments) {
    m_type = Type::TArray;
    m_array = arguments;
  }

  void set(const Object &object) {
    m_type = Type::TObject;
    m_object = object;
  }

  Type m_type;
  std::string m_string;
  Arguments m_array;
  Object m_object;

  union {
    int64_t i;
    uint64_t ui;
    double d;
    float f;
    bool b;
  } m_value;
};

using Arguments = std::vector<Argument_value>;
using Object = std::map<std::string, Argument_value>;

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XARGUMENT_H_

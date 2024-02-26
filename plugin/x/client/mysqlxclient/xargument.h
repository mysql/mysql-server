/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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
#include <utility>
#include <vector>

namespace xcl {

class Argument_value {
 public:
  using Arguments = std::vector<Argument_value>;
  using Object = std::map<std::string, Argument_value>;
  using Unordered_object = std::vector<std::pair<std::string, Argument_value>>;

  enum class String_type { k_string, k_octets, k_decimal };

  enum class Type {
    k_integer,
    k_uinteger,
    k_null,
    k_double,
    k_float,
    k_bool,
    k_string,
    k_octets,
    k_decimal,
    k_array,
    k_object
  };

  class Visitor {
   public:
    virtual ~Visitor() = default;

    virtual void visit_null() = 0;
    virtual void visit_integer(const int64_t value) = 0;
    virtual void visit_uinteger(const uint64_t value) = 0;
    virtual void visit_double(const double value) = 0;
    virtual void visit_float(const float value) = 0;
    virtual void visit_bool(const bool value) = 0;
    virtual void visit_object(const Object &value) = 0;
    virtual void visit_uobject(const Unordered_object &value) = 0;
    virtual void visit_array(const Arguments &value) = 0;
    virtual void visit_string(const std::string &value) = 0;
    virtual void visit_octets(const std::string &value) = 0;
    virtual void visit_decimal(const std::string &value) = 0;
  };

 public:
  Argument_value() { set(); }

  explicit Argument_value(const std::string &s, const String_type string_type =
                                                    String_type::k_string) {
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
    m_unordered_object.clear();
    m_array.clear();
    set(value);

    return *this;
  }

  Type type() const { return m_type; }

  void accept(Visitor *visitor) const {
    switch (m_type) {
      case Type::k_integer:
        visitor->visit_integer(m_value.i);
        return;

      case Type::k_uinteger:
        visitor->visit_uinteger(m_value.ui);
        return;

      case Type::k_null:
        visitor->visit_null();
        return;

      case Type::k_double:
        visitor->visit_double(m_value.d);
        return;

      case Type::k_float:
        visitor->visit_float(m_value.f);
        return;

      case Type::k_bool:
        visitor->visit_bool(m_value.b);
        return;

      case Type::k_string:
        visitor->visit_string(m_string);
        return;

      case Type::k_octets:
        visitor->visit_octets(m_string);
        return;

      case Type::k_decimal:
        visitor->visit_decimal(m_string);
        return;

      case Type::k_array:
        visitor->visit_array(m_array);
        return;

      case Type::k_object:
        if (m_object.empty())
          visitor->visit_uobject(m_unordered_object);
        else
          visitor->visit_object(m_object);
        return;
    }
  }

 private:
  Type get_type(const String_type type) const {
    switch (type) {
      case String_type::k_string:
        return Type::k_string;

      case String_type::k_octets:
        return Type::k_octets;

      case String_type::k_decimal:
        return Type::k_decimal;
    }

    return Type::k_null;
  }

  void set() { m_type = Type::k_null; }

  void set(const std::string &s, Type type = Type::k_string) {
    m_type = type;
    m_string = s;
  }

  void set(const char *s, Type type = Type::k_string) {
    m_type = type;
    m_string = s;
  }

  void set(const bool n) {
    m_type = Type::k_bool;
    m_value.b = n;
  }

  void set(const float n) {
    m_type = Type::k_float;
    m_value.f = n;
  }

  void set(const double n) {
    m_type = Type::k_double;
    m_value.d = n;
  }

  void set(const int64_t n) {
    m_type = Type::k_integer;
    m_value.i = n;
  }

  void set(const uint64_t n) {
    m_type = Type::k_uinteger;
    m_value.ui = n;
  }

  void set(const Arguments &arguments) {
    m_type = Type::k_array;
    m_array = arguments;
  }

  template <typename Value_type>
  void set(const std::vector<Value_type> &arguments) {
    m_type = Type::k_array;
    m_array.clear();
    for (const auto &a : arguments) {
      m_array.push_back(Argument_value(a));
    }
  }

  void set(const Object &object) {
    m_type = Type::k_object;
    m_object = object;
  }

  void set(const Unordered_object &object) {
    m_type = Type::k_object;
    m_unordered_object = object;
  }

  Type m_type;
  std::string m_string;
  Arguments m_array;
  Object m_object;
  Unordered_object m_unordered_object;

  union {
    int64_t i;
    uint64_t ui;
    double d;
    float f;
    bool b;
  } m_value;
};

using Argument_array = Argument_value::Arguments;
using Argument_object = Argument_value::Object;
using Argument_uobject = Argument_value::Unordered_object;
using Argument_visitor = Argument_value::Visitor;
using Argument_type = Argument_value::Type;

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XARGUMENT_H_

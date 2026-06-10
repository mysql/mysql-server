/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_VALUE_H_
#define ROUTER_SRC_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_VALUE_H_

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "mysqlrouter/jit_executor_plugin_export.h"

namespace shcore {

namespace polyglot {
class Polyglot_object;
class Object_bridge;
}  // namespace polyglot

class Parser_error : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

/** Basic types that can be passed around code in different languages.

 With the exception of Native and Function, all types can be serialized to JSON.
 */
enum Value_type {
  Undefined,  //! Undefined
  Null,       //! Null/None value
  Bool,       //! true or false
  String,     //! String values, UTF-8 encoding
  Integer,    //! 64bit integer numbers
  UInteger,   //! unsigned 64bit integer numbers
  Float,      //! double numbers

  //  Object,     //! Native/bridged C++ object refs, may or may not be
  //  serializable
  Object,        //! Polyglot object of any type
  ObjectBridge,  //! C++ Object

  Array,  //! Array/List container
  Map,    //! Dictionary/Map/Object container

  // Function,  //! A function reference, not serializable.
  Binary  //! Binary data
};

bool is_compatible_type(Value_type source_type, Value_type target_type);

// std::string type_description(Value_type type);
// std::string type_name(Value_type type);

// class Object_bridge;
// typedef std::shared_ptr<Object_bridge> Object_bridge_ref;

/** Pointer to a function that may be implemented in any language.
 */
// class Function_base;
// using Function_base_ref = std::shared_ptr<Function_base>;

/** A generic value that can be used from any language we support.

 Anything that can be represented using this can be passed as a parameter to
 scripting functions or stored in the internal registry or anywhere. If
 serializable types are used, then they may also be stored as a JSON document.

 Values are exposed to scripting languages according to the following rules:

 - Simple types (Null, Bool, String, Integer, Float, UInteger) are converted
 directly to the target type, both ways

 - Arrays and Maps are converted directly to the target type, both ways

 - Functions are wrapped into callable objects from C++ to scripting language
 - Scripting language functions are wrapped into an instance of a language
 specific subclass of Function_base

 - C++ Objects are generically wrapped into a scripting language object, except
 when there's a specific native counterpart

 - Scripting language objects are either generically wrapped to a language
 specific generic object wrapper or converted to a specific C++ Object subclass

 Example: JS Date object is converted to a C++ Date object and vice-versa, but
 Mysql_connection is wrapped generically

 @section Implicit type conversions

           Null Bool  String  Integer UInteger  Float Object  Array Map
 Null      OK   -     -       -       -         -     OK      OK    OK
 Bool      -    OK    -       OK      OK        OK    -       -     -
 String    -    OK    OK      OK      OK        OK    -       -     -
 Integer   -    OK    -       OK      OK        OK    -       -     -
 UInteger  -    OK    -       OK      OK        OK    -       -     -
 Float     -    OK    -       OK      OK        OK    -       -     -
 Object    -    -     -       -       -         -     OK      -     -
 Array     -    -     -       -       -         -     -       OK    -
 Map       -    -     -       -       -         -     -       -     OK

 * Integer <-> UInteger conversions are only possible if the range allows it
 * Null can be cast to Object/Array/Map, but a valid Object/Array/Map pointer
 is not NULL, so it can't be cast to it.
  */
struct JIT_EXECUTOR_PLUGIN_EXPORT Value final {
  typedef std::vector<Value> Array_type;
  typedef std::shared_ptr<Array_type> Array_type_ref;

  class Map_type final {
   public:
    typedef std::map<std::string, Value> container_type;
    typedef container_type::const_iterator const_iterator;
    typedef container_type::iterator iterator;
    using value_type = container_type::value_type;
    using reverse_iterator = container_type::reverse_iterator;
    using const_reverse_iterator = container_type::const_reverse_iterator;

    inline bool has_key(const std::string &k) const { return find(k) != end(); }

    Value_type get_type(const std::string &k) const;

    bool is_null(const std::string &k) const {
      return get_type(k) == Value_type::Null;
    }

    std::string get_string(const std::string &k,
                           const std::string &def = "") const;
    bool get_bool(const std::string &k, bool def = false) const;
    int64_t get_int(const std::string &k, int64_t def = 0) const;
    uint64_t get_uint(const std::string &k, uint64_t def = 0) const;
    double get_double(const std::string &k, double def = 0.0) const;
    std::shared_ptr<Value::Map_type> get_map(
        const std::string &k,
        std::shared_ptr<Map_type> def = std::shared_ptr<Map_type>()) const;
    std::shared_ptr<Value::Array_type> get_array(
        const std::string &k,
        std::shared_ptr<Array_type> def = std::shared_ptr<Array_type>()) const;
    void merge_contents(std::shared_ptr<Map_type> source, bool overwrite);

    // template <class C>
    // std::shared_ptr<C> get_object(
    //     const std::string &k,
    //     std::shared_ptr<C> def = std::shared_ptr<C>()) const {
    //   const_iterator iter = find(k);
    //   if (iter == end()) return def;
    //   iter->second.check_type(Object);
    //   return iter->second.as_object<C>();
    // }

    const_iterator find(const std::string &k) const { return _map.find(k); }
    iterator find(const std::string &k) { return _map.find(k); }

    size_t erase(const std::string &k) { return _map.erase(k); }
    iterator erase(const_iterator it) { return _map.erase(it); }
    iterator erase(iterator it) { return _map.erase(it); }
    void clear() { _map.clear(); }

    const_iterator begin() const { return _map.begin(); }
    iterator begin() { return _map.begin(); }

    const_reverse_iterator rbegin() const { return _map.rbegin(); }
    reverse_iterator rbegin() { return _map.rbegin(); }

    const_iterator end() const { return _map.end(); }
    iterator end() { return _map.end(); }

    const_reverse_iterator rend() const { return _map.rend(); }
    reverse_iterator rend() { return _map.rend(); }

    void set(const std::string &k, Value &&v) { _map[k] = std::move(v); }

    void set(const std::string &k, const Value &v) { _map[k] = v; }

    const container_type::mapped_type &at(const std::string &k) const {
      return _map.at(k);
    }
    container_type::mapped_type &operator[](const std::string &k) {
      return _map[k];
    }

    bool operator==(const Map_type &other) const { return _map == other._map; }
    bool operator!=(const Map_type &other) const { return !(*this == other); }

    // prevent default usage of these
    bool operator<(const Map_type &) const = delete;
    bool operator>(const Map_type &) const = delete;
    bool operator<=(const Map_type &) const = delete;
    bool operator>=(const Map_type &) const = delete;

    bool empty() const { return _map.empty(); }
    size_t size() const { return _map.size(); }
    size_t count(const std::string &k) const { return _map.count(k); }

    template <class T>
    std::pair<iterator, bool> emplace(const std::string &key, T &&v) {
      return _map.emplace(key, Value(std::forward<T>(v)));
    }

   private:
    container_type _map;
  };
  typedef std::shared_ptr<Map_type> Map_type_ref;

 public:
  Value() = default;
  Value(const Value &) = default;
  Value(Value &&) noexcept = default;
  Value &operator=(const Value &) = default;
  Value &operator=(Value &&) noexcept = default;

  ~Value() noexcept = default;

  explicit Value(const std::string &s, bool binary = false);
  explicit Value(std::string &&s, bool binary = false);
  explicit Value(const char *);
  explicit Value(const char *, size_t n, bool binary = false);
  explicit Value(std::string_view s, bool binary = false);
  explicit Value(std::wstring_view s);
  explicit Value(std::nullptr_t);
  explicit Value(int i);
  explicit Value(unsigned int ui);
  explicit Value(int64_t i);
  explicit Value(uint64_t ui);
  explicit Value(float f);
  explicit Value(double d);
  explicit Value(bool b);
  //   explicit Value(const std::shared_ptr<Function_base> &f);
  //   explicit Value(std::shared_ptr<Function_base> &&f);
  explicit Value(const std::shared_ptr<polyglot::Polyglot_object> &o);
  explicit Value(const std::shared_ptr<polyglot::Object_bridge> &o);
  //   explicit Value(std::shared_ptr<Object_bridge> &&o);
  explicit Value(const Map_type_ref &n);
  explicit Value(Map_type_ref &&n);
  explicit Value(const Array_type_ref &n);
  explicit Value(Array_type_ref &&n);

  //   static Value wrap(Object_bridge *o) {
  //     return Value(std::shared_ptr<Object_bridge>(o));
  //   }

  //   template <class T>
  //   static Value wrap(std::shared_ptr<T> o) {
  //     return Value(std::static_pointer_cast<Object_bridge>(std::move(o)));
  //   }

  static Value new_array() {
    return Value(std::shared_ptr<Array_type>(new Array_type()));
  }
  static Value new_map() {
    return Value(std::shared_ptr<Map_type>(new Map_type()));
  }

  static Value Null() {
    Value v;
    v.m_value = null_value{};
    return v;
  }

  static Value True() { return Value{true}; }
  static Value False() { return Value{false}; }

  //! parse a string returned by repr() back into a Value
  static Value parse(std::string_view s);

  bool operator==(const Value &other) const;
  bool operator!=(const Value &other) const { return !(*this == other); }

  // prevent default usage of these
  bool operator<(const Value &) const = delete;
  bool operator>(const Value &) const = delete;
  bool operator<=(const Value &) const = delete;
  bool operator>=(const Value &) const = delete;

  explicit operator bool() const noexcept {
    auto type = get_type();
    return (type != Value_type::Undefined) && (type != Value_type::Null);
  }

  bool is_null() const noexcept { return (get_type() == Value_type::Null); }

  // helper used by gtest
  friend std::ostream &operator<<(std::ostream &os, const Value &v);

  //! returns a human-readable description text for the value.
  // if pprint is true, it will try to pretty-print it (like adding newlines)
  std::string descr(bool pprint = false) const;

  //! returns a string representation of the serialized object, suitable to be
  //! passed to parse()
  std::string repr() const;

  //! returns a JSON representation of the object
  std::string json(bool pprint = false) const;

  //! returns a YAML representation of the Value
  //   std::string yaml() const;

  std::string &append_descr(std::string &s_out, int indent = -1,
                            char quote_strings = '\0') const;
  std::string &append_repr(std::string &s_out) const;

  void check_type(Value_type t) const;
  Value_type get_type() const noexcept;

  bool as_bool() const;
  int64_t as_int() const;
  uint64_t as_uint() const;
  double as_double() const;
  std::string as_string() const;
  std::wstring as_wstring() const;

  const std::string &get_string() const {
    check_type(String);

    if (std::holds_alternative<binary_string>(m_value))
      return std::get<binary_string>(m_value);
    return std::get<std::string>(m_value);
  }

  template <class C>
  std::shared_ptr<C> as_object_bridge() const {
    check_type(ObjectBridge);

    if (is_null()) return nullptr;
    return std::dynamic_pointer_cast<C>(
        std::get<std::shared_ptr<polyglot::Object_bridge>>(m_value));
  }

  std::shared_ptr<polyglot::Object_bridge> as_object_bridge() const;
  std::shared_ptr<polyglot::Polyglot_object> as_object() const;

  std::shared_ptr<Map_type> as_map() const {
    check_type(Map);

    if (is_null()) return nullptr;
    return std::get<std::shared_ptr<Map_type>>(m_value);
  }

  std::shared_ptr<Array_type> as_array() const {
    check_type(Array);

    if (is_null()) return nullptr;
    return std::get<std::shared_ptr<Array_type>>(m_value);
  }

  //   std::shared_ptr<Function_base> as_function() const {
  //     check_type(Function);

  //     if (is_null()) return nullptr;
  //     return std::get<std::shared_ptr<Function_base>>(m_value);
  //   }

  // template <class C>
  // C to_string_container() const {
  //   C vec;
  //   auto arr = as_array();

  //   if (arr) {
  //     std::transform(arr->begin(), arr->end(), std::inserter<C>(vec,
  //     vec.end()),
  //                    [](const Value &v) { return v.get_string(); });
  //   }

  //   return vec;
  // }

  // std::map<std::string, std::string> to_string_map() const {
  //   check_type(Map);

  //   std::map<std::string, std::string> map;
  //   for (const auto &v : *as_map()) {
  //     map.emplace(v.first, v.second.get_string());
  //   }
  //   return map;
  // }

  // template <class C>
  // std::map<std::string, C> to_container_map() const {
  //   check_type(Map);

  //   std::map<std::string, C> map;
  //   for (const auto &v : *as_map()) {
  //     map.emplace(v.first, v.second.to_string_container<C>());
  //   }
  //   return map;
  // }

 private:
  //   std::string yaml(int indent) const;

 private:
  struct null_value {};
  struct binary_string : std::string {};

  std::variant < std::monostate, null_value, bool, std::string, binary_string,
      int64_t, uint64_t, double,  std::shared_ptr<polyglot::Polyglot_object>,
      std::shared_ptr<polyglot::Object_bridge>,
      std::shared_ptr<Array_type>,
      std::shared_ptr<Map_type> /*,
std::shared_ptr<Function_base>*/>
          m_value;
};

using Argument_list = std::vector<Value>;
typedef Value::Map_type_ref Dictionary_t;
typedef Value::Array_type_ref Array_t;

Dictionary_t JIT_EXECUTOR_PLUGIN_EXPORT make_dict();

inline Array_t make_array() { return std::make_shared<Value::Array_type>(); }

template <typename... Arg>
inline Array_t make_array(Arg &&...args) {
  auto array = make_array();
  (void)std::initializer_list<int>{
      (array->emplace_back(std::forward<Arg>(args)), 0)...};
  return array;
}

template <template <typename...> class C, typename T>
inline Array_t make_array(const C<T> &container) {
  auto array = make_array();
  for (const auto &item : container) {
    array->emplace_back(item);
  }
  return array;
}

template <template <typename...> class C, typename T>
inline Array_t make_array(C<T> &&container) {
  auto array = make_array();
  for (auto &item : container) {
    array->emplace_back(std::move(item));
  }
  return array;
}

}  // namespace shcore

#endif  // ROUTER_SRC_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_VALUE_H_

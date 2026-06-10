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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_POLYGLOT_WRAPPERS_TYPES_POLYGLOT_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_POLYGLOT_WRAPPERS_TYPES_POLYGLOT_H_

#include <memory>

#include "utils/polyglot_api_clean.h"

#include "jit_executor_type_conversion.h"
#include "mysqlrouter/jit_executor_value.h"
#include "utils/polyglot_store.h"
#include "utils/polyglot_utils.h"

#include "utils/utils_json.h"

namespace shcore {
namespace polyglot {

class Polyglot_language;

/**
 * Utility class to allow wrapping a Polyglot object to be used from the C++.
 */
class Polyglot_object {
 public:
  Polyglot_object() = delete;

  Polyglot_object(const Polyglot_type_bridger *type_bridger, poly_thread thread,
                  poly_context context, poly_value object,
                  const std::string &class_name);

  Polyglot_object(const Polyglot_object &) = delete;
  Polyglot_object &operator=(const Polyglot_object &) = delete;

  Polyglot_object(Polyglot_object &&) = delete;
  Polyglot_object &operator=(Polyglot_object &&) = delete;

  virtual ~Polyglot_object() = default;

  std::string class_name() const;

  void append_json(JSON_dumper &dumper) const;

  std::vector<std::string> get_members() const;

  Value get_member(const std::string &prop) const;
  poly_value get_poly_member(const std::string &prop) const;
  bool has_member(const std::string &prop) const;
  void set_member(const std::string &prop, Value value);
  void set_poly_member(const std::string &prop, poly_value value);

  Value get_property(const std::string &prop) const;

  void set_property(const std::string &prop, Value value);

  Value call(const std::string &name, const std::vector<Value> &args);

  poly_value get() const { return m_object.get(); }
  bool remove_member(const std::string &name);

  bool is_exception() const;
  void throw_exception() const;

 private:
  const Polyglot_type_bridger *m_types;
  poly_thread m_thread;
  poly_context m_context;
  Store m_object;
  std::string m_class_name;
};

/**
 * Utility class to allow wrapping a Polyglot function to be used from C++.
 */
class Polyglot_function {
 public:
  Polyglot_function() = delete;

  Polyglot_function(std::weak_ptr<Polyglot_language> language,
                    poly_value function);

  Polyglot_function(const Polyglot_function &) = delete;
  Polyglot_function operator=(const Polyglot_function &) = delete;

  Polyglot_function(Polyglot_function &&) = delete;
  Polyglot_function operator=(Polyglot_function &&) = delete;

  ~Polyglot_function();

  const std::string &name() const { return m_name; }

  Value invoke(const std::vector<Value> &args);

 private:
  std::weak_ptr<Polyglot_language> m_language;
  poly_reference m_function;
  std::string m_name;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_POLYGLOT_WRAPPERS_TYPES_POLYGLOT_H_

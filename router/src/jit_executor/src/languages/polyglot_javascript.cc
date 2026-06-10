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

#include "languages/polyglot_javascript.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/polyglot_api_clean.h"
#include "utils/polyglot_error.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

poly_value Java_script_interface::undefined() const {
  return m_undefined.get();
}

bool Java_script_interface::is_undefined(poly_value value) const {
  bool result = false;
  {
    Scoped_global builder(this, value);
    auto undefined = builder.execute("<<global>> === undefined");

    throw_if_error(poly_value_as_boolean, thread(), undefined, &result);
  }

  return result;
}

poly_value Java_script_interface::array_buffer(const std::string &data) const {
  poly_value buffer;
  throw_if_error(poly_create_byte_buffer, thread(), context(),
                 static_cast<const void *>(data.c_str()), data.size(), &buffer);

  Scoped_global builder(this, buffer);
  return builder.execute("new ArrayBuffer(<<global>>)");
}

bool Java_script_interface::is_object(poly_value object,
                                      std::string *class_name) const {
  return polyglot::is_object(thread(), object, class_name);
}

const std::vector<std::string> &Java_script_interface::keywords() const {
  static std::vector<std::string> k_builtin_keywords = {
      "break",    "case",       "catch",  "class",    "const", "continue",
      "debugger", "default",    "delete", "do",       "else",  "export",
      "extends",  "finally",    "for",    "function", "if",    "import",
      "in",       "instanceof", "new",    "return",   "super", "switch",
      "this",     "throw",      "try",    "typeof",   "var",   "void",
      "while",    "with",       "yield"};

  return k_builtin_keywords;
}

void Java_script_interface::initialize(
    const std::shared_ptr<IFile_system> &fs) {
  Polyglot_language::initialize(fs);

  {
    Scoped_global builder(this);
    m_undefined = Store(thread(), builder.execute("<<global>>=undefined"));
  }
}

void Java_script_interface::finalize() {
  m_undefined.reset();
  Polyglot_language::finalize();
}

poly_value Java_script_interface::create_exception_object(
    const std::string &error, poly_value exception_object) const {
  Scoped_global scoped_global(this, exception_object);

  // Creates an error object to be set as an exception
  return scoped_global.execute(
      shcore::str_format("Error(%s, {cause:<<global>>})",
                         shcore::quote_string(error, '`').c_str()));
}

}  // namespace polyglot
}  // namespace shcore

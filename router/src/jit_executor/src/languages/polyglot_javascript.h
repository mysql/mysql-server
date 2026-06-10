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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_JAVASCRIPT_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_JAVASCRIPT_H_

#include <list>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "utils/polyglot_api_clean.h"

#include "languages/polyglot_language.h"
#include "utils/polyglot_store.h"

namespace shcore {
namespace polyglot {

class Java_script_interface : public Polyglot_language {
 public:
  using Polyglot_language::Polyglot_language;

  ~Java_script_interface() override = default;

  const char *get_language_id() const override { return "js"; }

  void initialize(const std::shared_ptr<IFile_system> &fs = {}) override;

  void finalize() override;

  poly_value undefined() const override;
  bool is_undefined(poly_value value) const override;
  poly_value array_buffer(const std::string &data) const override;
  bool is_object(poly_value object,
                 std::string *class_name = nullptr) const override;
  const std::vector<std::string> &keywords() const override;

 private:
  poly_value create_exception_object(
      const std::string &error, poly_value exception_object) const override;

  Store m_undefined;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_LANGUAGES_POLYGLOT_JAVASCRIPT_H_

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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ARRAY_WRAPPER_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ARRAY_WRAPPER_H_

#include <memory>

#include "utils/polyglot_api_clean.h"

#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_native_wrapper.h"

namespace shcore {
namespace polyglot {

class Polyglot_language;

class Polyglot_array_wrapper
    : public Polyglot_native_wrapper<Value::Array_type,
                                     Collectable_type::ARRAY> {
 public:
  Polyglot_array_wrapper() = delete;

  explicit Polyglot_array_wrapper(std::weak_ptr<Polyglot_language> language);
  ~Polyglot_array_wrapper() override = default;

  Polyglot_array_wrapper(const Polyglot_array_wrapper &) = delete;
  Polyglot_array_wrapper &operator=(const Polyglot_array_wrapper &) = delete;

  Polyglot_array_wrapper(Polyglot_array_wrapper &&) = delete;
  Polyglot_array_wrapper &operator=(Polyglot_array_wrapper &&) = delete;

 private:
  poly_value create_wrapper(poly_thread thread, poly_context context,
                            ICollectable *collectable) const override;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ARRAY_WRAPPER_H_

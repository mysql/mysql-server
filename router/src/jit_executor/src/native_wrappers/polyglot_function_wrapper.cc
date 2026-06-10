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

#include "mysqlshdk/scripting/polyglot/native_wrappers/polyglot_function_wrapper.h"

#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

#include "mysqlshdk/include/shellcore/interrupt_handler.h"
#include "mysqlshdk/scripting/polyglot/languages/polyglot_language.h"
#include "mysqlshdk/scripting/polyglot/native_wrappers/polyglot_collectable.h"
#include "mysqlshdk/scripting/polyglot/utils/polyglot_utils.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {
namespace {
struct Call {
  static Value callback(const Polyglot_function_wrapper::Native_ptr &function,
                        const shcore::Argument_list &argv) {
    return function->invoke(argv);
  }
};
}  // namespace

Polyglot_function_wrapper::Polyglot_function_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_function_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value poly_function;
  throw_if_error(poly_create_proxy_function, thread, context,
                 &Polyglot_function_wrapper::native_handler_variable_args<Call>,
                 &Polyglot_function_wrapper::handler_release_collectable,
                 collectable, &poly_function);

  return poly_function;
}

}  // namespace polyglot
}  // namespace shcore

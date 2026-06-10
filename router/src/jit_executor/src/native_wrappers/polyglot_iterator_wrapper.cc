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

#include "native_wrappers/polyglot_iterator_wrapper.h"

#include <memory>
#include <stdexcept>

#include "languages/polyglot_language.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"

namespace shcore {
namespace polyglot {

namespace {

struct Has_next {
  static constexpr const char *name = "hasNext";
  static Value callback(const Polyglot_iterator_wrapper::Native_ptr &data) {
    return Value(data->has_next());
  }
};

struct Get_next {
  static constexpr const char *name = "getNext";
  static Value callback(const Polyglot_iterator_wrapper::Native_ptr &data) {
    return data->get_next();
  }
};
}  // namespace

poly_value Polyglot_iterator_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value poly_iterator;
  throw_if_error(poly_create_proxy_iterator, thread, context, collectable,
                 &Polyglot_iterator_wrapper::native_handler_no_args<Has_next>,
                 &Polyglot_iterator_wrapper::native_handler_no_args<Get_next>,
                 &Polyglot_iterator_wrapper::handler_release_collectable,
                 &poly_iterator);

  return poly_iterator;
}

}  // namespace polyglot
}  // namespace shcore

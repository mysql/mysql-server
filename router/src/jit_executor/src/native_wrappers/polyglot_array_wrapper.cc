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

#include "native_wrappers/polyglot_array_wrapper.h"

#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

#include "languages/polyglot_language.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"

namespace shcore {
namespace polyglot {
namespace {

struct Get {
  static constexpr const char *name = "get";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_array_wrapper::Native_ptr &array,
                        const Argument_list &argv) {
    auto index = argv[0].as_uint();

    Value result;
    if (index < array->size()) {
      result = array->at(index);
    }

    return result;
  }
};

struct Set {
  static constexpr const char *name = "set";
  static constexpr std::size_t argc = 2;
  static Value callback(const Polyglot_array_wrapper::Native_ptr &array,
                        const Argument_list &argv) {
    auto index = argv[0].as_uint();

    if (index < array->size()) {
      (*array)[index] = argv[1];
    } else {
      // Fills with undefined as many items as needed to fill the array to
      // the index-1 position
      array->resize(index);

      // Inserts the received value
      array->push_back(argv[1]);
    }

    return {};
  }
};

struct Remove {
  static constexpr const char *name = "remove";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_array_wrapper::Native_ptr &array,
                        const Argument_list &argv) {
    auto index = argv[0].as_uint();

    bool was_removed = false;

    if (index < array->size()) {
      array->erase(array->begin() + index);
      was_removed = true;
    }

    return Value(was_removed);
  }
};

struct Get_size {
  static constexpr const char *name = "getSize";
  static Value callback(const Polyglot_array_wrapper::Native_ptr &array) {
    return Value(static_cast<uint64_t>(array->size()));
  }
};

}  // namespace

Polyglot_array_wrapper::Polyglot_array_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_array_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value value = nullptr;
  throw_if_error(
      poly_create_proxy_array,

      thread, context, collectable,
      &Polyglot_array_wrapper::native_handler_fixed_args<Get>,
      &Polyglot_array_wrapper::native_handler_fixed_args<Set>,
      &Polyglot_array_wrapper::native_handler_fixed_args<Remove>,
      &Polyglot_array_wrapper::native_handler_no_args<Get_size>,
      nullptr,  // &handler_get_iterator, nothing indicates it is needed
      &Polyglot_array_wrapper::handler_release_collectable, &value);

  return value;
}

}  // namespace polyglot
}  // namespace shcore

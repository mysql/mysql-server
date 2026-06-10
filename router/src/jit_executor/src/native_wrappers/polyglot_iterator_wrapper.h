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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ITERATOR_WRAPPER_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ITERATOR_WRAPPER_H_
#include <memory>

#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_collectable.h"
#include "native_wrappers/polyglot_native_wrapper.h"

namespace shcore {
namespace polyglot {

class IPolyglot_iterator {
 public:
  virtual bool has_next() const = 0;
  virtual shcore::Value get_next() = 0;

  virtual ~IPolyglot_iterator() = default;
};

class Polyglot_iterator_wrapper
    : public Polyglot_native_wrapper<IPolyglot_iterator,
                                     Collectable_type::ITERATOR> {
 public:
  explicit Polyglot_iterator_wrapper(std::weak_ptr<Polyglot_language> language)
      : Polyglot_native_wrapper(std::move(language)) {}

 private:
  poly_value create_wrapper(poly_thread thread, poly_context context,
                            ICollectable *collectable) const override;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_ITERATOR_WRAPPER_H_

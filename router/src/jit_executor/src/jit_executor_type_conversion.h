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

#ifndef _MYSQLSHDK_SCRIPTING_POLYGLOT_JIT_EXECUTOR_TYPE_CONVERSIONS_
#define _MYSQLSHDK_SCRIPTING_POLYGLOT_JIT_EXECUTOR_TYPE_CONVERSIONS_

#include "mysqlrouter/jit_executor_value.h"
#include "utils/polyglot_api_clean.h"

namespace shcore {
namespace polyglot {

class Polyglot_language;

struct Polyglot_type_bridger {
  Polyglot_type_bridger(std::shared_ptr<Polyglot_language> context);
  ~Polyglot_type_bridger();

  void init();
  void dispose();

  Value poly_value_to_native_value(const poly_value &value) const;
  poly_value native_value_to_poly_value(const Value &value) const;

  std::string type_name(poly_value value) const;
  poly_value type_info(poly_value value) const;

  std::vector<Value> convert_args(const std::vector<poly_value> &args);

  std::weak_ptr<Polyglot_language> owner;

  class Polyglot_map_wrapper *map_wrapper;
  class Polyglot_array_wrapper *array_wrapper;
  class Polyglot_object_wrapper *object_wrapper;
  class Polyglot_object_wrapper *indexed_object_wrapper;
};

}  // namespace polyglot
}  // namespace shcore

#endif  //_MYSQLSHDK_SCRIPTING_POLYGLOT_JIT_EXECUTOR_TYPE_CONVERSIONS_

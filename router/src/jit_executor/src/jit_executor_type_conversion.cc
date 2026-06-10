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

#include "jit_executor_type_conversion.h"

#include <cerrno>
#include <fstream>
#include <stdexcept>

#include "utils/polyglot_utils.h"

#include "languages/polyglot_language.h"

#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_array_wrapper.h"
#include "native_wrappers/polyglot_map_wrapper.h"
#include "native_wrappers/polyglot_object_wrapper.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {
namespace {

bool is_null(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_null, thread, value, &result);

  return result;
}

bool is_number(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_number, thread, value, &result);

  return result;
}

bool is_integer(poly_thread thread, poly_value value, bool is_numeric = false) {
  bool result{false};
  if (is_numeric || is_number(thread, value)) {
    throw_if_error(poly_value_fits_in_int64, thread, value, &result);
  }

  return result;
}

bool is_string(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_string, thread, value, &result);

  return result;
}

bool is_boolean(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_boolean, thread, value, &result);

  return result;
}

bool is_array_buffer(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_buffer, thread, value, &result);

  return result;
}

bool is_exception(poly_thread thread, poly_value value) {
  bool result{false};

  throw_if_error(poly_value_is_exception, thread, value, &result);

  return result;
}

}  // namespace

Polyglot_type_bridger::Polyglot_type_bridger(
    std::shared_ptr<Polyglot_language> context)
    : owner(std::move(context)), map_wrapper(nullptr), array_wrapper(nullptr) {}

void Polyglot_type_bridger::init() {
  map_wrapper = new Polyglot_map_wrapper(owner);
  array_wrapper = new Polyglot_array_wrapper(owner);
  object_wrapper = new Polyglot_object_wrapper(owner);
  indexed_object_wrapper = new Polyglot_object_wrapper(owner, true);
}

void Polyglot_type_bridger::dispose() {
  if (map_wrapper) {
    delete map_wrapper;
    map_wrapper = nullptr;
  }

  if (array_wrapper) {
    delete array_wrapper;
    array_wrapper = nullptr;
  }

  if (object_wrapper) {
    delete object_wrapper;
    object_wrapper = nullptr;
  }

  if (indexed_object_wrapper) {
    delete indexed_object_wrapper;
    indexed_object_wrapper = nullptr;
  }
}

Polyglot_type_bridger::~Polyglot_type_bridger() {}

Value Polyglot_type_bridger::poly_value_to_native_value(
    const poly_value &value) const {
  const auto ctx = owner.lock();
  if (!ctx) {
    throw std::logic_error(
        "Unable to convert polyglot value, context is gone!");
  }

  auto thread = ctx->thread();

  if (!value) {
    return Value();
  } else if (is_null(thread, value)) {
    if (ctx->is_undefined(value)) {
      return Value();
    } else {
      return Value::Null();
    }
  } else if (is_string(thread, value)) {
    return Value(ctx->to_string(value));
  } else if (is_number(thread, value)) {
    if (is_integer(thread, value, true)) {
      return Value(to_int(thread, value));
    } else {
      double result{0};
      throw_if_error(poly_value_as_double, ctx->thread(), value, &result);

      return Value(result);
    }
  } else if (is_boolean(thread, value)) {
    bool result{false};
    throw_if_error(poly_value_as_boolean, ctx->thread(), value, &result);
    return Value(result);
  } else if (is_array_buffer(thread, value)) {
    size_t buffer_size{0};
    throw_if_error(poly_value_as_byte_buffer, ctx->thread(), value, nullptr, 0,
                   &buffer_size);

    std::string buffer;
    buffer.resize(buffer_size);

    throw_if_error(poly_value_as_byte_buffer, ctx->thread(), value, &buffer[0],
                   buffer_size, &buffer_size);

    return Value(std::move(buffer), true);
  } else if (std::string class_name; ctx->is_object(value, &class_name)) {
    return ctx->to_native_object(value, class_name);
  } else if (std::shared_ptr<Value::Array_type> array;
             Polyglot_array_wrapper::unwrap(ctx->thread(), value, &array)) {
    return Value(std::move(array));
  } else if (Dictionary_t map;
             Polyglot_map_wrapper::unwrap(ctx->thread(), value, &map)) {
    return Value(std::move(map));
  } else if (Object_bridge_t object;
             Polyglot_object_wrapper::unwrap(ctx->thread(), value, &object)) {
    return Value(std::move(map));
  } else if (is_exception(ctx->thread(), value)) {
    throw Polyglot_error(ctx->thread(),
                         poly_value_throw_exception(ctx->thread(), value));
  } else {
    std::string description{"Cannot convert value to native value: "};
    try {
      description += polyglot::to_string(ctx->thread(), value);
    } catch (const std::exception &error) {
      description += error.what();
    }

    throw std::invalid_argument(description);
  }
  // }
  return {};
}

poly_value Polyglot_type_bridger::native_value_to_poly_value(
    const Value &value) const {
  const auto ctx = owner.lock();
  if (!ctx) {
    throw std::logic_error("Unable to convert native value, context is gone!");
  }

  switch (value.get_type()) {
    case Undefined:
      return ctx->undefined();
    case Null:
      return poly_null(ctx->thread(), ctx->context());
    case Bool:
      return poly_bool(ctx->thread(), ctx->context(), value.as_bool());
    case String:
      return poly_string(ctx->thread(), ctx->context(), value.get_string());
    case Integer:
      return poly_int(ctx->thread(), ctx->context(), value.as_int());
    case UInteger:
      return poly_uint(ctx->thread(), ctx->context(), value.as_uint());
    case Float:
      return poly_double(ctx->thread(), ctx->context(), value.as_double());
    case Object:
      return value.as_object().get();
    case ObjectBridge: {
      auto object_bridge = value.as_object_bridge();
      auto value = ctx->from_native_object(object_bridge);

      if (value) {
        return value;
      }

      if (object_bridge->is_indexed()) {
        return indexed_object_wrapper->wrap(object_bridge);
      } else {
        return object_wrapper->wrap(object_bridge);
      }
    }
    case Array:
      return array_wrapper->wrap(value.as_array());
    case Map:
      return map_wrapper->wrap(value.as_map());
    case Binary:
      return ctx->array_buffer(value.get_string());
  }

  return nullptr;
}

std::vector<Value> Polyglot_type_bridger::convert_args(
    const std::vector<poly_value> &args) {
  std::vector<Value> r;

  for (const auto arg : args) {
    r.push_back(poly_value_to_native_value(arg));
  }

  return r;
}

std::string Polyglot_type_bridger::type_name(poly_value value) const {
  if (!value) return "Undefined";

  auto ctx = owner.lock();
  if (!ctx) {
    throw std::logic_error("Unable retrieve typename, context is gone!");
  }

  auto thread = ctx->thread();

  if (is_null(thread, value)) {
    if (ctx->is_undefined(value)) {
      return "Undefined";
    } else {
      return "Null";
    }
  } else if (is_string(thread, value)) {
    return "String";
  } else if (is_number(thread, value)) {
    if (is_integer(thread, value, true)) {
      return "Integer";
    } else {
      return "Number";
    }
  } else if (is_boolean(thread, value)) {
    return "Bool";
    // Polyglot guest language objects
  } else if (std::string class_name; ctx->is_object(value, &class_name)) {
    return class_name;
  } else if (is_executable(thread, value)) {
    return "Function";
  }

  return "";
}

poly_value Polyglot_type_bridger::type_info(poly_value value) const {
  auto type = type_name(value);

  auto ctx = owner.lock();
  if (!ctx) {
    throw std::logic_error("Unable to retrieve type name, context is gone!");
  }

  if (!type.empty()) {
    return ctx->poly_string(type);
  }

  return nullptr;
}

}  // namespace polyglot
}  // namespace shcore
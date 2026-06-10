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

#include "utils/polyglot_utils.h"

#include <numeric>
#include <optional>
#include <random>
#include <regex>
#include <sstream>

#include "utils/polyglot_api_clean.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_error.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

IMPORT_LOG_FUNCTIONS()

namespace {

poly_value poly_string(poly_thread thread, poly_context context,
                       const char *data, size_t size) {
  poly_value str;
  throw_if_error(poly_create_string_utf8, thread, context, data, size, &str);

  return str;
}

}  // namespace

/**
 * Parses the callback information sent by polyglot, returning if requested a
 * vector with the arguments as well as the collectable associated to the call
 */
size_t parse_callback_args(poly_thread thread, poly_callback_info args,
                           std::vector<poly_value> *argv, void **data) {
  size_t argc = 0;
  void *tmp_data = nullptr;
  void **target_data = data ? data : &tmp_data;

  try {
    if (const auto rc =
            poly_get_callback_info(thread, args, &argc, nullptr, target_data);
        rc != poly_ok) {
      throw std::runtime_error(Polyglot_error(thread, rc).message());
    }

    if (argc != 0 && argv != nullptr) {
      argv->resize(argc);

      if (const auto rc = poly_get_callback_info(thread, args, &argc,
                                                 &(*argv)[0], target_data);
          rc != poly_ok) {
        throw std::runtime_error(Polyglot_error(thread, rc).message());
      }
    }
  } catch (const Polyglot_generic_error &error) {
    throw std::runtime_error(error.message());
  }

  return argc;
}

bool get_data(poly_thread thread, poly_callback_info args,
              std::string_view name, void **data) {
  try {
    if (0 != parse_callback_args(thread, args, nullptr, data)) {
      throw std::runtime_error(
          shcore::str_format("%.*s() takes no arguments",
                             static_cast<int>(name.length()), name.data()));
      return false;
    }
  } catch (const std::runtime_error &error) {
    throw_callback_exception(thread, error.what());
    return false;
  }

  return true;
}

bool get_args_and_data(poly_thread thread, poly_callback_info args,
                       std::string_view name, void **data, size_t expected_argc,
                       std::vector<poly_value> *argv) {
  try {
    if (expected_argc != parse_callback_args(thread, args, argv, data)) {
      throw std::runtime_error(shcore::str_format(
          "%.*s(...) takes %zd argument%s", static_cast<int>(name.length()),
          name.data(), expected_argc, expected_argc == 1 ? "" : "s"));
    }
  } catch (const std::runtime_error &error) {
    throw_callback_exception(thread, error.what());
    return false;
  }

  return true;
}

bool is_native_type(poly_thread thread, poly_value value, Collectable_type type,
                    void **native_data) {
  bool ret_val = false;
  void *data = nullptr;
  if (poly_ok == poly_value_get_native_data(thread, value, &data)) {
    if (!data) {
      return false;
    } else {
      ret_val = (static_cast<ICollectable *>(data))->type() == type;

      if (native_data) {
        *native_data = data;
      }
    }
  }

  return ret_val;
}

std::string to_string(poly_thread thread, poly_value obj) {
  size_t s{0};
  throw_if_error(poly_value_as_string_utf8, thread, obj, nullptr, 0, &s);

  std::string value;
  value.resize(s++);
  throw_if_error(poly_value_as_string_utf8, thread, obj, &value[0], s, &s);

  return value;
}

int64_t to_int(poly_thread thread, poly_value obj) {
  int64_t value = 0;
  throw_if_error(poly_value_as_int64, thread, obj, &value);

  return value;
}

double to_double(poly_thread thread, poly_value obj) {
  double value = 0;
  throw_if_error(poly_value_as_double, thread, obj, &value);

  return value;
}

double to_boolean(poly_thread thread, poly_value obj) {
  bool value = false;
  throw_if_error(poly_value_as_boolean, thread, obj, &value);

  return value;
}

poly_value poly_string(poly_thread thread, poly_context context,
                       std::string_view data) {
  return poly_string(thread, context, data.data(), data.length());
}

poly_value poly_bool(poly_thread thread, poly_context context, bool value) {
  poly_value boolean;
  throw_if_error(poly_create_boolean, thread, context, value, &boolean);

  return boolean;
}

poly_value poly_int(poly_thread thread, poly_context context, int64_t value) {
  poly_value integer;

  throw_if_error(poly_create_int64, thread, context, value, &integer);

  return integer;
}

poly_value poly_uint(poly_thread thread, poly_context context, uint64_t value) {
  poly_value integer;

  if (value <= std::numeric_limits<uint32_t>::max()) {
    throw_if_error(poly_create_uint32, thread, context, value, &integer);
  } else {
    throw std::runtime_error("Only 32 bit unsigned integers are supported");
  }

  return integer;
}

poly_value poly_double(poly_thread thread, poly_context context, double value) {
  poly_value result;

  throw_if_error(poly_create_double, thread, context, value, &result);
  return result;
}

poly_value poly_null(poly_thread thread, poly_context context) {
  poly_value result;
  throw_if_error(poly_create_null, thread, context, &result);

  return result;
}

poly_value poly_array(poly_thread thread, poly_context context,
                      const std::vector<poly_value> &values) {
  poly_value array;
  throw_if_error(poly_create_array, thread, context, &values[0], values.size(),
                 &array);

  return array;
}

std::vector<std::string> get_member_keys(poly_thread thread,
                                         poly_context context,
                                         poly_value object) {
  size_t size{0};
  throw_if_error(poly_value_get_member_keys, thread, context, object, &size,
                 nullptr);

  std::vector<poly_value> poly_keys;
  poly_keys.resize(size);
  throw_if_error(poly_value_get_member_keys, thread, context, object, &size,
                 &poly_keys[0]);

  std::vector<std::string> keys;
  for (auto key : poly_keys) {
    keys.push_back(to_string(thread, key));
  }
  return keys;
}

void throw_callback_exception(poly_thread thread, const char *error) {
  try {
    throw_if_error(poly_throw_exception, thread, error);
  } catch (const std::exception &err) {
    log_error("Error while throwing exception on C callback: %s", err.what());
  }
}

poly_value get_member(poly_thread thread, poly_value object,
                      const std::string &name) {
  poly_value member;
  throw_if_error(poly_value_get_member, thread, object, name.c_str(), &member);

  return member;
}

bool is_executable(poly_thread thread, poly_value object) {
  bool callable = false;

  throw_if_error(poly_value_can_execute, thread, object, &callable);

  return callable;
}

bool get_member(poly_thread thread, poly_value object, const char *name,
                poly_value *member) {
  bool has_member{false};
  throw_if_error(poly_value_has_member, thread, object, name, &has_member);

  if (has_member) {
    throw_if_error(poly_value_get_member, thread, object, name, member);
  }

  return has_member;
}

bool is_object(poly_thread thread, poly_value object, std::string *class_name) {
  poly_value constructor;
  if (get_member(thread, object, "constructor", &constructor)) {
    if (class_name) {
      poly_value poly_class_name;
      throw_if_error(poly_value_get_member, thread, constructor, "name",
                     &poly_class_name);

      *class_name = to_string(thread, poly_class_name);
    }

    return true;
  }

  return false;
}

}  // namespace polyglot
}  // namespace shcore

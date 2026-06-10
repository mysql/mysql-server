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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_UTILS_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_UTILS_H_

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "utils/polyglot_api_clean.h"

#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {

enum class Language { JAVASCRIPT };

inline const char *k_origin_shell{"(shell)"};

inline const char *k_syntax_error{"SyntaxError"};
inline const char *k_interrupted_error{"InterruptedError"};

/**
 * Returns the collectable and arguments sent by Polyglot on a call to a C++
 * function.
 *
 * This function is meant to be used by a C++ function that does not require to
 * validate the argument count.
 */
size_t parse_callback_args(poly_thread thread, poly_callback_info args,
                           std::vector<poly_value> *argv, void **data);

/**
 * Returns the collectable sent by Polyglot on a call to a C++ function.
 *
 * This function is meant to be used in C++ functions that have no arguments.
 *
 * It also centralizes a validation to ensure no arguments are sent by Polyglot.
 */
bool get_data(poly_thread thread, poly_callback_info args,
              std::string_view name, void **data);

/**
 * Returns the collectable and arguments sent by Polyglot on a call to a C++
 * function.
 *
 * This function is meant to be used in C++ functions that expect a specific
 * number of arguments since it centralizes a validation to ensure the expected
 * arguments were received.
 */
bool get_args_and_data(poly_thread thread, poly_callback_info args,
                       std::string_view name, void **data, size_t expected_argc,
                       std::vector<poly_value> *argv);

/**
 * Identifies if a given poly_value corresponds to a wrapped C++ element.
 */
bool is_native_type(poly_thread thread, poly_value value, Collectable_type type,
                    void **native_data = nullptr);

/**
 * Retrieves the names of the members of the given poly_value.
 */
std::vector<std::string> get_member_keys(poly_thread thread,
                                         poly_context context,
                                         poly_value object);

/**
 * Converts a string into a polyglot string
 */
poly_value poly_string(poly_thread thread, poly_context context,
                       std::string_view data);

/**
 * Converts a polyglot string into a C++ string
 */
std::string to_string(poly_thread thread, poly_value obj);

/**
 * Converts a polyglot string into a C++ int64_t
 */
int64_t to_int(poly_thread thread, poly_value obj);

/**
 * Converts a polyglot string into a C++ double
 */
double to_double(poly_thread thread, poly_value obj);

/**
 * Converts a polyglot string into a C++ double
 */
double to_boolean(poly_thread thread, poly_value obj);

/**
 * Converts a bool into a polyglot boolean
 */
poly_value poly_bool(poly_thread thread, poly_context context, bool value);

/**
 * Converts a bool into a polyglot boolean
 */
poly_value poly_uint(poly_thread thread, poly_context context, uint64_t value);

/**
 * Converts a bool into a polyglot boolean
 */
poly_value poly_int(poly_thread thread, poly_context context, int64_t value);

/**
 * Converts a double into a polyglot double
 */
poly_value poly_double(poly_thread thread, poly_context context, double value);

/**
 * Returns a polyglot null value
 */
poly_value poly_null(poly_thread thread, poly_context context);

/**
 * Converts an array of poly_values into a polyglot array
 */
poly_value poly_array(poly_thread thread, poly_context context,
                      const std::vector<poly_value> &values);

/**
 * Retrieves a member identified with name from the given object
 */
poly_value get_member(poly_thread thread, poly_value object,
                      const std::string &name);

/**
 * Returns true if the given value is executable
 */
bool is_executable(poly_thread thread, poly_value object);

/**
 * The integration of the PolyglotAPI is mostly centered in the registration of
 * C++ callbacks to implement the functionality for the different proxy objects.
 * Whenever an exception is reported from any of these callbacks, a
 * CallbackException object is created in the PolyglotAPI.
 *
 * This function is to report a callback exception from C++.
 */
void throw_callback_exception(poly_thread thread, const char *error);

/**
 * Generic handler to be used with functions that interact with the polyglot
 * library, this is:
 *
 * - Require the language instance for the execution
 * - Receive a vector of polyglot values as arguments
 * - Return a polyglot value as result
 */
template <typename Target, typename Config>
static poly_value polyglot_handler_fixed_args(poly_thread thread,
                                              poly_callback_info args) {
  std::vector<poly_value> argv;
  void *data = nullptr;
  poly_value value = nullptr;
  try {
    if (get_args_and_data(thread, args, Config::name, &data, Config::argc,
                          &argv)) {
      assert(data);
      const auto instance = static_cast<Target *>(data);
      const auto language = instance->language();
      try {
        value = (instance->*Config::callback)(argv);
      } catch (const Polyglot_error &exc) {
        language->throw_exception_object(exc);
      }
    }
  } catch (const std::exception &e) {
    throw_callback_exception(thread, e.what());
  }
  return value;
}

/**
 * Generic handler to be used with pure native functions, no interaction with
 * the polyglot library is done:
 *
 * - Receive a vector of Values as arguments
 * - Return a Value as result
 */
template <typename Target, typename Config>
static poly_value native_handler_fixed_args(poly_thread thread,
                                            poly_callback_info args) {
  std::vector<poly_value> argv;
  void *data = nullptr;
  poly_value value = nullptr;
  try {
    if (get_args_and_data(thread, args, Config::name, &data, Config::argc,
                          &argv)) {
      assert(data);
      const auto instance = static_cast<Target *>(data);
      const auto language = instance->language();
      try {
        value = language->convert(
            (instance->*Config::callback)(language->convert_args(argv)));
      } catch (const Polyglot_error &exc) {
        language->throw_exception_object(exc);
      }
    }
  } catch (const std::exception &e) {
    throw_callback_exception(thread, e.what());
  }
  return value;
}

/**
 * Generic handler to be used with functions that interact with the polyglot
 * library:
 *
 * - Receive no arguments
 * - Return a poly_value as result
 */
template <typename Target, typename Config>
static poly_value polyglot_handler_no_args(poly_thread thread,
                                           poly_callback_info args) {
  void *data = nullptr;
  poly_value value = nullptr;

  if (get_data(thread, args, Config::name, &data)) {
    assert(data);
    const auto instance = static_cast<Target *>(data);
    const auto language = instance->language();
    try {
      value = (instance->*Config::callback)();
    } catch (const Polyglot_error &exc) {
      language->throw_exception_object(exc);
    } catch (const std::exception &e) {
      throw_callback_exception(thread, e.what());
    }
  }

  return value;
}

/**
 * Generic handler to be used with pure native functions, no interaction with
 * the polyglot library is done:
 *
 * - Receive no arguments
 * - Return a Value as result
 */
template <typename Target, typename Config>
static poly_value native_handler_no_args(poly_thread thread,
                                         poly_callback_info args) {
  void *data = nullptr;
  poly_value value = nullptr;

  if (get_data(thread, args, Config::name, &data)) {
    assert(data);
    const auto instance = static_cast<Target *>(data);
    const auto language = instance->language();
    try {
      value = language->convert((instance->*Config::callback)());
    } catch (const Polyglot_error &exc) {
      language->throw_exception_object(exc);
    } catch (const std::exception &e) {
      throw_callback_exception(thread, e.what());
    }
  }

  return value;
}

/**
 * Generic handler to be used with pure native functions, no interaction with
 * the polyglot library is done:
 *
 * - Receive a vector of Values as arguments
 * - Return a Value as result
 */
template <typename Target, typename Config>
static poly_value native_handler_variable_args(poly_thread thread,
                                               poly_callback_info args) {
  std::vector<poly_value> argv;
  void *data = nullptr;
  poly_value value = nullptr;

  try {
    parse_callback_args(thread, args, &argv, &data);
    assert(data);
    const auto instance = static_cast<Target *>(data);
    const auto language = instance->language();

    try {
      value = language->convert(
          (instance->*Config::callback)(language->convert_args(argv)));
    } catch (const Polyglot_error &exc) {
      language->throw_exception_object(exc);
    }

  } catch (const std::exception &e) {
    throw_callback_exception(thread, e.what());
  }

  return value;
}

bool get_member(poly_thread thread, poly_value object, const char *name,
                poly_value *member);
bool is_object(poly_thread thread, poly_value object, std::string *class_name);

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_UTILS_POLYGLOT_UTILS_H_
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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_NATIVE_WRAPPER_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_NATIVE_WRAPPER_

#include <cassert>
#include <concepts>
#include <memory>
#include <type_traits>

#include "utils/polyglot_api_clean.h"

#include "languages/polyglot_language.h"
#include "mysqlrouter/jit_executor_exceptions.h"
#include "native_wrappers/polyglot_collectable.h"
#include "utils/polyglot_error.h"

namespace shcore {
namespace polyglot {

class Polyglot_language;

/**
 * Exposure of C++ objects to the polyglot library is done through the usage of
 * Proxy objects provided by the polyglot library. Those proxy objects implement
 * the needed operations to treat a C++ object as a standard polyglot object,
 * each of those operations require a C++ callback for the execution.
 *
 * In the C++ side it is needed to create the callbacks required by the proxy
 * objects in the polyglot library, for each C++ object type to be supported
 * there's a wrapper class which provides the needed callbacks.
 *
 * When a C++ object is given to the polyglot library, 2 things are needed:
 *
 * - The registrations of the C++ callbacks
 * - The identification of the target C++ instance to be used on the callbacks
 *
 * Each object wrapper provides the above. For the second item, a Collectable
 * object is created and given to the polyglot proxy object. This Collectable
 * object contains the necessary information to identify the target C++ object
 * on which the operation will be executed.
 *
 * This class holds general callback function to be used to release the created
 * Collectable objects once the Proxy object is garbage collected in the
 * Polyglot library.
 */
template <typename T, Collectable_type t>
class Polyglot_native_wrapper {
 public:
  using Collectable_t = Collectable<T, t>;
  using Native_ptr = std::shared_ptr<T>;

  Polyglot_native_wrapper() = delete;

  explicit Polyglot_native_wrapper(std::weak_ptr<Polyglot_language> language)
      : m_language(std::move(language)) {}

  Polyglot_native_wrapper(const Polyglot_native_wrapper &) = delete;
  Polyglot_native_wrapper &operator=(const Polyglot_native_wrapper &) = delete;

  Polyglot_native_wrapper(Polyglot_native_wrapper &&) = delete;
  Polyglot_native_wrapper &operator=(Polyglot_native_wrapper &&) = delete;

  virtual ~Polyglot_native_wrapper() = default;

  poly_value wrap(const Native_ptr &native_value) const {
    auto collectable =
        std::make_unique<Collectable_t>(native_value, m_language);
    auto language = collectable->language();

    auto value = create_wrapper(language->thread(), language->context(),
                                collectable.get());

    collectable->registry()->add(collectable.release());

    return value;
  }

  static bool unwrap(poly_thread thread, poly_value value,
                     Native_ptr *ret_object) {
    assert(ret_object);

    void *data = nullptr;

    if (!is_native_type(thread, value, t, &data)) {
      return false;
    }

    assert(data);

    const auto collectable = static_cast<Collectable_t *>(data);

    if (const auto &target_native = collectable->data()) {
      *ret_object = target_native;
    }

    return true;
  }

 protected:
  static poly_value handler_release_collectable(poly_thread thread,
                                                poly_callback_info args) {
    void *data = nullptr;

    if (get_data(thread, args, "destroy", &data)) {
      assert(data);
      const auto collectable = static_cast<ICollectable *>(data);

      // TODO(rennox): Safety code, found cases where registry is gone,
      // investigate why?
      auto registry = collectable->registry();
      if (registry) {
        registry->remove(collectable);
      }
    }

    return nullptr;
  }

  virtual poly_value create_wrapper(poly_thread thread, poly_context context,
                                    ICollectable *collectable) const = 0;

  /**
   * Generic handler to be used with pure native functions, no interaction with
   * the polyglot library is done:
   *
   * - Receive no arguments
   * - Return a Value as result
   */
  template <typename Config>
  static poly_value native_handler_no_args(poly_thread thread,
                                           poly_callback_info args) {
    void *data = nullptr;
    poly_value value = nullptr;
    if (get_data(thread, args, Config::name, &data)) {
      assert(data);
      const auto collectable = static_cast<Collectable_t *>(data);
      const auto language = collectable->language();
      try {
        value = language->convert(Config::callback(collectable->data()));
      } catch (const Polyglot_error &exc) {
        language->throw_exception_object(exc);
      } catch (const Jit_executor_exception &exc) {
        language->throw_jit_executor_exception(exc);
      } catch (const std::exception &e) {
        throw_callback_exception(thread, e.what());
      }
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
  template <typename Config>
  static poly_value polyglot_handler_no_args(poly_thread thread,
                                             poly_callback_info args) {
    void *data = nullptr;
    poly_value value = nullptr;
    if (get_data(thread, args, Config::name, &data)) {
      assert(data);
      const auto collectable = static_cast<Collectable_t *>(data);
      const auto language = collectable->language();
      try {
        value = Config::callback(language, collectable->data());
      } catch (const Polyglot_error &exc) {
        language->throw_exception_object(exc);
      } catch (const Jit_executor_exception &exc) {
        language->throw_jit_executor_exception(exc);
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
  template <typename Config>
  static poly_value native_handler_fixed_args(poly_thread thread,
                                              poly_callback_info args) {
    std::vector<poly_value> argv;
    void *data = nullptr;
    poly_value value = nullptr;
    try {
      if (get_args_and_data(thread, args, Config::name, &data, Config::argc,
                            &argv)) {
        assert(data);
        const auto collectable = static_cast<Collectable_t *>(data);
        const auto language = collectable->language();
        try {
          value = language->convert(Config::callback(
              collectable->data(), language->convert_args(argv)));
        } catch (const Polyglot_error &exc) {
          language->throw_exception_object(exc);
        } catch (const Jit_executor_exception &exc) {
          language->throw_jit_executor_exception(exc);
        }
      }
    } catch (const std::exception &e) {
      throw_callback_exception(thread, e.what());
    }
    return value;
  }

  /**
   * Generic handler to be used with functions that interact with the polyglot
   * library, this is:
   *
   * - Require the language instance for the execution
   * - Receive a vector of polyglot values as arguments
   * - Return a polyglot value as result
   */
  template <typename Config>
  static poly_value polyglot_handler_fixed_args(poly_thread thread,
                                                poly_callback_info args) {
    std::vector<poly_value> argv;
    void *data = nullptr;
    poly_value value = nullptr;
    try {
      if (get_args_and_data(thread, args, Config::name, &data, Config::argc,
                            &argv)) {
        assert(data);
        const auto collectable = static_cast<Collectable_t *>(data);
        const auto language = collectable->language();
        try {
          value = Config::callback(language, collectable->data(), argv);
        } catch (const Polyglot_error &exc) {
          language->throw_exception_object(exc);
        } catch (const Jit_executor_exception &exc) {
          language->throw_jit_executor_exception(exc);
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
  template <typename Config>
  static poly_value native_handler_variable_args(poly_thread thread,
                                                 poly_callback_info args) {
    std::vector<poly_value> argv;
    void *data = nullptr;
    poly_value value = nullptr;
    std::shared_ptr<Polyglot_language> language = {};

    try {
      parse_callback_args(thread, args, &argv, &data);
      assert(data);
      const auto collectable = static_cast<Collectable_t *>(data);

      // Since the polyglot library will not raise an interrupted exception when
      // calling native code, we have to handle that here
      // bool interrupted = false;
      // Interrupt_handler handler([&]() {
      //   interrupted = true;
      //   return true;
      // });

      language = collectable->language();
      const auto v =
          Config::callback(collectable->data(), language->convert_args(argv));

      // if (interrupted) {
      //   throw std::runtime_error(k_interrupted_error);
      // }

      value = language->convert(v);
    } catch (const Polyglot_error &exc) {
      language->throw_exception_object(exc);
    } catch (const Jit_executor_exception &exc) {
      language->throw_jit_executor_exception(exc);
    } catch (const std::exception &e) {
      throw_callback_exception(thread, e.what());
    }

    return value;
  }

  std::weak_ptr<Polyglot_language> m_language;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_NATIVE_WRAPPER_

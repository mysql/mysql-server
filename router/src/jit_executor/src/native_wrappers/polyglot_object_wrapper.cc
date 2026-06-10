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

#include "native_wrappers/polyglot_object_wrapper.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utils/polyglot_api_clean.h"

// #include "mysqlshdk/include/shellcore/interrupt_handler.h"
// #include "mysqlshdk/libs/db/session.h"  // mysqlshdk::db::Error
// #include "mysqlshdk/libs/utils/debug.h"
#include "languages/polyglot_language.h"
#include "native_wrappers/polyglot_collectable.h"
#include "native_wrappers/polyglot_iterator_wrapper.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"

namespace shcore {
namespace polyglot {

namespace {

class Object_iterator : public IPolyglot_iterator {
 public:
  Object_iterator(std::vector<std::string> &&members)
      : m_members(std::move(members)),
        m_index{m_members.begin()},
        m_end(m_members.end()) {}

  ~Object_iterator() override = default;

  bool has_next() const override { return m_index != m_end; }

  shcore::Value get_next() override {
    shcore::Array_t item;
    if (m_index != m_end) {
      item = shcore::make_array();
      item->push_back(shcore::Value(*m_index));
      m_index++;
    }

    return shcore::Value(item);
  }

 private:
  std::vector<std::string> m_members;
  std::vector<std::string>::iterator m_index;
  std::vector<std::string>::iterator m_end;
};

struct Get_member_keys {
  static constexpr const char *name = "getMemberKeys";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_object_wrapper::Native_ptr &object) {
    auto member_list = object->get_members();

    auto last = std::unique(member_list.begin(), member_list.end());
    member_list.erase(last, member_list.end());

    std::vector<poly_value> members;

    for (const auto &member : member_list) {
      members.push_back(
          poly_string(language->thread(), language->context(), member));
    }

    return poly_array(language->thread(), language->context(), members);
  }
};

struct Has_member {
  static constexpr const char *name = "hasMember";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_object_wrapper::Native_ptr &object,
                        const Argument_list &argv) {
    bool result = object->has_member(argv[0].as_string());

    if (!result && object->is_indexed()) {
      try {
        result = argv[0].as_uint() < static_cast<uint64_t>(object->length());
      } catch (...) {
        // NO-OP: was not a number, so no indexed access
      }
    }

    return Value(result);
  }
};

struct Get_member {
  static constexpr const char *name = "getMember";
  static constexpr std::size_t argc = 1;
  static poly_value callback(const std::shared_ptr<Polyglot_language> &language,
                             const Polyglot_object_wrapper::Native_ptr &object,
                             const std::vector<poly_value> &argv) {
    shcore::Value identifier = language->convert(argv[0]);
    std::string prop = identifier.as_string();

    if (object->has_method(prop)) {
      return Polyglot_method_wrapper(language).wrap(
          std::make_shared<Object_method>(object, prop));
    } else if (object->has_member(prop)) {
      return language->convert(object->get_member(prop));
    } else if (object->is_indexed()) {
      try {
        auto index = static_cast<size_t>(identifier.as_uint());
        if (index < object->length()) {
          return language->convert(object->get_member(index));
        }
      } catch (...) {
        // NO-OP: was not a number, so no indexed access
      }
    }

    return language->undefined();
  }
};

struct Put_member {
  static constexpr const char *name = "putMember";
  static constexpr std::size_t argc = 2;
  static Value callback(const Polyglot_object_wrapper::Native_ptr &object,
                        const Argument_list &argv) {
    object->set_member(argv[0].as_string(), argv[1]);
    return {};
  }
};

struct Get_iterator {
  static constexpr const char *name = "getIterator";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_object_wrapper::Native_ptr &object) {
    return Polyglot_iterator_wrapper(language).wrap(
        std::make_shared<Object_iterator>(object->get_members()));
  }
};

struct Method_call {
  static Value callback(
      const Polyglot_method_wrapper::Native_ptr &object_method,
      const shcore::Argument_list &argv) {
    return object_method->object()->call(object_method->method(), argv);
  }
};

}  // namespace

Polyglot_object_wrapper::Polyglot_object_wrapper(
    std::weak_ptr<Polyglot_language> language, bool indexed)
    : Polyglot_native_wrapper(std::move(language)), m_indexed(indexed) {}

poly_value Polyglot_object_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value poly_object;
  if (m_indexed) {
    throw_if_error(
        poly_create_proxy_iterable_object, thread, context, collectable,
        &Polyglot_object_wrapper::polyglot_handler_no_args<Get_member_keys>,
        &Polyglot_object_wrapper::native_handler_fixed_args<Has_member>,
        &Polyglot_object_wrapper::native_handler_fixed_args<Put_member>,
        &Polyglot_object_wrapper::polyglot_handler_fixed_args<Get_member>,
        nullptr,  // no remove member handler,
        &Polyglot_object_wrapper::polyglot_handler_no_args<Get_iterator>,
        &Polyglot_object_wrapper::handler_release_collectable, &poly_object);
  } else {
    throw_if_error(
        poly_create_proxy_object, thread, context, collectable,
        &Polyglot_object_wrapper::polyglot_handler_no_args<Get_member_keys>,
        &Polyglot_object_wrapper::native_handler_fixed_args<Has_member>,
        &Polyglot_object_wrapper::native_handler_fixed_args<Put_member>,
        &Polyglot_object_wrapper::polyglot_handler_fixed_args<Get_member>,
        nullptr,  // no remove member handler,
        &Polyglot_object_wrapper::handler_release_collectable, &poly_object);
  }

  return poly_object;
}

Polyglot_method_wrapper::Polyglot_method_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_method_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value function;
  throw_if_error(
      poly_create_proxy_function, thread, context,
      &Polyglot_method_wrapper::native_handler_variable_args<Method_call>,
      &Polyglot_method_wrapper::handler_release_collectable, collectable,
      &function);

  return function;
}

}  // namespace polyglot
}  // namespace shcore

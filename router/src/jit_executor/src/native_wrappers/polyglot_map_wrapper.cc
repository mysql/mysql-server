/*
 * Copyright (c) 2014, 2026, Oracle and/or its affiliates.
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

#include "native_wrappers/polyglot_map_wrapper.h"

#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

#include "languages/polyglot_language.h"
#include "native_wrappers/polyglot_collectable.h"
#include "native_wrappers/polyglot_iterator_wrapper.h"
#include "utils/polyglot_error.h"
#include "utils/polyglot_utils.h"

namespace shcore {
namespace polyglot {

namespace {

class Map_iterator : public IPolyglot_iterator {
 public:
  Map_iterator(Value::Map_type::iterator begin, Value::Map_type::iterator end)
      : m_index(begin), m_end(end) {}

  ~Map_iterator() override = default;

  bool has_next() const override { return m_index != m_end; }

  shcore::Value get_next() override {
    shcore::Array_t item;
    if (m_index != m_end) {
      item = shcore::make_array();
      item->push_back(shcore::Value(m_index->first));
      item->push_back(m_index->second);
      m_index++;
    }

    return shcore::Value(std::move(item));
  }

 private:
  Value::Map_type::iterator m_index;
  Value::Map_type::iterator m_end;
};

struct Get_member_keys {
  static constexpr const char *name = "getMemberKeys";
  static poly_value callback(const std::shared_ptr<Polyglot_language> &language,
                             const Polyglot_map_wrapper::Native_ptr &map) {
    std::vector<poly_value> keys;
    for (const auto &m : (*map)) {
      keys.push_back(
          poly_string(language->thread(), language->context(), m.first));
    }

    return poly_array(language->thread(), language->context(), keys);
  }
};

struct Has_member {
  static constexpr const char *name = "hasMember";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_map_wrapper::Native_ptr &map,
                        const Argument_list &argv) {
    return Value(map->has_key(argv[0].as_string()));
  }
};

struct Get_member {
  static constexpr const char *name = "getMember";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_map_wrapper::Native_ptr &map,
                        const Argument_list &argv) {
    auto member = argv[0].as_string();
    if (map->has_key(member)) {
      return map->at(member);

    } else {
      return {};
    }
  }
};

struct Put_member {
  static constexpr const char *name = "putMember";
  static constexpr std::size_t argc = 2;
  static Value callback(const Polyglot_map_wrapper::Native_ptr &map,
                        const Argument_list &argv) {
    auto member = argv[0].as_string();
    (*map)[member] = argv[1];

    return {};
  }
};

struct Remove {
  static constexpr const char *name = "remove";
  static constexpr std::size_t argc = 1;
  static Value callback(const Polyglot_map_wrapper::Native_ptr &map,
                        const Argument_list &argv) {
    bool was_removed = false;

    if (const auto it = map->find(argv[0].as_string()); it != map->end()) {
      map->erase(it);
      was_removed = true;
    }

    return Value(was_removed);
  }
};

struct Get_iterator {
  static constexpr const char *name = "getIterator";
  static poly_value callback(const std::shared_ptr<Polyglot_language> &language,
                             const Polyglot_map_wrapper::Native_ptr &map) {
    return Polyglot_iterator_wrapper(language).wrap(
        std::make_shared<Map_iterator>(map->begin(), map->end()));
  }
};

}  // namespace

Polyglot_map_wrapper::Polyglot_map_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_map_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value poly_map;
  throw_if_error(
      poly_create_proxy_iterable_object, thread, context, collectable,
      &Polyglot_map_wrapper::polyglot_handler_no_args<Get_member_keys>,
      &Polyglot_map_wrapper::native_handler_fixed_args<Has_member>,
      &Polyglot_map_wrapper::native_handler_fixed_args<Put_member>,
      &Polyglot_map_wrapper::native_handler_fixed_args<Get_member>,
      &Polyglot_map_wrapper::native_handler_fixed_args<Remove>,
      &Polyglot_map_wrapper::polyglot_handler_no_args<Get_iterator>,
      &Polyglot_map_wrapper::handler_release_collectable, &poly_map);

  return poly_map;
}

}  // namespace polyglot
}  // namespace shcore

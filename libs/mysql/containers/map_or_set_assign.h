// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CONTAINERS_MAP_OR_SET_ASSIGN_H
#define MYSQL_CONTAINERS_MAP_OR_SET_ASSIGN_H

/// @file
/// Experimental API header

#include "mysql/ranges/meta.h"           // Iterator_value_type
#include "mysql/utils/call_and_catch.h"  // call_and_catch

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers {

/// For a `node_handle` retrieved from the `extract` member of one of the
/// `std::[unordered_][multi]{set|map}` containers, and an iterator over a
/// container with the same value_type, copy the value pointed to by the
/// iterator to the node_handle.
///
/// The purpose is to provide a uniform API, since node_handle objects of sets
/// and maps are different.
///
/// @param node_handle Node handle to copy to.
///
/// @param iterator Iterator to the value to copy from.
void node_handle_assign(auto &node_handle, auto &iterator) noexcept {
  // True for set-like types (set, multiset), false for map-like types (map,
  // multimap).
  constexpr bool has_value_member = requires { node_handle.value(); };

  if constexpr (has_value_member) {
    node_handle.value() = *iterator;
  } else {
    node_handle.key() = iterator->first;
    node_handle.mapped() = iterator->second;
  }
}

/// Return a new object of the same type as the parameter, without any elements,
/// with the same allocator, and if the object type has key comparison, the same
/// comparison object.
template <class Map_or_set_t>
auto make_empty_map_or_set_and_copy_metadata(const Map_or_set_t &map_or_set) {
  constexpr bool has_key_comparison_member =
      requires { map_or_set.key_comp(); };
  if constexpr (has_key_comparison_member) {
    // For (ordered) [multi]{map|set}, copy the key comparison function
    // object and the allocator.
    return Map_or_set_t(map_or_set.key_comp(), map_or_set.get_allocator());
  } else {
    // For unordered_[multi]{map|set}, there is no key comparison function
    // object. Copy just the allocator.
    return Map_or_set_t(map_or_set.get_allocator());
  }
}

}  // namespace mysql::containers

namespace mysql::containers::throwing {

/// Replace the contents of `container` with that of the range given by the two
/// iterators, minimizing memory allocations.
///
/// This reuses existing nodes of the target container as far as possible, and
/// allocates new ones only if the source has more nodes than the target
/// container.
///
/// It does not copy the allocator.
///
/// Informally, this is to std::map what std::vector:assign is to std::vector.
///
/// @param map_or_set Container to overwrite.
///
/// @param first Iterator to first element of new contents.
///
/// @param last Iterator to last element of new contents.
///
/// @throws std::bad_alloc Out of memory. This can only occur if the
/// source range has more elements than the container.
template <class Map_or_set_t, class Source_iterator_t>
  requires std::convertible_to<
      mysql::ranges::Iterator_value_type<Source_iterator_t>,
      mysql::ranges::Range_value_type<Map_or_set_t>>
void map_or_set_assign(Map_or_set_t &map_or_set, const Source_iterator_t &first,
                       const std::sentinel_for<Source_iterator_t> auto &last) {
  Map_or_set_t tmp = make_empty_map_or_set_and_copy_metadata(map_or_set);
  auto it = first;

  // Reuse nodes by moving them one by one from map_or_set to tmp.
  for (; it != last && !map_or_set.empty(); ++it) {
    auto node_handle = map_or_set.extract(map_or_set.begin());
    mysql::containers::node_handle_assign(node_handle, it);
    tmp.insert(tmp.cend(), std::move(node_handle));
  }

  // Replace map_or_set by the result.
  map_or_set = std::move(tmp);

  // Allocate any remaining nodes.
  for (; it != last; ++it) {
    map_or_set.insert(map_or_set.cend(), *it);
  }
}

}  // namespace mysql::containers::throwing

namespace mysql::containers {

/// Replace the contents of `container` with that of the range given by the two
/// iterators, minimizing memory allocations.
///
/// This reuses existing nodes of the target container as far as possible, and
/// allocates new ones only if the source has more nodes than the target
/// container.
///
/// It does not copy the allocator.
///
/// Informally, this is to std::map what std::vector:assign is to std::vector.
///
/// @param map_or_set Container to overwrite.
///
/// @param first Iterator to first element of new contents.
///
/// @param last Iterator to last element of new contents.
///
/// @retval Return_status::ok Success
///
/// @retval Return_status::error Out of memory. This can only occur if the
/// source range has more elements than the container.
template <class Source_iterator_t>
[[nodiscard]] auto map_or_set_assign(
    auto &map_or_set, const Source_iterator_t &first,
    const std::sentinel_for<Source_iterator_t> auto &last) noexcept {
  return mysql::utils::call_and_catch(
      [&] { throwing::map_or_set_assign(map_or_set, first, last); });
}

}  // namespace mysql::containers

// addtogroup GroupLibsMysqlContainers
/// @}

#endif  // ifndef MYSQL_CONTAINERS_MAP_OR_SET_ASSIGN_H

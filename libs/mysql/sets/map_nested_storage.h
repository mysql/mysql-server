// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_SETS_MAP_NESTED_STORAGE_H
#define MYSQL_SETS_MAP_NESTED_STORAGE_H

/// @file
/// Experimental API header

#include <concepts>                                    // same_as
#include <iterator>                                    // next
#include <map>                                         // map
#include "mysql/allocators/allocator.h"                // Allocator
#include "mysql/allocators/memory_resource.h"          // Memory_resource
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/containers/map_or_set_assign.h"        // map_or_set_assign
#include "mysql/iterators/meta.h"  // Is_declared_legacy_bidirectional_iterator
#include "mysql/ranges/meta.h"     // Range_iterator_type
#include "mysql/sets/nested_set_meta.h"              // Is_nested_set_traits
#include "mysql/sets/upper_lower_bound_interface.h"  // Upper_lower_bound_interface
#include "mysql/utils/call_and_catch.h"              // call_and_catch
#include "mysql/utils/return_status.h"               // Return_status

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Storage for nested sets, backed by a std::map.
///
/// @tparam Set_traits_tp Nested set traits.
///
/// @tparam Map_tp Type of map. This should have an API similar to std::map,
/// with key type equal to Set_traits_tp::Key_traits_t::Element_t and value type
/// a Set with category Set_traits_tp::Mapped_category_t and traits
/// Set_traits_tp::Mapped_traits_t.
template <Is_nested_set_traits Set_traits_tp, class Map_tp>
class Map_nested_storage
    : public mysql::containers::Basic_container_wrapper<
          Map_nested_storage<Set_traits_tp, Map_tp>, Map_tp>,
      public Upper_lower_bound_interface<
          Map_nested_storage<Set_traits_tp, Map_tp>,
          typename Set_traits_tp::Key_traits_t,
          mysql::ranges::Range_iterator_type<Map_tp>,
          mysql::ranges::Range_const_iterator_type<Map_tp>,
          Iterator_get_first> {
 public:
  using Set_traits_t = Set_traits_tp;
  using Map_t = Map_tp;
  using This_t = Map_nested_storage<Set_traits_t, Map_t>;
  using Basic_container_wrapper_t =
      mysql::containers::Basic_container_wrapper<This_t, Map_t>;

  using Key_traits_t = typename Set_traits_t::Key_traits_t;
  using Key_t = typename Key_traits_t::Element_t;
  using Mapped_traits_t = typename Set_traits_t::Mapped_traits_t;
  using Mapped_category_t = typename Set_traits_t::Mapped_category_t;

  using Value_t = mysql::ranges::Range_value_type<Map_t>;
  using Mapped_t = mysql::ranges::Map_mapped_type<Map_t>;
  using Iterator_t = mysql::ranges::Range_iterator_type<Map_t>;
  static_assert(std::bidirectional_iterator<Iterator_t>);
  static_assert(
      mysql::iterators::Is_declared_legacy_bidirectional_iterator<Iterator_t>);
  using Const_iterator_t = mysql::ranges::Range_const_iterator_type<Map_t>;

  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = mysql::allocators::Allocator<Value_t>;

  static_assert(std::same_as<Key_t, mysql::ranges::Map_key_type<Map_t>>);
  static_assert(
      std::same_as<Mapped_category_t, typename mysql::ranges::Map_mapped_type<
                                          Map_t>::Set_category_t>);
  static_assert(std::same_as<
                Mapped_traits_t,
                typename mysql::ranges::Map_mapped_type<Map_t>::Set_traits_t>);

  static_assert(std::same_as<Value_t, std::pair<const Key_t, Mapped_t>>);

  explicit Map_nested_storage(
      const Memory_resource_t &memory_resource = Memory_resource_t{}) noexcept
      : Basic_container_wrapper_t(Allocator_t(memory_resource)) {}

  // use defaults for move/destructor; delete copy
  Map_nested_storage(const This_t &) = delete;
  Map_nested_storage(This_t &&) noexcept = default;
  This_t &operator=(const This_t &) = delete;
  This_t &operator=(This_t &&) noexcept = default;
  ~Map_nested_storage() = default;

  using Basic_container_wrapper_t::assign;

  template <class Iterator_t>
    requires std::same_as<mysql::ranges::Iterator_value_type<Iterator_t>,
                          Mapped_t>
  [[nodiscard]] auto assign(const Iterator_t &it1,
                            const Iterator_t &it2) noexcept {
    return mysql::containers::map_or_set_assign(map(), it1, it2);
  }

  template <class Iterator_t>
  [[nodiscard]] auto assign(const Iterator_t &it1,
                            const Iterator_t &it2) noexcept {
    using mysql::utils::Return_status;
    this->clear();
    for (auto input = it1; input != it2; ++input) {
      auto opt_output = this->emplace(input->first);
      if (!opt_output.has_value()) return Return_status::error;
      auto &output_mapped = opt_output.value()->second;
      auto ret = output_mapped.assign(input->second);
      if (ret == Return_status::error) return ret;
    }
    return Return_status::ok;
  }

  /// @return Non-const reference to the underlying map object.
  [[nodiscard]] auto &map() noexcept { return this->wrapped(); }

  /// @return Const reference to the underlying map object.
  [[nodiscard]] const auto &map() const noexcept { return this->wrapped(); }

  /// @return iterator to the entry with the given key, or end() if there is no
  /// entry for the given key.
  [[nodiscard]] Iterator_t find(const Key_t &key) noexcept {
    auto ret = map().find(key);
    return ret;
  }

  /// @return const iterator to the entry with the given key, or end() if there
  /// is no entry for the given key.
  [[nodiscard]] Const_iterator_t find(const Key_t &key) const noexcept {
    auto ret = map().find(key);
    return ret;
  }

 private:
  /// Common implementation of the const and non-const versions of find(cursor,
  /// key).
  ///
  /// @tparam Iter One of Iterator_t or Const_iterator_t
  ///
  /// @param the_map Reference or const reference to map().
  ///
  /// @param cursor Cursor position.
  ///
  /// @param key Key to search for.
  ///
  /// @return Iterator to element equal to @c key, or end() if none found.
  template <class Iter>
  [[nodiscard]] static Iter do_find(auto &the_map, Iter &cursor,
                                    const Key_t &key) noexcept {
    assert(cursor == the_map.begin() ||
           Key_traits_t::lt(std::ranges::prev(cursor)->first, key));
    // See if the hint allows us to return in constant time.
    if (cursor != the_map.end()) {
      auto order = Key_traits_t::cmp(key, cursor->first);
      if (order == 0) {
        // The key was found at the cursor position. Advance the cursor.
        Iter ret = cursor;
        ++cursor;
        return ret;
      }
      if (order < 0) return the_map.end();
      assert(order > 0);
    }
    // Look up the element in logarithmic time, update cursor, compute return
    // value.
    cursor = the_map.lower_bound(key);
    if (cursor == the_map.end()) {
      // Nothing found, and the cursor has moved to the end of the container.
      return the_map.end();
    }
    if (cursor->first == key) {
      // The key was found after the cursor position. Advance the cursor.
      Iter ret{cursor};
      ++cursor;
      return ret;
    }
    // The key was not found, the cursor position may have advanced.
    return the_map.end();
  }

 public:
  /// @return iterator to the entry with the given key, or end() if there is no
  /// entry for the given key. This uses the given cursor hint to make the
  /// operation constant-time in case key <= cursor->first.
  ///
  /// @param[in,out] cursor Iterator hint. If this is greater than
  /// map::lower_bound(key), the behavior is undefined. It will be updated to
  /// the upper bound for the key. Thus, it is suitable to use in successive
  /// calls to this function with increasing keys.
  ///
  /// @param key The key to find.
  ///
  /// @return Iterator to element equal to key, or end if no such element
  /// exists.
  [[nodiscard]] Iterator_t find(Iterator_t &cursor, const Key_t &key) noexcept {
    return do_find(map(), cursor, key);
  }

  /// @return const iterator to the entry with the given key, or end() if there
  /// is no entry for the given key. This uses the given cursor hint to make the
  /// operation constant-time in case key <= cursor->first.
  ///
  /// @param[in,out] cursor Iterator hint. If this is greater than
  /// map::lower_bound(key), the behavior is undefined. It will be updated to
  /// the upper bound for the key. Thus, it is suitable to use in successive
  /// calls to this function with increasing keys.
  ///
  /// @param key The key to find.
  ///
  /// @return Iterator to element equal to key, or end if no such element
  /// exists.
  [[nodiscard]] Const_iterator_t find(Const_iterator_t &cursor,
                                      const Key_t &key) const noexcept {
    return do_find(map(), cursor, key);
  }

  /// If no entry with the given key exists, insert one using the given cursor
  /// hint, and default-construct the mapped object.
  ///
  /// @param[in,out] cursor Iterator. If this points to  the insertion position,
  /// the insertion is O(1) time. Will be updated to the next element after the
  /// inserted one.
  ///
  /// @param key Key to insert.
  ///
  /// @return std::optional<Iterator_t>, which holds an iterator to the inserted
  /// element if the operation succeeded, and holds no value if an out-of-memory
  /// condition occurred.
  [[nodiscard]] auto emplace(Iterator_t &cursor, const Key_t &key) noexcept {
    auto ret = mysql::utils::call_and_catch([&] {
      return map().try_emplace(cursor, key, this->get_memory_resource());
    });
    if (ret.has_value()) cursor = std::next(ret.value());
    return ret;
  }

  /// If no entry with the given key exists, insert one and default-construct
  /// the mapped object.
  ///
  /// @param key Key to insert.
  ///
  /// @return std::optional<Iterator_t>, which holds an iterator to the inserted
  /// element if the operation succeeded, and holds no value if an out-of-memory
  /// condition occurred.
  [[nodiscard]] auto emplace(const Key_t &key) noexcept {
    return mysql::utils::call_and_catch([&] {
      return map().try_emplace(key, this->get_memory_resource()).first;
    });
  }

  /// If no entry with the given key exists, insert one and default-construct
  /// the mapped object.
  ///
  /// @param position Position before which the element shall be inserted.
  ///
  /// @param source Map_nested_storage from which element shall be stolen.
  ///
  /// @param steal_element Iterator to element in `source` that shall be stolen.
  ///
  /// @return iterator to the inserted element
  ///
  /// This operation cannot fail.
  [[nodiscard]] Iterator_t steal_and_insert(const Iterator_t &position,
                                            This_t &source,
                                            Iterator_t steal_element) noexcept {
    assert(steal_element != source.end());
    auto node = source.map().extract(steal_element);
    return map().insert(position, std::move(node));
  }

  /// Remove the element that the iterator points to.
  ///
  /// @return Iterator to the next element after the removed one.
  Iterator_t erase(const Iterator_t &iterator) { return map().erase(iterator); }

  /// Remove the range of elements from first, inclusive, to last, exclusive.
  ///
  /// @return Iterator to the next element after the removed one.
  Iterator_t erase(const Iterator_t &first, const Iterator_t &last) {
    return map().erase(first, last);
  }

};  // class Map_nested_storage

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_MAP_NESTED_STORAGE_H

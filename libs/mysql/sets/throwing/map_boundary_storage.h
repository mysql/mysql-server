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

#ifndef MYSQL_SETS_THROWING_MAP_BOUNDARY_STORAGE_H
#define MYSQL_SETS_THROWING_MAP_BOUNDARY_STORAGE_H

/// @file
/// Experimental API header
///
/// This file contains the class Map_boundary_storage, which provides storage
/// for a boundary container backed by a std::map.

#include <map>                                 // map
#include "my_compiler.h"                       // MY_COMPILER_DIAGNOSTIC_PUSH
#include "mysql/allocators/allocator.h"        // Allocator
#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/containers/map_or_set_assign.h"        // map_or_set_assign
#include "mysql/iterators/iterator_interface.h"        // Iterator_interface
#include "mysql/iterators/meta.h"  // Is_declared_legacy_bidirectional_iterator
#include "mysql/meta/is_same_ignore_const.h"  // Is_same_ignore_const
#include "mysql/ranges/disjoint_pairs.h"      // make_disjoint_pairs_iterator
#include "mysql/ranges/meta.h"                // Range_iterator_type
#include "mysql/sets/boundary_set_meta.h"     // Is_boundary_storage
#include "mysql/sets/upper_lower_bound_interface.h"  // Upper_lower_bound_interface

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::throwing::detail {

/// Boundary_iterator based on a std::map iterator or const iterator.
template <class Map_iterator_tp, class Mutable_map_iterator_tp = void>
class Map_boundary_iterator
    : public mysql::iterators::Iterator_interface<
          Map_boundary_iterator<Map_iterator_tp, Mutable_map_iterator_tp>> {
  using Map_iterator_t = Map_iterator_tp;
  using Mutable_map_iterator_t = Mutable_map_iterator_tp;

 public:
  /// Default constructor. The resulting iterator is in a "meaningless" state,
  /// where the only allowed operation is to assign to it.
  constexpr Map_boundary_iterator() = default;

  /// Construct an iterator at the given position, and at the given endpoint
  /// state.
  constexpr Map_boundary_iterator(const Map_iterator_t &position,
                                  bool is_endpoint)
      : m_position(position), m_is_endpoint(is_endpoint) {}

  /// Converting constructor from the mutable iterator type, enabled if this is
  /// a const iterator.
  explicit constexpr Map_boundary_iterator(
      const Map_boundary_iterator<Mutable_map_iterator_t> &other)
    requires(!std::same_as<Mutable_map_iterator_t, void>)
      : m_position(other.map_iterator()), m_is_endpoint(other.is_endpoint()) {}

  /// @return the current boundary.
  [[nodiscard]] auto &get() const {
    MY_COMPILER_DIAGNOSTIC_PUSH()
#if __GNUC__ <= 12
    // gcc 11.4.1 (on e.g. solaris11-sparc-64bit) gives a mysterious
    // -Wmaybe-uninitialized warning on the following line. The warning does not
    // appear in 12.2, so likely it was a compiler bug and was fixed. The gcc
    // changelogs between those two versions mention at least one false positive
    // for -Wmaybe-uninitialized,
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78993, so it could be that
    // or something else.
    MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wmaybe-uninitialized")
#endif
    return m_is_endpoint ? m_position->first : m_position->second;
    MY_COMPILER_DIAGNOSTIC_POP()
  }

  /// Move forward one step.
  constexpr void next() {
    if (m_is_endpoint) ++m_position;
    m_is_endpoint = !m_is_endpoint;
  }

  /// Move backward one step.
  constexpr void prev() {
    m_is_endpoint = !m_is_endpoint;
    if (m_is_endpoint) --m_position;
  }

  /// @return true if this object is equal to the other object.
  [[nodiscard]] constexpr bool is_equal(
      const Map_boundary_iterator &other) const {
    return m_position == other.m_position &&
           m_is_endpoint == other.m_is_endpoint;
  }

  /// @return true if this is an endpoint.
  [[nodiscard]] constexpr bool is_endpoint() const { return m_is_endpoint; }

  /// @return the underlying std::map iterator.
  [[nodiscard]] constexpr const Map_iterator_t &map_iterator() const {
    return m_position;
  }

 private:
  Map_iterator_t m_position{};
  bool m_is_endpoint{false};
};  // class Map_boundary_iterator

static_assert(Is_bidirectional_boundary_iterator<
              Map_boundary_iterator<std::map<int, int>::iterator>>);

}  // namespace mysql::sets::throwing::detail

namespace mysql::sets::throwing {

/// Storage for boundary points, backed by a std::map.
///
/// Internally, each interval is represented as a value pair in the map, where
/// the key is the endpoint and the mapped value is the start. (This may appear
/// "reversed". The reason is technical: some operations are faster this way.
/// See comments in @c upper_bound_impl.)
///
/// Insertion is worst-case logarithmic-time, but amortized constant-time if an
/// exact hint can be provided, which is possible when elements are inserted in
/// order. Deletion is linear-time in the number of deleted elements, plus
/// logarithmic-time in the total number of elements. upper_bound/lower_bound is
/// worst-case logarithmic-time, but constant-time if given an exact hint. The
/// iterator category is bidirectional_iterator.
///
/// @tparam Set_traits_tp Bounded set traits describing properties of the
/// element type.
///
/// @tparam Map_tp Class for the map data structure, expected to have an API
/// similar to `std::map<Set_traits_tp::Element_t, Set_traits_tp::Element_t>`.
template <Is_bounded_set_traits Set_traits_tp, class Map_tp>
class Map_boundary_storage
    : public mysql::containers::Basic_container_wrapper<
          Map_boundary_storage<Set_traits_tp, Map_tp>, Map_tp>,
      public mysql::sets::Upper_lower_bound_interface<
          Map_boundary_storage<Set_traits_tp, Map_tp>, Set_traits_tp,
          detail::Map_boundary_iterator<
              mysql::ranges::Range_iterator_type<Map_tp>>,
          detail::Map_boundary_iterator<
              mysql::ranges::Range_const_iterator_type<Map_tp>,
              mysql::ranges::Range_iterator_type<Map_tp>>,
          Iterator_get_value> {
 private:
  struct Make_pair_reverse {
    template <class Type_t>
    static constexpr std::pair<Type_t &, Type_t &> make_pair(Type_t &second,
                                                             Type_t &first) {
      return {first, second};
    }
    template <class Type_t>
    static constexpr std::pair<Type_t, Type_t> make_pair(const Type_t &second,
                                                         const Type_t &first) {
      return {first, second};
    }
  };

  using This_t = Map_boundary_storage<Set_traits_tp, Map_tp>;

 public:
  using Set_traits_t = Set_traits_tp;
  using Map_t = Map_tp;
  using Basic_container_wrapper_t =
      mysql::containers::Basic_container_wrapper<This_t, Map_t>;

  using Element_t = typename Set_traits_t::Element_t;
  using Map_value_t = mysql::ranges::Range_value_type<Map_t>;

  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = mysql::allocators::Allocator<Map_value_t>;

  using Map_iterator_t = mysql::ranges::Range_iterator_type<Map_t>;
  using Map_const_iterator_t = mysql::ranges::Range_const_iterator_type<Map_t>;
  using Iterator_t = detail::Map_boundary_iterator<Map_iterator_t>;
  static_assert(std::bidirectional_iterator<Iterator_t>);
  static_assert(
      mysql::iterators::Is_declared_legacy_bidirectional_iterator<Iterator_t>);
  using Const_iterator_t =
      detail::Map_boundary_iterator<Map_const_iterator_t, Map_iterator_t>;

  static_assert(
      std::same_as<std::pair<const Element_t, Element_t>, Map_value_t>);

  /// Declare that insertion is "fast", i.e., O(log(N)).
  static constexpr bool has_fast_insertion = true;

  /// Construct a new, empty object, with the given Memory_resource.
  explicit Map_boundary_storage(
      const Memory_resource_t &memory_resource = Memory_resource_t()) noexcept
      : Basic_container_wrapper_t(Allocator_t(memory_resource)) {
    static_assert(Is_boundary_storage<This_t>);
  }

  /// Constructor copying from another type, using the given Memory_resource.
  explicit Map_boundary_storage(
      const Is_readable_boundary_storage_over_traits<Set_traits_t> auto &source,
      const Memory_resource_t &memory_resource)
      : Basic_container_wrapper_t(source.begin(), source.end(),
                                  Allocator_t(memory_resource)) {}

  /// Constructor copying both values and Memory_resource from another storage.
  explicit Map_boundary_storage(
      const Is_readable_boundary_storage_over_traits<Set_traits_t> auto &source)
      : Basic_container_wrapper_t(source.begin(), source.end(),
                                  Allocator_t(source.get_memory_resource())) {}

  /// Constructor copying a range defined by the given iterators, using
  /// memory_resource if given, or the default Memory_resource() otherwise.
  template <std::input_iterator First_iterator_t>
  explicit Map_boundary_storage(
      const First_iterator_t &first,
      const std::sentinel_for<First_iterator_t> auto &last,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Basic_container_wrapper_t(
            mysql::ranges::make_disjoint_pairs_iterator<Make_pair_reverse>(
                first),
            mysql::ranges::make_disjoint_pairs_iterator<Make_pair_reverse>(
                last),
            Allocator_t(memory_resource)) {}

  // use defaults for copy/move/destructor
  Map_boundary_storage(const Map_boundary_storage &other) = default;
  Map_boundary_storage(Map_boundary_storage &&other) noexcept = default;
  Map_boundary_storage &operator=(const Map_boundary_storage &other) = default;
  Map_boundary_storage &operator=(Map_boundary_storage &&other) noexcept =
      default;
  ~Map_boundary_storage() = default;

  /// Assignment operator copying from another type, preserving the existing
  /// Memory_resource.
  Map_boundary_storage &operator=(
      const Is_boundary_storage_over_traits<Set_traits_t> auto &source) {
    this->assign(source);
    return *this;
  }

  [[nodiscard]] Map_t &map() { return this->wrapped(); }
  [[nodiscard]] const Map_t &map() const { return this->wrapped(); }

  /// Assign from another type, preserving the existing Memory_resource.
  void assign(const Is_readable_boundary_storage_over_traits<Set_traits_t> auto
                  &other) {
    assign(other.begin(), other.end());
  }

  /// Assign from the range defined by the given iterators, preserving the
  /// existing Memory_resource.
  template <std::input_iterator First_iterator_t>
  void assign(const First_iterator_t &first,
              const std::sentinel_for<First_iterator_t> auto &last) {
    mysql::containers::throwing::map_or_set_assign(
        map(),
        mysql::ranges::make_disjoint_pairs_iterator<Make_pair_reverse>(first),
        mysql::ranges::make_disjoint_pairs_iterator<Make_pair_reverse>(last));
  }

  /// @return const iterator to the beginning.
  [[nodiscard]] auto begin() const {
    return Const_iterator_t(map().begin(), false);
  }

  /// @return const iterator to the end.
  [[nodiscard]] auto end() const {
    return Const_iterator_t(map().end(), false);
  }

  /// @return mutable iterator to the beginning.
  [[nodiscard]] auto begin() { return Iterator_t(map().begin(), false); }

  /// @return mutable iterator to the end.
  [[nodiscard]] auto end() { return Iterator_t(map().end(), false); }

  /// @return the number of boundary points.
  [[nodiscard]] auto size() const { return 2 * map().size(); }

  /// @return true if this object is empty.
  [[nodiscard]] bool empty() const { return map().empty(); }

  /// @return true if this object is nonempty.
  [[nodiscard]] explicit operator bool() const { return (bool)map(); }

  /// @return the upper bound for the given element in the given storage.
  template <mysql::meta::Is_same_ignore_const<This_t> This_arg_t>
  [[nodiscard]] static auto upper_bound_impl(This_arg_t &self,
                                             const Element_t &element) {
    auto map_position = self.map().upper_bound(element);
    if (map_position == self.map().end()) return self.end();
    // map::upper_bound finds an upper bound among *keys* only, not values.
    // Since our keys are endpoints, it is possible that the real upper bound is
    // the start of the same interval. Therefore, we check if the mapped value
    // is an upper bound.
    //
    // This is where it is good that the map is ordered by endpoints. If we
    // would order by start, map::upper_bound would find an upper bound that is
    // a start of an interval, and the real upper bound may be the end of the
    // preceding interval. That would be a little bit more costly (and
    // complicated) to check.
    return mysql::ranges::Range_iterator_type<This_arg_t>(
        map_position, le(map_position->second, element));
  }

  /// @return the lower bound for the given element in the given storage.
  template <mysql::meta::Is_same_ignore_const<This_t> This_arg_t>
  [[nodiscard]] static auto lower_bound_impl(This_arg_t &self,
                                             const Element_t &element) {
    auto map_position = self.map().lower_bound(element);
    if (map_position == self.map().end()) return self.end();
    return mysql::ranges::Range_iterator_type<This_arg_t>(
        map_position, lt(map_position->second, element));
  }

  /// Erase an even-length range of boundary points.
  ///
  /// This invalidates iterators to any removed elements, and also, if `left` is
  /// an endpoint, invalidates iterators to std::prev(left).
  ///
  /// @param left The first boundary point to remove, inclusive.
  ///
  /// @param right The last boundary point to remove, exclusive.
  ///
  /// @return Iterator to the next point after the last removed point.
  ///
  /// @note It is required that left.is_endpoint() == right.is_endpoint().
  [[nodiscard]] Iterator_t erase(const Iterator_t &left,
                                 const Iterator_t &right) {
    assert(left.is_endpoint() == right.is_endpoint());
    assert(std::distance(left, right) % 2 == 0);
    if (left == right || !left.is_endpoint()) {
      // before: [{a, b}, {c, d}, {e, f}], *left==a, *right==e
      // after:  [{e, f}]

      // When left==right, it is tempting to have a special case `return left`.
      // However, left is a const iterator and the return type for this function
      // is non-const. Therefore we call erase with two equal iterators, which
      // is a no-op, and returns the non-const iterator type as needed. Cf.
      // https://stackoverflow.com/questions/765148/how-to-remove-constness-of-const-iterator
      auto interval_iterator =
          map().erase(left.map_iterator(), right.map_iterator());
      return Iterator_t(interval_iterator, left.is_endpoint());
    } else {
      // before: [{a, b}, {c, d}, {e, f}], *left==b, *right==f
      // after:  [{a, f}]
      auto map_iterator = left.map_iterator();
      auto left_start = map_iterator->second;
      map_iterator = map().erase(map_iterator, right.map_iterator());
      map_iterator->second = left_start;
      return Iterator_t(map_iterator, true);
    }
  }

  /// Insert two boundary points by allocating a new element.
  ///
  /// @param position Insertion point.
  ///
  /// @param v1 The first boundary point to insert.
  ///
  /// @param v2 The second boundary point to insert.
  ///
  /// @return Iterator to the next point after the inserted point.
  ///
  /// @throws bad_alloc on OOM.
  [[nodiscard]] Iterator_t insert(const Iterator_t &position,
                                  const Element_t &v1, const Element_t &v2) {
    return do_insert(
        position, v1, v2,
        [this](const auto &map_iterator, const auto &e1, const auto &e2) {
          this->map().emplace_hint(map_iterator, e1, e2);
        });
  }

  /// Insert two boundary points, stealing the first element from given source
  /// object (which must be nonempty).
  ///
  /// @param position Insertion point.
  ///
  /// @param v1 The first boundary point to insert.
  ///
  /// @param v2 The second boundary point to insert.
  ///
  /// @param source other Map_boundary_storage from which we may steal an
  /// element.
  ///
  /// @return Iterator to the next point after the inserted point.
  [[nodiscard]] Iterator_t steal_and_insert(const Iterator_t &position,
                                            const Element_t &v1,
                                            const Element_t &v2,
                                            This_t &source) noexcept {
    return do_insert(position, v1, v2,
                     [this, &source](const auto &map_iterator, const auto &e1,
                                     const auto &e2) {
                       auto node_handle =
                           source.map().extract(source.map().begin());
                       node_handle.key() = e1;
                       node_handle.mapped() = e2;
                       this->map().insert(map_iterator, std::move(node_handle));
                     });
  }

  /// Modify the boundary that the iterator points to.
  ///
  /// @param position Element to update.
  ///
  /// @param element New value.
  ///
  /// @return iterator to the next element. Note that this is different from
  /// `position`, and that `position` has been invalidated, in case
  /// `position.is_endpoint()==true`.
  [[nodiscard]] Iterator_t update_point(const Iterator_t &position,
                                        const Element_t &element) {
    assert(position == begin() || lt(*std::prev(position), element));
    assert(position == end() || std::next(position) == end() ||
           lt(element, *std::next(position)));
    auto map_iterator = position.map_iterator();
    if (position.is_endpoint()) {
      // before: [{a, b}], *position==b
      // after:  [{a, element}]
      auto next_map_iterator = std::next(map_iterator);
      Map_const_iterator_t map_const_iterator = map_iterator;
      auto node_handle = map().extract(map_const_iterator);
      node_handle.key() = element;
      auto it = map().insert(next_map_iterator, std::move(node_handle));
      return Iterator_t(std::next(it), false);
    } else {
      // before: [{a, b}], *position==a
      // after:  [{element, b}]
      map_iterator->second = element;
      return Iterator_t(map_iterator, true);
    }
  }

 private:
  /// Insert two boundary points. This is the common worker function for the
  /// three-argument and four-argument public insert functions.
  ///
  /// @param position Insertion point.
  ///
  /// @param v1 The first boundary point to insert.
  ///
  /// @param v2 The second boundary point to insert.
  ///
  /// @param inserter Callable that will insert two boundary points with a given
  /// iterator hint.
  ///
  /// @return Iterator to the next point after the inserted point.
  [[nodiscard]] Iterator_t do_insert(const Iterator_t &position,
                                     const Element_t &v1, const Element_t &v2,
                                     const auto &inserter) {
    // Verify the position is correct: prev(position) < v1 < v2 < position
    assert(position == begin() || lt(*std::prev(position), v1));
    assert(lt(v1, v2));
    assert(position == end() || lt(v2, *position));
    auto map_iterator = position.map_iterator();
    if (position.is_endpoint()) {
      // before: [{a, b}], *position==b
      // after:  [{a, v1}, {v2, b}]
      auto &element_a = map_iterator->second;
      inserter(map_iterator, v1, element_a);
      element_a = v2;
    } else {
      // before: [{a, b}], *position==a
      // after:  [{v1, v2}, {a, b}]
      inserter(map_iterator, v2, v1);
    }
    return position;
  }

  /// Return true if "left < right", according to the order defined by
  /// Set_traits_t.
  [[nodiscard]] static constexpr bool lt(const Element_t &left,
                                         const Element_t &right) {
    return Set_traits_t::lt(left, right);
  }

  /// Return true if "left <= right", according to the order defined by
  /// Set_traits_t.
  [[nodiscard]] static constexpr bool le(const Element_t &left,
                                         const Element_t &right) {
    return Set_traits_t::le(left, right);
  }
};  // class Map_boundary_storage

}  // namespace mysql::sets::throwing

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_THROWING_MAP_BOUNDARY_STORAGE_H

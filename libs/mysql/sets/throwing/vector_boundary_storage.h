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

#ifndef MYSQL_SETS_THROWING_VECTOR_BOUNDARY_STORAGE_H
#define MYSQL_SETS_THROWING_VECTOR_BOUNDARY_STORAGE_H

/// @file
/// Experimental API header
///
/// This file contains the class Vector_boundary_storage, which provides storage
/// for a boundary container backed by a std::vector.

#include <algorithm>                                   // upper_bound
#include <vector>                                      // vector
#include "mysql/allocators/allocator.h"                // Allocator
#include "mysql/allocators/memory_resource.h"          // Memory_resource
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/iterators/iterator_interface.h"        // Iterator_interface
#include "mysql/iterators/meta.h"  // Is_declared_legacy_bidirectional_iterator
#include "mysql/meta/is_same_ignore_const.h"    // Is_same_ignore_const
#include "mysql/ranges/collection_interface.h"  // Collection_interface
#include "mysql/sets/boundary_set_meta.h"       // Is_boundary_storage

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::throwing::detail {

/// Boundary_iterator based on a std::vector iterator or const iterator.
template <class Vector_iterator_tp, class Mutable_vector_iterator_tp = void>
class Vector_boundary_iterator
    : public mysql::iterators::Iterator_interface<Vector_boundary_iterator<
          Vector_iterator_tp, Mutable_vector_iterator_tp>> {
  using Vector_iterator_t = Vector_iterator_tp;
  using Mutable_vector_iterator_t = Mutable_vector_iterator_tp;

 public:
  /// Default constructor. The resulting iterator is in a "pointless" state,
  /// where the only allowed operation is to assign to it.
  Vector_boundary_iterator() = default;

  /// Construct an iterator at the given position, and at the given endpoint
  /// state.
  Vector_boundary_iterator(const Vector_iterator_t &position, bool is_endpoint)
      : m_position(position), m_is_endpoint(is_endpoint) {}

  /// Converting constructor from the mutable iterator type, enabled if this is
  /// a const iterator.
  explicit Vector_boundary_iterator(
      const Vector_boundary_iterator<Mutable_vector_iterator_t> &other)
    requires(!std::same_as<Mutable_vector_iterator_t, void>)
      : m_position(other.vector_iterator()),
        m_is_endpoint(other.is_endpoint()) {}

  /// @return pointer to the current boundary.
  [[nodiscard]] auto *get_pointer() const {
    return std::to_address(m_position);
  }

  /// Move forward the given number of steps.
  void advance(std::ptrdiff_t delta) {
    m_position += delta;
    if ((delta & 1) != 0) {
      m_is_endpoint = !m_is_endpoint;
    }
  }

  /// @return the distance from this iterator to the given one.
  [[nodiscard]] std::ptrdiff_t distance_from(
      const Vector_boundary_iterator &other) const {
    return m_position - other.m_position;
  }

  /// @return true if this is an endpoint.
  [[nodiscard]] bool is_endpoint() const { return m_is_endpoint; }

  /// @return the underlying std::vector iterator.
  [[nodiscard]] const Vector_iterator_t &vector_iterator() const {
    return m_position;
  }

 private:
  /// Iterator to the underlying vector.
  Vector_iterator_t m_position{};

  /// True if the current boundary is an endpoint.
  bool m_is_endpoint{false};
};  // class Vector_boundary_iterator

template <Is_bounded_set_traits Set_traits_t>
using Vector_for_set_traits =
    std::vector<typename Set_traits_t::Element_t,
                mysql::allocators::Allocator<typename Set_traits_t::Element_t>>;

}  // namespace mysql::sets::throwing::detail

namespace mysql::sets::throwing {

/// Storage for boundary points, backed by std::vector.
///
/// The vector is maintained sorted. Insertion and deletion is linear in the
/// number of elements to the right of the first inserted/deleted element.
/// upper_bound/lower_bound is logarithmic. The iterator category is
/// contiguous_iterator.
///
/// @tparam Set_traits_tp Bounded set traits describing properties of the
/// element type.
template <Is_bounded_set_traits Set_traits_tp>
class Vector_boundary_storage
    : public mysql::containers::Basic_container_wrapper<
          Vector_boundary_storage<Set_traits_tp>,
          detail::Vector_for_set_traits<Set_traits_tp>>,
      public Upper_lower_bound_interface<
          Vector_boundary_storage<Set_traits_tp>, Set_traits_tp,
          mysql::ranges::Range_iterator_type<
              detail::Vector_for_set_traits<Set_traits_tp>>,
          mysql::ranges::Range_const_iterator_type<
              detail::Vector_for_set_traits<Set_traits_tp>>,
          Iterator_get_value> {
  using This_t = Vector_boundary_storage<Set_traits_tp>;

 public:
  using Set_traits_t = Set_traits_tp;

  using Element_t = Set_traits_t::Element_t;
  using Less_t = Set_traits_t::Less_t;

  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = mysql::allocators::Allocator<Element_t>;

  using Vector_t = std::vector<Element_t, Allocator_t>;
  using Vector_iterator_t = Vector_t::iterator;
  using Vector_const_iterator_t = Vector_t::const_iterator;
  using Iterator_t = detail::Vector_boundary_iterator<Vector_iterator_t>;

  static_assert(std::contiguous_iterator<Iterator_t>);
  static_assert(
      mysql::iterators::Is_declared_legacy_bidirectional_iterator<Iterator_t>);
  using Const_iterator_t =
      detail::Vector_boundary_iterator<Vector_const_iterator_t>;

  using Basic_container_wrapper_t =
      mysql::containers::Basic_container_wrapper<This_t, Vector_t>;

  static_assert(
      std::same_as<Vector_t, detail::Vector_for_set_traits<Set_traits_t>>);

  /// Declare that insertion is "slow", i.e., O(N).
  static constexpr bool has_fast_insertion = false;

  /// Construct a new, empty object, with the given Memory_resource.
  explicit Vector_boundary_storage(
      const Memory_resource_t &memory_resource = Memory_resource_t()) noexcept
      : Basic_container_wrapper_t(Allocator_t(memory_resource)) {
    static_assert(Is_boundary_storage<This_t>);
  }

  /// Constructor copying both values and Memory_resource from another storage.
  explicit Vector_boundary_storage(
      const Is_readable_boundary_storage_over_traits<Set_traits_t> auto &source)
      : Basic_container_wrapper_t(source.begin(), source.end(),
                                  Allocator_t(source.get_memory_resource())) {}

  /// Constructor copying from another type, using the given Memory_resource.
  explicit Vector_boundary_storage(
      const Is_readable_boundary_storage_over_traits<Set_traits_t> auto &source,
      const Memory_resource_t &memory_resource)
      : Basic_container_wrapper_t(source.begin(), source.end(),
                                  Allocator_t(memory_resource)) {}

  /// Constructor copying a range defined by the given iterators, using
  /// memory_resource if given, or the default Memory_resource() otherwise.
  template <std::input_iterator First_iterator_t>
  explicit Vector_boundary_storage(
      const First_iterator_t &first,
      const std::sentinel_for<First_iterator_t> auto &last,
      const Memory_resource_t &memory_resource = Memory_resource_t())
      : Basic_container_wrapper_t(first, last, Allocator_t(memory_resource)) {}

  // use defaults for copy/move/destructor
  Vector_boundary_storage(const Vector_boundary_storage &) = default;
  Vector_boundary_storage(Vector_boundary_storage &&) noexcept = default;
  Vector_boundary_storage &operator=(const Vector_boundary_storage &) = default;
  Vector_boundary_storage &operator=(Vector_boundary_storage &&) noexcept =
      default;
  ~Vector_boundary_storage() = default;

  /// Assignment operator copying from another type, preserving the existing
  /// Memory_resource.
  Vector_boundary_storage &operator=(
      const Is_readable_boundary_storage_over_traits<Set_traits_t> auto
          &source) {
    this->assign(source);
    return *this;
  }

  [[nodiscard]] Vector_t &vector() { return this->wrapped(); }
  [[nodiscard]] const Vector_t &vector() const { return this->wrapped(); }

  /// @return const iterator to the beginning.
  [[nodiscard]] auto begin() const {
    return Const_iterator_t(vector().begin(), false);
  }

  /// @return const iterator to the end.
  [[nodiscard]] auto end() const {
    return Const_iterator_t(vector().end(), false);
  }

  /// @return mutable iterator to the beginning.
  [[nodiscard]] auto begin() { return Iterator_t(vector().begin(), false); }

  /// @return mutable iterator to the end.
  [[nodiscard]] auto end() { return Iterator_t(vector().end(), false); }

  /// @return the number of boundary points.
  [[nodiscard]] std::size_t size() const { return vector().size(); }

  /// @return true if this object is empty.
  [[nodiscard]] bool empty() const { return vector().empty(); }

  /// @return true if this object is nonempty.
  [[nodiscard]] explicit operator bool() const { return (bool)vector(); }

  /// @return the upper bound for the given element in the given storage.
  template <class Iter_t>
  [[nodiscard]] static Iter_t upper_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return std::upper_bound(hint, self.end(), element, Less_t());
  }

  /// Return the lower bound for the given element in the given storage.
  template <class Iter_t>
  [[nodiscard]] static Iter_t lower_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return std::lower_bound(hint, self.end(), element, Less_t());
  }

  /// Erase an even-length range of boundary points.
  ///
  /// This invalidates all iterators to elements from left (inclusive) until the
  /// end.
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
    assert(std::distance(left, right) % 2 == 0);
    bool is_endpoint = left.is_endpoint();
    return Iterator_t(
        vector().erase(left.vector_iterator(), right.vector_iterator()),
        is_endpoint);
  }

  /// Insert two boundary points.
  ///
  /// This invalidates all iterators.
  ///
  /// @param position Iterator to where the interval shall be inserted.
  ///
  /// @param v1 The first boundary point to insert.
  ///
  /// @param v2 The second boundary point to insert.
  ///
  /// @return Iterator to the next point after the inserted point.
  [[nodiscard]] Iterator_t insert(const Iterator_t &position,
                                  const Element_t &v1, const Element_t &v2) {
    assert(position == begin() || lt(*std::prev(position), v1));
    assert(lt(v1, v2));
    assert(position == end() || lt(v2, *position));
    bool is_endpoint = position.is_endpoint();
    return Iterator_t(vector().insert(position.vector_iterator(), {v1, v2}) + 2,
                      is_endpoint);
  }

  /// Modify the element that the iterator points to.
  ///
  /// @return iterator to next element.
  [[nodiscard]] Iterator_t update_point(const Iterator_t &position,
                                        const Element_t &element) {
    *position = element;
    return std::next(position);
  }

 private:
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
};  // class Vector_boundary_storage

}  // namespace mysql::sets::throwing

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_THROWING_VECTOR_BOUNDARY_STORAGE_H

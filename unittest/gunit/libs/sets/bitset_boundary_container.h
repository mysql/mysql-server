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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef UNITTEST_LIBS_SETS_BITSET_BOUNDARY_CONTAINER_H
#define UNITTEST_LIBS_SETS_BITSET_BOUNDARY_CONTAINER_H

#include <gtest/gtest.h>                         // TEST
#include <bit>                                   // countr_zero
#include <cassert>                               // assert
#include <iterator>                              // forward_iterator
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/iterators/meta.h"  // Is_declared_legacy_bidirectional_iterator
#include "mysql/meta/not_decayed.h"  // Not_decayed
#include "mysql/sets/sets.h"         // Boundary_set_category_tag

// Set types, even user-defined ones, must be defined in namespace mysql::sets.
// This ensures that they are found by argument-dependent lookup of free
// functions such as mysql::sets::is_superset etc.
namespace unittest::libs::sets {

/// The type in which we store the full set.
using Bitset_storage = uint64_t;

/// The values stored in the set. Each valid value corresponds to a bit position
/// in Bitset_storage.
using Bitset_value = int;

/// Bit mask of type Bitset_storage in which the number'th bit is set.
constexpr Bitset_storage bitset_mask(Bitset_value bit_number) {
  return Bitset_storage(1) << bit_number;
}

/// Bit mask of type Bitset_storage where all bits from start to exclusive_end
/// are 1 and the rest are 0.
inline Bitset_storage bitset_interval(Bitset_value start,
                                      Bitset_value exclusive_end) {
  return bitset_mask(exclusive_end) - bitset_mask(start);
}

/// Return the element at the given position.
inline bool bitset_get(Bitset_storage bitset_storage, Bitset_value element) {
  return (bitset_storage & bitset_mask(element)) != 0;
}

/// Return true if `element` is a boundary in `storage`
inline bool is_bitset_boundary(Bitset_storage bitset_storage,
                               Bitset_value element) {
  return ((bitset_storage ^ (bitset_storage << 1)) & bitset_mask(element)) != 0;
}

/// Random access iterator over the boundary points of a set of small integers
/// represented as an integral.  The boundary points are the bit alterations,
/// i.e., each index i such that:
///
/// @code
///   (bitset & (1 << i)) != (bitset & (1 << (i-1))).
/// @endcode
///
/// The values stored in the set are 0, 1, ..., max_exclusive-1. Therefore, the
/// iterator positions other than end are 0, 1, ..., max_exclusive-1,
/// max_exclusive. We represent one-past-the-end as max_exclusive+1.
template <Bitset_value max_exclusive_tp>
class Bitset_boundary_iterator
    : public mysql::iterators::Iterator_interface<
          Bitset_boundary_iterator<max_exclusive_tp>> {
 public:
  // Special value used to denote the end iterator.
  static constexpr Bitset_value max_exclusive = max_exclusive_tp;
  static constexpr Bitset_value end_position = max_exclusive + 1;
  static constexpr bool has_fast_insertion = true;

  using Self_t = Bitset_boundary_iterator<max_exclusive>;

  Bitset_boundary_iterator() = default;

  //
  Bitset_boundary_iterator(Bitset_value bits, Bitset_value position)
      : m_bits(bits), m_position(position) {}

  /// Dereference the iterator by returning the current position.
  [[nodiscard]] Bitset_value get() const { return m_position; }

  /// Move the iterator forward one step by finding the next boundary.
  void next() {
    if (m_position >= end_position) {
    }
    assert(m_position < end_position);
    do {
      ++m_position;
    } while (m_position < end_position &&
             !is_bitset_boundary(m_bits, m_position));
  }

  /// Move the iterator back one step by finding the previous boundary.
  void prev() {
    assert(m_position > 0);
    do {
      --m_position;
    } while (m_position > 0 && !is_bitset_boundary(m_bits, m_position));
  }

  /// Move N steps. By implementing this and distance_from, we make the iterator
  /// a random_access_iterator.
  void advance(std::ptrdiff_t steps) {
    if (steps < 0) {
      for (std::ptrdiff_t i = 0; i != steps; --i) prev();
    } else {
      for (std::ptrdiff_t i = 0; i != steps; ++i) next();
    }
  }

  /// Count the steps between this and other.
  [[nodiscard]] std::ptrdiff_t distance_from(const Self_t &other) const {
    if (other.m_position > m_position) return -other.distance_from(*this);
    std::ptrdiff_t ret{0};
    for (auto it = other; !it.is_equal(*this); ++it) ++ret;

    return ret;
  }

  /// Return true if this and other are equal.
  [[nodiscard]] bool is_equal(const Self_t &other) const {
    assert(m_bits == other.m_bits);
    return m_position == other.m_position;
  }

  /// Return true if the current boundary is an endpoint.
  [[nodiscard]] bool is_endpoint() const {
    return m_position < end_position && !bitset_get(m_bits, m_position);
  }

 private:
  /// The bitmap representing the set.
  Bitset_storage m_bits{};

  /// The current position.
  Bitset_value m_position{};
};

static_assert(std::random_access_iterator<Bitset_boundary_iterator<47>>);

/// Container of small-magnitude positive integers, represented as a bitmap.
///
/// This is meant to be used in tests, as a simpler reference implementation of
/// set operations (which are very easy to implement on bitmaps, using built-in
/// bit operators).
///
/// @tparam max_exclusive_tp The maximum value stored in the set.
template <Bitset_value max_exclusive_tp>
class Bitset_boundary_container_impl {
 public:
  static constexpr Bitset_value max_exclusive = max_exclusive_tp;
  static constexpr bool has_fast_insertion = true;

  using Iterator_t = Bitset_boundary_iterator<max_exclusive>;
  static_assert(std::bidirectional_iterator<Iterator_t>);
  static_assert(
      mysql::iterators::Is_declared_legacy_input_iterator<Iterator_t>);
  using Const_iterator_t = Bitset_boundary_iterator<max_exclusive>;
  using Set_category_t = mysql::sets::Boundary_set_category_tag;
  using Set_traits_t =
      mysql::sets::Int_set_traits<Bitset_value, 0, max_exclusive>;
  using Allocator_t = void;
  using Element_t = typename Set_traits_t::Element_t;
  using Difference_t = typename Set_traits_t::Difference_t;
  using Memory_resource_t = mysql::allocators::Memory_resource;

  /// Use defaults for the default constructor and the rule-of-5 members.
  Bitset_boundary_container_impl() = default;
  Bitset_boundary_container_impl(const Bitset_boundary_container_impl &other) =
      default;
  Bitset_boundary_container_impl(Bitset_boundary_container_impl &&other) =
      default;
  Bitset_boundary_container_impl &operator=(
      const Bitset_boundary_container_impl &other) = default;
  Bitset_boundary_container_impl &operator=(
      Bitset_boundary_container_impl &&other) = default;
  ~Bitset_boundary_container_impl() = default;

  /// Construct a new container with a given Memory_resource_t.
  explicit Bitset_boundary_container_impl(const Memory_resource_t &) {}

  /// Construct a new container from the given bitmap.
  explicit Bitset_boundary_container_impl(Bitset_storage bits) : m_bits(bits) {}

  /// Construct a new container as teh copy of the other one.
  explicit Bitset_boundary_container_impl(
      const mysql::sets::Is_boundary_set_over_traits<Set_traits_t> auto &source,
      const Memory_resource_t & = Memory_resource_t()) {
    for (const auto [start, end] :
         mysql::ranges::make_disjoint_pairs_view(source)) {
      for (auto element = start; element != end; ++element) {
        insert(element);
      }
    }
  }

  /// Copy the other container, overwriting this one.
  void assign(const Bitset_boundary_container_impl &other) {
    set_bits(other.m_bits);
  }

  /// Overwrite this container by the given bitmap.
  void set_bits(Bitset_storage bits) { m_bits = bits; }

  /// Return iterator to the beginning.
  [[nodiscard]] auto begin() const {
    if (m_bits == 0)  // begin == end
      return Iterator_t(m_bits, Iterator_t::end_position);
    return Iterator_t(m_bits, std::countr_zero(m_bits));
  }

  /// Return iterator to the end.
  [[nodiscard]] auto end() const {
    return Iterator_t(m_bits, Iterator_t::end_position);
  }

  /// Return first boundary.
  [[nodiscard]] auto front() const { return *begin(); }

  /// Return last boundary.
  [[nodiscard]] auto back() const { return *std::ranges::prev(end()); }

  /// Return nth boundary.
  [[nodiscard]] auto operator[](std::size_t index) const {
    return *std::next(begin(), index);
  }

  /// Return nth boundary.
  [[nodiscard]] auto operator[](std::size_t index) {
    return *std::next(begin(), index);
  }

  /// Return the number of boundaries, signed.
  [[nodiscard]] auto ssize() const { return std::distance(begin(), end()); }

  /// Return the number of boundaries, unsigned.
  [[nodiscard]] auto size() const { return std::size_t(ssize()); }

  /// Return true if size is 0.
  [[nodiscard]] bool empty() const { return !size(); }

  /// Return false if size is 0.
  [[nodiscard]] explicit operator bool() const { return !empty(); }

  /// Return true if element is in the set.
  [[nodiscard]] bool contains_element(Bitset_value element) const {
    return bitset_get(m_bits, element);
  }

  /// Return the bitmap.
  [[nodiscard]] Bitset_storage bits() const { return m_bits; }

  /// Return the first boundary at or after hint that is strictly greater than
  /// element.
  [[nodiscard]] auto upper_bound(const Iterator_t &hint,
                                 Bitset_value element) const {
    for (auto it = hint; it != end(); ++it) {
      if (*it > element) return it;
    }
    return end();
  }

  /// Return the first boundary that is strictly greater than element.
  [[nodiscard]] auto upper_bound(Bitset_value element) const {
    return upper_bound(begin(), element);
  }

  /// Return the first boundary at or after hint that is greater than or equal
  /// to element.
  [[nodiscard]] auto lower_bound(const Iterator_t &hint,
                                 Bitset_value element) const {
    for (auto it = hint; it != end(); ++it) {
      if (*it >= element) return it;
    }
    return end();
  }

  /// Return the first boundary that is greater than or equal to element.
  [[nodiscard]] auto lower_bound(Bitset_value element) const {
    return lower_bound(begin(), element);
  }

  /// Inplace-union the given element.
  void insert(const Element_t &element) { m_bits |= bitset_mask(element); }

  /// Inplace-subtract the given element.
  void remove(const Element_t &element) { m_bits &= ~bitset_mask(element); }

  /// Inplace-union with given interval
  void inplace_union(const Element_t &start, const Element_t &exclusive_end) {
    m_bits |= bitset_interval(start, exclusive_end);
  }

  /// Inplace-union with given interval and cursor (which we don't use)
  void inplace_union(Iterator_t &, const Element_t &start,
                     const Element_t &exclusive_end) {
    inplace_union(start, exclusive_end);
  }

  /// Inplace-union with other.
  void inplace_union(const Bitset_boundary_container_impl &other) {
    m_bits |= other.m_bits;
  }

  /// Inplace-subtract given interval
  void inplace_subtract(const Element_t &start,
                        const Element_t &exclusive_end) {
    m_bits &= ~bitset_interval(start, exclusive_end);
  }

  /// Inplace-subtract with given interval and cursor (which we don't use)
  void inplace_subtract(Iterator_t &, const Element_t &start,
                        const Element_t &exclusive_end) {
    inplace_subtract(start, exclusive_end);
  }

  /// Inplace-subtract other from this set.
  void inplace_subtract(const Bitset_boundary_container_impl &other) {
    m_bits &= ~other.m_bits;
  }

  /// Inplace-intersect with given interval
  void inplace_intersect(const Element_t &start,
                         const Element_t &exclusive_end) {
    m_bits &= bitset_interval(start, exclusive_end);
  }

  /// Inplace-intersect with other.
  void inplace_intersect(const Bitset_boundary_container_impl &other) {
    m_bits &= other.m_bits;
  }

  /// Replace this set by its complement complement.
  void inplace_complement() { m_bits ^= bitset_mask(max_exclusive) - 1; }

  /// Remove all boundaries.
  void clear() { m_bits = 0; }

  /// Return the total length of all intervals.
  [[nodiscard]] Difference_t volume() const {
    Difference_t ret{0};
    for (Element_t i = 0; i < max_exclusive; ++i) {
      if (contains_element(i)) ++ret;
    }
    return ret;
  }

 private:
  /// The bitmap representing the set.
  Bitset_storage m_bits = 0;
};  // class Bitset_boundary_container_impl

}  // namespace unittest::libs::sets

namespace mysql::sets {

template <unittest::libs::sets::Bitset_value max_exclusive_tp>
class Bitset_boundary_container
    : public unittest::libs::sets::Bitset_boundary_container_impl<
          max_exclusive_tp> {
  using This_t = Bitset_boundary_container<max_exclusive_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Bitset_boundary_container(Args_t &&...args)
      : unittest::libs::sets::Bitset_boundary_container_impl<max_exclusive_tp>(
            std::forward<Args_t>(args)...) {}
};

static_assert(mysql::sets::Is_boundary_container<
              mysql::sets::Bitset_boundary_container<21>>);

}  // namespace mysql::sets

#endif  // ifndef UNITTEST_LIBS_SETS_BITSET_BOUNDARY_CONTAINER_H

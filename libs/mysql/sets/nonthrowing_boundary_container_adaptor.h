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

#ifndef MYSQL_SETS_NONTHROWING_BOUNDARY_CONTAINER_ADAPTOR_H
#define MYSQL_SETS_NONTHROWING_BOUNDARY_CONTAINER_ADAPTOR_H

/// @file
/// Experimental API header

#include "mysql/allocators/memory_resource.h"  // Memory_resource
#include "mysql/meta/is_same_ignore_const.h"   // Is_same_ignore_const
#include "mysql/sets/boundary_set_interface.h"  // Basic_boundary_container_wrapper
#include "mysql/sets/boundary_set_meta.h"       // Is_boundary_container
#include "mysql/utils/call_and_catch.h"         // call_and_catch

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Non-throwing container that stores boundaries.
///
/// This implements the Is_boundary_set concept, and additionally has
/// functionality for in-place union, intersection, and subtraction, as well as
/// copy from another Boundary_set and making this container empty.
///
/// This is only wrappers around @c throwing::Boundary_container.
template <Is_boundary_container Throwing_boundary_container_tp>
  requires std::is_nothrow_default_constructible_v<
               Throwing_boundary_container_tp> &&
           std::is_nothrow_destructible_v<Throwing_boundary_container_tp>
class Nonthrowing_boundary_container_adaptor
    : public Basic_boundary_container_wrapper<
          Nonthrowing_boundary_container_adaptor<
              Throwing_boundary_container_tp>,
          Throwing_boundary_container_tp, mysql::utils::Shall_catch::yes> {
 public:
  using Throwing_boundary_container_t = Throwing_boundary_container_tp;
  using Iterator_t =
      mysql::ranges::Range_iterator_type<Throwing_boundary_container_t>;
  using Const_iterator_t =
      mysql::ranges::Range_const_iterator_type<Throwing_boundary_container_t>;
  using Set_traits_t = Throwing_boundary_container_t::Set_traits_t;
  using Element_t = Set_traits_t::Element_t;
  using Storage_t = Throwing_boundary_container_t::Storage_t;
  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = typename Throwing_boundary_container_t::Allocator_t;
  using This_t =
      Nonthrowing_boundary_container_adaptor<Throwing_boundary_container_t>;
  using Base_t = Basic_boundary_container_wrapper<
      Nonthrowing_boundary_container_adaptor<Throwing_boundary_container_tp>,
      Throwing_boundary_container_tp, mysql::utils::Shall_catch::yes>;

  static constexpr bool has_fast_insertion =
      Throwing_boundary_container_t::has_fast_insertion;

  /// Default constructor.
  Nonthrowing_boundary_container_adaptor() noexcept = default;

  // Default move semantics and destructor if it is non-throwing. Delete
  // copy-semantics since it is inherently throwing.
  Nonthrowing_boundary_container_adaptor(const This_t &other) = delete;
  Nonthrowing_boundary_container_adaptor(This_t &&other) noexcept = default;
  This_t &operator=(const This_t &other) = delete;
  This_t &operator=(This_t &&other) noexcept = default;
  ~Nonthrowing_boundary_container_adaptor() noexcept = default;

  /// Construct using the given Memory_resource.
  explicit Nonthrowing_boundary_container_adaptor(
      const Memory_resource_t &memory_resource) noexcept
      : Base_t(memory_resource) {}

  /// Return a non-const reference to the underlying, throwing boundary
  /// container.
  [[nodiscard]] auto &throwing() noexcept { return this->wrapped(); }

  /// Return a const reference to the underlying, throwing boundary container.
  [[nodiscard]] const auto &throwing() const noexcept {
    return this->wrapped();
  }

  /// Return a non-const reference to the underlying storage.
  [[nodiscard]] auto &storage() noexcept { return this->wrapped().storage(); }

  /// Return a const reference to the underlying storage.
  [[nodiscard]] const auto &storage() const noexcept {
    return this->wrapped().storage();
  }

  /// Insert the given element (inplace union).
  ///
  /// This may insert a new one-element interval, extend an existing interval at
  /// one end, merge two intervals that were separated by only the given
  /// element, or, if the element was already in this container, do nothing.
  ///
  /// @param element The element to insert.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// inserting the interval. This leaves the container unmodified.
  [[nodiscard]] auto insert(const Element_t &element) noexcept {
    return mysql::utils::call_and_catch([&] { throwing().insert(element); });
  }

  /// Remove the given element (inplace subtraction).
  ///
  /// This may split an interval into two parts, shorten an existing interval in
  /// one end, remove a one-element interval, or, if the element was not in this
  /// container, do nothing.
  ///
  /// @param element The element to remove.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// splitting an interval. This leaves the container unmodified.
  [[nodiscard]] auto remove(const Element_t &element) noexcept {
    return mysql::utils::call_and_catch([&] { throwing().remove(element); });
  }

  /// Insert the given interval (inplace union).
  ///
  /// This may merge intervals that overlap or are adjacent to the given
  /// interval, or insert the interval between existing intervals, or, if the
  /// interval was a subset of this container, do nothing.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// inserting the interval. This leaves the container unmodified.
  [[nodiscard]] auto inplace_union(const Element_t &start,
                                   const Element_t &exclusive_end) noexcept {
    return mysql::utils::call_and_catch(
        [&] { throwing().inplace_union(start, exclusive_end); });
  }

  /// Insert the given interval (inplace union), reading and updating the given
  /// cursor.
  ///
  /// This may merge intervals that overlap or are adjacent to the given
  /// interval, or insert the interval between existing intervals, or, if the
  /// interval was a subset of this container, do nothing.
  ///
  /// @param[in,out] cursor Hint for the insertion position. If it refers to
  /// `lower_bound(start)`, this function finds the insertion position in O(1);
  /// if it refers to a boundary less than `lower_bound(start)`, it may reduce
  /// the search space when finding the insertion position; otherwise, the hint
  /// is ignored and the entire set has to be searched. It will be updated to
  /// `upper_bound(exclusive_end)`, which makes it perfect to reuse for future
  /// calls to this function, with intervals following this one.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// inserting the interval. This leaves the container unmodified.
  [[nodiscard]] auto inplace_union(Iterator_t &cursor, const Element_t &start,
                                   const Element_t &exclusive_end) noexcept {
    return mysql::utils::call_and_catch(
        [&] { throwing().inplace_union(cursor, start, exclusive_end); });
  }

  /// In-place insert the intervals of the given set into this container
  /// (inplace union).
  ///
  /// This may merge intervals that overlap or are adjacent to a given interval,
  /// and/or insert intervals between existing intervals, or, if the set
  /// was a subset of this container, do nothing.
  ///
  /// This uses one of two algorithms, depending on the nature of the underlying
  /// container:
  ///
  /// - If the underlying container supports fast insertion in the middle (e.g.
  /// set or list), then it uses a true in-place algorithm, possibly adjusting
  /// endpoints of existing intervals, and reusing memory as much as possible.
  ///
  /// - Otherwise (e.g. sorted vector), it uses an out-of-place algorithm that
  /// computes the result in a new container and then move-assigns the new
  /// container to the current container.
  ///
  /// The complexity depends on the underlying container:
  ///
  /// - set: O(number_of_removed_intervals + input.size() * log(this->size()))
  ///
  /// - list: Normally, O(input.size() + this->size()); or O(input.size()) if
  /// input_set.front() >= this->back().
  ///
  /// - vector: Normally, O(input.size() + this->size()); or O(input.size()) if
  /// input_set.front() >= this->back().
  ///
  /// @param input_set The input set.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// inserting an interval. This may occur when the operation is
  /// half-completed, which may leave the container as a superset of the
  /// previous set and a subset of the union.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  [[nodiscard]] auto inplace_union(Input_set_t &&input_set) noexcept {
    return mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        throwing().inplace_union(std::forward<Input_set_t>(input_set))));
  }

  /// Subtract the given interval.
  ///
  /// This may split an interval into two parts, shorten an existing interval in
  /// one end, remove a one-element interval, or, if the element was not in this
  /// container, do nothing.
  ///
  /// This may truncate and/or split intervals that overlap partially with the
  /// subtracted interval, and remove intervals that overlap completely with the
  /// subtracted interval.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// splitting an interval. This leaves the container unmodified.
  [[nodiscard]] auto inplace_subtract(const Element_t &start,
                                      const Element_t &exclusive_end) noexcept {
    return mysql::utils::call_and_catch(
        [&] { throwing().inplace_subtract(start, exclusive_end); });
  }

  /// Subtract the given interval, reading and updating the given cursor.
  ///
  /// This may truncate and/or split intervals that overlap partially with the
  /// subtracted interval, and remove intervals that overlap completely with the
  /// subtracted interval.
  ///
  /// @param[in,out] cursor Hint for the insertion position. If it refers to
  /// `lower_bound(start)`, this function finds the insertion position in O(1);
  /// if it refers to a boundary less than `lower_bound(start)`, it may reduce
  /// the search space when finding the insertion position; otherwise, the hint
  /// is ignored and the entire set has to be searched. It will be updated to
  /// `upper_bound(exclusive_end)`, which makes it perfect to reuse for future
  /// calls to this function, with intervals following this one.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// splitting an interval. This leaves the container unmodified.
  [[nodiscard]] auto inplace_subtract(Iterator_t &cursor,
                                      const Element_t &start,
                                      const Element_t &exclusive_end) noexcept {
    return mysql::utils::call_and_catch(
        [&] { throwing().inplace_subtract(cursor, start, exclusive_end); });
  }

  /// In-place subtract intervals of the given container from this container.
  ///
  /// This may truncate and/or split intervals that overlap partially with
  /// subtracted intervals, and remove intervals that overlap completely with
  /// subtracted intervals.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param input_set The input set.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// splitting an interval. This may occur when the operation is
  /// half-completed, which may leave the container as a subset of the previous
  /// set and a superset of the difference.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  [[nodiscard]] auto inplace_subtract(Input_set_t &&input_set) noexcept {
    return mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        throwing().inplace_subtract(std::forward<Input_set_t>(input_set))));
  }

  /// In-place intersect this container with the given interval.
  ///
  /// This may truncate intervals that overlap partially with the given
  /// interval, and remove intervals that are disjoint from the given interval.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// If the underlying boundary container cannot throw, this returns void.
  /// Otherwise, retursn Return_status::ok on success and Return_status::error
  /// on out-of-memory error.
  [[nodiscard]] auto inplace_intersect(
      const Element_t &start, const Element_t &exclusive_end) noexcept {
    return mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        this->throwing().inplace_intersect(start, exclusive_end)));
  }

  /// In-place intersect this container with intervals of the given container.
  ///
  /// This may truncate intervals that overlap partially with one interval from
  /// the given set, split intervals that overlap partially with more
  /// than one interval from the given set, and remove intervals that are
  /// disjoint from the given interval set.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param input_set The input set.
  ///
  /// @retval Return_status::ok Success
  ///
  /// @retval Return_status::error An out-of-memory condition occurred while
  /// splitting an interval. This may occur when the operation is
  /// half-completed, which may leave the container as a subset of the previous
  /// set and a superset of the intersection.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  [[nodiscard]] auto inplace_intersect(Input_set_t &&input_set) noexcept {
    return mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        this->throwing().inplace_intersect(
            std::forward<Input_set_t>(input_set))));
  }

  /// Return iterator to the leftmost boundary at or after `cursor` that is
  /// greater than the given element.
  [[nodiscard]] static auto upper_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const auto &cursor,
      const Element_t &element) noexcept {
    return Throwing_boundary_container_t::upper_bound_impl(self.throwing(),
                                                           cursor, element);
  }

  /// Return iterator to the leftmost boundary at or after `cursor` that is
  /// greater than or equal to the given element.
  [[nodiscard]] static auto lower_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const auto &cursor,
      const Element_t &element) noexcept {
    return Throwing_boundary_container_t::lower_bound_impl(self.throwing(),
                                                           cursor, element);
  }
};  // class Nonthrowing_boundary_container_adaptor

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NONTHROWING_BOUNDARY_CONTAINER_ADAPTOR_H

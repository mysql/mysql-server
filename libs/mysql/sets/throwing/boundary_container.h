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

#ifndef MYSQL_SETS_THROWING_BOUNDARY_CONTAINER_H
#define MYSQL_SETS_THROWING_BOUNDARY_CONTAINER_H

/// @file
/// Experimental API header

#include <iterator>                                    // prev
#include "mysql/allocators/memory_resource.h"          // Memory_resource
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/iterators/meta.h"  // Is_declared_legacy_bidirectional_iterator
#include "mysql/meta/is_same_ignore_const.h"         // Is_same_ignore_const
#include "mysql/ranges/disjoint_pairs.h"             // make_disjoint_pairs_view
#include "mysql/sets/base_binary_operation_views.h"  // make_binary_operation_view
#include "mysql/sets/base_complement_view.h"         // make_complement_view
#include "mysql/sets/binary_operation.h"             // Binary_operation
#include "mysql/sets/boundary_set_binary_operation_view_base.h"  // make_complement_view
#include "mysql/sets/boundary_set_complement_view.h"  // Complement_view<Boundary_set>
#include "mysql/sets/boundary_set_interface.h"        // Boundary_set_interface
#include "mysql/sets/boundary_set_intersection_view.h"  // Boundary_set_intersection_view
#include "mysql/sets/boundary_set_meta.h"               // Is_boundary_storage
#include "mysql/sets/boundary_set_subtraction_view.h"  // Subtraction_view
#include "mysql/sets/boundary_set_union_view.h"        // Union_view
#include "mysql/sets/set_container_helpers.h"  // handle_inplace_op_trivial_cases

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::throwing {

/// Container that stores boundaries.
///
/// This implements the Is_boundary_set concept, and additionally has
/// functionality for in-place union, intersection, and subtraction, as well as
/// copy from another Boundary_set and making this container empty.
///
/// All members provide basic exception safety guarantee, as defined at
/// https://en.cppreference.com/w/cpp/language/exceptions#Exception_safety .
/// Some additionally provide strong exception safety guarantee or nothrow
/// exception guarantee: see documentation of each member for details.
template <Is_boundary_storage Storage_tp>
class Boundary_container : public mysql::sets::Basic_boundary_container_wrapper<
                               Boundary_container<Storage_tp>, Storage_tp> {
 public:
  using Storage_t = Storage_tp;
  using Iterator_t = mysql::ranges::Range_iterator_type<Storage_t>;
  static_assert(std::bidirectional_iterator<Iterator_t>);
  static_assert(
      mysql::iterators::Is_declared_legacy_bidirectional_iterator<Iterator_t>);
  using Const_iterator_t = mysql::ranges::Range_const_iterator_type<Storage_t>;
  using Set_traits_t = Storage_t::Set_traits_t;
  using Element_t = Set_traits_t::Element_t;
  using Memory_resource_t = mysql::allocators::Memory_resource;
  using Allocator_t = typename Storage_t::Allocator_t;
  using This_t = Boundary_container<Storage_t>;
  using Base_t = Basic_boundary_container_wrapper<This_t, Storage_t>;

  static constexpr bool has_fast_insertion = Storage_t::has_fast_insertion;

  /// Default constructor.
  Boundary_container() noexcept = default;

  // Default rule-of-5.
  Boundary_container(const Boundary_container &source) = default;
  Boundary_container(Boundary_container &&source) noexcept = default;
  Boundary_container &operator=(const Boundary_container &source) = default;
  Boundary_container &operator=(Boundary_container &&source) noexcept = default;
  ~Boundary_container() = default;

  /// Construct using the given Memory_resource.
  explicit Boundary_container(const Memory_resource_t &memory_resource) noexcept
      : Base_t(memory_resource) {}

  /// Construct from any other Boundary_set over the same Set traits.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  explicit Boundary_container(
      const Is_boundary_set_over_traits<Set_traits_t> auto &source,
      const Memory_resource_t &memory_resource)
      : Base_t(source.begin(), source.end(), memory_resource) {}

  /// Construct from any other Boundary_set over the same Set traits.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  explicit Boundary_container(
      const Is_boundary_set_over_traits<Set_traits_t> auto &source)
      : Base_t(source.begin(), source.end(),
               mysql::allocators::get_memory_resource_or_default(source)) {}

  /// Construct from iterators.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  template <Is_boundary_iterator_over_type<Element_t> First_iterator_t>
  explicit Boundary_container(
      const First_iterator_t &first,
      const std::sentinel_for<First_iterator_t> auto &last,
      const Memory_resource_t &memory_resource)
      : Base_t(first, last, memory_resource) {}

  /// Construct from iterators.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  template <Is_boundary_iterator_over_type<Element_t> First_iterator_t>
  explicit Boundary_container(
      const First_iterator_t &first,
      const std::sentinel_for<First_iterator_t> auto &last)
      : Base_t(first, last) {}

  /// Assign from any other Boundary_set over the same Set traits.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed,
  /// which may leave the container as a subset of @c source.
  This_t &operator=(
      const Is_boundary_set_over_traits<Set_traits_t> auto &source) {
    this->assign(source);
    return *this;
  }

  using Base_t::assign;

  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Source_t>
    requires(Can_donate_set<Source_t &&, This_t>)
  // The requires clause ensures that Source_t is an rvalue reference.
  // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
  void assign(Source_t &&source) {
    storage() = std::move(source.storage());
  }

  /// Return a const reference to the underlying storage.
  [[nodiscard]] const Storage_t &storage() const noexcept {
    return this->wrapped();
  }

  /// Return a const reference to the underlying storage.
  [[nodiscard]] Storage_t &storage() noexcept { return this->wrapped(); }

  /// Insert the given element (inplace union).
  ///
  /// This may insert a new one-element interval, extend an existing interval at
  /// one end, merge two intervals that were separated by only the given
  /// element, or, if the element was already in this container, do nothing.
  ///
  /// @param element The element to insert.
  ///
  /// @throws bad_alloc If an out-of-memory condition occurred while inserting
  /// the interval. This leaves the container unmodified.
  void insert(const Element_t &element) {
    inplace_union(element, Set_traits_t::next(element));
  }

  /// Remove the given element (inplace subtraction).
  ///
  /// This may split an interval into two parts, shorten an existing interval in
  /// one end, remove a one-element interval, or, if the element was not in this
  /// container, do nothing.
  ///
  /// @param element The element to remove.
  ///
  /// @throws bad_alloc If an out-of-memory condition occurred while splitting
  /// an interval. This leaves the container unmodified.
  void remove(const Element_t &element) {
    inplace_subtract(element, Set_traits_t::next(element));
  }

 private:
  /// Whether a `hint` parameter is guaranteed to satisfy the precondition that
  /// of being a lower bound.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum class Hint_guaranteed { no, yes };

 public:
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
  /// @throws bad_alloc If an out-of-memory condition occurred while inserting
  /// the interval. This leaves the container unmodified.
  void inplace_union(const Element_t &start, const Element_t &exclusive_end) {
    Iterator_t hint = this->begin();
    inplace_union_or_subtract<Binary_operation::op_union, Hint_guaranteed::yes>(
        hint, start, exclusive_end);
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
  /// @throws bad_alloc If an out-of-memory condition occurred while inserting
  /// the interval. This leaves the container unmodified.
  void inplace_union(Iterator_t &cursor, const Element_t &start,
                     const Element_t &exclusive_end) {
    inplace_union_or_subtract<Binary_operation::op_union, Hint_guaranteed::no>(
        cursor, start, exclusive_end);
  }

  /// In-place insert the intervals of the given set into this container
  /// (inplace union).
  ///
  /// This may merge intervals that overlap or are adjacent to a given
  /// interval, and/or insert intervals between existing intervals, or, if the
  /// set was a subset of this container, do nothing.
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
  /// @throws bad_alloc If an out-of-memory condition occurred while inserting
  /// an interval. This may occur when the operation is half-completed, which
  /// may leave the container as a superset of the previous set and a subset of
  /// the union.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  void inplace_union(Input_set_t &&input_set) {
    inplace_op<Binary_operation::op_union>(
        std::forward<Input_set_t>(input_set));
  }

  /// Subtract the given interval.
  ///
  /// This may split an interval into two parts, shorten an existing interval in
  /// one end, remove a one-element interval, or, if the element was not in this
  /// container, do nothing.
  ///
  /// This may truncate and/or split intervals that overlap partially with
  /// the subtracted interval, and remove intervals that overlap completely with
  /// the subtracted interval.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @throws bad_alloc If an out-of-memory condition occurred while splitting
  /// an interval. This leaves the container unmodified.
  void inplace_subtract(const Element_t &start,
                        const Element_t &exclusive_end) {
    Iterator_t hint = this->begin();
    inplace_union_or_subtract<Binary_operation::op_subtraction,
                              Hint_guaranteed::yes>(hint, start, exclusive_end);
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
  /// `upper_bound(exclusive_end)`, which makes it good to reuse for future
  /// calls to this function, with intervals following this one.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @throws bad_alloc If an out-of-memory condition occurred while splitting
  /// an interval. This leaves the container unmodified.
  void inplace_subtract(Iterator_t &cursor, const Element_t &start,
                        const Element_t &exclusive_end) {
    inplace_union_or_subtract<Binary_operation::op_subtraction,
                              Hint_guaranteed::no>(cursor, start,
                                                   exclusive_end);
  }

  /// In-place subtract intervals of the given container from this
  /// container.
  ///
  /// This may truncate and/or split intervals that overlap partially with
  /// subtracted intervals, and remove intervals that overlap completely with
  /// subtracted intervals.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @param input_set The input set.
  ///
  /// @throws bad_alloc If an out-of-memory condition occurred while splitting
  /// an interval. This may occur when the operation is half-completed,
  /// which may leave the container as a subset of the previous set and a
  /// superset of the difference.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  void inplace_subtract(Input_set_t &&input_set) {
    inplace_op<Binary_operation::op_subtraction>(
        std::forward<Input_set_t>(input_set));
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
  /// Since this cannot increase the number of intervals, it never throws.
  void inplace_intersect(const Element_t &start,
                         const Element_t &exclusive_end) noexcept {
    if (Set_traits_t::lt(exclusive_end, Set_traits_t::max_exclusive()))
      inplace_subtract(exclusive_end, Set_traits_t::max_exclusive());
    if (Set_traits_t::gt(start, Set_traits_t::min()))
      inplace_subtract(Set_traits_t::min(), start);
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
  /// @throws bad_alloc If an out-of-memory condition occurred while splitting
  /// an interval. This may occur when the operation is half-completed, which
  /// may leave the container as a subset of the previous set and a superset of
  /// the intersection.
  template <Is_boundary_set_over_traits_unqualified<Set_traits_t> Input_set_t>
  void inplace_intersect(Input_set_t &&input_set) {
    inplace_op<Binary_operation::op_intersection>(
        std::forward<Input_set_t>(input_set));
  }

  /// Return iterator to the leftmost boundary at or after `hint` that is
  /// greater than the given element.
  ///
  /// Complexity:
  /// - O(1) if the return element is end() or `hint`;
  /// - the complexity of Storage_t::upper_bound, otherwise.
  ///
  /// (We override the Boundary_set_interface member because this is
  /// faster.)
  [[nodiscard]] static auto upper_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const auto &hint,
      const Element_t &element) noexcept {
    return Storage_t::upper_bound_dispatch(self.storage(), hint, element);
  }

  /// Return iterator to the leftmost boundary at or after `hint` that is
  /// greater than or equal to the given element.
  ///
  /// Complexity:
  /// - O(1) if the return element is end() or `hint`;
  /// - the complexity of Storage_t::upper_bound, otherwise.
  ///
  /// (We override the Boundary_set_interface member because this is
  /// faster.)
  [[nodiscard]] static auto lower_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const auto &hint,
      const Element_t &element) noexcept {
    return Storage_t::lower_bound_dispatch(self.storage(), hint, element);
  }

 private:
  /// Indicates to a function that it may steal elements from a source object,
  /// rather than allocate new elements.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum class Can_donate { no, yes };

  /// Worker for either in-place union or in-place subtraction, adding or
  /// removing all intervals from another Boundary_set.
  ///
  /// Algorithm and complexity: @see inplace_union.
  ///
  /// @tparam operation The set operation to compute.
  ///
  /// @param source The container whose elements will be added or removed in
  /// this container.
  ///
  /// @throws bad_alloc This may occur when the operation is half-completed, so
  /// that a subset of `source` has been added or removed from this set.
  template <Binary_operation operation,
            Is_boundary_set_over_traits_unqualified<Set_traits_t> Source_t>
  void inplace_op(Source_t &&source) {
    if (mysql::sets::detail::handle_inplace_op_trivial_cases<operation>(
            *this, std::forward<Source_t>(source)))
      return;

    // For containers with random access iterators, insertion/deletion in the
    // middle is typically worst-case linear in the container size. So a
    // sequence of N insertions/deletions may be O(N*size). This is worst-case
    // quadratic, when both N and size are linear in this->size() +
    // source.size().  To avoid performance bottlenecks, we fall back to a
    // full-copy from a Union_view or Intersection_view, which is guaranteed to
    // be linear.
    //
    // Some cases are not really quadratic: An upper bound on the number of
    // insertions/deletions during which any intervals are moved, is the number
    // of intervals of source that are to the left of this->end(). And an upper
    // bound on the number of intervals that are moved each time, is the number
    // of intervals of this that are to the right of source.begin(). So, as long
    // as the product of those two numbers is not greater than this->size(),
    // in-place is probably faster.
    auto &self = *this;
    auto prefer_full_copy = [&self, &source] {
      // If the storage allows fast insertion, we never prefer to make a full
      // copy.
      if (has_fast_insertion) return false;
      // Iterator to next element in 'source' after the last one that will be
      // inserted before any element in 'self'.
      auto insert_end = source.upper_bound(self.back());
      // Maximum number of elements in 'source' that may be inserted in 'self'.
      auto max_elements_inserted = std::distance(source.begin(), insert_end);
      // First element of 'self' that may be moved during an insertion of an
      // element from 'source'.
      auto first_element_moved = self.upper_bound(source.front());
      // The maximum number of elements of 'self' that may be moved during an
      // insertion of an element of 'source'.
      auto max_elements_moved_per_insertion =
          std::distance(first_element_moved, self.end());
      // If both 'self' and 'source' have their elements uniformly distributed
      // between 'first_element_moved' and 'self.end()', then it is expected
      // that fewer and fewer elements will be moved in each iteration; the
      // number of elements to move decreases linearly so the expected average
      // number of elements to move is only half the maximum.
      auto expected_elements_moved_per_insertion =
          max_elements_moved_per_insertion >> 1;
      // If the worst-case expected number of moved elements exceeds the size
      // of the container, we prefer to make a full copy.
      return max_elements_inserted * expected_elements_moved_per_insertion >
             self.ssize();
    };

    if (prefer_full_copy()) {
      // Todo: This is sometimes faster than the in-place operation, even on
      // containers with has_fast_insertion==true. See intervals_perf-t.cc. The
      // reason is that the for loop below always iterates over the full
      // `source` set, whereas Union_view is able to take long steps. This could
      // be optimized similar to Union_view, i.e., `inplace_union_or_subtract`
      // could advance `it` to the upper bound
      This_t container(make_binary_operation_view<operation>(*this, source));
      this->assign(std::move(container));
    } else {
      // If the operation is 'intersection', subtract the complement instead.
      // (It's always true that "A intersect B" == "A - complement(B)".)
      // Therefore the type of `read_source` needs to be either
      // `Complement_view<Source_t> &` or `Source_t &`; we use a lambda with
      // deduced return type to declare the type of `read_source` conditionally.
      [[maybe_unused]] auto complement = make_complement_view(source);
      auto &read_source = [&]() -> auto & {
        if constexpr (operation == Binary_operation::op_intersection) {
          return complement;
        } else {
          return source;
        }
      }
      ();
      constexpr Binary_operation impl_operation =
          (operation == Binary_operation::op_intersection
               ? Binary_operation::op_subtraction
               : operation);
      constexpr bool types_allow_donation =
          Can_donate_set_elements<Source_t &&, This_t>;
      bool can_donate = false;
      if constexpr (types_allow_donation) {
        can_donate = (this->get_allocator() == source.get_allocator());
      }

      // Process one interval at a time from source. We don't use `range for`
      // because inplace_union_or_subtract may steal the current element, so we
      // must increment the iterator before invoking the function.
      auto cursor = this->begin();
      auto interval_set_view = Interval_set_view(read_source);
      auto it = interval_set_view.begin();
      while (it != interval_set_view.end()) {
        auto iv = *it;
        ++it;
        if (can_donate) {
          // If new elements are needed, donate from source.
          // Guard by `if constexpr (types_allow_donation)` since the call with
          // `Can_donate::yes` may not compile otherwise.
          if constexpr (types_allow_donation) {
            inplace_union_or_subtract<impl_operation, Hint_guaranteed::yes,
                                      Can_donate::yes>(
                cursor, iv.start(), iv.exclusive_end(), &source);
          }
        } else {
          // If new elements are needed, allocate.
          inplace_union_or_subtract<impl_operation, Hint_guaranteed::yes,
                                    Can_donate::no>(
              cursor, iv.start(), iv.exclusive_end(), &source);
        }

        if constexpr (operation != Binary_operation::op_union) {
          // As an optimization, if we reach the end of this container while
          // removing intervals, just return.
          if (cursor == this->end()) return;
        }
      }
    }
  }

  /// Either add or remove the interval, reading and updating the given cursor.
  ///
  /// Complexity:
  ///
  /// - list: O(number_of_removed_intervals + std::distance(cursor,
  /// first_removed_interval).
  ///
  /// - set: O(1) if it doesn't change the number of intervals; O(log(size()) +
  /// number_of_removed_intervals) otherwise.
  ///
  /// - Ordered_vector: constant if it doesn't change the number of intervals;
  /// linear in the distance from the first removed element to end() otherwise
  ///
  /// @tparam operation The operation: op_union or op_subtraction
  ///
  /// @tparam hint_guaranteed If yes, raise an assertion if @c hint does not
  /// satisfy the requirements. If no, ignore @c hint if it does not satisfy the
  /// requirements. See below.
  ///
  /// @tparam can_donate If no, allocate new elements when needed; if yes, steal
  /// elements from `source` when needed.
  ///
  /// @param[in,out] cursor Hint for the insertion position. If it refers to
  /// `upper_bound(start)`, this function finds the insertion position in O(1).
  /// If it refers to a boundary less than `upper_bound(start)`, it may reduce
  /// the search space when finding the insertion position. If it refers to a
  /// boundary greater than `upper_bound(start)`, the behavior depends on @c
  /// hint_guaranteed : If @c hint_guaranteed is yes, the behavior is undefined
  /// (an assertion is raised in debug mode); otherwise, the hint is ignored and
  /// the entire set has to be searched. @c cursor will be updated to
  /// `upper_bound(exclusive_end)`, which makes it good to reuse in future calls
  /// to this function, with intervals following this one.
  ///
  /// @param start The left boundary of the interval, inclusive.
  ///
  /// @param exclusive_end The right boundary of the interval, exclusive.
  ///
  /// @param source Container from which elements may be stolen.
  ///
  /// @throws bad_alloc This leaves the container unmodified.
  template <
      Binary_operation operation, Hint_guaranteed hint_guaranteed,
      Can_donate can_donate = Can_donate::no,
      Is_boundary_set_over_traits_unqualified<Set_traits_t> Source_t = This_t>
  void inplace_union_or_subtract(Iterator_t &cursor, const Element_t &start,
                                 const Element_t &exclusive_end,
                                 Source_t *source = nullptr) {
    assert(Set_traits_t::lt(start, exclusive_end));
    if constexpr (hint_guaranteed == Hint_guaranteed::yes) {
      assert(cursor == this->begin() ||
             Set_traits_t::lt(*std::ranges::prev(cursor), start));
    } else {
      if (cursor != this->begin() &&
          Set_traits_t::ge(*std::ranges::prev(cursor), start)) {
        cursor = this->begin();
      }
    }
    Iterator_t left = this->lower_bound(cursor, start);
    Iterator_t right = this->upper_bound(left, exclusive_end);
    constexpr bool is_union = operation == Binary_operation::op_union;
    // The following comments are written for the case operation==op_union. They
    // apply analogously for the case operation==op_subtraction, if we
    // interchange the words "interval" and "gap", and keep in mind that the
    // interval is to be erased.
    //
    // The diagrams illustrate each case, using [ for the start points left and
    // start, and ] for the endpoints right/exclusive_end. Question marks
    // indicate a range where an unknown number of boundaries occur; an odd
    // number of question marks represents any odd number of boundaries and an
    // even number of question marks represents any even number of boundraies.
    // In all cases, `right` is strictly greater than `exclusive_end`, and
    // `left` is greater than or equal to `start`.
    if (left.is_endpoint() == is_union) {
      // The beginning of the interval touches or intersects an interval that
      // extends to the left of it, which ends at `left`. So `left` must be
      // removed and `start` must not occur in the resulting set.
      if (right.is_endpoint() == is_union) {
        // The end of the interval touches or intersects an interval that
        // extends to the right of it, which ends at `right`. So `exclusive_end`
        // must not occur in the resulting set, and boundaries from `left`
        // (inclusive), to `right` (exclusive), must be removed.
        // new boundaries (start, exclusive_end):         [           ]
        //     existing boundaries (left, right):            ] ? ? ?     ]
        //                                result:                        ]
        cursor = storage().erase(left, right);
      } else {
        // The end of the interval does not touch or intersect any interval that
        // extends to the right of it. So `exclusive_end` will be an endpoint in
        // the output, and `right` is preserved. We overwrite `left` and erase
        // any boundaries covered by the interval.
        // new boundaries (start, exclusive_end):         [             ]
        //     existing boundaries (left, right):            ] ? ? ? ?     [
        //                                result:                       ]  [
        cursor = storage().erase(std::next(left), right);
        cursor =
            storage().update_point(std::ranges::prev(cursor), exclusive_end);
      }
    } else {
      // The beginning of the interval does not touch or intersect any interval
      // that extends to the left of it (but may touch the beginning of an
      // interval). So `start` must be a boundary in the resulting set.
      if (right.is_endpoint() == is_union) {
        // The end of the interval touches or intersects the beginning of a
        // following interval, which ends at `right`. So `exclusive_end` will
        // not occur in the resulting set. Boundaries between `left` (exclusive)
        // and `right` (exclusive) are removed, and `left` is replaced by
        // `start`.
        // new boundaries (start, exclusive_end):         [             ]
        //     existing boundaries (left, right):            [ ? ? ? ?     ]
        //                                result:         [                ]
        cursor = storage().erase(std::next(left), right);
        cursor = storage().update_point(std::ranges::prev(cursor), start);
      } else {
        // The end of the interval does not touch or intersect any interval that
        // extends to the right of it. So `exclusive_end` must be a boundary in
        // the output.
        if (left != right) {
          // There are intervals between `left` and `right`. So they need to be
          // removed. Reuse one of them to store the interval.
          // new boundaries (start, exclusive_end):       [           ]
          //     existing boundaries (left, right):          [ ? ? ?     [
          //                                result:       [           ]  [
          cursor = storage().erase(std::next(left, 2), right);
          cursor = storage().update_point(std::ranges::prev(cursor, 2), start);
          cursor = storage().update_point(cursor, exclusive_end);
        } else {
          // There are no intervals between `left` and `last`. In other words,
          // the interval is fully contained in a gap. So we insert it there.
          // new boundaries (start, exclusive_end):       [             ]
          //     existing boundaries (left==right):                        [
          //                                result:       [             ]  [
          if constexpr (can_donate == Can_donate::yes) {
            // Steal, don't allocate
            cursor = storage().steal_and_insert(left, start, exclusive_end,
                                                source->storage());
          } else {
            // Allocate, don't steal
            cursor = storage().insert(left, start, exclusive_end);
          }
        }
      }
    }
  }
};  // class Boundary_container

}  // namespace mysql::sets::throwing

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_THROWING_BOUNDARY_CONTAINER_H

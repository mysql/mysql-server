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

#ifndef MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_ITERATOR_H
#define MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_ITERATOR_H

/// @file
/// Experimental API header

#include <cassert>                                 // assert
#include "mysql/iterators/iterator_interface.h"    // Iterator_interface
#include "mysql/sets/binary_operation.h"           // Binary_operation
#include "mysql/sets/boundary_set_meta.h"          // Is_boundary_set
#include "mysql/sets/optional_view_source_set.h"   // Optional_view_source_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Forward iterator over the result of a binary set operation (union,
/// intersection, or subtraction) over two boundary sets.
template <Is_boundary_set Source1_tp, Is_boundary_set Source2_tp,
          Binary_operation operation_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Boundary_set_binary_operation_iterator
    : public mysql::iterators::Iterator_interface<
          Boundary_set_binary_operation_iterator<Source1_tp, Source2_tp,
                                                 operation_tp>> {
  // This class holds one iterator to each source boundary set: m_pos1 to
  // m_source1, and m_pos2 to m_source2. In this class implementation, we refer
  // to objects of this class as output iterators, and its values as output
  // boundaries.
  //
  // In general, a C++ iterator positioned at the end of the range, cannot be
  // dereferenced. However, we will need to compare two iterators that can
  // possibly be positioned at the end, treating end-positioned iterators as
  // greater than iterators not positioned at the end. To make the notation
  // convenient, we define (for exposition purposes) the "extended value" of an
  // iterator as:
  //
  //   V(iter) = *iter, when iter is not at the end; V(iter) = infinity, when
  //   iter is at the end.
  //
  //   Here, infinity is an exposition-only value that is strictly greater than
  //   any value returned from *iter when iter does not point to the end.
  //
  // Define posA as the smaller-valued, and posB as the greater-valued iterator,
  // comparing the extended values. If the extended values are equal, posA and
  // posB may be defined arbitrarily. Let sourceA, sourceB be their
  // corresponding sets.
  //
  // The current output boundary is *posA.
  //
  // The class maintains two invariants:
  //
  // I1. posB == source2.lower_bound(*posA)
  //
  //     This invariant is important for two reasons: (1) it ensures that each
  //     output boundary has a unique representation. (2) the algorithm that
  //     steps to the next output boundary can use lower_bound to take long
  //     steps.
  //
  // I2. posA points to a boundary that is included in the output set. This is
  //     determined by case analysis, depending on the following things:
  //     - The operation (union/intersection/subtraction);
  //     - Whether posA==posB;
  //     - Whether posA/posB are endpoints;
  //     - For subtraction, whether sourceA refers to m_source1 or m_source2.
  //
  //     The case analysis is as follows:
  //
  //     - For union, posA is an output boundary if:
  //
  //         V(posA) == V(posB)
  //           ? posA.is_endpoint() == posB.is_endpoint()
  //           : !posB.is_endpoint()
  //
  //       In other words:
  //
  //       - If the extended values are equal, posA is an output boundary if
  //         either both posA/posB are endpoints, or none is and endpoint. The
  //         reason is that, when two intervals from sourceA and sourceB begin
  //         or end at the same point, the union also has an interval that
  //         begins/ends at the same point; but when an interval from one source
  //         ends where an interval from the other source begins, the union
  //         contains those two intervals merged, where that point is not a
  //         boundary.
  //
  //         Graphically, posA is included/excluded as follows:
  //                       A      A                    A           A
  //         Included:  ---]  or  [---    Excluded: ---]     or    [---
  //                    ---]      [---                 [---     ---]
  //                       B      B                    B           B
  //
  //       - If the extended values are different, posA is an output boundary if
  //         posB is not an endpoint. The reason is that a boundary (posA) that
  //         is covered by an interval (ending at posB) cannot be a boundary in
  //         the union.
  //
  //         Graphically, posA is included/excluded as follows:
  //                       A           A                       A         A
  //         Included:  ---]       or  [---      Excluded:  ---]    or   [---
  //                         [---        [---                 ---]      ---]
  //                         B           B                       B         B
  //
  //       In all cases where posA is an output boundary, the output boundary
  //       is an endpoint if and only if posA is an endpoint.
  //
  //     - For intersection, posA is an output boundary if:
  //
  //       V(posA) == V(posB)
  //         ? a.is_endpoint() == posB.is_endpoint()
  //         : posB.is_endpoint()
  //
  //       In other words:
  //
  //       - If the extended values are equal, posA is an output boundary if
  //         either both posA/posB are endpoints, or none is an endpoint. The
  //         reason is that, when two intervals from sourceA and sourceB begin
  //         or end at the same point, the intersection also has an interval
  //         that begins/ends at the same point; but when an interval from one
  //         source ends where an interval from the other source begins, the
  //         intersection does not contain any of those two intervals, so that
  //         point is not an output boundary.
  //
  //         Graphically, posA is included/excluded as follows:
  //                       A      A                     A            A
  //         Included:  ---]  or  [---    Excluded:  ---]     or     [---
  //                    ---]      [---                  [---      ---]
  //                       B      B                     B            B
  //
  //       - If the extended values are different, posA is an output boundary if
  //         posB is an endpoint, because a boundary (posA) needs to be covered
  //         by an interval from the other source (ending at posB) in order
  //         to be a boundary in the intersection.
  //
  //         Graphically, posA is included/excluded as follows:
  //                       A         A                  A              A
  //         Included:  ---]    or   [---    Excluded:  [---    or  ---]
  //                      ---]      ---]                  [---           [---
  //                         B         B                  B              B
  //
  //       When posA is an output boundary, the output boundary is an endpoint
  //       if and only if posA is an endpoint.
  //
  //     - For subtraction, posA is an output boundary if:
  //
  //       V(posA) == V(posB)
  //         ? posA.is_endpoint() != posB.is_endpoint()
  //         : (sourceA == m_source1) ? !posB.is_endpoint() : posB.is_endpoint()
  //
  //       In other words:
  //
  //       - If the extended values are equal, posA is an output boundary if
  //         either posA is an endpoint and posB is not, or posB is an endpoint
  //         and posA is not. The reason is that, when two intervals from
  //         sourceA and sourceB end at the same point, the one from m_source2
  //         "deletes" the one from m_source1, so that point is not an output
  //         boundary; but when one ends where the other begins, the one from
  //         m_source1 is preserved in the output set.
  //
  //         Graphically, posA is included/excluded as follows:
  //                       A            A                     A      A
  //         Included:  ---]     or     [---    Excluded:  ---]  or  [---
  //                       [---      ---]                  ---]      [---
  //                       B            B                     B      B
  //
  //         When posA is an output boundary, the output boundary is an endpoint
  //         if and only if m_pos1 is an endpoint.
  //
  //       - If the extended values are different, the case depends on whether
  //         posA refers to m_pos1 or to m_pos2 (subtraction is not symmetric in
  //         the operands):
  //
  //         - If sourceA refers to m_source1, posA is included if posB is not
  //           an endpoint. The reason is that posB==m_pos2 is then the endpoint
  //           of an interval from the subtracted set, so posA is subtracted.
  //
  //           Graphically, posA==m_pos1 is included/excluded as follows:
  //                         A           A                       A         A
  //           Included:  ---]       or  [---      Excluded:  ---]    or   [---
  //                           [---        [---                 ---]      ---]
  //                           B           B                       B         B
  //
  //           When posA is an output boundary, the output boundary is an
  //           endpoint if and only if m_pos1 is an endpoint.
  //
  //         - If sourceA refers to m_source2, posA is included if posB is an
  //           endpoint. The reason is that posA==m_pos1 is then the endpoint
  //           of an interval from which we remove posB's interval, so posA
  //           becomes the boundary of a new "gap" in the result of the
  //           subtraction.
  //
  //           Graphically, posA==m_pos2 is included/excluded as follows:
  //                         A         A                     A           A
  //           Included:  ---]    or   [---    Excluded:  ---]       or  [---
  //                        ---]      ---]                     [---        [---
  //                           B         B                     B           B
  //
  //           When posA is an output boundary, the output boundary is an
  //           endpoint if and only if m_pos1 is not an endpoint.
  //
  // These invariants are maintained as follows:
  //
  // - To advance the iterator one step:
  //
  //    1. If m_pos1==m_pos2, advance both iterators one step; otherwise advance
  //       posA. Note that in both cases, I1 is maintained.
  //
  //    2. If the current position is an output boundary, we are done.
  //       Otherwise, advance posA to the lower bound of posA, and go to step 1.
  //
  //   The loop body of advance_to_boundary implements step 2 followed by step
  //   1.
  //
  //   The case analysis to determine if the current iterators represent an
  //   output boundary is implemented in the two is_output_boundary functions.
  //
  // - To initialize the 'begin' iterator, start with m_pos1=m_source1.begin()
  //   and m_pos2=m_source2.begin(). Note that I1 holds already. Then, execute
  //   (2) above.
 public:
  using Source1_t = Source1_tp;
  using Source2_t = Source2_tp;
  using Opt_source1_t = Optional_view_source_set<Source1_t>;
  using Opt_source2_t = Optional_view_source_set<Source2_t>;
  static constexpr auto operation = operation_tp;
  using Set_traits_t = typename Source1_t::Set_traits_t;
  using Element_t = typename Set_traits_t::Element_t;
  using Less_t = typename Set_traits_t::Less_t;
  using This_t =
      Boundary_set_binary_operation_iterator<Source1_t, Source2_t, operation>;
  using Iterator1_t = mysql::ranges::Range_const_iterator_type<Source1_t>;
  using Iterator2_t = mysql::ranges::Range_const_iterator_type<Source2_t>;

  // Default constructor. The resulting iterator is in a "pointless" state,
  // where the only possible operations are assignment from another iterator,
  // and comparison.
  Boundary_set_binary_operation_iterator() = default;

  // Default rule-of-5 members
  Boundary_set_binary_operation_iterator(
      const Boundary_set_binary_operation_iterator &source) noexcept = default;
  Boundary_set_binary_operation_iterator(
      Boundary_set_binary_operation_iterator &&) noexcept = default;
  Boundary_set_binary_operation_iterator &operator=(
      const Boundary_set_binary_operation_iterator &source) noexcept = default;
  Boundary_set_binary_operation_iterator &operator=(
      Boundary_set_binary_operation_iterator &&source) noexcept = default;
  ~Boundary_set_binary_operation_iterator() = default;

  // Construct from two sources and one iterator into each of them.
  Boundary_set_binary_operation_iterator(const Source1_t *source1,
                                         const Source2_t *source2,
                                         const Iterator1_t &pos1,
                                         const Iterator2_t &pos2)
      : m_source1(source1), m_source2(source2), m_pos1(pos1), m_pos2(pos2) {
    advance_to_boundary();
  }

  /// @return The value that this iterator currently points to.
  [[nodiscard]] Element_t get() const {
    if (m_order <= 0)
      return *m_pos1;
    else
      return *m_pos2;
  }

  /// @return true if this iterator equals other.
  [[nodiscard]] bool is_equal(const This_t &other) const {
    if (m_order <= 0)
      return m_pos1 == other.m_pos1;
    else
      return m_pos2 == other.m_pos2;
  }

  /// @return true if this iterator is at an endpoint boundary.
  [[nodiscard]] bool is_endpoint() const {
    if constexpr (operation == Binary_operation::op_subtraction) {
      // For op_subtraction: when m_order < 0, m_pos2 is a non-endpoint, and the
      // current boundary inherits endpointness from m_pos1. When m_order > 0,
      // m_pos1 must be an endpoint, and the boundary inherits the *reversed*
      // endpointness from m_pos2. When m_order == 0 and m_pos1 and m_pos2 are
      // both non-end iterators, m_pos1 and m_pos2 have opposite endpointness,
      // and we inherit the endpointness from m_pos1. When m_order == 0 and
      // m_pos1 and m_pos2 are end iterators, they are both non-endpoints and we
      // need to return a non-endpoint.
      if (m_order <= 0)
        return m_pos1.is_endpoint();
      else
        return !m_pos2.is_endpoint();
    } else {
      // For op_union/op_intersection: When m_order != 0, the current boundary
      // inherits endpointness from the boundary of the underlying set. When
      // m_order == 0, the boundary is in the set only when both underlying sets
      // have the same endpointness.
      if (m_order <= 0)
        return m_pos1.is_endpoint();
      else
        return m_pos2.is_endpoint();
    }
  }

  /// Move to the next iterator position.
  void next() {
    // if m_order == 0 then, yes, advance both!
    if (m_order <= 0) ++m_pos1;
    if (m_order >= 0) ++m_pos2;

    advance_to_boundary();
  }

  /// Return the current iterator to the first source.
  [[nodiscard]] Iterator1_t position1() const { return m_pos1; }

  /// Return the current iterator to the second source.
  [[nodiscard]] Iterator2_t position2() const { return m_pos2; }

 private:
  /// Set m_order to -1, 0, or 1, according to the relative order of m_pos1 and
  /// m_pos2.
  ///
  /// -1 indicates that m_pos1 is smaller, 0 that they are equal, and 1 that
  /// m_pos2 is smaller.
  void compute_order() {
    if (m_pos1 == m_source1.end()) {
      if (m_pos2 == m_source2.end())
        m_order = 0;  // both are at the end
      else
        m_order = 1;  // only pos1 is at the end
    } else if (m_pos2 == m_source2.end()) {
      m_order = -1;  // only pos2 is at the end
    } else {
      if (lt(*m_pos1, *m_pos2))
        m_order = -1;  // *pos1 < *pos2
      else if (*m_pos1 == *m_pos2)
        m_order = 0;  // *pos1 == *pos2
      else
        m_order = 1;  // *pos1 > *pos2
    }
  }

  /// Until the pair of iterators defines an output boundary, advance the
  /// smallest-valued iterator past the greatest-valued one.
  void advance_to_boundary() {
    compute_order();
    while (true) {
      if (m_order < 0) {
        if (advance_if_not_output_boundary(m_source1, m_pos1, m_source2,
                                           m_pos2))
          return;
      } else if (m_order > 0) {
        if (advance_if_not_output_boundary(m_source2, m_pos2, m_source1,
                                           m_pos1))
          return;
      } else {
        assert(m_order == 0);
        if (advance_if_not_output_boundary()) return;
      }
    }
  }

  /// Assuming both iterators point to the same value: if they are not an output
  /// boundary, advance both, and update m_order.
  ///
  /// @retval true if the position was at an output boundary.
  ///
  /// @retval false if the position was not at an output boundary, in which case
  /// the position was advanced.
  [[nodiscard]] bool advance_if_not_output_boundary() {
    assert(m_order == 0);
    if (is_output_boundary()) return true;
    ++m_pos1;
    ++m_pos2;
    compute_order();
    return false;
  }

  /// Assuming the iterators point to different values where pos_a < pos_b: if
  /// pos_a is not at an output boundary, advance it to the lower bound of
  /// pos_b, and update m_order.
  ///
  /// @param source_a the source boundary set that pos_a iterates over.
  ///
  /// @param pos_a the smallest-valued iterator.
  ///
  /// @param source_b the source boundary set that pos_b iterates over.
  ///
  /// @param pos_b the greatest-valued iterator.
  ///
  /// @retval true if the position was at an output boundary, or if advancing
  /// pos_a made us reach the end output boundary.
  ///
  /// @retval false if the position was not at an output boundary, and pos_a was
  /// advanced but not to the end output boundary.
  [[nodiscard]] bool advance_if_not_output_boundary(const auto &source_a,
                                                    auto &pos_a,
                                                    const auto &source_b,
                                                    auto &pos_b) {
    assert(source_a.has_object());
    assert(m_order != 0);
    assert(pos_a != source_a->end());
    if (is_output_boundary(pos_b.is_endpoint())) return true;
    if (pos_b == source_b.end()) {
      pos_a = source_a->end();
      m_order = 0;
      return true;
    } else {
      pos_a = source_a->lower_bound(pos_a, *pos_b);
      compute_order();
      return false;
    }
  }

  /// Assuming both iterators point to the same value, or both point to the end,
  /// return true if that position defines an output boundary.
  [[nodiscard]] constexpr bool is_output_boundary() {
    assert(m_order == 0);
    if (m_pos1 == m_source1.end()) return true;
    if constexpr (operation == Binary_operation::op_subtraction) {
      return m_pos1.is_endpoint() != m_pos2.is_endpoint();
    } else {
      return m_pos1.is_endpoint() == m_pos2.is_endpoint();
    }
  }

  /// Assuming the iterators point to different values, return true if the
  /// smallest-valued iterator points to an output boundary.
  ///
  /// The decision whether a boundary should be included in the output depends
  /// on the operation type, whether the next-greater boundary in the other
  /// source is an endpoint or not, and whether the queried boundary is in
  /// source1 or source2.
  ///
  /// @param pos_b_is_endpoint true if the next-greater boundary in the other
  /// source (not the source containing the boundary we query for) is an
  /// endpoint.
  [[nodiscard]] constexpr bool is_output_boundary(bool pos_b_is_endpoint) {
    assert(m_order != 0);
    if constexpr (operation == Binary_operation::op_union) {
      // pos_a is an output boundary if it is not covered by an interval ending
      // in pos_b.
      return !pos_b_is_endpoint;
    } else if constexpr (operation == Binary_operation::op_intersection) {
      // pos_a is an output boundary if it is covered by an interval ending in
      // pos_b.
      return pos_b_is_endpoint;
    } else if constexpr (operation == Binary_operation::op_subtraction) {
      // When m_pos1 < m_pos2 (m_order < 0), posA=m_pos1 is an output boundary
      // if it is not covered by an interval ending in posB.
      // When m_pos2 > m_pos2 (m_order > 0), posA=m_pos2 is an output boundary
      // if it is covered by an interval ending in posB=m_pos1.
      return pos_b_is_endpoint == (m_order > 0);
    }
  }

  /// @return true if the first value is smaller than the second value,
  /// according to the order defined by the Set traits.
  [[nodiscard]] static constexpr bool lt(const Element_t &element1,
                                         const Element_t &element2) {
    return Set_traits_t::lt(element1, element2);
  }

  /// Pointer to the left-hand-side source.
  Opt_source1_t m_source1;

  /// Pointer to the right-hand-side source.
  Opt_source2_t m_source2;

  /// Iterator to current position in m_source1.
  Iterator1_t m_pos1;

  /// Iterator to current position in m_source2.
  Iterator2_t m_pos2;

  /// -1 if m_pos1 < m_pos2; 0 if m_pos1 == m_pos2; 1 if m_pos1 > m_pos2. (We
  /// don't use std::strong_ordering because we want to be able to negate it.)
  int m_order{0};
};  // class Boundary_set_binary_operation_iterator

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_ITERATOR_H

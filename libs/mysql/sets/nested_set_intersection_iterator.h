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

#ifndef MYSQL_SETS_NESTED_SET_INTERSECTION_ITERATOR_H
#define MYSQL_SETS_NESTED_SET_INTERSECTION_ITERATOR_H

/// @file
/// Experimental API header

#include "mysql/sets/binary_operation.h"  // Binary_operation
#include "mysql/sets/nested_set_binary_operation_iterator_base.h"  // Nested_set_binary_operation_iterator_base
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Iterator over the intersection of two sets.
///
/// @tparam Source1_tp The first source.
///
/// @tparam Source2_tp The second source.
template <Is_nested_set Source1_tp, Is_nested_set Source2_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Nested_set_intersection_iterator
    : public Nested_set_binary_operation_iterator_base<
          Nested_set_intersection_iterator<Source1_tp, Source2_tp>, Source1_tp,
          Source2_tp, Binary_operation::op_intersection> {
  using Self_t = Nested_set_intersection_iterator<Source1_tp, Source2_tp>;
  using Base_t = Nested_set_binary_operation_iterator_base<
      Self_t, Source1_tp, Source2_tp, Binary_operation::op_intersection>;

 public:
  // This iterator holds iterators into each source. Each source iterator has to
  // skip keys that don't exist in the other set, and keys for which the
  // intersections of mapped sets is empty. Therefore, advancing the iterator
  // has two phases:
  //
  //  1. Advance both source iterators one step.
  //
  //  2. While the iterators point to different keys, or the mapped sets
  //     intersect, step the iterators forward (according the scheme in
  //     advance_if_needed).
  //
  // This iterator performs only phase 1 at the time the increment is requested,
  // and performs phase 2 "lazily", i.e., only when it is needed, which is at
  // the next dereference, advancement, or comparison operation.

  using typename Base_t::Iterator1_t;
  using typename Base_t::Iterator2_t;
  using typename Base_t::Key_traits_t;
  using typename Base_t::Source1_t;
  using typename Base_t::Source2_t;
  using typename Base_t::Value_t;

  Nested_set_intersection_iterator() = default;

  // Construct from two sources and one iterator into each of them.
  Nested_set_intersection_iterator(const Source1_t *source1,
                                   const Source2_t *source2,
                                   const Iterator1_t &iterator1,
                                   const Iterator2_t &iterator2)
      : Base_t(source1, source2, iterator1, iterator2) {}

  /// @return The value that this iterator currently points to.
  [[nodiscard]] auto get() const {
    clean();
    return this->make_value();
  }

  /// Move to the next iterator position.
  void next() {
    clean();
    ++this->m_iterator1;
    ++this->m_iterator2;
    m_is_dirty = true;
  }

  /// @return true if this iterator equals other.
  [[nodiscard]] bool is_equal(
      const Nested_set_intersection_iterator &other) const {
    clean();
    return this->m_iterator1 == other.m_iterator1;
  }

 private:
  /// True when the first phase of advancing the position but not the second one
  /// has completed.
  mutable bool m_is_dirty{true};

  /// Perform the second phase of advancing the position, unless it has already
  /// been done.
  void clean() const {
    if (m_is_dirty) {
      advance_if_needed();
      m_is_dirty = false;
    }
  }

  /// Whether the iteration is done or not.
  // NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
  enum class Done { no, yes };

  /// Perform the second phase of advancing the position.
  void advance_if_needed() const {
    auto &source1 = this->m_source1;
    auto &source2 = this->m_source2;
    auto &it1 = this->m_iterator1;
    auto &it2 = this->m_iterator2;

    // While both iterators point to the same key, increment both iterators
    // using ++.
    while (true) {
      if (it1 == source1.end()) {
        it2 = source2.end();
        return;
      }
      if (it2 == source2.end()) {
        it1 = source1.end();
        return;
      }
      auto order = Key_traits_t::cmp(it1->first, it2->first);
      if (order != 0) {
        // The iterators point to different keys, so the loop condition no
        // longer holds. So we break this loop. Before we break this loop, make
        // sure that the loop condition for the second loop holds.
        if (order > 0) {
          if (step_and_check_done(source2, source1, it2, it1) == Done::yes)
            return;
        }
        break;
      }
      if (is_intersecting(it1->second, it2->second)) return;
      ++it1;
      ++it2;
    }

    // While one iterator is less than the other, increment the smaller one
    // by using it as the cursor for find().
    while (true) {
      // Invariant: *it1 < *it2 && it2 != end
      if (step_and_check_done(source1, source2, it1, it2) == Done::yes) return;
      // Invariant: *it2 < *it1 && it1 != end
      if (step_and_check_done(source2, source1, it2, it1) == Done::yes) return;
    }
  }

  /// Assuming none of it and other_it is at the end, and *it < *other_it,
  /// advance it to point to an element with the same key as other_it if one
  /// exists and the mapped sets intersect; otherwise advance it to the next
  /// element if that is not end; otherwise set both iterators to the end.
  ///
  /// @param source The source that `it` points into.
  ///
  /// @param other_source The source that `other_it` points into.
  ///
  /// @param it The smaller of the iterators, which we will step.
  ///
  /// @param other_it The greater of the iterators.
  ///
  /// @retval Done::yes Iteration is done, because either `it` and `other_it`
  /// point to the same key and the mapped sets intersect, or `it` reached the
  /// end.
  ///
  /// @retval Done::no Iteration is not done, because `it` does not have a key
  /// equal to the one that `other_it` points to. In this case, `it` has
  /// advanced to the next element, and that is not the end iterator.
  Done step_and_check_done(const auto &source, const auto &other_source,
                           auto &it, auto &other_it) const {
    auto pos = source->find(it, other_it->first);
    if (pos != source->end()) {
      // Found `pos` in `source`, which has the same key as `other_it`.
      if (is_intersecting(other_it->second, pos->second)) {
        // The mapped sets intersect, so (pos, other_it) is a valid position.
        it = pos;
        return Done::yes;
      }
    }
    if (it == source->end()) {
      // Reached end of source without finding any pair of keys for which the
      // mapped sets intersect.
      other_it = other_source->end();
      return Done::yes;
    }
    return Done::no;
  }
};  // class Nested_set_intersection_iterator

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_INTERSECTION_ITERATOR_H

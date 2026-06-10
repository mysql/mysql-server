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

#ifndef MYSQL_SETS_NESTED_SET_SUBTRACTION_ITERATOR_H
#define MYSQL_SETS_NESTED_SET_SUBTRACTION_ITERATOR_H

/// @file
/// Experimental API header

#include "mysql/sets/binary_operation.h"  // Binary_operation
#include "mysql/sets/nested_set_binary_operation_iterator_base.h"  // Nested_set_binary_operation_iterator_base
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Iterator over the difference of two sets.
///
/// @tparam Source1_tp The first source.
///
/// @tparam Source2_tp The second source.
template <Is_nested_set Source1_tp, Is_nested_set Source2_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Nested_set_subtraction_iterator
    : public Nested_set_binary_operation_iterator_base<
          Nested_set_subtraction_iterator<Source1_tp, Source2_tp>, Source1_tp,
          Source2_tp, Binary_operation::op_subtraction> {
  using Self_t = Nested_set_subtraction_iterator<Source1_tp, Source2_tp>;
  using Base_t = Nested_set_binary_operation_iterator_base<
      Self_t, Source1_tp, Source2_tp, Binary_operation::op_subtraction>;

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
  using typename Base_t::Source1_t;
  using typename Base_t::Source2_t;
  using typename Base_t::Value_t;

  Nested_set_subtraction_iterator() = default;

  // Construct from two sources and one iterator into each of them.
  Nested_set_subtraction_iterator(const Source1_t *source1,
                                  const Source2_t *source2,
                                  const Iterator1_t &iterator1,
                                  const Iterator2_t &iterator2)
      : Base_t(source1, source2, iterator1, iterator2) {}

  /// @return The value that this iterator currently points to.
  [[nodiscard]] auto get() const {
    clean();
    if (this->m_iterator2 == this->m_source2.end()) return this->make_value1();
    return this->make_value();
  }

  /// Move to the next iterator position.
  void next() {
    clean();
    ++this->m_iterator1;
    m_is_dirty = true;
  }

  /// @return true if this iterator equals other.
  [[nodiscard]] bool is_equal(
      const Nested_set_subtraction_iterator &other) const {
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
      set_iterator2_and_advance_if_needed();
      m_is_dirty = false;
    }
  }

  /// Perform the second phase of advancing the position.
  ///
  /// Algorithm: try set the second iterator to point to the same key as the
  /// first iterator:
  ///
  /// - If no such key exists in the second set, leave the second iterator at
  /// the end position and return.
  ///
  /// - If the key exists in the second set, and the subtraction of the second
  /// mapped set from the first mapped set does not result in empty set (i.e.,
  /// the first mapped set is not a subset of the second mapped set), leave the
  /// second iterator at this position and continue.
  ///
  /// - Otherwise, step iterator1 and start over.
  void set_iterator2_and_advance_if_needed() const {
    if (!this->m_source2.has_object()) return;
    while (true) {
      if (this->m_iterator1 == this->m_source1.end()) {
        this->m_iterator2 = this->m_source2.end();
        return;
      }
      this->m_iterator2 = this->m_source2->find(this->m_iterator1->first);
      if (this->m_iterator2 == this->m_source2.end()) return;
      if (!is_subset(this->m_iterator1->second, this->m_iterator2->second))
        return;
      ++this->m_iterator1;
    }
  }
};  // class Nested_set_subtraction_iterator

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_SUBTRACTION_ITERATOR_H

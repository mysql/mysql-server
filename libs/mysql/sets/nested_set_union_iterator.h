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

#ifndef MYSQL_SETS_NESTED_SET_UNION_ITERATOR_H
#define MYSQL_SETS_NESTED_SET_UNION_ITERATOR_H

/// @file
/// Experimental API header

#include <cassert>                        // assert
#include "mysql/sets/binary_operation.h"  // Binary_operation
#include "mysql/sets/nested_set_binary_operation_iterator_base.h"  // Nested_set_binary_operation_iterator_base
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Iterator over the union of two sets.
///
/// @tparam Source1_tp The first source.
///
/// @tparam Source2_tp The second source.
template <Is_nested_set Source1_tp, Is_nested_set Source2_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Nested_set_union_iterator
    : public Nested_set_binary_operation_iterator_base<
          Nested_set_union_iterator<Source1_tp, Source2_tp>, Source1_tp,
          Source2_tp, Binary_operation::op_union> {
  using Self_t = Nested_set_union_iterator<Source1_tp, Source2_tp>;
  using Base_t =
      Nested_set_binary_operation_iterator_base<Self_t, Source1_tp, Source2_tp,
                                                Binary_operation::op_union>;

 public:
  using typename Base_t::Iterator1_t;
  using typename Base_t::Iterator2_t;
  using typename Base_t::Key_traits_t;
  using typename Base_t::Source1_t;
  using typename Base_t::Source2_t;
  using typename Base_t::Value_t;

  Nested_set_union_iterator() = default;

  // Construct from two sources and one iterator into each of them.
  Nested_set_union_iterator(const Source1_t *source1, const Source2_t *source2,
                            const Iterator1_t &iterator1,
                            const Iterator2_t &iterator2)
      : Base_t(source1, source2, iterator1, iterator2) {
    compute_order();
  }

  /// @return The value that this iterator currently points to.
  [[nodiscard]] auto get() const {
    if (m_order < 0) return this->make_value1();
    if (m_order > 0) return this->make_value2();
    assert(m_order == 0);
    return this->make_value();
  }

  /// Move to the next iterator position.
  void next() {
    if (m_order <= 0) ++this->m_iterator1;
    if (m_order >= 0) ++this->m_iterator2;
    compute_order();
  }

  /// @return true if this iterator equals other.
  [[nodiscard]] bool is_equal(const Nested_set_union_iterator &other) const {
    if (m_order <= 0)
      return this->m_iterator1 == other.m_iterator1;
    else
      return this->m_iterator2 == other.m_iterator2;
  }

 private:
  /// Store `less`, `equals`, or `greater` in `m_order`, according to how
  /// `m_iterator1` compares to `m_iterator2`.
  void compute_order() {
    if (this->m_iterator2 == this->m_source2.end()) {
      if (this->m_iterator1 == this->m_source1.end()) {
        m_order = std::strong_ordering::equal;
      } else {
        m_order = std::strong_ordering::less;
      }
    } else {
      if (this->m_iterator1 == this->m_source1.end()) {
        m_order = std::strong_ordering::greater;
      } else {
        m_order = Key_traits_t::cmp(this->m_iterator1->first,
                                    this->m_iterator2->first);
      }
    }
  }

  /// The relative order of the two current iterator positions.
  std::strong_ordering m_order{std::strong_ordering::equal};
};  // class Nested_set_union_iterator

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_UNION_ITERATOR_H

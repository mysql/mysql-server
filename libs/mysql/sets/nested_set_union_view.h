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

#ifndef MYSQL_SETS_NESTED_SET_UNION_VIEW_H
#define MYSQL_SETS_NESTED_SET_UNION_VIEW_H

/// @file
/// Experimental API header

#include "mysql/sets/base_binary_operation_views.h"  // Union_view
#include "mysql/sets/nested_set_binary_operation_view_interface.h"  // Nested_set_binary_operation_view_interface
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// View over the union of two nested sets.
///
/// @tparam Source1_tp The first source.
///
/// @tparam Source2_tp The second source.
template <Is_nested_set Source1_tp, Is_nested_set Source2_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Union_view<Source1_tp, Source2_tp>
    : public detail::Nested_set_binary_operation_view_interface<
          Union_view<Source1_tp, Source2_tp>, Source1_tp, Source2_tp,
          Binary_operation::op_union> {
  using Base_t = detail::Nested_set_binary_operation_view_interface<
      Union_view<Source1_tp, Source2_tp>, Source1_tp, Source2_tp,
      Binary_operation::op_union>;
  using Base_t::m_source1;
  using Base_t::m_source2;
  using Base_t::make_iterator;

 public:
  using typename Base_t::Iterator_t;
  using typename Base_t::Key_t;
  using typename Base_t::Source1_t;
  using typename Base_t::Source2_t;
  using Self_t = Union_view<Source1_t, Source2_t>;

  Union_view() noexcept = default;

  /// Construct a new Union_view over the given sources.
  ///
  /// @param source1 The first source.
  ///
  /// @param source2 The second source.
  Union_view(const Source1_t &source1, const Source2_t &source2) noexcept
      : Base_t(source1, source2) {}

  /// Construct a new Union_view over the given sources.
  ///
  /// Use this constructor if one of the sources may be empty and you do not
  /// have an object representing it; then pass nullptr for that source.
  ///
  /// @param source1 The first source, or nullptr for empty set.
  ///
  /// @param source2 The second source, or nullptr for empty set.
  Union_view(const Source1_t *source1, const Source2_t *source2) noexcept
      : Base_t(source1, source2) {}

  /// @return iterator to the first value pair.
  [[nodiscard]] auto begin() const noexcept {
    return make_iterator(this->m_source1.begin(), this->m_source2.begin());
  }

  /// @return iterator to sentinel value pair.
  [[nodiscard]] auto end() const noexcept {
    return make_iterator(this->m_source1.end(), this->m_source2.end());
  }

  /// Return iterator to the given key, or end() if not found.
  [[nodiscard]] auto find(const Key_t &key) const noexcept {
    Iterator_t cursor{begin()};
    return find(cursor, key);
  }

  /// Return iterator to the given key, or end() if not found, using the given
  /// iterator cursor as a hint.
  ///
  /// @param[in,out] cursor Hint for the position. The behavior is undefined if
  /// this is greater than the lower bound for the key. It will be updated to
  /// the upper bound for the key.
  ///
  /// @param key Key to search for.
  ///
  /// @return Iterator to the key, or end if not found.
  [[nodiscard]] auto find(Iterator_t &cursor, const Key_t &key) const noexcept {
    auto &cursor1 = cursor.iterator1();
    auto &cursor2 = cursor.iterator2();
    auto it1 = this->m_source1.find(cursor1, key);
    auto it2 = this->m_source2.find(cursor2, key);
    if (it1 == this->m_source1.end()) {
      if (it2 == this->m_source2.end()) return end();
      return make_iterator(cursor1, it2);
    }
    if (it2 == this->m_source2.end()) return make_iterator(it1, cursor2);
    return make_iterator(it1, it2);
  }
};  // class Union_view

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_UNION_VIEW_H

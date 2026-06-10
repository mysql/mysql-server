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

#ifndef MYSQL_SETS_NESTED_SET_BINARY_OPERATION_ITERATOR_BASE_H
#define MYSQL_SETS_NESTED_SET_BINARY_OPERATION_ITERATOR_BASE_H

/// @file
/// Experimental API header

#include "mysql/iterators/iterator_interface.h"      // Iterator_interface
#include "mysql/sets/base_binary_operation_views.h"  // Binary_operation_view_type
#include "mysql/sets/binary_operation.h"             // Binary_operation
#include "mysql/sets/nested_set_meta.h"              // Is_nested_set
#include "mysql/sets/optional_view_source_set.h"     // Optional_view_source_set
#include "mysql/sets/set_categories_and_traits.h"    // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Common base class for the forward iterators over union view, intersection
/// view, and subtraction view of two nested sets.
///
/// @tparam Source1_tp First nested set.
///
/// @tparam Source2_tp Second nested set.
///
/// @tparam operation_tp The type of operation.
template <class Self_tp, Is_nested_set Source1_tp, Is_nested_set Source2_tp,
          Binary_operation operation_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Nested_set_binary_operation_iterator_base
    : public mysql::iterators::Iterator_interface<Self_tp> {
 public:
  using Source1_t = Source1_tp;
  using Source2_t = Source2_tp;
  using Opt_source1_t = Optional_view_source_set<Source1_t>;
  using Opt_source2_t = Optional_view_source_set<Source2_t>;
  static constexpr auto operation = operation_tp;

  using Iterator1_t = typename Source1_t::Const_iterator_t;
  using Iterator2_t = typename Source2_t::Const_iterator_t;
  using Key_traits_t = typename Source1_tp::Key_traits_t;
  using Key_t = typename Key_traits_t::Element_t;
  using Mapped1_t = Source1_t::Mapped_t;
  using Mapped2_t = Source2_t::Mapped_t;
  using Mapped_t = Binary_operation_view_type<operation, Mapped1_t, Mapped2_t>;
  using Value_t = std::pair<const Key_t, Mapped_t>;

  // Default constructor. The resulting iterator is in a "pointless" state,
  // where the only possible operations are assignment from another iterator,
  // and comparison.
  Nested_set_binary_operation_iterator_base() = default;

  // Construct from two sources and one iterator into each of them.
  Nested_set_binary_operation_iterator_base(const Source1_t *source1,
                                            const Source2_t *source2,
                                            const Iterator1_t &iterator1,
                                            const Iterator2_t &iterator2)
      : m_source1(source1),
        m_source2(source2),
        m_iterator1(iterator1),
        m_iterator2(iterator2) {}

  // Default rule-of-5
  Nested_set_binary_operation_iterator_base(
      const Nested_set_binary_operation_iterator_base &other) noexcept =
      default;
  Nested_set_binary_operation_iterator_base(
      Nested_set_binary_operation_iterator_base &&other) noexcept = default;
  Nested_set_binary_operation_iterator_base &operator=(
      const Nested_set_binary_operation_iterator_base &other) noexcept =
      default;
  Nested_set_binary_operation_iterator_base &operator=(
      Nested_set_binary_operation_iterator_base &&other) noexcept = default;
  ~Nested_set_binary_operation_iterator_base() noexcept = default;

  /// Return const reference to the current iterator into the first set.
  [[nodiscard]] const auto &iterator1() const { return m_iterator1; }

  /// Return reference to the current iterator into the first set.
  [[nodiscard]] auto &iterator1() { return m_iterator1; }

  /// Return const reference to the current iterator into the second set.
  [[nodiscard]] const auto &iterator2() const { return m_iterator2; }

  /// Return reference to the current iterator into the second set.
  [[nodiscard]] auto &iterator2() { return m_iterator2; }

 protected:
  /// Return the current value, computed as operation(m_iterator1, m_iterator2).
  [[nodiscard]] Value_t make_value() const {
    return Value_t(m_iterator1->first,
                   Mapped_t(m_iterator1->second, m_iterator2->second));
  }

  /// Return the current value, computed as operation(m_iterator1, nullptr).
  [[nodiscard]] Value_t make_value1() const {
    return Value_t(m_iterator1->first, Mapped_t(&m_iterator1->second, nullptr));
  }

  /// Return the current value, computed as operation(nullptr, m_iterator2).
  [[nodiscard]] Value_t make_value2() const {
    return Value_t(m_iterator2->first, Mapped_t(nullptr, &m_iterator2->second));
  }

  /// The first source.
  Opt_source1_t m_source1{};

  /// The second source.
  Opt_source2_t m_source2{};

  /// Iterator into the first source.
  ///
  /// It is `mutable` because derived classes may be update it lazily.
  mutable Iterator1_t m_iterator1;

  /// Iterator into the second source.
  ///
  /// It is `mutable` because derived classes may be update it lazily.
  mutable Iterator2_t m_iterator2;
};  // class Nested_set_binary_operation_iterator_base

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_BINARY_OPERATION_ITERATOR_BASE_H

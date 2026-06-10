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

#ifndef MYSQL_SETS_NESTED_SET_BINARY_OPERATION_VIEW_INTERFACE_H
#define MYSQL_SETS_NESTED_SET_BINARY_OPERATION_VIEW_INTERFACE_H

/// @file
/// Experimental API header

#include <cassert>                            // assert
#include "mysql/sets/binary_operation.h"      // Binary_operation
#include "mysql/sets/nested_set_category.h"   // Nested_set_category_tag
#include "mysql/sets/nested_set_interface.h"  // Nested_view_interface
#include "mysql/sets/nested_set_intersection_iterator.h"  // Nested_set_intersection_iterator
#include "mysql/sets/nested_set_meta.h"                   // Is_nested_set
#include "mysql/sets/nested_set_subtraction_iterator.h"  // Nested_set_subtraction_iterator
#include "mysql/sets/nested_set_union_iterator.h"  // Nested_set_union_iterator
#include "mysql/sets/optional_view_source_set.h"   // Optional_view_source_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Type alias that provides the iterator type for a given Binary_operation.
///
/// @tparam Source1_t The first source.
///
/// @tparam Source2_t The second source.
///
/// @tparam operation_t The binary operation.
//
// The casts to int are workarounds for what appears to be a bug in MSVC 19.29
// (VS16.11). It gives the error "error C2677: binary '==': no global operator
// found which takes type 'mysql::sets::Binary_operation' (or there is no
// acceptable conversion)".
//
// @todo Remove the casts when we drop support for the buggy compiler version.
template <Is_nested_set Source1_t, Is_nested_set Source2_t,
          Binary_operation operation_t>
using Nested_binary_operation_iterator_type = std::conditional_t<
    (int)operation_t == (int)Binary_operation::op_union,
    Nested_set_union_iterator<Source1_t, Source2_t>,
    std::conditional_t<(int)operation_t ==
                           (int)Binary_operation::op_intersection,
                       Nested_set_intersection_iterator<Source1_t, Source2_t>,
                       Nested_set_subtraction_iterator<Source1_t, Source2_t>>>;

/// Common base class for all the view classes.
///
/// @tparam Source1_tp The first source.
///
/// @tparam Source2_tp The second source.
///
/// @tparam operation_tp The binary operation.
template <class Self_tp, Is_nested_set Source1_tp, Is_nested_set Source2_tp,
          Binary_operation operation_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Nested_set_binary_operation_view_interface
    : public Nested_view_interface<Self_tp,
                                   Nested_binary_operation_iterator_type<
                                       Source1_tp, Source2_tp, operation_tp>,
                                   Nested_binary_operation_iterator_type<
                                       Source1_tp, Source2_tp, operation_tp>,
                                   typename Source1_tp::Set_traits_t> {
  using Self_t = Self_tp;

 public:
  using Source1_t = Source1_tp;
  using Source2_t = Source2_tp;
  using Opt_source1_t = Optional_view_source_set<Source1_tp>;
  using Opt_source2_t = Optional_view_source_set<Source2_tp>;
  static constexpr auto operation = operation_tp;
  using Iterator1_t = mysql::ranges::Range_const_iterator_type<Source1_t>;
  using Iterator2_t = mysql::ranges::Range_const_iterator_type<Source2_t>;
  using Set_traits_t = typename Source1_t::Set_traits_t;
  using Set_category_t = mysql::sets::Nested_set_category_tag;
  using Key_traits_t = typename Set_traits_t::Key_traits_t;
  using Mapped_traits_t = typename Set_traits_t::Mapped_traits_t;
  using Mapped_category_t = typename Set_traits_t::Mapped_category_t;
  using Key_t = typename Key_traits_t::Element_t;
  using Iterator_t =
      Nested_binary_operation_iterator_type<Source1_t, Source2_t, operation>;
  using Mapped_t = typename Iterator_t::Mapped_t;
  using Value_t = typename Iterator_t::Value_t;
  static constexpr bool disable_fast_size = true;

  Nested_set_binary_operation_view_interface() = default;

  /// Construct a new Nested_set_binary_operation_view_interface over the given
  /// sources.
  ///
  /// @param source1 The first source.
  ///
  /// @param source2 The second source.
  Nested_set_binary_operation_view_interface(const Source1_t &source1,
                                             const Source2_t &source2) noexcept
      : m_source1(source1), m_source2(source2) {}

  /// Construct a new Nested_set_binary_operation_view_interface over the given
  /// sources.
  ///
  /// Use this constructor if one of the sources may be empty and you do not
  /// have an object representing it; then pass nullptr for that source.
  ///
  /// @param source1 The first source, or nullptr for empty set.
  ///
  /// @param source2 The second source, or nullptr for empty set.
  Nested_set_binary_operation_view_interface(const Source1_t *source1,
                                             const Source2_t *source2) noexcept
      : m_source1(source1), m_source2(source2) {}

  // Default rule-of-5.
  Nested_set_binary_operation_view_interface(
      const Nested_set_binary_operation_view_interface &) noexcept = default;
  Nested_set_binary_operation_view_interface(
      Nested_set_binary_operation_view_interface &&) noexcept = default;
  Nested_set_binary_operation_view_interface &operator=(
      const Nested_set_binary_operation_view_interface &) noexcept = default;
  Nested_set_binary_operation_view_interface &operator=(
      Nested_set_binary_operation_view_interface &&) noexcept = default;
  ~Nested_set_binary_operation_view_interface() noexcept = default;

  [[nodiscard]] auto operator[](const Key_t &key) const noexcept {
    auto it = self().find(key);
    assert(it != self().end());
    return it->second;
  }

  [[nodiscard]] auto operator[](const Key_t &key) noexcept {
    auto it = self().find(key);
    assert(it != self().end());
    return it->second;
  }

 protected:
  // Return a new iterator based on the two given source iterators.
  [[nodiscard]] Iterator_t make_iterator(const Iterator1_t &iterator1,
                                         const Iterator2_t &iterator2) const {
    return Iterator_t(m_source1.pointer(), m_source2.pointer(), iterator1,
                      iterator2);
  }

  /// Pointer to the first nested set operand.
  Opt_source1_t m_source1{};

  /// Pointer to the second nested set operand.
  Opt_source2_t m_source2{};

 private:
  [[nodiscard]] const auto &self() const {
    return static_cast<const Self_t &>(*this);
  }
  [[nodiscard]] auto &self() { return static_cast<Self_t &>(*this); }
};  // class Nested_set_binary_operation_view_interface

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_BINARY_OPERATION_VIEW_INTERFACE_H

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

#ifndef MYSQL_SETS_NESTED_SET_INTERFACE_H
#define MYSQL_SETS_NESTED_SET_INTERFACE_H

/// @file
/// Experimental API header

#include <iterator>                                    // forward_iterator
#include <utility>                                     // pair
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/meta/not_decayed.h"                    // Not_decayed
#include "mysql/ranges/collection_interface.h"         // Collection_interface
#include "mysql/ranges/meta.h"                         // Range_iterator_type
#include "mysql/sets/basic_set_container_wrapper.h"  // Basic_set_container_wrapper
#include "mysql/sets/nested_set_category.h"          // Nested_set_category_tag
#include "mysql/sets/nested_set_meta.h"              // Is_nested_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// CRTP base class/mixin used to define a Nested set based on an implementation
/// that provides the `find` and `upper_bound` functions.
///
/// This provides the member types and `operator[]`.
///
/// @tparam Self_tp Implementation subclass.
///
/// @tparam Iterator_tp Iterator type for the subclass.
///
/// @tparam Const_iterator_tp Const iterator type for the subclass.
///
/// @tparam Set_traits_tp Set traits.
template <class Self_tp, std::forward_iterator Iterator_tp,
          std::forward_iterator Const_iterator_tp,
          Is_nested_set_traits Set_traits_tp>
class Nested_set_interface {
  using Self_t = Self_tp;

 public:
  using Iterator_t = Iterator_tp;
  using Const_iterator_t = Const_iterator_tp;
  using Set_category_t = Nested_set_category_tag;
  using Iterator_value_t = mysql::ranges::Iterator_value_type<Iterator_t>;

  using Set_traits_t = Set_traits_tp;
  using Key_traits_t = typename Set_traits_t::Key_traits_t;
  using Key_t = typename Key_traits_t::Element_t;
  using Mapped_category_t = typename Set_traits_t::Mapped_category_t;
  using Mapped_traits_t = typename Set_traits_t::Mapped_traits_t;
  using Mapped_t = typename Iterator_value_t::second_type;

  static_assert(
      std::same_as<Iterator_value_t, std::pair<const Key_t, Mapped_t>>);

  /// Return non-const reference to the mapped Set for the given key.
  ///
  /// If the key is not in the set, the behavior is undefined.
  ///
  /// @param key Key to look for.
  [[nodiscard]] auto &operator[](const Key_t &key) noexcept {
    auto it = self().find(key);
    assert(it != self().end());
    return it->second;
  }

  /// Return const reference to the mapped Set for the given key.
  ///
  /// If the key is not in the set, the behavior is undefined.
  ///
  /// @param key Key to look for.
  [[nodiscard]] const auto &operator[](const Key_t &key) const noexcept {
    auto it = self().find(key);
    assert(it != self().end());
    return it->second;
  }

 private:
  [[nodiscard]] auto &self() { return static_cast<Self_t &>(*this); }
  [[nodiscard]] const auto &self() const {
    return static_cast<const Self_t &>(*this);
  }
};  // class Nested_set_interface

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// CRTP base class/mixin used to implement Nested sets that are *views*. This
/// defines the `operator[]` members based on `find` members in the subclass,
/// and also implements all the view members defined by
/// `mysql::ranges::Collection_interface`.
///
/// @tparam Self_tp Class deriving from this class.
///
/// @tparam Iterator_tp Iterator type.
///
/// @tparam Const_iterator_tp Const iterator type.
///
/// @tparam Set_traits_tp Nested set traits.
template <class Self_tp, std::input_iterator Iterator_tp,
          std::input_iterator Const_iterator_tp,
          Is_nested_set_traits Set_traits_tp>
class Nested_view_interface
    : public mysql::ranges::Collection_interface<Self_tp>,
      public detail::Nested_set_interface<Self_tp, Iterator_tp,
                                          Const_iterator_tp, Set_traits_tp>,
      public std::ranges::view_base {
  using Nested_set_base_t =
      detail::Nested_set_interface<Self_tp, Iterator_tp, Const_iterator_tp,
                                   Set_traits_tp>;

 public:
  // Prefer the subscript operator for nested sets over the one from
  // Collection_interface.
  using Nested_set_base_t::operator[];
};

/// CRTP base class/mixin, used to implement Nested Sets that are *container
/// wrappers*. This defines the `operator[]` members based on `find` in the
/// subclass, and also implements all the container members defined by
/// `mysql::containers::Basic_container_wrapper`.
///
/// @tparam Self_tp Class deriving from this class.
///
/// @tparam Wrapped_tp Type of the wrapped boundary set class.
template <class Self_tp, class Wrapped_tp>
class Basic_nested_container_wrapper
    : public Basic_set_container_wrapper<Self_tp, Wrapped_tp>,
      public detail::Nested_set_interface<
          Self_tp, mysql::ranges::Range_iterator_type<Wrapped_tp>,
          mysql::ranges::Range_const_iterator_type<Wrapped_tp>,
          typename Wrapped_tp::Set_traits_t> {
  using Wrapper_base_t = Basic_set_container_wrapper<Self_tp, Wrapped_tp>;
  using Nested_set_base_t = detail::Nested_set_interface<
      Self_tp, mysql::ranges::Range_iterator_type<Wrapped_tp>,
      mysql::ranges::Range_const_iterator_type<Wrapped_tp>,
      typename Wrapped_tp::Set_traits_t>;
  using This_t = Basic_nested_container_wrapper<Self_tp, Wrapped_tp>;

 public:
  // Prefer the subscript operator for nested sets over the one from
  // Collection_interface.
  using Nested_set_base_t::operator[];

  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Basic_nested_container_wrapper(Args_t &&...args) noexcept(
      noexcept(Wrapper_base_t(std::forward<Args_t>(args)...)))
      : Wrapper_base_t(std::forward<Args_t>(args)...) {}
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_INTERFACE_H

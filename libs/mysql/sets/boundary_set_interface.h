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

#ifndef MYSQL_SETS_BOUNDARY_SET_INTERFACE_H
#define MYSQL_SETS_BOUNDARY_SET_INTERFACE_H

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSets
/// @{

#include <utility>                                     // forward
#include "mysql/containers/basic_container_wrapper.h"  // Basic_container_wrapper
#include "mysql/meta/not_decayed.h"                    // Not_decayed
#include "mysql/ranges/collection_interface.h"         // Collection_interface
#include "mysql/ranges/meta.h"                         // Range_iterator_type
#include "mysql/sets/basic_set_container_wrapper.h"  // Basic_set_container_wrapper
#include "mysql/sets/boundary_set_category.h"  // Boundary_set_category_tag
#include "mysql/sets/boundary_set_meta.h"      // Is_boundary_iterator
#include "mysql/sets/set_traits.h"             // Is_bounded_set_traits
#include "mysql/sets/upper_lower_bound_interface.h"  // Upper_lower_bound_interface
#include "mysql/utils/call_and_catch.h"              // Shall_catch

namespace mysql::sets::detail {

/// CRTP base class used to implement Boundary Sets. This defines all the
/// lower_bound/upper_bound members based on the
/// lower_bound_impl/upper_bound_impl members in the subclass.
///
/// The subclass additionally has to define the view members, e.g. by deriving
/// from mysql::ranges::Collection_interface or
/// mysql::containers::Basic_container_wrapper. Doing so will make it satisfy
/// the mysql::sets::Is_boundary_set concept. As a convenience, use
/// Boundary_view_interface to inherit both from this class and from
/// Collection_interface, and use Basic_boundary_container_wrapper to inherit
/// both this class and from Basic_container_wrapper.
///
/// @tparam Self_tp The subclass. This must satisfy
/// Is_boundary_set_implementation.
///
/// @tparam Iterator_tp Iterator type.
///
/// @tparam Const_iterator_tp Const iterator type.
///
/// @tparam Set_traits_tp Set Traits for the set.
template <class Self_tp, Is_boundary_iterator Iterator_tp,
          Is_boundary_iterator Const_iterator_tp,
          Is_bounded_set_traits Set_traits_tp>
class Boundary_set_interface
    : public Upper_lower_bound_interface<Self_tp, Set_traits_tp, Iterator_tp,
                                         Const_iterator_tp,
                                         Iterator_get_value> {
 public:
  using Iterator_t = Iterator_tp;
  using Const_iterator_t = Const_iterator_tp;
  using Set_category_t = Boundary_set_category_tag;
  using Set_traits_t = Set_traits_tp;
  using Element_t = typename Set_traits_tp::Element_t;
};  // class Boundary_set_interface

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// CRTP base class/mixin, used to implement Boundary Sets that are *views*.
/// This defines all the lower_bound and upper_bound members based on
/// lower_bound_impl and upper_bound_impl members in the subclass, and also
/// implements all the view members defined by
/// mysql::ranges::Collection_interface.
///
/// @tparam Self_tp Class deriving from this class.
///
/// @tparam Iterator_tp Iterator type.
///
/// @tparam Const_iterator_tp Const iterator type.
///
/// @tparam Set_traits_tp Bounded set traits.
template <class Self_tp, Is_boundary_iterator Iterator_tp,
          Is_boundary_iterator Const_iterator_tp,
          Is_bounded_set_traits Set_traits_tp>
class Boundary_view_interface
    : public mysql::ranges::Collection_interface<Self_tp>,
      public detail::Boundary_set_interface<Self_tp, Iterator_tp,
                                            Const_iterator_tp, Set_traits_tp>,
      public std::ranges::view_base {};

/// CRTP base class/mixin, used to implement Boundary Sets that are *container
/// wrappers*. This defines all the lower_bound and upper_bound members based on
/// lower_bound_impl and upper_bound_impl members in the subclass, and also
/// implements all the container members defined by
/// mysql::containers::Basic_container_wrapper.
///
/// @tparam Self_tp Class deriving from this class.
///
/// @tparam Wrapped_tp Type of the wrapped boundary set class.
///
/// @tparam shall_catch_tp Whether `assign` should catch bad_alloc
/// exceptions and convert them to `Return_status` return values.
template <class Self_tp, class Wrapped_tp,
          mysql::utils::Shall_catch shall_catch_tp =
              mysql::utils::Shall_catch::no>
class Basic_boundary_container_wrapper
    : public Basic_set_container_wrapper<Self_tp, Wrapped_tp, shall_catch_tp>,
      public detail::Boundary_set_interface<
          Self_tp, mysql::ranges::Range_iterator_type<Wrapped_tp>,
          mysql::ranges::Range_const_iterator_type<Wrapped_tp>,
          typename Wrapped_tp::Set_traits_t> {
  using Basic_set_container_wrapper_t =
      Basic_set_container_wrapper<Self_tp, Wrapped_tp, shall_catch_tp>;
  using This_t =
      Basic_boundary_container_wrapper<Self_tp, Wrapped_tp, shall_catch_tp>;

 public:
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Basic_boundary_container_wrapper(Args_t &&...args)
      : Basic_set_container_wrapper_t(std::forward<Args_t>(args)...) {}

  // default rule-of-5
  Basic_boundary_container_wrapper(
      const Basic_boundary_container_wrapper &source) = default;
  Basic_boundary_container_wrapper(
      Basic_boundary_container_wrapper &&source) noexcept = default;
  Basic_boundary_container_wrapper &operator=(
      const Basic_boundary_container_wrapper &source) = default;
  Basic_boundary_container_wrapper &operator=(
      Basic_boundary_container_wrapper &&source) noexcept = default;
  ~Basic_boundary_container_wrapper() = default;
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_INTERFACE_H

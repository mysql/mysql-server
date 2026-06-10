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

#ifndef MYSQL_SETS_INTERVAL_SET_META_H
#define MYSQL_SETS_INTERVAL_SET_META_H

/// @file
/// Experimental API header
///
/// - Is_interval_set<T>: true if T is an interval set:
///   essentially has the members defined by std::view_interface, with value
///   type Interval, and has the member function boundaries() which returns the
///   underlying boundary set.
///
/// - Is_interval_container<T>: true if T is an interval container:
///   this is an interval set with additional members assign, clear, insert,
///   remove, inplace_union, inplace_subtract, inplace_intersect

#include "mysql/sets/boundary_set_meta.h"          // Is_boundary_container_ref
#include "mysql/sets/interval_set_category.h"      // Interval_set_category_tag
#include "mysql/sets/meta.h"                       // Enable_donate_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_set
#include "mysql/sets/set_traits.h"                 // Has_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{
///
/// This file defines the category tag class and the type traits (concepts)
/// related to boundary sets.
///
/// The main traits are the following:
///
/// - Is_interval_set<T>: true if T is an interval set: essentially has the
///   members defined by std::view_interface, with Interval as the iterator
///   value type, and a `boundaries()` member function that returns the
///   underlying boundary set.
///
/// - Is_interval_container<T>: true if T is an interval container: this is an
///   interval set with additional members clear, assign, insert, remove,
///   inplace_union, inplace_subtract, inplace_intersection, requiring that
///   boundaries() returns a boundary container.

namespace mysql::sets {

// ==== Is_interval_set ====

// Forward declaration.
template <Is_bounded_set_traits Set_traits_tp>
class Interval;

// True if Test is an interval set, i.e., provides a view over Interval
// objects and has the member function boundaries() which returns the underlying
// boundary set.
template <class Test>
concept Is_interval_set =
    Is_set<Test> && Has_set_category<Test, Interval_set_category_tag> &&
    Has_set_traits<Test> &&
    std::same_as<typename Test::Element_t,
                 typename Test::Set_traits_t::Element_t> &&
    mysql::ranges::Is_collection_over<Test,
                                      Interval<typename Test::Set_traits_t>> &&
    requires(Test t) {
      {
        t.boundaries()
        } -> Is_boundary_set_ref_over_traits<typename Test::Set_traits_t>;
    };

// True if Test is an interval set over the given Set traits.
template <class Test, class Set_traits_t>
concept Is_interval_set_over_traits =
    Is_interval_set<Test> && Is_bounded_set_traits<Set_traits_t> &&
    Has_set_traits<Test, Set_traits_t>;

// True if std::remove_cvref_t<Test> is an interval set over the given Set
// traits.
template <class Test, class Set_traits_t>
concept Is_interval_set_over_traits_unqualified =
    Is_interval_set_over_traits<std::remove_cvref_t<Test>, Set_traits_t>;

// ==== Is_interval_container ====

/// True if `Test` is an Interval_container.
template <class Test>
concept Is_interval_container =
    Is_interval_set<Test> &&
    detail::Is_boundary_or_interval_container_helper<
        Test, typename Test::Set_traits_t::Element_t,
        Interval<typename Test::Set_traits_t>> &&
    requires(Test t) {
      { t.boundaries() } -> Is_boundary_container_ref;
      {
        t.boundaries()
        } -> Is_boundary_set_ref_over_traits<typename Test::Set_traits_t>;
    };

/// The type of the Boundary Set for a given Interval Set.
template <Is_interval_container Interval_container_t>
using Interval_set_boundary_set_type = std::remove_cvref_t<
    decltype(std::declval<Interval_container_t>().boundaries())>;

// ==== Enable_donate_set[_elements] ====

namespace detail {

/// Helper concept to define the condition when Enable_donate_set shall be
/// defined for Interval Container types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_for_interval_container =
    Is_interval_container<Source_t> && Is_interval_container<Target_t> &&
    Is_compatible_set<Source_t, Target_t> &&
    Can_donate_set_unqualified<Interval_set_boundary_set_type<Source_t>,
                               Interval_set_boundary_set_type<Target_t>>;

/// Helper concept to define the condition when Enable_donate_set_elements shall
/// be defined for Interval Container types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_elements_for_interval_container =
    Is_interval_container<Source_t> && Is_interval_container<Target_t> &&
    Can_donate_set_elements_unqualified<
        Interval_set_boundary_set_type<Source_t>,
        Interval_set_boundary_set_type<Target_t>>;

}  // namespace detail

/// Declare that move-semantics is supported for full-set-copy operations on
/// compatible Interval Set container types, whenever the boundary types support
/// it.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_for_interval_container<Source_t,
                                                                  Target_t>
struct Enable_donate_set<Source_t, Target_t> : public std::true_type {};

/// Declare that move-semantics is supported for element operations on
/// compatible Interval Set container types, whenever the boundary type supports
/// it, and full-set-copy is enabled.
template <Is_interval_container Source_t, Is_interval_container Target_t>
  requires detail::Shall_enable_donate_set_elements_for_interval_container<
      Source_t, Target_t>
struct Enable_donate_set_elements<Source_t, Target_t> : public std::true_type {
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_META_H

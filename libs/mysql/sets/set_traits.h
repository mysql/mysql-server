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

#ifndef MYSQL_SETS_SET_TRAITS_H
#define MYSQL_SETS_SET_TRAITS_H

/// @file
/// Experimental API header

#include <concepts>                       // derived_from
#include "mysql/meta/optional_is_same.h"  // Optional_is_same

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== Set traits ====

/// Base class for all Set_traits classes.
///
/// Set traits are used to determine if sets (of the same category) are
/// compatible. For example, if two set classes have the same category and
/// traits, it is allowed to compute the union of them (using the algorithm for
/// "union" determined by their category tag).
struct Base_set_traits {};

/// True if Test is a Set_traits class, i.e., is derived from Base_set_traits.
template <class Test>
concept Is_set_traits = std::derived_from<Test, Base_set_traits>;

// ==== Has_*_traits ====

/// True if Test has a member Set_traits_t.
///
/// If Set_traits_t is given, true only if Test::Set_traits_t is the same type
/// as Set_traits_t.
template <class Test, class Set_traits_t = void>
concept Has_set_traits =
    Is_set_traits<typename Test::Set_traits_t> &&
    mysql::meta::Optional_is_same<typename Test::Set_traits_t, Set_traits_t>;

/// True if Test1 and Test2 have the same Set_traits_t.
template <class Test1, class Test2>
concept Has_same_set_traits =
    Has_set_traits<Test1> && Has_set_traits<Test2> &&
    std::same_as<typename Test1::Set_traits_t, typename Test2::Set_traits_t>;

// ==== Specific kinds of set traits ====

/// True if Test is a Set traits class with an element type member, Element_t.
///
/// This requires that the member type Element_t exists and satisfies
/// std::equality_comparable. If the template argument Element_t is given, it
/// also requires that Test::Element_t equals Element_t.
template <class Test, class Element_t = void>
concept Is_element_set_traits =
    Is_set_traits<Test> && std::equality_comparable<typename Test::Element_t> &&
    mysql::meta::Optional_is_same<typename Test::Element_t, Element_t>;

/// True if Test is an "ordered" Set traits class.
///
/// This requires that Test has an element type, and the static member functions
/// lt, le, gt, and ge to compare two values, and the member type Less_t which
/// is a function object class whose objects satisfy the *Compare* named
/// requirements (cf. https://en.cppreference.com/w/cpp/named_req/Compare).
template <class Test>
concept Is_ordered_set_traits =
    Is_element_set_traits<Test> && requires { typename Test::Less_t; } &&
    requires(typename Test::Element_t v1, typename Test::Element_t v2) {
      { Test::lt(v1, v2) } -> std::same_as<bool>;
      { Test::le(v1, v2) } -> std::same_as<bool>;
      { Test::gt(v1, v2) } -> std::same_as<bool>;
      { Test::ge(v1, v2) } -> std::same_as<bool>;
      { typename Test::Less_t()(v1, v2) } -> std::same_as<bool>;
    };

/// True if Test is a "bounded" Set traits class.
///
/// This requires that it is ordered, and has static member functions `min` and
/// `max_exclusive` that give the minimum and maximum value, respectively, as
/// well as the `in_range` convenience function that returns true if a value is
/// in the range.
template <class Test>
concept Is_bounded_set_traits =  // this comment helps clang-format
    Is_ordered_set_traits<Test> &&
    requires(typename Test::Element_t value) {
      { Test::min() } -> std::same_as<typename Test::Element_t>;
      { Test::max_exclusive() } -> std::same_as<typename Test::Element_t>;
      { Test::in_range(value) } -> std::same_as<bool>;
    };

/// True if Test is a "discrete" Set traits class, i.e., it bounded, and it is
/// possible to compute successors and predecessors.
///
/// This requires that each value has a successor and a predecessor given by the
/// static member functions prev and next.
template <class Test>
concept Is_discrete_set_traits =
    Is_bounded_set_traits<Test> &&
    requires(typename Test::Element_t v1, typename Test::Element_t v2) {
      { Test::next(v1) } -> std::same_as<typename Test::Element_t>;
      { Test::prev(v1) } -> std::same_as<typename Test::Element_t>;
    };

/// True if Test is a "metric" Set traits class, i.e., it is bounded, and it is
/// possible to compute differences between boundaries.
///
/// This requires that it has a member type Difference_t (which may or may not
/// be equal to Element_t), that the difference between two Element_t values is
/// of type Difference_t and can be computed using member function sub, that two
/// Difference_t objects can be added using `add` to give another Difference_t,
/// and that a Difference_t can be added to a Element_t using `add` to give
/// another Element_t.
template <class Test>
concept Is_metric_set_traits =
    Is_bounded_set_traits<Test> &&
    std::same_as<typename Test::Difference_t, typename Test::Difference_t> &&
    (!std::same_as<typename Test::Difference_t, void>) &&
    requires(Test t, typename Test::Element_t v1, typename Test::Element_t v2,
             typename Test::Difference_t d1, typename Test::Difference_t d2) {
      { Test::sub(v1, v2) } -> std::same_as<typename Test::Difference_t>;
      { Test::add(v1, d1) } -> std::same_as<typename Test::Element_t>;
      { Test::add(d1, v1) } -> std::same_as<typename Test::Element_t>;
      { Test::add(d1, d2) } -> std::same_as<typename Test::Difference_t>;
    };

/// True if Test satisfies both Is_discrete_set_traits and
/// Is_metric_set_traits.
template <class Test>
concept Is_discrete_metric_set_traits =
    Is_discrete_set_traits<Test> && Is_metric_set_traits<Test>;

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_SET_TRAITS_H

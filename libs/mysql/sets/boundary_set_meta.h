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

#ifndef MYSQL_SETS_BOUNDARY_SET_META_H
#define MYSQL_SETS_BOUNDARY_SET_META_H

/// @file
/// Experimental API header
///
/// This file defines the category tag class and the type traits (concepts)
/// related to boundary sets.
///
/// The main traits are the following:
///
/// - Is_boundary_iterator<T>: true if T is a boundary iterator (essentially, an
///   iterator with an is_endpoint member).
///
/// - Is_boundary_set<T>: true if T is a boundary set: essentially
///   has the members defined by std::view_interface, and in addition the
///   special upper_bound/lower_bound members, and the iterators satisfy
///   Is_boundary_iterator.
///
/// - Is_boundary_storage<T>: true if T is a boundary storage, i.e. satisfies
///   the requirements for the backing storage of a boundary container. This
///   essentially needs insert, delete, update_point, upper_bound, and
///   lower_bound members.
///
/// - Is_boundary_container<T>: true if T is a boundary container: this is a
///   boundary set with additional members assign, clear, insert, remove,
///   inplace_union, inplace_subtract, inplace_intersect.
///
/// In addition, many of these have variants which require particular Set traits
/// or element types.

#include <iterator>                                // forward_iterator
#include "mysql/meta/is_same_as_all.h"             // Is_same_as_all
#include "mysql/ranges/meta.h"                     // Is_collection_over
#include "mysql/sets/boundary_set_category.h"      // Boundary_set_category_tag
#include "mysql/sets/meta.h"                       // Enable_donate_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_set
#include "mysql/sets/set_traits.h"                 // Has_set_traits
#include "mysql/sets/upper_lower_bound_interface.h"  // Is_upper_lower_bound_implementation

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== Is_boundary_iterator ====

/// True if Test is a *boundary point iterator*, i.e.:
///
/// - Test is a forward iterator (or stronger)
/// - Test has a member `is_endpoint()` that returns bool
///
/// In addition, the following semantic requirements must hold:
/// - `is_endpoint()` returns true for every second element and false for every
///   second element. It is allowed to call is_endpoint() even for the end
///   iterator, and then it must return false.
/// - values are strictly increasing, i.e., if j == std::next(i), and both *i
///   and *j are defined, then *j > *i.
template <class Test>
concept Is_boundary_iterator =
    std::forward_iterator<Test> &&  // this comment helps clang-format
    requires(const Test iterator) {
      { iterator.is_endpoint() } -> std::convertible_to<bool>;
      // Semantic requirements holding for all `iterator` where *iterator and
      // *std::next(iterator) are defined:
      //
      // iterator.is_endpoint() != std::next(iterator).is_endpoint();
      //
      // *iterator < *std::next(iterator);
    };

/// True if Test is a boundary point iterator over values of type Value_t.
///
/// @see Is_boundary_iterator.
template <class Test, class Value_t>
concept Is_boundary_iterator_over_type =
    Is_boundary_iterator<Test> &&
    std::same_as<mysql::ranges::Iterator_value_type<Test>, Value_t>;

/// True if Test is a boundary point iterator and a bidirectional iterator.
template <class Test>
concept Is_bidirectional_boundary_iterator =
    Is_boundary_iterator<Test> && std::bidirectional_iterator<Test>;

/// True if Test is a boundary point iterator and a random access iterator.
template <class Test>
concept Is_random_access_boundary_iterator =
    Is_boundary_iterator<Test> && std::random_access_iterator<Test>;

/// True if Test is a boundary point iterator and a contiguous iterator.
template <class Test>
concept Is_contiguous_boundary_iterator =
    Is_boundary_iterator<Test> && std::contiguous_iterator<Test>;

}  // namespace mysql::sets

// ==== Is_boundary_set ====

namespace mysql::sets::detail {

template <class Test, class Iterator_t, class Const_iterator_t, class Element_t>
concept Is_collection_with_upper_lower_bound_helper =
    mysql::ranges::Is_collection_over<Test, Element_t> &&
    requires(const Test &ct, Test &t, const Iterator_t &i,
             const Const_iterator_t &ci, const Element_t &e) {
      { t.upper_bound(e) } -> std::same_as<Iterator_t>;
      { t.upper_bound(i, e) } -> std::same_as<Iterator_t>;
      { t.lower_bound(e) } -> std::same_as<Iterator_t>;
      { t.lower_bound(i, e) } -> std::same_as<Iterator_t>;
      { ct.upper_bound(e) } -> std::same_as<Const_iterator_t>;
      { ct.upper_bound(ci, e) } -> std::same_as<Const_iterator_t>;
      { ct.lower_bound(e) } -> std::same_as<Const_iterator_t>;
      { ct.lower_bound(ci, e) } -> std::same_as<Const_iterator_t>;
      // Semantic requirements which must hold for all t, e, i and i', where i
      // is a lower bound for e, i.e.,
      //   i==t.begin() || (i==std::next(i') && *i' < e):
      //
      // t.upper_bound(e) == t.upper_bound(i, e);
      // t.upper_bound(e) == t.end() || *t.upper_bound(e) > e;
      // t.upper_bound(e) == t.begin() ||
      //   *std::advance(t.begin(),
      //                 std::distance(t.begin(), t.upper_bound(e)) - 1) <=
      //   *t.upper_bound(e);
      //
      // t.lower_bound(e) == t.lower_bound(i, e);
      // t.lower_bound(e) == t.end() || *t.lower_bound(e) >= e;
      // t.lower_bound(e) == t.begin() ||
      //   *std::advance(t.begin(),
      //                 std::distance(t.begin(), t.lower_bound(e)) - 1) <
      //   *t.lower_bound(e);
    };

/// True if `Test` is an interval set with `Element_t` as its element type,
/// assuming that `Iterator_t` is its iterator type and Const_iterator_t its
/// const iterator type.
template <class Test, class Iterator_t, class Const_iterator_t, class Element_t>
concept Is_boundary_set_helper =
    Is_set<Test> && Has_set_category<Test, Boundary_set_category_tag> &&
    Is_bounded_set_traits<typename Test::Set_traits_t> &&
    mysql::meta::Is_same_as_all<typename Test::Element_t,
                                typename Test::Set_traits_t::Element_t,
                                Element_t> &&
    Is_boundary_iterator_over_type<Iterator_t, Element_t> &&
    Is_collection_with_upper_lower_bound_helper<Test, Iterator_t,
                                                Const_iterator_t, Element_t>;
// Semantic requirements which must hold for all t and i:
//
// t.size() % 2 == 0;
// t.size() == std::distance(t.begin(), t.end());
// t.begin().is_endpoint() == false;
// i == t.end() || std::next(i).is_endpoint() != i.is_endpoint();

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// True if `Test` is an *interval set*, i.e., provides a view
/// over intervals sorted by their endpoints, where each interval is nonempty,
/// disjoint from other intervals, and even does not share endpoint with
/// adjacent intervals. It also provides ways to compute upper bounds of values,
/// i.e., minimal boundary points in the set which are strictly greater than a
/// given value, respectively greater than or equal to a given value.
///
/// This requires, given `Test set`, `Test::iterator iterator`, and
/// `Test::value_type value`:
/// - Test satisfies mysql::containers::Is_set.
/// - Test::iterator satisfies Is_boundary_iterator_over<Test::value_type>.
/// - set.upper_bound(value), set.lower_bound(value) return Test::iterator.
/// - set.upper_bound(iterator, value), set.lower_bound(iterator, value)
///   return Test::iterator.
///
/// In addition, the following semantic requirements must hold:
/// - set.size() is even.
/// - set.begin()->is_endpoint() == false.
/// - set.upper_bound(value) returns an iterator to the first point
/// strictly greater than value, or set.end() if no such point exists.
/// - set.lower_bound(value) returns an iterator ot the first point
/// greater than or equal to value, or set.end() if no such point exists.
/// - If iterator is before or equal to set.upper_bound(value), then
/// set.upper_bound(iterator, value) == set.upper_bound(value). (Otherwise, the
/// result is undefined.)
/// - If iterator is before or equal to set.lower_bound(value), then
/// set.lower_bound(iterator, value) == set.lower_bound(value). (Otherwise, the
/// result is undefined.)
template <class Test>
concept Is_boundary_set = detail::Is_boundary_set_helper<
    Test, mysql::ranges::Range_iterator_type<Test>,
    mysql::ranges::Range_const_iterator_type<Test>, typename Test::Element_t>;

/// True if `Test` is an interval set over the given Set traits.
///
/// @see Is_boundary_set
template <class Test, class Set_traits_t>
concept Is_boundary_set_over_traits =
    Has_set_traits<Test, Set_traits_t> && Is_bounded_set_traits<Set_traits_t> &&
    Is_boundary_set<Test>;

/// True if `Test` is a reference to a boundary set.
///
/// @see Is_boundary_set
template <class Test>
concept Is_boundary_set_ref =
    Is_boundary_set<std::remove_reference_t<Test>> && std::is_reference_v<Test>;

/// True if `Test` is a reference to a boundary set.
///
/// @see Is_boundary_set_over_traits
template <class Test, class Set_traits_t>
concept Is_boundary_set_ref_over_traits =
    Is_boundary_set_over_traits<std::remove_reference_t<Test>, Set_traits_t> &&
    std::is_reference_v<Test>;

// True if std::remove_cvref_t<Test> is a boundary set over the given Set
// traits.
template <class Test, class Set_traits_t>
concept Is_boundary_set_over_traits_unqualified =
    Is_boundary_set_over_traits<std::remove_cvref_t<Test>, Set_traits_t>;

}  // namespace mysql::sets

// ==== Is_boundary_storage ====

namespace mysql::sets::detail {

/// True if `Test satisfies `Is_readable_boundary_storage` with  `Element_t` as
/// its element type, assuming that `Iterator_t` and `Const_iterator_t` are its
/// iterator/const iterator types.
///
/// @see Is_readable_boundary_storage
template <class Test, class Iterator_t, class Const_iterator_t, class Element_t>
concept Is_readable_boundary_storage_helper =
    Is_bounded_set_traits<typename Test::Set_traits_t> &&
    mysql::meta::Is_same_as_all<typename Test::Set_traits_t::Element_t,
                                typename Test::Element_t, Element_t> &&
    mysql::ranges::Is_collection_over<Test, Element_t> &&
    Is_boundary_iterator_over_type<Iterator_t, Element_t> &&
    Is_boundary_iterator_over_type<Const_iterator_t, Element_t> &&
    Is_upper_lower_bound_implementation<Test, Iterator_t, Const_iterator_t,
                                        Element_t>;

/// True if `Test satisfies `Is_boundary_storage` with  `Element_t` as its
/// element type, assuming that `Iterator_t` and `Const_iterator_t` are its
/// iterator/const iterator types.
///
/// @see Is_boundary_storage
template <class Test, class Iterator_t, class Const_iterator_t, class Element_t>
concept Is_boundary_storage_helper =
    Is_readable_boundary_storage_helper<Test, Iterator_t, Const_iterator_t,
                                        Element_t> &&
    requires(Test t, const Test ct, const Iterator_t i1, const Iterator_t i2,
             const Element_t e1, const Element_t e2) {
      t.assign(ct);
      t.clear();
      { t.update_point(i1, e1) } -> std::same_as<Iterator_t>;
      { t.insert(i1, e1, e2) } -> std::same_as<Iterator_t>;
      { t.erase(i1, i2) } -> std::same_as<Iterator_t>;
      { Test::has_fast_insertion } -> std::convertible_to<bool>;
    };

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// True if `Test` is a *readable boundary storage*, i.e., an object that can
/// be used as the back-end storage for a boundary container, without the
/// requirement to have altering operations.
///
/// Normally, use Is_boundary_storage instead. This weaker form is usable when
/// constraining parameters of the altering operations themselves, to prevent
/// circular dependencies among constraints.
///
/// This requires the following:
/// - `Test` satisfies `Is_collection` and
///   `Is_upper_lower_bound_implementation`.
/// - `Test::Set_traits_t` satisfies `Is_bounded_set_traits`
/// - The iterators for `Test` satisfy `Is_boundary_iterator`.
/// - The value types for the iterators equal `Test::Element_t` and
///   `Test::Set_traits_t::Element_t`.
template <class Test>
concept Is_readable_boundary_storage =
    detail::Is_readable_boundary_storage_helper<
        Test, mysql::ranges::Range_iterator_type<Test>,
        mysql::ranges::Range_const_iterator_type<Test>,
        typename Test::Element_t>;

/// True if Test is a boundary storage over the given Set traits.
///
/// @see Is_boundary_storage
template <class Test, class Set_traits_t>
concept Is_readable_boundary_storage_over_traits =
    Has_set_traits<Test, Set_traits_t> && Is_readable_boundary_storage<Test> &&
    Is_bounded_set_traits<Set_traits_t>;

/// True if `Test` is a *readable boundary storage*, i.e., an object that can
/// be used as the back-end storage for a boundary container.
///
/// This requires the following, where `storage` is an object of type
/// `Test`:
/// - `Is_readable_boundary_storage<Test>`.
/// - `storage.clear()`, `storage.insert(i, v1, v2)`, `storage.erase(i1,
///   i2)`, and `storage.update_point(i, v1)` are defined.
/// - Test::has_fast_insertion is a static constexpr bool member variable.
///
/// In addition, the following semantic requirements must hold:
/// - After `storage.clear()`, `storage.empty()` is true.
/// - `storage.insert(i, v1, v2)` shall insert v1 and v2 just before i, provided
///   all the following are true:
///   - v1 < v2
///   - i==begin() or the element preceding i is strictly less than v1.
///   - i==end() or *i > v2.
///   (Otherwise the effect of `storage.insert(i, v1, v2)` is undefined.)
/// - `storage.erase(i1, i2)` shall remove all elements between i1, inclusive,
///   and i2, exclusive, provided std::ranges::distance(i1, i2) is even. (If
///   the distance is odd, `storage.insert(i, v1, v2)` is undefined.)
/// - has_fast_insertion is true if insertion at a random position has time and
///   space complexity O(log(N)) (possibly expected, amortized); false
///   otherwise.
template <class Test>
concept Is_boundary_storage = detail::Is_boundary_storage_helper<
    Test, mysql::ranges::Range_iterator_type<Test>,
    mysql::ranges::Range_const_iterator_type<Test>, typename Test::Element_t>;

/// True if Test is a boundary storage over the given Set traits.
///
/// @see Is_boundary_storage
template <class Test, class Set_traits_t>
concept Is_boundary_storage_over_traits =
    Has_set_traits<Test, Set_traits_t> && Is_boundary_storage<Test> &&
    Is_bounded_set_traits<Set_traits_t>;

}  // namespace mysql::sets

namespace mysql::sets::detail {

/// Helper to define Storage_or_void.
///
/// Has type member `Type`, which equals: (1) `Container_t::Storage_t` if such a
/// member exists; (2) `void` otherwise.
template <class Container_t>
struct Storage_or_void_helper {
  using Type = void;
};
template <class Container_t>
  requires requires { typename Container_t::Storage_t; }
struct Storage_or_void_helper<Container_t> {
  using Type = typename Container_t::Storage_t;
};

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Type alias for Container_t::Storage_t if that exists, or void otherwise.
template <class Container_t>
using Storage_or_void = detail::Storage_or_void_helper<Container_t>::Type;

}  // namespace mysql::sets

// ==== Is_boundary_container ====

namespace mysql::sets::detail {

/// Helper to implement Is_boundary_container and Is_interval_container.
///
/// tparam Test class to test
///
/// tparam Element_t Element_t type passed to insert and remove members.
///
/// tparam Interval_t... Parameter type(s) passed to inplace_union,
/// inplace_subtract, and inplace_intersect with a single interval argument.
template <class Test, class Element_t, class... Interval_t>
concept Is_boundary_or_interval_container_helper =
    std::is_nothrow_move_assignable_v<Test> &&
    std::is_nothrow_move_constructible_v<Test> &&
    requires(Test t, Test other, Element_t value, Interval_t... interval) {
      t.assign(other);
      t.clear();
      t.insert(value);
      t.remove(value);
      t.inplace_union(other);
      t.inplace_union(interval...);
      t.inplace_subtract(other);
      t.inplace_subtract(interval...);
      t.inplace_intersect(other);
      t.inplace_intersect(interval...);
    };

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// True if `Test` is a Boundary container.
///
/// This requires that the container is a boundary set, has nothrow move
/// constructor/assignment operator, and that it has the following members:
///
/// @code
/// test.assign(source);
/// test.clear();
/// test.insert(value);
/// test.remove(value);
/// test.inplace_union(source);
/// test.inplace_union(start, exclusive_end);
/// test.inplace_union(cursor, start, exclusive_end);
/// test.inplace_subtract(source);
/// test.inplace_subtract(start, exclusive_end);
/// test.inplace_subtract(cursor, start, exclusive_end);
/// test.inplace_intersect(source);
/// test.inplace_intersect(start, exclusive_end);
/// @endcode
template <class Test>
concept Is_boundary_container =
    Is_boundary_set<Test> &&
    detail::Is_boundary_or_interval_container_helper<
        Test, typename Test::Set_traits_t::Element_t,
        typename Test::Set_traits_t::Element_t,
        typename Test::Set_traits_t::Element_t> &&
    requires(Test t, typename Test::Set_traits_t::Element_t start,
             typename Test::Set_traits_t::Element_t exclusive_end,
             mysql::ranges::Range_iterator_type<Test> it) {
      t.inplace_union(it, start, exclusive_end);
      t.inplace_subtract(it, start, exclusive_end);
    };

/// True if `Test` is a reference to a Boundary_container.
template <class Test>
concept Is_boundary_container_ref =
    Is_boundary_container<std::remove_reference_t<Test>> &&
    std::is_reference_v<Test>;

// ==== Enable_donate_set[_elements] ====

namespace detail {

/// Helper concept to define the condition when Enable_donate_set_elements shall
/// be defined for Boundary Storage types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_elements_for_boundary_storage =
    Is_boundary_storage<Source_t> && Is_boundary_storage<Target_t> &&
    requires(Source_t source, Target_t target,
             mysql::ranges::Range_iterator_type<Source_t> it,
             mysql::ranges::Range_value_type<Source_t> e1,
             mysql::ranges::Range_value_type<Source_t> e2) {
      {
        target.steal_and_insert(it, e1, e2, source)
        } noexcept
            -> std::same_as<mysql::ranges::Range_iterator_type<Target_t>>;
    };

/// Helper concept to define the condition when Enable_donate_set_elements shall
/// be defined for Boundary Container types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_elements_for_boundary_container =
    Is_boundary_set<Source_t> && Is_boundary_set<Target_t> &&
    Can_donate_set_elements_unqualified<typename Source_t::Storage_t,
                                        typename Target_t::Storage_t>;

}  // namespace detail

/// Declare that move-semantics is supported for element operations on Boundary
/// Storage types, whenever full-set-copy is enabled and the function
/// `steal_and_insert` is defined.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_elements_for_boundary_storage<
      Source_t, Target_t>
struct Enable_donate_set_elements<Source_t, Target_t> : public std::true_type {
};

/// Declare that move-semantics is supported for element operations on
/// compatible Boundary Set container types, whenever the storage supports it,
/// and full-set-copy is enabled.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_elements_for_boundary_container<
      Source_t, Target_t>
struct Enable_donate_set_elements<Source_t, Target_t> : public std::true_type {
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_META_H

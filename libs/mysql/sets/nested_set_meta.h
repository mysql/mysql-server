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

#ifndef MYSQL_SETS_NESTED_SET_META_H
#define MYSQL_SETS_NESTED_SET_META_H

/// @file
/// Experimental API header

#include <concepts>                                // same_as
#include "mysql/ranges/meta.h"                     // Is_collection_over
#include "mysql/sets/meta.h"                       // Enable_donate_set
#include "mysql/sets/nested_set_category.h"        // Nested_set_category_tag
#include "mysql/sets/set_categories.h"             // Base_set_category_tag
#include "mysql/sets/set_categories_and_traits.h"  // Is_set
#include "mysql/sets/set_traits.h"                 // Is_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== Is_nested_set_traits ====

/// True if Test is a Set Traits class for a nested set.
///
/// This requires Test is a set traits which has a type member Key_traits_t
/// which is an ordered set traits, and a type member Mapped_traits_t which is a
/// set traits, and a type member Mapped_category which is a set category.
template <class Test>
concept Is_nested_set_traits =
    Is_set_traits<Test> && Is_ordered_set_traits<typename Test::Key_traits_t> &&
    Is_set_traits<typename Test::Mapped_traits_t> &&
    Is_set_category<typename Test::Mapped_category_t>;

}  // namespace mysql::sets

// ==== Is_nested_set ====

namespace mysql::sets::detail {

/// Common helper for Is_nested_set and Is_nested_storage.
template <class Test>
concept Is_nested_set_or_storage =
    Is_nested_set_traits<typename Test::Set_traits_t> &&
    std::same_as<typename Test::Set_traits_t::Key_traits_t,
                 typename Test::Key_traits_t> &&
    std::same_as<typename Test::Set_traits_t::Mapped_traits_t,
                 typename Test::Mapped_traits_t> &&
    std::same_as<typename Test::Set_traits_t::Mapped_category_t,
                 typename Test::Mapped_category_t> &&
    std::same_as<typename Test::Set_traits_t::Key_traits_t::Element_t,
                 typename Test::Key_t> &&
    mysql::ranges::Is_collection_over<
        Test, std::pair<const typename Test::Key_t, typename Test::Mapped_t>> &&
    requires(Test t, const Test ct, typename Test::Key_t k) {
      { t.find(k) } -> std::same_as<mysql::ranges::Range_iterator_type<Test>>;
      {
        ct.find(k)
        } -> std::same_as<mysql::ranges::Range_const_iterator_type<Test>>;
    };

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// True if Test is a nested set.
///
/// Test must have the following type members:
///
/// - Set_category_t: equal to Nested_set_category_tag
/// - Set_traits_t: satisfies Is_nested_set_traits
/// - Key_traits: equal to Set_traits_t::Key_traits_t
/// - Mapped_traits_t: equal to Set_traits_t::Mapped_traits_t
/// - Mapped_category_t: equal to Set_traits_t::Mapped_category_t
/// - Key_t: equal to Set_traits_t::Key_traits_t::Element_t
///
/// Test must also satisfy the following concepts:
/// - Is_set
/// - Is_collection_over<pair<Test::Key_t, Test::Mapped_t>>
///   (which requires a number of member functions; @see Is_collection_over)
///
/// Additionally, test must support the following operations:
///
/// @code
/// t[k];       // return the mapped set for key k, if k is a key
/// t.find(k);  // return iterator to the pair with k as key, or to the end
/// @endcode
template <class Test>
concept Is_nested_set =
    Is_set<Test> && Has_set_category<Test, Nested_set_category_tag> &&
    detail::Is_nested_set_or_storage<Test> &&
    requires(Test t, const Test ct, typename Test::Key_t k) {
      {
        t[k]
        } -> Is_set_or_set_ref_over_category_and_traits<
            typename Test::Set_traits_t::Mapped_category_t,
            typename Test::Set_traits_t::Mapped_traits_t>;
      {
        ct[k]
        } -> Is_set_or_set_ref_over_category_and_traits<
            typename Test::Set_traits_t::Mapped_category_t,
            typename Test::Set_traits_t::Mapped_traits_t>;
    };

/// True if Test satisfies Is_nested_set and its traits is Set_traits_t.
template <class Test, class Set_traits_t>
concept Is_nested_set_over_traits =
    Is_nested_set<Test> && Has_set_traits<Test, Set_traits_t>;

// True if std::remove_cvref_t<Test> satisfies
// Is_nested_set_over_traits<Set_traits_t>.
template <class Test, class Set_traits_t>
concept Is_nested_set_over_traits_unqualified =
    Is_nested_set_over_traits<std::remove_cvref_t<Test>, Set_traits_t>;

// ==== Is_nested_storage ====

/// True if Test is a Nested Set Storage.
///
/// This is similar to Is_nested_set, with the following exceptions:
/// - This does not have to satisfy Is_set
/// - This does not require member Set_category_t
/// - This does not require subscript operator
/// - This requires the following members:
/// @code
/// t.clear();        // make the set empty
/// t.emplace(k);     // insert element with the given key and empty mapped set
/// t.emplace(it, k); // similar, with an iterator hint
/// t.erase(it);      // remove element pointed to by iterator
/// typename Test::Container_t // type of underlying container
/// @endcode
template <class Test>
concept Is_nested_storage =
    detail::Is_nested_set_or_storage<Test> &&
    requires(Test t, const Test ct, typename Test::Key_t k,
             mysql::ranges::Range_iterator_type<Test> it) {
      t.assign(ct);
      t.clear();
      t.emplace(k);
      t.emplace(it, k);
      { t.erase(it) } -> std::same_as<mysql::ranges::Range_iterator_type<Test>>;
    };

/// True if Test is a Nested Storage and its traits equals Set_traits_t.
template <class Test, class Set_traits_t>
concept Is_nested_storage_over_traits =
    Is_nested_storage<Test> && Has_set_traits<Test, Set_traits_t>;

// True if Test is a Nested Container.
template <class Test>
concept Is_nested_container =
    Is_nested_set<Test> && Is_nested_storage<typename Test::Storage_t> &&
    requires(Test t, Test ct, typename Test::Key_t k, typename Test::Mapped_t m,
             mysql::ranges::Range_iterator_type<Test> it) {
      t.clear();
      t.assign(t);

      t.inplace_union(t);
      t.inplace_union(k, m);

      t.inplace_intersect(t);
      t.inplace_intersect(k, m);
      t.inplace_intersect(k);

      t.inplace_subtract(t);
      t.inplace_subtract(k, m);
      t.inplace_subtract(k);
      t.inplace_subtract(it, k);
    };

// ==== Enable_donate_set[_elements] ====

namespace detail {

/// Helper concept to define the condition when Enable_donate_set_elements shall
/// be defined for Nested Storage types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_elements_for_nested_storage =
    Is_nested_storage<Source_t> && Is_nested_storage<Target_t> &&
    requires(Source_t source, Target_t target,
             mysql::ranges::Range_iterator_type<Target_t> target_it,
             mysql::ranges::Range_iterator_type<Source_t> source_it) {
      {
        target.steal_and_insert(target_it, source, source_it)
        } noexcept
            -> std::same_as<mysql::ranges::Range_iterator_type<Target_t>>;
    } &&
    Can_donate_set_elements_unqualified<typename Source_t::Mapped_t,
                                        typename Target_t::Mapped_t>;

/// Helper concept to define the condition when Enable_donate_set_elements shall
/// be defined for Nested Container types.
template <class Source_t, class Target_t>
concept Shall_enable_donate_set_elements_for_nested_container =
    Is_nested_set<Source_t> && Is_nested_set<Target_t> &&
    Can_donate_set_elements_unqualified<typename Source_t::Storage_t,
                                        typename Target_t::Storage_t>;

}  // namespace detail

/// Declare that move-semantics is supported for element operations on Nested
/// Storage types, whenever full-set-copy is enabled and the function
/// `steal_and_insert` is defined.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_elements_for_nested_storage<Source_t,
                                                                       Target_t>
struct Enable_donate_set_elements<Source_t, Target_t> : public std::true_type {
};

/// Declare that move-semantics is supported for element operations on
/// compatible Nested Set container types, whenever the storage supports it,
/// and full-set-copy is enabled.
template <class Source_t, class Target_t>
  requires detail::Shall_enable_donate_set_elements_for_nested_container<
      Source_t, Target_t>
struct Enable_donate_set_elements<Source_t, Target_t> : public std::true_type {
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_META_H

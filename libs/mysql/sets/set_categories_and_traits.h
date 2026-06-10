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

#ifndef MYSQL_SETS_SET_CATEGORIES_AND_TRAITS_H
#define MYSQL_SETS_SET_CATEGORIES_AND_TRAITS_H

/// @file
/// Experimental API header

#include <concepts>                     // same_as
#include "mysql/sets/set_categories.h"  // Has_same_set_category
#include "mysql/sets/set_traits.h"      // Has_same_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== Is_set* ====

/// True if Test is a set with category Category_t and traits Traits_t.
template <class Test, class Category_t, class Traits_t>
concept Is_set_over_category_and_traits =
    Is_set_category<Category_t> && Is_set_traits<Traits_t> &&
    std::same_as<typename Test::Set_category_t, Category_t> &&
    std::same_as<typename Test::Set_traits_t, Traits_t>;

/// True if Test is a set with category Category_t.
template <class Test, class Category_t>
concept Is_set_over_category =
    Is_set_over_category_and_traits<Test, Category_t,
                                    typename Test::Set_traits_t>;

/// True if Test is a set with traits Traits_t.
template <class Test, class Traits_t>
concept Is_set_over_traits =
    Is_set_over_category_and_traits<Test, typename Test::Set_category_t,
                                    Traits_t>;

/// True if Test is a set.
template <class Test>
concept Is_set = Is_set_over_traits<Test, typename Test::Set_traits_t>;

// ==== Is_set_ref* ====

/// True if Test is a reference to a set with category Category_t and traits
/// Traits_t.
template <class Test, class Category_t, class Traits_t>
concept Is_set_or_set_ref_over_category_and_traits =
    Is_set_over_category_and_traits<std::remove_reference_t<Test>, Category_t,
                                    Traits_t>;

// ==== Is_compatible_set ====

/// True if Set1_t and Set2_t have the same Set_category_t and Set_traits_t.
template <class Set1_t, class Set2_t>
concept Is_compatible_set =
    Is_set<Set1_t> && Is_set<Set2_t> && Has_same_set_category<Set1_t, Set2_t> &&
    Has_same_set_traits<Set1_t, Set2_t>;

// ==== Specific kinds of sets ====

/// True if the given set is of a category declared to be iterator-defined.
///
/// Iterator-defined means that any set type of the category must have member
/// functions begin and end, and two sets with the same traits are equal if the
/// sequences of values provided by iterating over the elements are equal.
///
/// For example, this enables the "default" equality comparison functions in
/// common_predicates.h.
template <class Test>
concept Is_iterator_defined_set =
    Is_set<Test> &&
    is_iterator_defined_set_category<typename Test::Set_category_t>;

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_SET_CATEGORIES_AND_TRAITS_H

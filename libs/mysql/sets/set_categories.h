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

#ifndef MYSQL_SETS_SET_CATEGORIES_H
#define MYSQL_SETS_SET_CATEGORIES_H

/// @file
/// Experimental API header

#include <concepts>                       // derived_from
#include "mysql/meta/optional_is_same.h"  // Optional_is_same

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Base class for all Set category tag classes.
///
/// Set categories are used to tag-dispatch to the correct algorithms when
/// computing operations on sets, such as unions and other set operations;
/// membership tests and other Boolean set predicates; string conversion; etc.
struct Base_set_category_tag {};

/// True if Test is a Set category tag, i.e., derived from
/// Base_set_category_tag.
template <class Test>
concept Is_set_category = std::derived_from<Test, Base_set_category_tag>;

/// True if Test has a member Set_category_t satisfying Is_set_category.
///
/// If the template argument Set_category_t is given, true only if
/// Test::Set_category_t is the same type as Set_category_t.
template <class Test, class Set_category_t = void>
concept Has_set_category =
    Is_set_category<typename Test::Set_category_t> &&
    mysql::meta::Optional_is_same<typename Test::Set_category_t,
                                  Set_category_t>;

/// True if the two Set classes have the same Set_category_t.
template <class Test1, class Test2>
concept Has_same_set_category =
    Has_set_category<Test1> && Has_set_category<Test2> &&
    std::same_as<typename Test1::Set_category_t,
                 typename Test2::Set_category_t>;

// ==== Specific kinds of set categories ====

/// Primary variable template for customization point that declares that a set
/// category is iterator-defined. See Is_iterator_defined_set for details.
///
/// To declare that a set category T is iterator-defined, specialize this like:
///
/// @code
/// template <>
/// inline constexpr bool is_iterator_defined_set_category<T> = true;
/// @endcode
template <Is_set_category>
inline constexpr bool is_iterator_defined_set_category = false;

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_SET_CATEGORIES_H

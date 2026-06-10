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

#ifndef MYSQL_META_IS_SAME_AS_ALL_H
#define MYSQL_META_IS_SAME_AS_ALL_H

/// @file
/// Experimental API header

#include <type_traits>

/// @addtogroup GroupLibsMysqlMeta
/// @{

namespace mysql::meta::detail {

/// Helper to implement Is_same_as_all. This is the primary template.
template <class... Types>
struct All_same_helper : std::false_type {};

/// Helper to implement Is_same_as_all. This is the specialization to zero
/// arguments.
template <>
struct All_same_helper<> : std::true_type {};

/// Helper to implement Is_same_as_all. This is the specialization to one
/// argument.
template <class Type>
struct All_same_helper<Type> : std::true_type {};

/// Helper to implement Is_same_as_all. This is the recursive step with two or
/// more arguments.
template <class First, class... Rest>
struct All_same_helper<First, First, Rest...>
    : All_same_helper<First, Rest...> {};

}  // namespace mysql::meta::detail

namespace mysql::meta {

/// True if all the given types are equal (like a vararg version of
/// std::same_as).
template <class... Types>
concept Is_same_as_all = detail::All_same_helper<Types...>::value;

}  // namespace mysql::meta

// addtogroup GroupLibsMysqlMeta
/// @}

#endif  // ifndef MYSQL_META_IS_SAME_AS_ALL_H

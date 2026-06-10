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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_META_IS_CHARLIKE_H
#define MYSQL_META_IS_CHARLIKE_H

/// @file
/// Experimental API header

#include <concepts>  // same_as
#include <cstddef>   // byte

/// @addtogroup GroupLibsMysqlMeta
/// @{

namespace mysql::meta {

/// true if `Test`, with cvref removed, is `char`, `unsigned char`, or
/// `std::byte`.
///
/// This is useful to define APIs that just take a raw pointer to a string, and
/// don't care if the characters are signed or unsigned.
///
/// This is intentionally true only for these three types, and not for e.g.
/// `int8_t`, because the C++ standard defines special cases for them. In
/// particular, `reinterpret_cast<T *>(...)` is defined for all argument types
/// only when `T` is `char`, `unsigned char`, or `std::byte`
/// (http://en.cppreference.com/w/cpp/language/reinterpret_cast.html sec "Type
/// Accessibility"; https://timsong-cpp.github.io/cppwp/n4868/basic.lval#11.3)
template <class Test>
concept Is_charlike = std::same_as<std::remove_cvref_t<Test>, char> ||
                      std::same_as<std::remove_cvref_t<Test>, unsigned char> ||
                      std::same_as<std::remove_cvref_t<Test>, std::byte>;

/// true if `Test` is pointer to `char`, `unsigned char`, or `std::byte`, and
/// not any array.
template <class Test>
concept Is_pointer_to_charlike =
    std::is_pointer_v<Test> && Is_charlike<std::remove_pointer_t<Test>>;

/// True if Test has a data() member function that returns char, unsigned char,
/// or std::byte, and has a size() member functions returning std::size_t.
///
/// This is useful to define APIs that take a "string", uses only the `data` and
/// `size` members, and don't care if it is represented as `std::string`,
/// `std::string_view`, or something else that has these members.
template <class Test>
concept Is_stringlike = requires(Test stringlike) {
                          { *stringlike.data() } -> Is_charlike;
                          { stringlike.size() } -> std::same_as<std::size_t>;
                        };

}  // namespace mysql::meta

// addtogroup GroupLibsMysqlMeta
/// @}

#endif  // ifndef MYSQL_META_IS_CHARLIKE_H

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

#ifndef MYSQL_UTILS_FORWARD_LIKE_H
#define MYSQL_UTILS_FORWARD_LIKE_H

/// @file
/// Experimental API header

#include <type_traits>  // is_lvalue_reference
#include <utility>      // move

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// Implementation of C++23's `std::forward_like`.
//
// Todo: When we have C++23, remove this and use `std::forward_like`.
//
// clang-tidy has many complaints here. Basically the reason is that we are not
// following typical idioms, and the reason for that is that we are implementing
// the idiom. This implementation is equivalent to the "possible implementation"
// at https://en.cppreference.com/w/cpp/utility/forward_like.html
template <class Qualifiers_from_t, class Value_t>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
constexpr auto &&forward_like(Value_t &&x) noexcept {
  constexpr bool is_const =
      std::is_const_v<std::remove_reference_t<Qualifiers_from_t>>;
  if constexpr (std::is_lvalue_reference_v<Qualifiers_from_t &&>) {
    if constexpr (is_const)
      return std::as_const(x);
    else
      // NOLINTNEXTLINE(readability-redundant-casting)
      return static_cast<Value_t &>(x);
  } else {
    if constexpr (is_const)
      return std::move(std::as_const(x));
    else
      // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
      return std::move(x);
  }
}

}  // namespace mysql::utils

// addtogroup GroupLibsMysqlUtils
/// @}

#endif  // ifndef MYSQL_UTILS_FORWARD_LIKE_H

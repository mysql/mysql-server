// Copyright (c) 2023, 2026, Oracle and/or its affiliates.
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

#include <gtest/gtest.h>            // TEST
#include <concepts>                 // same_as
#include "mysql/utils/char_cast.h"  // Char_cast_type

namespace {

// Statically assert that the type of char_cast<Targ_t>(c2) is equal to Ret_t,
// where c2 is of type Arg_t.
template <class Ret_t, class Targ_t, class Arg_t>
void assert_char_cast_one() {
  static_assert(std::same_as<decltype(mysql::utils::char_cast<Targ_t>(
                                 std::declval<Arg_t>())),
                             Ret_t>);
}

// Statically assert that the type of char_cast<Char1_t>(c2) is equal to
// Char1_t, whenever c2 is of type Char_t, Uchar_t, or Byte_t.
template <class Ret_t, class Targ_t, class Char_t, class Uchar_t, class Byte_t>
void assert_char_cast_three() {
  assert_char_cast_one<Ret_t, Targ_t, Char_t>();
  assert_char_cast_one<Ret_t, Targ_t, Uchar_t>();
  assert_char_cast_one<Ret_t, Targ_t, Byte_t>();
}

// Statically assert that the type of char_cast<X>(c2) is equal to X, whenever
// c2 is of type Char_t, Uchar_t, or Byte_t and X is equal to Char_t, Uchar_t,
// or Byte_t.
template <class Char_t, class Uchar_t, class Byte_t>
void assert_char_cast() {
  assert_char_cast_three<Char_t, char, Char_t, Uchar_t, Byte_t>();
  assert_char_cast_three<Uchar_t, unsigned char, Char_t, Uchar_t, Byte_t>();
  assert_char_cast_three<Byte_t, std::byte, Char_t, Uchar_t, Byte_t>();
}

// Verify at compile time that the return type from char_cast is as we expect,
// for many combinations of sized arrays, non-sized arrays, pointers, lvalue
// references, rvalue references, and const.
//
// There is nothing to execute, this is a compile-time-only test.
[[maybe_unused]] void compile_time_test() {
  assert_char_cast<char &, unsigned char &, std::byte &>();
  assert_char_cast<const char &, const unsigned char &, const std::byte &>();
  assert_char_cast<char *, unsigned char *, std::byte *>();
  assert_char_cast<const char *, const unsigned char *, const std::byte *>();
}

}  // namespace

TEST(LibsUtilsCharCast, Basic) {}

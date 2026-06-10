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

#ifndef MYSQL_DEBUGGING_UNITTEST_ASSERTIONS_H
#define MYSQL_DEBUGGING_UNITTEST_ASSERTIONS_H

/// @file
/// Experimental API header

#include <gtest/gtest.h>                      // ASSERT_EQ
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/utils/return_status.h"        // Return_status

/// @addtogroup GroupLibsMysqlDebugging
/// @{

namespace mysql::debugging {

// Use macros, because ASSERT_EQ is a macro that stringifies its argument for
// the error message.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// Equivalent to ASSERT_EQ(EXPRESSION, Return_status::ok)
#define ASSERT_OK(EXPRESSION) \
  ASSERT_EQ((EXPRESSION), mysql::utils::Return_status::ok)

/// Equivalent to ASSERT_EQ(EXPRESSION, Return_status::error)
#define ASSERT_ERROR(EXPRESSION) \
  ASSERT_EQ((EXPRESSION), mysql::utils::Return_status::error)

/// Assert that the result type of the given expression is void, and invoke it.
#define ASSERT_VOID(EXPRESSION)                              \
  do {                                                       \
    static_assert(std::same_as<decltype(EXPRESSION), void>); \
    (EXPRESSION);                                            \
  } while (false)

// NOLINTEND(cppcoreguidelines-macro-usage)

/// Assert that both (left==right) and !(left!=right) have the same truth values
/// as `equal`.
///
/// @param left Left-hand-side operand.
///
/// @param right Right-hand-side operand.
///
/// @param equal If true (the default), left and right are expected to be equal;
/// otherwise they are expected to be different.
void test_eq_one_way(const auto &left, const auto &right, bool equal = true) {
  ASSERT_EQ(left == right, equal);
  ASSERT_EQ(left != right, !equal);
}

/// Assert that (left==right), (right==left), !(left!=right), and !(right!=left)
/// all have the same truth values as `equal`.
///
/// @param left Left-hand-side operand.
///
/// @param right Right-hand-side operand.
///
/// @param equal If true (the default), left and right are expected to be equal;
/// otherwise they are expected to be different.
///
/// (Despite the parameter names, this will also test the case where left and
/// right are swapped).
void test_eq(const auto &left, const auto &right, bool equal = true) {
  {
    MY_SCOPED_TRACE("left OP right");
    test_eq_one_way(left, right, equal);
  }
  {
    MY_SCOPED_TRACE("right OP left");
    test_eq_one_way(right, left, equal);
  }
}

/// For all 7 comparison operators, assert that (left OP right) == (cmp OP 0).
///
/// @param left Left-hand-side operand.
///
/// @param right Right-hand-side operand.
///
/// @param cmp Expected outcome. This must be of a type that can be compared
/// with `0`: typically either `int` or `std::strong_ordering`.
void test_cmp_one_way(const auto &left, const auto &right, auto cmp) {
  MY_SCOPED_TRACE(cmp < 0 ? "lt" : cmp == 0 ? "eq" : "gt");
  test_eq_one_way(left, right, cmp == 0);
  ASSERT_EQ(left < right, cmp < 0);
  ASSERT_EQ(left > right, cmp > 0);
  ASSERT_EQ(left <= right, cmp <= 0);
  ASSERT_EQ(left >= right, cmp >= 0);
  ASSERT_EQ(left <=> right, cmp <=> 0);
}

/// For all 7 comparison operators, assert that (left OP right) == (cmp OP 0),
/// and that (right OP left) == (0 OP cmp).
///
/// @param left Left-hand-side operand.
///
/// @param right Right-hand-side operand.
///
/// @param cmp Expected outcome.
///
/// (Despite the parameter names, this will also test the case where left and
/// right are swapped and cmp reversed).
void test_cmp(const auto &left, const auto &right, std::strong_ordering cmp) {
  {
    MY_SCOPED_TRACE("left OP right");
    test_cmp_one_way(left, right, cmp);
  }
  {
    MY_SCOPED_TRACE("right OP left");
    test_cmp_one_way(right, left, cmp < 0 ? 1 : cmp == 0 ? 0 : -1);
  }
}

/// For all 7 comparison operators, assert that (left OP right) == (cmp OP 0),
/// and that (right OP left) == (0 OP cmp). This overload is for when cmp is of
/// type int.
///
/// @param left Left-hand-side operand.
///
/// @param right Right-hand-side operand.
///
/// @param cmp Expected outcome: -1 if left<right, 0 if left==right, and 1 if
/// left>right.
///
/// (Despite the parameter names, this will also test the case where left and
/// right are swapped and cmp reversed).
void test_cmp(const auto &left, const auto &right, int cmp) {
  test_cmp(left, right, cmp <=> 0);
}

}  // namespace mysql::debugging

// addtogroup GroupLibsMysqlDebugging
/// @}

#endif  // ifndef MYSQL_DEBUGGING_UNITTEST_ASSERTIONS_H

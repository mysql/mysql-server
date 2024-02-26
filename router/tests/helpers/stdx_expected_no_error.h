/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_TEST_HELPERS_STDX_EXPECTED_NO_ERROR_H_
#define MYSQL_TEST_HELPERS_STDX_EXPECTED_NO_ERROR_H_

#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected.h"

template <class T, class E>
::testing::AssertionResult StdxExpectedSuccess(const char *expr,
                                               const stdx::expected<T, E> &e) {
  if (e) return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "Expected: " << expr << " succeeds.\n"
                                       << "  Actual: " << e.error() << "\n";
}

template <class T, class E>
::testing::AssertionResult StdxExpectedFailure(const char *expr,
                                               const stdx::expected<T, E> &e) {
  if (!e) return ::testing::AssertionSuccess();

  if constexpr (std::is_void_v<T>) {
    return ::testing::AssertionFailure() << "Expected: " << expr << " fails.\n"
                                         << "  Actual: succeeded\n";
  } else {
    return ::testing::AssertionFailure()
           << "Expected: " << expr << " fails.\n"
           << "  Actual: " << ::testing::PrintToString(e.value()) << "\n";
  }
}

#define EXPECT_NO_ERROR(x) EXPECT_PRED_FORMAT1(StdxExpectedSuccess, (x))
#define ASSERT_NO_ERROR(x) ASSERT_PRED_FORMAT1(StdxExpectedSuccess, (x))

#define EXPECT_ERROR(x) EXPECT_PRED_FORMAT1(StdxExpectedFailure, (x))
#define ASSERT_ERROR(x) ASSERT_PRED_FORMAT1(StdxExpectedFailure, (x))

#endif

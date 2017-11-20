/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef ASSERT_ERROR_CODE_H_
#define ASSERT_ERROR_CODE_H_

#include <gtest/gtest.h>

#include "ngs/error_code.h"
#include "mysqlx_error.h"

namespace xpl {
namespace test {

inline ::testing::AssertionResult Assert_error_code(const char* e1_expr,
                                                    const char* e2_expr, int e1,
                                                    const ngs::Error_code& e2) {
  return (e1 == e2.error) ? ::testing::AssertionSuccess()
                          : (::testing::AssertionFailure()
                             << "Value of: " << e2_expr << "\nActual: {"
                             << e2.error << ", " << e2.message << "}\n"
                             << "Expected: " << e1_expr << "\nWhich is:" << e1);
}

#define ASSERT_ERROR_CODE(a, b) ASSERT_PRED_FORMAT2(Assert_error_code, a, b);
#define ER_X_SUCCESS 0
#define ER_X_ARTIFICIAL1 7001
#define ER_X_ARTIFICIAL2 7002
#define ER_X_ARTIFICIAL3 7003
#define ER_X_ARTIFICIAL4 7004
#define ER_X_ARTIFICIAL5 7005

}  // namespace test
}  // namespace xpl

#endif  // ASSERT_ERROR_CODE_H_

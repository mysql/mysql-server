/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_HELPER_EXPECT_THROW_MSG_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_HELPER_EXPECT_THROW_MSG_H_

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "helpers/router_test_helpers.h"

namespace testing {
namespace internal {

struct String_msg {
  String_msg(const char *str) : value(str) {}
  operator bool() const { return true; }
  std::string value;
};

}  // namespace internal
}  // namespace testing

#define TEST_THROW_MSG_(statement, expected_exception, msg, fail)             \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                               \
  if (::testing::internal::String_msg gtest_msg = "") {                       \
    bool gtest_caught_expected = false;                                       \
    try {                                                                     \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);              \
    } catch (expected_exception const &ee) {                                  \
      std::string actual(ee.what());                                          \
      if (actual == msg) {                                                    \
        gtest_caught_expected = true;                                         \
      } else {                                                                \
        gtest_msg.value = "Expected: " #statement                             \
                          " throws an exception with message \"" +            \
                          std::string(msg) + "\".\n  Actual: message is \"" + \
                          actual + "\".";                                     \
        goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__);        \
      }                                                                       \
    } catch (...) {                                                           \
      gtest_msg.value = "Expected: " #statement                               \
                        " throws an exception of type " #expected_exception   \
                        ".\n  Actual: it throws a different type.";           \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__);          \
    }                                                                         \
    if (!gtest_caught_expected) {                                             \
      gtest_msg.value = "Expected: " #statement                               \
                        " throws an exception of type " #expected_exception   \
                        ".\n  Actual: it throws nothing.";                    \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__);          \
    }                                                                         \
  } else                                                                      \
    GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__)                  \
        : fail(gtest_msg.value.c_str())

#define EXPECT_THROW_MSG(statement, expected_exception, msg) \
  TEST_THROW_MSG_(statement, expected_exception, msg, GTEST_NONFATAL_FAILURE_)

#define EXPECT_HTTP_ERROR_(statement, sts, msg, fail)                  \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                        \
  if (::testing::internal::String_msg gtest_msg = "") {                \
    bool gtest_caught_expected = false;                                \
    try {                                                              \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);       \
    } catch (mrs::http::Error const &ee) {                             \
      if (ee.message == msg && ee.status == sts) {                     \
        gtest_caught_expected = true;                                  \
      } else {                                                         \
        gtest_msg.value =                                              \
            "Expected: " #statement " throws HTTP Error status=" +     \
            std::to_string(sts) + " message=\"" + std::string(msg) +   \
            "\".\n  Actual: status=" + std::to_string(ee.status) +     \
            " message=\"" + ee.message + "\".";                        \
        goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__); \
      }                                                                \
    } catch (...) {                                                    \
      gtest_msg.value = "Expected: " #statement                        \
                        " throws an exception of type http::Error"     \
                        ".\n  Actual: it throws a different type.";    \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__);   \
    }                                                                  \
    if (!gtest_caught_expected) {                                      \
      gtest_msg.value = "Expected: " #statement                        \
                        " throws an exception of type http::Error"     \
                        ".\n  Actual: it throws nothing.";             \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__);   \
    }                                                                  \
  } else                                                               \
    GTEST_CONCAT_TOKEN_(gtest_label_testthrowmsg_, __LINE__)           \
        : fail(gtest_msg.value.c_str())

#define EXPECT_HTTP_ERROR(statement, sts, msg) \
  EXPECT_HTTP_ERROR_(statement, sts, msg, GTEST_NONFATAL_FAILURE_)

#define EXPECT_REST_ERROR(statement, msg) \
  EXPECT_THROW_LIKE(statement, mrs::interface::RestError, msg)

#define EXPECT_MYSQL_ERROR(statement, msg) \
  EXPECT_THROW_LIKE(statement, mysqlrouter::MySQLSession::Error, msg)

#define EXPECT_DUALITY_ERROR(statement, msg) \
  EXPECT_THROW_LIKE(statement, mrs::database::DualityViewError, msg)

#define EXPECT_JSON_ERROR(statement, msg) \
  EXPECT_THROW_LIKE(statement, mrs::database::JSONInputError, msg)

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_HELPER_EXPECT_THROW_MSG_H_
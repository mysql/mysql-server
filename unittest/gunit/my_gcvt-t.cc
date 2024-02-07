/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "my_sys.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/m_ctype.h"
#include "sql/sql_const.h"

namespace double_to_string_to_double_unittest {

const double test_values[] = {DBL_MAX, DBL_MIN, -DBL_MAX, -DBL_MIN};

class GcvtTest : public ::testing::TestWithParam<double> {
 protected:
  void SetUp() override { test_input = GetParam(); }
  double test_input;
};

INSTANTIATE_TEST_SUITE_P(Foo, GcvtTest, ::testing::ValuesIn(test_values));

CHARSET_INFO *init_collation(const char *name) {
  MY_CHARSET_ERRMSG errmsg;
  return my_collation_get_by_name(name, MYF(0), &errmsg);
}

TEST_P(GcvtTest, Convert) {
  // DBL_MIN and -DBL_MAX requires + 1
  // -DBL_MIN requires another + 1
  const int width = MAX_DOUBLE_STR_LENGTH + 2;

  char buff[MAX_DOUBLE_STR_LENGTH * 2] = {};
  bool error{false};

  char *to = &buff[0];
  size_t len = my_gcvt(test_input, MY_GCVT_ARG_DOUBLE, width, to, &error);
  EXPECT_LE(len, width);
  EXPECT_FALSE(error);

  int int_error{0};
  const char *str_start = to;
  const char *str_end = to + width;
  double result = my_strtod(str_start, &str_end, &int_error);
  EXPECT_EQ(0, int_error) << "buff[" << buff << "]" << std::endl;
  EXPECT_EQ(test_input, result) << "buff[" << buff << "]" << std::endl;

  CHARSET_INFO *cs = init_collation("utf8mb4_0900_as_ci");
  const char *end;
  int conv_error;
  double nr = my_strntod(cs, str_start, width, &end, &conv_error);
  EXPECT_EQ(0, conv_error) << "buff[" << buff << "]" << std::endl;
  EXPECT_EQ(test_input, nr) << "buff[" << buff << "]" << std::endl;
}

}  // namespace double_to_string_to_double_unittest

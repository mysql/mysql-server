/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <array>

#include "field_types.h"
#include "my_inttypes.h"
#include "mysql/binlog/event/export/binary_log_funcs.h"

namespace mysql::binlog::event::unittests {

TEST(BinaryLogFuncsTest, CheckedVarcharRejectsTruncatedTwoByteLength) {
  const std::array<uchar, 1> row_data{{0}};
  uint32 field_size = 123;

  EXPECT_EQ(CALC_FIELD_SIZE_TRUNCATED,
            calc_field_size_checked(MYSQL_TYPE_VARCHAR, row_data.data(), 300,
                                    row_data.size(), &field_size));
  EXPECT_EQ(0U, field_size);
}

TEST(BinaryLogFuncsTest, CheckedVarcharAcceptsCompleteTwoByteLength) {
  const std::array<uchar, 5> row_data{{3, 0, 'a', 'b', 'c'}};
  uint32 field_size = 0;

  EXPECT_EQ(CALC_FIELD_SIZE_OK,
            calc_field_size_checked(MYSQL_TYPE_VARCHAR, row_data.data(), 300,
                                    row_data.size(), &field_size));
  EXPECT_EQ(5U, field_size);
}

TEST(BinaryLogFuncsTest, CheckedBlobRejectsTruncatedLength) {
  const std::array<uchar, 1> row_data{{0}};
  uint32 field_size = 123;

  EXPECT_EQ(CALC_FIELD_SIZE_TRUNCATED,
            calc_field_size_checked(MYSQL_TYPE_BLOB, row_data.data(), 2,
                                    row_data.size(), &field_size));
  EXPECT_EQ(0U, field_size);
}

}  // namespace mysql::binlog::event::unittests

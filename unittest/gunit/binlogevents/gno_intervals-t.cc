/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "libbinlogevents/include/gtids/gtidset.h"

namespace binary_log::gtids::unittests {

class GnoIntervalTest : public ::testing::Test {
 protected:
  GnoIntervalTest() = default;

  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(GnoIntervalTest, GnoIntervalBasic) {
  binary_log::gtids::Gno_interval i1{1, 1};
  binary_log::gtids::Gno_interval i2{100, 200};

  ASSERT_EQ(i1.get_start(), 1);
  ASSERT_EQ(i1.get_end(), 1);

  ASSERT_EQ(i2.get_start(), 100);
  ASSERT_EQ(i2.get_end(), 200);
}

TEST_F(GnoIntervalTest, GnoIntervalComparison) {
  binary_log::gtids::Gno_interval i_1_1{1, 1};
  binary_log::gtids::Gno_interval i_1_1_copy{1, 1};
  binary_log::gtids::Gno_interval i_1_2{1, 2};

  ASSERT_TRUE(i_1_1 == i_1_1_copy);
  ASSERT_FALSE(i_1_1 == i_1_2);
}

TEST_F(GnoIntervalTest, GnoIntervalCopyAssignment) {
  binary_log::gtids::Gno_interval i2{100, 200};
  binary_log::gtids::Gno_interval i2_assigned = i2;

  ASSERT_EQ(i2_assigned, i2);
}

TEST_F(GnoIntervalTest, GnoIntervalLessThan) {
  binary_log::gtids::Gno_interval i_1_1{1, 1};
  binary_log::gtids::Gno_interval i_1_1_copy{1, 1};
  binary_log::gtids::Gno_interval i_1_2{1, 2};
  binary_log::gtids::Gno_interval i_100_200{100, 200};
  binary_log::gtids::Gno_interval i_100_150{100, 150};

  ASSERT_TRUE(i_1_1 < i_100_200);
  ASSERT_TRUE(i_1_1 < i_1_2);
  ASSERT_FALSE(i_1_1_copy < i_1_1);
  ASSERT_FALSE(i_100_200 < i_100_150);
  ASSERT_TRUE(i_100_150 < i_100_200);
}

TEST_F(GnoIntervalTest, GnoIntervalIntersection) {
  binary_log::gtids::Gno_interval orig{10, 20};

  binary_log::gtids::Gno_interval i1{8, 9};
  ASSERT_FALSE(orig.intersects(i1));
  ASSERT_FALSE(i1.intersects(orig));

  binary_log::gtids::Gno_interval i2{22, 23};
  ASSERT_FALSE(orig.intersects(i2));
  ASSERT_FALSE(i2.intersects(orig));

  binary_log::gtids::Gno_interval i3{5, 11};
  ASSERT_TRUE(orig.intersects(i3));
  ASSERT_TRUE(i3.intersects(orig));

  binary_log::gtids::Gno_interval i4{20, 25};
  ASSERT_TRUE(orig.intersects(i4));
  ASSERT_TRUE(i4.intersects(orig));

  binary_log::gtids::Gno_interval i5{21, 1000};
  ASSERT_FALSE(orig.intersects(i5));
  ASSERT_FALSE(i5.intersects(orig));

  binary_log::gtids::Gno_interval i6{8, 8};
  ASSERT_FALSE(i6.intersects(orig));
}

TEST_F(GnoIntervalTest, GnoIntervalContiguous) {
  binary_log::gtids::Gno_interval i1{10, 20};
  binary_log::gtids::Gno_interval i2{8, 10};
  ASSERT_FALSE(i1.contiguous(i2));
  ASSERT_FALSE(i2.contiguous(i1));

  binary_log::gtids::Gno_interval i2_1{8, 9};
  ASSERT_TRUE(i1.contiguous(i2_1));
  ASSERT_TRUE(i2_1.contiguous(i1));

  binary_log::gtids::Gno_interval i3{10, 20};
  binary_log::gtids::Gno_interval i4{21, 22};
  ASSERT_TRUE(i4.contiguous(i3));
  ASSERT_TRUE(i3.contiguous(i4));

  binary_log::gtids::Gno_interval i5{10, 20};
  binary_log::gtids::Gno_interval i6{100, 200};
  ASSERT_FALSE(i5.contiguous(i6));
  ASSERT_FALSE(i6.contiguous(i5));

  binary_log::gtids::Gno_interval i7{10, 20};
  binary_log::gtids::Gno_interval i8{15, 18};
  ASSERT_FALSE(i7.contiguous(i8));
  ASSERT_FALSE(i8.contiguous(i7));
}

TEST_F(GnoIntervalTest, GnoIntervalAdd) {
  binary_log::gtids::Gno_interval i1{10, 20};
  binary_log::gtids::Gno_interval i2{8, 10};

  ASSERT_FALSE(i1.add(i2));

  binary_log::gtids::Gno_interval i3{10, 20};
  binary_log::gtids::Gno_interval i4{8, 8};

  ASSERT_TRUE(i3.add(i4));
  ASSERT_TRUE(i4.add(i3));

  binary_log::gtids::Gno_interval i5{10, 20};
  binary_log::gtids::Gno_interval i6{21, 100};

  ASSERT_FALSE(i5.add(i6));
}

TEST_F(GnoIntervalTest, GnoIntervalCount) {
  binary_log::gtids::Gno_interval i1{10, 20};

  // the interval has eleven elements, first one is 10, last one is 20.
  ASSERT_EQ(i1.count(), 11);

  binary_log::gtids::Gno_interval i2{10, 10};
  ASSERT_EQ(i2.count(), 1);
}

TEST_F(GnoIntervalTest, GnoIntervalToString) {
  binary_log::gtids::Gno_interval i1{1, 1};
  ASSERT_EQ(i1.to_string(), "1");

  binary_log::gtids::Gno_interval i2{1, 9};
  ASSERT_EQ(i2.to_string(), "1-9");
}

TEST_F(GnoIntervalTest, GnoIntervalInvalid) {
  binary_log::gtids::Gno_interval i1{1, 1};
  ASSERT_TRUE(i1.is_valid());

  binary_log::gtids::Gno_interval i2{1, 2};
  ASSERT_TRUE(i2.is_valid());

  binary_log::gtids::Gno_interval i3{2, 1};
  ASSERT_FALSE(i3.is_valid());

  binary_log::gtids::Gno_interval i4{-1, 1};
  ASSERT_FALSE(i4.is_valid());
}

}  // namespace binary_log::gtids::unittests

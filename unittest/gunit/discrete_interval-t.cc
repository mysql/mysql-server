/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <gtest/gtest.h>

#include "sql/discrete_interval.h"

namespace {

TEST(DiscreteIntervalTest, MultiplicationOverflow) {
  Discrete_interval interval(1, ULLONG_MAX / 2 + 1, 2);
  EXPECT_EQ(ULLONG_MAX, interval.maximum());
  EXPECT_TRUE(interval.in_range(ULLONG_MAX - 2));
}

TEST(DiscreteIntervalTest, ContiguousBoundOverflow) {
  Discrete_interval interval(ULLONG_MAX - 8, 1, 3);
  EXPECT_FALSE(interval.merge_if_contiguous(ULLONG_MAX - 5, 2, 3));
  EXPECT_EQ(3, interval.values());
  EXPECT_EQ(ULLONG_MAX, interval.maximum());
  EXPECT_TRUE(interval.in_range(ULLONG_MAX - 2));
}

TEST(DiscreteIntervalTest, MergeUnboundedInterval) {
  Discrete_interval interval(1, 2, 1);
  EXPECT_FALSE(interval.merge_if_contiguous(3, ULLONG_MAX, 1));
  EXPECT_EQ(ULLONG_MAX, interval.values());
  EXPECT_EQ(ULLONG_MAX, interval.maximum());
  EXPECT_FALSE(interval.merge_if_contiguous(ULLONG_MAX, 1, 1));
  EXPECT_EQ(ULLONG_MAX, interval.values());
  EXPECT_EQ(ULLONG_MAX, interval.maximum());
}

}  // namespace

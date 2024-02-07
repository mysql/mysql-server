/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql/binlog/event/compression/buffer/grow_calculator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mysql::binlog::event::compression::buffer {
namespace grow_calculator::unittest {

using Grow_calculator_t =
    mysql::binlog::event::compression::buffer::Grow_calculator;
using Size_t =
    mysql::binlog::event::compression::buffer::Grow_calculator::Size_t;
using Result_t =
    mysql::binlog::event::compression::buffer::Grow_calculator::Result_t;

TEST(GrowPolicyTest, BasicGrowPolicyTest) {
  Grow_calculator_t grow_calculator;
  grow_calculator.set_max_size(1000);
  grow_calculator.set_grow_factor(1.5);
  grow_calculator.set_grow_increment(100);
  grow_calculator.set_block_size(101);
  auto success = [](Size_t size) { return Result_t(false, size); };
  auto error = Result_t(true, 0);

  // increment decides size
  EXPECT_EQ(success(101), grow_calculator.compute_new_size(0, 1));
  // factor decides size
  EXPECT_EQ(success(808), grow_calculator.compute_new_size(500, 501));
  // max_capacity caps size
  EXPECT_EQ(success(1000), grow_calculator.compute_new_size(700, 701));
  // boundary around block_size
  EXPECT_EQ(success(101), grow_calculator.compute_new_size(0, 100));
  EXPECT_EQ(success(101), grow_calculator.compute_new_size(0, 101));
  EXPECT_EQ(success(202), grow_calculator.compute_new_size(0, 102));

  // can reach the max capacity
  EXPECT_EQ(success(1000), grow_calculator.compute_new_size(0, 1000));
  // cannot exceed the max capacity
  EXPECT_EQ(error, grow_calculator.compute_new_size(0, 1001));
}

}  // namespace grow_calculator::unittest
}  // namespace mysql::binlog::event::compression::buffer

/*
   Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_config.h"
#include <gtest/gtest.h>

#include <nullable.h>

namespace nullable_unittest {

using Mysql::Nullable;

class NullableTest : public ::testing::TestWithParam<uint>
{
protected:
  virtual void SetUp() {}
};

TEST(NullableTest, NullConstructor)
{
  Nullable<int> nullable_int;
  EXPECT_FALSE(nullable_int.has_value());
}


TEST(NullableTest, ValueConstructor)
{
  Nullable<int> nullable_int(42);
  EXPECT_TRUE(nullable_int.has_value());
  EXPECT_EQ(42, nullable_int.value());

  Nullable<int> nullable_int2= 42;
  EXPECT_TRUE(nullable_int2.has_value());
  EXPECT_EQ(42, nullable_int2.value());

}


TEST(NullableTest, Assignment)
{
  Nullable<int> ni(42);
  Nullable<int> ni2= ni;
  EXPECT_TRUE(ni2.has_value());
  EXPECT_EQ(42, ni2.value());

  Nullable<int> mynull;
  Nullable<int> mynull2= mynull;
  EXPECT_FALSE(mynull2.has_value());
}

}

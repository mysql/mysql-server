/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "sql_array.h"

namespace bounds_check_array_unittest {

typedef Bounds_checked_array<int> Int_array;

class BoundsCheckedArray : public ::testing::Test
{
public:
  BoundsCheckedArray() : some_integer(0) {}

  virtual void SetUp()
  {
    for (int ix= 0; ix < c_array_size; ++ix)
      c_array[ix]= ix;
  }

  static const int c_array_size= 5;
  int c_array[c_array_size];

  int some_integer;
  Int_array int_array;
};

}

/*
  operator<<() is needed by the EXPECT macros.
  It is a template argument, so static rather than in unnamed namespace.
 */
static inline
std::ostream &operator<<(std::ostream &s,
                         const bounds_check_array_unittest::Int_array &v)
{
  return s << "{"
           << v.array() << ", "
           << v.size()
           << "}"
    ;
}

namespace bounds_check_array_unittest  {

TEST_F(BoundsCheckedArray, Empty)
{
  EXPECT_EQ(sizeof(int), int_array.element_size());
  EXPECT_EQ(0U, int_array.size());
  EXPECT_TRUE(int_array.is_null());
  int *pi= NULL;
  EXPECT_EQ(pi, int_array.array());
}

#if !defined(DBUG_OFF)

// Google Test recommends DeathTest suffix for classes used in death tests.
typedef BoundsCheckedArray BoundsCheckedArrayDeathTest;

TEST_F(BoundsCheckedArrayDeathTest, BoundsCheckRead)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int_array= Int_array(c_array, 2);
  EXPECT_DEATH_IF_SUPPORTED(some_integer= int_array[5],
                            ".*Assertion .*n < m_size.*");
}

TEST_F(BoundsCheckedArrayDeathTest, BoundsCheckAssign)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int_array= Int_array(c_array, 2);
  EXPECT_DEATH_IF_SUPPORTED(int_array[5]= some_integer,
                            ".*Assertion .*n < m_size.*");
}

TEST_F(BoundsCheckedArrayDeathTest, BoundsCheckPopFront)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int_array= Int_array(c_array, 1);
  int_array.pop_front();
  EXPECT_DEATH_IF_SUPPORTED(int_array.pop_front(),
                            ".*Assertion .*m_size > 0.*");
}

TEST_F(BoundsCheckedArrayDeathTest, BoundsCheckResize)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int_array= Int_array(c_array, 1);
  EXPECT_DEATH_IF_SUPPORTED(int_array.resize(2),
                            ".*Assertion .*new_size <= m_size.*");
}

TEST_F(BoundsCheckedArrayDeathTest, BoundsCheckResizeAssign)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  int_array= Int_array(c_array, 2);
  int_array[1]= some_integer;
  int_array.resize(1);
  EXPECT_DEATH_IF_SUPPORTED(int_array[1]= some_integer,
                            ".*Assertion .*n < m_size.*");
}

#endif  // !defined(DBUG_OFF)

TEST_F(BoundsCheckedArray, Indexing)
{
  int_array= Int_array(c_array, c_array_size);
  EXPECT_EQ(0, int_array[0]);
  int_array[0]= 42;
  EXPECT_EQ(42, int_array[0]);
}


TEST_F(BoundsCheckedArray, Reset)
{
  int_array= Int_array(c_array, c_array_size);
  EXPECT_EQ(c_array, int_array.array());
  EXPECT_FALSE(int_array.is_null());
  int_array.reset();
  int *pi= NULL;
  EXPECT_EQ(pi, int_array.array());
  EXPECT_TRUE(int_array.is_null());
}

TEST_F(BoundsCheckedArray, Resize)
{
  int_array= Int_array(c_array, c_array_size);
  int_array.resize(c_array_size - 1);
  EXPECT_EQ(c_array_size - 1, static_cast<int>(int_array.size()));
  
  int count= 0;
  while (int_array.size() > 0)
  {
    EXPECT_EQ(count, int_array[0]);
    count++;
    int_array.pop_front();
  }

  EXPECT_EQ(count, c_array_size - 1);
}


TEST_F(BoundsCheckedArray, PopFront)
{
  int_array= Int_array(c_array, c_array_size);
  for (int ix= 0; ix < c_array_size; ++ix)
  {
    EXPECT_EQ(ix, int_array[0]);
    int_array.pop_front();
  }
}

TEST_F(BoundsCheckedArray, Equality)
{
  int_array= Int_array(c_array, c_array_size);
  EXPECT_EQ(int_array, int_array);

  Int_array int_array_copy(int_array);
  EXPECT_EQ(int_array, int_array_copy);

  int_array_copy.resize(c_array_size - 1);
  EXPECT_NE(int_array, int_array_copy);

  // We share the underlying array, so these should be equal.
  Int_array int_array_two(c_array, c_array_size);
  EXPECT_EQ(int_array, int_array_two);

  int_array_two.pop_front();
  EXPECT_NE(int_array, int_array_two);
}

}  // namespace

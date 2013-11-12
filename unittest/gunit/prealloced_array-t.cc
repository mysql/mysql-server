/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <algorithm>

#include "prealloced_array.h"
#include "sql_alloc.h"

namespace prealloced_array_unittest {

class PreallocedArrayTest : public ::testing::Test
{
public:
  PreallocedArrayTest()
    : int_10(PSI_NOT_INSTRUMENTED)
  {}

protected:
  Prealloced_array<int, 10> int_10;
  int some_integer;
};


TEST_F(PreallocedArrayTest, Empty)
{
  EXPECT_EQ(10U, int_10.capacity());
  EXPECT_EQ(sizeof(int), int_10.element_size());
  EXPECT_TRUE(int_10.empty());
  EXPECT_EQ(0U, int_10.size());
}

#if !defined(DBUG_OFF)
// Google Test recommends DeathTest suffix for classes used in death tests.
typedef PreallocedArrayTest PreallocedArrayDeathTest;

TEST_F(PreallocedArrayDeathTest, OutOfBoundsRead)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(some_integer= int_10[5],
                            ".*Assertion .*n < size.*");
}

TEST_F(PreallocedArrayDeathTest, OutOfBoundsWrite)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(int_10[5] = some_integer,
                            ".*Assertion .*n < size.*");
}

TEST_F(PreallocedArrayDeathTest, EmptyBack)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(int_10.back() = 42,
                            ".*Assertion .*n < size.*");
}

TEST_F(PreallocedArrayDeathTest, EmptyPopBack)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(int_10.pop_back(),
                            ".*Assertion .*!empty.*");
}

TEST_F(PreallocedArrayDeathTest, EmptyErase)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(int_10.erase(0),
                            ".*Assertion .*n < size.*");
}

#endif // DBUG_OFF

TEST_F(PreallocedArrayTest, Insert5)
{
  for (int ix= 0; ix < 5; ++ix)
    int_10.push_back(ix);
  for (int ix= 0; ix < 5; ++ix)
    EXPECT_EQ(ix, int_10[ix]);
  for (int ix= 0; ix < 5; ++ix)
    int_10[ix]= ix;
  EXPECT_EQ(5U, int_10.size());
  EXPECT_EQ(10U, int_10.capacity());
}

TEST_F(PreallocedArrayTest, Insert15)
{
  for (int ix= 0; ix < 15; ++ix)
    int_10.push_back(ix);
  for (int ix= 0; ix < 15; ++ix)
    EXPECT_EQ(ix, int_10[ix]);
  for (int ix= 0; ix < 15; ++ix)
    int_10[ix]= ix;
  EXPECT_EQ(15U, int_10.size());
  EXPECT_LE(15U, int_10.capacity());
}

TEST_F(PreallocedArrayTest, Sort)
{
  for (int ix= 20; ix >= 0; --ix)
    int_10.push_back(ix);
  std::sort(int_10.begin(), int_10.end());
  for (int ix= 0; ix <= 20; ++ix)
    EXPECT_EQ(ix, int_10[ix]);
}

TEST_F(PreallocedArrayTest, Back)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(15, int_10.back());
  int_10.back()= 42;
  EXPECT_EQ(42, int_10.back());
}

TEST_F(PreallocedArrayTest, PopBack)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  for (int ix= 15; ix >= 0; --ix)
  {
    EXPECT_EQ(ix, int_10.back());
    int_10.pop_back();
  }
}

TEST_F(PreallocedArrayTest, EraseFirst)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(0, int_10[0]);
  EXPECT_EQ(16U, int_10.size());
  int_10.erase(0);
  EXPECT_EQ(15U, int_10.size());
  for (int ix= 0; ix < static_cast<int>(int_10.size()); ++ix)
  {
    EXPECT_EQ(ix + 1, int_10[ix]);
  }
}

TEST_F(PreallocedArrayTest, EraseLast)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(15, int_10.back());
  EXPECT_EQ(15, int_10.at(15));
  int_10.erase(15);
  EXPECT_EQ(14, int_10.back());
  EXPECT_EQ(14, int_10.at(14));
}

TEST_F(PreallocedArrayTest, EraseMiddle)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(6, int_10[6]);
  EXPECT_EQ(7, int_10[7]);
  EXPECT_EQ(16U, int_10.size());
  int_10.erase(7);
  EXPECT_EQ(6, int_10[6]);
  EXPECT_EQ(8, int_10[7]);
  EXPECT_EQ(9, int_10[8]);
  EXPECT_EQ(15U, int_10.size());
}

/*
  A simple class for testing that object copying and destruction is done
  properly when we have to expand the array a few times,
  and has_trivial_destructor == false.
 */
class IntWrap
{
public:
  explicit IntWrap(int arg)
  {
    m_int= new int(arg);
  }
  IntWrap(const IntWrap &other)
  {
    m_int= new int(other.getval());
  }
  ~IntWrap()
  {
    delete m_int;
  }
  int getval() const { return *m_int; }
private:
  int *m_int;
};

/*
  To verify that there are no leaks, do:
  valgrind ./prealloced_array-t --gtest_filter="-*DeathTest*"
*/
TEST_F(PreallocedArrayTest, NoMemLeaksPushing)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  for (int ix= 0; ix < 42; ++ix)
    EXPECT_EQ(ix, array[ix].getval());
}

TEST_F(PreallocedArrayTest, NoMemLeaksPopping)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  while (!array.empty())
    array.pop_back();
}

TEST_F(PreallocedArrayTest, NoMemLeaksErasing)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  for (int ix= 0; !array.empty(); ++ix)
  {
    EXPECT_EQ(ix, array[0].getval());
    array.erase(0);
  }
}

TEST_F(PreallocedArrayTest, NoMemLeaksClearing)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  array.clear();
  EXPECT_EQ(0U, array.size());
}

/*
  A simple class to verify that Prealloced_array also works for
  classes which have their own operator new/delete.
 */
class TestAlloc : public Sql_alloc
{
public:
  explicit TestAlloc(int val)
    : m_int(val)
  {}

  int getval() const { return m_int; }
private:
  int m_int;
};


/*
  There is no THD and no mem-root available for the execution of this test.
  This shows that the memory management of Prealloced_array works OK for
  classes inheriting from Sql_alloc.
 */
TEST_F(PreallocedArrayTest, SqlAlloc)
{
  Prealloced_array<TestAlloc, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(TestAlloc(ix));
  for (int ix= 0; ix < 42; ++ix)
    EXPECT_EQ(ix, array[ix].getval());
}

}

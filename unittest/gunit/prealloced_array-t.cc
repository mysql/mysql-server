/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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
  size_t ix= 0;
  EXPECT_DEATH_IF_SUPPORTED(int_10.erase(ix),
                            ".*Assertion .*ix < size.*");
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
  int_10.erase(int_10.begin());
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

TEST_F(PreallocedArrayTest, ResizeSame)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(16U, int_10.size());
  int_10.resize(16U);
  EXPECT_EQ(16U, int_10.size());
}

TEST_F(PreallocedArrayTest, ResizeGrow)
{
  int_10.push_back(1);
  int_10.resize(20);
  EXPECT_EQ(1, int_10[0]);
  EXPECT_EQ(0, int_10[1]);
  EXPECT_EQ(20U, int_10.size());
  EXPECT_GE(int_10.capacity(), 20U);
}

TEST_F(PreallocedArrayTest, ResizeGrowVal)
{
  int_10.resize(20, 42);
  EXPECT_EQ(42, int_10[0]);
  EXPECT_EQ(42, int_10[19]);
  EXPECT_EQ(20U, int_10.size());
  EXPECT_GE(int_10.capacity(), 20U);
}

TEST_F(PreallocedArrayTest, ResizeShrink)
{
  for (int ix= 0; ix <= 15; ++ix)
    int_10.push_back(ix);
  EXPECT_EQ(16U, int_10.size());
  int_10.resize(10);
  EXPECT_EQ(10U, int_10.size());
}

TEST_F(PreallocedArrayTest, InsertUnique)
{
  for (int ix= 0; ix < 10; ++ix)
  {
    int_10.push_back(ix);
    int_10.push_back(ix);
  }
  std::random_shuffle(int_10.begin(), int_10.end());
  Prealloced_array<int, 1> unique_arr(PSI_NOT_INSTRUMENTED);
  for (int *pi= int_10.begin(); pi != int_10.end(); ++pi)
  {    
    unique_arr.insert_unique(*pi);
    EXPECT_EQ(1U, unique_arr.count_unique(*pi));
  }
  EXPECT_EQ(10U, unique_arr.size());
  // Duplicates should have been ignored, and the result should be sorted.
  for (int ix= 0; ix < static_cast<int>(unique_arr.size()); ++ix)
  {
    EXPECT_EQ(ix, unique_arr[ix]);
  }
}

TEST_F(PreallocedArrayTest, EraseUnique)
{
  for (int ix= 0; ix < 20; ++ix)
    int_10.push_back(ix);

  // The array should be sorted by default.
  for (int ix= 0; ix < 20; ++ix)
    EXPECT_EQ(ix, int_10[ix]);

  // Now remove all even numbers.
  for (int ix= 0; ix < 10; ++ix)
    EXPECT_EQ(1U, int_10.erase_unique(2 * ix));

  // 10 numbers should remain.
  EXPECT_EQ(10U, int_10.size());

  // Removing non-existing numbers should return 0.
  for (int ix= 0; ix < 10; ++ix)
  {
    EXPECT_EQ(0U, int_10.count_unique(2 * ix));
    EXPECT_EQ(0U, int_10.erase_unique(2 * ix));
  }

  // 10 numbers should still remain.
  EXPECT_EQ(10U, int_10.size());

  // The array should still be sorted and contain odd numbers.
  for (int ix= 0; ix < 10; ++ix)
    EXPECT_EQ(2 * ix + 1, int_10[ix]);
}

/*
  A simple class for testing that object copying and destruction is done
  properly when we have to expand the array a few times,
  and has_trivial_destructor == false.
 */
class IntWrap
{
public:
  IntWrap()
  {
    m_int= new int(0);
  }
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
  IntWrap &operator=(const IntWrap &rhs)
  {
    *m_int= rhs.getval();
    return *this;
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
    array.erase(array.begin());
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

TEST_F(PreallocedArrayTest, NoMemLeaksResizing)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  array.resize(0);
  EXPECT_EQ(0U, array.size());
}

TEST_F(PreallocedArrayTest, NoMemLeaksAssigning)
{
  Prealloced_array<IntWrap, 1, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  Prealloced_array<IntWrap, 1, false> array2(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 10; ++ix)
    array2.push_back(IntWrap(ix + 100));
  array2= array1;
  EXPECT_EQ(array1.size(), array2.size());
  for (size_t ix= 0; ix < array1.size(); ++ix)
    EXPECT_EQ(array1[ix].getval(), array2[ix].getval());
}

TEST_F(PreallocedArrayTest, NoMemLeaksEraseAll)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  array.erase(array.begin(), array.end());
  EXPECT_EQ(0U, array.size());
}

TEST_F(PreallocedArrayTest, NoMemLeaksEraseMiddle)
{
  Prealloced_array<IntWrap, 1, false> array(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array.push_back(IntWrap(ix));
  array.erase(array.begin() + 1, array.end() - 1);
  EXPECT_EQ(2U, array.size());
  EXPECT_EQ(0, array[0].getval());
  EXPECT_EQ(41, array[1].getval());
}

TEST_F(PreallocedArrayTest, NoMemLeaksEraseSwap)
{
  Prealloced_array<IntWrap, 1, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  Prealloced_array<IntWrap, 1, false> array2(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 10; ++ix)
    array2.push_back(IntWrap(ix + 100));
  array1.swap(array2);
  EXPECT_EQ(10U, array1.size());
  EXPECT_EQ(42U, array2.size());
  Prealloced_array<IntWrap, 1, false>(PSI_NOT_INSTRUMENTED).swap(array1);
  EXPECT_EQ(0U, array1.size());
}

TEST_F(PreallocedArrayTest, NoMemLeaksMySwap)
{
  Prealloced_array<IntWrap, 2, false> array1(PSI_NOT_INSTRUMENTED);
  Prealloced_array<IntWrap, 2, false> array2(PSI_NOT_INSTRUMENTED);
  array1.push_back(IntWrap(1));
  array2.push_back(IntWrap(2));
  array2.push_back(IntWrap(22));
  array1.swap(array2);
  EXPECT_EQ(2U, array1.size());
  EXPECT_EQ(1U, array2.size());
  EXPECT_EQ(2,  array1[0].getval());
  EXPECT_EQ(22, array1[1].getval());
  EXPECT_EQ(1,  array2[0].getval());
}

TEST_F(PreallocedArrayTest, NoMemLeaksStdSwap)
{
  Prealloced_array<IntWrap, 1, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  Prealloced_array<IntWrap, 1, false>
    array2(PSI_NOT_INSTRUMENTED, array1.begin(), array1.begin() + 10);
  EXPECT_EQ(10U, array2.size());
  IntWrap *p1= array1.begin();
  IntWrap *p2= array2.begin();
  array1.swap(array2);
  EXPECT_EQ(10U, array1.size());
  EXPECT_EQ(42U, array2.size());
  // We expect a buffer swap here.
  EXPECT_EQ(p1, array2.begin());
  EXPECT_EQ(p2, array1.begin());
}

TEST_F(PreallocedArrayTest, NoMemLeaksShrinkToFitMalloc)
{
  Prealloced_array<IntWrap, 1, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  IntWrap *p1= array1.begin();
  array1.shrink_to_fit();
  EXPECT_EQ(42U, array1.size());
  EXPECT_EQ(42U, array1.capacity());
  EXPECT_NE(p1, array1.begin());
}

TEST_F(PreallocedArrayTest, NoMemLeaksShrinkToFitSameSize)
{
  Prealloced_array<IntWrap, 10, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  for (int ix= 0; array1.size() != array1.capacity(); ++ix)
    array1.push_back(IntWrap(ix));
  IntWrap *p1= array1.begin();
  array1.shrink_to_fit();
  EXPECT_EQ(p1, array1.begin());
}

TEST_F(PreallocedArrayTest, NoMemLeaksShrinkToFitPrealloc)
{
  Prealloced_array<IntWrap, 100, false> array1(PSI_NOT_INSTRUMENTED);
  for (int ix= 0; ix < 42; ++ix)
    array1.push_back(IntWrap(ix));
  IntWrap *p1= array1.begin();
  array1.shrink_to_fit();
  EXPECT_EQ(42U, array1.size());
  EXPECT_EQ(100U, array1.capacity());
  EXPECT_EQ(p1, array1.begin());
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

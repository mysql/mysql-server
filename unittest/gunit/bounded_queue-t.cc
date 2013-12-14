/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved. 

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
#include <algorithm>
#include <stddef.h>

#include "bounded_queue.h"
#include "filesort_utils.h"
#include "my_sys.h"

namespace bounded_queue_unittest {

const int num_elements= 14;

// A simple helper function to determine array size.
template <class T, int size>
int array_size(const T (&)[size])
{
  return size;
}


/*
  Elements to be sorted by tests below.
  We put some data in front of 'val' to verify (when debugging)
  that all the reinterpret_casts involved when using QUEUE are correct.
*/
struct Test_element
{
  Test_element()      { *this= -1; }
  Test_element(int i) { *this= i; }

  Test_element &operator=(int i)
  {
    val= i;
    snprintf(text, array_size(text), "%4d", i);
    return *this;
  }

  char text[8]; // Some data.
  int  val;     // The value we use for generating the key.
};


/*
  The key, which is actually sorted by queue_xxx() functions.
  We sort on the key only.
 */
struct Test_key
{
  Test_key() : element(NULL), key(-1) {}

  Test_element *element; // The actual data element.
  int           key;     // The generated key for the data element.
};


/*
  Comparison function for Test_key objects.
 */
int test_key_compare(size_t *cmp_arg, Test_key **a, Test_key **b)
{
  EXPECT_EQ(*cmp_arg, sizeof(int));

  int a_num= (*a)->key;
  int b_num= (*b)->key;

  if (a_num > b_num)
    return +1;
  if (a_num < b_num)
    return -1;
  return 0;
}


/*
  Generates a Test_key for a given Test_element.
 */
void test_keymaker(Sort_param *sp, Test_key *key, Test_element *element)
{
  key->element= element;
  key->key= element->val;
}


/*
  A struct to wrap the actual keys, and an array of pointers to the keys.
 */
template<int sz, typename Key_type>
struct Key_container
{
  Key_container()
  {
    for (int ix= 0; ix <= sz; ++ix)
      key_ptrs[ix]= &key_data[ix];
  }

  Key_type *key_ptrs[sz+1];
  Key_type  key_data[sz+1];
};


class BoundedQueueTest : public ::testing::Test
{
protected:
  BoundedQueueTest() : m_key_size(sizeof(int))
  {
  }

  virtual void SetUp()
  {
    int ix;
    for (ix=0; ix < array_size(m_test_data); ++ix)
      m_test_data[ix]= ix;
    std::random_shuffle(&m_test_data[0], &m_test_data[array_size(m_test_data)]);
  }

  void insert_test_data()
  {
    for (int ix= 0; ix < array_size(m_test_data); ++ix)
      m_queue.push(&m_test_data[ix]);
  }

  // Key pointers and data, used by the queue_xxx() functions.
  Key_container<num_elements / 2, Test_key> m_keys;

  // Some random intput data, to be sorted.
  Test_element  m_test_data[num_elements];

  size_t m_key_size;
  Bounded_queue<Test_element, Test_key> m_queue;
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(BoundedQueueTest);
};


// Google Test recommends DeathTest suffix for classes used in death tests.
typedef BoundedQueueTest BoundedQueueDeathTest;

#if !defined(DBUG_OFF)
/*
  Verifies that we DBUG_ASSERT if trying to push to an un-initialized queue.
 */
TEST_F(BoundedQueueDeathTest, DieIfNotInitialized)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  Test_element foo= 1;
  EXPECT_DEATH_IF_SUPPORTED(m_queue.push(&foo),
                            ".*Assertion .*is_initialized.*");
}

/*
  Verifies that popping an empty queue hits a DBUG_ASSERT.
 */
TEST_F(BoundedQueueDeathTest, DieIfPoppingEmptyQueue)
{
  EXPECT_EQ(0, m_queue.init(0, true, test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(m_queue.pop(),
                            ".*Assertion .*elements > 0.*");
}
#endif  // !defined(DBUG_OFF)


/*
  Verifies that construct, initialize, destroy works.
 */
TEST_F(BoundedQueueTest, ConstructAndDestruct)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, true,
                            test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
}


/*
  Verifies that we reject too large queues.
 */
TEST_F(BoundedQueueTest, TooManyElements)
{
  EXPECT_EQ(1, m_queue.init(UINT_MAX, true,
                            test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  EXPECT_EQ(1, m_queue.init(UINT_MAX - 1, true,
                            test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
}


/*
  Verifies that zero-size queue works.
 */
TEST_F(BoundedQueueTest, ZeroSizeQueue)
{
  EXPECT_EQ(0, m_queue.init(0, true, test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  insert_test_data();
  EXPECT_EQ(1U, m_queue.num_elements());
}


/*
  Verifies that push and bounded size works, and that pop() gives sorted order.
 */
TEST_F(BoundedQueueTest, PushAndPopKeepLargest)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, false, test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  insert_test_data();
  // We expect the queue to contain [7 .. 13]
  const int max_key_val= array_size(m_test_data) - 1;
  while (m_queue.num_elements() > 0)
  {
    Test_key **top= m_queue.pop();
    int expected_key_val= max_key_val - m_queue.num_elements();
    int key_val= (*top)->key;
    EXPECT_EQ(expected_key_val, key_val);
    Test_element *element= (*top)->element;
    EXPECT_EQ(expected_key_val, element->val);
  }
}


/*
  Verifies that push and bounded size works, and that pop() gives sorted order.
  Note that with max_at_top == true, we will pop() in reverse order.
 */
TEST_F(BoundedQueueTest, PushAndPopKeepSmallest)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, true, test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  insert_test_data();
  // We expect the queue to contain [6 .. 0]
  while (m_queue.num_elements() > 0)
  {
    Test_key **top= m_queue.pop();
    int expected_key_val= m_queue.num_elements();
    int key_val= (*top)->key;
    EXPECT_EQ(expected_key_val, key_val);
    Test_element *element= (*top)->element;
    EXPECT_EQ(expected_key_val, element->val);
  }
}


/*
  Verifies that push, with bounded size, followed by sort() works.
 */
TEST_F(BoundedQueueTest, InsertAndSort)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, true, test_key_compare,
                            m_key_size,
                            &test_keymaker, NULL, m_keys.key_ptrs));
  insert_test_data();
  uchar *base=  (uchar*) &m_keys.key_ptrs[0];
  size_t size=  sizeof(Test_key);
  // We sort our keys as strings, so erase all the element pointers first.
  for (int ii= 0; ii < array_size(m_keys.key_data); ++ii)
    m_keys.key_data[ii].element= NULL;

  my_string_ptr_sort(base, array_size(m_keys.key_ptrs), size);
  for (int ii= 0; ii < num_elements/2; ++ii)
  {
    Test_key *sorted_key= m_keys.key_ptrs[ii];
    EXPECT_EQ(ii, sorted_key->key);
  }
}


/*
  A test of the function get_merge_many_buffs_cost_fast()
 */
TEST(CostEstimationTest, MergeManyBuff)
{
  ha_rows num_rows= 512;
  ulong num_keys= 100;
  ulong row_lenght= 100;
  double prev_cost= 0.0;
  while (num_rows <= MAX_FILE_SIZE/4)
  {
    double merge_cost=
      get_merge_many_buffs_cost_fast(num_rows, num_keys, row_lenght);
    EXPECT_LT(0.0, merge_cost);
    EXPECT_LT(prev_cost, merge_cost);
    num_rows*= 2;
    prev_cost= merge_cost;
  }
}


/*
  Comparison function for integers.
 */
int int_ptr_compare(size_t *cmp_arg, int **a, int **b)
{
  EXPECT_EQ(*cmp_arg, sizeof(int));

  int a_num= **a;
  int b_num= **b;

  if (a_num > b_num)
    return +1;
  if (a_num < b_num)
    return -1;
  return 0;
}


/*
  Generates an integer key for a given integer element.
 */
void int_keymaker(Sort_param *sp, int *to, int *from)
{
  memcpy(to, from, sizeof(int));
}


/*
  Some basic performance testing, to compute the overhead of Bounded_queue.
  Run the with 'bounded_queue-t --disable-tap-output' to see the
  millisecond output from Google Test.
 */
const int num_rows= 10000;
const int row_limit= 100;
const int num_iterations= 10;

class PerfTestSmall : public ::testing::Test
{
public:
  /*
    The extra overhead of malloc/free should be part of the measurement,
    so we do not define the key container as a member here.
  */
  typedef Key_container<row_limit, int> Container;
  enum { limit= row_limit };
};

class PerfTestLarge : public ::testing::Test
{
public:
  /*
    The extra overhead of malloc/free should be part of the measurement,
    so we do not define the key container as a member here.
  */
  typedef Key_container<num_rows, int> Container;
  enum { limit= num_rows };
};


template <int limit>
void insert_and_sort()
{
  typedef Key_container<limit, int> Container;
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    Bounded_queue<int, int> queue;
    EXPECT_EQ(0, queue.init(limit, true, int_ptr_compare,
                            sizeof(int), &int_keymaker, NULL, keys->key_ptrs));
    for (int ix= 0; ix < num_rows; ++ix)
    {
      int data= rand();
      queue.push(&data);
    }
    my_string_ptr_sort((uchar*) &keys->key_ptrs[0],
                       queue.num_elements(), sizeof(int));
    delete keys;
  }
}


/*
  Test with Bounded_queue size == <limit>.
 */
TEST_F(PerfTestSmall, InsertAndSort)
{
  insert_and_sort<limit>();
}


/*
  Test with Bounded_queue size == <number of rows>
 */
TEST_F(PerfTestLarge, InsertAndSort)
{
  insert_and_sort<limit>();
}


/*
  Test without bounded queue, i.e. insert keys into array, and sort it.
 */
TEST_F(PerfTestLarge, WithoutQueue)
{
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    for (int ix= 0; ix < limit; ++ix)
    {
      int data= rand();
      keys->key_data[ix]= data;
    }
    my_string_ptr_sort((uchar*) &keys->key_ptrs[0], limit, sizeof(int));
    delete keys;
  }
}


/*
  Computes the overhead of setting up sort arrays, and rand() calls.
 */
TEST_F(PerfTestLarge, NoSorting)
{
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    for (int ix= 0; ix < limit; ++ix)
    {
      int data= rand();
      keys->key_data[ix]= data;
    }
    delete keys;
  }
}

}  // namespace

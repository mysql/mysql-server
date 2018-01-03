/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <sys/types.h>
#include <algorithm>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "sql/bounded_queue.h"
#include "sql/filesort_utils.h"
#include "sql/opt_costmodel.h"
#include "unittest/gunit/bounded_queue_boost.cc"
#include "unittest/gunit/bounded_queue_boost.h"
#include "unittest/gunit/bounded_queue_c.h"
#include "unittest/gunit/bounded_queue_std.h"
#include "unittest/gunit/fake_costmodel.h"
#include "unittest/gunit/test_utils.h"

namespace bounded_queue_unittest {

const int num_elements= 14;

static int count_int_ptr_compare= 0;
static int count_operator= 0;
static int count_test_key= 0;

/*
  Elements to be sorted by tests below.
  We put some data in front of 'val' to verify (when debugging)
  that all the reinterpret_casts involved when using QUEUE are correct.
*/
struct Test_element
{
  Test_element()      { *this= -1; }
  Test_element(int i) { *this= i; }

  // To silence some narrowing warnings on MSVC.
  Test_element &operator=(size_t i)
  {
    return *this= static_cast<int>(i);
  }

  Test_element &operator=(int i)
  {
    val= static_cast<int>(i);
    my_snprintf(text, array_elements(text), "%4d", i);
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


class Test_key_compare
{
public:
  bool operator()(const Test_key *a, const Test_key *b) const
  {
    ++count_test_key;
    return a->key < b->key;
  }
  size_t m_size;
};


/*
  Generates a Test_key for a given Test_element.
 */
class Test_keymaker
{
public:
  uint make_sortkey(Test_key *key, Test_element *element)
  {
    key->element= element;
    key->key= element->val;
    return sizeof(key->key);
  }
  size_t max_compare_length() const { return sizeof(int); }
};


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
  BoundedQueueTest() {}

  virtual void SetUp()
  {
    for (size_t ix=0; ix < array_elements(m_test_data); ++ix)
      m_test_data[ix]= ix;
    std::random_shuffle(&m_test_data[0], &m_test_data[array_elements(m_test_data)]);
  }

  void insert_test_data()
  {
    for (size_t ix= 0; ix < array_elements(m_test_data); ++ix)
      m_queue.push(&m_test_data[ix]);
  }

  void insert_test_data_heap()
  {
    for (size_t ix= 0; ix < array_elements(m_test_data); ++ix)
    {
      m_heap.push(&m_test_data[ix]);
    }
  }

  // Key pointers and data, used by the queue_xxx() functions.
  Key_container<num_elements / 2, Test_key> m_keys;

  // Some random intput data, to be sorted.
  Test_element  m_test_data[num_elements];

  Test_keymaker m_keymaker;
  Bounded_QUEUE<Test_element *, Test_key *, Test_keymaker> m_queue;
  Bounded_queue<Test_element *, Test_key *,
                Test_keymaker, Test_key_compare> m_heap;
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
                            &m_keymaker, m_keys.key_ptrs));
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
                            &m_keymaker, m_keys.key_ptrs));
}


/*
  Verifies that we reject too large queues.
 */
TEST_F(BoundedQueueTest, TooManyElements)
{
  EXPECT_EQ(1, m_queue.init(UINT_MAX, true,
                            test_key_compare,
                            &m_keymaker, m_keys.key_ptrs));
  EXPECT_EQ(1, m_queue.init(UINT_MAX - 1, true,
                            test_key_compare,
                            &m_keymaker, m_keys.key_ptrs));
}


/*
  Verifies that zero-size queue works.
 */
TEST_F(BoundedQueueTest, ZeroSizeQueue)
{
  EXPECT_EQ(0, m_queue.init(0, true, test_key_compare,
                            &m_keymaker, m_keys.key_ptrs));
  insert_test_data();
  // There is always one extra element in the queue.
  EXPECT_EQ(1U, m_queue.num_elements());
}


/*
  Verifies that push and bounded size works, and that pop() gives sorted order.
 */
TEST_F(BoundedQueueTest, PushAndPopKeepLargest)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, false, test_key_compare,
                            &m_keymaker, m_keys.key_ptrs));
  insert_test_data();
  // We expect the queue to contain [7 .. 13]
  const int max_key_val= array_elements(m_test_data) - 1;
  EXPECT_EQ(static_cast<uint>(num_elements / 2 + 1), m_queue.num_elements());
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
                            &m_keymaker, m_keys.key_ptrs));
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


static void my_string_ptr_sort(Test_key **base, uint items, size_t size)
{
  if (size && items)
  {
    std::sort(base, base + items,
      [](const Test_key *a, const Test_key *b)
      {
        return memcmp(&a->key, &b->key, sizeof(a->key)) < 0;
      });
  }
}


/*
  Verifies that push, with bounded size, followed by sort() works.
 */
TEST_F(BoundedQueueTest, InsertAndSort)
{
  EXPECT_EQ(0, m_queue.init(num_elements/2, true, test_key_compare,
                            &m_keymaker, m_keys.key_ptrs));
  insert_test_data();
  Test_key **base=  &m_keys.key_ptrs[0];
  size_t size=  sizeof(Test_key);
  // We sort our keys as strings, so erase all the element pointers first.
  for (size_t ii= 0; ii < array_elements(m_keys.key_data); ++ii)
    m_keys.key_data[ii].element= NULL;

  my_string_ptr_sort(base, array_elements(m_keys.key_ptrs), size);
  for (size_t ii= 0; ii < num_elements/2; ++ii)
  {
    Test_key *sorted_key= m_keys.key_ptrs[ii];
    EXPECT_EQ(static_cast<int>(ii), sorted_key->key);
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

  // Make a cost model object that the merge code will use
  Fake_Cost_model_table cost_model_table;

  while (num_rows <= MAX_FILE_SIZE/4)
  {
    const double merge_cost=
      get_merge_many_buffs_cost_fast(num_rows, num_keys, row_lenght,
                                     &cost_model_table);
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

  ++count_int_ptr_compare;

  int a_num= **a;
  int b_num= **b;

  if (a_num > b_num)
    return +1;
  if (a_num < b_num)
    return -1;
  return 0;
}


class Int_keymaker;
class Int_ptr_compare
{
public:
  bool operator()(const int *a, const int *b) const
  {
    ++count_operator;
    return *a > *b;
  }
  size_t m_compare_length;
  Int_keymaker *m_param;
};


/*
  Generates an integer key for a given integer element.
 */
class Int_keymaker
{
public:
  uint make_sortkey(int *to, int *from)
  {
    memcpy(to, from, sizeof(int));
    return sizeof(int);
  }
  size_t max_compare_length() const { return sizeof(int); }
  bool using_varlen_keys() const { return false; }
};


/*
  Some basic performance testing, to compute the overhead of Bounded_QUEUE.
  Run the with 'bounded_queue-t --disable-tap-output' to see the
  millisecond output from Google Test.

  ./bounded_queue-t --disable-tap-output --gtest_filter="PerfTest*"

  For testing, I used
  num_rows  = 100000
  row_limit = { 1, 10, 100, 1000, 10000 }
  num_iterations = 100

  The inser_into_xxx() functions insert rand() data.
  Also test with
    identical data : queue.push(0)
    increasing keys: queue.push(ix)
    decreasing keys: queue.push(-ix)
 */
const ha_rows num_rows= 100000;
const ha_rows row_limit= 1;
const int num_iterations= 1;

inline int test_data(int ix MY_ATTRIBUTE((unused)))
{
  return rand();
  // return 42;
  // return ix;
  // return -ix;
}


class PerfTest : public ::testing::Test
{
public:
  virtual void SetUp()
  {
    count_int_ptr_compare= 0;
    count_operator= 0;
    count_test_key= 0;
  }
  virtual void TearDown()
  {
    std::cout << "C-compare " << count_int_ptr_compare
              << " Cpp-compare " << count_operator << std::endl << std::endl;
  }
  /*
    The extra overhead of malloc/free should be part of the measurement,
    so we do not define the key container as a member here.
  */
  typedef Key_container<row_limit, int> Container;
};


// Different queue.init, so separate insert function, see insert_into_heap.
void insert_into_queue()
{
  typedef Key_container<row_limit, int> Container;
  std::cout << "insert " << num_rows
            << " rows into queue size " << row_limit << std::endl;
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    Int_keymaker int_keymaker;
    Bounded_QUEUE<int *, int *, Int_keymaker> queue;
    EXPECT_EQ(0, queue.init(row_limit, false, int_ptr_compare,
                            &int_keymaker, keys->key_ptrs));
    for (ha_rows ix= 0; ix < num_rows; ++ix)
    {
      int data= test_data(ix);
      queue.push(&data);
    }
    delete keys;
  }
}


// Inserts test data into Queue.
template<typename Queue_t>
void insert_into_heap()
{
  typedef Key_container<row_limit, int> Container;
  std::cout << "insert " << num_rows
            << " rows into heap size " << row_limit << std::endl;
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    Int_keymaker int_keymaker;
    Queue_t queue;
    EXPECT_EQ(0, queue.init(row_limit,
                            &int_keymaker, keys->key_ptrs));
    for (ha_rows ix= 0; ix < num_rows; ++ix)
    {
      int data= test_data(ix);
      queue.push(&data);
    }
    delete keys;
  }
}


TEST_F(PerfTest, InsertIntoQUEUE)
{
  insert_into_queue();
}

TEST_F(PerfTest, InsertIntoPriorityQueue)
{
  insert_into_heap<Bounded_queue<int *, int *,
                                 Int_keymaker, Int_ptr_compare> >();
}

TEST_F(PerfTest, InsertIntoStdQueue)
{
  insert_into_heap<Bounded_queue_std<int *, int *,
                                     Int_keymaker, Int_ptr_compare> >();
}

TEST_F(PerfTest, InsertIntoBoostQueue)
{
  insert_into_heap<Bounded_queue_boost<int *, int *,
                                       Int_keymaker, Int_ptr_compare> >();
}


/*
  Computes the overhead of setting up sort arrays, and rand() calls.
 */
TEST_F(PerfTest, NoSorting)
{
  for (int it= 0; it < num_iterations; ++it)
  {
    Container *keys= new Container;
    srand(0);
    for (ha_rows ix= 0; ix < row_limit; ++ix)
    {
      int data= rand();
      keys->key_data[ix]= data;
    }
    delete keys;
  }
}

}  // namespace

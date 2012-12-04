/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "sql_select.h"
#include "mem_root_array.h"

/**
   WL#5774 Decrease number of malloc's for normal DML queries.
   One of the malloc's was due to DYNAMIC_ARRAY keyuse;
   We replace the DYNAMIC_ARRAY with a std::vector-like class Mem_root_array.

   Below are unit tests for comparing performance, and for testing
   functionality of Mem_root_array.
*/


/*
  Rewrite of sort_keyuse() to comparison operator for use by std::less<>
  It is a template argument, so static rather than in unnamed namespace.
*/
static inline bool operator<(const Key_use &a, const Key_use &b)
{
  if (a.table->tablenr != b.table->tablenr)
    return a.table->tablenr < b.table->tablenr;
  if (a.key != b.key)
    return a.key < b.key;
  if (a.keypart != b.keypart)
    return a.keypart < b.keypart;
  const bool atab = test((a.used_tables & ~OUTER_REF_TABLE_BIT));
  const bool btab = test((b.used_tables & ~OUTER_REF_TABLE_BIT));
  if (atab != btab)
    return atab < btab;
  return
    ((a.optimize & KEY_OPTIMIZE_REF_OR_NULL) <
     (b.optimize & KEY_OPTIMIZE_REF_OR_NULL));
}


/*
  Compare for equality.
  It is a template argument, so static rather than in unnamed namespace.
*/
static inline bool operator==(const Key_use &lhs, const Key_use &rhs)
{
  return
    lhs.table->tablenr == rhs.table->tablenr &&
    lhs.key            == rhs.key            &&
    lhs.keypart        == rhs.keypart        &&
    test((lhs.used_tables & ~OUTER_REF_TABLE_BIT))
    ==
    test((rhs.used_tables & ~OUTER_REF_TABLE_BIT)) &&
    (lhs.optimize & KEY_OPTIMIZE_REF_OR_NULL)
    ==
    (rhs.optimize & KEY_OPTIMIZE_REF_OR_NULL);
}


static inline std::ostream &operator<<(std::ostream &s, const Key_use &v)
{
  return s << "{"
           << v.table->tablenr << ", "
           << v.key            << ", "
           << v.keypart        << ", "
           << v.used_tables    << ", "
           << v.optimize
           << "}"
    ;
}


namespace dynarray_unittest {

/*
  Cut'n paste this function from sql_select.cc,
  to avoid linking in the entire server for this unit test.
*/
inline int sort_keyuse(Key_use *a, Key_use *b)
{
  int res;
  if (a->table->tablenr != b->table->tablenr)
    return (int) (a->table->tablenr - b->table->tablenr);
  if (a->key != b->key)
    return (int) (a->key - b->key);
  if (a->keypart != b->keypart)
    return (int) (a->keypart - b->keypart);
  // Place const values before other ones
  if ((res= test((a->used_tables & ~OUTER_REF_TABLE_BIT)) -
       test((b->used_tables & ~OUTER_REF_TABLE_BIT))))
    return res;
  /* Place rows that are not 'OPTIMIZE_REF_OR_NULL' first */
  return (int) ((a->optimize & KEY_OPTIMIZE_REF_OR_NULL) -
		(b->optimize & KEY_OPTIMIZE_REF_OR_NULL));
}


// We generate some random data at startup, for testing of sorting.
void generate_test_data(Key_use *keys, TABLE *tables, int n)
{
  int ix;
  for (ix= 0; ix < n; ++ix)
  {
    tables[ix].tablenr= ix % 3;
    keys[ix]=
      Key_use(&tables[ix],
              NULL,                           // Item      *val
              0,                              // table_map  used_tables
              ix % 4,                         // uint       key
              ix % 2,                         // uint       keypart
              0,                              // uint       optimize
              0,                              //            keypart_map
              0,                              // ha_rows    ref_table_rows
              true,                           // bool       null_rejecting
              NULL,                           // bool      *cond_guard
              0                               // uint       sj_pred_no
              );
  }
  std::random_shuffle(&keys[0], &keys[n]);
}


// Play around with these constants to see std::sort speedup vs. my_qsort.
const int num_elements= 200;
const int num_iterations= 1000;

/*
  This class is used for comparing performance of
    std::vector<> and std::sort()
  vs
    DYNAMIC_ARRAY and my_qsort()
 */
class DynArrayTest : public ::testing::Test
{
public:
  DynArrayTest() {}

  static void SetUpTestCase()
  {
    generate_test_data(test_data, table_list, num_elements);
  }

  virtual void SetUp()
  {
    my_init_dynamic_array(&m_keyuse_dyn, sizeof(Key_use), num_elements, 64);
    m_keyuse_vec.reserve(num_elements);
  }

  void insert_and_sort_dynamic()
  {
    reset_dynamic(&m_keyuse_dyn);
    for (int ix= 0; ix < num_elements; ++ix)
    {
      insert_dynamic(&m_keyuse_dyn, &test_data[ix]);
    }
    my_qsort(m_keyuse_dyn.buffer, m_keyuse_dyn.elements, sizeof(Key_use),
             reinterpret_cast<qsort_cmp>(sort_keyuse));
  }

  void insert_and_sort_vector()
  {
    m_keyuse_vec.clear();
    for (int ix= 0; ix < num_elements; ++ix)
    {
      m_keyuse_vec.push_back(test_data[ix]);
    }
    std::sort(m_keyuse_vec.begin(), m_keyuse_vec.end(), std::less<Key_use>());
  }

  DYNAMIC_ARRAY           m_keyuse_dyn;
  std::vector<Key_use>    m_keyuse_vec;
private:
  static Key_use test_data[num_elements];
  static TABLE   table_list[num_elements];

  GTEST_DISALLOW_COPY_AND_ASSIGN_(DynArrayTest);
};

Key_use DynArrayTest::test_data[num_elements];
TABLE   DynArrayTest::table_list[num_elements];


// Test insert_dynamic() and my_qsort().
TEST_F(DynArrayTest, DynArray)
{
  for (int ix= 0; ix < num_iterations; ++ix)
    insert_and_sort_dynamic();
}


// Test vector::push_back() and std::sort()
TEST_F(DynArrayTest, Vector)
{
  for (int ix= 0; ix < num_iterations; ++ix)
    insert_and_sort_vector();
}


/*
  This class is for unit testing of Mem_root_array.
 */
class MemRootTest : public ::testing::Test
{
protected:
  MemRootTest()
    : m_mem_root_p(&m_mem_root),
      m_array_mysys(m_mem_root_p),
      m_array_std(m_mem_root_p)
  {}

  virtual void SetUp()
  {
    init_sql_alloc(&m_mem_root, 1024, 0);
    ASSERT_EQ(0, my_pthread_setspecific_ptr(THR_MALLOC, &m_mem_root_p));
    MEM_ROOT *root= *my_pthread_getspecific_ptr(MEM_ROOT**, THR_MALLOC);
    ASSERT_EQ(root, m_mem_root_p);

    m_array_mysys.reserve(num_elements);
    m_array_std.reserve(num_elements);
  }

  virtual void TearDown()
  {
    free_root(&m_mem_root, MYF(0));
  }

  static void SetUpTestCase()
  {
    generate_test_data(test_data, table_list, num_elements);
    ASSERT_EQ(0, pthread_key_create(&THR_THD, NULL));
    ASSERT_EQ(0, pthread_key_create(&THR_MALLOC, NULL));
  }

  static void TearDownTestCase()
  {
    pthread_key_delete(THR_THD);
    pthread_key_delete(THR_MALLOC);
  }

  void insert_and_sort_mysys()
  {
    m_array_mysys.clear();
    for (int ix= 0; ix < num_elements; ++ix)
    {
      m_array_mysys.push_back(test_data[ix]);
    }
    my_qsort(m_array_mysys.begin(), m_array_mysys.size(),
             m_array_mysys.element_size(),
             reinterpret_cast<qsort_cmp>(sort_keyuse));
  }

  void insert_and_sort_std()
  {
    m_array_std.clear();
    for (int ix= 0; ix < num_elements; ++ix)
    {
      m_array_std.push_back(test_data[ix]);
    }
    std::sort(m_array_std.begin(), m_array_std.end(), std::less<Key_use>());
  }

  MEM_ROOT m_mem_root;
  MEM_ROOT *m_mem_root_p;
  Key_use_array m_array_mysys;
  Key_use_array m_array_std;
private:
  static Key_use test_data[num_elements];
  static TABLE   table_list[num_elements];

  GTEST_DISALLOW_COPY_AND_ASSIGN_(MemRootTest);
};

Key_use MemRootTest::test_data[num_elements];
TABLE   MemRootTest::table_list[num_elements];


// Test Mem_root_array::push_back() and my_qsort()
TEST_F(MemRootTest, KeyUseMysys)
{
  for (int ix= 0; ix < num_iterations; ++ix)
    insert_and_sort_mysys();
}


// Test Mem_root_array::push_back() and std::sort()
TEST_F(MemRootTest, KeyUseStd)
{
  for (int ix= 0; ix < num_iterations; ++ix)
    insert_and_sort_std();
}


// Test that my_qsort() and std::sort() generate same order.
TEST_F(MemRootTest, KeyUseCompare)
{
  insert_and_sort_mysys();
  insert_and_sort_std();
  for (int ix= 0; ix < num_elements; ++ix)
  {
    Key_use k1= m_array_mysys.at(ix);
    Key_use k2= m_array_std.at(ix);
    EXPECT_EQ(k1, k2);
  }
}


// Test that Mem_root_array re-expanding works.
TEST_F(MemRootTest, Reserve)
{
  Mem_root_array<uint, true> intarr(m_mem_root_p);
  intarr.reserve(2);
  const uint num_pushes= 20;
  for (uint ix=0; ix < num_pushes; ++ix)
  {
    EXPECT_EQ(ix, intarr.size());
    EXPECT_FALSE(intarr.push_back(ix));
    EXPECT_EQ(ix, intarr.at(ix));
  }
  for (uint ix=0; ix < num_pushes; ++ix)
  {
    EXPECT_EQ(ix, intarr.at(ix));
  }
  EXPECT_EQ(sizeof(uint), intarr.element_size());
  EXPECT_EQ(num_pushes, intarr.size());
  EXPECT_LE(num_pushes, intarr.capacity());
}


class DestroyCounter
{
public:
  DestroyCounter(const DestroyCounter &rhs) : p_counter(rhs.p_counter) {}
  DestroyCounter(size_t *p) : p_counter(p) {}
  ~DestroyCounter() { (*p_counter)+= 1; }
private:
  size_t *p_counter;
};


// Test chop() and clear() and that destructors are executed.
TEST_F(MemRootTest, ChopAndClear)
{
  Mem_root_array<DestroyCounter, false> array(m_mem_root_p);
  const size_t nn= 4;
  array.reserve(nn);
  size_t counter= 0;
  DestroyCounter foo(&counter);
  for (size_t ix= 0; ix < array.capacity(); ++ix)
    array.push_back(foo);

  EXPECT_EQ(0U, counter);
  array.chop(nn / 2);
  EXPECT_EQ(nn / 2, counter);
  EXPECT_EQ(nn / 2, array.size());

  array.clear();
  EXPECT_EQ(nn, counter);
}


// Test that elements are destroyed if push_back() needs to call reserve().
TEST_F(MemRootTest, ReserveDestroy)
{
  Mem_root_array<DestroyCounter, false> array(m_mem_root_p);
  const size_t nn= 4;
  array.reserve(nn / 2);
  size_t counter= 0;
  DestroyCounter foo(&counter);
  for (size_t ix= 0; ix < nn; ++ix)
    array.push_back(foo);
  
  EXPECT_EQ(nn / 2, counter);
  EXPECT_EQ(nn, array.size());

  counter= 0;
  array.clear();
  EXPECT_EQ(nn, counter);
}


}

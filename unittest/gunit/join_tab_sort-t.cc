/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved. 

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

#include "test_utils.h"

#include "sql_select.h"
#include "merge_sort.h"

#include <vector>

namespace join_tab_sort_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class JTSortTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  Server_initializer initializer;
};


class MOCK_JOIN_TAB : public JOIN_TAB
{
public:
  MOCK_JOIN_TAB(uint recs, uint table_no) : JOIN_TAB()
  {
    found_records= recs;
    m_table.map= 1ULL << table_no;
    this->table= &m_table;
  }
  
  TABLE       m_table;
};

std::ostream &operator<<(std::ostream &s, const MOCK_JOIN_TAB &jt)
{
  return s << "{"
           << jt.found_records << ", "
           << jt.m_table.map
           << "}";
}


TEST_F(JTSortTest, SimpleSortTest)
{
  MOCK_JOIN_TAB jt1(UINT_MAX, 0);
  MOCK_JOIN_TAB jt2(2, 0);
  MOCK_JOIN_TAB jt3(1, 0);
  MOCK_JOIN_TAB jt4(10, 0);
  MOCK_JOIN_TAB jt5(5, 0);

  MOCK_JOIN_TAB *arr[5];
  arr[0]= &jt1;
  arr[1]= &jt2;
  arr[2]= &jt3;
  arr[3]= &jt4;
  arr[4]= &jt5;

  insert_sort(arr, arr+5, Join_tab_compare_default());

  EXPECT_EQ(1U, arr[0]->found_records);
  EXPECT_EQ(2U, arr[1]->found_records);
  EXPECT_EQ(5U, arr[2]->found_records);
  EXPECT_EQ(10U, arr[3]->found_records);
  EXPECT_EQ(UINT_MAX, arr[4]->found_records);

}


TEST_F(JTSortTest, SortFoundRecordsTest)
{
  const int num_tables= 50;
  MOCK_JOIN_TAB *arr[num_tables];

  for (int i= 0; i < num_tables; i++)
    arr[i]= new MOCK_JOIN_TAB(i, 0);

  // MERGE SORT
  std::random_shuffle(arr, arr + 50);
  merge_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records > arr[i-1]->found_records);

  // INSERT SORT
  std::random_shuffle(arr, arr + 50);
  insert_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records > arr[i-1]->found_records);

  for (int i= 0; i < num_tables; i++)
  {
    delete arr[i];
  }
}


TEST_F(JTSortTest, SortDependsTest)
{
  const int num_tables= 50;
  MOCK_JOIN_TAB *arr[num_tables];

  /*
    dependency has higher precedence than found_records, so the tables
    shall be ordered with decreasing number of records in this test
  */
  for (int i= 0; i < num_tables; i++)
  {
    arr[i]= new MOCK_JOIN_TAB(i, i);
    for (int j= i+1; j < num_tables; j++)
      arr[i]->dependent|= 1ULL << j;
  }

  // MERGE SORT
  std::random_shuffle(arr, arr + num_tables);
  merge_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records < arr[i-1]->found_records)
      << "i: " << *(arr[i]) << " "
      << "i-1: " << *(arr[i-1]);

  // INSERT SORT
  std::random_shuffle(arr, arr + num_tables);
  insert_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records < arr[i-1]->found_records);

  for (int i= 0; i < num_tables; i++)
  {
    delete arr[i];
  }
}


TEST_F(JTSortTest, SortKeyDependsTest)
{
  const int num_tables= 50;
  MOCK_JOIN_TAB *arr[num_tables];

  /*
    key_dependency has higher precedence than found_records, so the
    tables shall be ordered with decreasing number of records in this
    test
  */
  for (int i= 0; i < num_tables; i++)
  {
    arr[i]= new MOCK_JOIN_TAB(i, i);
    for (int j= i+1; j < num_tables; j++)
      arr[i]->key_dependent|= 1ULL << j;
  }

  // MERGE SORT
  std::random_shuffle(arr, arr + num_tables);
  merge_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records < arr[i-1]->found_records);

  // INSERT SORT
  std::random_shuffle(arr, arr + num_tables);
  insert_sort(arr, arr + num_tables, Join_tab_compare_default());
  for (int i= 1; i < num_tables; i++)
    EXPECT_TRUE(arr[i]->found_records < arr[i-1]->found_records);

  for (int i= 0; i < num_tables; i++)
    delete arr[i];
}

/*
  Above, sorting for JOIN_TABs were tested. Below we check that the
  sorting works for ints types as well. 
*/

class Int_compare_ptr :
  public std::binary_function<const int*, const int*, bool>
{
public:
  bool operator()(const int *i1, const int *i2) const
  {
    return *i1 < *i2;
  }
};


TEST_F(JTSortTest, SortIntTest)
{
  const uint ints_to_sort= 1000;

  std::vector<int> arr;
  std::vector<int*> arr_ptr;

  arr.reserve(ints_to_sort);
  arr_ptr.reserve(ints_to_sort);

  for (uint i= 0; i < ints_to_sort; i++)
  {
    arr.push_back(i);
    arr_ptr.push_back(&arr[i]);
  }

  EXPECT_TRUE(arr.size() == ints_to_sort);
  EXPECT_TRUE(arr_ptr.size() == ints_to_sort);

  // MERGE SORT
  std::random_shuffle(&arr_ptr.front(), &arr_ptr.back() + 1);
  merge_sort(&arr_ptr.front(), &arr_ptr.back() + 1, Int_compare_ptr());
  for (uint i= 0; i < arr_ptr.size(); i++)
    EXPECT_TRUE(*arr_ptr[i] == (int)i);

  // INSERT SORT
  std::random_shuffle(&arr_ptr.front(), &arr_ptr.back() + 1);
  insert_sort(&arr_ptr.front(), &arr_ptr.back() + 1, Int_compare_ptr());
  for (uint i= 0; i < arr_ptr.size(); i++)
    EXPECT_TRUE(*arr_ptr[i] == (int)i);
}


TEST_F(JTSortTest, SortInt2Test)
{
  const uint ints_to_sort= 1000;

  std::vector<int> arr;
  std::vector<int*> arr_ptr;

  arr.reserve(ints_to_sort);
  arr_ptr.reserve(ints_to_sort);

  for (uint i= 0; i < (ints_to_sort - 2); i++)
  {
    arr.push_back((i % 2) ? i : (i * -1));
    arr_ptr.push_back(&arr[i]);
  }

  arr.push_back(INT_MAX32);
  arr_ptr.push_back(&arr.back());

  arr.push_back(INT_MIN32);
  arr_ptr.push_back(&arr.back());

  EXPECT_TRUE(arr.size() == ints_to_sort);
  EXPECT_TRUE(arr_ptr.size() == ints_to_sort);

  // MERGE SORT
  std::random_shuffle(&arr_ptr.front(), &arr_ptr.back() + 1);
  merge_sort(&arr_ptr.front(), &arr_ptr.back() + 1, Int_compare_ptr());
  for (uint i= 1; i < arr_ptr.size(); i++)
    EXPECT_TRUE(*arr_ptr[i-1] < *arr_ptr[i]);

  // INSERT SORT
  std::random_shuffle(&arr_ptr.front(), &arr_ptr.back() + 1);
  insert_sort(&arr_ptr.front(), &arr_ptr.back() + 1, Int_compare_ptr());
  for (uint i= 1; i < arr_ptr.size(); i++)
    EXPECT_TRUE(*arr_ptr[i-1] < *arr_ptr[i]);
}

}

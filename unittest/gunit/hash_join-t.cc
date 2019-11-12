/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "extra/lz4/my_xxhash.h"
#include "include/my_murmur3.h"
#include "my_alloc.h"
#include "sql/hash_join_buffer.h"
#include "sql/hash_join_iterator.h"
#include "sql/item_cmpfunc.h"
#include "sql/row_iterator.h"
#include "sql/sql_executor.h"
#include "sql/sql_optimizer.h"
#include "sql_string.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/fake_integer_iterator.h"
#include "unittest/gunit/fake_string_iterator.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/temptable/mock_field_long.h"
#include "unittest/gunit/temptable/mock_field_varstring.h"
#include "unittest/gunit/test_utils.h"

namespace hash_join_unittest {

using std::vector;

static hash_join_buffer::TableCollection CreateTenTableJoin(MEM_ROOT *mem_root,
                                                            bool store_data) {
  constexpr int kNumColumns = 10;
  constexpr bool kColumnsNullable = true;
  constexpr int kNumTablesInJoin = 10;

  // Set up a ten-table join. For simplicity, allocate everything on a MEM_ROOT
  // that will take care of releasing allocated memory.
  vector<QEP_TAB *> qep_tabs;
  for (int i = 0; i < kNumTablesInJoin; ++i) {
    Fake_TABLE *fake_table =
        new (mem_root) Fake_TABLE(kNumColumns, kColumnsNullable);
    QEP_TAB *qep_tab = new (mem_root) QEP_TAB;
    qep_tab->set_qs(new (mem_root) QEP_shared);
    qep_tab->set_table(fake_table);
    qep_tab->table_ref = fake_table->pos_in_table_list;

    if (store_data) {
      bitmap_set_all(fake_table->write_set);

      for (uint j = 0; j < fake_table->s->fields; ++j) {
        fake_table->field[j]->store(1000, false /* is_unsigned */);
      }
    }

    qep_tabs.push_back(qep_tab);
  }

  return hash_join_buffer::TableCollection(qep_tabs);
}

static void BM_StoreFromTableBuffersNoData(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  MEM_ROOT mem_root;
  hash_join_buffer::TableCollection table_collection =
      CreateTenTableJoin(&mem_root, false);

  String buffer;
  buffer.reserve(1024);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(
        hash_join_buffer::StoreFromTableBuffers(table_collection, &buffer));
    ASSERT_GT(buffer.length(), 0);
  }
  StopBenchmarkTiming();

  initializer.TearDown();
}
BENCHMARK(BM_StoreFromTableBuffersNoData)

static void BM_StoreFromTableBuffersWithData(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  MEM_ROOT mem_root;
  hash_join_buffer::TableCollection table_collection =
      CreateTenTableJoin(&mem_root, true);

  String buffer;
  buffer.reserve(1024);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(
        hash_join_buffer::StoreFromTableBuffers(table_collection, &buffer));
    ASSERT_GT(buffer.length(), 0);
  }
  StopBenchmarkTiming();

  initializer.TearDown();
}
BENCHMARK(BM_StoreFromTableBuffersWithData)

// Return eight bytes of data.
static vector<uchar> GetShortData() { return {1, 2, 3, 4, 5, 6, 7, 8}; }

// Return 1024 bytes of data.
static vector<uchar> GetLongData() {
  constexpr int kDataSize = 1024;
  vector<uchar> data(kDataSize);
  for (int i = 0; i < kDataSize; ++i) {
    data.push_back(i);
  }
  return data;
}

static void BM_Murmur3ShortData(size_t num_iterations) {
  StopBenchmarkTiming();

  vector<uchar> data = GetShortData();
  StartBenchmarkTiming();

  size_t sum = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    sum += murmur3_32(&data[0], data.size(), 0);
  }
  StopBenchmarkTiming();

  // The sum variable is just to assure that the compiler doesn't optimize away
  // the entire for loop.
  EXPECT_NE(0, sum);
  SetBytesProcessed(num_iterations * data.size());
}
BENCHMARK(BM_Murmur3ShortData)

static void BM_MurmurLongData(size_t num_iterations) {
  StopBenchmarkTiming();

  vector<uchar> data = GetLongData();
  StartBenchmarkTiming();

  size_t sum = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    sum += murmur3_32(&data[0], data.size(), 0);
  }
  StopBenchmarkTiming();

  // The sum variable is just to assure that the compiler doesn't optimize away
  // the entire for loop.
  EXPECT_NE(0, sum);
  SetBytesProcessed(num_iterations * data.size());
}
BENCHMARK(BM_MurmurLongData)

static void BM_XXHash64ShortData(size_t num_iterations) {
  StopBenchmarkTiming();

  vector<uchar> data = GetShortData();
  StartBenchmarkTiming();

  size_t sum = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    sum += MY_XXH64(&data[0], data.size(), 0);
  }
  StopBenchmarkTiming();

  // The sum variable is just to assure that the compiler doesn't optimize away
  // the entire for loop.
  EXPECT_NE(0, sum);
  SetBytesProcessed(num_iterations * data.size());
}
BENCHMARK(BM_XXHash64ShortData)

static void BM_XXHash64LongData(size_t num_iterations) {
  StopBenchmarkTiming();

  vector<uchar> data = GetLongData();
  StartBenchmarkTiming();

  size_t sum = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    sum += MY_XXH64(&data[0], data.size(), 0);
  }
  StopBenchmarkTiming();

  // The sum variable is just to assure that the compiler doesn't optimize away
  // the entire for loop.
  EXPECT_NE(0, sum);
  SetBytesProcessed(num_iterations * data.size());
}
BENCHMARK(BM_XXHash64LongData)

// A class that takes care of setting up an environment for testing a hash join
// iterator. The constructors will set up two tables (left and right), as well
// as two (fake) iterators that reads data from these two tables. Both tables
// has only one column, and the join condition between the two tables is a
// simple equality between these two columns. There are two different
// constructors; one for integer data sets and one for string data sets.
//
// The user must provide the data contents for both tables in the constructor.
class HashJoinTestHelper {
 public:
  unique_ptr_destroy_only<RowIterator> left_iterator;
  unique_ptr_destroy_only<RowIterator> right_iterator;
  QEP_TAB *left_qep_tab;
  QEP_TAB *right_qep_tab;
  Item_func_eq *join_condition;

  HashJoinTestHelper(Server_initializer *initializer,
                     const vector<int> &left_dataset,
                     const vector<int> &right_dataset) {
    m_left_table_field.reset(
        new (&m_mem_root) Mock_field_long("column1", false /* is_nullable */));
    Fake_TABLE *left_table =
        new (&m_mem_root) Fake_TABLE(m_left_table_field.get());

    m_right_table_field.reset(
        new (&m_mem_root) Mock_field_long("column1", false /* is_nullable */));
    Fake_TABLE *right_table =
        new (&m_mem_root) Fake_TABLE(m_right_table_field.get());
    SetupFakeTables(initializer, left_table, right_table);

    left_iterator.reset(new (&m_mem_root) FakeIntegerIterator(
        initializer->thd(), left_table,
        down_cast<Field_long *>(left_table->field[0]), move(left_dataset)));
    right_iterator.reset(new (&m_mem_root) FakeIntegerIterator(
        initializer->thd(), right_table,
        down_cast<Field_long *>(right_table->field[0]), move(right_dataset)));
  }

  HashJoinTestHelper(Server_initializer *initializer,
                     const vector<std::string> &left_dataset,
                     const vector<std::string> &right_dataset) {
    m_left_table_field.reset(new (&m_mem_root) Mock_field_varstring(
        nullptr, "column1", 255 /* length */, false /* is_nullable */));
    Fake_TABLE *left_table =
        new (&m_mem_root) Fake_TABLE(m_left_table_field.get());

    m_right_table_field.reset(new (&m_mem_root) Mock_field_varstring(
        nullptr, "column1", 255 /* length */, false /* is_nullable */));
    Fake_TABLE *right_table =
        new (&m_mem_root) Fake_TABLE(m_right_table_field.get());
    SetupFakeTables(initializer, left_table, right_table);

    left_iterator.reset(new (&m_mem_root) FakeStringIterator(
        initializer->thd(), left_table,
        down_cast<Field_varstring *>(left_table->field[0]),
        move(left_dataset)));
    right_iterator.reset(new (&m_mem_root) FakeStringIterator(
        initializer->thd(), right_table,
        down_cast<Field_varstring *>(right_table->field[0]),
        move(right_dataset)));
  }

 private:
  void SetupFakeTables(Server_initializer *initializer, Fake_TABLE *left_table,
                       Fake_TABLE *right_table) {
    bitmap_set_all(left_table->write_set);
    bitmap_set_all(left_table->read_set);
    bitmap_set_all(right_table->write_set);
    bitmap_set_all(right_table->read_set);

    SELECT_LEX *select_lex =
        parse(initializer,
              "SELECT * FROM t1 JOIN t2 ON (t1.column1 = t2.column1);", 0);
    JOIN *join = new (&m_mem_root) JOIN(initializer->thd(), select_lex);

    left_qep_tab = new (&m_mem_root) QEP_TAB;
    left_qep_tab->set_qs(new (&m_mem_root) QEP_shared);
    left_qep_tab->set_table(left_table);
    left_qep_tab->table_ref = left_table->pos_in_table_list;
    left_qep_tab->set_join(join);

    right_qep_tab = new (&m_mem_root) QEP_TAB;
    right_qep_tab->set_qs(new (&m_mem_root) QEP_shared);
    right_qep_tab->set_table(right_table);
    right_qep_tab->table_ref = right_table->pos_in_table_list;
    right_qep_tab->set_join(join);

    join_condition = new Item_func_eq(new Item_field(left_table->field[0]),
                                      new Item_field(right_table->field[0]));
    join_condition->set_cmp_func();
  }

  // For simplicity, we allocate everything on a MEM_ROOT that takes care of
  // releasing any memory. However, we must ensure that the destructor is called
  // for Mock_field_varstring. Wrapping the fields in a unique_ptr_destroy_only
  // will ensure this.
  MEM_ROOT m_mem_root;
  unique_ptr_destroy_only<Field> m_left_table_field;
  unique_ptr_destroy_only<Field> m_right_table_field;
};

TEST(HashJoinTest, JoinIntOneToOneMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  vector<int> left_data;
  left_data.push_back(3);

  vector<int> right_data;
  right_data.push_back(3);

  HashJoinTestHelper test_helper(&initializer, left_data, right_data);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(3, test_helper.left_qep_tab->table()->field[0]->val_int());
  EXPECT_EQ(-1, hash_join_iterator.Read());

  initializer.TearDown();
}

TEST(HashJoinTest, JoinIntNoMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(&initializer, {2, 4}, {3, 5});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_EQ(-1, hash_join_iterator.Read());
  initializer.TearDown();
}

TEST(HashJoinTest, JoinIntOneToManyMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(&initializer, {2}, {2, 2});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  ASSERT_FALSE(hash_join_iterator.Init());

  // We expect two result rows before the iterator should return -1 (EOF).
  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(2, test_helper.left_qep_tab->table()->field[0]->val_int());

  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(2, test_helper.left_qep_tab->table()->field[0]->val_int());

  EXPECT_EQ(-1, hash_join_iterator.Read());
  initializer.TearDown();
}

TEST(HashJoinTest, JoinStringOneToOneMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(&initializer, {"abc"}, {"abc"});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_EQ(0, hash_join_iterator.Read());
  String buffer;
  String *result =
      test_helper.left_qep_tab->table()->field[0]->val_str(&buffer);
  EXPECT_EQ(std::string(result->ptr(), result->length()), std::string("abc"));

  EXPECT_EQ(-1, hash_join_iterator.Read());
  initializer.TearDown();
}

// Do a benchmark of HashJoinIterator::Init(). This function is responsible for
// building the hash table, and this step is also known as the "build phase".
//
// The table that the hash table is built from is a single-column table with
// 10000 uniformly distributed values between [0, 10000). We give the hash table
// enough memory so that it doesn't spill out to disk.
static void BM_HashTableIteratorBuild(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  const int min_value = 0;
  const int max_value = 10000;
  const int seed = 8834245;
  std::mt19937 generator(seed);
  std::uniform_int_distribution<> distribution(min_value, max_value);

  vector<int> left_dataset;
  vector<int> right_dataset;
  for (int i = 0; i < (max_value - min_value); ++i) {
    left_dataset.push_back(distribution(generator));
    right_dataset.push_back(distribution(generator));
  }

  HashJoinTestHelper test_helper(&initializer, left_dataset, right_dataset);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(hash_join_iterator.Init());
  }
  StopBenchmarkTiming();

  initializer.TearDown();
}
BENCHMARK(BM_HashTableIteratorBuild)

// Do a benchmark of HashJoinIterator::Read(). This function will read a row
// from the right table, and look for a matching row in the hash table. This is
// also known as the "probe phase".
//
// The table that the hash table is built from is a single-column table with
// 10000 uniformly distributed values between [0, 10000). We give the hash table
// enough memory so that is doesn't spill out to disk.
static void BM_HashTableIteratorProbe(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  const int min_value = 0;
  const int max_value = 10000;
  const int seed = 8834245;
  std::mt19937 generator(seed);
  std::uniform_int_distribution<> distribution(min_value, max_value);

  vector<int> left_dataset;
  vector<int> right_dataset;
  for (int i = 0; i < (max_value - min_value); ++i) {
    left_dataset.push_back(distribution(generator));
    right_dataset.push_back(distribution(generator));
  }
  HashJoinTestHelper test_helper(&initializer, left_dataset, right_dataset);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      {test_helper.left_qep_tab}, std::move(test_helper.right_iterator),
      test_helper.right_qep_tab, 10 * 1024 * 1024 /* 10 MB */,
      {test_helper.join_condition}, true);

  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(hash_join_iterator.Init());
    StartBenchmarkTiming();
    int result;
    do {
      result = hash_join_iterator.Read();
    } while (result == 0);
    StopBenchmarkTiming();
  }

  initializer.TearDown();
}
BENCHMARK(BM_HashTableIteratorProbe)

}  // namespace hash_join_unittest

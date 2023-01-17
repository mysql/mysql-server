/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>  // IWYU pragma: keep
#include <gtest/gtest.h>  // IWYU pragma: keep

#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_config.h"
#include "my_inttypes.h"
#include "my_murmur3.h"
#include "my_xxhash.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "prealloced_array.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/pack_rows.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/fake_integer_iterator.h"
#include "unittest/gunit/fake_string_iterator.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/mock_field_long.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/temptable/mock_field_varstring.h"
#include "unittest/gunit/test_utils.h"

class Query_block;

using pack_rows::TableCollection;
using std::nullopt;
using std::optional;
using std::pair;
using std::string;
using std::vector;
using testing::ElementsAre;

namespace hash_join_unittest {

static TableCollection CreateTenTableJoin(
    const my_testing::Server_initializer &initializer, MEM_ROOT *mem_root,
    bool store_data) {
  constexpr int kNumColumns = 10;
  constexpr bool kColumnsNullable = true;
  constexpr int kNumTablesInJoin = 10;
  Prealloced_array<TABLE *, 4> tables(PSI_NOT_INSTRUMENTED);

  // Set up a ten-table join. For simplicity, allocate everything on a MEM_ROOT
  // that will take care of releasing allocated memory.
  Query_block *query_block = parse(&initializer, "SELECT * FROM dummy", 0);
  JOIN join(initializer.thd(), query_block);
  join.qep_tab = mem_root->ArrayAlloc<QEP_TAB>(kNumTablesInJoin);
  join.tables = kNumTablesInJoin;
  for (int i = 0; i < kNumTablesInJoin; ++i) {
    Fake_TABLE *fake_table =
        new (mem_root) Fake_TABLE(kNumColumns, kColumnsNullable);
    fake_table->pos_in_table_list->set_tableno(i);
    QEP_TAB *qep_tab = &join.qep_tab[i];
    qep_tab->set_qs(new (mem_root) QEP_shared);
    qep_tab->set_table(fake_table);
    qep_tab->table_ref = fake_table->pos_in_table_list;

    if (store_data) {
      bitmap_set_all(fake_table->write_set);

      for (unsigned j = 0; j < fake_table->s->fields; ++j) {
        fake_table->field[j]->store(1000, /*is_unsigned=*/false);
      }
    }
    tables.push_back(fake_table);
  }

  return {tables,
          /*store_rowids=*/false,
          /*tables_to_get_rowid_for=*/0,
          /*tables_to_store_contents_of_null_rows_for=*/0};
}

static void DestroyFakeTables(const TableCollection &table_collection) {
  for (const pack_rows::Table &table : table_collection.tables())
    destroy(pointer_cast<Fake_TABLE *>(table.table));
}

static void BM_StoreFromTableBuffersNoData(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();
  MEM_ROOT mem_root;
  TableCollection table_collection =
      CreateTenTableJoin(initializer, &mem_root, false);

  String buffer;
  buffer.reserve(1024);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(StoreFromTableBuffers(table_collection, &buffer));
    ASSERT_GT(buffer.length(), 0);
  }
  StopBenchmarkTiming();

  DestroyFakeTables(table_collection);
}
BENCHMARK(BM_StoreFromTableBuffersNoData)

static void BM_StoreFromTableBuffersWithData(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  MEM_ROOT mem_root;
  TableCollection table_collection =
      CreateTenTableJoin(initializer, &mem_root, true);

  String buffer;
  buffer.reserve(1024);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(StoreFromTableBuffers(table_collection, &buffer));
    ASSERT_GT(buffer.length(), 0);
  }
  StopBenchmarkTiming();

  DestroyFakeTables(table_collection);
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
  HashJoinCondition *join_condition = nullptr;
  Mem_root_array<Item *> extra_conditions;

  HashJoinTestHelper(const Server_initializer &initializer,
                     vector<optional<int>> left_dataset,
                     vector<optional<int>> right_dataset,
                     bool is_nullable = false, bool null_safe_equal = false) {
    m_left_table_field.reset(new (&m_mem_root) Mock_field_long(
        "column1", is_nullable, /*is_unsigned=*/false));
    m_left_table.reset(new (&m_mem_root) Fake_TABLE(m_left_table_field.get()));

    m_right_table_field.reset(new (&m_mem_root) Mock_field_long(
        "column1", is_nullable, /*is_unsigned=*/false));
    m_right_table.reset(new (&m_mem_root)
                            Fake_TABLE(m_right_table_field.get()));
    SetupFakeTables(initializer, null_safe_equal);

    left_iterator.reset(new (&m_mem_root) FakeIntegerIterator(
        initializer.thd(), m_left_table.get(),
        down_cast<Field_long *>(m_left_table->field[0]),
        std::move(left_dataset)));
    right_iterator.reset(new (&m_mem_root) FakeIntegerIterator(
        initializer.thd(), m_right_table.get(),
        down_cast<Field_long *>(m_right_table->field[0]),
        std::move(right_dataset)));
  }

  HashJoinTestHelper(const Server_initializer &initializer,
                     vector<optional<string>> left_dataset,
                     vector<optional<string>> right_dataset,
                     bool is_nullable = false, bool null_safe_equal = false)
      : extra_conditions(initializer.thd()->mem_root) {
    m_left_table_field.reset(new (&m_mem_root) Mock_field_varstring(
        nullptr, "column1", /*char_len=*/255, is_nullable));
    m_left_table.reset(new (&m_mem_root) Fake_TABLE(m_left_table_field.get()));

    m_right_table_field.reset(new (&m_mem_root) Mock_field_varstring(
        nullptr, "column1", /*char_len=*/255, is_nullable));
    m_right_table.reset(new (&m_mem_root)
                            Fake_TABLE(m_right_table_field.get()));
    SetupFakeTables(initializer, null_safe_equal);

    left_iterator.reset(new (&m_mem_root) FakeStringIterator(
        initializer.thd(), m_left_table.get(),
        down_cast<Field_varstring *>(m_left_table->field[0]),
        std::move(left_dataset)));
    right_iterator.reset(new (&m_mem_root) FakeStringIterator(
        initializer.thd(), m_right_table.get(),
        down_cast<Field_varstring *>(m_right_table->field[0]),
        std::move(right_dataset)));
  }

  Prealloced_array<TABLE *, 4> left_tables() const {
    return Prealloced_array<TABLE *, 4>{left_qep_tab->table()};
  }
  Prealloced_array<TABLE *, 4> right_tables() const {
    return Prealloced_array<TABLE *, 4>{right_qep_tab->table()};
  }

 private:
  void SetupFakeTables(const Server_initializer &initializer,
                       bool null_safe_equal) {
    bitmap_set_all(m_left_table->write_set);
    bitmap_set_all(m_left_table->read_set);
    bitmap_set_all(m_right_table->write_set);
    bitmap_set_all(m_right_table->read_set);

    Query_block *query_block =
        parse(&initializer,
              "SELECT * FROM t1 JOIN t2 ON (t1.column1 = t2.column1);", 0);
    JOIN *join = new (&m_mem_root) JOIN(initializer.thd(), query_block);
    join->tables = 2;
    join->qep_tab = m_mem_root.ArrayAlloc<QEP_TAB>(join->tables);

    left_qep_tab = &join->qep_tab[0];
    left_qep_tab->set_qs(new (&m_mem_root) QEP_shared);
    left_qep_tab->set_idx(0);
    left_qep_tab->set_table(m_left_table.get());
    left_qep_tab->table_ref = m_left_table->pos_in_table_list;
    left_qep_tab->set_join(join);

    right_qep_tab = &join->qep_tab[1];
    right_qep_tab->set_qs(new (&m_mem_root) QEP_shared);
    right_qep_tab->set_idx(1);
    right_qep_tab->set_table(m_right_table.get());
    right_qep_tab->table_ref = m_right_table->pos_in_table_list;
    right_qep_tab->set_join(join);

    Item_field *const left_arg = new Item_field(m_left_table->field[0]);
    Item_field *const right_arg = new Item_field(m_right_table->field[0]);
    Item_eq_base *eq;
    if (null_safe_equal) {
      eq = new Item_func_equal(left_arg, right_arg);
    } else {
      eq = new Item_func_eq(left_arg, right_arg);
    }
    eq->set_cmp_func();
    join_condition = new (&m_mem_root) HashJoinCondition(eq, &m_mem_root);
  }

  // For simplicity, we allocate everything on a MEM_ROOT that takes care of
  // releasing any memory. However, we must ensure that the destructor is called
  // for Mock_field_varstring. Wrapping the fields in a unique_ptr_destroy_only
  // will ensure this.
  MEM_ROOT m_mem_root;
  unique_ptr_destroy_only<Fake_TABLE> m_left_table;
  unique_ptr_destroy_only<Fake_TABLE> m_right_table;
  unique_ptr_destroy_only<Field> m_left_table_field;
  unique_ptr_destroy_only<Field> m_right_table_field;
};

static vector<optional<int>> CollectIntResults(HashJoinIterator *iterator,
                                               Field *field) {
  vector<optional<int>> results;
  int error;
  while ((error = iterator->Read()) == 0) {
    if (field->is_null()) {
      results.emplace_back(nullopt);
    } else {
      results.emplace_back(field->val_int());
    }
  }
  EXPECT_EQ(-1, error);  // EOF
  return results;
}

static vector<optional<string>> CollectStringResults(HashJoinIterator *iterator,
                                                     Field *field) {
  vector<optional<string>> results;
  String buffer;
  int error;
  while ((error = iterator->Read()) == 0) {
    if (field->is_null()) {
      results.emplace_back(nullopt);
    } else {
      const String *res = field->val_str(&buffer);
      results.emplace_back(to_string(*res));
    }
  }
  EXPECT_EQ(-1, error);  // EOF
  return results;
}

TEST(HashJoinTest, InnerJoinIntOneToOneMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, vector<optional<int>>{3}, {3});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_THAT(CollectIntResults(&hash_join_iterator,
                                test_helper.left_qep_tab->table()->field[0]),
              ElementsAre(3));
}

TEST(HashJoinTest, InnerJoinIntNoMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {2, 4}, {3, 5});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_EQ(-1, hash_join_iterator.Read());
}

TEST(HashJoinTest, InnerJoinIntOneToManyMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {2}, {2, 2});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  // We expect two result rows before the iterator should return -1 (EOF).
  EXPECT_THAT(CollectIntResults(&hash_join_iterator,
                                test_helper.left_qep_tab->table()->field[0]),
              ElementsAre(2, 2));
}

TEST(HashJoinTest, InnerJoinStringOneToOneMatch) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {"abc"}, {"abc"});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_THAT(CollectStringResults(&hash_join_iterator,
                                   test_helper.left_qep_tab->table()->field[0]),
              ElementsAre("abc"));
}

TEST(HashJoinTest, HashTableCaching) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {2, 3}, {1, 2, 3});
  FakeIntegerIterator *build_iterator =
      down_cast<FakeIntegerIterator *>(test_helper.left_iterator.get());

  uint64_t hash_table_generation = 0;
  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, &hash_table_generation);

  Field *const probe_field = test_helper.left_qep_tab->table()->field[0];

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_EQ(3, build_iterator->num_read_calls());

  EXPECT_THAT(CollectIntResults(&hash_join_iterator, probe_field),
              ElementsAre(2, 3));

  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_EQ(3, build_iterator->num_read_calls());  // Unchanged due to caching.

  EXPECT_THAT(CollectIntResults(&hash_join_iterator, probe_field),
              ElementsAre(2, 3));

  hash_table_generation = 1;
  ASSERT_FALSE(hash_join_iterator.Init());
  EXPECT_EQ(6, build_iterator->num_read_calls());

  EXPECT_THAT(CollectIntResults(&hash_join_iterator, probe_field),
              ElementsAre(2, 3));
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

  vector<optional<int>> left_dataset;
  vector<optional<int>> right_dataset;
  for (int i = 0; i < (max_value - min_value); ++i) {
    left_dataset.emplace_back(distribution(generator));
    right_dataset.emplace_back(distribution(generator));
  }

  HashJoinTestHelper test_helper(initializer, std::move(left_dataset),
                                 std::move(right_dataset));

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(hash_join_iterator.Init());
  }
  StopBenchmarkTiming();
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

  vector<optional<int>> left_dataset;
  vector<optional<int>> right_dataset;
  for (int i = 0; i < (max_value - min_value); ++i) {
    left_dataset.emplace_back(distribution(generator));
    right_dataset.emplace_back(distribution(generator));
  }
  HashJoinTestHelper test_helper(initializer, std::move(left_dataset),
                                 std::move(right_dataset));

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(hash_join_iterator.Init());
    StartBenchmarkTiming();
    int result;
    do {
      result = hash_join_iterator.Read();
    } while (result == 0);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_HashTableIteratorProbe)

// Do a benchmark of lookup in the hash table for semijoin. This is to see if
// there is any difference between equal_range() and find(),
//
// The table that the hash table is built from is a single-column table with
// 10000 uniformly distributed values between [0, 5000). We give the hash table
// enough memory so that is doesn't spill out to disk.
static void BM_HashTableIteratorProbeSemiJoin(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  const int num_value = 10000;
  const int min_value = 0;
  const int max_value = 5000;
  const int seed = 8834245;
  std::mt19937 generator(seed);
  std::uniform_int_distribution<> distribution(min_value, max_value);

  vector<optional<int>> left_dataset;
  vector<optional<int>> right_dataset;
  for (int i = 0; i < num_value; ++i) {
    left_dataset.emplace_back(distribution(generator));
    right_dataset.emplace_back(distribution(generator));
  }
  HashJoinTestHelper test_helper(initializer, std::move(left_dataset),
                                 std::move(right_dataset));

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::SEMI,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  for (size_t i = 0; i < num_iterations; ++i) {
    ASSERT_FALSE(hash_join_iterator.Init());
    StartBenchmarkTiming();
    int result;
    do {
      result = hash_join_iterator.Read();
    } while (result == 0);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_HashTableIteratorProbeSemiJoin)

TEST(HashJoinTest, SemiJoinInt) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // The iterator will execute something that is equivalent to the query
  // "SELECT * FROM probe_data WHERE a IN (SELECT b FROM build_data);"
  HashJoinTestHelper test_helper(initializer, {3, 3, 4, 5}, {3, 5, 6});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::SEMI,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_THAT(CollectIntResults(&hash_join_iterator,
                                test_helper.right_qep_tab->table()->field[0]),
              ElementsAre(3, 5));
}

TEST(HashJoinTest, AntiJoinInt) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // The iterator will execute something that is equivalent to the query
  // "SELECT * FROM probe_data WHERE a NOT IN (SELECT b FROM build_data);"
  HashJoinTestHelper test_helper(initializer, {3, 3, 4, 5}, {3, 5, 6});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 10 * 1024 * 1024 /* 10 MB */,
      {*test_helper.join_condition}, true, JoinType::ANTI,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(6, test_helper.right_qep_tab->table()->field[0]->val_int());
  EXPECT_EQ(-1, hash_join_iterator.Read());
}

TEST(HashJoinTest, LeftHashJoinInt) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // The iterator will execute something that is equivalent to the query
  // "SELECT * FROM left_data p LEFT JOIN right_data b ON p.col = b.col;"
  HashJoinTestHelper test_helper(initializer, vector<optional<int>>{3}, {});

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.left_iterator), test_helper.left_tables(),
      /*store_rowids=*/false, /*tables_to_get_rowid_for=*/0,
      10 * 1024 * 1024 /* 10 MB */, {*test_helper.join_condition}, true,
      JoinType::OUTER, test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(3, test_helper.left_qep_tab->table()->field[0]->val_int());
  EXPECT_FALSE(test_helper.left_qep_tab->table()->field[0]->is_null());

  test_helper.right_qep_tab->table()->field[0]->val_int();
  EXPECT_TRUE(test_helper.right_qep_tab->table()->field[0]->is_null());

  EXPECT_EQ(-1, hash_join_iterator.Read());
}

TEST(HashJoinTest, HashJoinResetNullFlagBeforeBuild) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // The iterator will execute something that is equivalent to the query
  // "SELECT * FROM left_data p LEFT JOIN right_data b ON p.col = b.col;"
  HashJoinTestHelper test_helper(initializer, vector<optional<int>>{3}, {3});

  // Explicitly set the NULL row flag for the right/build input. The hash join
  // iterator should reset this flag before building the hash table.
  test_helper.right_iterator->SetNullRowFlag(/*is_null_row=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*estimated_build_rows=*/1000,
      std::move(test_helper.left_iterator), test_helper.left_tables(),
      /*store_rowids=*/false, /*tables_to_get_rowid_for=*/0,
      10 * 1024 * 1024 /* 10 MB */, {*test_helper.join_condition}, true,
      JoinType::OUTER, test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  // Verify that we do not get any NULL value back, even though we explicitly
  // set the NULL row flag before Init was called; Init() should reset the NULL
  // row flag before reading from the build table.
  EXPECT_EQ(0, hash_join_iterator.Read());
  EXPECT_EQ(3, test_helper.left_qep_tab->table()->field[0]->val_int());
  EXPECT_FALSE(test_helper.left_qep_tab->table()->field[0]->is_null());

  EXPECT_EQ(3, test_helper.right_qep_tab->table()->field[0]->val_int());
  EXPECT_FALSE(test_helper.right_qep_tab->table()->field[0]->is_null());

  EXPECT_EQ(-1, hash_join_iterator.Read());
}

TEST(HashJoinTest, HashJoinChunkFiles) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  vector<optional<int>> left_dataset;
  int dataset_sz = 1000;
  if (SIZEOF_VOIDP == 4) {
    // 32-bit arch, modify #rows to get same numbers for chunk calculations
    dataset_sz *= 2;
  }
  for (int i = 0; i < dataset_sz; ++i) {
    left_dataset.emplace_back(i);
  }

  HashJoinTestHelper test_helper(initializer, left_dataset, left_dataset);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(), /*estimated_build_rows=*/dataset_sz,
      std::move(test_helper.right_iterator), test_helper.right_tables(),
      /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0, 1024 /* 1 KB */,
      {*test_helper.join_condition}, true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  // We hash 1000 rows (64-bit arch) or 2000 rows (32-bit arch). The hash
  // table can normally hold about 410 rows on 64-bit machines and 820 rows on
  // 32-bit machines (verified experimentally). To get the required number of
  // chunks, the number of remaining rows should be divided by the number of
  // hash table rows. But as a safeguard, this calculation is adjusted to yield
  // a few extra chunks rather than risk having too few chunks. So the number
  // of remaining rows is instead divided by a reduced count of hash table rows
  // The reduced count is obtained by multiplying the hash table row count by
  // a 'reduction factor' of 0.9. E.g. for 64-bit rows:
  // reduced_rows_in_hash_table = 410 * 0.9 = 369
  // remaining_rows = 1000 - 410 = 590
  // required number of chunks = remaining_rows / reduced_rows_in_hash_table
  //                           = 590 / 369 = 1.59, rounded up to 2
  // So a count of 2 chunks is expected.
  EXPECT_EQ(2, hash_join_iterator.ChunkCount());
}

TEST(HashJoinTest, InnerJoinIntNullable) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer,
                                 {-2, 0, nullopt, 1, 2, 3, nullopt},
                                 {-2, -1, 0, nullopt, 2, 3, 4, nullopt},
                                 /*is_nullable=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_THAT(CollectIntResults(&hash_join_iterator,
                                test_helper.left_qep_tab->table()->field[0]),
              ElementsAre(-2, 0, 2, 3));
}

TEST(HashJoinTest, InnerJoinStringNullable) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer,
                                 {"abc", nullopt, "xyz", "", "x", nullopt},
                                 {"abc", nullopt, "xyz", "", "y", nullopt},
                                 /*is_nullable=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_THAT(CollectStringResults(&hash_join_iterator,
                                   test_helper.left_qep_tab->table()->field[0]),
              ElementsAre("abc", "xyz", ""));
}

TEST(HashJoinTest, InnerJoinIntNullSafeEqual) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {nullopt, 0, 1, 2, nullopt, 3},
                                 {nullopt, 0, 1, nullopt, 1, 2, 4},
                                 /*is_nullable=*/true,
                                 /*null_safe_equal=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_THAT(CollectIntResults(&hash_join_iterator,
                                test_helper.left_qep_tab->table()->field[0]),
              ElementsAre(nullopt, nullopt, 0, 1, nullopt, nullopt, 1, 2));
}

TEST(HashJoinTest, LeftJoinIntNullSafeEqual) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer, {nullopt, 0, 1, 2, nullopt, 3},
                                 {nullopt, 0, 1, nullopt, 1, 2, 4},
                                 /*is_nullable=*/true,
                                 /*null_safe_equal=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::OUTER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  const Field *const build_field = test_helper.left_qep_tab->table()->field[0];
  const Field *const probe_field = test_helper.right_qep_tab->table()->field[0];
  String buffer;
  vector<pair<optional<int>, optional<int>>> results;
  int error;
  while ((error = hash_join_iterator.Read()) == 0) {
    pair<optional<int>, optional<int>> &values = results.emplace_back();
    if (!build_field->is_null()) {
      values.first = build_field->val_int();
    }
    if (!probe_field->is_null()) {
      values.second = probe_field->val_int();
    }
  }
  EXPECT_EQ(-1, error);
  EXPECT_THAT(
      results,
      ElementsAre(pair(nullopt, nullopt), pair(nullopt, nullopt), pair(0, 0),
                  pair(1, 1), pair(nullopt, nullopt), pair(nullopt, nullopt),
                  pair(1, 1), pair(2, 2), pair(nullopt, 4)));
}

TEST(HashJoinTest, InnerJoinStringNullSafeEqual) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer,
                                 {nullopt, "", "abc", "def", nullopt, "abc"},
                                 {nullopt, "", "abc", nullopt, "xyz", "def"},
                                 /*is_nullable=*/true,
                                 /*null_safe_equal=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::INNER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  EXPECT_THAT(
      CollectStringResults(&hash_join_iterator,
                           test_helper.left_qep_tab->table()->field[0]),
      ElementsAre(nullopt, nullopt, "", "abc", "abc", nullopt, nullopt, "def"));
}

TEST(HashJoinTest, LeftJoinStringNullSafeEqual) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  HashJoinTestHelper test_helper(initializer,
                                 {nullopt, "", "abc", "def", nullopt, "abc"},
                                 {nullopt, "", "abc", nullopt, "xyz", "def"},
                                 /*is_nullable=*/true,
                                 /*null_safe_equal=*/true);

  HashJoinIterator hash_join_iterator(
      initializer.thd(), std::move(test_helper.left_iterator),
      test_helper.left_tables(),
      /*estimated_build_rows=*/1000, std::move(test_helper.right_iterator),
      test_helper.right_tables(), /*store_rowids=*/false,
      /*tables_to_get_rowid_for=*/0,
      /*max_memory_available=*/size_t{10} * 1024 * 1024,
      {*test_helper.join_condition},
      /*allow_spill_to_disk=*/true, JoinType::OUTER,
      test_helper.extra_conditions,
      /*probe_input_batch_mode=*/false, /*hash_table_generation=*/nullptr);

  ASSERT_FALSE(hash_join_iterator.Init());

  const Field *const build_field = test_helper.left_qep_tab->table()->field[0];
  const Field *const probe_field = test_helper.right_qep_tab->table()->field[0];
  String buffer;
  vector<pair<optional<string>, optional<string>>> results;
  int error;
  while ((error = hash_join_iterator.Read()) == 0) {
    pair<optional<string>, optional<string>> &row = results.emplace_back();
    if (!build_field->is_null()) {
      row.first = to_string(*build_field->val_str(&buffer));
    }
    if (!probe_field->is_null()) {
      row.second = to_string(*probe_field->val_str(&buffer));
    }
  }
  EXPECT_EQ(-1, error);
  EXPECT_THAT(results,
              ElementsAre(pair(nullopt, nullopt), pair(nullopt, nullopt),
                          pair("", ""), pair("abc", "abc"), pair("abc", "abc"),
                          pair(nullopt, nullopt), pair(nullopt, nullopt),
                          pair(nullopt, "xyz"), pair("def", "def")));
}

}  // namespace hash_join_unittest

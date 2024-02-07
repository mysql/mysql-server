/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "sql/field.h"                   // my_charset_numeric
#include "sql/histograms/equi_height.h"  // Equi_height
#include "sql/histograms/histogram.h"    // Histogram, Histogram_comparator
#include "sql/histograms/table_histograms.h"
#include "sql/histograms/value_map.h"  // Value_map<T>
#include "sql/psi_memory_key.h"
#include "sql/sql_base.h"  // LOCK_open

using namespace histograms;

// Ensures LOCK_open is available.
class TableHistogramsCollectionTest : public ::testing::Test {
 public:
  void SetUp() override { mysql_mutex_init(0, &LOCK_open, MY_MUTEX_INIT_FAST); }
  void TearDown() override { mysql_mutex_destroy(&LOCK_open); }
};

TEST(Table_histograms, create) {
  Table_histograms *table_histograms =
      Table_histograms::create(key_memory_histograms);
  ASSERT_TRUE(table_histograms);
  table_histograms->destroy();
}

TEST(Table_histograms, insert_histogram) {
  Table_histograms *table_histograms =
      Table_histograms::create(key_memory_histograms);
  ASSERT_TRUE(table_histograms);

  // Create a histogram to be inserted.
  MEM_ROOT mem_root;
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  for (longlong i = 0; i < 100; ++i) {
    values.add_values(i, 1);
  }
  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);
  size_t num_buckets = 4;
  ASSERT_FALSE(histogram->build_histogram(values, num_buckets));

  EXPECT_FALSE(table_histograms->insert_histogram(1, histogram));
  EXPECT_TRUE(table_histograms->insert_histogram(1, histogram));

  EXPECT_TRUE(table_histograms->find_histogram(1) != nullptr);
  EXPECT_TRUE(table_histograms->find_histogram(2) == nullptr);
  table_histograms->destroy();
}

TEST_F(TableHistogramsCollectionTest, Insert) {
  mysql_mutex_lock(&LOCK_open);
  Table_histograms_collection histograms_collection;
  EXPECT_TRUE(histograms_collection.size() == 0);

  // Collection is empty.
  EXPECT_TRUE(histograms_collection.acquire() == nullptr);

  // Insert a Table_histograms object.
  Table_histograms *table_histograms =
      Table_histograms::create(key_memory_histograms);
  ASSERT_TRUE(table_histograms);
  EXPECT_FALSE(histograms_collection.insert(table_histograms));
  EXPECT_TRUE(histograms_collection.size() == 1);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 0);

  // Acquire and release.
  const Table_histograms *current_histograms = histograms_collection.acquire();
  EXPECT_TRUE(current_histograms != nullptr);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 1);
  histograms_collection.release(current_histograms);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 0);
  EXPECT_TRUE(histograms_collection.size() == 1);

  // Acquire twice.
  current_histograms = histograms_collection.acquire();
  EXPECT_TRUE(current_histograms != nullptr);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 1);

  const Table_histograms *second_current_histograms =
      histograms_collection.acquire();
  EXPECT_TRUE(second_current_histograms != nullptr);
  EXPECT_TRUE(second_current_histograms == current_histograms);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 2);

  // Release the first pointer.
  histograms_collection.release(current_histograms);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 1);

  // Insert another Table_histograms object.
  Table_histograms *second_table_histograms =
      Table_histograms::create(key_memory_histograms);
  ASSERT_TRUE(second_table_histograms);
  EXPECT_FALSE(histograms_collection.insert(second_table_histograms));
  EXPECT_TRUE(histograms_collection.size() == 2);

  // After releasing the second pointer to the previous current object it should
  // have a reference count of zero and be released.
  histograms_collection.release(second_current_histograms);
  EXPECT_TRUE(histograms_collection.total_reference_count() == 0);
  EXPECT_TRUE(histograms_collection.size() == 1);
  mysql_mutex_unlock(&LOCK_open);
}

TEST_F(TableHistogramsCollectionTest, FullCollection) {
  mysql_mutex_lock(&LOCK_open);
  Table_histograms_collection histograms_collection;
  EXPECT_TRUE(histograms_collection.size() == 0);

  // Fill the collection
  std::vector<const Table_histograms *> acquired_histograms;
  for (size_t i = 0; i < kMaxNumberOfTableHistogramsInCollection; ++i) {
    Table_histograms *table_histograms =
        Table_histograms::create(key_memory_histograms);
    ASSERT_TRUE(table_histograms);
    EXPECT_FALSE(histograms_collection.insert(table_histograms));
    const Table_histograms *current_histograms =
        histograms_collection.acquire();
    EXPECT_TRUE(current_histograms != nullptr);
    EXPECT_TRUE(histograms_collection.size() == i + 1);
    acquired_histograms.push_back(current_histograms);
  }

  // The next insertion should fail due to the collection being full.
  EXPECT_TRUE(histograms_collection.size() ==
              kMaxNumberOfTableHistogramsInCollection);
  Table_histograms *table_histograms =
      Table_histograms::create(key_memory_histograms);
  ASSERT_TRUE(table_histograms);
  EXPECT_TRUE(histograms_collection.insert(table_histograms));

  // The insertion failed so the Table_histograms_collection did not take
  // ownership of the pointer and we have to destroy the Table_histograms
  // object.
  table_histograms->destroy();

  // Clean up by releasing all acquired histograms and destroying the
  // collection.
  for (const Table_histograms *histograms : acquired_histograms) {
    histograms_collection.release(histograms);
  }
  mysql_mutex_unlock(&LOCK_open);
}
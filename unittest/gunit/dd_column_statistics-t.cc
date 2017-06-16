/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <gtest/gtest.h>

#include <cstring>

#include "dd.h"
#include "dd/impl/dictionary_impl.h"
#include "dd/impl/raw/raw_record.h"
#include "dd/impl/types/column_statistics_impl.h"
#include "my_inttypes.h"
#include "test_utils.h"

#include "histograms/equi_height.h"
#include "histograms/singleton.h"
#include "histograms/value_map.h"

namespace dd_column_statistics_unittest {

using namespace dd;
using namespace dd_unittest;

using dd_unittest::Mock_dd_field_longlong;
using dd_unittest::Mock_dd_field_varstring;

using my_testing::Server_initializer;

using ::testing::Invoke;
using ::testing::WithArgs;

template <typename T>
class ColumnStatisticsTest : public ::testing::Test
{
public:
  ColumnStatisticsTest()
  {}

  virtual void SetUp()
  {
    m_dict= new Dictionary_impl();

    // Dummy server initialization.
    m_init.SetUp();
  }

  virtual void TearDown()
  {
    delete m_dict;

    // Tear down dummy server.
    m_init.TearDown();
  }

  // Return dummy thd.
  THD *thd()
  {
    return m_init.thd();
  }

  Dictionary_impl *m_dict;                // Dictionary instance.
  my_testing::Server_initializer m_init;  // Server initializer.
};

void add_values(histograms::Value_map<longlong> &value_map)
{
  value_map.add_values(0LL, 10);
}

void add_values(histograms::Value_map<ulonglong> &value_map)
{
  value_map.add_values(0ULL, 10);
}

void add_values(histograms::Value_map<double> &value_map)
{
  value_map.add_values(0.0, 10);
}

void add_values(histograms::Value_map<String> &value_map)
{
  value_map.add_values(String(), 10);
}

void add_values(histograms::Value_map<MYSQL_TIME> &value_map)
{
  MYSQL_TIME my_time;
  my_time.year= 2017;
  my_time.month= 1;
  my_time.day= 1;
  my_time.hour= 10;
  my_time.minute= 0;
  my_time.second= 0;
  my_time.second_part= 0;
  my_time.neg= false;
  my_time.time_type= MYSQL_TIMESTAMP_DATETIME;
  value_map.add_values(my_time, 10);
}

void add_values(histograms::Value_map<my_decimal> &value_map)
{
  my_decimal my_decimal;
  double2my_decimal(0, 0.0, &my_decimal);
  value_map.add_values(my_decimal, 10);
}

typedef ::testing::Types
<longlong, ulonglong, double, String, MYSQL_TIME, my_decimal> HistogramTypes;
TYPED_TEST_CASE(ColumnStatisticsTest, HistogramTypes);

TYPED_TEST(ColumnStatisticsTest, StoreAndRestoreAttributesEquiHeight)
{
  List<Field> m_field_list;
  Fake_TABLE_SHARE dummy_share(1); // Keep Field_varstring constructor happy.

  // Add fields
  Mock_dd_field_longlong id;
  Mock_dd_field_longlong catalog_id;
  Mock_dd_field_varstring name(255, &dummy_share);
  Mock_dd_field_varstring schema_name(64, &dummy_share);
  Mock_dd_field_varstring table_name(64, &dummy_share);
  Mock_dd_field_varstring column_name(64, &dummy_share);
  Base_mock_field_json histogram;

  m_field_list.push_back(&id);
  m_field_list.push_back(&catalog_id);
  m_field_list.push_back(&name);
  m_field_list.push_back(&schema_name);
  m_field_list.push_back(&table_name);
  m_field_list.push_back(&column_name);
  m_field_list.push_back(&histogram);

  // Create table object (and table share implicitly).
  Fake_TABLE table(m_field_list);
  bitmap_set_all(table.write_set);
  dd::Raw_record r(&table);

  MEM_ROOT mem_root(PSI_NOT_INSTRUMENTED, 256, 0);

  dd::Column_statistics_impl column_statistics;

  {
    /*
      Create a new scope, so that value_map goes out of scope before the
      MEM_ROOT is freed.
    */
    histograms::Value_map<TypeParam> value_map(&my_charset_latin1);
    add_values(value_map);

    histograms::Equi_height<TypeParam> equi_height(&mem_root, "schema", "table",
                                                  "column");

    EXPECT_FALSE(equi_height.build_histogram(value_map, 1024));

    // Set the attributes
    column_statistics.set_histogram(&equi_height);
    column_statistics.set_schema_name("schema");
    column_statistics.set_table_name("table");
    column_statistics.set_column_name("column");

    /*
      Note: We cannot mock away and make expectations store_json/val_json since
      the function is not virtual.
    */
    ON_CALL(catalog_id, store(_, _)).
      WillByDefault(Invoke(&catalog_id, &Mock_dd_field_longlong::fake_store));

    ON_CALL(name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&name,
                                       &Mock_dd_field_varstring::fake_store)));

    ON_CALL(schema_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&schema_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(schema_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&schema_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    ON_CALL(table_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&table_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(table_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&table_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    ON_CALL(column_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&column_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(column_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&column_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    EXPECT_CALL(catalog_id, store(_, _)).Times(1);
    EXPECT_CALL(name, store(_, _, _)).Times(1);
    EXPECT_CALL(schema_name,
                store(column_statistics.schema_name().c_str(), _, _)).Times(1);
    EXPECT_CALL(table_name,
                store(column_statistics.table_name().c_str(), _, _)).Times(1);
    EXPECT_CALL(column_name,
                store(column_statistics.column_name().c_str(), _, _)).Times(1);

    // Store attributes
    EXPECT_FALSE(column_statistics.store_attributes(&r));

    EXPECT_CALL(id, val_int()).Times(1);
    EXPECT_CALL(name, val_str(_, _)).Times(1);
    EXPECT_CALL(schema_name, val_str(_, _)).Times(1);
    EXPECT_CALL(table_name, val_str(_, _)).Times(1);
    EXPECT_CALL(column_name, val_str(_, _)).Times(1);

    dd::Column_statistics_impl column_statistics_restored;
    EXPECT_FALSE(column_statistics_restored.restore_attributes(r));

    // Verify that the stored and restored contents are the same.
    EXPECT_EQ(std::strcmp(column_statistics.schema_name().c_str(),
                          column_statistics_restored.schema_name().c_str()), 0);
    EXPECT_EQ(std::strcmp(column_statistics.table_name().c_str(),
                          column_statistics_restored.table_name().c_str()), 0);
    EXPECT_EQ(std::strcmp(column_statistics.column_name().c_str(),
                          column_statistics_restored.column_name().c_str()), 0);

    // Check if the histogram contents is still the same
    EXPECT_EQ(column_statistics.histogram()->get_num_buckets(),
              column_statistics_restored.histogram()->get_num_buckets());

    EXPECT_EQ(
      column_statistics.histogram()->get_num_buckets_specified(),
      column_statistics_restored.histogram()->get_num_buckets_specified());

    EXPECT_EQ(
      column_statistics.histogram()->get_character_set()->number,
      column_statistics_restored.histogram()->get_character_set()->number);

    EXPECT_DOUBLE_EQ(
      column_statistics.histogram()->get_null_values_fraction(),
      column_statistics_restored.histogram()->get_null_values_fraction());

    EXPECT_DOUBLE_EQ(
      column_statistics.histogram()->get_sampling_rate(),
      column_statistics_restored.histogram()->get_sampling_rate());
  }
}

TYPED_TEST(ColumnStatisticsTest, StoreAndRestoreAttributesSingleton)
{
  List<Field> m_field_list;
  Fake_TABLE_SHARE dummy_share(1); // Keep Field_varstring constructor happy.

  // Add fields
  Mock_dd_field_longlong id;
  Mock_dd_field_longlong catalog_id;
  Mock_dd_field_varstring name(255, &dummy_share);
  Mock_dd_field_varstring schema_name(64, &dummy_share);
  Mock_dd_field_varstring table_name(64, &dummy_share);
  Mock_dd_field_varstring column_name(64, &dummy_share);
  Base_mock_field_json histogram;

  m_field_list.push_back(&id);
  m_field_list.push_back(&catalog_id);
  m_field_list.push_back(&name);
  m_field_list.push_back(&schema_name);
  m_field_list.push_back(&table_name);
  m_field_list.push_back(&column_name);
  m_field_list.push_back(&histogram);

  // Create table object (and table share implicitly).
  Fake_TABLE table(m_field_list);
  bitmap_set_all(table.write_set);
  dd::Raw_record r(&table);

  MEM_ROOT mem_root(PSI_NOT_INSTRUMENTED, 256, 0);

  dd::Column_statistics_impl column_statistics;

  {
    /*
      Create a new scope, so that value_map goes out of scope before the
      MEM_ROOT is freed.
    */
    histograms::Value_map<TypeParam> value_map(&my_charset_latin1);
    add_values(value_map);

    histograms::Singleton<TypeParam> singleton(&mem_root, "schema", "table",
                                               "column");

    EXPECT_FALSE(singleton.build_histogram(value_map, 1024));

    // Set the attributes
    column_statistics.set_histogram(&singleton);
    column_statistics.set_schema_name("schema");
    column_statistics.set_table_name("table");
    column_statistics.set_column_name("column");

    /*
      Note: We cannot mock away and make expectations store_json/val_json since
      the function is not virtual.
    */
    ON_CALL(catalog_id, store(_, _)).
      WillByDefault(Invoke(&catalog_id, &Mock_dd_field_longlong::fake_store));

    ON_CALL(name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&name,
                                       &Mock_dd_field_varstring::fake_store)));

    ON_CALL(schema_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&schema_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(schema_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&schema_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    ON_CALL(table_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&table_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(table_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&table_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    ON_CALL(column_name, store(_, _, _)).
      WillByDefault(WithArgs<0>(Invoke(&column_name,
                                       &Mock_dd_field_varstring::fake_store)));
    ON_CALL(column_name, val_str(_, _)).
      WillByDefault(WithArgs<1>(
                    Invoke(&column_name,
                           &Mock_dd_field_varstring::fake_val_str)));

    EXPECT_CALL(catalog_id, store(_, _)).Times(1);
    EXPECT_CALL(name, store(_, _, _)).Times(1);
    EXPECT_CALL(schema_name,
                store(column_statistics.schema_name().c_str(), _, _)).Times(1);
    EXPECT_CALL(table_name,
                store(column_statistics.table_name().c_str(), _, _)).Times(1);
    EXPECT_CALL(column_name,
                store(column_statistics.column_name().c_str(), _, _)).Times(1);

    // Store attributes
    EXPECT_FALSE(column_statistics.store_attributes(&r));

    EXPECT_CALL(id, val_int()).Times(1);
    EXPECT_CALL(name, val_str(_, _)).Times(1);
    EXPECT_CALL(schema_name, val_str(_, _)).Times(1);
    EXPECT_CALL(table_name, val_str(_, _)).Times(1);
    EXPECT_CALL(column_name, val_str(_, _)).Times(1);

    dd::Column_statistics_impl column_statistics_restored;
    EXPECT_FALSE(column_statistics_restored.restore_attributes(r));

    // Verify that the stored and restored contents are the same.
    EXPECT_EQ(std::strcmp(column_statistics.schema_name().c_str(),
                          column_statistics_restored.schema_name().c_str()), 0);
    EXPECT_EQ(std::strcmp(column_statistics.table_name().c_str(),
                          column_statistics_restored.table_name().c_str()), 0);
    EXPECT_EQ(std::strcmp(column_statistics.column_name().c_str(),
                          column_statistics_restored.column_name().c_str()), 0);

    // Check if the histogram contents is still the same
    EXPECT_EQ(column_statistics.histogram()->get_num_buckets(),
              column_statistics_restored.histogram()->get_num_buckets());

    EXPECT_EQ(column_statistics.histogram()->get_character_set()->number,
              column_statistics_restored.histogram()->get_character_set()->number);

    EXPECT_DOUBLE_EQ(
      column_statistics.histogram()->get_null_values_fraction(),
      column_statistics_restored.histogram()->get_null_values_fraction());
  }
}

}

/* Copyright (c) 2012, 2026, Oracle and/or its affiliates.

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

#include "unittest/gunit/handler-t.h"

#include <stddef.h>
#include <sys/types.h>
#include <memory>

#include "sql/sql_executor.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/mock_field_datetime.h"
#include "unittest/gunit/test_utils.h"

namespace {

using my_testing::Mock_error_handler;
using my_testing::Server_initializer;

using ::testing::StrictMock;

class HandlerTest : public ::testing::Test {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};

class Auto_increment_handler : public Base_mock_HANDLER {
 public:
  explicit Auto_increment_handler(TABLE_SHARE *share)
      : Base_mock_HANDLER(nullptr, share) {}

  void get_auto_increment(ulonglong, ulonglong, ulonglong desired,
                          ulonglong *first, ulonglong *reserved) override {
    ++engine_calls;
    *first = 1000;
    *reserved = desired;
  }

  unsigned engine_calls{0};
};

class HandlerAutoIncrementTest : public HandlerTest {
 protected:
  void SetUp() override {
    HandlerTest::SetUp();
    field = std::make_unique<Mock_field_long>(true);
    fake_table = std::make_unique<Fake_TABLE>(field.get());
    fake_table->get_share()->tmp_table = INTERNAL_TMP_TABLE;
    fake_table->next_number_field = field.get();
    auto_handler =
        std::make_unique<Auto_increment_handler>(fake_table->get_share());
    auto_handler->change_table_ptr(fake_table.get(), fake_table->get_share());
    fake_table->set_handler(auto_handler.get());
    field->make_readable();
    field->make_writable();
    thd()->variables.auto_increment_increment = 1;
    thd()->variables.auto_increment_offset = 1;
  }

  void TearDown() override {
    auto_handler->ha_release_auto_increment();
    auto_handler.reset();
    fake_table.reset();
    field.reset();
    HandlerTest::TearDown();
  }

  int update(ulonglong value = 0) {
    EXPECT_EQ(TYPE_OK, field->store(static_cast<longlong>(value), true));
    fake_table->autoinc_field_has_explicit_non_null_value = value != 0;
    return auto_handler->update_auto_increment();
  }

  std::unique_ptr<Mock_field_long> field;
  std::unique_ptr<Fake_TABLE> fake_table;
  std::unique_ptr<Auto_increment_handler> auto_handler;
};

TEST_F(HandlerAutoIncrementTest, StatementReset) {
  auto_handler->ha_start_bulk_insert(3);
  thd()->force_one_auto_inc_interval(1);
  ASSERT_EQ(0, update());
  EXPECT_EQ(1, field->val_int());
  EXPECT_EQ(0, auto_handler->engine_calls);

  // End the statement with unused forced values.
  auto_handler->ha_end_bulk_insert();
  auto_handler->ha_release_auto_increment();
  auto_handler->ha_start_bulk_insert(1);
  ASSERT_EQ(0, update());
  EXPECT_EQ(1000, field->val_int());
  EXPECT_EQ(1, auto_handler->engine_calls);
}

TEST_F(HandlerAutoIncrementTest, UnknownRowCount) {
  auto_handler->ha_start_bulk_insert(0);
  thd()->force_one_auto_inc_interval(1);
  ASSERT_EQ(0, update());
  ASSERT_EQ(0, update(100));
  ASSERT_EQ(0, update());
  EXPECT_EQ(101, field->val_int());
  EXPECT_EQ(ULLONG_MAX, auto_handler->auto_inc_interval_for_cur_row.maximum());
  EXPECT_EQ(0, auto_handler->engine_calls);
}

TEST_F(HandlerAutoIncrementTest, ExhaustedEstimateKeepsForcedSequence) {
  auto_handler->ha_start_bulk_insert(1);
  thd()->force_one_auto_inc_interval(1);
  for (longlong value = 1; value <= 4; ++value) {
    ASSERT_EQ(0, update());
    EXPECT_EQ(value, field->val_int());
  }
  EXPECT_EQ(4, auto_handler->auto_inc_interval_for_cur_row.minimum());
  EXPECT_EQ(4, auto_handler->auto_inc_interval_for_cur_row.values());
  EXPECT_EQ(0, auto_handler->engine_calls);
}

/**
  Some handler error returns are passed on to report_handler_error()
  which will:
    - ignore errors like END_OF_FILE
    - print most errors to the error log
    - pass the error code back to handler::print_error()
 */
TEST_F(HandlerTest, ReportErrorHandler) {
  Mock_field_datetime field_datetime;
  Fake_TABLE *table = static_cast<Fake_TABLE *>(field_datetime.table);
  handlerton *hton = nullptr;
  StrictMock<Mock_HANDLER> mock_handler(hton, table->get_share());
  table->set_handler(&mock_handler);

  // This error should be ignored.
  EXPECT_EQ(-1, report_handler_error(table, HA_ERR_END_OF_FILE));

  // This one should not be printed to stderr, but passed on to the handler.
  EXPECT_CALL(mock_handler, print_error(HA_ERR_TABLE_DEF_CHANGED, 0)).Times(1);
  EXPECT_EQ(1, report_handler_error(table, HA_ERR_TABLE_DEF_CHANGED));
}

TEST_F(HandlerTest, TableInMemoryEstimate) {
  Mock_field_datetime field_datetime;
  Fake_TABLE *table = static_cast<Fake_TABLE *>(field_datetime.table);
  handlerton *hton = nullptr;
  StrictMock<Mock_HANDLER> mock_handler(hton, table->get_share());
  table->set_handler(&mock_handler);

  // Verify that the handler does not know the buffer size
  EXPECT_EQ(mock_handler.get_memory_buffer_size(), -1);
  /*
    The implementation of table_in_memory_estimate() assumes that the
    memory buffer is 100 MB if the storage engine does not report the
    size of its memory buffer.
  */
  const uint mem_buf_size = 100 * 1024 * 1024;

  /*
    Define representative table sizes to use in tests.
  */
  // Table that is less than 20% of memory buffer
  const uint table_size_small = static_cast<uint>(mem_buf_size * 0.19);
  // Table that is larger than 20% but less than 100% of memory buffer
  const uint table_size_medium = mem_buf_size / 2;
  // Table that is larger than memory buffer
  const uint table_size_large = mem_buf_size * 2;

  /*
    Verify that the default table in memory estimate for a handler has been
    correctly initialized.
  */
  EXPECT_EQ(mock_handler.stats.table_in_mem_estimate,
            IN_MEMORY_ESTIMATE_UNKNOWN);

  /*
    Test with a table that is less than 20% of memory buffer. This should
    be entirely in the memory buffer.
  */
  mock_handler.stats.data_file_length = table_size_small;
  EXPECT_EQ(mock_handler.table_in_memory_estimate(), 1.0);

  /*
    Test with a medium sized table that is more than 20% but less than
    100% of the memory buffer size.
  */
  mock_handler.stats.data_file_length = table_size_medium;
  EXPECT_GT(mock_handler.table_in_memory_estimate(), 0.0);
  EXPECT_LT(mock_handler.table_in_memory_estimate(), 1.0);

  /*
    Test with a huge table. This should not be in memory at all.
  */
  mock_handler.stats.data_file_length = table_size_large;
  EXPECT_EQ(mock_handler.table_in_memory_estimate(), 0.0);

  /*
    Simulate that the storage engine has reported that 50 percent of the
    table is in a memory buffer.
  */
  mock_handler.stats.table_in_mem_estimate = 0.5;

  /*
    Set the table size to be less than 20 percent but larger than 10K.
  */
  mock_handler.stats.data_file_length = table_size_small;
  EXPECT_DOUBLE_EQ(mock_handler.table_in_memory_estimate(), 0.5);

  /*
    Set the table size to be larger than 20 percent but less than 100 percent.
  */
  mock_handler.stats.data_file_length = table_size_medium;
  EXPECT_DOUBLE_EQ(mock_handler.table_in_memory_estimate(), 0.5);

  /*
    Set the table size to be larger than the memory buffer.
  */
  mock_handler.stats.data_file_length = table_size_large;
  EXPECT_DOUBLE_EQ(mock_handler.table_in_memory_estimate(), 0.5);
}

TEST_F(HandlerTest, IndexInMemoryEstimate) {
  Mock_field_datetime field_datetime;
  Fake_TABLE *table = static_cast<Fake_TABLE *>(field_datetime.table);
  handlerton *hton = nullptr;
  NiceMock<Mock_HANDLER> mock_handler(hton, table->get_share());
  table->set_handler(&mock_handler);
  mock_handler.change_table_ptr(table, table->get_share());
  const uint key_no = 0;

  // Verify that the handler does not know the buffer size
  EXPECT_EQ(mock_handler.get_memory_buffer_size(), -1);
  /*
    The implementation of index_in_memory_estimate() assumes that the
    memory buffer is 100 MB if the storage engine does not report the
    size of its memory buffer.
  */
  const uint mem_buf_size = 100 * 1024 * 1024;

  /*
    Define representative table and index sizes to use in tests.
  */
  // Index that is less than 20% of memory buffer
  const uint index_size_small = static_cast<uint>(mem_buf_size * 0.19);
  // Index that is larger than 20% but less than 100% of memory buffer
  const uint index_size_medium = mem_buf_size / 2;
  // Index that is larger than memory buffer
  const uint index_size_large = mem_buf_size * 2;

  // Initialize the estimate for how much of the index that is in memory
  table->key_info[key_no].set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);

  /*
    Test with an index that is less than 20% of memory buffer. This should
    be entirely in the memory buffer.
  */
  mock_handler.stats.index_file_length = index_size_small;
  EXPECT_EQ(mock_handler.index_in_memory_estimate(key_no), 1.0);

  /*
    Test with a medium sized index that is more than 20% but less than
    100% of the memory buffer size.
  */
  mock_handler.stats.index_file_length = index_size_medium;
  EXPECT_GT(mock_handler.index_in_memory_estimate(key_no), 0.0);
  EXPECT_LT(mock_handler.index_in_memory_estimate(key_no), 1.0);

  /*
    Test with a huge index. This should not be in memory at all.
  */
  mock_handler.stats.index_file_length = index_size_large;
  EXPECT_EQ(mock_handler.index_in_memory_estimate(key_no), 0.0);

  /*
    Simulate that the storage engine has reported that 50 percent of the
    index is in a memory buffer.
  */
  table->key_info[key_no].set_in_memory_estimate(0.5);

  /*
    Set the index size to be less than 20 percent but larger than 10K.
  */
  mock_handler.stats.index_file_length = index_size_small;
  EXPECT_DOUBLE_EQ(mock_handler.index_in_memory_estimate(key_no), 0.5);

  /*
    Set the index size to be larger than 20 percent but less than 100 percent.
  */
  mock_handler.stats.index_file_length = index_size_medium;
  EXPECT_DOUBLE_EQ(mock_handler.index_in_memory_estimate(key_no), 0.5);

  /*
    Set the index size to be larger than the memory buffer.
  */
  mock_handler.stats.index_file_length = index_size_large;
  EXPECT_DOUBLE_EQ(mock_handler.index_in_memory_estimate(key_no), 0.5);
}

TEST_F(HandlerTest, SamplingInterfaceAllRows) {
  Mock_field_datetime field_datetime;
  Fake_TABLE *table = static_cast<Fake_TABLE *>(field_datetime.table);
  StrictMock<Mock_SAMPLING_HANDLER> mock_handler(nullptr, table,
                                                 table->get_share());
  table->set_handler(&mock_handler);

  uchar buffer[8];
  void *scan_ctx = nullptr;

  // rnd_init should be called exactly one time by ha_sample_init.
  EXPECT_CALL(mock_handler, rnd_init(true)).Times(1);
  EXPECT_EQ(mock_handler.ha_sample_init(scan_ctx, 100.0, 0,
                                        enum_sampling_method::SYSTEM, false),
            0);
  EXPECT_EQ(mock_handler.inited, handler::SAMPLING);

  /*
    Since we have set the sampling rate to 100%, all rows should be returned.
    Thus, rnd_next should be called exactly as many times as ha_sample_next().
  */
  const int num_iterations = 100;
  EXPECT_CALL(mock_handler, rnd_next(buffer)).Times(num_iterations);

  for (int i = 0; i < num_iterations; ++i)
    mock_handler.ha_sample_next(scan_ctx, buffer);

  // rnd_end should be called exactly one time by ha_sample_end.
  EXPECT_CALL(mock_handler, rnd_end()).Times(1);
  EXPECT_EQ(mock_handler.ha_sample_end(scan_ctx), 0);
  EXPECT_EQ(mock_handler.inited, handler::NONE);
}

TEST_F(HandlerTest, SamplingInterfaceNoRows) {
  Mock_field_datetime field_datetime;
  Fake_TABLE *table = reinterpret_cast<Fake_TABLE *>(field_datetime.table);
  StrictMock<Mock_SAMPLING_HANDLER> mock_handler(nullptr, table,
                                                 table->get_share());
  table->set_handler(&mock_handler);

  uchar buffer[8];
  void *scan_ctx = nullptr;

  // rnd_init should be called exactly one time by ha_sample_init.
  EXPECT_CALL(mock_handler, rnd_init(true)).Times(1);
  EXPECT_EQ(mock_handler.ha_sample_init(scan_ctx, 0.0, 0,
                                        enum_sampling_method::SYSTEM, false),
            0);
  EXPECT_EQ(mock_handler.inited, handler::SAMPLING);

  /*
    Since we have set the sampling rate to 0%, no rows should be returned. Thus,
    rnd_next should never be called.
  */
  EXPECT_CALL(mock_handler, rnd_next(buffer)).Times(0);

  for (int i = 0; i < 100; ++i) mock_handler.ha_sample_next(scan_ctx, buffer);

  // rnd_end should be called exactly one time by ha_sample_end.
  EXPECT_CALL(mock_handler, rnd_end()).Times(1);
  EXPECT_EQ(mock_handler.ha_sample_end(scan_ctx), 0);
  EXPECT_EQ(mock_handler.inited, handler::NONE);
}

}  // namespace

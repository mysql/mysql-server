/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "mysql/plugin.h"
#include "sql/mysqld.h"
#include "storage/temptable/include/temptable/handler.h"
#include "unittest/gunit/temptable/table_helper.h"
#include "unittest/gunit/test_utils.h"

namespace temptable_test {

#ifdef NDEBUG

/* In release builds there will be an error reported as well
as my_error generated. */
#define EXPECT_UPDATE_UNSUPPORTED(x)                                 \
  m_server_initializer.set_expected_error(ER_CHECK_NOT_IMPLEMENTED); \
  EXPECT_EQ(x, HA_ERR_UNSUPPORTED);                                  \
  m_server_initializer.set_expected_error(0);

#else

/* In debug builds there will be an assert. */
#define EXPECT_UPDATE_UNSUPPORTED(x) \
  EXPECT_DEATH_IF_SUPPORTED(x, ".*Assertion .*");

#endif

// To use a test fixture, derive a class from testing::Test.
class Handler_test : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // LOCK_plugin is initialized in setup_server_for_unit_tests().
    // Destroy it here, before re-initializing in plugin_early_load_one().
    mysql_mutex_destroy(&LOCK_plugin);
    plugin_early_load_one(
        nullptr, nullptr,
        nullptr);  // a hack which is needed to at least get
                   // LOCK_plugin_xxx mutexes initialized in order make this
                   // test-suite up and running again.
  }
  static void TearDownTestCase() {
    plugin_shutdown();  // see a comment in SetUpTestCase() for a reason why
                        // this is needed
    cleanup_global_system_variables();
  }
  void SetUp() override {
    init_handlerton();
    m_server_initializer.SetUp();
  }

  void TearDown() override {
    m_server_initializer.TearDown();
    delete remove_hton2plugin(m_temptable_handlerton.slot);
  }

  THD *thd() { return m_server_initializer.thd(); }

  handlerton *hton() { return &m_temptable_handlerton; }

 protected:
  my_testing::Server_initializer m_server_initializer;

 private:
  handlerton m_temptable_handlerton;

  void init_handlerton() {
    m_temptable_handlerton = handlerton();

    m_temptable_handlerton.file_extensions = nullptr;
    m_temptable_handlerton.state = SHOW_OPTION_YES;
    m_temptable_handlerton.db_type = DB_TYPE_TEMPTABLE;
    m_temptable_handlerton.create = nullptr;
    m_temptable_handlerton.flags =
        HTON_ALTER_NOT_SUPPORTED | HTON_CAN_RECREATE | HTON_HIDDEN |
        HTON_NOT_USER_SELECTABLE | HTON_NO_PARTITION | HTON_NO_BINLOG_ROW_OPT |
        HTON_SUPPORTS_EXTENDED_KEYS;

    insert_hton2plugin(m_temptable_handlerton.slot, new st_plugin_int());
    temptable::Allocator<uint8_t>::init();
  }
};

TEST_F(Handler_test, SimpleTableCreate) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

#ifndef NDEBUG
#ifndef _WIN32
TEST_F(
    Handler_test,
    TableCreateReturnsRecordFileFullWhenTempTableAllocatorThrowsRecordFileFull) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  DBUG_SET("+d,temptable_allocator_record_file_full");
  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            HA_ERR_RECORD_FILE_FULL);
  DBUG_SET("-d,temptable_allocator_record_file_full");
}
#endif /* _WIN32 */

#ifndef _WIN32
TEST_F(Handler_test,
       TableCreateReturnsOutOfMemoryWhenTempTableAllocatorThrowsOutOfMemory) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  DBUG_SET("+d,temptable_allocator_oom");
  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            HA_ERR_OUT_OF_MEM);
  DBUG_SET("-d,temptable_allocator_oom");
}
#endif /* _WIN32 */

TEST_F(Handler_test,
       TableCreateReturnsOutOfMemoryWhenCatchAllHandlerIsActivated) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  DBUG_SET("+d,temptable_create_return_non_result_type_exception");
  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            HA_ERR_OUT_OF_MEM);
  DBUG_SET("-d,temptable_create_return_non_result_type_exception");
}
#endif /* NDEBUG */

TEST_F(Handler_test, SimpleTableOpsFixedSize) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.add_field_long("col1", true);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_long>(0)->store(1, false);
  table_helper.field<Field_long>(1)->store(1, false);
  table_helper.field<Field_long>(1)->set_notnull();
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(2, false);
  table_helper.field<Field_long>(1)->store(2, false);
  table_helper.field<Field_long>(1)->set_null();
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(3, false);
  table_helper.field<Field_long>(1)->store(1, false);
  table_helper.field<Field_long>(1)->set_null();
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Update one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.field<Field_long>(0)->store(10, false);
  table_helper.field<Field_long>(1)->store(10, false);
  table_helper.field<Field_long>(1)->set_notnull();
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Delete one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  EXPECT_EQ(handler.delete_row(table_helper.record_1()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

TEST_F(Handler_test, SimpleTableOpsVarSize) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_varstring("col0", 20, false);
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_varstring>(0)->store(1, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_varstring>(0)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_varstring>(0)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Update one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.field<Field_varstring>(0)->store(10, false);
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Delete one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  EXPECT_EQ(handler.delete_row(table_helper.record_1()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

TEST_F(Handler_test, SingleIndex) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.add_field_long("col1", false);
  table_helper.add_index(HA_KEY_ALG_HASH, true, {0});
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_long>(0)->store(1, false);
  table_helper.field<Field_long>(1)->store(1, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(2, false);
  table_helper.field<Field_long>(1)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(3, false);
  table_helper.field<Field_long>(1)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Insert (duplicate key). */
  table_helper.field<Field_long>(0)->store(2, false);
  table_helper.field<Field_long>(1)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), HA_ERR_FOUND_DUPP_KEY);

  /* Update (duplicate row) - verify unsupported error/assert is generated. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value = table_helper.field<Field_long>(0)->val_int();
  auto new_value = (old_value == 1) ? 2 : 1;
  table_helper.field<Field_long>(0)->store(new_value, false);
  EXPECT_UPDATE_UNSUPPORTED(
      handler.update_row(table_helper.record_1(), table_helper.record_0()));
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Update (success). */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  table_helper.field<Field_long>(1)->store(10, false);
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Delete one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  EXPECT_EQ(handler.delete_row(table_helper.record_1()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

TEST_F(Handler_test, MultiIndex) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.add_field_long("col1", false);
  table_helper.add_field_long("col2", false);
  table_helper.add_index(HA_KEY_ALG_HASH, true, {0});
  table_helper.add_index(HA_KEY_ALG_BTREE, true, {1});
  table_helper.add_index(HA_KEY_ALG_HASH, false, {0, 1});
  table_helper.add_index(HA_KEY_ALG_BTREE, false, {0, 1});
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_long>(0)->store(1, false);
  table_helper.field<Field_long>(1)->store(1, false);
  table_helper.field<Field_long>(2)->store(1, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(2, false);
  table_helper.field<Field_long>(1)->store(2, false);
  table_helper.field<Field_long>(2)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(3, false);
  table_helper.field<Field_long>(1)->store(3, false);
  table_helper.field<Field_long>(2)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Insert (duplicate key). */
  table_helper.field<Field_long>(0)->store(4, false);
  table_helper.field<Field_long>(1)->store(2, false);
  table_helper.field<Field_long>(2)->store(9, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), HA_ERR_FOUND_DUPP_KEY);

  /* Update (duplicate row) - verify unsupported error/assert is generated. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value1 = table_helper.field<Field_long>(1)->val_int();
  auto new_value1 = (old_value1 == 1) ? 2 : 1;
  table_helper.field<Field_long>(1)->store(new_value1, false);
  EXPECT_UPDATE_UNSUPPORTED(
      handler.update_row(table_helper.record_1(), table_helper.record_0()));
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value2 = table_helper.field<Field_long>(1)->val_int();
  auto new_value2 = (old_value2 == 1) ? 2 : 1;
  table_helper.field<Field_long>(0)->store(100, false);
  table_helper.field<Field_long>(1)->store(new_value2, false);
  EXPECT_UPDATE_UNSUPPORTED(
      handler.update_row(table_helper.record_1(), table_helper.record_0()));
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Update (success). */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  table_helper.field<Field_long>(2)->store(99, false);
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Delete one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  EXPECT_EQ(handler.delete_row(table_helper.record_1()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

TEST_F(Handler_test, MultiIndexVarchar) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_varstring("col0", 20, false);
  table_helper.add_field_varstring("col1", 20, false);
  table_helper.add_field_varstring("col2", 20, false);
  table_helper.add_index(HA_KEY_ALG_HASH, true, {0});
  table_helper.add_index(HA_KEY_ALG_BTREE, true, {1});
  table_helper.add_index(HA_KEY_ALG_HASH, false, {0, 1});
  table_helper.add_index(HA_KEY_ALG_BTREE, false, {0, 1});
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_varstring>(0)->store(1, false);
  table_helper.field<Field_varstring>(1)->store(1, false);
  table_helper.field<Field_varstring>(2)->store(1, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_varstring>(0)->store(2, false);
  table_helper.field<Field_varstring>(1)->store(2, false);
  table_helper.field<Field_varstring>(2)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_varstring>(0)->store(3, false);
  table_helper.field<Field_varstring>(1)->store(3, false);
  table_helper.field<Field_varstring>(2)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Insert (duplicate key). */
  table_helper.field<Field_varstring>(0)->store(4, false);
  table_helper.field<Field_varstring>(1)->store(2, false);
  table_helper.field<Field_varstring>(2)->store(9, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), HA_ERR_FOUND_DUPP_KEY);

  /* Update (duplicate row) - verify unsupported error/assert is generated. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value1 = table_helper.field<Field_varstring>(1)->val_int();
  auto new_value1 = (old_value1 == 1) ? 2 : 1;
  table_helper.field<Field_varstring>(1)->store(new_value1, false);
  EXPECT_UPDATE_UNSUPPORTED(
      handler.update_row(table_helper.record_1(), table_helper.record_0()));
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value2 = table_helper.field<Field_varstring>(1)->val_int();
  auto new_value2 = (old_value2 == 1) ? 2 : 1;
  table_helper.field<Field_varstring>(0)->store(100, false);
  table_helper.field<Field_varstring>(1)->store(new_value2, false);
  EXPECT_UPDATE_UNSUPPORTED(
      handler.update_row(table_helper.record_1(), table_helper.record_0()));
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Update (success). */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  table_helper.field<Field_varstring>(2)->store(99, false);
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Delete one row. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  EXPECT_EQ(handler.delete_row(table_helper.record_1()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

TEST_F(Handler_test, IndexOnOff) {
  const char *table_name = "t1";

  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("col0", false);
  table_helper.add_index(HA_KEY_ALG_HASH, true, {0});
  table_helper.finalize();

  temptable::Handler handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);
  EXPECT_EQ(handler.open(table_name, 0, 0, nullptr), 0);

  /* Insert (success). */
  table_helper.field<Field_long>(0)->store(1, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Insert (duplicate key). */
  table_helper.field<Field_long>(0)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), HA_ERR_FOUND_DUPP_KEY);

  /* Disable indexes. */
  EXPECT_EQ(handler.disable_indexes(HA_KEY_SWITCH_ALL), 0);

  /* Update (duplicate row) - should succeed. */
  EXPECT_EQ(handler.rnd_init(false), 0);
  EXPECT_EQ(handler.rnd_next(table_helper.record_1()), 0);
  table_helper.copy_record_1_to_0();
  auto old_value = table_helper.field<Field_long>(0)->val_int();
  auto new_value = (old_value == 1) ? 2 : 1;
  table_helper.field<Field_long>(0)->store(new_value, false);
  EXPECT_EQ(
      handler.update_row(table_helper.record_1(), table_helper.record_0()), 0);
  EXPECT_EQ(handler.rnd_end(), 0);

  /* Insert (duplicate key) - should succeed. */
  table_helper.field<Field_long>(0)->store(3, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  /* Enable indexes (should fail, table not empty). */
  EXPECT_EQ(handler.enable_indexes(HA_KEY_SWITCH_ALL), HA_ERR_WRONG_COMMAND);

  /* Truncate table. */
  EXPECT_EQ(handler.truncate(nullptr), 0);

  /* Enable indexes (should succeed). */
  EXPECT_EQ(handler.enable_indexes(HA_KEY_SWITCH_ALL), 0);

  /* Insert & check for duplicate */
  table_helper.field<Field_long>(0)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), 0);

  table_helper.field<Field_long>(0)->store(2, false);
  EXPECT_EQ(handler.write_row(table_helper.record_0()), HA_ERR_FOUND_DUPP_KEY);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table(table_name, nullptr), 0);
}

}  // namespace temptable_test

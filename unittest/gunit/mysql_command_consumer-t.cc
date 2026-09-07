/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <cstring>
#include <initializer_list>
#include <string>

#include "my_alloc.h"
#include "sql/mysqld.h"
#include "sql/server_component/mysql_command_backend.h"
#include "sql/server_component/mysql_command_consumer_imp.h"
#include "sql/server_component/mysql_command_services_imp.h"

namespace mysql_command_consumer_unittest {

/*
  Verify that field_metadata() produces libmysql-compatible MYSQL_FIELD string
  metadata: catalog is "def", and each copied string has its byte length in the
  corresponding length member.
*/
TEST(MysqlCommandConsumerTest, FieldMetadataPopulatesCatalogAndLengths) {
  // Build the minimal DOM result context required by field_metadata().
  MEM_ROOT row_mem_root;
  MEM_ROOT field_mem_root;
  MYSQL_DATA result{};
  result.alloc = &row_mem_root;
  MYSQL_DATA *result_ptr = &result;

  MYSQL mysql{};
  mysql.field_alloc = &field_mem_root;
  MYSQL_FIELD mysql_field{};
  Dom_ctx ctx{};
  ctx.m_mysql = &mysql;
  ctx.m_result = &result_ptr;
  ctx.m_fields = &mysql_field;

  Field_metadata field{.db_name = "database",
                       .table_name = "table_alias",
                       .org_table_name = "table_name",
                       .col_name = "column_alias",
                       .org_col_name = "column_name",
                       .length = 42,
                       .charsetnr = 255,
                       .flags = 0,
                       .decimals = 0,
                       .type = MYSQL_TYPE_LONG};

  EXPECT_FALSE(mysql_command_consumer_dom_imp::field_metadata(
      reinterpret_cast<SRV_CTX_H>(&ctx), &field, nullptr));

  EXPECT_STREQ("def", mysql_field.catalog);
  EXPECT_EQ(3U, mysql_field.catalog_length);
  EXPECT_STREQ(field.db_name, mysql_field.db);
  EXPECT_EQ(std::strlen(field.db_name), mysql_field.db_length);
  EXPECT_STREQ(field.table_name, mysql_field.table);
  EXPECT_EQ(std::strlen(field.table_name), mysql_field.table_length);
  EXPECT_STREQ(field.org_table_name, mysql_field.org_table);
  EXPECT_EQ(std::strlen(field.org_table_name), mysql_field.org_table_length);
  EXPECT_STREQ(field.col_name, mysql_field.name);
  EXPECT_EQ(std::strlen(field.col_name), mysql_field.name_length);
  EXPECT_STREQ(field.org_col_name, mysql_field.org_name);
  EXPECT_EQ(std::strlen(field.org_col_name), mysql_field.org_name_length);
}

class MysqlCommandAuthenticatedOptionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    saved_bind_address_ = my_bind_addr_str;
    saved_disable_networking_ = opt_disable_networking;
    saved_mysqld_port_ = mysqld_port;
    // Ensure endpoint validation cannot mask the option-file rejection.
    opt_disable_networking = false;
    mysqld_port = 1;

    mysql_handle_.mysql = mysql_init(nullptr);
    ASSERT_NE(nullptr, mysql_handle_.mysql);
    mysql_handle_.client_methods = mysql_handle_.mysql->methods;

    ASSERT_FALSE(mysql_command_services_imp::set(
        reinterpret_cast<MYSQL_H>(&mysql_handle_), MYSQL_COMMAND_USER_NAME,
        "mcs_auth_user"));
    ASSERT_FALSE(mysql_command_services_imp::set(
        reinterpret_cast<MYSQL_H>(&mysql_handle_), MYSQL_COMMAND_PASSWORD,
        "mcs_auth_password"));
  }

  void TearDown() override {
    if (mysql_handle_.mysql != nullptr) mysql_close(mysql_handle_.mysql);
    my_bind_addr_str = saved_bind_address_;
    opt_disable_networking = saved_disable_networking_;
    mysqld_port = saved_mysqld_port_;
  }

  std::string bind_address_;
  Mysql_handle mysql_handle_;
  char *saved_bind_address_ = nullptr;
  bool saved_disable_networking_ = false;
  uint saved_mysqld_port_ = 0;
};

TEST_F(MysqlCommandAuthenticatedOptionsTest, RejectsReadDefaultFile) {
  constexpr char default_file[] = "my";
  ASSERT_FALSE(
      mysql_command_services_imp::set(reinterpret_cast<MYSQL_H>(&mysql_handle_),
                                      MYSQL_READ_DEFAULT_FILE, default_file));

  EXPECT_TRUE(mysql_command_services_imp::connect(
      reinterpret_cast<MYSQL_H>(&mysql_handle_)));
  EXPECT_EQ(CR_INVALID_PARAMETER_NO, mysql_errno(mysql_handle_.mysql));
}

TEST_F(MysqlCommandAuthenticatedOptionsTest, RejectsReadDefaultGroup) {
  constexpr char default_group[] = "mcs_auth_defaults";
  ASSERT_FALSE(
      mysql_command_services_imp::set(reinterpret_cast<MYSQL_H>(&mysql_handle_),
                                      MYSQL_READ_DEFAULT_GROUP, default_group));

  EXPECT_TRUE(mysql_command_services_imp::connect(
      reinterpret_cast<MYSQL_H>(&mysql_handle_)));
  EXPECT_EQ(CR_INVALID_PARAMETER_NO, mysql_errno(mysql_handle_.mysql));
}

TEST_F(MysqlCommandAuthenticatedOptionsTest,
       RejectsLoopbackForNonmatchingBindAddress) {
  bind_address_ = "192.0.2.1";
  my_bind_addr_str = bind_address_.data();
  ASSERT_FALSE(
      mysql_command_services_imp::set(reinterpret_cast<MYSQL_H>(&mysql_handle_),
                                      MYSQL_COMMAND_HOST_NAME, "127.0.0.1"));

  EXPECT_TRUE(mysql_command_services_imp::connect(
      reinterpret_cast<MYSQL_H>(&mysql_handle_)));
  EXPECT_EQ(CR_INVALID_PARAMETER_NO, mysql_errno(mysql_handle_.mysql));
}

TEST(MysqlCommandBindAddressTest, ValidatesLiteralMatrix) {
  struct Test_case {
    const char *bind_address;
    bool use_ipv4;
    bool expected;
  };

  constexpr Test_case test_cases[] = {
      {"*", true, true},
      {"*", false, true},
      {"127.0.0.1", true, true},
      {"0.0.0.0", true, true},
      {"::1", false, true},
      {"::", false, true},
      {"127.0.0.1", false, false},
      {"0.0.0.0", false, false},
      {"::1", true, false},
      {"::", true, false},
      {"localhost", true, false},
      {"localhost", false, false},
      {"127.0.0.1,192.0.2.10", true, false},
      {"::1,2001:db8::1", false, false},
      {"192.0.2.1", true, false},
      {nullptr, true, false},
      {nullptr, false, false},
  };

  for (const auto &test_case : test_cases) {
    EXPECT_EQ(test_case.expected,
              mysql_command_services_imp::bind_address_accepts_loopback(
                  test_case.bind_address, test_case.use_ipv4))
        << "bind_address="
        << (test_case.bind_address == nullptr ? "<null>"
                                              : test_case.bind_address)
        << ", use_ipv4=" << test_case.use_ipv4;
  }
}

class MysqlCommandUseResultTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mysql_ = mysql_init(nullptr);
    ASSERT_NE(nullptr, mysql_);
    mysql_->methods = &cs::mysql_methods;

    Mysql_handle mysql_handle{mysql_};
    ASSERT_FALSE(mysql_command_consumer_dom_imp::start(
        &srv_ctx_, reinterpret_cast<MYSQL_H *>(&mysql_handle)));
  }

  void TearDown() override {
    if (result_ != nullptr) mysql_free_result(result_);
    if (srv_ctx_ != nullptr) mysql_command_consumer_dom_imp::end(srv_ctx_);
    mysql_close(mysql_);
  }

  bool materialize_rows(std::initializer_list<long long> values) {
    if (mysql_command_consumer_dom_imp::start_result_metadata(srv_ctx_, 1, 0,
                                                              nullptr))
      return true;

    Field_metadata field{.db_name = "database",
                         .table_name = "table_alias",
                         .org_table_name = "table_name",
                         .col_name = "column_alias",
                         .org_col_name = "column_name",
                         .length = 42,
                         .charsetnr = 255,
                         .flags = 0,
                         .decimals = 0,
                         .type = MYSQL_TYPE_LONG};
    if (mysql_command_consumer_dom_imp::field_metadata(srv_ctx_, &field,
                                                       nullptr))
      return true;

    if (mysql_command_consumer_dom_imp::end_result_metadata(srv_ctx_, 0, 0))
      return true;

    for (long long value : values) {
      if (mysql_command_consumer_dom_imp::start_row(srv_ctx_) ||
          mysql_command_consumer_dom_imp::get(srv_ctx_, value) ||
          mysql_command_consumer_dom_imp::end_row(srv_ctx_))
        return true;
    }

    return false;
  }

  bool materialize_next_command_rows(std::initializer_list<long long> values) {
    // Match csi_advanced_command() replacing the default consumer at the next
    // accepted command boundary.
    mysql_command_consumer_dom_imp::end(srv_ctx_);
    srv_ctx_ = nullptr;
    free_old_query(mysql_);

    Mysql_handle mysql_handle{mysql_};
    if (mysql_command_consumer_dom_imp::start(
            &srv_ctx_, reinterpret_cast<MYSQL_H *>(&mysql_handle)))
      return true;

    return materialize_rows(values);
  }

  MYSQL *mysql_ = nullptr;
  SRV_CTX_H srv_ctx_ = nullptr;
  MYSQL_RES *result_ = nullptr;
};

TEST_F(MysqlCommandUseResultTest,
       RejectsCommandWhileMaterializedResultIsActive) {
  ASSERT_FALSE(materialize_rows({1}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  EXPECT_EQ(nullptr, result_->data);
  EXPECT_EQ(nullptr, result_->data_cursor);
  EXPECT_NE(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_NE(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);
  EXPECT_EQ(0U, result_->row_count);
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);

  constexpr char query[] = "SELECT 2";
  EXPECT_TRUE(cs::csi_advanced_command(mysql_, COM_QUERY, nullptr, 0,
                                       reinterpret_cast<const uchar *>(query),
                                       sizeof(query) - 1, true, nullptr));
  EXPECT_EQ(CR_COMMANDS_OUT_OF_SYNC, mysql_errno(mysql_));
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);
  EXPECT_EQ(srv_ctx_, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->consumer_srv_data);

  MYSQL_ROW row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("1", row[0]);
  unsigned long *lengths = mysql_fetch_lengths(result_);
  ASSERT_NE(nullptr, lengths);
  EXPECT_EQ(1U, lengths[0]);
  EXPECT_EQ(1U, result_->row_count);
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);

  EXPECT_EQ(nullptr, mysql_fetch_row(result_));
  EXPECT_TRUE(result_->eof);
  EXPECT_EQ(nullptr, result_->handle);
  EXPECT_EQ(nullptr, mysql_->unbuffered_fetch_owner);
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);

  MYSQL_RES *first_result = result_;
  ASSERT_FALSE(materialize_next_command_rows({2}));
  MYSQL_RES *next_result = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, next_result);
  result_ = next_result;
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);

  mysql_free_result(first_result);
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);
  row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("2", row[0]);
}

TEST_F(MysqlCommandUseResultTest,
       RejectsFactoryReplacementWhileUseResultIsActive) {
  ASSERT_FALSE(materialize_rows({1, 2}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql_);
  ASSERT_NE(nullptr, mcs_extn->data);
  ASSERT_NE(nullptr, mcs_extn->use_result_cursor);

  // Replacing the consumer would end the owner of the active result's rows.
  Mysql_handle mysql_handle{mysql_};
  EXPECT_TRUE(
      mysql_command_services_imp::set(reinterpret_cast<MYSQL_H>(&mysql_handle),
                                      MYSQL_TEXT_CONSUMER_FACTORY, nullptr));
  EXPECT_EQ(CR_COMMANDS_OUT_OF_SYNC, mysql_errno(mysql_));
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);
  EXPECT_EQ(srv_ctx_, mcs_extn->consumer_srv_data);
  EXPECT_NE(nullptr, mcs_extn->data);
  EXPECT_NE(nullptr, mcs_extn->use_result_cursor);

  MYSQL_ROW row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("1", row[0]);

  mysql_free_result(result_);
  result_ = nullptr;
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
  EXPECT_EQ(nullptr, mcs_extn->data);
  EXPECT_EQ(nullptr, mcs_extn->use_result_cursor);
}

TEST_F(MysqlCommandUseResultTest, ConsumerEndClearsUseResultStorage) {
  ASSERT_FALSE(materialize_rows({1}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  auto *mcs_extn = MYSQL_COMMAND_SERVICE_EXTN(mysql_);
  ASSERT_NE(nullptr, mcs_extn->data);
  ASSERT_NE(nullptr, mcs_extn->use_result_cursor);

  // Simulate teardown ending the consumer before MYSQL_RES is released.
  mysql_command_consumer_dom_imp::end(srv_ctx_);
  srv_ctx_ = nullptr;
  EXPECT_EQ(nullptr, mcs_extn->data);
  EXPECT_EQ(nullptr, mcs_extn->use_result_cursor);

  // Later result cleanup must not observe the released DOM storage.
  mysql_free_result(result_);
  result_ = nullptr;
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
}

TEST_F(MysqlCommandUseResultTest, HandlesEmptyResult) {
  ASSERT_FALSE(materialize_rows({}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  EXPECT_EQ(nullptr, result_->data);
  EXPECT_EQ(nullptr, result_->data_cursor);
  EXPECT_NE(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);

  EXPECT_EQ(nullptr, mysql_fetch_row(result_));
  EXPECT_EQ(0U, result_->row_count);
  EXPECT_TRUE(result_->eof);
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);
}

TEST_F(MysqlCommandUseResultTest, OwnsFieldMetadataUntilResultIsFreed) {
  ASSERT_FALSE(materialize_rows({1}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  MYSQL_FIELD *fields = mysql_fetch_fields(result_);
  ASSERT_NE(nullptr, fields);
  ASSERT_TRUE(result_->field_alloc->Contains(fields[0].catalog));
  ASSERT_TRUE(result_->field_alloc->Contains(fields[0].name));

  ASSERT_NE(nullptr, mysql_fetch_row(result_));
  EXPECT_EQ(nullptr, mysql_fetch_row(result_));
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_STREQ("def", fields[0].catalog);
  EXPECT_STREQ("column_alias", fields[0].name);
}

TEST_F(MysqlCommandUseResultTest, FreeAfterPartialFetchReleasesHandle) {
  ASSERT_FALSE(materialize_rows({1, 2}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  ASSERT_NE(nullptr, mysql_fetch_row(result_));
  EXPECT_EQ(MYSQL_STATUS_USE_RESULT, mysql_->status);

  mysql_free_result(result_);
  result_ = nullptr;
  EXPECT_EQ(nullptr, mysql_->unbuffered_fetch_owner);
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);

  ASSERT_FALSE(materialize_next_command_rows({3}));
  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  MYSQL_ROW row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("3", row[0]);
}

TEST_F(MysqlCommandUseResultTest, UseResultDoesNotExposeRowsForSeek) {
  ASSERT_FALSE(materialize_rows({1, 2}));

  result_ = mysql_use_result(mysql_);
  ASSERT_NE(nullptr, result_);
  EXPECT_EQ(nullptr, result_->data);
  EXPECT_EQ(nullptr, result_->data_cursor);

  char dummy_value[] = "dummy";
  char *dummy_columns[] = {dummy_value, nullptr};
  MYSQL_ROWS dummy_row{};
  dummy_row.data = dummy_columns;
  // A public cursor must not override the private use_result cursor.
  result_->data_cursor = &dummy_row;

  MYSQL_ROW row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("1", row[0]);
  unsigned long *lengths = mysql_fetch_lengths(result_);
  ASSERT_NE(nullptr, lengths);
  EXPECT_EQ(1U, lengths[0]);
  EXPECT_EQ(nullptr, result_->data_cursor);

  row = mysql_fetch_row(result_);
  ASSERT_NE(nullptr, row);
  EXPECT_STREQ("2", row[0]);
  lengths = mysql_fetch_lengths(result_);
  ASSERT_NE(nullptr, lengths);
  EXPECT_EQ(1U, lengths[0]);
  EXPECT_EQ(2U, result_->row_count);

  EXPECT_EQ(nullptr, mysql_fetch_row(result_));
  EXPECT_TRUE(result_->eof);
  EXPECT_EQ(MYSQL_STATUS_READY, mysql_->status);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->data);
  EXPECT_EQ(nullptr, MYSQL_COMMAND_SERVICE_EXTN(mysql_)->use_result_cursor);

  // An exhausted unbuffered result must not follow a stale public cursor.
  result_->data_cursor = &dummy_row;
  EXPECT_EQ(nullptr, mysql_fetch_row(result_));
  EXPECT_EQ(2U, result_->row_count);
}

}  // namespace mysql_command_consumer_unittest

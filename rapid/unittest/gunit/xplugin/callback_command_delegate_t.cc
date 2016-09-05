/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "callback_command_delegate.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>


namespace xpl
{
namespace test
{
namespace
{

const longlong   EXPECTED_VALUE_INTEGER = 1;
const longlong   EXPECTED_VALUE_LONGLONG = 2;
const longlong   EXPECTED_IS_LONGLONG_UNSIGNED = true;
const longlong   EXPECTED_IS_LONGLONG_UNSIGNED_DEFAULT = false;
const decimal_t  EXPECTED_VALUE_DECIMAL = { 0, 1, 2, FALSE, NULL };
const double     EXPECTED_VALUE_DOUBLE = 20.0;
const MYSQL_TIME EXPECTED_VALUE_DATATIME = { 2016, 12, 20, 20, 30, 00, 0, 0, MYSQL_TIMESTAMP_DATETIME };
const char      *EXPECTED_VALUE_STRING = "TEST STRING";

} // namespace


using namespace ::testing;

class Mock_callback_commands
{
public:
    MOCK_METHOD0(start_row, Callback_command_delegate::Row_data *());
    MOCK_METHOD1(end_row, bool (Callback_command_delegate::Row_data *));
};


MATCHER_P(Eq_mysql_time, n, "")
{
  if ((arg.year != n.year) ||
      (arg.month != n.month) ||
      (arg.day != n.day) ||
      (arg.hour != n.hour) ||
      (arg.minute != n.minute) ||
      (arg.second != n.second) ||
      (arg.second_part != n.second_part) ||
      (arg.neg != n.neg) ||
      (arg.time_type != n.time_type))
    return false;

  return true;
}

MATCHER_P(Eq_decimal, n, "")
{
  if ((arg.intg != n.intg) ||
      (arg.frac != n.frac) ||
      (arg.len != n.len) ||
      (arg.sign != n.sign) ||
      (arg.buf != n.buf))
    return false;

  return true;
}


class Callback_command_delegate_testsuite: public Test
{
public:
  void SetUp()
  {
    m_sut.reset(new Callback_command_delegate());
  }

  void create_sut_with_callback_mock()
  {
    Callback_command_delegate::Start_row_callback start_row =
        ngs::bind(&Mock_callback_commands::start_row, &m_mock_callbacks);
    Callback_command_delegate::End_row_callback end_row =
        ngs::bind(&Mock_callback_commands::end_row, &m_mock_callbacks, ngs::placeholders::_1);

    m_sut.reset(new Callback_command_delegate(start_row, end_row));
  }

  void assert_row_and_data_functions(const bool expected_result)
  {
    const int expect_success = 0;

    // Processing of data should be always successful
    // it doesn't depend on result of start_row !
    ASSERT_EQ(expected_result, m_sut->start_row());
    ASSERT_EQ(expect_success, m_sut->get_null());
    ASSERT_EQ(expect_success, m_sut->get_integer(EXPECTED_VALUE_INTEGER));
    ASSERT_EQ(expect_success, m_sut->get_longlong(EXPECTED_VALUE_LONGLONG,
                                                        EXPECTED_IS_LONGLONG_UNSIGNED));
    ASSERT_EQ(expect_success, m_sut->get_decimal(&EXPECTED_VALUE_DECIMAL));
    ASSERT_EQ(expect_success, m_sut->get_double(EXPECTED_VALUE_DOUBLE, 0));
    ASSERT_EQ(expect_success, m_sut->get_date(&EXPECTED_VALUE_DATATIME));
    ASSERT_EQ(expect_success, m_sut->get_time(&EXPECTED_VALUE_DATATIME, 0));
    ASSERT_EQ(expect_success, m_sut->get_datetime(&EXPECTED_VALUE_DATATIME, 0));
    ASSERT_EQ(expect_success, m_sut->get_string(EXPECTED_VALUE_STRING, strlen(EXPECTED_VALUE_STRING), NULL));
    ASSERT_EQ(expected_result, m_sut->end_row());
  }

  void assert_sut_status_should_be_empty()
  {
    EXPECT_EQ(0, m_sut->server_status());
    EXPECT_EQ(0, m_sut->statement_warn_count());
    EXPECT_EQ(0, m_sut->affected_rows());
    EXPECT_EQ(0, m_sut->last_insert_id());
    EXPECT_EQ("", m_sut->message());
  }

  void assert_sut_handle_ok_and_its_status()
  {
    const uint expected_status = 1;
    const uint expected_wrn_count = 2;
    const ulonglong expected_affected_rows = 3;
    const ulonglong expected_last_inserted_id = 4;
    const std::string expected_message = "Test message";

    m_sut->handle_ok(expected_status, expected_wrn_count,
                     expected_affected_rows, expected_last_inserted_id,
                     expected_message.c_str());

    ASSERT_EQ(expected_status, m_sut->server_status());
    ASSERT_EQ(expected_wrn_count, m_sut->statement_warn_count());
    ASSERT_EQ(expected_affected_rows, m_sut->affected_rows());
    ASSERT_EQ(expected_last_inserted_id, m_sut->last_insert_id());
    ASSERT_EQ(expected_message, m_sut->message());
  }

  void assert_row_container(Callback_command_delegate::Row_data &row_data)
  {
    // assert_row_and_data_functions
    const std::size_t expected_size_inserted_by_testsuite = 9;

    ASSERT_EQ(expected_size_inserted_by_testsuite, row_data.fields.size());

    ASSERT_THAT(row_data.fields,
               ElementsAre(IsNull(), NotNull(), NotNull(), NotNull(), NotNull(),
               NotNull(), NotNull(), NotNull(), NotNull()));

    ASSERT_EQ(EXPECTED_VALUE_INTEGER, row_data.fields[1]->value.v_long);
    ASSERT_EQ(EXPECTED_IS_LONGLONG_UNSIGNED_DEFAULT, row_data.fields[1]->is_unsigned);
    ASSERT_FALSE(row_data.fields[1]->is_string);

    ASSERT_EQ(EXPECTED_VALUE_LONGLONG, row_data.fields[2]->value.v_long);
    ASSERT_EQ(EXPECTED_IS_LONGLONG_UNSIGNED, row_data.fields[2]->is_unsigned);
    ASSERT_FALSE(row_data.fields[2]->is_string);

    ASSERT_THAT(row_data.fields[3]->value.v_decimal, Eq_decimal(EXPECTED_VALUE_DECIMAL));
    ASSERT_FALSE(row_data.fields[3]->is_string);

    ASSERT_EQ(EXPECTED_VALUE_DOUBLE, row_data.fields[4]->value.v_double);
    ASSERT_FALSE(row_data.fields[4]->is_string);

    ASSERT_THAT(row_data.fields[5]->value.v_time, Eq_mysql_time(EXPECTED_VALUE_DATATIME));
    ASSERT_FALSE(row_data.fields[5]->is_string);

    ASSERT_THAT(row_data.fields[6]->value.v_time, Eq_mysql_time(EXPECTED_VALUE_DATATIME));
    ASSERT_FALSE(row_data.fields[6]->is_string);

    ASSERT_THAT(row_data.fields[7]->value.v_time, Eq_mysql_time(EXPECTED_VALUE_DATATIME));
    ASSERT_FALSE(row_data.fields[7]->is_string);

    ASSERT_EQ(EXPECTED_VALUE_STRING, *row_data.fields[8]->value.v_string);
    ASSERT_TRUE(row_data.fields[8]->is_string);
  }

  void assert_sut_constant_parameters()
  {
    ASSERT_EQ(CS_TEXT_REPRESENTATION, m_sut->representation());
    ASSERT_EQ(CLIENT_DEPRECATE_EOF, m_sut->get_client_capabilities());
  }

  StrictMock<Mock_callback_commands> m_mock_callbacks;
  ngs::unique_ptr<Command_delegate> m_sut;
};


TEST_F(Callback_command_delegate_testsuite, process_data_without_callback_functions)
{
  const bool expect_success = false;

  ASSERT_NO_FATAL_FAILURE(assert_sut_constant_parameters());
  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());
  ASSERT_NO_FATAL_FAILURE(assert_row_and_data_functions(expect_success));
  ASSERT_NO_FATAL_FAILURE(assert_sut_handle_ok_and_its_status());

  m_sut->reset();
  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());
}

TEST_F(Callback_command_delegate_testsuite, process_data_verify_that_callbacks_are_called_but_container_is_missing)
{
  const bool expect_failure = true;
  Callback_command_delegate::Row_data *expected_container = NULL;

  create_sut_with_callback_mock();

  ASSERT_NO_FATAL_FAILURE(assert_sut_constant_parameters());
  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());

  EXPECT_CALL(m_mock_callbacks, start_row()).WillOnce(Return(expected_container));
  EXPECT_CALL(m_mock_callbacks, end_row(expected_container)).WillOnce(Return(!expect_failure));
  ASSERT_NO_FATAL_FAILURE(assert_row_and_data_functions(expect_failure));

  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());
}

TEST_F(Callback_command_delegate_testsuite, process_data_verify_that_callbacks_are_called_and_data_in_container)
{
  const bool expect_success = false;
  Callback_command_delegate::Row_data expected_container;

  create_sut_with_callback_mock();

  ASSERT_NO_FATAL_FAILURE(assert_sut_constant_parameters());
  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());

  EXPECT_CALL(m_mock_callbacks, start_row()).WillOnce(Return(&expected_container));
  EXPECT_CALL(m_mock_callbacks, end_row(&expected_container)).WillOnce(Return(!expect_success));
  ASSERT_NO_FATAL_FAILURE(assert_row_and_data_functions(expect_success));
  ASSERT_NO_FATAL_FAILURE(assert_row_container(expected_container)); // assert data filled in assert_row_and_data_functions

  ASSERT_NO_FATAL_FAILURE(assert_sut_status_should_be_empty());
}

} // namespace test
} // namespace xpl

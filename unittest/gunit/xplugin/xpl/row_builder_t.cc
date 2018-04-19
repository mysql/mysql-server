/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdio.h>
#include <limits>
#include <set>
#include <string>

#include "decimal.h"
#include "plugin/x/client/mysqlxclient/xdatetime.h"
#include "plugin/x/client/mysqlxclient/xdecimal.h"
#include "plugin/x/client/mysqlxclient/xrow.h"
#include "plugin/x/client/xrow_impl.h"
#include "plugin/x/ngs/include/ngs/protocol/buffer.h"
#include "plugin/x/ngs/include/ngs/protocol/output_buffer.h"
#include "plugin/x/ngs/include/ngs/protocol/row_builder.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "unittest/gunit/xplugin/xpl/protobuf_message.h"

namespace xpl {
namespace test {

typedef ngs::Output_buffer Output_buffer;
typedef ngs::Row_builder Row_builder;
typedef ngs::Page_pool Page_pool;

const ngs::Pool_config default_pool_config = {0, 0, BUFFER_PAGE_SIZE};

static std::vector<ngs::shared_ptr<ngs::Page>> page_del;

static void add_pages(Output_buffer *ob, const size_t no_of_pages,
                      const size_t page_size) {
  for (size_t i = 0; i < no_of_pages; i++) {
    page_del.push_back(ngs::shared_ptr<ngs::Page>(
        new ngs::Page(static_cast<uint32_t>(page_size))));
    ngs::Resource<ngs::Page> page(page_del.back().get());
    ob->push_back(page);
  }
}

template <typename Expected_value_type, typename Method_type>
void assert_row_getter(const Expected_value_type &expected_value,
                       const Method_type &method, const std::string &buffer) {
  Expected_value_type value;

  const bool result = method(buffer, &value);

  ASSERT_TRUE(result);
  ASSERT_EQ(expected_value, value);
}

TEST(row_builder, row_start) {
  Row_builder rb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_null_field();
  rb.add_null_field();

  rb.start_row(obuffer.get());
  rb.end_row();

  ASSERT_EQ(0u, rb.get_num_fields());
  ASSERT_FALSE(NULL == obuffer);
}

TEST(row_builder, row_msg_size) {
  Row_builder rb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));
  add_pages(obuffer.get(), 2, 8);

  rb.start_row(obuffer.get());
  rb.add_null_field();
  rb.end_row();

  int32_t size;
  obuffer->int32_at(0, size);
  // 1 byte for msg tag + 1 byte for field header + 1 byte
  // for field value (NULL)
  ASSERT_EQ(3, size);

  rb.start_row(obuffer.get());
  rb.add_null_field();
  rb.add_null_field();
  rb.end_row();

  // offset of the size is 7 (3 bytes for prev msg + 4 for its size)
  obuffer->int32_at(7, size);
  // 1 byte for msg tag + 2*(1 byte for field header + 1 byte
  // for field value (NULL))
  ASSERT_EQ(5, size);
}

TEST(row_builder, row_abort) {
  Row_builder rb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_null_field();
  rb.add_null_field();

  rb.abort_row();
  ASSERT_EQ(0u, rb.get_num_fields());

  rb.end_row();
}

TEST(row_builder, fields_qty) {
  Row_builder rb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  ASSERT_EQ(0u, rb.get_num_fields());

  rb.add_null_field();
  rb.add_null_field();

  ASSERT_EQ(2u, rb.get_num_fields());

  rb.add_longlong_field(0, true);
  rb.add_float_field(0.0f);
  rb.add_float_field(0.0f);

  ASSERT_EQ(5u, rb.get_num_fields());

  rb.end_row();

  ASSERT_EQ(0u, rb.get_num_fields());
}

TEST(row_builder, null_field) {
  Row_builder rb;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_null_field();

  rb.end_row();

  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  ASSERT_EQ(0u, row->mutable_field(0)->length());
}

TEST(row_builder, unsigned64_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_longlong_field(0, true);
  rb.add_longlong_field(500, true);
  rb.add_longlong_field(10000000, true);
  rb.add_longlong_field(0x7fffffffffffffffLL, true);
  rb.add_longlong_field(1, true);
  rb.add_longlong_field(0xffffffffffffffffLL, true);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(static_cast<uint64_t>(0),
                              &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(500, &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(10000000, &xcl::row_decoder::buffer_to_u64,
                              *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0x7fffffffffffffffULL,
                              &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(1, &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0xffffffffffffffffULL,
                              &xcl::row_decoder::buffer_to_u64, *buffer);
}

TEST(row_builder, signed64_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_longlong_field(0, false);
  rb.add_longlong_field(-500, false);
  rb.add_longlong_field(-10000000, false);
  rb.add_longlong_field(0x7fffffffffffffffLL, false);
  rb.add_longlong_field(-1, false);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  assert_row_getter<int64_t>(0, &xcl::row_decoder::buffer_to_s64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<int64_t>(-500, &xcl::row_decoder::buffer_to_s64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<int64_t>(-10000000, &xcl::row_decoder::buffer_to_s64,
                             *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<int64_t>(0x7fffffffffffffffLL,
                             &xcl::row_decoder::buffer_to_s64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<int64_t>(-1, &xcl::row_decoder::buffer_to_s64, *buffer);
}

TEST(row_builder, float_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_float_field(0.0f);
  rb.add_float_field(0.0001f);
  rb.add_float_field(-10000000.1f);
  rb.add_float_field(9999.91992f);
  rb.add_float_field(std::numeric_limits<float>::min());
  rb.add_float_field(std::numeric_limits<float>::max());

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(0.0f, &xcl::row_decoder::buffer_to_float, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(0.0001f, &xcl::row_decoder::buffer_to_float,
                           *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(-10000000.1f, &xcl::row_decoder::buffer_to_float,
                           *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(9999.91992f, &xcl::row_decoder::buffer_to_float,
                           *buffer);

  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(std::numeric_limits<float>::min(),
                           &xcl::row_decoder::buffer_to_float, *buffer);

  buffer = row->mutable_field(idx++);
  assert_row_getter<float>(std::numeric_limits<float>::max(),
                           &xcl::row_decoder::buffer_to_float, *buffer);
}

TEST(row_builder, double_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_double_field(0.0);
  rb.add_double_field(0.0001);
  rb.add_double_field(-10000000.1);
  rb.add_double_field(9999.91992);
  rb.add_double_field(std::numeric_limits<double>::min());
  rb.add_double_field(std::numeric_limits<double>::max());

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(0.0, &xcl::row_decoder::buffer_to_double, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(0.0001, &xcl::row_decoder::buffer_to_double,
                            *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(-10000000.1, &xcl::row_decoder::buffer_to_double,
                            *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(9999.91992, &xcl::row_decoder::buffer_to_double,
                            *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(std::numeric_limits<double>::min(),
                            &xcl::row_decoder::buffer_to_double, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<double>(std::numeric_limits<double>::max(),
                            &xcl::row_decoder::buffer_to_double, *buffer);
}

TEST(row_builder, string_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  const char *pstr;
  size_t len;
  const char *const STR1 = "ABBABABBBAAA-09-0900--==0,\0\0\0\0\0";
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_string_field("", 0, NULL);
  rb.add_string_field(STR1, strlen(STR1), NULL);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_string(*buffer, &pstr, &len));
  ASSERT_STREQ("", pstr);
  ASSERT_EQ(len, 0u);

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_string(*buffer, &pstr, &len));
  for (size_t i = 0; i < len; i++) {
    ASSERT_EQ(STR1[i], pstr[i]);
  }
  ASSERT_EQ(len, strlen(STR1));
}

TEST(row_builder, date_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.year = 2006;
  time.month = 3;
  time.day = 24;

  rb.start_row(obuffer.get());

  rb.add_date_field(&time);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  xcl::DateTime xtime;
  bool has_time = false;
  ASSERT_TRUE(xcl::row_decoder::buffer_to_datetime(*buffer, &xtime, has_time));
  ASSERT_EQ(time.year, xtime.year());
  ASSERT_EQ(time.month, xtime.month());
  ASSERT_EQ(time.day, xtime.day());
}

TEST(row_builder, time_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.neg = false;
  time.hour = 12;
  time.minute = 0;
  time.second = 0;
  time.second_part = 999999;

  MYSQL_TIME time2;
  time2.neg = false;
  time2.hour = 0;
  time2.minute = 0;
  time2.second = 0;
  time2.second_part = 0;

  MYSQL_TIME time3;
  time3.neg = true;
  time3.hour = 811;
  time3.minute = 0;
  time3.second = 0;
  time3.second_part = 0;

  rb.start_row(obuffer.get());

  rb.add_time_field(&time, 0);
  rb.add_time_field(&time2, 0);
  rb.add_time_field(&time3, 0);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};

  buffer = row->mutable_field(idx++);
  xcl::Time xtime;

  ASSERT_TRUE(xcl::row_decoder::buffer_to_time(*buffer, &xtime));
  ASSERT_TRUE(false == xtime.is_negate());
  ASSERT_EQ(time.hour, xtime.hour());
  ASSERT_EQ(time.minute, xtime.minutes());
  ASSERT_EQ(time.second, xtime.seconds());
  ASSERT_EQ(time.second_part, xtime.useconds());

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_time(*buffer, &xtime));
  ASSERT_TRUE(false == xtime.is_negate());
  ASSERT_EQ(time2.hour, xtime.hour());
  ASSERT_EQ(time2.minute, xtime.minutes());
  ASSERT_EQ(time2.second, xtime.seconds());
  ASSERT_EQ(time2.second_part, xtime.useconds());

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_time(*buffer, &xtime));
  ASSERT_TRUE(true == xtime.is_negate());
  ASSERT_EQ(time3.hour, xtime.hour());
  ASSERT_EQ(time3.minute, xtime.minutes());
  ASSERT_EQ(time3.second, xtime.seconds());
  ASSERT_EQ(time3.second_part, xtime.useconds());
}

TEST(row_builder, datetime_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.year = 2016;
  time.month = 12;
  time.day = 24;
  time.hour = 13;
  time.minute = 55;
  time.second = 55;
  time.second_part = 999999;
  time.time_type = MYSQL_TIMESTAMP_DATETIME;

  MYSQL_TIME time2;
  time2.year = 2000;
  time2.month = 1;
  time2.day = 1;
  time2.hour = 0;
  time2.minute = 0;
  time2.second = 0;
  time2.second_part = 0;
  time2.time_type = MYSQL_TIMESTAMP_DATETIME;

  rb.start_row(obuffer.get());

  rb.add_datetime_field(&time, 0);
  rb.add_datetime_field(&time2, 0);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};

  buffer = row->mutable_field(idx++);
  xcl::DateTime xtime;
  bool has_time = true;
  ASSERT_TRUE(xcl::row_decoder::buffer_to_datetime(*buffer, &xtime, has_time));
  ASSERT_EQ(time.year, xtime.year());
  ASSERT_EQ(time.month, xtime.month());
  ASSERT_EQ(time.day, xtime.day());
  ASSERT_EQ(time.hour, xtime.hour());
  ASSERT_EQ(time.minute, xtime.minutes());
  ASSERT_EQ(time.second, xtime.seconds());
  ASSERT_EQ(time.second_part, xtime.useconds());

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_datetime(*buffer, &xtime, has_time));
  ASSERT_EQ(time2.year, xtime.year());
  ASSERT_EQ(time2.month, xtime.month());
  ASSERT_EQ(time2.day, xtime.day());
  ASSERT_EQ(time2.hour, xtime.hour());
  ASSERT_EQ(time2.minute, xtime.minutes());
  ASSERT_EQ(time2.second, xtime.seconds());
  ASSERT_EQ(time2.second_part, xtime.useconds());
}

TEST(row_builder, decimal_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  xcl::Decimal xdecimal;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  decimal_digit_t arr1[] = {1, 0};
  decimal_t dec1 = {1, 1, 2, 1, arr1};
  rb.add_decimal_field(&dec1);

  decimal_digit_t arr2[] = {1, 0};
  decimal_t dec2 = {1, 1, 2, 0, arr2};
  rb.add_decimal_field(&dec2);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_decimal(*buffer, &xdecimal));
  ASSERT_EQ("-1.0", xdecimal.to_string());

  buffer = row->mutable_field(idx++);
  ASSERT_TRUE(xcl::row_decoder::buffer_to_decimal(*buffer, &xdecimal));
  ASSERT_EQ("1.0", xdecimal.to_string());
}

TEST(row_builder, set_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_set_field("A,B,C,D", strlen("A,B,C,D"), NULL);
  rb.add_set_field("", strlen(""), NULL);  // empty SET case
  rb.add_set_field("A", strlen("A"), NULL);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row(
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get()));

  buffer = row->mutable_field(idx++);
  std::set<std::string> elems;
  std::string elems_string;
  ASSERT_TRUE(xcl::row_decoder::buffer_to_string_set(*buffer, &elems_string));
  ASSERT_STREQ("A,B,C,D", elems_string.c_str());
  ASSERT_TRUE(xcl::row_decoder::buffer_to_set(*buffer, &elems));
  ASSERT_EQ(1u, elems.count("A"));
  ASSERT_EQ(1u, elems.count("B"));
  ASSERT_EQ(1u, elems.count("C"));
  ASSERT_EQ(1u, elems.count("D"));
  ASSERT_EQ(4u, elems.size());

  buffer = row->mutable_field(idx++);
  xcl::row_decoder::buffer_to_set(*buffer, &elems);
  ASSERT_EQ(true, elems.empty());
  ASSERT_TRUE(xcl::row_decoder::buffer_to_string_set(*buffer, &elems_string));
  ASSERT_STREQ("", elems_string.c_str());

  buffer = row->mutable_field(idx++);
  xcl::row_decoder::buffer_to_set(*buffer, &elems);
  ASSERT_EQ(1u, elems.size());
  ASSERT_EQ(1u, elems.count("A"));
  ASSERT_TRUE(xcl::row_decoder::buffer_to_string_set(*buffer, &elems_string));
  ASSERT_STREQ("A", elems_string.c_str());
}

TEST(row_builder, bit_field) {
  Row_builder rb;
  std::string *buffer;
  int idx = 0;
  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  rb.start_row(obuffer.get());

  rb.add_bit_field("\x00", 1, NULL);
  rb.add_bit_field("\x01", 1, NULL);
  rb.add_bit_field("\xff\x00", 2, NULL);
  rb.add_bit_field("\x00\x00\x00\x00\x00\x00\x00\x00", 8, NULL);
  rb.add_bit_field("\xff\xff\xff\xff\xff\xff\xff\xff", 8, NULL);

  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};

  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0x0u, &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0x1u, &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0xff00u, &xcl::row_decoder::buffer_to_u64,
                              *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0x0000000000000000ULL,
                              &xcl::row_decoder::buffer_to_u64, *buffer);
  buffer = row->mutable_field(idx++);
  assert_row_getter<uint64_t>(0xffffffffffffffffULL,
                              &xcl::row_decoder::buffer_to_u64, *buffer);
}

TEST(row_builder, datetime_content_type_set) {
  xcl::XRow_impl::Metadata metadata;
  xcl::XRow_impl::Metadata::value_type metadata_row;
  metadata_row.type = xcl::Column_type::DATETIME;
  metadata_row.length = 19;
  metadata_row.has_content_type = true;
  metadata_row.content_type =
      static_cast<uint32_t>(Mysqlx::Resultset::DATETIME);
  metadata.push_back(metadata_row);

  xcl::Context context;

  ::testing::StrictMock<xcl::XRow_impl> row_mock(&metadata, &context);

  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.year = 2016;
  time.month = 12;
  time.day = 24;
  time.hour = 13;
  time.minute = 55;
  time.second = 55;
  time.second_part = 999999;
  time.time_type = MYSQL_TIMESTAMP_DATETIME;

  Row_builder rb;
  rb.start_row(obuffer.get());
  rb.add_datetime_field(&time, 0);
  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};
  row_mock.set_row(std::move(row));

  xcl::DateTime result;
  row_mock.get_datetime(0, &result);
  EXPECT_TRUE(result.has_time());
  EXPECT_EQ(result.year(), time.year);
  EXPECT_EQ(result.month(), time.month);
  EXPECT_EQ(result.day(), time.day);
  EXPECT_EQ(result.hour(), time.hour);
  EXPECT_EQ(result.minutes(), time.minute);
  EXPECT_EQ(result.seconds(), time.second);
  EXPECT_EQ(result.useconds(), time.second_part);
}

TEST(row_builder, datetime_content_type_not_set_and_has_time_part) {
  xcl::XRow_impl::Metadata metadata;
  xcl::XRow_impl::Metadata::value_type metadata_row;
  metadata_row.type = xcl::Column_type::DATETIME;
  metadata_row.length = 19;
  metadata_row.has_content_type = false;
  metadata.push_back(metadata_row);

  xcl::Context context;

  ::testing::StrictMock<xcl::XRow_impl> row_mock(&metadata, &context);

  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.year = 2016;
  time.month = 12;
  time.day = 24;
  time.hour = 13;
  time.minute = 55;
  time.second = 55;
  time.second_part = 0;
  time.time_type = MYSQL_TIMESTAMP_DATETIME;

  Row_builder rb;
  rb.start_row(obuffer.get());
  rb.add_datetime_field(&time, 0);
  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};
  row_mock.set_row(std::move(row));

  xcl::DateTime result;
  row_mock.get_datetime(0, &result);
  EXPECT_EQ(row_mock.get_number_of_fields(), 1);
  EXPECT_TRUE(result.has_time());
  EXPECT_EQ(result.year(), time.year);
  EXPECT_EQ(result.month(), time.month);
  EXPECT_EQ(result.day(), time.day);
  EXPECT_EQ(result.hour(), time.hour);
  EXPECT_EQ(result.minutes(), time.minute);
  EXPECT_EQ(result.seconds(), time.second);
  EXPECT_EQ(result.useconds(), time.second_part);
}

TEST(row_builder, datetime_content_type_not_set_and_not_contains_time_part) {
  xcl::XRow_impl::Metadata metadata;
  xcl::XRow_impl::Metadata::value_type metadata_row;
  metadata_row.type = xcl::Column_type::DATETIME;
  metadata_row.length = 10;
  metadata_row.has_content_type = false;
  metadata.push_back(metadata_row);

  xcl::Context context;

  ::testing::StrictMock<xcl::XRow_impl> row_mock(&metadata, &context);

  ngs::unique_ptr<Page_pool> page_pool(new Page_pool(default_pool_config));
  ngs::unique_ptr<Output_buffer> obuffer(new Output_buffer(*page_pool));

  MYSQL_TIME time;
  time.year = 2016;
  time.month = 12;
  time.day = 24;
  time.hour = 0;
  time.minute = 0;
  time.second = 0;
  time.second_part = 0;
  time.time_type = MYSQL_TIMESTAMP_DATE;

  Row_builder rb;
  rb.start_row(obuffer.get());
  rb.add_datetime_field(&time, 0);
  rb.end_row();
  ngs::unique_ptr<Mysqlx::Resultset::Row> row{
      message_from_buffer<Mysqlx::Resultset::Row>(obuffer.get())};
  row_mock.set_row(std::move(row));

  xcl::DateTime result;
  row_mock.get_datetime(0, &result);
  EXPECT_EQ(result.year(), time.year);
  EXPECT_EQ(result.month(), time.month);
  EXPECT_EQ(result.day(), time.day);

  EXPECT_FALSE(result.has_time());
  EXPECT_EQ(result.hour(), 0xFF);
  EXPECT_EQ(result.minutes(), 0xFF);
  EXPECT_EQ(result.seconds(), 0xFF);
  EXPECT_EQ(result.useconds(), 0xFFFFFF);
}

}  // namespace test

}  // namespace xpl

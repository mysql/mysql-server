/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <climits>
#include <cstring>   // std::memcmp
#include <iterator>  // std::next
#include <random>

#include "my_alloc.h"                  // MEM_ROOT
#include "mysql_time.h"                // MYSQL_TIME
#include "sql/field.h"                 // my_charset_numeric
#include "sql/histograms/value_map.h"  // Value_map
#include "sql/memroot_allocator.h"
#include "sql/my_decimal.h"  // my_decimal
#include "sql/sql_time.h"    // my_time_compare
#include "sql_string.h"      // String

namespace value_map_unittest {

class ValueMapTest : public ::testing::Test {
 public:
  ValueMapTest() {}
};

TEST_F(ValueMapTest, LongLongValueMap) {
  histograms::Value_map<longlong> value_map(&my_charset_numeric,
                                            histograms::Value_map_type::INT);

  EXPECT_EQ(value_map.size(), 0U);

  value_map.add_values(1LL, 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(1LL, 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(std::numeric_limits<longlong>::min(), 1);
  EXPECT_EQ(value_map.size(), 2U);

  value_map.add_values(0LL, 1);
  EXPECT_EQ(value_map.size(), 3U);

  value_map.add_values(0LL, 1000);
  EXPECT_EQ(value_map.size(), 3U);

  // Check that data is sorted
  EXPECT_EQ(value_map.begin()->first, std::numeric_limits<longlong>::min());
  EXPECT_EQ(std::next(value_map.begin(), 1)->first, 0LL);
  EXPECT_EQ(std::next(value_map.begin(), 2)->first, 1LL);

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 1U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 1001U);
  EXPECT_EQ(std::next(value_map.begin(), 2)->second, 2U);
}

TEST_F(ValueMapTest, ULongLongValueMap) {
  histograms::Value_map<ulonglong> value_map(&my_charset_numeric,
                                             histograms::Value_map_type::UINT);

  EXPECT_EQ(value_map.size(), 0U);

  value_map.add_values(std::numeric_limits<ulonglong>::max(), 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(0ULL, 1);
  EXPECT_EQ(value_map.size(), 2U);

  // Check that data is sorted
  EXPECT_EQ(value_map.begin()->first, 0ULL);
  EXPECT_EQ(std::next(value_map.begin(), 1)->first,
            std::numeric_limits<ulonglong>::max());

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 1U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 1U);
}

TEST_F(ValueMapTest, DoubleValueMap) {
  histograms::Value_map<double> value_map(&my_charset_numeric,
                                          histograms::Value_map_type::DOUBLE);

  EXPECT_EQ(value_map.size(), 0U);

  value_map.add_values(std::numeric_limits<double>::max(), 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(std::numeric_limits<double>::lowest(), 42);
  EXPECT_EQ(value_map.size(), 2U);

  value_map.add_values(0.0, 1);
  EXPECT_EQ(value_map.size(), 3U);

  // Check that data is sorted
  EXPECT_EQ(value_map.begin()->first, std::numeric_limits<double>::lowest());
  EXPECT_EQ(std::next(value_map.begin(), 1)->first, 0.0);
  EXPECT_EQ(std::next(value_map.begin(), 2)->first,
            std::numeric_limits<double>::max());

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 42U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 1U);
  EXPECT_EQ(std::next(value_map.begin(), 2)->second, 1U);
}

TEST_F(ValueMapTest, DecimalValueMap) {
  histograms::Value_map<my_decimal> value_map(
      &my_charset_numeric, histograms::Value_map_type::DECIMAL);

  EXPECT_EQ(value_map.size(), 0U);

  my_decimal val1;
  double2my_decimal(0, -12.0, &val1);
  value_map.add_values(val1, 9);
  EXPECT_EQ(value_map.size(), 1U);

  my_decimal val2;
  double2my_decimal(0, -100.1, &val2);
  value_map.add_values(val2, 8);
  EXPECT_EQ(value_map.size(), 2U);

  my_decimal val3;
  double2my_decimal(0, 99.9, &val3);
  value_map.add_values(val3, 7);
  EXPECT_EQ(value_map.size(), 3U);

  // Check that data is sorted
  String res1;
  my_decimal2string(E_DEC_FATAL_ERROR, &value_map.begin()->first, 0, 0, 0,
                    &res1);
  EXPECT_EQ(strcmp(res1.ptr(), "-100.1"), 0);

  String res2;
  my_decimal2string(E_DEC_FATAL_ERROR, &std::next(value_map.begin(), 1)->first,
                    0, 0, 0, &res2);
  EXPECT_EQ(strcmp(res2.ptr(), "-12"), 0);

  String res3;
  my_decimal2string(E_DEC_FATAL_ERROR, &std::next(value_map.begin(), 2)->first,
                    0, 0, 0, &res3);
  EXPECT_EQ(strcmp(res3.ptr(), "99.9"), 0);

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 8U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 9U);
  EXPECT_EQ(std::next(value_map.begin(), 2)->second, 7U);
}

TEST_F(ValueMapTest, MysqlTimeValueMap) {
  histograms::Value_map<MYSQL_TIME> value_map(
      &my_charset_numeric, histograms::Value_map_type::DATETIME);

  EXPECT_EQ(value_map.size(), 0U);

  MYSQL_TIME time1;
  time1.year = 2017;
  time1.month = 1;
  time1.day = 1;
  time1.hour = 10;
  time1.minute = 0;
  time1.second = 0;
  time1.second_part = 0;
  time1.neg = false;
  time1.time_type = MYSQL_TIMESTAMP_DATETIME;
  value_map.add_values(time1, 1);
  EXPECT_EQ(value_map.size(), 1U);

  MYSQL_TIME time2;
  time2.year = 2017;
  time2.month = 1;
  time2.day = 1;
  time2.hour = 10;
  time2.minute = 0;
  time2.second = 0;
  time2.second_part = 1;
  time2.neg = false;
  time2.time_type = MYSQL_TIMESTAMP_DATETIME;
  value_map.add_values(time2, 2);
  EXPECT_EQ(value_map.size(), 2U);

  MYSQL_TIME time3;
  time3.year = 1000;
  time3.month = 1;
  time3.day = 1;
  time3.hour = 10;
  time3.minute = 10;
  time3.second = 11;
  time3.second_part = 12;
  time3.neg = false;
  time3.time_type = MYSQL_TIMESTAMP_DATETIME;
  value_map.add_values(time3, 3);
  EXPECT_EQ(value_map.size(), 3U);

  // Same value as time2
  MYSQL_TIME time4;
  time4.year = 2017;
  time4.month = 1;
  time4.day = 1;
  time4.hour = 10;
  time4.minute = 0;
  time4.second = 0;
  time4.second_part = 1;
  time4.neg = false;
  time4.time_type = MYSQL_TIMESTAMP_DATETIME;
  value_map.add_values(time4, 2);
  EXPECT_EQ(value_map.size(), 3U);

  // Check that data is sorted
  EXPECT_EQ(my_time_compare(&time3, &value_map.begin()->first), 0);
  EXPECT_EQ(my_time_compare(&time1, &std::next(value_map.begin(), 1)->first),
            0);
  EXPECT_EQ(my_time_compare(&time2, &std::next(value_map.begin(), 2)->first),
            0);

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 3U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 1U);
  EXPECT_EQ(std::next(value_map.begin(), 2)->second, 4U);
}

TEST_F(ValueMapTest, StringValueMap) {
  histograms::Value_map<String> value_map(&my_charset_latin1,
                                          histograms::Value_map_type::STRING);

  EXPECT_EQ(value_map.size(), 0U);

  value_map.add_values(String("string2", &my_charset_latin1), 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(String("string2", &my_charset_latin1), 1);
  EXPECT_EQ(value_map.size(), 1U);

  value_map.add_values(String("string1", &my_charset_latin1), 1);
  EXPECT_EQ(value_map.size(), 2U);

  value_map.add_values(String("string1", &my_charset_latin1), 1000);
  EXPECT_EQ(value_map.size(), 2U);

  // Check that data is sorted
  EXPECT_EQ(strcmp(value_map.begin()->first.ptr(), "string1"), 0);
  EXPECT_EQ(strcmp(std::next(value_map.begin(), 1)->first.ptr(), "string2"), 0);

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 1001U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 2U);
}

TEST_F(ValueMapTest, ValueMapWithLongStrings) {
  histograms::Value_map<String> value_map(&my_charset_latin1,
                                          histograms::Value_map_type::STRING);

  EXPECT_EQ(value_map.size(), 0U);

  /*
    If HISTOGRAM_MAX_COMPARE_LENGTH change to anything else than 42, some of
    these tests needs to be changed accordingly.
  */
  EXPECT_TRUE(histograms::HISTOGRAM_MAX_COMPARE_LENGTH == 42);

  // Exactly 42 characters
  value_map.add_values(
      String("abcdefghijklmnopqrstuvwxyz1234567890abcdef", &my_charset_latin1),
      1);
  EXPECT_EQ(value_map.size(), 1U);

  // Exactly 42 characters, with a small difference from the first string
  value_map.add_values(
      String("abcdefghijklmnopqrstuvwxyz1234567890abcdeg", &my_charset_latin1),
      1);
  EXPECT_EQ(value_map.size(), 2U);

  /*
    Exactly 43 characters. The 42 first are the same as the first string, so no
    new values should be added.
  */
  value_map.add_values(
      String("abcdefghijklmnopqrstuvwxyz1234567890abcdef1", &my_charset_latin1),
      1);
  EXPECT_EQ(value_map.size(), 2U);

  // Check that data is sorted
  EXPECT_EQ(strcmp(value_map.begin()->first.ptr(),
                   "abcdefghijklmnopqrstuvwxyz1234567890abcdef"),
            0);
  EXPECT_EQ(strcmp(std::next(value_map.begin(), 1)->first.ptr(),
                   "abcdefghijklmnopqrstuvwxyz1234567890abcdeg"),
            0);

  // Check that the counts are correct
  EXPECT_EQ(value_map.begin()->second, 2U);
  EXPECT_EQ(std::next(value_map.begin(), 1)->second, 1U);
}

TEST_F(ValueMapTest, LongLongValueMapExtended) {
  histograms::Value_map<longlong> value_map(&my_charset_latin1,
                                            histograms::Value_map_type::INT);

  EXPECT_EQ(value_map.size(), 0U);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<longlong> dis(-1000, 1000);

  for (int i = 0; i < 10000; ++i) value_map.add_values(dis(gen), 1);

  // Check that all values are ordered and unique.
  auto previous = value_map.begin();
  for (auto current = std::next(value_map.begin()); current != value_map.end();
       ++current) {
    EXPECT_LT(previous->first, current->first);
    std::advance(previous, 1);
  }
}

}  // namespace value_map_unittest

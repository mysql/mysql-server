
/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "my_temporal.h"
#include "my_time.h"

namespace Time_val_unittest {

//////////////////////////////////////////////////////////////////////////////

TEST(Time_val, MYSQL_TIME) {
  Time_val time1(false, 24, 0, 0, 0);
  MYSQL_TIME mt = static_cast<MYSQL_TIME>(time1);
  Time_val time2 = Time_val(mt);
  EXPECT_EQ(0, time1.compare(time2));
  MYSQL_TIME mytime(2023, 1, 30, 12, 0, 0, 0, false, MYSQL_TIMESTAMP_DATETIME,
                    0);
  Time_val a(false, 12, 0, 0, 0);
  EXPECT_EQ(Time_val::strip_date(mytime), a);
  Time_val b{mt};
  EXPECT_EQ(b, time1);
}
TEST(Time_val, fields) {
  Time_val a(true, 1, 2, 3, 4);
  EXPECT_EQ(a.is_negative(), true);
  EXPECT_EQ(a.hour(), 1);
  EXPECT_EQ(a.minute(), 2);
  EXPECT_EQ(a.second(), 3);
  EXPECT_EQ(a.microsecond(), 4);
}
TEST(Time_val, compare) {
  Time_val time00(true, 838, 59, 59, 0);
  Time_val time01(true, 838, 0, 0, 0);
  Time_val time02(true, 1, 0, 0, 0);
  Time_val time10(true, 0, 59, 0, 0);
  Time_val time11(true, 0, 1, 0, 0);
  Time_val time20(true, 0, 0, 59, 0);
  Time_val time21(true, 0, 0, 1, 0);
  Time_val time30(true, 0, 0, 0, 999999);
  Time_val time31(true, 0, 0, 0, 1);
  Time_val time40(false, 0, 0, 0, 0);
  Time_val time41(false, 0, 0, 0, 1);
  Time_val time42(false, 0, 0, 0, 999999);
  Time_val time50(false, 0, 0, 1, 0);
  Time_val time51(false, 0, 0, 59, 0);
  Time_val time60(false, 0, 1, 0, 0);
  Time_val time61(false, 0, 59, 0, 0);
  Time_val time70(false, 1, 0, 0, 0);
  Time_val time71(false, 838, 0, 0, 0);
  Time_val time72(false, 838, 59, 59, 0);

  EXPECT_GT(0, time00.compare(time01));
  EXPECT_GT(0, time01.compare(time02));
  EXPECT_GT(0, time02.compare(time10));
  EXPECT_GT(0, time10.compare(time11));
  EXPECT_GT(0, time11.compare(time20));
  EXPECT_GT(0, time20.compare(time21));
  EXPECT_GT(0, time21.compare(time30));
  EXPECT_GT(0, time30.compare(time31));
  EXPECT_GT(0, time31.compare(time40));
  EXPECT_GT(0, time40.compare(time41));
  EXPECT_GT(0, time41.compare(time42));
  EXPECT_GT(0, time42.compare(time50));
  EXPECT_GT(0, time50.compare(time51));
  EXPECT_GT(0, time51.compare(time60));
  EXPECT_GT(0, time60.compare(time61));
  EXPECT_GT(0, time61.compare(time70));
  EXPECT_GT(0, time70.compare(time71));
  EXPECT_GT(0, time71.compare(time72));
}
TEST(Time_val, to_seconds) {
  int32_t seconds1 = Time_val(false, 2, 10, 10, 123456).to_seconds();
  EXPECT_EQ(seconds1, 7810);
  int32_t seconds2 = Time_val(true, 2, 10, 10, 123456).to_seconds();
  EXPECT_EQ(seconds2, -7810);
}
TEST(Time_val, to_microseconds) {
  int64_t micro1 = Time_val(false, 2, 10, 10, 123456).to_microseconds();
  EXPECT_EQ(micro1, 7810123456);
  int64_t micro2 = Time_val(true, 2, 10, 10, 123456).to_microseconds();
  EXPECT_EQ(micro2, -7810123456);
}
TEST(Time_val, to_int_rounded) {
  int64_t hhmmss1 = Time_val(false, 2, 10, 10, 500000).to_int_rounded();
  EXPECT_EQ(hhmmss1, 21011);
  int64_t hhmmss2 = Time_val(false, 2, 10, 10, 499999).to_int_rounded();
  EXPECT_EQ(hhmmss2, 21010);
  int64_t hhmmss3 = Time_val(true, 2, 10, 10, 500000).to_int_rounded();
  EXPECT_EQ(hhmmss3, -21011);
  int64_t hhmmss4 = Time_val(true, 2, 10, 10, 499999).to_int_rounded();
  EXPECT_EQ(hhmmss4, -21010);
}
TEST(Time_val, to_int_truncated) {
  int64_t hhmmss1 = Time_val(false, 2, 10, 10, 500000).to_int_truncated();
  EXPECT_EQ(hhmmss1, 21010);
  int64_t hhmmss2 = Time_val(false, 2, 10, 10, 499999).to_int_truncated();
  EXPECT_EQ(hhmmss2, 21010);
  int64_t hhmmss3 = Time_val(true, 2, 10, 10, 500000).to_int_truncated();
  EXPECT_EQ(hhmmss3, -21010);
  int64_t hhmmss4 = Time_val(true, 2, 10, 10, 499999).to_int_truncated();
  EXPECT_EQ(hhmmss4, -21010);
}
TEST(Time_val, to_double) {
  Time_val tv1(false, 23, 3, 23, 456789);
  EXPECT_EQ(230323.456789, tv1.to_double());
  Time_val tv2(true, 23, 3, 23, 456789);
  EXPECT_EQ(-230323.456789, tv2.to_double());
}
TEST(Time_val, add_nanoseconds_round) {
  Time_val a;
  a.set_zero();
  a.add_nanoseconds_round(999999999);
  Time_val b(false, 0, 0, 1, 0);
  EXPECT_EQ(a, b);
  Time_val c;
  c.set_zero();
  c.add_nanoseconds_round(-999999999);
  Time_val d(true, 0, 0, 1, 0);
  EXPECT_EQ(c, d);
}
TEST(Time_val, round) {
  Time_val time0 = Time_val(false, 0, 0, 0, 940000);
  Time_val time1 = Time_val(false, 0, 0, 0, 950000);
  Time_val time2 = Time_val(false, 0, 0, 0, 990000);
  time0.adjust_fraction(1, true);
  time1.adjust_fraction(1, true);
  time2.adjust_fraction(1, true);
  Time_val rounded0(false, 0, 0, 0, 900000);
  Time_val rounded1(false, 0, 0, 1, 0);
  Time_val rounded2(false, 0, 0, 1, 0);
  EXPECT_EQ(time0, rounded0);
  EXPECT_EQ(time1, rounded1);
  EXPECT_EQ(time2, rounded2);

  Time_val time3 = Time_val(true, 0, 0, 0, 940000);
  Time_val time4 = Time_val(true, 0, 0, 0, 950000);
  Time_val time5 = Time_val(true, 0, 0, 0, 990000);
  time3.adjust_fraction(1, true);
  time4.adjust_fraction(1, true);
  time5.adjust_fraction(1, true);
  Time_val rounded3(true, 0, 0, 0, 900000);
  Time_val rounded4(true, 0, 0, 1, 0);
  Time_val rounded5(true, 0, 0, 1, 0);
  EXPECT_EQ(time3, rounded3);
  EXPECT_EQ(time4, rounded4);
  EXPECT_EQ(time5, rounded5);

  Time_val time6(false, 10, 20, 30, 0);
  time6.adjust_fraction(2, true);
  Time_val time6_dup(false, 10, 20, 30, 0);
  EXPECT_EQ(time6, time6_dup);
}

TEST(Time_val, truncate) {
  Time_val time0 = Time_val(false, 0, 0, 0, 940000);
  Time_val time1 = Time_val(false, 0, 0, 0, 950000);
  Time_val time2 = Time_val(false, 0, 0, 0, 990000);
  time0.adjust_fraction(1, false);
  time1.adjust_fraction(1, false);
  time2.adjust_fraction(1, false);
  Time_val truncated0(false, 0, 0, 0, 900000);
  Time_val truncated1(false, 0, 0, 0, 900000);
  Time_val truncated2(false, 0, 0, 0, 900000);
  EXPECT_EQ(time0, truncated0);
  EXPECT_EQ(time1, truncated1);
  EXPECT_EQ(time2, truncated2);
  Time_val time3 = Time_val(true, 0, 0, 0, 940000);
  Time_val time4 = Time_val(true, 0, 0, 0, 950000);
  Time_val time5 = Time_val(true, 0, 0, 0, 990000);
  time3.adjust_fraction(1, false);
  time4.adjust_fraction(1, false);
  time5.adjust_fraction(1, false);
  Time_val truncated3(true, 0, 0, 0, 900000);
  Time_val truncated4(true, 0, 0, 0, 900000);
  Time_val truncated5(true, 0, 0, 0, 900000);
  EXPECT_EQ(time3, truncated3);
  EXPECT_EQ(time4, truncated4);
  EXPECT_EQ(time5, truncated5);
}
TEST(Time_val, to_string) {
  char buffer[20];
  Time_val time = Time_val(true, 1, 2, 3, 4);
  size_t buf_len = time.to_string(buffer, 6);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "-01:02:03.000004"));
}
TEST(Time_val, add) {
  Time_val time0(false, 10, 10, 10, 10);
  time0.add(Time_val(false, 10, 10, 10, 10), false);
  EXPECT_EQ(time0, Time_val(false, 20, 20, 20, 20));
  time0.add(Time_val(false, 10, 10, 10, 10), true);
  EXPECT_EQ(time0, Time_val(false, 10, 10, 10, 10));

  Interval iv;

  Time_val time1(false, 11, 12, 13, 456789);
  iv.second_part = 900000;
  EXPECT_FALSE(time1.add(iv, false));
  EXPECT_EQ(time1, Time_val(false, 11, 12, 14, 356789));
  EXPECT_FALSE(time1.add(iv, true));
  EXPECT_EQ(time1, Time_val(false, 11, 12, 13, 456789));

  Time_val time2(false, 11, 12, 13, 456789);
  iv.second_part = 0;
  iv.second = 60 * 60 + 59;
  EXPECT_FALSE(time2.add(iv, false));
  EXPECT_EQ(time2, Time_val(false, 12, 13, 12, 456789));
  EXPECT_FALSE(time2.add(iv, true));
  EXPECT_EQ(time2, Time_val(false, 11, 12, 13, 456789));

  Time_val time3(false, 11, 12, 13, 456789);
  iv.second = 0;
  iv.minute = 24 * 60 + 59;
  EXPECT_FALSE(time3.add(iv, false));
  EXPECT_EQ(time3, Time_val(false, 36, 11, 13, 456789));
  EXPECT_FALSE(time3.add(iv, true));
  EXPECT_EQ(time3, Time_val(false, 11, 12, 13, 456789));

  Time_val time4(false, 11, 12, 13, 456789);
  iv.minute = 0;
  iv.hour = 800;
  EXPECT_FALSE(time4.add(iv, false));
  EXPECT_EQ(time4, Time_val(false, 811, 12, 13, 456789));
  EXPECT_FALSE(time4.add(iv, true));
  EXPECT_EQ(time4, Time_val(false, 11, 12, 13, 456789));

  Time_val time5(false, 0, 0, 0, 0);
  iv.second_part = 0ULL;
  iv.second = 0ULL;
  iv.minute = 0ULL;
  iv.hour = 839UL;
  EXPECT_TRUE(time5.add(iv, false));
  EXPECT_TRUE(time5.add(iv, true));
  iv.hour = 838UL;
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 838, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(true, 838, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));

  iv.minute = 839ULL * 60ULL;
  iv.hour = 0UL;
  EXPECT_TRUE(time5.add(iv, false));
  EXPECT_TRUE(time5.add(iv, true));
  iv.minute = 838ULL * 60ULL + 59ULL;
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 838, 59, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(true, 838, 59, 0, 0));
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));

  iv.second = 839ULL * 3600ULL;
  iv.minute = 0ULL;
  EXPECT_TRUE(time5.add(iv, false));
  EXPECT_TRUE(time5.add(iv, true));
  iv.second = 838ULL * 3600ULL + 59ULL * 60ULL + 59ULL;
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 838, 59, 59, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(true, 838, 59, 59, 0));
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));

  iv.second_part = 839ULL * 3600ULL * 1000000ULL;
  iv.second = 0ULL;
  EXPECT_TRUE(time5.add(iv, false));
  EXPECT_TRUE(time5.add(iv, true));
  iv.second_part = (838ULL * 3600ULL + 59ULL * 60ULL + 59ULL) * 1000000ULL;
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 838, 59, 59, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));
  EXPECT_FALSE(time5.add(iv, true));
  EXPECT_EQ(time5, Time_val(true, 838, 59, 59, 0));
  EXPECT_FALSE(time5.add(iv, false));
  EXPECT_EQ(time5, Time_val(false, 0, 0, 0, 0));

  Time_val time6(false, 0, 0, 0, 0);
  EXPECT_FALSE(time6.add_nanoseconds_round(500));
  EXPECT_EQ(time6, Time_val(false, 0, 0, 0, 1));

  Time_val time7(false, 0, 0, 0, 0);
  EXPECT_FALSE(time7.add_nanoseconds_round(-500));
  EXPECT_EQ(time7, Time_val(true, 0, 0, 0, 1));
}
TEST(Time_val, extreme_values) {
  Time_val time;
  time.set_zero();
  EXPECT_EQ(time, Time_val(false, 0, 0, 0, 0));
  time.set_extreme_value(false);
  EXPECT_EQ(time, Time_val(false, 838, 59, 59, 0));
  time.set_extreme_value(true);
  EXPECT_EQ(time, Time_val(true, 838, 59, 59, 0));
}
TEST(Time_val, is_adjusted) {
  EXPECT_TRUE(Time_val(false, 838, 59, 58, 999999).is_adjusted(6));
  EXPECT_FALSE(Time_val(false, 838, 59, 58, 999999).is_adjusted(5));
  EXPECT_TRUE(Time_val(false, 838, 58, 59, 999990).is_adjusted(5));
  EXPECT_FALSE(Time_val(false, 838, 58, 59, 999990).is_adjusted(4));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 999900).is_adjusted(4));
  EXPECT_FALSE(Time_val(false, 23, 59, 59, 999900).is_adjusted(3));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 999000).is_adjusted(3));
  EXPECT_FALSE(Time_val(true, 23, 59, 59, 999000).is_adjusted(2));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 990000).is_adjusted(2));
  EXPECT_FALSE(Time_val(false, 23, 59, 59, 990000).is_adjusted(1));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 900000).is_adjusted(1));
  EXPECT_FALSE(Time_val(true, 23, 59, 59, 900000).is_adjusted(0));
  EXPECT_TRUE(Time_val(false, 23, 59, 59, 0).is_adjusted(0));
  EXPECT_TRUE(Time_val(true, 23, 59, 59, 0).is_adjusted(0));
}
TEST(Time_val, actual_decimals) {
  EXPECT_EQ(6, Time_val(false, 838, 59, 58, 999999).actual_decimals());
  EXPECT_EQ(5, Time_val(true, 838, 59, 58, 999990).actual_decimals());
  EXPECT_EQ(4, Time_val(false, 23, 59, 59, 999900).actual_decimals());
  EXPECT_EQ(3, Time_val(true, 23, 59, 59, 999000).actual_decimals());
  EXPECT_EQ(2, Time_val(false, 23, 59, 59, 990000).actual_decimals());
  EXPECT_EQ(1, Time_val(true, 23, 59, 59, 900000).actual_decimals());
  EXPECT_EQ(0, Time_val(false, 23, 59, 59, 0).actual_decimals());
}
}  // namespace Time_val_unittest

namespace Date_val_unittest {
TEST(Date_val, make_date) {
  Date_val date;
  EXPECT_FALSE(Date_val::make_date(0, 1, 1, 0, &date));
  EXPECT_FALSE(Date_val::make_date(9999, 12, 31, 0, &date));
  EXPECT_FALSE(Date_val::make_date(0, 1, 1, TIME_NO_ZERO_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(9999, 12, 31, TIME_NO_ZERO_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(0, 1, 1, TIME_NO_ZERO_IN_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(9999, 12, 31, TIME_NO_ZERO_IN_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(0, 1, 1, TIME_NO_INVALID_DATES, &date));
  EXPECT_FALSE(Date_val::make_date(9999, 12, 31, TIME_NO_INVALID_DATES, &date));
  EXPECT_FALSE(Date_val::make_date(0, 0, 0, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 0, 0, TIME_NO_ZERO_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(0, 0, 1, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 0, 1, TIME_NO_ZERO_IN_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(0, 1, 0, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 1, 0, TIME_NO_ZERO_IN_DATE, &date));
  EXPECT_FALSE(Date_val::make_date(0, 2, 29, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 2, 29, TIME_NO_INVALID_DATES, &date));
  EXPECT_FALSE(Date_val::make_date(2000, 2, 30, 0, &date));
  EXPECT_TRUE(Date_val::make_date(2000, 2, 30, TIME_NO_INVALID_DATES, &date));
  EXPECT_TRUE(Date_val::make_date(10000, 1, 1, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 13, 1, 0, &date));
  EXPECT_TRUE(Date_val::make_date(0, 1, 32, 0, &date));
}
TEST(Date_val, store_load_date) {
  uint8_t buffer[3];
  Date_val date;

  Date_val date1{0, 1, 1};
  date1.store_date(buffer);
  Date_val::load_date(buffer, &date);
  EXPECT_EQ(date, date1);

  Date_val date2{9999, 12, 31};
  date2.store_date(buffer);
  Date_val::load_date(buffer, &date);
  EXPECT_EQ(date, date2);
}
TEST(Date_val, MYSQL_TIME) {
  Date_val date1(2025, 2, 28);
  MYSQL_TIME mt = static_cast<MYSQL_TIME>(date1);
  Date_val date2 = Date_val{mt};
  EXPECT_EQ(0, date1.compare(date2));
  MYSQL_TIME mytime(2023, 1, 30, 12, 0, 0, 0, false, MYSQL_TIMESTAMP_DATETIME,
                    0);
  Date_val a(2023, 1, 30);
  EXPECT_EQ(Date_val::strip_time(mytime), a);
  Date_val b{mt};
  EXPECT_EQ(0, b.compare(date1));
}
TEST(Date_val, fields) {
  Date_val date(2025, 2, 28);
  EXPECT_EQ(date.year(), 2025);
  EXPECT_EQ(date.month(), 2);
  EXPECT_EQ(date.day(), 28);
  EXPECT_FALSE(date.is_zero_date());
  EXPECT_EQ(date.check_date(0), 0);
  EXPECT_EQ(date.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(date.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(date.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val zerodate(0, 0, 0);
  EXPECT_EQ(zerodate.year(), 0);
  EXPECT_EQ(zerodate.month(), 0);
  EXPECT_EQ(zerodate.day(), 0);
  EXPECT_TRUE(zerodate.is_zero_date());
  EXPECT_EQ(zerodate.check_date(0), 0);
  EXPECT_EQ(zerodate.check_date(TIME_NO_ZERO_DATE), MYSQL_TIME_WARN_ZERO_DATE);
  EXPECT_EQ(zerodate.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(zerodate.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val zeroday(0, 12, 0);
  EXPECT_EQ(zeroday.year(), 0);
  EXPECT_EQ(zeroday.month(), 12);
  EXPECT_EQ(zeroday.day(), 0);
  EXPECT_FALSE(zeroday.is_zero_date());
  EXPECT_EQ(zeroday.check_date(0), 0);
  EXPECT_EQ(zeroday.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(zeroday.check_date(TIME_NO_ZERO_IN_DATE),
            MYSQL_TIME_WARN_ZERO_IN_DATE);
  EXPECT_EQ(zeroday.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val zeromonth(0, 0, 31);
  EXPECT_EQ(zeromonth.year(), 0);
  EXPECT_EQ(zeromonth.month(), 0);
  EXPECT_EQ(zeromonth.day(), 31);
  EXPECT_FALSE(zeromonth.is_zero_date());
  EXPECT_EQ(zeromonth.check_date(0), 0);
  EXPECT_EQ(zeromonth.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(zeromonth.check_date(TIME_NO_ZERO_IN_DATE),
            MYSQL_TIME_WARN_ZERO_IN_DATE);
  EXPECT_EQ(zeromonth.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val invdate1(2024, 2, 30);
  EXPECT_EQ(invdate1.year(), 2024);
  EXPECT_EQ(invdate1.month(), 2);
  EXPECT_EQ(invdate1.day(), 30);
  EXPECT_FALSE(invdate1.is_zero_date());
  EXPECT_EQ(invdate1.check_date(0), 0);
  EXPECT_EQ(invdate1.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(invdate1.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(invdate1.check_date(TIME_NO_INVALID_DATES),
            MYSQL_TIME_WARN_OUT_OF_RANGE);

  Date_val invdate2(2025, 2, 29);
  EXPECT_EQ(invdate2.year(), 2025);
  EXPECT_EQ(invdate2.month(), 2);
  EXPECT_EQ(invdate2.day(), 29);
  EXPECT_FALSE(invdate2.is_zero_date());
  EXPECT_EQ(invdate2.check_date(0), 0);
  EXPECT_EQ(invdate2.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(invdate2.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(invdate2.check_date(TIME_NO_INVALID_DATES),
            MYSQL_TIME_WARN_OUT_OF_RANGE);

  Date_val mindate(0, 1, 1);
  EXPECT_EQ(mindate.year(), 0);
  EXPECT_EQ(mindate.month(), 1);
  EXPECT_EQ(mindate.day(), 1);
  EXPECT_FALSE(mindate.is_zero_date());
  EXPECT_EQ(mindate.check_date(0), 0);
  EXPECT_EQ(mindate.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(mindate.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(mindate.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val maxdate(9999, 12, 31);
  EXPECT_EQ(maxdate.year(), 9999);
  EXPECT_EQ(maxdate.month(), 12);
  EXPECT_EQ(maxdate.day(), 31);
  EXPECT_FALSE(maxdate.is_zero_date());
  EXPECT_EQ(maxdate.check_date(0), 0);
  EXPECT_EQ(maxdate.check_date(TIME_NO_ZERO_DATE), 0);
  EXPECT_EQ(maxdate.check_date(TIME_NO_ZERO_IN_DATE), 0);
  EXPECT_EQ(maxdate.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val leapdate1(0, 2, 29);
  EXPECT_EQ(leapdate1.check_date(TIME_NO_INVALID_DATES),
            MYSQL_TIME_WARN_OUT_OF_RANGE);

  Date_val leapdate2(1900, 2, 29);
  EXPECT_EQ(leapdate2.check_date(TIME_NO_INVALID_DATES),
            MYSQL_TIME_WARN_OUT_OF_RANGE);

  Date_val leapdate3(2000, 2, 29);
  EXPECT_EQ(leapdate3.check_date(TIME_NO_INVALID_DATES), 0);

  Date_val leapdate4(2024, 2, 29);
  EXPECT_EQ(leapdate4.check_date(TIME_NO_INVALID_DATES), 0);
}
TEST(Date_val, compare) {
  Date_val date0(0, 1, 1);
  Date_val date1(0, 1, 2);
  Date_val date2(0, 1, 31);
  Date_val date3(0, 2, 1);
  Date_val date4(0, 2, 28);
  Date_val date5(0, 12, 31);
  Date_val date6(1, 1, 1);
  Date_val date7(1, 12, 31);
  Date_val date8(1000, 1, 1);
  Date_val date9(9999, 1, 1);
  Date_val date10(9999, 12, 31);

  EXPECT_GT(0, date0.compare(date1));
  EXPECT_GT(0, date1.compare(date2));
  EXPECT_GT(0, date2.compare(date3));
  EXPECT_GT(0, date3.compare(date4));
  EXPECT_GT(0, date4.compare(date5));
  EXPECT_GT(0, date5.compare(date6));
  EXPECT_GT(0, date6.compare(date7));
  EXPECT_GT(0, date7.compare(date8));
  EXPECT_GT(0, date8.compare(date9));
  EXPECT_GT(0, date9.compare(date10));
  EXPECT_LT(date0.for_comparison(), date1.for_comparison());
  EXPECT_LT(date1.for_comparison(), date2.for_comparison());
  EXPECT_LT(date2.for_comparison(), date3.for_comparison());
  EXPECT_LT(date3.for_comparison(), date4.for_comparison());
  EXPECT_LT(date4.for_comparison(), date5.for_comparison());
  EXPECT_LT(date5.for_comparison(), date6.for_comparison());
  EXPECT_LT(date6.for_comparison(), date7.for_comparison());
  EXPECT_LT(date7.for_comparison(), date8.for_comparison());
  EXPECT_LT(date8.for_comparison(), date9.for_comparison());
  EXPECT_LT(date9.for_comparison(), date10.for_comparison());
}
TEST(Date_val, to_int) {
  int32_t date1 = Date_val{1, 1, 1}.to_int();
  EXPECT_EQ(date1, 10101);
  int32_t date2 = Date_val{2025, 2, 28}.to_int();
  EXPECT_EQ(date2, 20250228);
  int32_t date3 = Date_val{9999, 12, 31}.to_int();
  EXPECT_EQ(date3, 99991231);
  int32_t date4 = Date_val{0, 0, 0}.to_int();
  EXPECT_EQ(date4, 0);
  int32_t date5 = Date_val{9999, 0, 31}.to_int();
  EXPECT_EQ(date5, 99990031);
  int32_t date6 = Date_val{9999, 12, 0}.to_int();
  EXPECT_EQ(date6, 99991200);
  int32_t date7 = Date_val{2000, 2, 31}.to_int();
  EXPECT_EQ(date7, 20000231);
}
TEST(Date_val, to_double) {
  Date_val date1(1, 1, 1);
  EXPECT_EQ(10101.0e0, date1.to_double());
  Date_val date2(2025, 2, 28);
  EXPECT_EQ(20250228.0e0, date2.to_double());
  Date_val date3(9999, 12, 31);
  EXPECT_EQ(99991231.0e0, date3.to_double());
  Date_val date4(0, 0, 0);
  EXPECT_EQ(0, date4.to_double());
  Date_val date5(9999, 0, 31);
  EXPECT_EQ(99990031.0e0, date5.to_double());
  Date_val date6(9999, 12, 0);
  EXPECT_EQ(99991200.0e0, date6.to_double());
  Date_val date7(2000, 2, 31);
  EXPECT_EQ(20000231.0e0, date7.to_double());
}
TEST(Date_val, to_string) {
  char buffer[11];
  Date_val date1 = Date_val{1, 1, 1};
  size_t buf_len = date1.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "0001-01-01"));
  Date_val date2 = Date_val{2025, 2, 28};
  buf_len = date2.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "2025-02-28"));
  Date_val date3 = Date_val{9999, 12, 31};
  buf_len = date3.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "9999-12-31"));
  Date_val date4 = Date_val{0, 0, 0};
  buf_len = date4.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "0000-00-00"));
  Date_val date5 = Date_val{0, 1, 0};
  buf_len = date5.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "0000-01-00"));
  Date_val date6 = Date_val{0, 0, 1};
  buf_len = date6.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "0000-00-01"));
  Date_val date7 = Date_val{2000, 2, 30};
  buf_len = date7.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "2000-02-30"));
  Date_val date8 = Date_val{2001, 2, 29};
  buf_len = date8.to_string(buffer);
  buffer[buf_len] = 0;
  EXPECT_EQ(0, strcmp(buffer, "2001-02-29"));
}
TEST(Date_val, add) {
  Interval iv;
  Date_val date1{0, 1, 31};
  iv.month = 1;
  EXPECT_TRUE(date1.add(iv, true));
  EXPECT_FALSE(date1.add(iv, false));
  // Year zero is not a leap year:
  EXPECT_EQ(date1, Date_val(0, 2, 28));
  iv.year = 1;
  iv.month = 0;
  EXPECT_FALSE(date1.add(iv, false));
  EXPECT_EQ(date1, Date_val(1, 2, 28));
  iv.year = 0;
  iv.month = 120000;
  EXPECT_TRUE(date1.add(iv, false));
  EXPECT_TRUE(date1.add(iv, true));
  iv.year = 10000;
  iv.month = 0;
  EXPECT_TRUE(date1.add(iv, false));
  EXPECT_TRUE(date1.add(iv, true));

  Date_val date2{2000, 1, 31};
  iv.year = 0;
  iv.month = 1;
  EXPECT_FALSE(date2.add(iv, false));
  EXPECT_EQ(date2, Date_val(2000, 2, 29));
  iv.year = 1;
  iv.month = 0;
  EXPECT_FALSE(date2.add(iv, false));
  EXPECT_EQ(date2, Date_val(2001, 2, 28));

  Date_val date3{0, 1, 1};
  iv.year = 0;
  iv.month = 0;
  iv.day = 3652425;
  EXPECT_TRUE(date3.add(iv, false));
  EXPECT_TRUE(date3.add(iv, true));

  iv.day = 1;
  EXPECT_TRUE(date3.add(iv, true));
  EXPECT_FALSE(date3.add(iv, false));
  EXPECT_EQ(date3, Date_val(0, 1, 2));
  iv.day = 365;
  EXPECT_FALSE(date3.add(iv, false));
  EXPECT_EQ(date3, Date_val(1, 1, 2));

  Date_val date4{2000, 1, 1};
  iv.day = 90;
  EXPECT_FALSE(date4.add(iv, false));
  EXPECT_EQ(date4, Date_val(2000, 3, 31));
  EXPECT_FALSE(date4.add(iv, true));
  EXPECT_EQ(date4, Date_val(2000, 1, 1));

  iv.day = 366;
  EXPECT_FALSE(date4.add(iv, false));
  EXPECT_EQ(date4, Date_val(2001, 1, 1));
  EXPECT_FALSE(date4.add(iv, true));
  EXPECT_EQ(date4, Date_val(2000, 1, 1));

  Date_val date5{2000, 3, 1};
  iv.day = 90;
  EXPECT_FALSE(date5.add(iv, false));
  EXPECT_EQ(date5, Date_val(2000, 5, 30));
  EXPECT_FALSE(date5.add(iv, true));
  EXPECT_EQ(date5, Date_val(2000, 3, 1));

  iv.day = 365;
  EXPECT_FALSE(date5.add(iv, false));
  EXPECT_EQ(date5, Date_val(2001, 3, 1));
  EXPECT_FALSE(date5.add(iv, true));
  EXPECT_EQ(date5, Date_val(2000, 3, 1));

  Date_val date6{9999, 12, 31};
  iv.day = 1;
  EXPECT_TRUE(date6.add(iv, false));
  EXPECT_FALSE(date6.add(iv, true));
  EXPECT_EQ(date6, Date_val(9999, 12, 30));
}
TEST(Date_val, day_number) {
  for (uint32_t daynr = 1; daynr <= 3652424; daynr++) {
    Date_val date{daynr};
    uint32_t number = date.day_number();
    EXPECT_EQ(daynr, number);
  }
}
TEST(Date_val, last_day) {
  Date_val date1{0, 1, 1};
  date1.set_last_day_of_month();
  EXPECT_EQ(date1, Date_val(0, 1, 31));
  Date_val date2{0, 2, 1};
  date2.set_last_day_of_month();
  EXPECT_EQ(date2, Date_val(0, 2, 28));
  Date_val date3{0, 3, 1};
  date3.set_last_day_of_month();
  EXPECT_EQ(date3, Date_val(0, 3, 31));
  Date_val date4{0, 4, 1};
  date4.set_last_day_of_month();
  EXPECT_EQ(date4, Date_val(0, 4, 30));
  Date_val date5{0, 5, 1};
  date5.set_last_day_of_month();
  EXPECT_EQ(date5, Date_val(0, 5, 31));
  Date_val date6{0, 6, 1};
  date6.set_last_day_of_month();
  EXPECT_EQ(date6, Date_val(0, 6, 30));
  Date_val date7{0, 7, 1};
  date7.set_last_day_of_month();
  EXPECT_EQ(date7, Date_val(0, 7, 31));
  Date_val date8{0, 8, 1};
  date8.set_last_day_of_month();
  EXPECT_EQ(date8, Date_val(0, 8, 31));
  Date_val date9{0, 9, 1};
  date9.set_last_day_of_month();
  EXPECT_EQ(date9, Date_val(0, 9, 30));
  Date_val date10{0, 10, 1};
  date10.set_last_day_of_month();
  EXPECT_EQ(date10, Date_val(0, 10, 31));
  Date_val date11{0, 11, 1};
  date11.set_last_day_of_month();
  EXPECT_EQ(date11, Date_val(0, 11, 30));
  Date_val date12{0, 12, 1};
  date12.set_last_day_of_month();
  EXPECT_EQ(date12, Date_val(0, 12, 31));
  Date_val date13{2000, 2, 1};
  date13.set_last_day_of_month();
  EXPECT_EQ(date13, Date_val(2000, 2, 29));
}
}  // namespace Date_val_unittest

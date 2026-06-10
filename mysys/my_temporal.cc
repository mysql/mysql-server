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

/**
  @defgroup MY_TEMPORAL Mysys temporal utilities
  @ingroup MYSYS
  @{

  @file mysys/my_temporal.cc

  Implementation of low level date, time and datetime utilities.
*/

#include "my_temporal.h"
#include <stdio.h>  // for sprintf
#include <cassert>
#include "myisampack.h"  // for mi_uint3korr, mi_int3store, mi_uint2korr
#include "mysql_time.h"  // for MYSQL_TIME, enum_mysql_timestamp_type

using namespace std;

static const uint32_t divisors[] = {1000000, 100000, 10000, 1000, 100, 10};

// clang-format off
static const uint32_t month_days[] =
   {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint32_t leap_days[] =
   {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const uint32_t sum_days[] =
   {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
// clang-format on

bool Time_val::is_adjusted(uint32_t decimals) const {
  return decimals >= DATETIME_MAX_DECIMALS ||
         (microsecond() % divisors[decimals]) == 0;
}

uint32_t Time_val::actual_decimals() const {
  uint32_t micro = microsecond();
  for (uint count = DATETIME_MAX_DECIMALS; count > 0; count--) {
    if (micro % divisors[count - 1] != 0) return count;
  }
  return 0;
}

void Time_val::adjust_fraction(uint32_t decimals, bool round) {
  assert(decimals <= DATETIME_MAX_DECIMALS);
  if (decimals == DATETIME_MAX_DECIMALS) return;
  uint32_t divisor = divisors[decimals];
  uint32_t fraction = microsecond();
  uint32_t remainder = fraction % divisor;
  if (round && remainder >= divisor / 2) {
    fraction = fraction + divisor - remainder;
    if (fraction == 1000000) {
      bool negative = is_negative();
      set_microsecond(0);
      // Avoid possible negative zero
      if (m_value == 0x7FFFFFFFFFFF) {
        m_value = BITS_SIGN;
      }
      // Max value is 838:59:59.000000, thus rounding can never overflow
      (void)add_seconds(negative ? -1 : 1);
    } else {
      set_microsecond(fraction);
    }
  } else {
    set_microsecond(fraction - remainder);
  }
  // Negative zero is converted to positive zero:
  if (m_value == 0x7FFFFFFFFFFF) {
    m_value = BITS_SIGN;
  }
  assert(is_valid());
}

bool Time_val::add(Time_val tv, bool subtract) {
  assert(is_valid() && tv.is_valid());
  int64_t micro1 = unsigned_microsec(hour(), minute(), second(), microsecond());
  if (is_negative()) micro1 = -micro1;
  int64_t micro2 =
      unsigned_microsec(tv.hour(), tv.minute(), tv.second(), tv.microsecond());
  if (tv.is_negative() ^ subtract) micro2 = -micro2;

  int64_t signed_micro = micro1 + micro2;
  bool neg = signed_micro < 0;
  uint64_t micro = neg ? -signed_micro : signed_micro;
  if (micro > MAX_TIME_MICROSEC) {
    return true;
  }
  uint32_t hour = static_cast<uint32_t>(micro / TIME_MULT_HOUR);
  micro %= TIME_MULT_HOUR;
  uint32_t minute = static_cast<uint32_t>(micro / TIME_MULT_MINUTE);
  micro %= TIME_MULT_MINUTE;
  uint32_t second = static_cast<uint32_t>(micro / TIME_MULT_SECOND);
  micro %= TIME_MULT_SECOND;

  *this = Time_val(neg, hour, minute, second, static_cast<uint32_t>(micro));
  return false;
}

bool Time_val::add(Interval &iv, bool subtract) {
  assert(is_valid() && iv.year == 0 && iv.month == 0 && iv.day == 0);
  int64_t micro1 = unsigned_microsec(hour(), minute(), second(), microsecond());
  if (is_negative()) micro1 = -micro1;

  if (iv.second_part > MAX_TIME_MICROSEC) return true;
  uint64_t micro2 = iv.second_part;
  if (iv.second != 0) {
    if (iv.second > MAX_TIME_MICROSEC / TIME_MULT_SECOND) return true;
    micro2 += iv.second * TIME_MULT_SECOND;
    if (micro2 > MAX_TIME_MICROSEC) return true;
  }
  if (iv.minute != 0) {
    if (iv.minute > MAX_TIME_MICROSEC / TIME_MULT_MINUTE) return true;
    micro2 += iv.minute * TIME_MULT_MINUTE;
    if (micro2 > MAX_TIME_MICROSEC) return true;
  }
  if (iv.hour != 0) {
    if (iv.hour > MAX_TIME_MICROSEC / TIME_MULT_HOUR) return true;
    micro2 += iv.hour * TIME_MULT_HOUR;
    if (micro2 > MAX_TIME_MICROSEC) return true;
  }

  if (iv.neg ^ subtract) {
    micro1 -= micro2;
  } else {
    micro1 += micro2;
  }

  bool neg = micro1 < 0;
  uint64_t micro = neg ? -micro1 : micro1;
  if (micro > MAX_TIME_MICROSEC) {
    return true;
  }
  uint32_t hour = static_cast<uint32_t>(micro / TIME_MULT_HOUR);
  micro %= TIME_MULT_HOUR;
  uint32_t minute = static_cast<uint32_t>(micro / TIME_MULT_MINUTE);
  micro %= TIME_MULT_MINUTE;
  uint32_t second = static_cast<uint32_t>(micro / TIME_MULT_SECOND);
  micro %= TIME_MULT_SECOND;

  *this = Time_val(neg, hour, minute, second, static_cast<uint32_t>(micro));
  return false;
}

Time_val Time_val::strip_date(const MYSQL_TIME &mt) {
  assert(mt.time_type == MYSQL_TIMESTAMP_DATETIME);
  return Time_val{mt.neg, mt.hour, mt.minute, mt.second,
                  static_cast<uint32_t>(mt.second_part)};
}

Time_val::operator MYSQL_TIME() const {
  MYSQL_TIME mtime = {0,
                      0,
                      0,
                      hour(),
                      minute(),
                      second(),
                      microsecond(),
                      is_negative(),
                      MYSQL_TIMESTAMP_TIME,
                      0};
  return mtime;
}

void Time_val::store_time(uint8_t *ptr, uint32_t dec) const {
  assert(dec <= DATETIME_MAX_DECIMALS);
  assert(is_valid());
  // Make sure the stored value is properly rounded or truncated
  assert((microsecond() %
          static_cast<int>(log_10_int[DATETIME_MAX_DECIMALS - dec])) == 0);

  uint64_t val = m_value;
  if ((val & BITS_SIGN) == 0) val++;
  uint64_t frac = val & 0xFFFFFF;
  switch (dec) {
    case 0:
    default:
      mi_int3store(ptr, val >> 24);
      break;
    case 1:
    case 2:
      mi_int3store(ptr, val >> 24);
      if ((val & BITS_SIGN) == 0 && frac != 0) {
        frac = 256 - ((16777216 - frac) / 10000ULL);
      } else {
        frac /= 10000ULL;
      }
      ptr[3] = static_cast<uint8_t>(frac);
      break;
    case 4:
    case 3:
      mi_int3store(ptr, val >> 24);
      if ((val & BITS_SIGN) == 0 && frac != 0) {
        frac = 65536 - ((16777216 - frac) / 100ULL);
      } else {
        frac /= 100ULL;
      }
      mi_int2store(ptr + 3, frac);
      break;
    case 5:
    case 6:
      mi_int6store(ptr, val);
      break;
  }
}

void Time_val::load_time(const uint8_t *ptr, uint32_t dec, Time_val *time) {
  assert(dec <= DATETIME_MAX_DECIMALS);

  uint64_t val, frac;

  switch (dec) {
    case 0:
    default:
      val = static_cast<uint64_t>(mi_uint3korr(ptr)) << 24;
      break;
    case 1:
    case 2:
      val = static_cast<uint64_t>(mi_uint3korr(ptr)) << 24;
      frac = static_cast<uint64_t>(ptr[3]);
      if ((val & BITS_SIGN) == 0 && frac != 0) {
        val |= 16777216 - ((256 - frac) * 10000);
      } else {
        val |= frac * 10000;
      }
      break;
    case 3:
    case 4:
      val = static_cast<uint64_t>(mi_uint3korr(ptr)) << 24;
      frac = static_cast<uint64_t>(mi_uint2korr(ptr + 3));
      if ((val & BITS_SIGN) == 0 && frac != 0) {
        val |= 16777216 - ((65536 - frac) * 100);
      } else {
        val |= frac * 100;
      }
      break;
    case 5:
    case 6:
      val = mi_uint6korr(ptr);
      break;
  }
  if ((val & BITS_SIGN) == 0) val--;

  time->m_value = val;
  assert(time->is_valid());
}

int64_t Time_val::to_int_rounded() const {
  Time_val tv = *this;
  tv.adjust_fraction(0, true);
  int64_t val = (tv.hour() * 10000) + (tv.minute() * 100) + tv.second();
  return is_negative() ? -val : val;
}

int64_t Time_val::to_int_truncated() const {
  int64_t val = (hour() * 10000) + (minute() * 100) + second();
  return is_negative() ? -val : val;
}

double Time_val::to_double() const {
  return (is_negative() ? -1 : 1) * (hour() * 10000 + minute() * 100 +
                                     second() + microsecond() / 1000000.);
}

size_t Time_val::to_string(char *buffer, uint32_t dec) const {
  size_t length;
  if (hour() >= 100) {
    length = sprintf(buffer, "%s%3i:%02i:%02i", is_negative() ? "-" : "",
                     hour(), minute(), second());
  } else {
    length = sprintf(buffer, "%s%02i:%02i:%02i", is_negative() ? "-" : "",
                     hour(), minute(), second());
  }
  if (dec != 0) {
    char micro[32];
    (void)sprintf(micro, ".%06i", microsecond());
    memmove(buffer + length, micro, dec + 1);
    length += dec + 1;
  }
  return length;
}

std::string Time_val::to_string() const {
  char buffer[18];
  size_t length = to_string(buffer, DATETIME_MAX_DECIMALS);
  return string{buffer, length};
}

int Date_val::make_date(uint32_t year, uint32_t month, uint32_t day,
                        my_time_flags_t flags, Date_val *date) {
  if (year > DATE_MAX_YEAR || month > DATE_MAX_MONTH || day > DATE_MAX_DAY) {
    return MYSQL_TIME_WARN_OUT_OF_RANGE;
  }
  *date = Date_val(year, month, day);

  return date->check_date(flags);
}

int Date_val::check_date(my_time_flags_t flags) const {
  // With no checks, bail out immediately:
  if (flags == 0) return 0;

  if ((flags & TIME_NO_ZERO_DATE) != 0 && m_value == 0) {
    return MYSQL_TIME_WARN_ZERO_DATE;
  }
  if ((flags & TIME_NO_ZERO_IN_DATE) != 0 && m_value != 0 &&
      (month() == 0 || day() == 0)) {
    return MYSQL_TIME_WARN_ZERO_IN_DATE;
  }
  const bool leap_year = is_leap_year(year());
  const uint32_t *days = leap_year ? leap_days : month_days;
  if ((flags & TIME_NO_INVALID_DATES) != 0 && month() != 0 &&
      day() > days[month() - 1]) {
    return MYSQL_TIME_WARN_OUT_OF_RANGE;
  }
  return 0;
}

/**
  Convert a day number to a date. Day number 1 is returned as date 0000-01-01.

  This function is called from mysqld's print_fatal_signal().
  Do not make changes to this function that make that call unsafe.
*/
void Date_val::day_number_to_date(uint32_t daynr, uint32_t *year,
                                  uint32_t *month, uint32_t *day) {
  assert(daynr > 0 && daynr <= DATE_LAST_DAY);
  uint32_t y = static_cast<uint32_t>(static_cast<uint64_t>(daynr - 1) * 10000 /
                                     DATE_LAST_DAY);
  uint32_t first_day = Date_val(y, 1, 1).day_number();
  if (first_day > daynr) {  // This happens very rarely...
    y--;
    first_day = Date_val(y, 1, 1).day_number();
  }
  assert(first_day <= daynr);
  uint32_t day_of_year = daynr - first_day;
  uint32_t days_in_year;
  while (day_of_year >= (days_in_year = is_leap_year(y) ? DATE_DAYS_IN_LEAP
                                                        : DATE_DAYS_IN_YEAR)) {
    day_of_year -= days_in_year;
    y++;
  }
  uint32_t m = 0;
  const uint32_t *days =
      days_in_year == DATE_DAYS_IN_LEAP ? leap_days : month_days;
  while (day_of_year >= days[m]) {
    day_of_year -= days[m];
    m++;
  }
  *year = y;
  *month = m;
  *day = day_of_year;
}

bool Date_val::add(Interval &iv, bool subtract) {
  // With zero components, an interval cannot be added
  if (month() == 0 || day() == 0) {
    return true;
  }
  const int32_t sign = subtract ^ iv.neg ? -1 : 1;

  uint32_t year_no = year();
  uint32_t month_no = month() - 1;
  uint32_t day_no = day() - 1;
  if (iv.year != 0 || iv.month != 0) {
    // Simple guards against arithmetic overflow when calculating period.
    if (iv.month >= DATE_MAX_YEAR * DATE_MAX_MONTH ||
        iv.year >= DATE_MAX_YEAR) {
      return true;
    }
    int32_t period = (year_no * DATE_MAX_MONTH +
                      sign * static_cast<int32_t>(iv.year) * DATE_MAX_MONTH +
                      month_no + sign * static_cast<int32_t>(iv.month));
    if (period < 0 ||
        period >= static_cast<int32_t>((DATE_MAX_YEAR + 1) * DATE_MAX_MONTH)) {
      return true;
    }
    year_no = static_cast<uint32_t>(period / 12);
    month_no = static_cast<uint32_t>(period % 12);
    // Adjust day if the new month doesn't have enough days
    if (day_no >= month_days[month_no]) {
      day_no = month_days[month_no] - 1;
      if (month_no == 1 && is_leap_year(year_no)) {
        day_no++;  // Leap year
      }
    }
  } else if (iv.day != 0) {
    uint32_t period = day_number();
    if (sign < 0) {
      if (iv.day >= period) {  // Before year 0.
        return true;
      }
      period -= iv.day;
    } else {
      if (iv.day > MAX_DAY_NUMBER || period + iv.day > MAX_DAY_NUMBER) {
        return true;  // After 9999-12-31.
      }
      period += iv.day;
    }
    day_number_to_date(period, &year_no, &month_no, &day_no);
  }
  *this = Date_val(year_no, month_no + 1, day_no + 1);
  return false;
}

void Date_val::set_last_day_of_month() {
  assert(month() != 0);
  const uint32_t *days = is_leap_year(year()) ? leap_days : month_days;
  uint32_t dayno = days[month() - 1];
  m_value = (m_value & ~DATE_BITS_DAY) | dayno;
  assert(is_valid());
}

Date_val Date_val::strip_time(const MYSQL_TIME &mt) {
  assert(mt.time_type == MYSQL_TIMESTAMP_DATETIME);
  return Date_val{mt.year, mt.month, mt.day};
}

Date_val::operator MYSQL_TIME() const {
  MYSQL_TIME mtime = {
      year(), month(), day(), 0, 0, 0, 0, 0, MYSQL_TIMESTAMP_DATE, 0};
  return mtime;
}

Date_val::operator Datetime_val() const {
  MYSQL_TIME dt = {
      year(), month(), day(), 0, 0, 0, 0, 0, MYSQL_TIMESTAMP_DATETIME, 0};
  return *pointer_cast<Datetime_val *>(&dt);
}

int32_t Date_val::to_int() const {
  return year() * 10000 + month() * 100 + day();
}

uint32_t Date_val::day_number() const {
  assert(month() != 0 && day() != 0);
  uint32_t y = year();
  return (y * DATE_DAYS_IN_YEAR) + (y / 4) - (y / 100) + (y / 400) +
         sum_days[month() - 1] + day() -
         (month() <= 2 && is_leap_year(y) ? 1 : 0);
}

size_t Date_val::to_string(char *buffer) const {
  return sprintf(buffer, "%04i-%02i-%02i", year(), month(), day());
}

std::string Date_val::to_string() const {
  char buffer[11];
  size_t length = to_string(buffer);
  return string{buffer, length};
}

/**
   @} (end of defgroup MY_TEMPORAL)
*/

/*
 * Copyright (c) 2015, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "objects/polyglot_date.h"

#include <time.h>
#include <cstdio>
#include <stdexcept>

#include "utils/utils_json.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

Date::Date(const Date &date)
    : Object_bridge(),
      _year(date._year),
      _month(date._month),
      _day(date._day),
      _hour(date._hour),
      _min(date._min),
      _sec(date._sec),
      _usec(date._usec),
      _has_time(date._has_time),
      _has_date(date._has_date) {}

Date::Date(int year, int month, int day, int hour, int min, int sec, int usec)
    : Object_bridge(),
      _year(year),
      _month(month - 1),
      _day(day),
      _hour(hour),
      _min(min),
      _sec(sec),
      _usec(usec),
      _has_time(true),
      _has_date(true) {
  validate();
}

Date::Date(int year, int month, int day)
    : Object_bridge(),
      _year(year),
      _month(month - 1),
      _day(day),
      _hour(0),
      _min(0),
      _sec(0),
      _usec(0),
      _has_time(false),
      _has_date(true) {
  validate();
}

Date::Date(int hour, int min, int sec, int usec)
    : Object_bridge(),
      _year(0),
      _month(0),
      _day(0),
      _hour(hour),
      _min(min),
      _sec(sec),
      _usec(usec),
      _has_time(true),
      _has_date(false) {
  validate();
}

bool Date::operator==(const Object_bridge &other) const {
  if (other.class_name() == "Date") {
    return *this == *static_cast<const Date *>(&other);
  }
  return false;
}

bool Date::operator==(const Date &other) const {
  if (_year == other._year && _month == other._month && _day == other._day &&
      _hour == other._hour && _min == other._min && _sec == other._sec &&
      _usec == other._usec && _has_time == other._has_time)
    return true;
  return false;
}

std::string &Date::append_descr(std::string &s_out, int /*indent*/,
                                int quote_strings) const {
  if (quote_strings) s_out.push_back(quote_strings);

  if (_has_time) {
    if (_has_date) {
      if (_usec != 0)
        s_out.append(str_format("%04d-%02d-%02d %02d:%02d:%02d.%06d", _year,
                                (_month + 1), _day, _hour, _min, _sec, _usec));
      else
        s_out.append(str_format("%04d-%02d-%02d %02d:%02d:%02d", _year,
                                (_month + 1), _day, _hour, _min, _sec));
    } else {
      if (_usec != 0)
        s_out.append(
            str_format("%02d:%02d:%02d.%06d", _hour, _min, _sec, _usec));
      else
        s_out.append(str_format("%02d:%02d:%02d", _hour, _min, _sec));
    }
  } else {
    s_out.append(str_format("%04d-%02d-%02d", _year, (_month + 1), _day));
  }
  if (quote_strings) s_out.push_back(quote_strings);
  return s_out;
}

std::string &Date::append_repr(std::string &s_out) const {
  return append_descr(s_out, 0, '"');
}

void Date::append_json(shcore::JSON_dumper &dumper) const {
  dumper.start_object();

  if (_has_date) {
    dumper.append_int("year", _year);
    dumper.append_int("month", _month + 1);
    dumper.append_int("day", _day);
  }
  if (_has_time) {
    dumper.append_int("hour", _hour);
    dumper.append_int("minute", _min);
    dumper.append_int("second", _sec);
    dumper.append_int("usecond", _usec);
  }
  dumper.end_object();
}

namespace {
int decimal_to_usec(char dec[7]) {
  for (int i = 0; i < 6; i++) {
    if (dec[i] == 0) dec[i] = '0';
  }
  dec[6] = 0;

  return std::stoi(dec);
}
}  // namespace

Date Date::unrepr(const std::string &s) {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;
  char usec[7] = {0};

  if (s.find(':') != std::string::npos && s.find('-') == std::string::npos) {
    int c = sscanf(s.c_str(), "%d:%d:%d.%6s", &hour, &min, &sec, usec);
    if (c >= 2) {
      return Date(hour, min, sec, decimal_to_usec(usec));
    }
  }

  int c = sscanf(s.c_str(), "%d-%d-%d %d:%d:%d.%6s", &year, &month, &day, &hour,
                 &min, &sec, usec);
  if (c == 3)
    return Date(year, month, day);
  else if (c >= 6)
    return Date(year, month, day, hour, min, sec, decimal_to_usec(usec));
  else
    throw std::invalid_argument("Invalid date value '" + s + "'");
}

int64_t Date::as_ms() const {
  struct tm t;
  // caution, this obviously doesnt work for dates before 1970 yr
  t.tm_year = _year - 1900;
  t.tm_mon = _month;
  t.tm_mday = _day;
  t.tm_hour = _hour;
  t.tm_min = _min;
  t.tm_sec = _sec;

  int64_t seconds_since_epoch = mktime(&t);

  return seconds_since_epoch * 1000 + _usec / 1000;
}

Date Date::from_ms(int64_t ms_since_epoch) {
  int ms = ms_since_epoch % 1000;
  time_t seconds_since_epoch = ms_since_epoch / 1000;

  struct tm t;
#ifdef _WIN32
  localtime_s(&t, &seconds_since_epoch);
#else
  localtime_r(&seconds_since_epoch, &t);
#endif

  return Date(t.tm_year + 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min,
              t.tm_sec, ms * 1000);
}

void Date::validate() {
  if (_has_date) {
    if (_year > 9999 || _year < 0)
      throw std::invalid_argument("Valid year range is 0-9999");
    if (_month > 11 || _month + 1 < 0)
      throw std::invalid_argument("Valid month range is 0-12");
    if (_day > 31 || _day < 0)
      throw std::invalid_argument("Valid day range is 0-31");
  }
  if (_has_time) {
    if (_hour > 23 || _hour < 0)
      throw std::invalid_argument("Valid hour range is 0-23");
    if (_min > 59 || _min < 0)
      throw std::invalid_argument("Valid minute range is 0-59");
    if (_sec > 59 || _sec < 0)
      throw std::invalid_argument("Valid second range is 0-59");
    if (_usec > 999999 || _usec < 0)
      throw std::invalid_argument("Valid millisecond range is 0-999999");
  }
}
}  // namespace polyglot
}  // namespace shcore
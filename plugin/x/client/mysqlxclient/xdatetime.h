/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_CLIENT_MYSQLXCLIENT_XDATETIME_H_
#define PLUGIN_X_CLIENT_MYSQLXCLIENT_XDATETIME_H_

#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

namespace xcl {

class Time {
 public:
  Time()
      : m_negate(false),
        m_hour(0),
        m_minutes(0),
        m_seconds(0),
        m_useconds(0),
        m_valid(false) {}

  Time(const bool negate_, const uint32_t hour_, const uint8_t minutes_,
       const uint8_t seconds_, const uint32_t useconds_ = 0)
      : m_negate(negate_),
        m_hour(hour_),
        m_minutes(minutes_),
        m_seconds(seconds_),
        m_useconds(useconds_),
        m_valid(true) {
    if (minutes_ > 59 || seconds_ > 59 || useconds_ >= 1000000) {
      m_valid = false;
    }
  }

  bool valid() const { return m_valid; }

  explicit operator bool() const { return m_valid; }

  bool is_negate() const { return m_negate; }

  uint32_t hour() const { return m_hour; }
  uint8_t minutes() const { return m_minutes; }
  uint8_t seconds() const { return m_seconds; }
  uint32_t useconds() const { return m_useconds; }

  std::string to_string() const {
    std::stringstream ss;

    if (!valid()) return "";

    ss.fill('0');
    ss << (is_negate() ? "-" : "") << std::setw(2) << static_cast<int>(hour())
       << m_time_separator << std::setw(2) << static_cast<int>(minutes())
       << m_time_separator << std::setw(2) << static_cast<int>(seconds())
       << us_to_str(useconds());

    return ss.str();
  }

  static std::string us_to_str(uint32_t val) {
    if (0 == val) return "";

    const uint8_t k_us_length = 6;

    auto val_representation = std::to_string(val);
    // Add preceding '0's
    std::string result(k_us_length - val_representation.length(), '0');
    result += val_representation;
    const size_t last = result.find_last_not_of('0') + 1;
    result.erase(last);

    return "." + result;
  }

  void set_time_separator(char separator) { m_time_separator = separator; }

 private:
  bool m_negate;
  uint32_t m_hour;
  uint8_t m_minutes;
  uint8_t m_seconds;
  uint32_t m_useconds;
  bool m_valid;
  char m_time_separator = ':';
};

class DateTime {
 public:
  DateTime() = default;

  DateTime(const uint16_t year_, const uint8_t month_, const uint8_t day_)
      : DateTime(year_, month_, day_, 0xFF, 0xFF, 0xFF, 0xFFFFFF) {
    m_has_time_part = false;
  }

  DateTime(const uint16_t year_, const uint8_t month_, const uint8_t day_,
           const uint8_t hour_, const uint8_t minutes_, const uint8_t seconds_,
           const uint32_t useconds_)
      : m_year(year_),
        m_month(month_),
        m_day(day_),
        m_hour(hour_),
        m_minutes(minutes_),
        m_seconds(seconds_),
        m_useconds(useconds_),
        m_valid((year_ > 9999 || month_ > 12 || day_ > 31) ? false : true),
        m_has_time_part((hour_ > 23 || minutes_ > 59 || seconds_ > 59 ||
                         useconds_ >= 1000000)
                            ? false
                            : true) {}

  bool valid() const { return m_valid; }

  explicit operator bool() const { return m_valid; }

  bool has_time() const { return m_has_time_part; }

  std::string to_string() const {
    if (!valid()) return "";

    std::stringstream ss;
    ss.fill('0');

    ss << std::setw(4) << year() << m_date_separator << std::setw(2)
       << static_cast<int>(month()) << m_date_separator << std::setw(2)
       << static_cast<int>(day());

    if (has_time()) {
      ss << " " << std::setw(2) << static_cast<int>(hour()) << m_time_separator
         << std::setw(2) << static_cast<int>(minutes()) << m_time_separator
         << std::setw(2) << static_cast<int>(seconds())
         << Time::us_to_str(useconds());
    }

    return ss.str();
  }

  uint16_t year() const { return m_year; }
  uint8_t month() const { return m_month; }
  uint8_t day() const { return m_day; }

  uint8_t hour() const { return m_hour; }
  uint8_t minutes() const { return m_minutes; }
  uint8_t seconds() const { return m_seconds; }
  uint32_t useconds() const { return m_useconds; }
  Time time() const {
    return Time(false, m_hour, m_minutes, m_seconds, m_useconds);
  }

  void set_time_separator(char separator) { m_time_separator = separator; }
  void set_date_separator(char separator) { m_date_separator = separator; }

 private:
  uint16_t m_year;
  uint8_t m_month;
  uint8_t m_day;

  uint8_t m_hour = 0xFF;
  uint8_t m_minutes = 0xFF;
  uint8_t m_seconds = 0xFF;
  uint32_t m_useconds = 0xFFFFFF;

  bool m_valid;

  char m_time_separator = ':';
  char m_date_separator = '-';
  bool m_has_time_part;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MYSQLXCLIENT_XDATETIME_H_

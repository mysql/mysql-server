/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef _MYSQLX_DATETIME_H_
#define _MYSQLX_DATETIME_H_

#include <string>
#include <stdint.h>
#include <ostream>
#include <ios>
#include <iomanip>
#include <sstream>

namespace mysqlx
{
  class Time
  {
  public:
    Time() : m_negate(false), m_hour(0), m_minutes(0), m_seconds(0),
      m_useconds(0), m_valid(false)
    {}

    Time(bool negate_,
         uint32_t hour_,
         uint8_t minutes_,
         uint8_t seconds_,
         uint32_t useconds_ = 0)
    : m_negate(negate_), m_hour(hour_), m_minutes(minutes_),
      m_seconds(seconds_), m_useconds(useconds_), m_valid(true)
    {
      if (minutes_ > 59 || seconds_ > 59 || useconds_ >= 1000000)
        m_valid = false;
    }

    bool valid() const
    {
      return m_valid;
    }

    operator bool () const
    {
      return m_valid;
    }

    bool negate() const { return m_negate; }

    uint32_t hour() const { return m_hour; }
    uint8_t minutes() const { return m_minutes; }
    uint8_t seconds() const { return m_seconds; }
    uint32_t useconds() const { return m_useconds; }

    std::string to_string() const
    {
      std::stringstream ss;

      if (!valid())
        return "";

      ss.fill('0');
      ss <<  (negate() ? "-" : "")
        << std::setw(2) << static_cast<int>(hour())
        << ":" << std::setw(2) << static_cast<int>(minutes())
        << ":" << std::setw(2) << static_cast<int>(seconds())
        << us_to_str(useconds());

      return ss.str();
    }

    static std::string us_to_str(uint32_t val)
    {
      if (0 == val)
        return "";

      std::stringstream ss;
      ss << "." <<  val;
      std::string result = ss.str();
      size_t last = result.find_last_not_of('0') + 1;
      result.erase(last);

      return result;
    }

  private:
    bool m_negate;
    uint32_t m_hour;
    uint8_t m_minutes;
    uint8_t m_seconds;
    uint32_t m_useconds;
    bool m_valid;
  };


  class DateTime
  {
  public:
    DateTime(uint16_t year_,
             uint8_t month_,
             uint8_t day_,

             uint8_t hour_ = 0xff,
             uint8_t minutes_ = 0,
             uint8_t seconds_ = 0,
             uint32_t useconds_ = 0)
    : m_year(year_), m_month(month_), m_day(day_),
    m_hour(hour_), m_minutes(minutes_), m_seconds(seconds_),
    m_useconds(useconds_), m_valid(true)
    {
      if (year_ > 9999 || month_ > 12 || day_ > 31)
        m_valid = false;
      if (hour_ != 0xff)
      {
        if (hour_ > 23 || minutes_ > 59 || seconds_ > 59 || useconds_ >= 1000000)
          m_valid = false;
      }
    }

    DateTime() {}

    bool valid() const
    {
      return m_valid;
    }

    operator bool () const
    {
      return m_valid;
    }

    bool has_time() const
    {
      return m_hour != 0xff;
    }

    std::string to_string() const
    {
      if (!valid())
        return "";

      std::stringstream ss;
      ss.fill('0');

      ss << std::setw(4) << year() << "/"
        << std::setw(2) << static_cast<int>(month()) << "/"
        << std::setw(2) << static_cast<int>(day());

      if (has_time())
      {
        ss << " " << std::setw(2) << static_cast<int>(hour())
          << ":" << std::setw(2) << static_cast<int>(minutes())
          << ":" << std::setw(2) << static_cast<int>(seconds())
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
    Time time() const { return Time(false, m_hour, m_minutes, m_seconds, m_useconds); }

  private:
    uint16_t m_year;
    uint8_t m_month;
    uint8_t m_day;

    uint8_t m_hour;
    uint8_t m_minutes;
    uint8_t m_seconds;
    uint32_t m_useconds;

    bool m_valid;
  };


  inline std::ostream & operator<<(std::ostream &os, const DateTime &v)
  {
    if (v)
    {
      os << v.to_string();
    }
    return os;
  }

  inline std::ostream & operator<<(std::ostream &os, const Time &v)
  {
    if (v)
    {
      os << v.to_string();
    }
    return os;
  }
}

#endif

/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_BINARY_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_BINARY_H_

#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <system_error>  // error_code
#include <utility>       // move

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_wire.h"

namespace classic_protocol {
namespace borrowable {
namespace binary {

template <bool Borrowed>
using string_type = std::conditional_t<Borrowed, std::string_view, std::string>;

/**
 * base type of all binary scalar value types.
 */
template <class T>
class TypeBase {
 public:
  using value_type = T;

  constexpr TypeBase(value_type val) : v_(val) {}

  [[nodiscard]] constexpr value_type value() const { return v_; }

  friend bool operator==(const TypeBase &lhs, const TypeBase &rhs) {
    return lhs.value() == rhs.value();
  }

  friend bool operator!=(const TypeBase &lhs, const TypeBase &rhs) {
    return !(lhs == rhs);
  }

 private:
  value_type v_;
};

/**
 * POD base-type for Datetime, Timestamp, Date.
 */
class DatetimeBase {
 public:
  DatetimeBase() = default;

  constexpr DatetimeBase(uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second,
                         uint32_t microsecond = 0)
      : year_(year),
        month_(month),
        day_(day),
        hour_(hour),
        minute_(minute),
        second_(second),
        microsecond_(microsecond) {}

  constexpr DatetimeBase(uint16_t year, uint8_t month, uint8_t day)
      : DatetimeBase(year, month, day, 0, 0, 0, 0) {}

  [[nodiscard]] constexpr uint16_t year() const { return year_; }
  [[nodiscard]] constexpr uint8_t month() const { return month_; }
  [[nodiscard]] constexpr uint8_t day() const { return day_; }
  [[nodiscard]] constexpr uint8_t hour() const { return hour_; }
  [[nodiscard]] constexpr uint8_t minute() const { return minute_; }
  [[nodiscard]] constexpr uint8_t second() const { return second_; }
  [[nodiscard]] constexpr uint32_t microsecond() const { return microsecond_; }

  friend bool operator==(const DatetimeBase &lhs, const DatetimeBase &rhs) {
    return lhs.year() == rhs.year() &&      //
           lhs.month() == rhs.month() &&    //
           lhs.day() == rhs.day() &&        //
           lhs.hour() == rhs.hour() &&      //
           lhs.minute() == rhs.minute() &&  //
           lhs.second() == rhs.second() &&  //
           lhs.microsecond() == rhs.microsecond();
  }

  friend bool operator!=(const DatetimeBase &lhs, const DatetimeBase &rhs) {
    return !(lhs == rhs);
  }

 private:
  uint16_t year_{};
  uint8_t month_{};
  uint8_t day_{};
  uint8_t hour_{};
  uint8_t minute_{};
  uint8_t second_{};
  uint32_t microsecond_{};
};

class DateTime : public DatetimeBase {
 public:
  using DatetimeBase::DatetimeBase;
};

class Timestamp : public DatetimeBase {
 public:
  using DatetimeBase::DatetimeBase;
};

class Date : public DatetimeBase {
 public:
  using DatetimeBase::DatetimeBase;
};

class Time {
 public:
  constexpr Time() = default;

  constexpr Time(bool is_negative, uint32_t days, uint8_t hour, uint8_t minute,
                 uint8_t second, uint32_t microsecond = 0)
      : is_negative_(is_negative),
        days_(days),
        hour_(hour),
        minute_(minute),
        second_(second),
        microsecond_(microsecond) {}

  [[nodiscard]] constexpr bool is_negative() const { return is_negative_; }
  [[nodiscard]] constexpr uint32_t days() const { return days_; }
  [[nodiscard]] constexpr uint8_t hour() const { return hour_; }
  [[nodiscard]] constexpr uint8_t minute() const { return minute_; }
  [[nodiscard]] constexpr uint8_t second() const { return second_; }
  [[nodiscard]] constexpr uint32_t microsecond() const { return microsecond_; }

  friend bool operator==(const Time &lhs, const Time &rhs) {
    return lhs.days() == rhs.days() &&                //
           lhs.hour() == rhs.hour() &&                //
           lhs.minute() == rhs.minute() &&            //
           lhs.second() == rhs.second() &&            //
           lhs.microsecond() == rhs.microsecond() &&  //
           lhs.is_negative() == rhs.is_negative();
  }

  friend bool operator!=(const Time &lhs, const Time &rhs) {
    return !(lhs == rhs);
  }

 private:
  bool is_negative_{};
  uint32_t days_{};
  uint8_t hour_{};
  uint8_t minute_{};
  uint8_t second_{};
  uint32_t microsecond_{};
};

class LongLong : public TypeBase<uint64_t> {
 public:
  using TypeBase::TypeBase;
};

class Long : public TypeBase<uint32_t> {
 public:
  using TypeBase::TypeBase;
};

class Int24 : public TypeBase<uint32_t> {
 public:
  using TypeBase::TypeBase;
};

class Short : public TypeBase<uint16_t> {
 public:
  using TypeBase::TypeBase;
};

class Year : public TypeBase<uint16_t> {
 public:
  using TypeBase::TypeBase;
};

class Tiny : public TypeBase<uint8_t> {
 public:
  using TypeBase::TypeBase;
};

class Double : public TypeBase<double> {
 public:
  using TypeBase::TypeBase;
};

class Float : public TypeBase<float> {
 public:
  using TypeBase::TypeBase;
};

template <bool Borrowed>
class String : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class VarString : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Varchar : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Json : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Blob : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class TinyBlob : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class MediumBlob : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class LongBlob : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Enum : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Set : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Decimal : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class NewDecimal : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Bit : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

template <bool Borrowed>
class Geometry : public TypeBase<string_type<Borrowed>> {
 public:
  using TypeBase<string_type<Borrowed>>::TypeBase;
};

class Null {};

}  // namespace binary
}  // namespace borrowable

namespace borrowed {
namespace binary {
using Tiny = borrowable::binary::Tiny;
using Short = borrowable::binary::Short;
using Int24 = borrowable::binary::Int24;
using Long = borrowable::binary::Long;
using LongLong = borrowable::binary::LongLong;
using Float = borrowable::binary::Float;
using Double = borrowable::binary::Double;
using Year = borrowable::binary::Year;
using Time = borrowable::binary::Time;
using Date = borrowable::binary::Date;
using DateTime = borrowable::binary::DateTime;
using Timestamp = borrowable::binary::Timestamp;
using String = borrowable::binary::String<true>;
using VarString = borrowable::binary::VarString<true>;
using Varchar = borrowable::binary::Varchar<true>;
using TinyBlob = borrowable::binary::TinyBlob<true>;
using Blob = borrowable::binary::Blob<true>;
using MediumBlob = borrowable::binary::MediumBlob<true>;
using LongBlob = borrowable::binary::LongBlob<true>;
using Enum = borrowable::binary::Enum<true>;
using Set = borrowable::binary::Set<true>;
using Decimal = borrowable::binary::Decimal<true>;
using NewDecimal = borrowable::binary::NewDecimal<true>;
using Json = borrowable::binary::Json<true>;
using Geometry = borrowable::binary::Geometry<true>;
using Bit = borrowable::binary::Bit<true>;
using Null = borrowable::binary::Null;
}  // namespace binary
}  // namespace borrowed

namespace binary {
using Tiny = borrowable::binary::Tiny;
using Short = borrowable::binary::Short;
using Int24 = borrowable::binary::Int24;
using Long = borrowable::binary::Long;
using LongLong = borrowable::binary::LongLong;
using Float = borrowable::binary::Float;
using Double = borrowable::binary::Double;
using Year = borrowable::binary::Year;
using Time = borrowable::binary::Time;
using Date = borrowable::binary::Date;
using DateTime = borrowable::binary::DateTime;
using Timestamp = borrowable::binary::Timestamp;
using String = borrowable::binary::String<false>;
using VarString = borrowable::binary::VarString<false>;
using Varchar = borrowable::binary::Varchar<false>;
using TinyBlob = borrowable::binary::TinyBlob<false>;
using Blob = borrowable::binary::Blob<false>;
using MediumBlob = borrowable::binary::MediumBlob<false>;
using LongBlob = borrowable::binary::LongBlob<false>;
using Enum = borrowable::binary::Enum<false>;
using Set = borrowable::binary::Set<false>;
using Decimal = borrowable::binary::Decimal<false>;
using NewDecimal = borrowable::binary::NewDecimal<false>;
using Json = borrowable::binary::Json<false>;
using Geometry = borrowable::binary::Geometry<false>;
using Bit = borrowable::binary::Bit<false>;
using Null = borrowable::binary::Null;
}  // namespace binary
}  // namespace classic_protocol

#endif

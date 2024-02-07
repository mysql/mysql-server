/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_BINARY_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_BINARY_H_

#include <cstddef>       // size_t
#include <cstdint>       // uint8_t
#include <cstring>       // memcpy
#include <system_error>  // error_code
#include <utility>       // move

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_binary.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlrouter/classic_protocol_wire.h"

namespace classic_protocol {

namespace impl {
template <class T>
struct BinaryTypeBase;

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Decimal<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Decimal;
};

template <>
struct BinaryTypeBase<borrowable::binary::Tiny> {
  static constexpr const uint16_t binary_field_type = field_type::Tiny;
  static constexpr const uint8_t byte_size = 1;
};

template <>
struct BinaryTypeBase<borrowable::binary::Short> {
  static constexpr const uint16_t binary_field_type = field_type::Short;
  static constexpr const uint8_t byte_size = 2;
};

template <>
struct BinaryTypeBase<borrowable::binary::Long> {
  static constexpr const uint16_t binary_field_type = field_type::Long;
  static constexpr const uint8_t byte_size = 4;
};

template <>
struct BinaryTypeBase<borrowable::binary::Float> {
  static constexpr const uint16_t binary_field_type = field_type::Float;
  static constexpr const uint8_t byte_size = 4;
};

template <>
struct BinaryTypeBase<borrowable::binary::Double> {
  static constexpr const uint16_t binary_field_type = field_type::Double;
  static constexpr const uint8_t byte_size = 8;
};

template <>
struct BinaryTypeBase<borrowable::binary::Null> {
  static constexpr const uint16_t binary_field_type = field_type::Null;
};

template <>
struct BinaryTypeBase<borrowable::binary::Timestamp> {
  static constexpr const uint16_t binary_field_type = field_type::Timestamp;
};

template <>
struct BinaryTypeBase<borrowable::binary::LongLong> {
  static constexpr const uint16_t binary_field_type = field_type::LongLong;
  static constexpr const uint8_t byte_size = 8;
};

template <>
struct BinaryTypeBase<borrowable::binary::Int24> {
  static constexpr const uint16_t binary_field_type = field_type::Int24;
  static constexpr const uint8_t byte_size = 4;
};

template <>
struct BinaryTypeBase<borrowable::binary::Date> {
  static constexpr const uint16_t binary_field_type = field_type::Date;
};

template <>
struct BinaryTypeBase<borrowable::binary::Time> {
  static constexpr const uint16_t binary_field_type = field_type::Time;
};

template <>
struct BinaryTypeBase<borrowable::binary::DateTime> {
  static constexpr const uint16_t binary_field_type = field_type::DateTime;
};

template <>
struct BinaryTypeBase<borrowable::binary::Year> {
  static constexpr const uint16_t binary_field_type = field_type::Year;
  static constexpr const uint8_t byte_size = 2;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Varchar<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Varchar;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Bit<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Bit;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Json<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Json;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::NewDecimal<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::NewDecimal;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Enum<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Enum;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Set<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Set;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::TinyBlob<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::TinyBlob;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::MediumBlob<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::MediumBlob;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::LongBlob<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::LongBlob;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Blob<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Blob;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::String<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::String;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::VarString<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::VarString;
};

template <bool Borrowed>
struct BinaryTypeBase<borrowable::binary::Geometry<Borrowed>> {
  static constexpr const uint16_t binary_field_type = field_type::Geometry;
};

// codec base

template <class T>
class FixedIntCodec : public impl::EncodeBase<FixedIntCodec<T>> {
  static constexpr const int byte_size = BinaryTypeBase<T>::byte_size;

  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    accu.step(wire::FixedInt<byte_size>(val_.value()));

    return accu.result();
  }

 public:
  using value_type = T;
  using base_ = impl::EncodeBase<FixedIntCodec<value_type>>;

  friend base_;

  constexpr FixedIntCodec(value_type val, capabilities::value_type caps)
      : base_(caps), val_{std::move(val)} {}

  static constexpr uint16_t type() {
    return BinaryTypeBase<T>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto value_res = accu.template step<wire::FixedInt<byte_size>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(value_res->value()));
  }

 private:
  const value_type val_;
};

template <class T>
class FloatCodec : public impl::EncodeBase<FloatCodec<T>> {
  static constexpr const int byte_size = BinaryTypeBase<T>::byte_size;

  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    // reinterpret_cast<> isn't allowed in constexpr.
    //
    // Use a union as a workaround.
    union overlapped_storage {
      constexpr overlapped_storage(typename value_type::value_type val)
          : val_(val) {}

      typename value_type::value_type val_;

      char addr_[sizeof(val_)];
    } overlapped(val_.value());

    static_assert(sizeof(overlapped.val_) == byte_size);

    return accu
        .step(borrowed::wire::String{
            std::string_view((&overlapped.addr_[0]), sizeof(overlapped.val_))})
        .result();
  }

 public:
  using value_type = T;
  using base_ = impl::EncodeBase<FloatCodec<value_type>>;

  friend base_;

  constexpr FloatCodec(value_type val, capabilities::value_type caps)
      : base_(caps), val_{std::move(val)} {}

  static constexpr uint16_t type() {
    return BinaryTypeBase<T>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto value_res = accu.template step<borrowed::wire::String>(byte_size);
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    typename T::value_type val;

    memcpy(&val, value_res->value().data(), byte_size);

    return std::make_pair(accu.result().value(), value_type(val));
  }

 private:
  const value_type val_;
};

template <bool Borrowed, class T>
class StringCodec : public impl::EncodeBase<StringCodec<Borrowed, T>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    accu.step(borrowable::wire::String<Borrowed>(val_.value()));

    return accu.result();
  }

 public:
  using value_type = T;
  using base_ = impl::EncodeBase<StringCodec<Borrowed, value_type>>;

  friend base_;

  constexpr StringCodec(value_type val, capabilities::value_type caps)
      : base_(caps), val_{std::move(val)} {}

  static constexpr uint16_t type() {
    return BinaryTypeBase<T>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto value_res = accu.template step<borrowable::wire::String<Borrowed>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(accu.result().value(),
                          value_type(value_res->value()));
  }

 private:
  const value_type val_;
};

template <class T>
class DatetimeCodec : public impl::EncodeBase<DatetimeCodec<T>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    const bool has_ms = val_.microsecond();
    const bool has_time =
        val_.hour() || val_.minute() || val_.second() || has_ms;
    const bool has_date = val_.year() || val_.month() || val_.day() || has_time;

    if (has_date) {
      accu.step(wire::FixedInt<2>(val_.year()))
          .step(wire::FixedInt<1>(val_.month()))
          .step(wire::FixedInt<1>(val_.day()));
      if (has_time) {
        accu.step(wire::FixedInt<1>(val_.hour()))
            .step(wire::FixedInt<1>(val_.minute()))
            .step(wire::FixedInt<1>(val_.second()));
        if (has_ms) {
          accu.step(wire::FixedInt<4>(val_.microsecond()));
        }
      }
    }
    return accu.result();
  }

 public:
  using value_type = T;
  using base_ = impl::EncodeBase<DatetimeCodec<value_type>>;

  friend base_;

  constexpr DatetimeCodec(value_type val, capabilities::value_type caps)
      : base_(caps), val_{std::move(val)} {}

  static constexpr uint16_t type() {
    return BinaryTypeBase<T>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto year_res = accu.template try_step<wire::FixedInt<2>>();
    if (!year_res) {
      return std::make_pair(accu.result().value(), value_type());
    }

    auto month_res = accu.template step<wire::FixedInt<1>>();
    auto day_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto hour_res = accu.template try_step<wire::FixedInt<1>>();
    if (!hour_res) {
      return std::make_pair(
          accu.result().value(),
          value_type(year_res->value(), month_res->value(), day_res->value()));
    }

    auto minute_res = accu.template step<wire::FixedInt<1>>();
    auto second_res = accu.template step<wire::FixedInt<1>>();
    if (!accu.result()) return stdx::unexpected(accu.result().error());

    auto microsecond_res = accu.template try_step<wire::FixedInt<4>>();
    if (!microsecond_res) {
      return std::make_pair(
          accu.result().value(),
          value_type(year_res->value(), month_res->value(), day_res->value(),
                     hour_res->value(), minute_res->value(),
                     second_res->value()));
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(year_res->value(), month_res->value(), day_res->value(),
                   hour_res->value(), minute_res->value(), second_res->value(),
                   microsecond_res->value()));
  }

 private:
  value_type val_;
};

template <class T>
class TimeCodec : public impl::EncodeBase<TimeCodec<T>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    if (val_.days() || val_.hour() || val_.minute() || val_.second() ||
        val_.microsecond()) {
      accu.step(wire::FixedInt<1>(val_.is_negative()))
          .step(wire::FixedInt<4>(val_.days()))
          .step(wire::FixedInt<1>(val_.hour()))
          .step(wire::FixedInt<1>(val_.minute()))
          .step(wire::FixedInt<1>(val_.second()));
      if (val_.microsecond()) {
        accu.step(wire::FixedInt<4>(val_.microsecond()));
      }
    }
    return accu.result();
  }

 public:
  using value_type = T;
  using base_ = impl::EncodeBase<TimeCodec<value_type>>;

  friend base_;

  constexpr TimeCodec(value_type val, capabilities::value_type caps)
      : base_(caps), val_{std::move(val)} {}

  static constexpr uint16_t type() {
    return BinaryTypeBase<T>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer &buffer, capabilities::value_type caps) {
    impl::DecodeBufferAccumulator accu(buffer, caps);

    auto is_negative_res = accu.template try_step<wire::FixedInt<1>>();
    if (!is_negative_res) {
      return std::make_pair(accu.result().value(), value_type());
    }
    auto days_res = accu.template step<wire::FixedInt<4>>();
    auto hour_res = accu.template step<wire::FixedInt<1>>();
    auto minute_res = accu.template step<wire::FixedInt<1>>();
    auto second_res = accu.template step<wire::FixedInt<1>>();
    auto microsecond_res = accu.template try_step<wire::FixedInt<4>>();

    if (!microsecond_res) {
      if (!accu.result()) return stdx::unexpected(accu.result().error());

      return std::make_pair(
          accu.result().value(),
          value_type(is_negative_res->value(), days_res->value(),
                     hour_res->value(), minute_res->value(),
                     second_res->value()));
    }

    if (!accu.result()) return stdx::unexpected(accu.result().error());

    return std::make_pair(
        accu.result().value(),
        value_type(is_negative_res->value(), days_res->value(),
                   hour_res->value(), minute_res->value(), second_res->value(),
                   microsecond_res->value()));
  }

 private:
  const value_type val_;
};

}  // namespace impl

template <>
class Codec<borrowable::binary::Null>
    : public impl::EncodeBase<Codec<borrowable::binary::Null>> {
  template <class Accumulator>
  constexpr auto accumulate_fields(Accumulator &&accu) const {
    return accu.result();
  }

 public:
  using value_type = borrowable::binary::Null;
  using base_ = impl::EncodeBase<Codec<value_type>>;

  friend base_;

  constexpr Codec(value_type /* val */, capabilities::value_type caps)
      : base_(caps) {}

  static constexpr uint16_t type() {
    return impl::BinaryTypeBase<value_type>::binary_field_type;
  }

  static stdx::expected<std::pair<size_t, value_type>, std::error_code> decode(
      const net::const_buffer & /* buffer */,
      capabilities::value_type /* caps */) {
    return std::make_pair(0, value_type());
  }
};

template <>
class Codec<borrowable::binary::LongLong>
    : public impl::FixedIntCodec<borrowable::binary::LongLong> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Long>
    : public impl::FixedIntCodec<borrowable::binary::Long> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Int24>
    : public impl::FixedIntCodec<borrowable::binary::Int24> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Short>
    : public impl::FixedIntCodec<borrowable::binary::Short> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Tiny>
    : public impl::FixedIntCodec<borrowable::binary::Tiny> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Year>
    : public impl::FixedIntCodec<borrowable::binary::Year> {
 public:
  using FixedIntCodec::FixedIntCodec;
};

template <>
class Codec<borrowable::binary::Double>
    : public impl::FloatCodec<borrowable::binary::Double> {
 public:
  using FloatCodec::FloatCodec;
};

template <>
class Codec<borrowable::binary::Float>
    : public impl::FloatCodec<borrowable::binary::Float> {
 public:
  using FloatCodec::FloatCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::String<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::String<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::String<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::VarString<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::VarString<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::VarString<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Varchar<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::Varchar<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Varchar<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Json<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::Json<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Json<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Blob<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::Blob<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Blob<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::TinyBlob<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::TinyBlob<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::TinyBlob<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::MediumBlob<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::MediumBlob<Borrowed>> {
 public:
  using impl::StringCodec<
      Borrowed, borrowable::binary::MediumBlob<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::LongBlob<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::LongBlob<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::LongBlob<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Enum<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::Enum<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Enum<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Set<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::Set<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Set<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Decimal<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::Decimal<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Decimal<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::NewDecimal<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::NewDecimal<Borrowed>> {
 public:
  using impl::StringCodec<
      Borrowed, borrowable::binary::NewDecimal<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Bit<Borrowed>>
    : public impl::StringCodec<Borrowed, borrowable::binary::Bit<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Bit<Borrowed>>::StringCodec;
};

template <bool Borrowed>
class Codec<borrowable::binary::Geometry<Borrowed>>
    : public impl::StringCodec<Borrowed,
                               borrowable::binary::Geometry<Borrowed>> {
 public:
  using impl::StringCodec<Borrowed,
                          borrowable::binary::Geometry<Borrowed>>::StringCodec;
};

template <>
class Codec<borrowable::binary::Date>
    : public impl::DatetimeCodec<borrowable::binary::Date> {
 public:
  using DatetimeCodec::DatetimeCodec;
};

template <>
class Codec<borrowable::binary::DateTime>
    : public impl::DatetimeCodec<borrowable::binary::DateTime> {
 public:
  using DatetimeCodec::DatetimeCodec;
};

template <>
class Codec<borrowable::binary::Timestamp>
    : public impl::DatetimeCodec<borrowable::binary::Timestamp> {
 public:
  using DatetimeCodec::DatetimeCodec;
};

template <>
class Codec<borrowable::binary::Time>
    : public impl::TimeCodec<borrowable::binary::Time> {
 public:
  using TimeCodec::TimeCodec;
};

}  // namespace classic_protocol

#endif

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

#include "classic_query_param.h"

#include <charconv>  // from_chars
#include <iomanip>   // quoted
#include <sstream>

#include "mysqlrouter/classic_protocol_binary.h"
#include "mysqlrouter/classic_protocol_codec_binary.h"

template <class T>
static constexpr uint16_t binary_type() {
  return classic_protocol::Codec<T>::type();
}

namespace classic_protocol::borrowable::binary {
template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::String<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Json<Borrowed> &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Varchar<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::VarString<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Decimal<Borrowed> &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::NewDecimal<Borrowed> &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(
    std::ostream &os, const classic_protocol::borrowable::binary::Double &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Float &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Tiny &v) {
  os << static_cast<unsigned int>(v.value());
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Int24 &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Short &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Long &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(
    std::ostream &os, const classic_protocol::borrowable::binary::LongLong &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::TinyBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Blob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::MediumBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::LongBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::DatetimeBase &v) {
  std::ostringstream oss;

  oss << std::setfill('0')  //
      << std::setw(4) << v.year() << "-" << std::setw(2)
      << static_cast<unsigned int>(v.month()) << "-" << std::setw(2)
      << static_cast<unsigned int>(v.day());
  if (v.hour() || v.minute() || v.second() || v.microsecond()) {
    oss << " " << std::setw(2) << static_cast<unsigned int>(v.hour()) << ":"
        << std::setw(2) << static_cast<unsigned int>(v.minute()) << ":"
        << std::setw(2) << static_cast<unsigned int>(v.second());

    if (v.microsecond()) {
      oss << "." << std::setw(6) << v.microsecond();
    }
  }

  os << oss.str();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::binary::Time &v) {
  std::ostringstream oss;

  oss << std::setfill('0')  //
      << static_cast<unsigned int>(v.days()) << "d " << std::setw(2)
      << static_cast<unsigned int>(v.hour()) << ":" << std::setw(2)
      << static_cast<unsigned int>(v.minute()) << ":" << std::setw(2)
      << static_cast<unsigned int>(v.second());

  if (v.microsecond()) {
    oss << "." << std::setw(6) << v.microsecond();
  }

  os << oss.str();
  return os;
}
}  // namespace classic_protocol::borrowable::binary

stdx::expected<std::string, std::error_code> param_to_string(
    const classic_protocol::borrowed::message::client::Query::Param &param) {
  enum class BinaryType {
    Decimal = binary_type<classic_protocol::binary::Decimal>(),
    NewDecimal = binary_type<classic_protocol::binary::NewDecimal>(),
    Double = binary_type<classic_protocol::binary::Double>(),
    Float = binary_type<classic_protocol::binary::Float>(),
    LongLong = binary_type<classic_protocol::binary::LongLong>(),
    Long = binary_type<classic_protocol::binary::Long>(),
    Int24 = binary_type<classic_protocol::binary::Int24>(),
    Short = binary_type<classic_protocol::binary::Short>(),
    Tiny = binary_type<classic_protocol::binary::Tiny>(),
    String = binary_type<classic_protocol::binary::String>(),
    Varchar = binary_type<classic_protocol::binary::Varchar>(),
    VarString = binary_type<classic_protocol::binary::VarString>(),
    MediumBlob = binary_type<classic_protocol::binary::MediumBlob>(),
    TinyBlob = binary_type<classic_protocol::binary::TinyBlob>(),
    Blob = binary_type<classic_protocol::binary::Blob>(),
    LongBlob = binary_type<classic_protocol::binary::LongBlob>(),
    Json = binary_type<classic_protocol::binary::Json>(),
    Date = binary_type<classic_protocol::binary::Date>(),
    DateTime = binary_type<classic_protocol::binary::DateTime>(),
    Timestamp = binary_type<classic_protocol::binary::Timestamp>(),
    Time = binary_type<classic_protocol::binary::Time>(),
  };

  std::ostringstream oss;

  auto type = param.type_and_flags & 0xff;

  oss << "<" << type << "> ";

  switch (BinaryType{type}) {
    case BinaryType::Double: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Double>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Float: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Float>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Tiny: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Tiny>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Short: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Short>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Int24: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Int24>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Long: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Long>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::LongLong: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::LongLong>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::String: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::String>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::VarString: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::VarString>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Varchar: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Varchar>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Json: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Json>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::TinyBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::TinyBlob>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::MediumBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::MediumBlob>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Blob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Blob>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::LongBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::LongBlob>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Date: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Date>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::DateTime: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::DateTime>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Timestamp: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Timestamp>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Time: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Time>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Decimal: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Decimal>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::NewDecimal: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::NewDecimal>(
              net::buffer(*param.value), {});
      if (!decode_res) return stdx::unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
  }

  return oss.str();
}

stdx::expected<uint64_t, std::error_code> param_to_number(
    const classic_protocol::borrowed::message::client::Query::Param &param) {
  switch (param.type_and_flags & 0xff) {
    case binary_type<classic_protocol::binary::Blob>():
    case binary_type<classic_protocol::binary::TinyBlob>():
    case binary_type<classic_protocol::binary::MediumBlob>():
    case binary_type<classic_protocol::binary::LongBlob>():
    case binary_type<classic_protocol::binary::Varchar>():
    case binary_type<classic_protocol::binary::VarString>():
    case binary_type<classic_protocol::binary::String>(): {
      uint64_t val{};

      auto str = *param.value;

      auto conv_res = std::from_chars(str.data(), str.data() + str.size(), val);
      if (conv_res.ec == std::errc{}) return val;

      return stdx::unexpected(make_error_code(conv_res.ec));
    }
    case binary_type<classic_protocol::binary::Tiny>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Tiny>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Short>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Short>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Int24>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Int24>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Long>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Long>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::LongLong>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::LongLong>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::unexpected(decode_res.error());

      return decode_res->second.value();
    }
  }

  // all other types: fail.
  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<std::string, std::error_code> param_as_string(
    const classic_protocol::borrowed::message::client::Query::Param &param) {
  switch (param.type_and_flags & 0xff) {
    case binary_type<classic_protocol::binary::Blob>(): {
      auto dec_res = classic_protocol::decode<classic_protocol::binary::Blob>(
          net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::TinyBlob>(): {
      auto dec_res =
          classic_protocol::decode<classic_protocol::binary::TinyBlob>(
              net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::MediumBlob>(): {
      auto dec_res =
          classic_protocol::decode<classic_protocol::binary::MediumBlob>(
              net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::LongBlob>(): {
      auto dec_res =
          classic_protocol::decode<classic_protocol::binary::LongBlob>(
              net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::Varchar>(): {
      auto dec_res =
          classic_protocol::decode<classic_protocol::binary::Varchar>(
              net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::VarString>(): {
      auto dec_res =
          classic_protocol::decode<classic_protocol::binary::VarString>(
              net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
    case binary_type<classic_protocol::binary::String>(): {
      auto dec_res = classic_protocol::decode<classic_protocol::binary::String>(
          net::buffer(*param.value), {});
      if (!dec_res) return stdx::unexpected(dec_res.error());

      return dec_res->second.value();
    }
  }

  // all other types: fail.
  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

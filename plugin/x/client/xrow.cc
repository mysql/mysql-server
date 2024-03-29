/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/client/mysqlxclient/xrow.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace xcl {
namespace row_decoder {

namespace pb = google::protobuf;

namespace {

template <typename Hour_type>
void read_optional_time(pb::io::CodedInputStream *input_stream, Hour_type *hour,
                        uint8_t *minutes, uint8_t *seconds,
                        uint32_t *useconds) {
  pb::uint64 value;

  if (!input_stream->ReadVarint64(&value)) return;

  *hour = static_cast<Hour_type>(value);

  if (!input_stream->ReadVarint64(&value)) return;

  *minutes = static_cast<uint8_t>(value);

  if (!input_stream->ReadVarint64(&value)) return;

  *seconds = static_cast<uint8_t>(value);

  if (!input_stream->ReadVarint64(&value)) return;

  *useconds = static_cast<uint32_t>(value);
}

template <typename Numeric_type>
bool read_required_uint64(pb::io::CodedInputStream *input_buffer,
                          Numeric_type *out_result) {
  pb::uint64 value;

  if (!input_buffer->ReadVarint64(&value)) {
    return false;
  }

  if (out_result) *out_result = static_cast<Numeric_type>(value);
  return true;
}

}  // namespace

bool buffer_to_s64(const std::string &buffer, int64_t *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  pb::uint64 value;
  const bool was_succesfull = input_stream.ReadVarint64(&value);

  if (!was_succesfull) {
    return false;
  }

  if (out_result)
    *out_result = pb::internal::WireFormatLite::ZigZagDecode64(value);

  return true;
}

bool buffer_to_u64(const std::string &buffer, uint64_t *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  pb::uint64 value;

  const bool was_succesfull = input_stream.ReadVarint64(&value);

  if (!was_succesfull) {
    return false;
  }

  if (out_result) *out_result = value;

  return true;
}

bool buffer_to_string(const std::string &buffer, Row_str *out_result,
                      size_t *out_length) {
  if (buffer.empty()) return false;

  /*Last byte contains trailing '\0' that we want to skip here*/
  if (out_length) *out_length = buffer.length() - 1;
  if (out_result) *out_result = buffer.c_str();

  return true;
}

bool buffer_to_set(const std::string &buffer,
                   std::set<std::string> *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));
  if (out_result) out_result->clear();

  pb::uint64 len;
  std::string elem;
  bool has_next = true;
  bool empty = true;
  while (true) {
    has_next = input_stream.ReadVarint64(&len);
    if (has_next && len > 0) {
      bool ok = input_stream.ReadString(&elem, static_cast<int>(len));
      if (!ok) {
        if (empty && (0x01 == len)) {
          /*special case for empty set*/
          break;
        } else {
          if (out_result) out_result->clear();
          return false;
        }
      }
      if (out_result) out_result->insert(elem);
    } else {
      break;
    }
    empty = false;
  }

  return true;
}

bool buffer_to_string_set(const std::string &buffer, std::string *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));
  std::string result;

  pb::uint64 len;
  std::string elem;
  while (true) {
    const bool has_next = input_stream.ReadVarint64(&len);

    if (has_next && len > 0) {
      const bool ok = input_stream.ReadString(&elem, static_cast<int>(len));

      if (!ok) {
        if (result.empty() && (0x01 == len)) {
          /*special case for empty set*/
          break;
        } else {
          return false;
        }
      }
      if (!result.empty()) result.append(",");
      result.append(elem);
    } else {
      break;
    }
  }
  if (out_result) *out_result = std::move(result);

  return true;
}

bool buffer_to_float(const std::string &buffer, float *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  pb::uint32 value;
  const bool was_succesfull = input_stream.ReadLittleEndian32(&value);

  if (!was_succesfull) {
    return false;
  }
  if (out_result)
    *out_result = pb::internal::WireFormatLite::DecodeFloat(value);

  return true;
}

bool buffer_to_double(const std::string &buffer, double *out_result) {
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  pb::uint64 value;
  const bool was_successful = input_stream.ReadLittleEndian64(&value);

  if (!was_successful) {
    return false;
  }
  if (out_result)
    *out_result = pb::internal::WireFormatLite::DecodeDouble(value);

  return true;
}

bool buffer_to_datetime(const std::string &buffer, DateTime *out_result,
                        const bool has_time) {
  uint16_t year;
  uint8_t month, day;
  uint8_t hour = 0, minutes = 0, seconds = 0;
  uint32_t useconds = 0;

  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  if (!read_required_uint64(&input_stream, &year)) return false;

  if (!read_required_uint64(&input_stream, &month)) return false;

  if (!read_required_uint64(&input_stream, &day)) return false;

  if (out_result) {
    if (has_time) {
      read_optional_time(&input_stream, &hour, &minutes, &seconds, &useconds);
      *out_result =
          DateTime(year, month, day, hour, minutes, seconds, useconds);
    } else {
      *out_result = DateTime(year, month, day);
    }
    return true;
  }

  return false;
}

bool buffer_to_time(const std::string &buffer, Time *out_result) {
  uint32_t hour = 0;
  uint8_t minutes = 0;
  uint8_t seconds = 0;
  uint32_t useconds = 0;

  pb::uint8 sign;
  pb::io::CodedInputStream input_stream(
      reinterpret_cast<const pb::uint8 *>(&buffer[0]),
      static_cast<int>(buffer.length()));

  if (!input_stream.ReadRaw(&sign, 1)) return false;

  read_optional_time(&input_stream, &hour, &minutes, &seconds, &useconds);

  if (out_result)
    *out_result = Time((sign != 0x00), hour, minutes, seconds, useconds);

  return true;
}

bool buffer_to_decimal(const std::string &buffer, Decimal *out_result) {
  if (out_result) *out_result = Decimal::from_bytes(buffer);

  return true;
}

}  // namespace row_decoder
}  // namespace xcl

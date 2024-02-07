/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_BASE_H_
#define PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_BASE_H_

#include <google/protobuf/wire_format_lite.h>
#include <cassert>
#include <cstdint>

namespace protocol {

namespace primitives {

namespace base {

template <uint64_t length, uint64_t value>
struct Varint_length_value {
  static void encode(uint8_t *&) {
    static_assert(0 == length, "Length must be grater then zero.");
    static_assert(length,
                  "You have specified unsupported version of length, currently "
                  "supported 1-10.");
  }
};

template <uint64_t value>
struct Varint_length_value<1, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<2, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<3, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<4, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<5, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<6, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 28) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<7, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 28) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 35) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 42) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<8, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 28) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 35) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 42) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 49) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<9, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 28) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 35) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 42) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 49) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 56) & 0x7F);
  }
};

template <uint64_t value>
struct Varint_length_value<10, value> {
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>((value & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 7) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 14) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 21) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 28) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 35) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 42) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 49) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>(((value >> 56) & 0x7F) | 0x80);
    *(out++) = static_cast<uint8_t>((value >> 63) & 0x7F);
  }
};

template <uint64_t length>
struct Varint_length {
  static void encode(uint8_t *&, const uint64_t) {
    static_assert(0 == length, "Length must be grater then zero.");
    static_assert(length,
                  "You have specified unsupported version of length, currently "
                  "supported 1-10.");
  }
};

template <>
struct Varint_length<1> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F);
  }
};

template <>
struct Varint_length<2> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F);
  }
};

template <>
struct Varint_length<3> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F);
  }
};

template <>
struct Varint_length<4> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F);
  }
};

template <>
struct Varint_length<5> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F);
  }
};

template <>
struct Varint_length<6> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F);
  }
};

template <>
struct Varint_length<7> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 42) & 0x7F);
  }
};

template <>
struct Varint_length<8> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 42) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 49) & 0x7F);
  }
};

template <>
struct Varint_length<9> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 42) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 49) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 56) & 0x7F);
  }
};

template <>
struct Varint_length<10> {
  static void encode(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 7) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 14) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 21) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 28) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 35) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 42) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 49) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 56) & 0x7F) | 0x80;
    *(out++) = static_cast<uint8_t>((value >> 63) & 0x7F);
  }
};

struct Varint {
  template <typename Value_type>
  static void encode(uint8_t *&out, Value_type value) {  // NOLINT
    while (value > 0x7F) {
      *(out++) = static_cast<uint8_t>(value & 0x7F) | 0x80;
      value >>= 7;
    }

    *(out++) = static_cast<uint8_t>(value & 0x7F);
  }
};

struct Fixint {};

template <uint32_t length>
struct Fixint_length {};

template <>
struct Fixint_length<1> {
  template <uint8_t value>
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = value;
  }

  static void encode_value(uint8_t *&out, const uint8_t value) {  // NOLINT
    *(out++) = value;
  }
};

template <>
struct Fixint_length<4> {
  template <uint32_t value>
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value);
    *(out++) = static_cast<uint8_t>(value >> 8);
    *(out++) = static_cast<uint8_t>(value >> 16);
    *(out++) = static_cast<uint8_t>(value >> 24);
  }

  static void encode_value(uint8_t *&out, const uint32_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value);
    *(out++) = static_cast<uint8_t>(value >> 8);
    *(out++) = static_cast<uint8_t>(value >> 16);
    *(out++) = static_cast<uint8_t>(value >> 24);
  }
};

template <>
struct Fixint_length<8> {
  template <uint64_t value>
  static void encode(uint8_t *&out) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value);
    *(out++) = static_cast<uint8_t>(value >> 8);
    *(out++) = static_cast<uint8_t>(value >> 16);
    *(out++) = static_cast<uint8_t>(value >> 24);
    *(out++) = static_cast<uint8_t>(value >> 32);
    *(out++) = static_cast<uint8_t>(value >> 40);
    *(out++) = static_cast<uint8_t>(value >> 48);
    *(out++) = static_cast<uint8_t>(value >> 56);
  }

  static void encode_value(uint8_t *&out, const uint64_t value) {  // NOLINT
    *(out++) = static_cast<uint8_t>(value);
    *(out++) = static_cast<uint8_t>(value >> 8);
    *(out++) = static_cast<uint8_t>(value >> 16);
    *(out++) = static_cast<uint8_t>(value >> 24);
    *(out++) = static_cast<uint8_t>(value >> 32);
    *(out++) = static_cast<uint8_t>(value >> 40);
    *(out++) = static_cast<uint8_t>(value >> 48);
    *(out++) = static_cast<uint8_t>(value >> 56);
  }
};

class Helper {
 public:
  using WireType = google::protobuf::internal::WireFormatLite::WireType;

  static constexpr uint32_t encode_field_tag(const uint32_t field_no,
                                             const WireType wt) {
    return static_cast<uint32_t>(
        field_no << ::google::protobuf::internal::WireFormatLite::kTagTypeBits |
        wt);
  }

  static constexpr uint64_t encode_zigzag(const int64_t value) {
    return static_cast<uint64>(value >> 63) ^ (static_cast<uint64>(value) << 1);
  }

  static constexpr uint32_t encode_zigzag(const int32_t value) {
    return (value >> 31) ^ (value << 1);
  }

  constexpr static int get_varint_length(const uint64_t value, uint64_t shift,
                                         uint8_t level) {
    return (value < shift) ? level
                           : get_varint_length(value, shift << 7, level + 1);
  }

  static int get_varint_length(const uint64_t value) {
    if (value < 0x0000000000000080) return 1;
    if (value < 0x0000000000004000) return 2;
    if (value < 0x0000000000200000) return 3;
    if (value < 0x0000000010000000) return 4;
    if (value < 0x0000000800000000) return 5;
    if (value < 0x0000040000000000) return 6;
    if (value < 0x0002000000000000) return 7;
    if (value < 0x0100000000000000) return 8;
    if (value < 0x8000000000000000) return 9;

    return 10;
  }
};

}  // namespace base

}  // namespace primitives

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_BASE_H_

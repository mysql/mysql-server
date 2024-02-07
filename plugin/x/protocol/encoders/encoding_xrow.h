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

#ifndef PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XROW_H_
#define PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XROW_H_

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include <google/protobuf/wire_format_lite.h>
MY_COMPILER_DIAGNOSTIC_POP()
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "decimal.h"
#include "mysql_time.h"

#include "plugin/x/client/mysqlxclient/xdecimal.h"
#include "plugin/x/protocol/encoders/encoding_xmessages.h"

namespace protocol {

template <typename Encoder_type>
class XRow_encoder_base {
 private:
  using Position = typename Encoder_type::Position;
  Encoder_type *m_encoder = nullptr;
  Position m_row_begin;
  uint32_t m_fields;

 public:
  explicit XRow_encoder_base(Encoder_type *encoder) : m_encoder(encoder) {}

  uint32_t get_num_fields() const { return m_fields; }

  void begin_row() {
    m_encoder->template begin_xmessage<tags::Row::server_id, 100>(&m_row_begin);
    m_fields = 0;
  }

  void end_row() { m_encoder->end_xmessage(m_row_begin); }

  void abort_row() { m_encoder->abort_xmessage(m_row_begin); }

  void field_null() {
    ++m_fields;
    m_encoder->template ensure_buffer_size<20>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->template encode_const_var_uint<0>();
  }

  void field_signed_longlong(const longlong value) {
    ++m_fields;
    m_encoder->template ensure_buffer_size<30>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();
    m_encoder->encode_var_sint64(static_cast<int64_t>(value));
    m_encoder->end_delimited_field(field_begin);
  }

  void field_unsigned_longlong(const ulonglong value) {
    ++m_fields;
    m_encoder->template ensure_buffer_size<30>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();
    m_encoder->encode_var_uint64(static_cast<uint64_t>(value));
    m_encoder->end_delimited_field(field_begin);
  }

  void field_bit(const char *const value, size_t length) {
    assert(length <= 8);
    ++m_fields;

    uint64_t binary_value = 0;
    for (size_t i = 0; i < length; ++i) {
      binary_value +=
          ((static_cast<uint64_t>(value[i]) & 0xff) << ((length - i - 1) * 8));
    }

    m_encoder->template ensure_buffer_size<30>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();
    m_encoder->encode_var_uint64(binary_value);
    m_encoder->end_delimited_field(field_begin);
  }

  void field_set(const char *const value, size_t length) {
    ++m_fields;
    // special case: empty SET
    if (0 == length) {
      // write length=0x01 to the buffer and we're done here
      m_encoder->template ensure_buffer_size<30>();
      m_encoder->template encode_field_delimited_header<tags::Row::field>();
      m_encoder->template encode_const_var_uint<1>();
      m_encoder->template encode_const_var_uint<1>();
      return;
    }

    std::vector<std::string> set_vals;
    const char *comma, *p_value = value;
    unsigned int elem_len;
    do {
      comma = std::strchr(p_value, ',');
      if (comma != nullptr) {
        elem_len = static_cast<unsigned int>(comma - p_value);
        set_vals.push_back(std::string(p_value, elem_len));
        p_value = comma + 1;
      }
    } while (comma != nullptr);

    // still sth left to store
    if (static_cast<size_t>(p_value - value) < length) {
      elem_len = static_cast<unsigned int>(length - (p_value - value));
      set_vals.push_back(std::string(p_value, elem_len));
    }

    m_encoder->template ensure_buffer_size<20>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field, 3>();
    for (size_t i = 0; i < set_vals.size(); ++i) {
      m_encoder->template ensure_buffer_size<10>();
      m_encoder->encode_var_uint64(set_vals[i].length());
      m_encoder->encode_raw(
          reinterpret_cast<const uint8_t *>(set_vals[i].c_str()),
          set_vals[i].length());
    }
    m_encoder->end_delimited_field(field_begin);
  }

  void field_string(const char *value, const size_t length) {
    ++m_fields;
    m_encoder->template ensure_buffer_size<30>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->encode_var_uint32(static_cast<uint32_t>(length + 1));
    m_encoder->encode_raw(reinterpret_cast<const uint8_t *>(value), length);
    m_encoder->encode_raw(reinterpret_cast<const uint8_t *>("\0"), 1);
  }

  void field_datetime(const MYSQL_TIME *value) {
    assert(value->year < 10000);
    assert(value->month < 13);
    assert(value->day < 32);
    assert(value->hour < 24);
    assert(value->minute < 60);
    assert(value->second < 60);
    assert(value->second_part < 1000000);
    ++m_fields;
    m_encoder->template ensure_buffer_size<32>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();

    m_encoder->encode_fixedvar16_uint32(value->year);
    m_encoder->encode_fixedvar8_uint8(value->month);
    m_encoder->encode_fixedvar8_uint8(value->day);

    if (value->hour || value->minute || value->second || value->second_part) {
      m_encoder->encode_fixedvar8_uint8(value->hour);
      if (value->minute || value->second || value->second_part) {
        m_encoder->encode_fixedvar8_uint8(value->minute);
        if (value->second || value->second_part) {
          m_encoder->encode_fixedvar8_uint8(value->second);
          if (value->second_part) {
            m_encoder->encode_var_uint32(value->second_part);
          }
        }
      }
    }
    m_encoder->end_delimited_field(field_begin);
  }

  void field_time(const MYSQL_TIME *value) {
    assert(value->minute < 60);
    assert(value->second < 60);
    assert(value->second_part < 1000000);
    ++m_fields;
    m_encoder->template ensure_buffer_size<47>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();

    if (value->neg)
      m_encoder->template encode_const_var_uint<1>();
    else
      m_encoder->template encode_const_var_uint<0>();

    if (value->hour || value->minute || value->second || value->second_part) {
      m_encoder->encode_var_uint64(value->hour);
      if (value->minute || value->second || value->second_part) {
        m_encoder->encode_fixedvar8_uint8(value->minute);
        if (value->second || value->second_part) {
          m_encoder->encode_fixedvar8_uint8(value->second);
          if (value->second_part) {
            m_encoder->encode_var_uint32(value->second_part);
          }
        }
      }
    }
    m_encoder->end_delimited_field(field_begin);
  }

  void field_date(const MYSQL_TIME *value) {
    assert(value->year < 10000);
    assert(value->month < 13);
    assert(value->day < 32);
    ++m_fields;
    m_encoder->template ensure_buffer_size<27>();
    auto field_begin =
        m_encoder->template begin_delimited_field<tags::Row::field>();

    m_encoder->encode_var_uint32(value->year);
    m_encoder->encode_fixedvar8_uint8(value->month);
    m_encoder->encode_fixedvar8_uint8(value->day);
    m_encoder->end_delimited_field(field_begin);
  }

  void field_float(const float value) {
    ++m_fields;
    m_encoder->template ensure_buffer_size<24>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->template encode_const_var_uint<4>();  // Field size
    m_encoder->encode_fixed_uint32(
        google::protobuf::internal::WireFormatLite::EncodeFloat(value));
  }

  void field_double(const double value) {
    ++m_fields;
    m_encoder->template ensure_buffer_size<28>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->template encode_const_var_uint<8>();  // Field size
    m_encoder->encode_fixed_uint64(
        google::protobuf::internal::WireFormatLite::EncodeDouble(value));
  }

  void field_decimal(const char *value, const size_t length) {
    ++m_fields;
    std::string dec_str(value, length);
    xcl::Decimal dec(dec_str);
    std::string dec_bytes = dec.to_bytes();

    m_encoder->template ensure_buffer_size<30>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->encode_var_uint32(static_cast<uint32_t>(dec_bytes.length()));
    m_encoder->encode_raw(reinterpret_cast<const uint8_t *>(dec_bytes.c_str()),
                          (static_cast<uint32_t>(dec_bytes.length())));
  }

  void field_decimal(const decimal_t *value) {
    ++m_fields;
    std::string str_buf;
    int str_len = 200;
    str_buf.resize(str_len);
    decimal2string(value, &(str_buf)[0], &str_len);
    str_buf.resize(str_len);

    xcl::Decimal dec(str_buf);
    std::string dec_bytes = dec.to_bytes();

    m_encoder->template ensure_buffer_size<30>();
    m_encoder->template encode_field_delimited_header<tags::Row::field>();
    m_encoder->encode_var_uint32(static_cast<uint32_t>(dec_bytes.length()));
    m_encoder->encode_raw(reinterpret_cast<const uint8_t *>(dec_bytes.c_str()),
                          (static_cast<uint32_t>(dec_bytes.length())));
  }
};

class XRow_encoder : public XRow_encoder_base<XProtocol_encoder> {
 public:
  using Base = XRow_encoder_base<XProtocol_encoder>;
  using Base::Base;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XROW_H_

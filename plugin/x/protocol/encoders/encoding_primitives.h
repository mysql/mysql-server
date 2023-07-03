/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_H_
#define PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_H_

#include <assert.h>
#include <algorithm>
#include <cassert>
#include <cstdint>

#include "plugin/x/protocol/encoders/encoding_buffer.h"
#include "plugin/x/protocol/encoders/encoding_primitives_base.h"

namespace protocol {

class Primitives_encoder {
 private:
  using Helper = primitives::base::Helper;
  template <uint64_t length, uint64_t value>
  using Varint_length_value =
      primitives::base::Varint_length_value<length, value>;

 public:
  explicit Primitives_encoder(Encoding_buffer *buffer) {
    m_buffer = buffer;
    m_page = m_buffer->m_current;
  }

  template <uint64_t value>
  void encode_const_var_uint() {
    using Varint_length_value_length =
        Varint_length_value<Helper::get_varint_length(value, 0x80, 1), value>;
    Varint_length_value_length::encode(m_page->m_current_data);
  }

  template <uint64_t value>
  void encode_const_var_enum() {
    using Varint_length_value_length =
        Varint_length_value<Helper::get_varint_length(value, 0x80, 1), value>;
    Varint_length_value_length::encode(m_page->m_current_data);
  }

  void encode_fixedvar32_uint32(const uint32_t value) {
    assert(value < (1 << (4 * 7)));

    using Varint_length_value_length = primitives::base::Varint_length<4>;
    Varint_length_value_length::encode(m_page->m_current_data, value);
  }

  void encode_fixedvar16_uint32(const uint32_t value) {
    assert(value < (1 << (2 * 7)));

    using Varint_length_value_length = primitives::base::Varint_length<2>;
    Varint_length_value_length::encode(m_page->m_current_data, value);
  }

  void encode_fixedvar8_uint8(const uint8_t value) {
    assert(value < (1 << (7)));

    using Varint_length_value_length = primitives::base::Varint_length<1>;
    Varint_length_value_length::encode(m_page->m_current_data, value);
  }

  template <int64_t value>
  void encode_const_var_sint() {
    encode_const_var_uint<Helper::encode_zigzag(value)>();
  }

  uint8_t *reserve(const uint32_t size) {
    assert(m_page->is_at_least(size));

    auto result = m_page->m_current_data;
    m_page->m_current_data += size;

    return result;
  }

  void encode_fixed_uint8(const uint8_t value) {
    primitives::base::Fixint_length<1>::encode_value(m_page->m_current_data,
                                                     value);
  }

  void encode_fixed_uint32(const uint32_t value) {
    primitives::base::Fixint_length<4>::encode_value(m_page->m_current_data,
                                                     value);
  }

  void encode_fixed_uint64(const uint64_t value) {
    primitives::base::Fixint_length<8>::encode_value(m_page->m_current_data,
                                                     value);
  }

  void encode_fixed_sint32(const int32_t value);
  void encode_fixed_sint64(const int64_t value);

  void encode_var_uint32(const uint32_t value) {
    primitives::base::Varint::encode(m_page->m_current_data, value);
  }

  void encode_var_sint32(const int32_t value) {
    encode_var_uint32(Helper::encode_zigzag(value));
  }

  void encode_var_uint64(const uint64_t value) {
    primitives::base::Varint::encode(m_page->m_current_data, value);
  }

  void encode_var_sint64(const int64_t value) {
    encode_var_uint64(Helper::encode_zigzag(value));
  }

  void encode_raw_no_boundry_check(const uint8_t *source,
                                   const uint32_t source_size) {
    memcpy(m_page->m_current_data, source, source_size);
    m_page->m_current_data += source_size;
  }

  void encode_raw(const uint8_t *source, uint32_t source_size) {
    while (source_size) {
      if (m_page->is_full()) m_page = m_buffer->get_next_page();

      const auto to_copy = std::min(source_size, m_page->get_free_bytes());
      encode_raw_no_boundry_check(source, to_copy);

      source += to_copy;
      source_size -= to_copy;
    }
  }

  void buffer_set(Encoding_buffer *buffer) {
    m_buffer = buffer;
    m_page = m_buffer->m_current;
  }

  void buffer_reset() {
    m_buffer->remove_page_list(m_buffer->m_front->m_next_page);
    m_buffer->m_front->reset();
    m_page = m_buffer->m_current = m_buffer->m_front;
  }

  Encoding_buffer *m_buffer;
  Page *m_page;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_PRIMITIVES_H_

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

#ifndef PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XPROTOCOL_H_
#define PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XPROTOCOL_H_

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include <google/protobuf/wire_format_lite.h>
MY_COMPILER_DIAGNOSTIC_POP()
#include <cassert>
#include <cstdint>
#include <string>

#include "my_dbug.h"

#include "plugin/x/protocol/encoders/encoding_protobuf.h"

namespace protocol {

enum class Compression_type { k_single, k_multiple, k_group };

class Compression_buffer_interface {
 public:
  virtual ~Compression_buffer_interface() = default;

  virtual void reset_counters() = 0;
  virtual bool process(Encoding_buffer *output_buffer,
                       const Encoding_buffer *input_buffer) = 0;

  virtual void get_processed_data(uint32_t *out_uncompressed,
                                  uint32_t *out_compressed) = 0;
};

/**
  This class is wraps protobuf payload with X Protocol header

  This class generates X Protocol headers for protobuf messages
  and for compressed messages.
  Additionally it supplies sub-field protobuf functionality,
  because similar mechanism was used for generation for protobuf
  fields and X headers.
*/
class XProtocol_encoder : public Protobuf_encoder {
 private:
  constexpr static uint32_t k_xmessage_header_length = 5;

  enum class Header_configuration { k_full, k_size_only, k_none };

  Header_configuration m_header_configuration = Header_configuration::k_full;
  uint32_t m_header_size = header_size(m_header_configuration);

  void set_header_config(const Header_configuration config) {
    m_header_configuration = config;
    m_header_size = header_size(m_header_configuration);
  }

  static uint32_t header_size(const Header_configuration config) {
    switch (config) {
      case Header_configuration::k_full:
        return 5;
      case Header_configuration::k_none:
        return 0;
      case Header_configuration::k_size_only:
        return 4;
      default:
        assert(false && "Not allowed value");
        return 0;
    }
  }

 public:
  explicit XProtocol_encoder(Encoding_buffer *buffer)
      : Protobuf_encoder(buffer) {
    ensure_buffer_size<1>();
  }

  struct Position {
    Page *m_page;
    uint8_t *m_position;

    uint8_t *get_position() const { return m_position; }

    uint32_t bytes_until_page(Page *current_page) const {
      uint32_t size = m_page->m_current_data - m_position;

      if (current_page == m_page) {
        return size;
      }

      Page *i = m_page->m_next_page;
      for (;;) {
        assert(nullptr != i);
        size += i->get_used_bytes();

        if (i == current_page) {
          assert(nullptr == i->m_next_page);
          break;
        }

        i = i->m_next_page;
      }

      return size;
    }
  };

  template <uint32_t delimiter_length>
  struct Field_delimiter : Position {};

  struct Compression_position : Position {
    Encoding_buffer *m_compressed_buffer;
    Compression_type m_compression_type;
    Delayed_fixed_varuint32 m_uncompressed_size;
    Field_delimiter<5> m_payload;
    uint8_t m_msg_id;
  };

  template <uint32_t id>
  void empty_xmessage() {
    ensure_buffer_size<k_xmessage_header_length>();

    if (Header_configuration::k_full == m_header_configuration) {
      DBUG_LOG("debug", "empty_msg_full_header");
      primitives::base::Fixint_length<4>::encode<1>(m_page->m_current_data);
      primitives::base::Fixint_length<1>::encode<id>(m_page->m_current_data);
    } else if (Header_configuration::k_size_only == m_header_configuration) {
      DBUG_LOG("debug", "empty_msg_size_only");
      primitives::base::Fixint_length<4>::encode<0>(m_page->m_current_data);
    }
  }

  Compression_position begin_compression(const uint8_t msg_id,
                                         const Compression_type type,
                                         Encoding_buffer *to_compress) {
    Compression_position result;

    result.m_msg_id = msg_id;
    begin_xmessage<tags::Compression::server_id, 100>(&result);

    switch (type) {
      case Compression_type::k_single:
      case Compression_type::k_multiple:
        set_header_config(Header_configuration::k_full);
        encode_field_var_uint32<tags::Compression::server_messages>(msg_id);
        break;
      case Compression_type::k_group:
        set_header_config(Header_configuration::k_full);
        break;
    }

    result.m_uncompressed_size =
        encode_field_fixed_uint32<tags::Compression::uncompressed_size>();
    begin_delimited_field<tags::Compression::payload>(&result.m_payload);

    assert(to_compress->m_current == to_compress->m_front);
    assert(to_compress->m_current->m_begin_data ==
           to_compress->m_current->m_current_data);
    result.m_compressed_buffer = m_buffer;
    result.m_compression_type = type;
    // Reset buffer, and initialize the 'handy' data hold inside
    // 'Encoder_primitives'
    buffer_set(to_compress);

    return result;
  }

  bool end_compression(const Compression_position &position,
                       Compression_buffer_interface *compress) {
    Position before_compression{m_buffer->m_front,
                                m_buffer->m_front->m_begin_data};
    const auto before_compression_size =
        before_compression.bytes_until_page(m_page);

    position.m_uncompressed_size.encode(before_compression_size);

    if (!compress->process(position.m_compressed_buffer, m_buffer))
      return false;

    auto ptr = position.m_position;
    const auto message_size =
        position.bytes_until_page(position.m_compressed_buffer->m_current);

    // Lets discard data inside new/compression buffer
    // in case when 'compress' call didn't do that.
    m_buffer->reset();

    // and now we restore original buffer
    buffer_set(position.m_compressed_buffer);
    end_delimited_field(position.m_payload);
    primitives::base::Fixint_length<4>::encode_value(ptr, message_size - 4);

    set_header_config(Header_configuration::k_full);

    return true;
  }

  template <uint32_t id, uint32_t needed_buffer_size>
  Position begin_xmessage() {
    Position result;

    begin_xmessage<id, needed_buffer_size>(&result);

    return result;
  }

  template <uint32_t needed_buffer_size>
  Position begin_xmessage(const uint32_t id) {
    Position result;

    ensure_buffer_size<needed_buffer_size + k_xmessage_header_length>();

    auto xmsg_start = m_page->m_current_data;
    if (Header_configuration::k_full == m_header_configuration) {
      auto xmsg_type = xmsg_start + 4;
      primitives::base::Fixint_length<1>::encode_value(xmsg_type, id);
    }
    result.m_page = m_page;
    result.m_position = xmsg_start;

    m_page->m_current_data += m_header_size;

    return result;
  }

  template <uint32_t id, uint32_t needed_buffer_size>
  void begin_xmessage(Position *position) {
    ensure_buffer_size<needed_buffer_size + k_xmessage_header_length>();

    auto xmsg_start = m_page->m_current_data;
    if (Header_configuration::k_full == m_header_configuration) {
      auto xmsg_type = xmsg_start + 4;
      primitives::base::Fixint_length<1>::encode<id>(xmsg_type);
    }
    position->m_page = m_page;
    position->m_position = xmsg_start;

    m_page->m_current_data += m_header_size;
  }

  void end_xmessage(const Position &position) {
    auto ptr = position.get_position();

    if (Header_configuration::k_none != m_header_configuration) {
      primitives::base::Fixint_length<4>::encode_value(
          ptr, position.bytes_until_page(m_page) - 4);
    }
  }

  void abort_xmessage(const Position &position) {
    auto page = position.m_page->m_next_page;

    m_buffer->remove_page_list(page);

    m_page = position.m_page;
    m_page->m_current_data = position.m_position;
  }

  void abort_compression(const Compression_position &position) {
    // Lets discard data inside new/compression buffer
    // in case when 'compress' call didn't do that.
    m_buffer->reset();

    // and now we restore original buffer
    buffer_set(position.m_compressed_buffer);

    set_header_config(Header_configuration::k_full);

    abort_xmessage(position);
  }

  template <uint32_t id, uint32_t delimiter_length = 1>
  Field_delimiter<delimiter_length> begin_delimited_field() {
    Field_delimiter<delimiter_length> result;

    begin_delimited_field<id>(&result);

    return result;
  }

  template <uint32_t id, uint32_t delimiter_length = 1>
  void begin_delimited_field(Field_delimiter<delimiter_length> *position) {
    encode_field_delimited_header<id>();
    position->m_position = m_page->m_current_data;
    position->m_page = m_page;

    m_page->m_current_data += delimiter_length;
  }

  template <uint32_t delimiter_length>
  void end_delimited_field(const Field_delimiter<delimiter_length> &position) {
    auto ptr = position.get_position();
    primitives::base::Varint_length<delimiter_length>::encode(
        ptr, position.bytes_until_page(m_page) - delimiter_length);
  }
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_ENCODERS_ENCODING_XPROTOCOL_H_

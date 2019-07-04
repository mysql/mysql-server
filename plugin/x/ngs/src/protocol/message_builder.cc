/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/protocol/message_builder.h"

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/protocol/page_output_stream.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

using google::protobuf::io::ZeroCopyOutputStream;

const int k_header_size = 5;

Message_builder::Message_builder(const bool memory_managed)
    : m_memory_managed(memory_managed) {
  if (m_memory_managed) m_out_stream = Stream_allocator().allocate(1);
}

Message_builder::~Message_builder() {
  if (m_memory_managed) Stream_allocator().deallocate(m_out_stream, 1);
}

void Message_builder::skip_field() { ++m_field_number; }

void Message_builder::encode_uint32(const uint32 value, const bool write) {
  ++m_field_number;

  if (write) {
    google::protobuf::internal::WireFormatLite::WriteTag(
        m_field_number,
        google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT,
        m_out_stream);
    m_out_stream->WriteVarint32(value);
  }
}

void Message_builder::encode_uint64(const uint64 value, const bool write) {
  ++m_field_number;

  if (write) {
    google::protobuf::internal::WireFormatLite::WriteTag(
        m_field_number,
        google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT,
        m_out_stream);
    m_out_stream->WriteVarint64(value);
  }
}

void Message_builder::encode_int32(const int32 value, const bool write) {
  ++m_field_number;
  if (write) {
    google::protobuf::internal::WireFormatLite::WriteTag(
        m_field_number,
        google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT,
        m_out_stream);
    m_out_stream->WriteVarint32SignExtended(value);
  }
}

void Message_builder::encode_string(const char *value, const size_t len,
                                    const bool write) {
  ++m_field_number;

  if (write) {
    google::protobuf::internal::WireFormatLite::WriteTag(
        m_field_number,
        google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
        m_out_stream);
    m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(len));
    m_out_stream->WriteRaw(value, static_cast<int>(len));
  }
}

void Message_builder::encode_string(const char *value, const bool write) {
  encode_string(value, write ? strlen(value) : 0, write);
}

void Message_builder::construct_stream() {
  construct_stream(m_out_page_stream);
}

void Message_builder::construct_stream(ZeroCopyOutputStream *zero_stream) {
  DBUG_ASSERT(m_memory_managed);
  if (m_valid_out_stream) reset_stream();

  Stream_allocator().construct(m_out_stream, zero_stream);
  m_valid_out_stream = true;
}

void Message_builder::reset_stream() {
  DBUG_ASSERT(m_memory_managed);
  DBUG_ASSERT(m_valid_out_stream);
  Stream_allocator().destroy(m_out_stream);
  m_valid_out_stream = false;
}

void Message_builder::start_message(Page_output_stream *out_buffer,
                                    const uint8 type) {
  m_field_number = 0;

  m_out_page_stream = out_buffer;
  m_out_page_stream->backup_current_position();
  m_header_addr =
      static_cast<uint8 *>(m_out_page_stream->reserve_space(k_header_size));

  if (nullptr == m_header_addr) return;

  m_header_addr[4] = type;
  m_start_from = static_cast<uint32>(m_out_page_stream->ByteCount());

  construct_stream();
}

void Message_builder::end_message() {
  // here we already know the buffer size, so write it at the beginning of the
  // buffer the order is important here as the stream's destructor calls
  // buffer's BackUp() validating ByteCount
  reset_stream();

  uint32 msg_size =
      static_cast<uint32>(m_out_page_stream->ByteCount()) - m_start_from + 1;
  CodedOutputStream::WriteLittleEndian32ToArray(
      msg_size, static_cast<google::protobuf::uint8 *>(m_header_addr));
}

uint8 *Message_builder::encode_empty_message(Page_output_stream *out_buffer,
                                             const uint8 type) {
  uint8 *dst_ptr =
      static_cast<uint8 *>(out_buffer->reserve_space(k_header_size));
  const uint32 MSG_SIZE = sizeof(uint8);

  if (nullptr == dst_ptr) return nullptr;

  CodedOutputStream::WriteLittleEndian32ToArray(MSG_SIZE, dst_ptr);
  dst_ptr += sizeof(uint32);
  *(dst_ptr) = type;

  return dst_ptr;
}

}  // namespace ngs

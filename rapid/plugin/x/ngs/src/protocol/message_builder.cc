/*
* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of the
* License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301  USA
*/

#include "ngs/protocol/message_builder.h"
#include "ngs/protocol/output_buffer.h"

#include "ngs_common/protocol_protobuf.h"

using namespace ngs;


Message_builder::Message_builder()
: m_out_buffer(NULL)
{}

Message_builder::~Message_builder()
{
}

void Message_builder::encode_uint32(uint32 value, bool write)
{
  ++m_field_number;
  if (write)
  {
    google::protobuf::internal::WireFormatLite::WriteTag(m_field_number, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT, m_out_stream.get());
    m_out_stream->WriteVarint32(value);
  }
}

void Message_builder::encode_uint64(uint64 value, bool write)
{
  ++m_field_number;
  if (write)
  {
    google::protobuf::internal::WireFormatLite::WriteTag(m_field_number, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT, m_out_stream.get());
    m_out_stream->WriteVarint64(value);
  }
}

void Message_builder::encode_int32(int32 value, bool write)
{
  ++m_field_number;
  if (write)
  {
    google::protobuf::internal::WireFormatLite::WriteTag(m_field_number, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT, m_out_stream.get());
    m_out_stream->WriteVarint32SignExtended(value);
  }
}

void Message_builder::encode_string(const char* value, size_t len, bool write)
{
  ++m_field_number;
  if (write)
  {
    google::protobuf::internal::WireFormatLite::WriteTag(m_field_number, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED, m_out_stream.get());
    m_out_stream->WriteVarint32(static_cast<google::protobuf::uint32>(len));
    m_out_stream->WriteRaw(value, static_cast<int>(len));
  }
}

void Message_builder::start_message(Output_buffer* out_buffer, uint8 type)
{
  m_field_number = 0;

  m_out_buffer = out_buffer;
  m_out_buffer->save_state();
  m_out_buffer->reserve(5);
  m_start_from = static_cast<uint32>(m_out_buffer->ByteCount());
  m_out_stream.reset(ngs::allocate_object<CodedOutputStream>(m_out_buffer));

  // at this point we don't know the size but we need to reserve the space for it
  // it is possible that the size which is stored on 4-bytes will be split into 2 pages
  // in that case we need to keep 2 addresses to be able to write the size when it is known
  m_out_stream->GetDirectBufferPointer(reinterpret_cast<void**>(&m_size_addr1), &m_size_addr1_size);
  DBUG_ASSERT(m_size_addr1_size >= 1);
  if (static_cast<size_t>(m_size_addr1_size) < sizeof(google::protobuf::uint32))
  {
    int bytes_left = sizeof(google::protobuf::uint32) - m_size_addr1_size;
    int size_addr2_size;
    m_out_stream->Skip(m_size_addr1_size);
    m_out_stream->GetDirectBufferPointer(reinterpret_cast<void**>(&m_size_addr2), &size_addr2_size);
    DBUG_ASSERT(size_addr2_size > bytes_left);
    m_out_stream->Skip(bytes_left);
  }
  else
  {
    m_size_addr1_size = sizeof(google::protobuf::uint32);
    m_out_stream->Skip(m_size_addr1_size);
  }

  // write message type
  m_out_stream->WriteRaw(&type, 1);
}

void Message_builder::end_message()
{
  // here we already know the buffer size, so write it at the beginning of the buffer
  // the order is important here as the stream's destructor calls buffer's BackUp() validating ByteCount
  m_out_stream.reset();

  uint32 msg_size = static_cast<uint32>(m_out_buffer->ByteCount()) - m_start_from - sizeof(google::protobuf::uint32);
  if (static_cast<size_t>(m_size_addr1_size) >= sizeof(google::protobuf::uint32))
  {
    // easy case, whole size written into continous memory
    CodedOutputStream::WriteLittleEndian32ToArray(msg_size, static_cast<google::protobuf::uint8*>(m_size_addr1));
  }
  else
  {
    // message size is split into 2 pages
    google::protobuf::uint8 source[4];
    memcpy(source, &msg_size, sizeof(msg_size));
#ifdef WORDS_BIGENDIAN
    std::swap(source[0], source[3]);
    std::swap(source[1], source[2]);
#endif
    google::protobuf::uint8* target[4];
    target[0] = m_size_addr1;
    target[1] = (m_size_addr1_size > 1) ? (m_size_addr1 + 1) : (m_size_addr2 + 1 - m_size_addr1_size);
    target[2] = (m_size_addr1_size > 2) ? (m_size_addr1 + 2) : (m_size_addr2 + 2 - m_size_addr1_size);
    target[3] = m_size_addr2 + 3 - m_size_addr1_size;

    for (size_t i = 0; i < 4; ++i)
      *target[i] = source[i];
  }
}

void Message_builder::encode_empty_message(Output_buffer* out_buffer, uint8 type)
{
  const uint32 MSG_SIZE = sizeof(uint8);
  out_buffer->add_int32(MSG_SIZE);
  out_buffer->add_int8(type);
}

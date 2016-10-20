/*
* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_MESSAGE_BUILDER_H_
#define _NGS_MESSAGE_BUILDER_H_

#include "m_ctype.h"
#include "ngs_common/protocol_protobuf.h"
#include "ngs/memory.h"

namespace ngs
{
  class Output_buffer;

  class Message_builder
  {
  public:
    Message_builder();
    ~Message_builder();

    void encode_empty_message(Output_buffer* out_buffer, uint8 type);

  protected:
    typedef ::google::protobuf::io::CodedOutputStream CodedOutputStream;
    void start_message(Output_buffer* out_buffer, uint8 type);
    void end_message();

    Output_buffer     *m_out_buffer;
    ngs::Memory_instrumented<CodedOutputStream>::Unique_ptr m_out_stream;

    int m_size_addr2_size;

    int m_field_number;

    void encode_int32(int32 value, bool write = true);
    void encode_uint32(uint32 value, bool write = true);
    void encode_uint64(uint64 value, bool write = true);
    void encode_string(const char* value, size_t len, bool write = true);
  private:
    // at what byte offset of the m_out_buffer the current row starts
    uint32 m_start_from;

    // address of the buffer part where we need to write row size when it's ready
    google::protobuf::uint8*  m_size_addr1;
    int m_size_addr1_size;
    // address of the second buffer part where we need to write row size when it's ready (if it does not fit one page)
    google::protobuf::uint8*  m_size_addr2;
  };
}


#endif //  _NGS_MESSAGE_BUILDER_H_

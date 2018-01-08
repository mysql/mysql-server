/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_MESSAGE_BUILDER_H_
#define _NGS_MESSAGE_BUILDER_H_

#include "m_ctype.h"
#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"


namespace ngs {

class Output_buffer;

class Message_builder {
 public:
  Message_builder();
  ~Message_builder();

  void encode_empty_message(Output_buffer* out_buffer, const uint8 type);

 protected:
  using CodedOutputStream =
      ::google::protobuf::io::CodedOutputStream;
  using CodedOutputStream_ptr =
      ngs::Memory_instrumented<CodedOutputStream>::Unique_ptr;

 protected:
  void start_message(
      Output_buffer* out_buffer,
      const uint8 type);
  void end_message();

  Output_buffer        *m_out_buffer;
  CodedOutputStream_ptr m_out_stream;
  int                   m_size_addr2_size;
  int                   m_field_number;

  void encode_int32(
      const int32 value,
      const bool write = true);
  void encode_uint32(
      const uint32 value,
      const bool write = true);
  void encode_uint64(
      const uint64 value,
      const bool write = true);
  void encode_string(
      const char* value,
      const size_t len,
      const bool write = true);
  void encode_string(
      const char* value,
      const bool write = true);
private:
  // at what byte offset of the m_out_buffer the current row starts
  uint32 m_start_from;

  // address of the buffer part where we need to write row size when it's ready
  google::protobuf::uint8*  m_size_addr1;
  int m_size_addr1_size;
  // address of the second buffer part where we need to write row size when it's ready (if it does not fit one page)
  google::protobuf::uint8*  m_size_addr2;
};

}  // namespace ngs


#endif  //  _NGS_MESSAGE_BUILDER_H_

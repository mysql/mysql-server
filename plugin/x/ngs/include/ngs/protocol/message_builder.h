/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_BUILDER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_BUILDER_H_

#include <memory>
#include "m_ctype.h"
#include "my_inttypes.h"
#include "page_output_stream.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

class Output_buffer;

class Message_builder {
 public:
  explicit Message_builder(const bool memory_managed = true);
  ~Message_builder();

  uint8 *encode_empty_message(Page_output_stream *out_buffer, const uint8 type);

 protected:
  using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;
  using ZeroCopyOutputStream = ::google::protobuf::io::ZeroCopyOutputStream;
  using Stream_allocator = std::allocator<CodedOutputStream>;

 protected:
  void start_message(Page_output_stream *out_buffer, const uint8 type);
  void end_message();

  void construct_stream();
  void construct_stream(ZeroCopyOutputStream *zero_stream);
  void reset_stream();

  std::string *m_out_string = nullptr;
  Page_output_stream *m_out_page_stream = nullptr;
  CodedOutputStream *m_out_stream;
  bool m_valid_out_stream{false};
  bool m_memory_managed;

  int m_field_number;

  void encode_int32(const int32 value, const bool write = true);
  void encode_uint32(const uint32 value, const bool write = true);
  void encode_uint64(const uint64 value, const bool write = true);
  void encode_string(const char *value, const size_t len,
                     const bool write = true);
  void encode_string(const char *value, const bool write = true);
  void skip_field();

 private:
  // at what byte offset of the m_out_page_stream the current row starts
  uint32 m_start_from;
  google::protobuf::uint8 *m_header_addr;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_BUILDER_H_

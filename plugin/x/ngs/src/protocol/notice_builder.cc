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

#include "plugin/x/ngs/include/ngs/protocol/notice_builder.h"

#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

void Notice_builder::encode_frame(Page_output_stream *out_buffer, uint32 type,
                                  const bool is_local,
                                  const std::string &data) {
  start_message(out_buffer, Mysqlx::ServerMessages::NOTICE);

  // 1) Type
  encode_uint32(type);
  // 2) Scope
  if (is_local)
    encode_int32(Mysqlx::Notice::Frame_Scope_LOCAL);
  else
    skip_field();
  // 3) Payload
  encode_string(data.c_str(), data.length());

  end_message();
}

void Notice_builder::encode_rows_affected(Page_output_stream *out_buffer,
                                          uint64 value) {
  int32 param = Mysqlx::Notice::SessionStateChanged::ROWS_AFFECTED;
  int32 type = Mysqlx::Datatypes::Scalar_Type_V_UINT;

  start_message(out_buffer, Mysqlx::ServerMessages::NOTICE);

  // 1) Type
  encode_uint32(3);

  // 2) Scope
  encode_int32(static_cast<int>(Mysqlx::Notice::Frame_Scope_LOCAL));

  // 3) Payload
  google::protobuf::internal::WireFormatLite::WriteTag(
      3, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
      m_out_stream);
  uint32 size_scalar = CodedOutputStream::VarintSize32SignExtended(type) +
                       CodedOutputStream::VarintSize64(value) + 2 /*tags*/;
  uint32 size_payload =
      1 /* param tag */ + CodedOutputStream::VarintSize32SignExtended(param) +
      1 /* scalar tag */ + CodedOutputStream::VarintSize32(size_scalar) +
      size_scalar;
  m_out_stream->WriteVarint32(size_payload);
  {
    m_field_number = 0;
    // 1) Param
    encode_int32(param);
    // 2) Scalar
    google::protobuf::internal::WireFormatLite::WriteTag(
        2,
        google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
        m_out_stream);
    m_out_stream->WriteVarint32(size_scalar);
    {
      m_field_number = 0;
      // 1) Type
      encode_int32(type);
      // 3!) V_unisgned_int
      m_field_number = 2; /*Need to skip one field tag here*/
      encode_uint64(value);
    }
  }

  end_message();
}

}  // namespace ngs

/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/protocol/metadata_builder.h"

#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

const int k_message_header = 5;

void Metadata_builder::encode_metadata(const Encode_column_info *column_info) {
  const bool has_content_type = column_info->m_content_type != 0;
  const bool write_text_info = !column_info->m_compact;

  google::protobuf::io::StringOutputStream string_stream(&m_metadata);

  m_metadata_start = m_metadata.size();
  {
    CodedOutputStream stream(&string_stream);
    m_out_stream = &stream;
    begin_metadata_message(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);

    // 1) FieldType
    encode_int32(column_info->m_type);
    // 2) Name
    encode_string(column_info->m_col_name, write_text_info);
    // 3) OriginalName
    encode_string(column_info->m_org_col_name, write_text_info);
    // 4) Table
    encode_string(column_info->m_table_name, write_text_info);
    // 5) OriginalTable
    encode_string(column_info->m_org_table_name, write_text_info);
    // 6) Schema
    encode_string(column_info->m_db_name, write_text_info);
    // 7) Catalog
    encode_string(column_info->m_catalog, write_text_info);
    // 8) Collation
    encode_uint64(column_info->m_collation, column_info->m_has_collation);
    // 9) FractionalDigits
    encode_uint32(column_info->m_decimals, column_info->m_has_decimals);
    // 10) Length
    encode_uint32(column_info->m_length, column_info->m_has_length);
    // 11) Flags
    encode_uint32(column_info->m_flags, column_info->m_has_flags);
    // 12) ContentType
    encode_uint32(column_info->m_content_type, has_content_type);
  }

  end_metadata_message();
}

void Metadata_builder::start_metadata_encoding() { m_metadata.erase(); }

const std::string &Metadata_builder::stop_metadata_encoding() const {
  return m_metadata;
}

void Metadata_builder::begin_metadata_message(const uint8 type_id) {
  m_field_number = 0;
  m_out_stream->Skip(k_message_header);
  m_metadata[m_metadata_start + k_message_header - 1] = type_id;
}

void Metadata_builder::end_metadata_message() {
  const uint32 msg_size =
      static_cast<uint32>(m_metadata.size()) - m_metadata_start - 4;
  CodedOutputStream::WriteLittleEndian32ToArray(
      msg_size, reinterpret_cast<google::protobuf::uint8 *>(
                    &(m_metadata[m_metadata_start])));
}

}  // namespace ngs

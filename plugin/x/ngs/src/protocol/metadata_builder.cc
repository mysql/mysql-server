/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"


namespace ngs {

void Metadata_builder::encode_metadata(
  Output_buffer* out_buffer,
  const Encode_column_info *column_info) {
  const bool has_content_type = column_info->m_content_type != 0;
  const bool write_text_info = !column_info->m_compact;

  start_message(out_buffer, Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);

  // 1) FieldType
  encode_int32(column_info->m_type);
  // 2) Name
  encode_string(column_info->m_col_name,
                write_text_info);
  // 3) OriginalName
  encode_string(column_info->m_org_col_name,
                write_text_info);
  // 4) Table
  encode_string(column_info->m_table_name,
                write_text_info);
  // 5) OriginalTable
  encode_string(column_info->m_org_table_name,
                write_text_info);
  // 6) Schema
  encode_string(column_info->m_db_name,
                write_text_info);
  // 7) Catalog
  encode_string(column_info->m_catalog,
                write_text_info);
  // 8) Collation
  encode_uint64(column_info->m_collation,
                column_info->m_has_collation);
  // 9) FractionalDigits
  encode_uint32(column_info->m_decimals,
                column_info->m_has_decimals);
  // 10) Length
  encode_uint32(column_info->m_length,
                column_info->m_has_length);
  // 11) Flags
  encode_uint32(column_info->m_flags,
                column_info->m_has_flags);
  // 12) ContentType
  encode_uint32(column_info->m_content_type,
                has_content_type);

  end_message();
}

}  // namespace ngs

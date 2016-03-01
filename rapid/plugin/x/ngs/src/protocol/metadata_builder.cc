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

#include "ngs/protocol/metadata_builder.h"
#include "ngs_common/protocol_protobuf.h"

using namespace ngs;

void Metadata_builder::encode_metadata(
  Output_buffer* out_buffer,
  const std::string &catalog, const std::string &db_name,
  const std::string &table_name, const std::string &org_table_name,
  const std::string &col_name, const std::string &org_col_name,
  uint64 collation, int type, int decimals,
  uint32 flags, uint32 length, uint32 content_type)
{
  start_message(out_buffer, Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);

  // 1) FieldType
  encode_int32(type);
  // 2) Name
  encode_string(col_name.c_str(), col_name.length());
  // 3) OriginalName
  encode_string(org_col_name.c_str(), org_col_name.length());
  // 4) Table
  encode_string(table_name.c_str(), table_name.length());
  // 5) OriginalTable
  encode_string(org_table_name.c_str(), org_table_name.length());
  // 6) Schema
  encode_string(db_name.c_str(), db_name.length());
  // 7) Catalog
  encode_string(catalog.c_str(), catalog.length());
  // 8) Collation
  encode_uint64(collation);
  // 9) FractionalDigits
  encode_uint32(decimals);
  // 10) Length
  encode_uint32(length);
  // 11) Flags
  encode_uint32(flags);
  // 12) ContentType
  encode_uint32(content_type, content_type != 0);

  end_message();
}

void Metadata_builder::encode_metadata(
  Output_buffer* out_buffer,
  uint64 collation, int type, int decimals,
  uint32 flags, uint32 length, uint32 content_type)
{
  start_message(out_buffer, Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA);

  // 1) FieldType
  encode_int32(type);
  // 2) Name
  encode_string("", 0, false);
  // 3) OriginalName
  encode_string("", 0, false);
  // 4) Table
  encode_string("", 0, false);
  // 5) OriginalTable
  encode_string("", 0, false);
  // 6) Schema
  encode_string("", 0, false);
  // 7) Catalog
  encode_string("", 0, false);
  // 8) Collation
  encode_uint64(collation);
  // 9) FractionalDigits
  encode_uint32(decimals);
  // 10) Length
  encode_uint32(length);
  // 11) Flags
  encode_uint32(flags);
  // 12) ContentType
  encode_uint32(content_type, content_type != 0);

  end_message();
}

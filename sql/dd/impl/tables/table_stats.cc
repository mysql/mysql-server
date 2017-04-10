/* Copyright (c) 2015, 2016  Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/tables/table_stats.h"

#include "dd/impl/raw/object_keys.h"   // Composite_char_key
#include "dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Table_stats::Table_stats()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_SCHEMA_NAME, "FIELD_SCHEMA_NAME",
          "schema_name VARCHAR(64) NOT NULL");
  m_target_def.add_field(FIELD_TABLE_NAME, "FIELD_TABLE_NAME",
          "table_name VARCHAR(64) NOT NULL");
  m_target_def.add_field(FIELD_TABLE_ROWS, "FIELD_TABLE_ROWS",
          "table_rows BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_AVG_ROW_LENGTH, "FIELD_AVG_ROW_LENGTH",
          "avg_row_length BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_DATA_LENGTH, "FIELD_DATA_LENGTH",
          "data_length BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_MAX_DATA_LENGTH, "FIELD_MAX_DATA_LENGTH",
          "max_data_length BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_INDEX_LENGTH, "FIELD_INDEX_LENGTH",
          "index_length BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_DATA_FREE, "FIELD_DATA_FREE",
          "data_free BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_AUTO_INCREMENT, "FIELD_AUTO_INCREMENT",
          "auto_increment BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_CHECKSUM, "FIELD_CHECKSUM",
          "checksum BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_UPDATE_TIME, "FIELD_UPDATE_TIME",
          "update_time TIMESTAMP NULL");
  m_target_def.add_field(FIELD_CHECK_TIME, "FIELD_CHECK_TIME",
          "check_time TIMESTAMP NULL");

  m_target_def.add_index("PRIMARY KEY (schema_name, table_name)");
}

///////////////////////////////////////////////////////////////////////////

const Table_stats &Table_stats::instance()
{
  static Table_stats *s_instance= new Table_stats();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Table_stat::name_key_type *Table_stats::create_object_key(
  const String_type &schema_name,
  const String_type &table_name)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_char_key(INDEX_NO,
                                               FIELD_SCHEMA_NAME, schema_name,
                                               FIELD_TABLE_NAME, table_name);
}

///////////////////////////////////////////////////////////////////////////

}
}

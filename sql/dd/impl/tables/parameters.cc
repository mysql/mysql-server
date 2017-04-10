/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/parameters.h"

#include <new>

#include "dd/impl/raw/object_keys.h"      // dd::Parent_id_range_key
#include "dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Parameters &Parameters::instance()
{
  static Parameters *s_instance= new Parameters();
  return *s_instance;
}

Parameters::Parameters()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_ROUTINE_ID,
                         "FIELD_ROUTINE_ID",
                         "routine_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ORDINAL_POSITION,
                         "FIELD_ORDINAL_POSITION",
                         "ordinal_position INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_MODE,
                         "FIELD_MODE",
                         "mode ENUM('IN','OUT','INOUT')");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_DATA_TYPE,
                         "FIELD_DATA_TYPE",
                         "data_type ENUM(\n"
                         "    'MYSQL_TYPE_DECIMAL', 'MYSQL_TYPE_TINY',\n"
                         "    'MYSQL_TYPE_SHORT',  'MYSQL_TYPE_LONG',\n"
                         "    'MYSQL_TYPE_FLOAT',  'MYSQL_TYPE_DOUBLE',\n"
                         "    'MYSQL_TYPE_NULL', 'MYSQL_TYPE_TIMESTAMP',\n"
                         "    'MYSQL_TYPE_LONGLONG','MYSQL_TYPE_INT24',\n"
                         "    'MYSQL_TYPE_DATE',   'MYSQL_TYPE_TIME',\n"
                         "    'MYSQL_TYPE_DATETIME', 'MYSQL_TYPE_YEAR',\n"
                         "    'MYSQL_TYPE_NEWDATE', 'MYSQL_TYPE_VARCHAR',\n"
                         "    'MYSQL_TYPE_BIT', 'MYSQL_TYPE_TIMESTAMP2',\n"
                         "    'MYSQL_TYPE_DATETIME2', 'MYSQL_TYPE_TIME2',\n"
                         "    'MYSQL_TYPE_NEWDECIMAL', 'MYSQL_TYPE_ENUM',\n"
                         "    'MYSQL_TYPE_SET', 'MYSQL_TYPE_TINY_BLOB',\n"
                         "    'MYSQL_TYPE_MEDIUM_BLOB', "
                         "    'MYSQL_TYPE_LONG_BLOB', 'MYSQL_TYPE_BLOB',\n"
                         "    'MYSQL_TYPE_VAR_STRING',\n"
                         "    'MYSQL_TYPE_STRING', 'MYSQL_TYPE_GEOMETRY',\n"
                         "    'MYSQL_TYPE_JSON'\n"
                         "  ) NOT NULL");
  m_target_def.add_field(FIELD_DATA_TYPE_UTF8,
                         "FIELD_DATA_TYPE_UTF8",
                         "data_type_utf8 MEDIUMTEXT NOT NULL");
  m_target_def.add_field(FIELD_IS_ZEROFILL,
                         "FIELD_IS_ZEROFILL",
                         "is_zerofill BOOL");
  m_target_def.add_field(FIELD_IS_UNSIGNED,
                         "FIELD_IS_UNSIGNED",
                         "is_unsigned BOOL");
  m_target_def.add_field(FIELD_CHAR_LENGTH,
                         "FIELD_CHAR_LENGTH",
                         "char_length INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_PRECISION,
                         "FIELD_NUMERIC_PRECISION",
                         "numeric_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_SCALE,
                         "FIELD_NUMERIC_SCALE",
                         "numeric_scale INT UNSIGNED");
  m_target_def.add_field(FIELD_DATETIME_PRECISION,
                         "FIELD_DATETIME_PRECISION",
                         "datetime_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_COLLATION_ID,
                         "FIELD_COLLATION_ID",
                         "collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_OPTIONS,
                         "FIELD_OPTIONS",
                         "options MEDIUMTEXT");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY (routine_id, ordinal_position)");

  m_target_def.add_foreign_key("FOREIGN KEY (routine_id) REFERENCES "
                               "routines(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (collation_id) REFERENCES "
                               "collations(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Parameters::create_key_by_routine_id(
  Object_id routine_id)
{
  return new (std::nothrow) Parent_id_range_key(
                              1, FIELD_ROUTINE_ID, routine_id);
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Parameters::create_primary_key(
  Object_id routine_id,
  int ordinal_position)
{
  const int INDEX_NO= 1;

  return new (std::nothrow) Composite_pk(
                              INDEX_NO,
                              FIELD_ROUTINE_ID, routine_id,
                              FIELD_ORDINAL_POSITION, ordinal_position);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

} // namespaces tables
} // namespaces dd

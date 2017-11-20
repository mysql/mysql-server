/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/tables/columns.h"

#include <new>

#include "sql/dd/impl/raw/object_keys.h" // Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Columns &Columns::instance()
{
  static Columns *s_instance= new Columns();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Columns::Columns()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_TABLE_ID,
                         "FIELD_TABLE_ID",
                         "table_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE utf8_tolower_ci");
  m_target_def.add_field(FIELD_ORDINAL_POSITION,
                         "FIELD_ORDINAL_POSITION",
                         "ordinal_position INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_TYPE,
                         "FIELD_TYPE",
                         "type ENUM(\n"
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
                         "'MYSQL_TYPE_LONG_BLOB',\n"
                         "    'MYSQL_TYPE_BLOB', 'MYSQL_TYPE_VAR_STRING',\n"
                         "    'MYSQL_TYPE_STRING', 'MYSQL_TYPE_GEOMETRY',\n"
                         "    'MYSQL_TYPE_JSON'\n"
                         "  ) NOT NULL");
  m_target_def.add_field(FIELD_IS_NULLABLE,
                         "FIELD_IS_NULLABLE",
                         "is_nullable BOOL NOT NULL");
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
  m_target_def.add_field(FIELD_HAS_NO_DEFAULT,
                         "FIELD_HAS_NO_DEFAULT",
                         "has_no_default BOOL");
  m_target_def.add_field(FIELD_DEFAULT_VALUE,
                         "FIELD_DEFAULT_VALUE",
                         "default_value BLOB");
  m_target_def.add_field(FIELD_DEFAULT_VALUE_UTF8,
                         "FIELD_DEFAULT_VALUE_UTF8",
                         "default_value_utf8 TEXT");
  m_target_def.add_field(FIELD_DEFAULT_OPTION,
                         "FIELD_DEFAULT_OPTION",
                         "default_option BLOB");
  m_target_def.add_field(FIELD_UPDATE_OPTION,
                         "FIELD_UPDATE_OPTION",
                         "update_option VARCHAR(32)");
  m_target_def.add_field(FIELD_IS_AUTO_INCREMENT,
                         "FIELD_IS_AUTO_INCREMENT",
                         "is_auto_increment BOOL");
  m_target_def.add_field(FIELD_IS_VIRTUAL,
                         "FIELD_IS_VIRTUAL",
                         "is_virtual BOOL");
  m_target_def.add_field(FIELD_GENERATION_EXPRESSION,
                         "FIELD_GENERATION_EXPRESSION",
                         "generation_expression LONGBLOB");
  m_target_def.add_field(FIELD_GENERATION_EXPRESSION_UTF8,
                         "FIELD_GENERATION_EXPRESSION_UTF8",
                         "generation_expression_utf8 LONGTEXT");
  m_target_def.add_field(FIELD_COMMENT,
                         "FIELD_COMMENT",
                         "comment VARCHAR(2048) NOT NULL");
  m_target_def.add_field(FIELD_HIDDEN,
                         "FIELD_HIDDEN",
                         "hidden BOOL NOT NULL");
  m_target_def.add_field(FIELD_OPTIONS,
                         "FIELD_OPTIONS",
                         "options MEDIUMTEXT");
  m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                         "FIELD_SE_PRIVATE_DATA",
                         "se_private_data MEDIUMTEXT");
  m_target_def.add_field(FIELD_COLUMN_KEY,
                         "FIELD_COLUMN_KEY",
                         "column_key ENUM('','PRI','UNI','MUL') NOT NULL");
  m_target_def.add_field(FIELD_COLUMN_TYPE_UTF8,
                         "FIELD_COLUMN_TYPE_UTF8",
                         "column_type_utf8 MEDIUMTEXT NOT NULL");
  m_target_def.add_field(FIELD_SRS_ID,
                         "FIELD_SRS_ID",
                         "srs_id INT UNSIGNED DEFAULT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY(table_id, name)");
  m_target_def.add_index("UNIQUE KEY(table_id, ordinal_position)");

  m_target_def.add_foreign_key("FOREIGN KEY (table_id) REFERENCES tables(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (collation_id) "
                               "REFERENCES collations(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (srs_id) "
                               "REFERENCES st_spatial_reference_systems(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Columns::create_key_by_table_id(
  Object_id table_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

}
}

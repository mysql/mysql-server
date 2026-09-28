/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/tables/udt_types.h"

#include <cstring>
#include <new>

#include "mysql/strings/m_ctype.h"
#include "sql/dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/tables/dd_properties.h"  // TARGET_DD_VERSION
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/types/udt_type_impl.h"  // dd::UDT_type_impl

namespace dd::tables {

const UDT_Types &UDT_Types::instance() {
  static auto *s_instance = new UDT_Types();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

const CHARSET_INFO *UDT_Types::name_collation() {
  return &my_charset_utf8mb3_general_ci;
}

///////////////////////////////////////////////////////////////////////////

UDT_Types::UDT_Types() {
  m_target_def.set_table_name("types");

  m_target_def.add_field(FIELD_ID, "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_SCHEMA_ID, "FIELD_SCHEMA_ID",
                         "schema_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME, "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE " +
                             String_type(name_collation()->m_coll_name));

  // FIXME: revise enum
  m_target_def.add_field(FIELD_TYPE, "FIELD_TYPE",
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
                         "'MYSQL_TYPE_LONG_BLOB',\n"
                         "    'MYSQL_TYPE_BLOB', 'MYSQL_TYPE_VAR_STRING',\n"
                         "    'MYSQL_TYPE_STRING', 'MYSQL_TYPE_GEOMETRY',\n"
                         "    'MYSQL_TYPE_JSON', 'MYSQL_TYPE_VECTOR' \n"
                         "  ) NOT NULL");

  m_target_def.add_field(FIELD_IS_UNSIGNED, "FIELD_IS_UNSIGNED",
                         "is_unsigned BOOL");
  m_target_def.add_field(FIELD_CHAR_LENGTH, "FIELD_CHAR_LENGTH",
                         "char_length INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_PRECISION, "FIELD_NUMERIC_PRECISION",
                         "numeric_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_SCALE, "FIELD_NUMERIC_SCALE",
                         "numeric_scale INT UNSIGNED");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "FIELD_DATETIME_PRECISION",
                         "datetime_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_COLLATION_ID, "FIELD_COLLATION_ID",
                         "collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_COLUMN_TYPE_UTF8, "FIELD_COLUMN_TYPE_UTF8",
                         "column_type_utf8 MEDIUMTEXT NOT NULL");
  m_target_def.add_field(FIELD_IS_EXPLICIT_COLLATION,
                         "FIELD_IS_EXPLICIT_COLLATION",
                         "is_explicit_collation BOOL");
  m_target_def.add_field(FIELD_CREATED, "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL");
  m_target_def.add_field(FIELD_LAST_ALTERED, "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL");

  m_target_def.add_index(INDEX_PK_ID, "INDEX_PK_ID", "PRIMARY KEY (id)");
  m_target_def.add_index(INDEX_UK_SCHEMA_ID_NAME, "INDEX_UK_SCHEMA_ID_NAME",
                         "UNIQUE KEY (schema_id, name)");
  m_target_def.add_index(INDEX_K_COLLATION_ID, "INDEX_K_COLLATION_ID",
                         "KEY(collation_id)");

  m_target_def.add_foreign_key(FK_SCHEMA_ID, "FK_SCHEMA_ID",
                               "FOREIGN KEY (schema_id) "
                               "REFERENCES schemata(id)");
  m_target_def.add_foreign_key(FK_COLLATION_ID, "FK_COLLATIONS_ID",
                               "FOREIGN KEY (collation_id) "
                               "REFERENCES collations(id)");
}

///////////////////////////////////////////////////////////////////////////

UDT_Type *UDT_Types::create_entity_object(const Raw_record &) const {
  return new (std::nothrow) UDT_Type_impl();
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Types::update_object_key(Item_name_key *key, Object_id schema_id,
                                  const String_type &name) {
  key->update(FIELD_SCHEMA_ID, schema_id, FIELD_NAME, name, name_collation());
  return false;
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd::tables

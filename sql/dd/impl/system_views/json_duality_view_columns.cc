/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "sql/dd/impl/system_views/json_duality_view_columns.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Json_duality_view_columns &Json_duality_view_columns::instance() {
  static Json_duality_view_columns *s_instance =
      new Json_duality_view_columns();
  return *s_instance;
}

Json_duality_view_columns::Json_duality_view_columns() {
  m_target_def.set_view_name(view_name());
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_REFERENCED_TABLE_CATALOG,
                         "REFERENCED_TABLE_CATALOG",
                         "columns.referenced_table_catalog" +
                             m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(FIELD_REFERENCED_TABLE_SCHEMA,
                         "REFERENCED_TABLE_SCHEMA",
                         "columns.referenced_table_schema" +
                             m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(FIELD_REFERENCED_TABLE_NAME, "REFERENCED_TABLE_NAME",
                         "columns.referenced_table_name" +
                             m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(FIELD_IS_ROOT_TABLE, "IS_ROOT_TABLE",
                         "columns.is_root_table");
  m_target_def.add_field(FIELD_REFERENCED_TABLE_ID, "REFERENCED_TABLE_ID",
                         "columns.referenced_table_id");
  m_target_def.add_field(FIELD_REFERENCED_COLUMN_NAME, "REFERENCED_COLUMN_NAME",
                         "columns.referenced_column_name");
  m_target_def.add_field(FIELD_JSON_KEY_NAME, "JSON_KEY_NAME",
                         "columns.json_key_name");
  m_target_def.add_field(FIELD_ALLOW_INSERT, "ALLOW_INSERT",
                         "columns.allow_insert");
  m_target_def.add_field(FIELD_ALLOW_UPDATE, "ALLOW_UPDATE",
                         "columns.allow_update");
  m_target_def.add_field(FIELD_ALLOW_DELETE, "ALLOW_DELETE",
                         "columns.allow_delete");
  m_target_def.add_field(FIELD_READ_ONLY, "READ_ONLY", "columns.read_only");

  m_target_def.add_from("mysql.tables tbl");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");
  m_target_def.add_from(
      "JOIN JSON_TABLE(GET_JDV_PROPERTY_KEY_VALUE(sch.name, tbl.name, "
      "GET_DD_PROPERTY_KEY_VALUE(tbl.options, 'view_valid'), '" +
      view_name() +
      "'), "
      "'$.entries[*]' "
      "  COLUMNS ( "
      "    referenced_table_id INT PATH '$.referenced_table_id', "
      "    referenced_table_catalog VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.referenced_table_catalog', "
      "    referenced_table_schema VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.referenced_table_schema', "
      "    referenced_table_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.referenced_table_name', "
      "    is_root_table TINYINT PATH '$.is_root_table', "
      "    referenced_column_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.referenced_column_name', "
      "    json_key_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.json_key_name', "
      "    allow_insert TINYINT PATH '$.allow_insert', "
      "    allow_update TINYINT PATH '$.allow_update', "
      "    allow_delete TINYINT PATH '$.allow_delete', "
      "    read_only TINYINT PATH '$.read_only' "
      "  ) "
      ") "
      "AS columns");

  m_target_def.add_where(
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options)");
  m_target_def.add_where(
      "AND CAN_ACCESS_COLUMN(columns.referenced_table_schema, "
      "columns.referenced_table_name, columns.referenced_column_name)");
  m_target_def.add_where("AND tbl.type = 'VIEW'");
  m_target_def.add_where(
      "AND GET_DD_PROPERTY_KEY_VALUE(tbl.options, 'view_type') = "
      "'JSON_DUALITY'");
}

}  // namespace system_views
}  // namespace dd

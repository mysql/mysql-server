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

#include "sql/dd/impl/system_views/json_duality_view_links.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Json_duality_view_links &Json_duality_view_links::instance() {
  static Json_duality_view_links *s_instance = new Json_duality_view_links();
  return *s_instance;
}

Json_duality_view_links::Json_duality_view_links() {
  m_target_def.set_view_name(view_name());
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(
      FIELD_PARENT_TABLE_CATALOG, "PARENT_TABLE_CATALOG",
      "links.parent_table_catalog" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_PARENT_TABLE_SCHEMA, "PARENT_TABLE_SCHEMA",
      "links.parent_table_schema" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_PARENT_TABLE_NAME, "PARENT_TABLE_NAME",
      "links.parent_table_name" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_CHILD_TABLE_CATALOG, "CHILD_TABLE_CATALOG",
      "links.child_table_catalog" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_CHILD_TABLE_SCHEMA, "CHILD_TABLE_SCHEMA",
      "links.child_table_schema" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_CHILD_TABLE_NAME, "CHILD_TABLE_NAME",
      "links.child_table_name" + m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(FIELD_PARENT_COLUMN_NAME, "PARENT_COLUMN_NAME",
                         "links.parent_column_name COLLATE utf8mb4_0900_ai_ci");
  m_target_def.add_field(FIELD_CHILD_COLUMN_NAME, "CHILD_COLUMN_NAME",
                         "links.child_column_name COLLATE utf8mb4_0900_ai_ci");
  m_target_def.add_field(FIELD_JOIN_TYPE, "JOIN_TYPE", "links.join_type");
  m_target_def.add_field(FIELD_JSON_KEY_NAME, "JSON_KEY_NAME",
                         "links.json_key_name");

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
      "    parent_table_catalog VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.parent_table_catalog', "
      "    parent_table_schema VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.parent_table_schema', "
      "    parent_table_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.parent_table_name', "
      "    parent_column_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.parent_column_name', "
      "    child_table_catalog VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.child_table_catalog', "
      "    child_table_schema VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.child_table_schema', "
      "    child_table_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.child_table_name', "
      "    child_column_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.child_column_name', "
      "    join_type VARCHAR(64) CHARACTER SET utf8mb4 PATH '$.join_type', "
      "    json_key_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.json_key_name'"
      "  ) "
      ") "
      "AS links");

  m_target_def.add_where(
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options)");
  m_target_def.add_where(
      "AND CAN_ACCESS_COLUMN(links.parent_table_schema, "
      "links.parent_table_name, links.parent_column_name)");
  m_target_def.add_where(
      "AND CAN_ACCESS_COLUMN(links.child_table_schema, links.child_table_name, "
      "links.child_column_name)");
  m_target_def.add_where("AND tbl.type = 'VIEW'");
  m_target_def.add_where(
      "AND GET_DD_PROPERTY_KEY_VALUE(tbl.options, 'view_type') = "
      "'JSON_DUALITY'");
}

}  // namespace system_views
}  // namespace dd

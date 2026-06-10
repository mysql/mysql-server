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

#include "sql/dd/impl/system_views/json_duality_views.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Json_duality_views &Json_duality_views::instance() {
  static Json_duality_views *s_instance = new Json_duality_views();
  return *s_instance;
}

Json_duality_views::Json_duality_views() {
  m_target_def.set_view_name(view_name());
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_DEFINER, "DEFINER", "tbl.view_definer");
  m_target_def.add_field(FIELD_SECURITY_TYPE, "SECURITY_TYPE",
                         "IF (tbl.view_security_type='DEFAULT', 'DEFINER', "
                         "tbl.view_security_type)");
  m_target_def.add_field(FIELD_JSON_COLUMN_NAME, "JSON_COLUMN_NAME",
                         "col.name");
  m_target_def.add_field(
      FIELD_ROOT_TABLE_CATALOG, "ROOT_TABLE_CATALOG",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.root_table_catalog, '')" +
          m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_ROOT_TABLE_SCHEMA, "ROOT_TABLE_SCHEMA",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.root_table_schema, '')" +
          m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_ROOT_TABLE_NAME, "ROOT_TABLE_NAME",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.root_table_name, '')" +
          m_target_def.fs_name_collation_utf8mb4());
  m_target_def.add_field(
      FIELD_ALLOW_INSERT, "ALLOW_INSERT",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.allow_insert, '')");
  m_target_def.add_field(
      FIELD_ALLOW_UPDATE, "ALLOW_UPDATE",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.allow_update, '')");
  m_target_def.add_field(
      FIELD_ALLOW_DELETE, "ALLOW_DELETE",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.allow_delete, '')");
  m_target_def.add_field(
      FIELD_READ_ONLY, "READ_ONLY",
      "IF(CAN_ACCESS_TABLE(views.root_table_schema, views.root_table_name) AND "
      "CAN_ACCESS_VIEW(sch.name, tbl.name, tbl.view_definer, tbl.options), "
      "views.read_only, '')");
  m_target_def.add_field(FIELD_STATUS, "STATUS",
                         "IF (GET_DD_PROPERTY_KEY_VALUE(tbl.options, "
                         "'view_valid')=TRUE, 'valid', 'invalid')");

  m_target_def.add_from("mysql.columns col");
  m_target_def.add_from("JOIN mysql.tables tbl ON col.table_id=tbl.id");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");
  m_target_def.add_from(
      "JOIN JSON_TABLE(GET_JDV_PROPERTY_KEY_VALUE(sch.name, tbl.name, "
      "GET_DD_PROPERTY_KEY_VALUE(tbl.options, 'view_valid'), '" +
      view_name() +
      "'), "
      "'$.entries[*]' "
      "  COLUMNS ( "
      "    root_table_catalog VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.root_table_catalog', "
      "    root_table_schema VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.root_table_schema', "
      "    root_table_name VARCHAR(64) CHARACTER SET utf8mb4 PATH "
      "'$.root_table_name', "
      "    allow_insert TINYINT PATH '$.allow_insert', "
      "    allow_update TINYINT PATH '$.allow_update', "
      "    allow_delete TINYINT PATH '$.allow_delete', "
      "    read_only TINYINT PATH '$.read_only' "
      "  ) "
      ") "
      "AS views");

  m_target_def.add_where("CAN_ACCESS_TABLE(sch.name, tbl.name)");
  m_target_def.add_where("AND tbl.type = 'VIEW'");
  m_target_def.add_where(
      "AND GET_DD_PROPERTY_KEY_VALUE(tbl.options, 'view_type') = "
      "'JSON_DUALITY'");
}

}  // namespace system_views
}  // namespace dd

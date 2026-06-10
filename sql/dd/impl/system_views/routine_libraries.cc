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

#include "sql/dd/impl/system_views/routine_libraries.h"

namespace dd::system_views {

const Routine_libraries &Routine_libraries::instance() {
  static auto *s_instance = new Routine_libraries();
  return *s_instance;
}

Routine_libraries::Routine_libraries() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_ROUTINE_CATALOG, "ROUTINE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_ROUTINE_SCHEMA, "ROUTINE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_ROUTINE_NAME, "ROUTINE_NAME", "rtn.name");
  m_target_def.add_field(FIELD_ROUTINE_TYPE, "ROUTINE_TYPE", "rtn.type");
  m_target_def.add_field(FIELD_LIBRARY_CATALOG, "LIBRARY_CATALOG",
                         "IF(ISNULL(lib.catalog), cat.name, lib.catalog)");
  m_target_def.add_field(FIELD_LIBRARY_SCHEMA, "LIBRARY_SCHEMA",
                         "IF(ISNULL(lib.sch), sch.name, lib.sch)");
  m_target_def.add_field(FIELD_LIBRARY_NAME, "LIBRARY_NAME",
                         "lib.library_name");
  m_target_def.add_field(FIELD_LIBRARY_VERSION, "LIBRARY_VERSION",
                         "lib.version");

  m_target_def.add_from("mysql.routines rtn");
  m_target_def.add_from("JOIN mysql.schemata sch ON rtn.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");
  m_target_def.add_from(
      "JOIN "
      "JSON_TABLE(GET_DD_PROPERTY_KEY_VALUE(rtn.options,'libraries'), '$[*]' "
      "COLUMNS(catalog VARCHAR(64) character set utf8mb4 PATH '$.catalog', "
      "sch VARCHAR(100) character set utf8mb4 PATH '$.schema', "
      "library_name VARCHAR(100) character set utf8mb4 PATH '$.name', "
      "version VARCHAR(100) character set utf8mb4 PATH '$.version' ) ) "
      "as lib");

  m_target_def.add_where(
      "CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)");

  m_target_def.add_where("AND rtn.options IS NOT NULL");
  m_target_def.add_where(
      "AND JSON_VALID(GET_DD_PROPERTY_KEY_VALUE(rtn.options,'libraries')) = 1");
}

}  // namespace dd::system_views

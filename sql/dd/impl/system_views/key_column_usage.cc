/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/system_views/key_column_usage.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Key_column_usage &Key_column_usage::instance() {
  static Key_column_usage *s_instance = new Key_column_usage();
  return *s_instance;
}

Key_column_usage::Key_column_usage() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_CONSTRAINT_CATALOG, "CONSTRAINT_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_CONSTRAINT_SCHEMA, "CONSTRAINT_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_CONSTRAINT_NAME, "CONSTRAINT_NAME",
                         "constraints.CONSTRAINT_NAME");
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_COLUMN_NAME, "COLUMN_NAME",
                         "col.name COLLATE utf8mb3_tolower_ci");
  m_target_def.add_field(FIELD_ORDINAL_POSITION, "ORDINAL_POSITION",
                         "constraints.ordinal_position");
  m_target_def.add_field(FIELD_POSITION_IN_UNIQUE_CONSTRAINT,
                         "POSITION_IN_UNIQUE_CONSTRAINT",
                         "constraints.POSITION_IN_UNIQUE_CONSTRAINT");
  m_target_def.add_field(FIELD_REFERENCED_TABLE_SCHEMA,
                         "REFERENCED_TABLE_SCHEMA",
                         "constraints.REFERENCED_TABLE_SCHEMA");
  m_target_def.add_field(FIELD_REFERENCED_TABLE_NAME, "REFERENCED_TABLE_NAME",
                         "constraints.REFERENCED_TABLE_NAME");
  m_target_def.add_field(FIELD_REFERENCED_COLUMN_NAME, "REFERENCED_COLUMN_NAME",
                         "constraints.REFERENCED_COLUMN_NAME");

  m_target_def.add_from("mysql.tables tbl");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");
  m_target_def.add_from(
      ", LATERAL (SELECT"
      "    idx.name AS CONSTRAINT_NAME,"
      "    icu.ordinal_position AS ORDINAL_POSITION,"
      "    NULL AS POSITION_IN_UNIQUE_CONSTRAINT,"
      "    NULL AS REFERENCED_TABLE_SCHEMA,"
      "    NULL AS REFERENCED_TABLE_NAME,"
      "    NULL AS REFERENCED_COLUMN_NAME,"
      "    icu.column_id,"
      "    idx.hidden OR icu.hidden AS HIDDEN"
      "    FROM mysql.indexes idx"
      "    JOIN mysql.index_column_usage icu ON icu.index_id = idx.id"
      "    WHERE idx.table_id = tbl.id"
      "      AND idx.type IN ('PRIMARY', 'UNIQUE')"
      "  UNION ALL SELECT"
      "    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,"
      "    fkcu.ordinal_position AS ORDINAL_POSITION,"
      "    fkcu.ordinal_position AS POSITION_IN_UNIQUE_CONSTRAINT,"
      "    fk.referenced_table_schema AS REFERENCED_TABLE_SCHEMA,"
      "    fk.referenced_table_name AS REFERENCED_TABLE_NAME,"
      "    fkcu.referenced_column_name AS REFERENCED_COLUMN_NAME,"
      "    fkcu.column_id,"
      "    FALSE AS HIDDEN"
      "    FROM mysql.foreign_keys fk"
      "    JOIN mysql.foreign_key_column_usage fkcu ON fkcu.foreign_key_id = "
      "fk.id"
      "    WHERE fk.table_id = tbl.id"
      ") constraints");
  m_target_def.add_from(
      "JOIN mysql.columns col ON constraints.COLUMN_ID=col.id");

  m_target_def.add_where("CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name)");
  m_target_def.add_where(
      "AND IS_VISIBLE_DD_OBJECT(tbl.hidden, "
      "col.hidden NOT IN ('Visible', 'User') OR constraints.HIDDEN, "
      "col.options)");
}

}  // namespace system_views
}  // namespace dd

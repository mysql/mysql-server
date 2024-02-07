/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "sql/dd/impl/system_views/table_constraints.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Table_constraints &Table_constraints::instance() {
  static Table_constraints *s_instance = new Table_constraints();
  return *s_instance;
}

Table_constraints::Table_constraints() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_CONSTRAINT_CATALOG, "CONSTRAINT_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_CONSTRAINT_SCHEMA, "CONSTRAINT_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_CONSTRAINT_NAME, "CONSTRAINT_NAME",
                         "constraints.CONSTRAINT_NAME");
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_CONSTRAINT_TYPE, "CONSTRAINT_TYPE",
                         "constraints.CONSTRAINT_TYPE");
  m_target_def.add_field(FIELD_ENFORCED, "ENFORCED", "constraints.ENFORCED");

  m_target_def.add_from("mysql.tables tbl");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id = sch.catalog_id");
  m_target_def.add_from(
      ", LATERAL ( SELECT"
      "    idx.name AS CONSTRAINT_NAME,"
      "    IF (idx.type='PRIMARY', 'PRIMARY KEY', idx.type) as CONSTRAINT_TYPE,"
      "    'YES' as ENFORCED"
      "  FROM mysql.indexes idx"
      "    WHERE idx.table_id=tbl.id AND idx.type IN ('PRIMARY', 'UNIQUE')"
      "          AND IS_VISIBLE_DD_OBJECT(tbl.hidden, idx.hidden, idx.options)"
      " UNION ALL"
      "  SELECT"
      "    fk.name COLLATE utf8mb3_tolower_ci AS CONSTRAINT_NAME,"
      "    'FOREIGN KEY' as CONSTRAINT_TYPE,"
      "    'YES' as ENFORCED"
      "  FROM mysql.foreign_keys fk WHERE fk.table_id=tbl.id"
      " UNION ALL"
      "  SELECT"
      "    cc.name AS CONSTRAINT_NAME,"
      "    'CHECK' as CONSTRAINT_TYPE,"
      "    cc.enforced as ENFORCED"
      "  FROM mysql.check_constraints cc WHERE cc.table_id=tbl.id"
      ") constraints");
  m_target_def.add_where("CAN_ACCESS_TABLE(sch.name, tbl.name)");
  m_target_def.add_where("AND IS_VISIBLE_DD_OBJECT(tbl.hidden)");
}

}  // namespace system_views
}  // namespace dd

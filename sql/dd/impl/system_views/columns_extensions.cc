/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "sql/dd/impl/system_views/columns_extensions.h"

#include <string>

#include "sql/dd/string_type.h"
#include "string_with_len.h"

namespace {
enum {
  FIELD_TABLE_CATALOG,
  FIELD_TABLE_SCHEMA,
  FIELD_TABLE_NAME,
  FIELD_COLUMN_NAME,
  FIELD_ENGINE_ATTRIBUTE,
  FIELD_SECONDARY_ENGINE_ATTRIBUTE
};

const dd::String_type s_view_name{STRING_WITH_LEN("COLUMNS_EXTENSIONS")};
const dd::system_views::Columns_extensions *s_instance =
    new dd::system_views::Columns_extensions(s_view_name);

}  // namespace

namespace dd {
namespace system_views {

const Columns_extensions &Columns_extensions::instance() { return *s_instance; }

Columns_extensions::Columns_extensions(const dd::String_type &n) {
  m_target_def.set_view_name(n);

  // SELECT Identifier
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_COLUMN_NAME, "COLUMN_NAME",
                         "col.name COLLATE utf8mb3_tolower_ci");

  // SELECT extension fields
  m_target_def.add_field(FIELD_ENGINE_ATTRIBUTE, "ENGINE_ATTRIBUTE",
                         "col.engine_attribute");

  m_target_def.add_field(FIELD_SECONDARY_ENGINE_ATTRIBUTE,
                         "SECONDARY_ENGINE_ATTRIBUTE",
                         "col.secondary_engine_attribute");

  // FROM
  m_target_def.add_from("mysql.columns col");
  m_target_def.add_from("JOIN mysql.tables tbl ON col.table_id=tbl.id");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");

  // WHERE
  m_target_def.add_where(
      "INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name,"
      "tbl.name, tbl.type, tbl.options)");
  m_target_def.add_where(
      "AND CAN_ACCESS_COLUMN(sch.name, tbl.name, "
      "col.name)");
  m_target_def.add_where(
      "AND IS_VISIBLE_DD_OBJECT(tbl.hidden, col.hidden NOT IN ('Visible', "
      "'User'), col.options)");
}

const dd::String_type &Columns_extensions::view_name() { return s_view_name; }
}  // namespace system_views
}  // namespace dd

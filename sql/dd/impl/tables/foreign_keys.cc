/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/foreign_keys.h"

#include <new>

#include "dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "dd/impl/types/object_table_definition_impl.h"
#include "system_variables.h"

namespace dd {
namespace tables {

const Foreign_keys &Foreign_keys::instance()
{
  static Foreign_keys *s_instance= new Foreign_keys();
  return *s_instance;
}

Foreign_keys::Foreign_keys()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_SCHEMA_ID,
                         "FIELD_SCHEMA_ID",
                         "schema_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_TABLE_ID,
                         "FIELD_TABLE_ID",
                         "table_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_UNIQUE_CONSTRAINT_ID,
                         "FIELD_UNIQUE_CONSTRAINT_ID",
                         "unique_constraint_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_MATCH_OPTION,
                         "FIELD_MATCH_OPTION",
                         "match_option ENUM('NONE', 'PARTIAL', 'FULL') "
                         "NOT NULL");
  m_target_def.add_field(FIELD_UPDATE_RULE,
                         "FIELD_UPDATE_RULE",
                         "update_rule ENUM(\n"
                         "  'NO ACTION', 'RESTRICT',\n"
                         "  'CASCADE', 'SET NULL',\n"
                         "  'SET DEFAULT'\n"
                         ") NOT NULL");
  m_target_def.add_field(FIELD_DELETE_RULE,
                         "FIELD_DELETE_RULE",
                         "delete_rule ENUM(\n"
                         "  'NO ACTION', 'RESTRICT',\n"
                         "  'CASCADE', 'SET NULL',\n"
                         "  'SET DEFAULT'\n"
                         ") NOT NULL");
  m_target_def.add_field(FIELD_REFERENCED_CATALOG,
                         "FIELD_REFERENCED_CATALOG",
                         "referenced_table_catalog "
                         "VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_REFERENCED_SCHEMA,
                         "FIELD_REFERENCED_SCHEMA",
                         "referenced_table_schema "
                         "VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_REFERENCED_TABLE,
                         "FIELD_REFERENCED_TABLE",
                         "referenced_table_name "
                         "VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));

  m_target_def.add_index("PRIMARY KEY (id)");
  m_target_def.add_index("UNIQUE KEY (schema_id, name)");
  m_target_def.add_index("UNIQUE KEY (table_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (schema_id) REFERENCES "
                               "schemata(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (unique_constraint_id) "
                               "REFERENCES indexes(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Foreign_keys::create_key_by_table_id(Object_id table_id)
{
  return new (std::nothrow) Parent_id_range_key(2, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

}
}

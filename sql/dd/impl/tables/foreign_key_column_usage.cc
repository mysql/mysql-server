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

#include "sql/dd/impl/tables/foreign_key_column_usage.h"

#include <new>

#include "sql/dd/impl/raw/object_keys.h" // Parent_id_range_key
#include "sql/dd/impl/tables/dd_properties.h"     // TARGET_DD_VERSION
#include "sql/dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Foreign_key_column_usage &Foreign_key_column_usage::instance()
{
  static Foreign_key_column_usage *s_instance= new Foreign_key_column_usage();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Foreign_key_column_usage::Foreign_key_column_usage()
{
  m_target_def.set_table_name("foreign_key_column_usage");

  m_target_def.add_field(FIELD_FOREIGN_KEY_ID,
                         "FIELD_FOREIGN_KEY_ID",
                         "foreign_key_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ORDINAL_POSITION,
                         "FIELD_ORDINAL_POSITION",
                         "ordinal_position INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_COLUMN_ID,
                         "FIELD_COLUMN_ID",
                         "column_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_REFERENCED_COLUMN_NAME,
                         "FIELD_REFERENCED_COLUMN_NAME",
                         "referenced_column_name VARCHAR(64) NOT NULL "
                         "COLLATE utf8_tolower_ci");

  m_target_def.add_index(INDEX_PK_FOREIGN_KEY_ID_ORDINAL_POSITION,
                         "INDEX_PK_FOREIGN_KEY_ID_ORDINAL_POSITION",
                         "PRIMARY KEY(foreign_key_id, ordinal_position)");
  m_target_def.add_index(INDEX_UK_FOREIGN_KEY_ID_COLUMN_ID,
                         "INDEX_UK_FOREIGN_KEY_ID_COLUMN_ID",
                         "UNIQUE KEY(foreign_key_id, column_id, "
                         "referenced_column_name)");
  m_target_def.add_index(INDEX_K_COLUMN_ID,
                         "INDEX_K_COLUMN_ID",
                         "KEY(column_id)");

  m_target_def.add_foreign_key(FK_FOREIGN_KEY_ID,
                               "FK_FOREIGN_KEY_ID",
                               "FOREIGN KEY (foreign_key_id) REFERENCES "
                               "foreign_keys(id)");
  m_target_def.add_foreign_key(FK_COLUMN_ID,
                               "FK_COLUMN_ID",
                               "FOREIGN KEY (column_id) REFERENCES "
                               "columns(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Foreign_key_column_usage::create_key_by_foreign_key_id(Object_id fk_id)
{
  return new (std::nothrow) Parent_id_range_key(
          INDEX_PK_FOREIGN_KEY_ID_ORDINAL_POSITION, FIELD_FOREIGN_KEY_ID, fk_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Foreign_key_column_usage::create_primary_key(
  Object_id fk_id, int ordinal_position)
{
  return new (std::nothrow) Composite_pk(
                          INDEX_PK_FOREIGN_KEY_ID_ORDINAL_POSITION,
                          FIELD_FOREIGN_KEY_ID, fk_id,
                          FIELD_ORDINAL_POSITION, ordinal_position);
}

///////////////////////////////////////////////////////////////////////////

}
}

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

#include "sql/dd/impl/tables/indexes.h"

#include <new>

#include "sql/dd/impl/raw/object_keys.h" // dd::Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Indexes &Indexes::instance()
{
  static Indexes *s_instance= new Indexes();
  return *s_instance;
}

Indexes::Indexes()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_TABLE_ID,
                         "FIELD_TABLE_ID",
                         "table_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE utf8_tolower_ci");
  m_target_def.add_field(FIELD_TYPE,
                         "FIELD_TYPE",
                         "type ENUM(\n"
                         "  'PRIMARY',\n"
                         "  'UNIQUE',\n"
                         "  'MULTIPLE',\n"
                         "  'FULLTEXT',\n"
                         "  'SPATIAL'\n"
                         ") NOT NULL");
  m_target_def.add_field(FIELD_ALGORITHM,
                         "FIELD_ALGORITHM",
                         "algorithm ENUM(\n"
                         "  'SE_SPECIFIC',\n"
                         "  'BTREE',\n"
                         "  'RTREE',\n"
                         "  'HASH',\n"
                         "  'FULLTEXT'\n"
                         ") NOT NULL");
  m_target_def.add_field(FIELD_IS_ALGORITHM_EXPLICIT,
                         "FIELD_IS_ALGORITHM_EXPLICIT",
                         "is_algorithm_explicit BOOL NOT NULL");
  m_target_def.add_field(FIELD_IS_VISIBLE,
                         "FIELD_IS_VISIBLE",
                         "is_visible BOOL NOT NULL");
  m_target_def.add_field(FIELD_IS_GENERATED,
                         "FIELD_IS_GENERATED",
                         "is_generated BOOL NOT NULL");
  m_target_def.add_field(FIELD_HIDDEN,
                         "FIELD_HIDDEN",
                         "hidden BOOL NOT NULL");
  m_target_def.add_field(FIELD_ORDINAL_POSITION,
                         "FIELD_ORDINAL_POSITION",
                         "ordinal_position INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_COMMENT,
                         "FIELD_COMMENT",
                         "comment VARCHAR(2048) NOT NULL");
  m_target_def.add_field(FIELD_OPTIONS,
                         "FIELD_OPTIONS",
                         "options MEDIUMTEXT");
  m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                         "FIELD_SE_PRIVATE_DATA",
                         "se_private_data MEDIUMTEXT");
  m_target_def.add_field(FIELD_TABLESPACE_ID,
                         "FIELD_TABLESPACE_ID",
                         "tablespace_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_ENGINE,
                         "FIELD_ENGINE",
                         "engine VARCHAR(64) NOT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY(table_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (table_id) REFERENCES "
                               "tables(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) REFERENCES "
                               "tablespaces(id)");
}


///////////////////////////////////////////////////////////////////////////

Object_key *Indexes::create_key_by_table_id(Object_id table_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

}
}

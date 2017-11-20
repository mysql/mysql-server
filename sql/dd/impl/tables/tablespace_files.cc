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

#include "sql/dd/impl/tables/tablespace_files.h"

#include <new>

#include "sql/dd/impl/raw/object_keys.h" // Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Tablespace_files &Tablespace_files::instance()
{
  static Tablespace_files *s_instance= new Tablespace_files();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Tablespace_files::Tablespace_files()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_TABLESPACE_ID,
                         "FIELD_TABLESPACE_ID",
                         "tablespace_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ORDINAL_POSITION,
                         "FIELD_ORDINAL_POSITION",
                         "ordinal_position INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_FILE_NAME,
                         "FIELD_FILE_NAME",
                         "file_name VARCHAR(512) NOT NULL");
  m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                         "FIELD_SE_PRIVATE_DATA",
                         "se_private_data MEDIUMTEXT");

  m_target_def.add_index("UNIQUE KEY (tablespace_id, ordinal_position)");
  m_target_def.add_index("UNIQUE KEY (file_name)");

  m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) \
                                REFERENCES tablespaces(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Tablespace_files::create_key_by_tablespace_id(
  Object_id tablespace_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_TABLESPACE_ID,
                                               tablespace_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Tablespace_files::create_primary_key(
  Object_id tablespace_id, int ordinal_position)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_pk(INDEX_NO,
                          FIELD_TABLESPACE_ID, tablespace_id,
                          FIELD_ORDINAL_POSITION, ordinal_position);
}

///////////////////////////////////////////////////////////////////////////

}
}

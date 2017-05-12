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

#include "dd/impl/tables/tablespaces.h"

#include <new>

#include "dd/impl/raw/object_keys.h"       // dd::Global_name_key
#include "dd/impl/types/object_table_definition_impl.h"
#include "dd/impl/types/tablespace_impl.h" // dd::Tablespace_impl

namespace dd {
class Dictionary_object;
class Raw_record;
}  // namespace dd

namespace dd {
namespace tables {

const Tablespaces &Tablespaces::instance()
{
  static Tablespaces *s_instance= new Tablespaces();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Tablespaces::Tablespaces()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(255) NOT NULL COLLATE utf8_bin");
  m_target_def.add_field(FIELD_OPTIONS,
                         "FIELD_OPTIONS",
                         "options MEDIUMTEXT");
  m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                         "FIELD_SE_PRIVATE_DATA",
                         "se_private_data MEDIUMTEXT");
  m_target_def.add_field(FIELD_COMMENT,
                         "FIELD_COMMENT",
                         "comment VARCHAR(2048) NOT NULL");
  m_target_def.add_field(FIELD_ENGINE,
                         "FIELD_ENGINE",
                         "engine VARCHAR(64) NOT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY(name)");
}

///////////////////////////////////////////////////////////////////////////

Dictionary_object*
Tablespaces::create_dictionary_object(const Raw_record &) const
{
  return new (std::nothrow) Tablespace_impl();
}

///////////////////////////////////////////////////////////////////////////

bool Tablespaces::update_object_key(Global_name_key *key,
                                    const String_type &tablespace_name)
{
  key->update(FIELD_NAME, tablespace_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

}
}

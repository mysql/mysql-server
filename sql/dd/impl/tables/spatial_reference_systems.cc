/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/tables/spatial_reference_systems.h"

#include <string.h>
#include <new>

#include "m_ctype.h"
#include "sql/dd/impl/raw/object_keys.h"                // Parent_id_range_key
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"// dd::Spatial_refere...

namespace dd {
namespace tables {

const Spatial_reference_systems & Spatial_reference_systems::instance()
{
  static Spatial_reference_systems *s_instance=
    new Spatial_reference_systems();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Spatial_reference_systems::Spatial_reference_systems()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id INTEGER UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_CATALOG_ID, "FIELD_CATALOG_ID",
                         "catalog_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name CHARACTER VARYING(80)\n"
                         "NOT NULL COLLATE utf8_general_ci");
  m_target_def.add_field(FIELD_LAST_ALTERED,
                         "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL\n"
                         "DEFAULT CURRENT_TIMESTAMP\n"
                         "ON UPDATE CURRENT_TIMESTAMP");
  m_target_def.add_field(FIELD_CREATED,
                         "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL\n"
                         "DEFAULT CURRENT_TIMESTAMP");
  m_target_def.add_field(FIELD_ORGANIZATION,
                         "FIELD_ORGANIZATION",
                         "organization CHARACTER VARYING(256)\n");
  m_target_def.add_field(FIELD_ORGANIZATION_COORDSYS_ID,
                         "FIELD_ORGANIZATION_COORDSYS_ID",
                         "organization_coordsys_id INTEGER UNSIGNED");
  m_target_def.add_field(FIELD_DEFINITION,
                         "FIELD_DEFINITION",
                         "definition CHARACTER VARYING(4096)\n"
                         "NOT NULL");
  m_target_def.add_field(FIELD_DESCRIPTION,
                         "FIELD_DESCRIPTION",
                         "description CHARACTER VARYING(2048)");

  m_target_def.add_index("PRIMARY KEY (id)");
  m_target_def.add_index("UNIQUE KEY (catalog_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (catalog_id) REFERENCES \
                                  catalogs(id)");
}


///////////////////////////////////////////////////////////////////////////

Spatial_reference_system*
Spatial_reference_systems::create_entity_object(const Raw_record &) const
{
  return new (std::nothrow) Spatial_reference_system_impl();
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_systems::update_object_key(Item_name_key *key,
                                                  Object_id catalog_id,
                                                  const String_type &name)
{
  // Construct a lowercase version of the key. The collation of the
  // name column is also accent insensitive, but we don't have a
  // function to make a canonical accent insensitive representation
  // yet. We have to settle for a lowercase name here and reject
  // accent variations when trying to store the object.
  char lowercase_name[257];
  memcpy(lowercase_name, name.c_str(), name.size() + 1);
  my_casedn_str(&my_charset_utf8_general_ci, lowercase_name);
  key->update(FIELD_CATALOG_ID, catalog_id, FIELD_NAME, lowercase_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *
Spatial_reference_systems::create_key_by_catalog_id(Object_id catalog_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_CATALOG_ID,
                                                catalog_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}

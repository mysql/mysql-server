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

#include "dd/impl/tables/schemata.h"

#include "dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "dd/impl/types/schema_impl.h"                  // dd::Schema_impl

#include <string>

namespace dd {
namespace tables {

const Schemata &Schemata::instance()
{
  static Schemata *s_instance= new Schemata();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Schemata::Schemata()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID,
                         "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_CATALOG_ID, "FIELD_CATALOG_ID",
                         "catalog_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE " +
                         std::string(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_DEFAULT_COLLATION_ID,
                         "FIELD_DEFAULT_COLLATION_ID",
                         "default_collation_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_CREATED,
                         "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL\n"
                         " DEFAULT CURRENT_TIMESTAMP\n"
                         " ON UPDATE CURRENT_TIMESTAMP");
  m_target_def.add_field(FIELD_LAST_ALTERED,
                         "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");

  m_target_def.add_index("PRIMARY KEY (id)");
  m_target_def.add_index("UNIQUE KEY (catalog_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (catalog_id) REFERENCES \
                                catalogs(id)");
  m_target_def.add_foreign_key("FOREIGN KEY (default_collation_id) \
                                REFERENCES collations(id)");
}

///////////////////////////////////////////////////////////////////////////

bool Schemata::update_object_key(Item_name_key *key,
                                 Object_id catalog_id,
                                 const std::string &schema_name)
{
  char buf[NAME_LEN + 1];
  key->update(FIELD_CATALOG_ID, catalog_id, FIELD_NAME,
              Object_table_definition_impl::fs_name_case(schema_name, buf));
  return false;
}

///////////////////////////////////////////////////////////////////////////

Dictionary_object *Schemata::create_dictionary_object(const Raw_record &) const
{
  return new (std::nothrow) Schema_impl();

}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Schemata::create_key_by_catalog_id(
  Object_id catalog_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_CATALOG_ID, catalog_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}

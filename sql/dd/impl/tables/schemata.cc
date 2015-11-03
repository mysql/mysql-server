/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

namespace dd {
namespace tables {

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

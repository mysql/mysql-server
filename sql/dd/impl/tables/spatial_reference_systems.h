/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__SPATIAL_REFERENCE_SYSTEMS_INCLUDED
#define DD_TABLES__SPATIAL_REFERENCE_SYSTEMS_INCLUDED

#include "my_global.h"

#include "dd/impl/types/dictionary_object_table_impl.h" // dd::Dictionary_obj...

#include <string>

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Spatial_reference_systems : public Dictionary_object_table_impl
{
public:
  static const Spatial_reference_systems &instance();

  static const std::string &table_name()
  {
    static std::string s_table_name("st_spatial_reference_systems");
    return s_table_name;
  }

  enum enum_fields
  {
    FIELD_ID,
    FIELD_CATALOG_ID,
    FIELD_NAME,
    FIELD_LAST_ALTERED,
    FIELD_CREATED,
    FIELD_ORGANIZATION,
    FIELD_ORGANIZATION_COORDSYS_ID,
    FIELD_DEFINITION,
    FIELD_DESCRIPTION
  };

  Spatial_reference_systems();

  virtual const std::string &name() const
  { return Spatial_reference_systems::table_name(); }

  virtual Dictionary_object *create_dictionary_object(const Raw_record &) const;

  static bool update_object_key(Item_name_key *key,
                                Object_id catalog_id,
                                const std::string &name);

  static Object_key *create_key_by_catalog_id(Object_id catalog_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__SPATIAL_REFERENCE_SYSTEMS_INCLUDED

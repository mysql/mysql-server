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

#include "sql/dd/impl/types/spatial_reference_system_impl.h"

#include <stdint.h>

#include "my_rapidjson_size_t.h"    // IWYU pragma: keep
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "m_string.h"
#include "sql/dd/impl/dictionary_impl.h"   // Dictionary_impl
#include "sql/dd/impl/raw/raw_record.h"    // Raw_record
#include "sql/dd/impl/sdi_impl.h"          // sdi read/write functions
#include "sql/dd/impl/tables/spatial_reference_systems.h" // Spatial_reference_sy...
#include "sql/dd/impl/transaction_impl.h"  // Open_dictionary_tables_ctx
#include "sql/gis/srs/srs.h"               // gis::srs::parse_wkt

namespace dd {
class Sdi_rcontext;
class Sdi_wcontext;
}  // namespace dd

using dd::tables::Spatial_reference_systems;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Spatial reference system implementation.
///////////////////////////////////////////////////////////////////////////

const Entity_object_table &Spatial_reference_system::OBJECT_TABLE()
{
  return Spatial_reference_systems::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Spatial_reference_system::TYPE()
{
  static Spatial_reference_system_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Spatial_reference_system_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system_impl::validate() const
{
  // The ID is an unsigned value, so we don't need to check the lower
  // bound.
  return id() > UINT32_MAX;
}


bool Spatial_reference_system_impl::is_lat_long() const
{
  return (is_geographic() &&
          (m_parsed_definition->axis_direction(0) ==
           gis::srs::Axis_direction::NORTH ||
           m_parsed_definition->axis_direction(0) ==
           gis::srs::Axis_direction::SOUTH));
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Spatial_reference_systems::FIELD_ID);
  restore_name(r, Spatial_reference_systems::FIELD_NAME);

  m_last_altered= r.read_int(Spatial_reference_systems::FIELD_LAST_ALTERED);
  m_created= r.read_int(Spatial_reference_systems::FIELD_CREATED);
  m_organization= r.read_str(Spatial_reference_systems::FIELD_ORGANIZATION);
  m_organization_coordsys_id= r.read_int(
                     Spatial_reference_systems::FIELD_ORGANIZATION_COORDSYS_ID);
  m_definition= r.read_str(Spatial_reference_systems::FIELD_DEFINITION);
  m_description= r.read_str(Spatial_reference_systems::FIELD_DESCRIPTION);

  return parse_definition();
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system_impl::store_attributes(Raw_record *r)
{
  Object_id default_catalog_id= Dictionary_impl::instance()->default_catalog_id();

  return store_id(r, Spatial_reference_systems::FIELD_ID) ||
         store_name(r, Spatial_reference_systems::FIELD_NAME) ||
         r->store(Spatial_reference_systems::FIELD_CATALOG_ID,
                  default_catalog_id) ||
         r->store(Spatial_reference_systems::FIELD_LAST_ALTERED,
                  m_last_altered) ||
         r->store(Spatial_reference_systems::FIELD_CREATED, m_created) ||
         r->store(Spatial_reference_systems::FIELD_ORGANIZATION,
                  m_organization) ||
         r->store(Spatial_reference_systems::FIELD_ORGANIZATION_COORDSYS_ID,
                  m_organization_coordsys_id) ||
         r->store(Spatial_reference_systems::FIELD_DEFINITION, m_definition) ||
         r->store(Spatial_reference_systems::FIELD_DESCRIPTION, m_description);
}

///////////////////////////////////////////////////////////////////////////

void Spatial_reference_system_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w)
  const
{
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);
  write(w, m_last_altered, STRING_WITH_LEN("last_altered"));
  write(w, m_created, STRING_WITH_LEN("created"));
  write(w, m_organization, STRING_WITH_LEN("organization"));
  write(w, m_organization_coordsys_id,
        STRING_WITH_LEN("organization_coordsys_id"));
  write(w, m_definition, STRING_WITH_LEN("definition"));
  write(w, m_description, STRING_WITH_LEN("description"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system_impl::deserialize(Sdi_rcontext *rctx,
                                                const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);
  read(&m_last_altered, val, "last_altered");
  read(&m_created, val, "created");
  read(&m_organization, val, "organization");
  read(&m_organization_coordsys_id, val, "organization_coordsys_id");
  read(&m_definition, val, "definition");
  read(&m_description, val, "description");

  return parse_definition();
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system_impl::parse_definition()
{
  gis::srs::Spatial_reference_system *srs= nullptr;
  // parse_wkt() will only allocate memory if successful.
  if (!gis::srs::parse_wkt(id(),
                          &m_definition.front(),
                          &m_definition.back()+1,
                          &srs))
  {
    m_parsed_definition.reset(srs);
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Spatial_reference_system::update_name_key(name_key_type *key,
                                               const String_type &name)
{
  return Spatial_reference_systems::update_object_key(key,
                      Dictionary_impl::instance()->default_catalog_id(),
                      name);
}

///////////////////////////////////////////////////////////////////////////
// Spatial_reference_system_type implementation.
///////////////////////////////////////////////////////////////////////////

void Spatial_reference_system_type::register_tables(
                      Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Spatial_reference_systems>();
}

///////////////////////////////////////////////////////////////////////////

}

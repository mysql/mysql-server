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

#ifndef DD__SPATIAL_REFERENCE_SYSTEM_INCLUDED
#define DD__SPATIAL_REFERENCE_SYSTEM_INCLUDED

#include <cstddef>                        // std::nullptr_t

#include "my_inttypes.h"
#include "nullable.h"
#include "sql/dd/impl/raw/object_keys.h"  // IWYU pragma: keep
#include "sql/dd/types/entity_object.h"   // dd::Entity_object
#include "sql/gis/srid.h"
#include "sql/gis/srs/srs.h"              // srid_t

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object_table;
class Item_name_key;
class Object_type;
class Void_key;

namespace tables {
  class Spatial_reference_systems;
}

///////////////////////////////////////////////////////////////////////////

class Spatial_reference_system : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Entity_object_table &OBJECT_TABLE();

  typedef Spatial_reference_system cache_partition_type;
  typedef tables::Spatial_reference_systems cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Item_name_key name_key_type;
  typedef Void_key aux_key_type;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(name_key_type *key, const String_type &name);

  virtual bool update_aux_key(aux_key_type*) const
  { return true; }

public:
  virtual ~Spatial_reference_system()
  { };

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created(bool convert_time) const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered(bool convert_time) const = 0;
  virtual void set_last_altered(ulonglong last_altered) = 0;

  /////////////////////////////////////////////////////////////////////////
  // organization
  /////////////////////////////////////////////////////////////////////////

  virtual const Mysql::Nullable<String_type> &organization() const = 0;
  virtual void set_organization(const String_type &organization) = 0;
  virtual void set_organization(std::nullptr_t) = 0;

  /////////////////////////////////////////////////////////////////////////
  // organization_coordsys_id
  /////////////////////////////////////////////////////////////////////////

  virtual const Mysql::Nullable<gis::srid_t> &organization_coordsys_id()
      const = 0;
  virtual void
    set_organization_coordsys_id(gis::srid_t organization_coordsys_id) = 0;
  virtual void set_organization_coordsys_id(std::nullptr_t) = 0;

  /////////////////////////////////////////////////////////////////////////
  // definition
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definition() const = 0;
  virtual void set_definition(const String_type &definition) = 0;
  virtual bool is_projected() const = 0;
  virtual bool is_cartesian() const = 0;
  virtual bool is_geographic() const = 0;

  /**
    Check whether an SRS has latitude-longitude axis ordering.
    
    @retval true the axis order is latitude-longitude
    @retval false the SRS is not geographic, or is geographic longitude-latitude
  */
  virtual bool is_lat_long() const = 0;

  virtual double semi_major_axis() const = 0;
  virtual double semi_minor_axis() const = 0;
  virtual double angular_unit() const = 0;
  virtual double prime_meridian() const = 0;
  virtual bool positive_east() const = 0;
  virtual bool positive_north() const = 0;
  virtual double to_radians(double d) const = 0;
  virtual double from_radians(double d) const = 0;

  /////////////////////////////////////////////////////////////////////////
  // description
  /////////////////////////////////////////////////////////////////////////

  virtual const Mysql::Nullable<String_type> &description() const = 0;
  virtual void set_description(const String_type &description) = 0;
  virtual void set_description(std::nullptr_t) = 0;

  /**
    Allocate a new object and invoke the copy constructor

    @return pointer to dynamically allocated copy
  */
  virtual Spatial_reference_system *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SPATIAL_REFERENCE_SYSTEM_INCLUDE

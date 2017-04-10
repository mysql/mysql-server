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

#ifndef DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED
#define DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED

#include <stdio.h>
#include <memory>                             // std::unique_ptr
#include <new>
#include <string>

#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/sdi_fwd.h"
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/spatial_reference_system.h"// dd:Spatial_reference_system
#include "dd/types/weak_object.h"
#include "gis/srid.h"
#include "gis/srs/srs.h"                      // gis::srs::Spatial_reference_...
#include "my_inttypes.h"

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Open_dictionary_tables_ctx;
class Sdi_rcontext;
class Sdi_wcontext;

///////////////////////////////////////////////////////////////////////////

class Spatial_reference_system_impl : public Entity_object_impl,
                                      public Spatial_reference_system
{
public:
  Spatial_reference_system_impl()
   :m_created(0),
    m_last_altered(0),
    m_organization(),
    m_organization_coordsys_id(0),
    m_definition(),
    m_parsed_definition(),
    m_description()
  { }

  virtual ~Spatial_reference_system_impl()
  { }

private:
  Spatial_reference_system_impl(const Spatial_reference_system_impl &srs)
    :Weak_object(srs),
     Entity_object_impl(srs),
     m_created(srs.m_created),
     m_last_altered(srs.m_last_altered),
     m_organization(srs.m_organization),
     m_organization_coordsys_id(srs.m_organization_coordsys_id),
     m_definition(srs.m_definition),
     m_parsed_definition(srs.m_parsed_definition->clone()),
     m_description(srs.m_description)
  { }

public:
  virtual const Dictionary_object_table &object_table() const override
  { return Spatial_reference_system::OBJECT_TABLE(); }

  virtual bool validate() const override;

  virtual bool store_attributes(Raw_record *r) override;

  virtual bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  /// Parse the SRS definition string.
  ///
  /// Used internally. Made public to make it easier to write unit tests.
  bool parse_definition();

public:
  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created() const override
  { return m_created; }

  virtual void set_created(ulonglong created) override
  { m_created= created; }

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const override
  { return m_last_altered; }

  virtual void set_last_altered(ulonglong last_altered) override
  { m_last_altered= last_altered; }

  /////////////////////////////////////////////////////////////////////////
  // organization
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &organization() const override
  { return m_organization; }

  virtual void set_organization(const String_type &organization) override
  { m_organization= organization; }

  /////////////////////////////////////////////////////////////////////////
  // organization_coordsys_id
  /////////////////////////////////////////////////////////////////////////

  virtual gis::srid_t organization_coordsys_id() const override
  { return m_organization_coordsys_id; }

  virtual void set_organization_coordsys_id(
     gis::srid_t organization_coordsys_id) override
  { m_organization_coordsys_id= organization_coordsys_id; }

  /////////////////////////////////////////////////////////////////////////
  // definition
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definition() const override
  { return m_definition; }

  virtual void set_definition(const String_type &definition) override
  { m_definition= definition; }

  virtual bool is_projected() const override
  { return (m_parsed_definition->srs_type() == gis::srs::Srs_type::PROJECTED); }

  virtual bool is_geographic() const override
  { return (m_parsed_definition->srs_type() == gis::srs::Srs_type::GEOGRAPHIC); }

  virtual bool is_cartesian() const override
  { return (m_parsed_definition->srs_type() == gis::srs::Srs_type::PROJECTED); }

  virtual bool is_lat_long() const override;

  virtual double semi_major_axis() const override
  {
    if (is_geographic())
    {
      return static_cast<gis::srs::Geographic_srs *>
        (m_parsed_definition.get())->semi_major_axis();
    }
    else
    {
      return 0.0;
    }
  }

  virtual double semi_minor_axis() const override
  {
    if (is_geographic())
    {
      gis::srs::Geographic_srs *srs= static_cast<gis::srs::Geographic_srs *>
        (m_parsed_definition.get());
      if (srs->inverse_flattening() == 0.0)
        return srs->semi_major_axis();
      else
        return srs->semi_major_axis() * (1 - 1 / srs->inverse_flattening());
    }
    else
    {
      return 0.0;
    }
  }

  virtual double angular_unit() const override
  {
    return m_parsed_definition->angular_unit();
  }

  virtual double prime_meridian() const override
  {
    return m_parsed_definition->prime_meridian();
  }

  virtual bool positive_east() const override
  {
    if (is_lat_long())
    {
      return (m_parsed_definition->axis_direction(1) ==
              gis::srs::Axis_direction::EAST);
    }
    else
    {
      return (m_parsed_definition->axis_direction(0) ==
              gis::srs::Axis_direction::EAST);
    }
  }

  virtual bool positive_north() const override
  {
    if (is_lat_long())
    {
      return (m_parsed_definition->axis_direction(0) ==
              gis::srs::Axis_direction::NORTH);
    }
    else
    {
      return (m_parsed_definition->axis_direction(1) ==
              gis::srs::Axis_direction::NORTH);
    }
  }

  virtual double from_radians(double d) const override
  {
    DBUG_ASSERT(is_geographic());
    DBUG_ASSERT(angular_unit() > 0.0);
    return d / angular_unit();
  }

  /////////////////////////////////////////////////////////////////////////
  // description
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &description() const override
  { return m_description; }

  virtual void set_description(const String_type &description) override
  { m_description= description; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl() override
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const override
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const override
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const override
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const override
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name) override
  { Entity_object_impl::set_name(name); }

public:
  virtual void debug_print(String_type &outb) const override
  {
    char outbuf[1024];
    sprintf(outbuf, "SPATIAL REFERENCE SYSTEM OBJECT: id= {OID: %lld}, "
            "name= %s, m_created= %llu, m_last_altered= %llu",
            id(), name().c_str(), m_created, m_last_altered);
    outb= String_type(outbuf);
  }

private:
  // Fields
  ulonglong m_created;
  ulonglong m_last_altered;
  String_type m_organization;
  gis::srid_t m_organization_coordsys_id;
  String_type m_definition;
  std::unique_ptr<gis::srs::Spatial_reference_system> m_parsed_definition;
  String_type m_description;

  Spatial_reference_system *clone() const override
  {
    return new Spatial_reference_system_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Spatial_reference_system_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Spatial_reference_system_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED

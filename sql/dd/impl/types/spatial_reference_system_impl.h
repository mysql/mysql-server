/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED
#define DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED

#include <assert.h>
#include <stdio.h>

#include <cstddef>  // std::nullptr_t
#include <memory>   // std::unique_ptr
#include <new>
#include <optional>

#include "my_inttypes.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/spatial_reference_system.h"  // dd:Spatial_reference_system
#include "sql/dd/types/weak_object.h"
#include "sql/gis/geometries.h"  // gis::Coordinate_system
#include "sql/gis/srid.h"        // srid_t
#include "sql/gis/srs/srs.h"     // gis::srs::Spatial_reference_...
#include "sql/sql_time.h"        // gmt_time_to_local_time

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Spatial_reference_system_impl : public Entity_object_impl,
                                      public Spatial_reference_system {
 public:
  Spatial_reference_system_impl()
      : m_created(0),
        m_last_altered(0),
        m_organization(),
        m_organization_coordsys_id(),
        m_definition(),
        m_parsed_definition(),
        m_description() {}

 private:
  Spatial_reference_system_impl(const Spatial_reference_system_impl &srs)
      : Weak_object(srs),
        Entity_object_impl(srs),
        m_created(srs.m_created),
        m_last_altered(srs.m_last_altered),
        m_organization(srs.m_organization),
        m_organization_coordsys_id(srs.m_organization_coordsys_id),
        m_definition(srs.m_definition),
        m_parsed_definition(srs.m_parsed_definition->clone()),
        m_description(srs.m_description) {}

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  /// Parse the SRS definition string.
  ///
  /// Used internally. Made public to make it easier to write unit tests.
  bool parse_definition();

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  ulonglong created(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_created) : m_created;
  }

  void set_created(ulonglong created) override { m_created = created; }

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  ulonglong last_altered(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_last_altered)
                        : m_last_altered;
  }

  void set_last_altered(ulonglong last_altered) override {
    m_last_altered = last_altered;
  }

  /////////////////////////////////////////////////////////////////////////
  // organization
  /////////////////////////////////////////////////////////////////////////

  const std::optional<String_type> &organization() const override {
    return m_organization;
  }

  void set_organization(const String_type &organization) override {
    m_organization = std::optional<String_type>(organization);
  }

  void set_organization(std::nullptr_t) override {
    m_organization = std::optional<String_type>();
  }

  /////////////////////////////////////////////////////////////////////////
  // organization_coordsys_id
  /////////////////////////////////////////////////////////////////////////

  const std::optional<gis::srid_t> &organization_coordsys_id() const override {
    return m_organization_coordsys_id;
  }

  void set_organization_coordsys_id(
      gis::srid_t organization_coordsys_id) override {
    m_organization_coordsys_id =
        std::optional<gis::srid_t>(organization_coordsys_id);
  }

  void set_organization_coordsys_id(std::nullptr_t) override {
    m_organization_coordsys_id = std::optional<gis::srid_t>();
  }

  /////////////////////////////////////////////////////////////////////////
  // definition
  /////////////////////////////////////////////////////////////////////////

  const String_type &definition() const override { return m_definition; }

  void set_definition(const String_type &definition) override {
    m_definition = definition;
  }

  gis::Coordinate_system cs_type() const override {
    // Work around bugs in Developer Studio 12.5 on Solaris by casting the enum
    // to int. Otherwise the default case, and only the default case, is always
    // executed. This happens regardless of SRS type value.
    switch (static_cast<int>(m_parsed_definition->srs_type())) {
      case static_cast<int>(gis::srs::Srs_type::PROJECTED):
        return gis::Coordinate_system::kCartesian;
      case static_cast<int>(gis::srs::Srs_type::GEOGRAPHIC):
        return gis::Coordinate_system::kGeographic;
      default:
        /* purecov: begin deadcode */
        assert(false);
        return gis::Coordinate_system::kCartesian;
        /* purecov: end */
    }
  }

  bool is_projected() const override {
    return (m_parsed_definition->srs_type() == gis::srs::Srs_type::PROJECTED);
  }

  bool is_geographic() const override {
    return (m_parsed_definition->srs_type() == gis::srs::Srs_type::GEOGRAPHIC);
  }

  bool is_cartesian() const override {
    return (m_parsed_definition->srs_type() == gis::srs::Srs_type::PROJECTED);
  }

  bool is_lat_long() const override;

  double semi_major_axis() const override {
    if (is_geographic()) {
      return static_cast<gis::srs::Geographic_srs *>(m_parsed_definition.get())
          ->semi_major_axis();
    } else {
      return 0.0;
    }
  }

  double semi_minor_axis() const override {
    if (is_geographic()) {
      gis::srs::Geographic_srs *srs =
          static_cast<gis::srs::Geographic_srs *>(m_parsed_definition.get());
      if (srs->inverse_flattening() == 0.0)
        return srs->semi_major_axis();
      else
        return srs->semi_major_axis() * (1 - 1 / srs->inverse_flattening());
    } else {
      return 0.0;
    }
  }

  double linear_unit() const override {
    return m_parsed_definition->linear_unit();
  }

  double angular_unit() const override {
    return m_parsed_definition->angular_unit();
  }

  double prime_meridian() const override {
    return m_parsed_definition->prime_meridian();
  }

  bool positive_east() const override {
    if (is_lat_long()) {
      return (m_parsed_definition->axis_direction(1) ==
              gis::srs::Axis_direction::EAST);
    } else {
      return (m_parsed_definition->axis_direction(0) ==
              gis::srs::Axis_direction::EAST);
    }
  }

  bool positive_north() const override {
    if (is_lat_long()) {
      return (m_parsed_definition->axis_direction(0) ==
              gis::srs::Axis_direction::NORTH);
    } else {
      return (m_parsed_definition->axis_direction(1) ==
              gis::srs::Axis_direction::NORTH);
    }
  }

  bool missing_towgs84() const override {
    return (!m_parsed_definition->is_wgs84_based() &&
            !m_parsed_definition->has_towgs84());
  }

  double to_radians(double d) const override {
    assert(is_geographic());
    assert(angular_unit() > 0.0);
    return d * angular_unit();
  }

  double from_radians(double d) const override {
    assert(is_geographic());
    assert(angular_unit() > 0.0);
    return d / angular_unit();
  }

  double to_normalized_latitude(double d) const override {
    double latitude = to_radians(d);
    if (!positive_north()) latitude *= -1.0;
    return latitude;
  }

  double from_normalized_latitude(double d) const override {
    double latitude = from_radians(d);
    if (!positive_north()) latitude *= -1.0;
    return latitude;
  }

  double to_normalized_longitude(double d) const override {
    double longitude = d;
    if (!positive_east()) longitude *= -1.0;
    longitude += prime_meridian();
    longitude *= angular_unit();
    return longitude;
  }

  double from_normalized_longitude(double d) const override {
    double longitude = d;
    longitude /= angular_unit();
    longitude -= prime_meridian();
    if (!positive_east()) longitude *= -1.0;
    return longitude;
  }

  bool can_be_modified_to(const Spatial_reference_system &srs) const override {
    return m_parsed_definition->can_be_modified_to(
        *static_cast<const Spatial_reference_system_impl &>(srs)
             .m_parsed_definition);
  }

  String_type proj4_parameters() const override {
    return m_parsed_definition->proj4_parameters().c_str();
  }

  /////////////////////////////////////////////////////////////////////////
  // description
  /////////////////////////////////////////////////////////////////////////

  const std::optional<String_type> &description() const override {
    return m_description;
  }

  void set_description(const String_type &description) override {
    m_description = std::optional<String_type>(description);
  }

  void set_description(std::nullptr_t) override {
    m_description = std::optional<String_type>();
  }

  // Fix "inherits ... via dominance" warnings
  Entity_object_impl *impl() override { return Entity_object_impl::impl(); }
  const Entity_object_impl *impl() const override {
    return Entity_object_impl::impl();
  }
  Object_id id() const override { return Entity_object_impl::id(); }
  bool is_persistent() const override {
    return Entity_object_impl::is_persistent();
  }
  const String_type &name() const override {
    return Entity_object_impl::name();
  }
  void set_name(const String_type &name) override {
    Entity_object_impl::set_name(name);
  }

 public:
  void debug_print(String_type &outb) const override {
    char outbuf[1024];
    sprintf(outbuf,
            "SPATIAL REFERENCE SYSTEM OBJECT: id= {OID: %lld}, "
            "name= %s, m_created= %llu, m_last_altered= %llu",
            id(), name().c_str(), m_created, m_last_altered);
    outb = String_type(outbuf);
  }

 private:
  // Fields
  ulonglong m_created;
  ulonglong m_last_altered;
  std::optional<String_type> m_organization;
  std::optional<gis::srid_t> m_organization_coordsys_id;
  String_type m_definition;
  std::unique_ptr<gis::srs::Spatial_reference_system> m_parsed_definition;
  std::optional<String_type> m_description;

  Spatial_reference_system *clone() const override {
    return new Spatial_reference_system_impl(*this);
  }

  Spatial_reference_system *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop SRSes en masse we still create slimmed
      down version for consistency sake.
    */
    Spatial_reference_system_impl *placeholder =
        new Spatial_reference_system_impl();
    placeholder->set_id(id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__SPATIAL_REFERENCE_SYSTEM_IMPL_INCLUDED

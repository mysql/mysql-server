/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef UNITTEST_GUNIT_GIS_SRS_H_INCLUDED
#define UNITTEST_GUNIT_GIS_SRS_H_INCLUDED

#include <memory>  // std::unique_ptr

#include "sql/dd/dd.h"  // dd:create_object
#include "sql/dd/impl/types/spatial_reference_system_impl.h"

namespace {
namespace gis_srs {

// EPSG 4326, but with long-lat axes (E-N).
std::unique_ptr<dd::Spatial_reference_system_impl> swapped_epsg4326() {
  auto srs = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl*>(
          dd::create_object<dd::Spatial_reference_system>())};

  srs->set_id(4326);
  srs->set_name("WGS 84");
  srs->set_created(0UL);
  srs->set_last_altered(0UL);
  srs->set_organization("EPSG");
  srs->set_organization_coordsys_id(4326);
  srs->set_definition(
      "GEOGCS[\"WGS 84\",DATUM[\"World Geodetic System "
      "1984\",SPHEROID[\"WGS "
      "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
      "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
      "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
      "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\","
      "NORTH],AUTHORITY[\"EPSG\",\"4326\"]]");
  srs->set_description("");
  srs->parse_definition();

  return srs;
}

}  // namespace gis_srs
}  // namespace

#endif  // include guard

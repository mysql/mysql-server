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

#ifndef UNITTEST_GUNIT_GIS_TYPESET_H_INCLUDED
#define UNITTEST_GUNIT_GIS_TYPESET_H_INCLUDED

#include "sql/gis/geometries_cs.h"  // gis::{Cartesian_*,Geographic_*}

namespace {
namespace gis_typeset {

struct Cartesian {
  using Point = gis::Cartesian_point;
  using Linestring = gis::Cartesian_linestring;
  using Linearring = gis::Cartesian_linearring;
  using Polygon = gis::Cartesian_polygon;
  using Geometrycollection = gis::Cartesian_geometrycollection;
  using Multipoint = gis::Cartesian_multipoint;
  using Multilinestring = gis::Cartesian_multilinestring;
  using Multipolygon = gis::Cartesian_multipolygon;

  static gis::Coordinate_system coordinate_system() {
    return gis::Coordinate_system::kCartesian;
  }
};

struct Geographic {
  using Point = gis::Geographic_point;
  using Linestring = gis::Geographic_linestring;
  using Linearring = gis::Geographic_linearring;
  using Polygon = gis::Geographic_polygon;
  using Geometrycollection = gis::Geographic_geometrycollection;
  using Multipoint = gis::Geographic_multipoint;
  using Multilinestring = gis::Geographic_multilinestring;
  using Multipolygon = gis::Geographic_multipolygon;

  static gis::Coordinate_system coordinate_system() {
    return gis::Coordinate_system::kGeographic;
  }
};

typedef ::testing::Types<Cartesian, Geographic> Test_both;

}  // namespace gis_types
}  // namespace

#endif  // include guard

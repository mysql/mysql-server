/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <initializer_list>

#include <gtest/gtest.h>
#include "mysqld_error.h"
#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/frechet_distance.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/st_units_of_measure.h"
#include "unittest/gunit/test_utils.h"

namespace frechet_distance_unittest {

auto GetCartesianSrs() {
  // Cartesian SRS.
  return std::unique_ptr<dd::Spatial_reference_system_impl>();
}

auto GetGeographicalSrs() {
  // EPSG 4326, but with long-lat axes (E-N).
  std::unique_ptr<dd::Spatial_reference_system_impl> srs(
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>()));
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

auto GetGeographicalSrsDiffFlat() {
  // EPSG 4326, but with long-lat axes (E-N) and different flattening.
  std::unique_ptr<dd::Spatial_reference_system_impl> srs(
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>()));
  srs->set_id(4326);
  srs->set_name("WGS 84");
  srs->set_created(0UL);
  srs->set_last_altered(0UL);
  srs->set_organization("EPSG");
  srs->set_organization_coordsys_id(4326);
  srs->set_definition(
      "GEOGCS[\"WGS 84\",DATUM[\"World Geodetic System "
      "1984\",SPHEROID[\"WGS "
      "84\",6378137,290,AUTHORITY[\"EPSG\",\"7030\"]],"
      "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
      "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
      "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\","
      "NORTH],AUTHORITY[\"EPSG\",\"4326\"]]");
  srs->set_description("");
  srs->parse_definition();
  return srs;
}

template <typename Point, typename Linestring>
auto MakeLinestring(const std::initializer_list<double> &data) {
  if (data.size() % 2 != 0) {
    throw std::exception(); /* purecov: dead code */
  }
  Linestring linestring;
  for (auto it = data.begin(); it != data.end(); std::advance(it, 2)) {
    linestring.push_back(Point(*it, *(it + 1)));
  }
  return linestring;
}

template <typename SRS>
double TestFrechetDistance(const gis::Geometry &g1, const gis::Geometry &g2,
                           SRS srs, bool *null_value = nullptr) {
  double frechet_distance = 0.0;
  if (null_value != nullptr) {
    bool res = gis::frechet_distance(srs, &g1, &g2, "testcase",
                                     &frechet_distance, null_value);
    EXPECT_FALSE(res);
  } else {
    bool is_null = false;
    bool res = gis::frechet_distance(srs, &g1, &g2, "testcase",
                                     &frechet_distance, &is_null);
    EXPECT_FALSE(res);
    EXPECT_FALSE(is_null);
  }
  return frechet_distance;
}

TEST(FrechetDistanceUnitTest, LinestringLinestring) {
  auto gl1 = MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
      {-1., 0.5, -.1, 0.5});
  auto gl2 = MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
      {-1., 0.5, .1, 0.5});
  auto srs1 = GetGeographicalSrs();
  // 32bit computes distance as 1119900.6074340444
  EXPECT_NEAR(TestFrechetDistance(gl1, gl2, srs1.get()), 1119900.6074340483,
              1e-8);

  auto srs2 = GetGeographicalSrsDiffFlat();
  // 32bit computes distance as 1119925.1618088416
  EXPECT_NEAR(TestFrechetDistance(gl1, gl2, srs2.get()), 1119925.1618088456,
              1e-8);

  auto cl1 = MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {-1., 0.5, -.1, 0.5});
  auto cl2 = MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {-1., 0.5, .1, 0.5});
  auto srs3 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestFrechetDistance(cl1, cl2, srs3.get()), 0.2);
}

}  // namespace frechet_distance_unittest

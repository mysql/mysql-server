/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>
#include <initializer_list>
#include "mysqld_error.h"
#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/hausdorff_distance.h"
#include "sql/gis/st_units_of_measure.h"
#include "unittest/gunit/test_utils.h"

namespace hausdorff_distance_unittest {

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

template <typename Point, typename MultiPoint>
auto MakeMultipoint(
    const std::initializer_list<std::initializer_list<double>> &data) {
  MultiPoint multipoint;
  for (auto point : data) {
    EXPECT_EQ(point.size(), 2);
    multipoint.push_back(Point(*point.begin(), *(point.begin() + 1)));
  }
  return multipoint;
}

template <typename SRS>
double TestHausdorffDistance(const gis::Geometry &g1, const gis::Geometry &g2,
                             SRS srs, bool *null_value = nullptr) {
  double hausdorff_distance = 0.0;
  if (null_value != nullptr) {
    bool res = gis::hausdorff_distance(srs, &g1, &g2, "testcase",
                                       &hausdorff_distance, null_value);
    EXPECT_FALSE(res);
  } else {
    bool is_null = false;
    bool res = gis::hausdorff_distance(srs, &g1, &g2, "testcase",
                                       &hausdorff_distance, &is_null);
    EXPECT_FALSE(res);
    EXPECT_FALSE(is_null);
  }
  return hausdorff_distance;
}

TEST(HausdorffDistanceUnitTest, LinestringLinestring) {
  auto gl1 = MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
      {-1., 0.5, -.1, 0.5});
  auto gl2 = MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
      {-1., 0.5, .1, 0.5});
  auto srs1 = GetGeographicalSrs();
  // 32bit may compute distance as 1119900.6074340444
  EXPECT_NEAR(TestHausdorffDistance(gl1, gl2, srs1.get()), 1119900.6074340483,
              1e-8);
  auto srs2 = GetGeographicalSrsDiffFlat();
  // 32bit may compute distance as 1119925.1618088416
  EXPECT_NEAR(TestHausdorffDistance(gl1, gl2, srs2.get()), 1119925.1618088456,
              1e-8);
  auto cl1 = MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {-1., 0.5, -.1, 0.5});
  auto cl2 = MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {-1., 0.5, .1, 0.5});
  auto srs3 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cl1, cl2, srs3.get()), 0.2);
}

TEST(HausdorffDistanceUnitTest, PointMultipoint) {
  gis::Geographic_point gp{0.0, 0.0};
  auto gmp = MakeMultipoint<gis::Geographic_point, gis::Geographic_multipoint>(
      {{0, 1}, {1, 0}, {1, 1}, {1, 2}, {1, 3}});
  auto srs1 = GetGeographicalSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gp, gmp, srs1.get()),
                   6352860.8773248382);
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gmp, gp, srs1.get()),
                   6352860.8773248382);

  gis::Cartesian_point cp{0.0, 0.0};
  auto cmp = MakeMultipoint<gis::Cartesian_point, gis::Cartesian_multipoint>(
      {{0, 1}, {1, 0}, {1, 1}, {1, 2}, {1, 3}});
  auto srs2 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cp, cmp, srs2.get()), 1);
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cmp, cp, srs2.get()), 1);
}

TEST(HausdorffDistanceUnitTest, MultipointMultipoint) {
  auto gmp1 = MakeMultipoint<gis::Geographic_point, gis::Geographic_multipoint>(
      {{0, 0}, {1, 1}, {2, 1}, {1, 2}, {2, 2}});
  auto gmp2 = MakeMultipoint<gis::Geographic_point, gis::Geographic_multipoint>(
      {{0, 1}, {1, 0}, {1, 1}, {1, 2}, {1, 3}});
  auto srs1 = GetGeographicalSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gmp1, gmp2, srs1.get()),
                   6352860.8773248382);

  auto cmp1 = MakeMultipoint<gis::Cartesian_point, gis::Cartesian_multipoint>(
      {{0, 0}, {1, 1}, {2, 1}, {1, 2}, {2, 2}});
  auto cmp2 = MakeMultipoint<gis::Cartesian_point, gis::Cartesian_multipoint>(
      {{0, 1}, {1, 0}, {1, 1}, {1, 2}, {1, 3}});
  auto srs2 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cmp1, cmp2, srs2.get()), 1);
}

TEST(HausdorffDistanceUnitTest, MultilinestringMultilinestring) {
  gis::Geographic_multilinestring gml1;
  gml1.push_back(
      MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
          {0, 0, 1, 1, 2, 1, 1, 2, 2, 2}));
  gis::Geographic_multilinestring gml2;
  gml2.push_back(
      MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
          {0, 1, 1, 0, 1, 1, 1, 2, 1, 3}));

  auto srs1 = GetGeographicalSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gml1, gml2, srs1.get()),
                   6352860.8773248382);

  gis::Cartesian_multilinestring cml1;
  cml1.push_back(
      MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
          {0, 0, 1, 1, 2, 1, 1, 2, 2, 2}));
  gis::Cartesian_multilinestring cml2;
  cml2.push_back(
      MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
          {0, 1, 1, 0, 1, 1, 1, 2, 1, 3}));

  auto srs2 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cml1, cml2, srs2.get()), 1);
}

TEST(HausdorffDistanceUnitTest, LinestringMultilinestring) {
  auto gl = MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
      {0, 0, 1, 1, 2, 1, 1, 2, 2, 2});
  gis::Geographic_multilinestring gml;
  gml.push_back(
      MakeLinestring<gis::Geographic_point, gis::Geographic_linestring>(
          {0, 1, 1, 0, 1, 1, 1, 2, 1, 3}));

  auto srs1 = GetGeographicalSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gl, gml, srs1.get()),
                   6352860.8773248382);
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(gml, gl, srs1.get()),
                   6352860.8773248382);

  auto cl = MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {0, 0, 1, 1, 2, 1, 1, 2, 2, 2});
  gis::Cartesian_multilinestring cml;
  cml.push_back(MakeLinestring<gis::Cartesian_point, gis::Cartesian_linestring>(
      {0, 1, 1, 0, 1, 1, 1, 2, 1, 3}));

  auto srs2 = GetCartesianSrs();
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cl, cml, srs2.get()), 1);
  EXPECT_DOUBLE_EQ(TestHausdorffDistance(cml, cl, srs2.get()), 1);
}

}  // namespace hausdorff_distance_unittest

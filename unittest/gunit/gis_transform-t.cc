// Copyright (c) 2022, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include "my_config.h"

#include <gtest/gtest.h>
#include <memory>  // unique_ptr

#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/relops.h"
#include "sql/gis/transform.h"
#include "unittest/gunit/gis_setops_testshapes.h"
#include "unittest/gunit/gis_typeset.h"

#include <boost/geometry.hpp>

namespace gis_transform_unittest {

dd::String_type wgs84 =
    "GEOGCS[\"WGS 84\",DATUM[\"World Geodetic System "
    "1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,"
    "0,0,0],"
    "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\","
    "NORTH],AUTHORITY[\"EPSG\",\"4326\"]]";

dd::String_type webmerc3857 =
    "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"World "
    "Geodetic System 1984\",SPHEROID[\"WGS "
    "84\",6378137,298.257223563],TOWGS84[0,0,0,0,0,0,0]],PRIMEM[\"Greenwich\","
    "0],UNIT[\"degree\",0."
    "017453292519943278],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST]],PROJECTION["
    "\"Popular Visualisation Pseudo "
    "Mercator\",AUTHORITY[\"EPSG\",\"1024\"]],PARAMETER[\"Latitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude "
    "of natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1],AXIS[\"X\","
    "EAST],AXIS[\"Y\",NORTH]]";

dd::String_type modairy =
    "GEOGCS[\"modairy\",DATUM[\"modairy\",SPHEROID[\"Bessel 1841 "
    "84\",6377340.189,299.324937365,AUTHORITY[\"EPSG\",\"7004\"]],TOWGS84[0,0,"
    "0,0,0,0,0],"
    "AUTHORITY[\"EPSG\",\"6120\"]],PRIMEM[\"Greenwich\",0,AUTHORITY["
    "\"EPSG\",\"8901\"]],UNIT[\"degree\",0.017453292519943278,"
    "AUTHORITY[\"EPSG\",\"9122\"]],AXIS[\"Lon\",EAST],AXIS[\"Lat\","
    "NORTH],AUTHORITY[\"EPSG\",\"4120\"]]";

dd::String_type webmerc_modairy =
    "PROJCS[\"modairy / Pseudo-Mercator\",GEOGCS[\"modairy\",DATUM[\" "
    "modairy\",SPHEROID[\"Bessel "
    "1841\",6377340.189,299.324937365],TOWGS84[0,0,0,0,0,0,0]],PRIMEM["
    "\"Greenwich\",0],UNIT[\"degree\",0."
    "017453292519943278],AXIS[\"Lat\",NORTH],AXIS[\"Lon\",EAST]],PROJECTION["
    "\"Popular Visualisation Pseudo "
    "Mercator\",AUTHORITY[\"EPSG\",\"1024\"]],PARAMETER[\"Latitude of "
    "natural origin\",0,AUTHORITY[\"EPSG\",\"8801\"]],PARAMETER[\"Longitude "
    "of natural origin\",0,AUTHORITY[\"EPSG\",\"8802\"]],PARAMETER[\"False "
    "easting\",0,AUTHORITY[\"EPSG\",\"8806\"]],PARAMETER[\"False "
    "northing\",0,AUTHORITY[\"EPSG\",\"8807\"]],UNIT[\"metre\",1],AXIS[\"X\","
    "EAST],AXIS[\"Y\",NORTH]]";

template <typename Geometry>
struct print {
  static void apply() {}
};

template <>
struct print<gis::Cartesian_point> {
  static void apply(gis::Cartesian_point p) {
    std::cout << std::setprecision(20) << p.x() << " , " << p.y() << std::endl;
  }
};
template <>
struct print<gis::Geographic_point> {
  static void apply(gis::Geographic_point p) {
    std::cout << std::setprecision(20) << p.x() << " , " << p.y() << std::endl;
  }
};

template <>
struct print<gis::Cartesian_linestring> {
  static void apply(gis::Cartesian_linestring g2) {
    for (std::size_t p = 0; p < g2.size(); p++) {
      std::cout << std::setprecision(20) << g2[p].x() << "," << g2[p].y()
                << " ";
    }
    std::cout << std::endl;
  }
};

template <>
struct print<gis::Geographic_linestring> {
  static void apply(gis::Geographic_linestring g2) {
    for (std::size_t p = 0; p < g2.size(); p++) {
      std::cout << std::setprecision(20) << g2[p].x() << "," << g2[p].y()
                << " ";
    }
    std::cout << std::endl;
  }
};

template <typename Geometry1>
auto coverage_transform(const dd::Spatial_reference_system_impl &srs1,
                        const dd::Spatial_reference_system_impl &srs2,
                        const Geometry1 &g1) {
  if (srs1.is_projected())
    assert(g1.coordinate_system() == gis::Coordinate_system::kCartesian);
  else
    assert(g1.coordinate_system() == gis::Coordinate_system::kGeographic);

  std::unique_ptr<gis::Geometry> result_g;
  gis::transform(&srs1, g1, &srs2, "unittest", &result_g);

  return result_g;
}

template <typename Geometry1>
void coverage_transform(const dd::String_type &srs1_str,
                        const dd::String_type &srs2_str, const Geometry1 &g1) {
  auto srs1 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs1->set_id(1000001);
  srs1->set_name("Test1");
  srs1->set_created(0UL);
  srs1->set_last_altered(0UL);
  srs1->set_definition(srs1_str);
  srs1->parse_definition();

  auto srs2 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs2->set_id(1000000);
  srs2->set_name("Test2");
  srs2->set_created(0UL);
  srs2->set_last_altered(0UL);
  srs2->set_definition(srs2_str);
  srs2->parse_definition();

  coverage_transform(*srs1, *srs2, g1);
}

template <typename Geometry1, typename Geometry2>
void check_transform(const dd::Spatial_reference_system_impl &srs1,
                     const dd::Spatial_reference_system_impl &srs2,
                     const Geometry1 &g1, const Geometry2 &g2) {
  if (srs2.is_projected())
    assert(g2.coordinate_system() == gis::Coordinate_system::kCartesian);
  else
    assert(g2.coordinate_system() == gis::Coordinate_system::kGeographic);

  std::unique_ptr<gis::Geometry> result_g;
  result_g = coverage_transform(srs1, srs2, g1);

  auto g = dynamic_cast<Geometry2 *>(result_g.get());

  // Verify result is correct.
  bool is_equals = false;
  bool is_null = false;
  bool equals_error =
      gis::equals(&srs2, &g2, g, "unittest", &is_equals, &is_null);
  EXPECT_FALSE(equals_error);
  EXPECT_FALSE(is_null);
  EXPECT_TRUE(is_equals);

  // for debugging
  // print<Geometry2>::apply(*g);
  // print<Geometry2>::apply(g2);
}

template <typename Geometry1, typename Geometry2>
void check_transform(const dd::String_type &srs1_str,
                     const dd::String_type &srs2_str, const Geometry1 &g1,
                     const Geometry2 &g2) {
  auto srs1 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs1->set_id(1000001);
  srs1->set_name("Test1");
  srs1->set_created(0UL);
  srs1->set_last_altered(0UL);
  srs1->set_definition(srs1_str);
  srs1->parse_definition();

  auto srs2 = std::unique_ptr<dd::Spatial_reference_system_impl>{
      dynamic_cast<dd::Spatial_reference_system_impl *>(
          dd::create_object<dd::Spatial_reference_system>())};
  srs2->set_id(1000000);
  srs2->set_name("Test2");
  srs2->set_created(0UL);
  srs2->set_last_altered(0UL);
  srs2->set_definition(srs2_str);
  srs2->parse_definition();

  check_transform(*srs1, *srs2, g1, g2);
}

TEST(TransformTest, GeogcsProjcsCombinations) {
  gis::Geographic_point gp{0.001, 0.0002};

  // Point to Point transformations
  // geogcs - geogcs
  check_transform(wgs84, modairy, gp,
                  gis::Geographic_point{0.0010000000000000000208,
                                        0.00019999503232222135473});
  check_transform(
      modairy, wgs84, gis::Geographic_point{0.1, 0.2},
      gis::Geographic_point{0.099999999999999991673, 0.20000483632518123445});

  // geogcs - projcs
  check_transform(
      wgs84, webmerc3857, gp,
      gis::Cartesian_point{6378.1369999999997162, 1275.6274085031250252});
  check_transform(
      webmerc3857, wgs84,
      gis::Cartesian_point{6378.1369999999997162, 1275.6274085031250252},
      gis::Geographic_point{0.00099999999999999980398,
                            0.00019999999999975592857});
  check_transform(
      modairy, webmerc3857, gp,
      gis::Cartesian_point{6378.1369999999997162, 1275.6590978060371526});
  check_transform(
      wgs84, webmerc_modairy, gp,
      gis::Cartesian_point{6377.3401890000004641, 1275.4363657304534172});
  check_transform(
      modairy, webmerc_modairy, gp,
      gis::Cartesian_point{6377.3401890000004641, 1275.468046302062703});

  // projcs - projcs
  check_transform(webmerc3857, webmerc_modairy,
                  gis::Cartesian_point{6378.137, 1275.627},
                  gis::Cartesian_point{6377.3401889999986, 1275.4359572862386});
  check_transform(
      webmerc_modairy, webmerc3857,
      gis::Cartesian_point{6377.3401889999986, 1275.4359572862386},
      gis::Cartesian_point{6378.1369999999969878, 1275.6270039824446485});

  // Linestring - Linestring
  gis::Cartesian_point p1{0, -7.0811545516136220613e-10};
  gis::Cartesian_point p2{637813.70000000006985, -7.0811545516136220613e-10};
  gis::Cartesian_linestring ln;
  ln.push_back(p1);
  ln.push_back(p2);

  check_transform(wgs84, webmerc3857, simple_ls<gis_typeset::Geographic>(), ln);

  // for the rest of geometry types perform unit tests for coverage
  coverage_transform(webmerc3857, wgs84, base_py<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mpt<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mls<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, simple_mpy<gis_typeset::Cartesian>());
  typename gis_typeset::Cartesian::Geometrycollection gc_cartesian;
  gc_cartesian.push_back(simple_ls<gis_typeset::Cartesian>());
  gc_cartesian.push_back(base_py<gis_typeset::Cartesian>());
  coverage_transform(webmerc3857, wgs84, gc_cartesian);

  coverage_transform(wgs84, webmerc3857, base_py<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mpt<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mls<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, simple_mpy<gis_typeset::Geographic>());
  typename gis_typeset::Geographic::Geometrycollection gc_geo;
  gc_geo.push_back(simple_ls<gis_typeset::Geographic>());
  gc_geo.push_back(base_py<gis_typeset::Geographic>());
  coverage_transform(wgs84, webmerc3857, gc_geo);
}

}  // namespace gis_transform_unittest

// Copyright (c) 2021, 2024, Oracle and/or its affiliates.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License, version 2.0,
//  as published by the Free Software Foundation.
//
//  This program is designed to work with certain software (including
//  but not limited to OpenSSL) that is licensed under separate terms,
//  as designated in a particular file or component or in included license
//  documentation.  The authors of MySQL hereby grant you an additional
//  permission to link the program and your derivative works with the
//  separately licensed software that they have either included with
//  the program or referenced in the documentation.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License, version 2.0, for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include <gtest/gtest.h>
#include <memory>

#include "sql/gis/buffer.h"             // gis::buffer
#include "sql/gis/buffer_strategies.h"  // gis::BufferStrategies
#include "sql/gis/geometries.h"         // gis::{Coordinate_system, Geometry}
#include "sql/gis/geometries_cs.h"      // gis::{geometry types}
#include "unittest/gunit/gis_srs.h"     // gis_srs::swapped_epsg4326

namespace gis_buffer_unittest {

// Not testing the internal points of geometries as Boost has its own unittests

// Explicit testing because (1) not all strategy combinations are valid for all
// geometry types, and (2) the return type may be a Polygon, Multipolygon, or
// Geometrycollection depending on the input.

// The buffer of a multi-geometry may be merged into a single Polygon if the
// geometries are close enough or buffer distance is large enough. It may
// become an empty Geometrycollection if distance is so negative that buffers
// are shrunk to disappearance.

template <typename T>
void test_valid_input(const gis::Geometry &g,
                      const gis::BufferStrategies &strats, const T &expected_g,
                      const int &expected_size) {
  auto m_srs =
      (expected_g.coordinate_system() == gis::Coordinate_system::kGeographic)
          ? gis_srs::swapped_epsg4326()
          : nullptr;
  std::unique_ptr<gis::Geometry> result;
  bool buffer_error = gis::buffer(m_srs.get(), g, strats, "unittest", &result);

  // If true an exception has been thrown.
  ASSERT_FALSE(buffer_error);
  // Verify returned geometry is of correct type
  ASSERT_EQ(result->type(), expected_g.type());
  // Verify returned geometry has correct size
  EXPECT_EQ(down_cast<T *>(result.get())->size(), expected_size);
}

// Function used to create linearrings for Polygons
gis::Cartesian_linearring linearringFromVector(std::vector<double> data) {
  if (data.size() % 2 != 0) {
    throw std::exception(); /* purecov: dead code */
  }
  gis::Cartesian_linearring lr;
  for (size_t i = 0; i + 1 < data.size(); i += 2) {
    lr.push_back(gis::Cartesian_point(data[i], data[i + 1]));
  }
  return lr;
}

void reset_strategy_combinations(gis::BufferStrategies &s) {
  s.combination = 0;
  s.join_is_set = false;
  s.end_is_set = false;
  s.point_is_set = false;
}

class BufferTest : public ::testing::Test {
 public:
  gis::BufferStrategies strat;
  gis::Cartesian_polygon expected_py;
  gis::Cartesian_multipolygon expected_mpy;
  gis::Cartesian_geometrycollection expected_gc;
};

//////////////////////////////////////////////////////////////////////////////

// CARTESIAN GEOMETRIES (all)

TEST_F(BufferTest, CartesianPoint) {
  gis::Cartesian_point c_pt{0, 0};
  // Distance cannot be negative --> Only possible valid return type is Polygon
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_pt, strat, expected_py, 1);
}

TEST_F(BufferTest, CartesianLinestring) {
  gis::Cartesian_linestring c_ls;
  c_ls.push_back(gis::Cartesian_point(0, 0));
  c_ls.push_back(gis::Cartesian_point(1, 1));
  c_ls.push_back(gis::Cartesian_point(2, 0));

  // Distance cannot be negative --> Only Polygon
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_ls, strat, expected_py, 1);
}

TEST_F(BufferTest, CartesianPolygon) {
  gis::Cartesian_polygon c_py;
  c_py.push_back(linearringFromVector({0, 0, 4, 0, 4, 4, 0, 4, 0, 0}));

  // Distance CAN be negative --> May return Polygon or empty GC
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_py, strat, expected_py, 1);
  strat.distance = -3;
  test_valid_input<gis::Cartesian_geometrycollection>(c_py, strat, expected_gc,
                                                      0);
}

TEST_F(BufferTest, CartesianMultipoint) {
  gis::Cartesian_multipoint c_mpt;
  c_mpt.push_back(gis::Cartesian_point(0, 0));
  c_mpt.push_back(gis::Cartesian_point(1, 1));
  c_mpt.push_back(gis::Cartesian_point(5, 5));

  // Distance cannot be negative for multipoint
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_mpt, strat, expected_py, 1);
  strat.distance = 1;
  test_valid_input<gis::Cartesian_multipolygon>(c_mpt, strat, expected_mpy, 2);
}

TEST_F(BufferTest, CartesianMultilinestring) {
  gis::Cartesian_linestring ls1;
  ls1.push_back(gis::Cartesian_point(0, 0));
  ls1.push_back(gis::Cartesian_point(1, 1));
  ls1.push_back(gis::Cartesian_point(2, 0));

  gis::Cartesian_linestring ls2;
  ls2.push_back(gis::Cartesian_point(0, 4));
  ls2.push_back(gis::Cartesian_point(1, 5));
  ls2.push_back(gis::Cartesian_point(2, 4));

  gis::Cartesian_multilinestring c_mls;
  c_mls.push_back(ls1);
  c_mls.push_back(ls2);

  // Distance cannot be negative --> Only Polygon or Multipolygon
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_mls, strat, expected_py, 1);
  strat.distance = 1;
  test_valid_input<gis::Cartesian_multipolygon>(c_mls, strat, expected_mpy, 2);
}

TEST_F(BufferTest, CartesianMultipolygon) {
  gis::Cartesian_polygon py1;
  py1.push_back(linearringFromVector({0, 0, 4, 0, 4, 4, 0, 4, 0, 0}));

  gis::Cartesian_polygon py2;
  py2.push_back(linearringFromVector({8, 0, 9, 0, 9, 1, 8, 1, 8, 0}));

  gis::Cartesian_multipolygon c_mpy;
  c_mpy.push_back(py1);
  c_mpy.push_back(py2);

  // Distance CAN be negative: May return Polygon, Multipolygon or empty GC
  strat.distance = 3;
  test_valid_input<gis::Cartesian_polygon>(c_mpy, strat, expected_py, 1);
  strat.distance = 1;
  test_valid_input<gis::Cartesian_multipolygon>(c_mpy, strat, expected_mpy, 2);
  strat.distance = -1;
  test_valid_input<gis::Cartesian_polygon>(c_mpy, strat, expected_py, 1);
  strat.distance = -3;
  test_valid_input<gis::Cartesian_geometrycollection>(c_mpy, strat, expected_gc,
                                                      0);
}

TEST_F(BufferTest, CartesianGeometrycollectionPos) {
  gis::Cartesian_linestring ls1;
  ls1.push_back(gis::Cartesian_point(0, 0));
  ls1.push_back(gis::Cartesian_point(1, 1));
  ls1.push_back(gis::Cartesian_point(2, 0));

  gis::Cartesian_polygon py2;
  py2.push_back(linearringFromVector({8, 0, 9, 0, 9, 1, 8, 1, 8, 0}));

  gis::Cartesian_geometrycollection c_gc;
  c_gc.push_back(gis::Cartesian_point(0, 2));
  c_gc.push_back(ls1);
  c_gc.push_back(py2);

  // Distance CAN be negative --> May return Polygon, Multipolygon or empty GC
  strat.distance = 3.1;
  test_valid_input<gis::Cartesian_polygon>(c_gc, strat, expected_py, 1);
  strat.distance = 1;
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);
}

TEST_F(BufferTest, CartesianGeometrycollectionNeg) {
  gis::Cartesian_polygon py1;
  py1.push_back(linearringFromVector({0, 0, 4, 0, 4, 4, 0, 4, 0, 0}));

  gis::Cartesian_polygon py2;
  py1.push_back(linearringFromVector({8, 0, 9, 0, 9, 1, 8, 1, 8, 0}));

  gis::Cartesian_polygon py3;
  py1.push_back(
      linearringFromVector({10, 0, 10.5, 0, 10.5, 0.5, 10, 0.5, 10, 0}));

  gis::Cartesian_multipolygon mpy1;
  mpy1.push_back(py1);
  mpy1.push_back(py2);

  gis::Cartesian_geometrycollection c_gc;
  c_gc.push_back(mpy1);
  c_gc.push_back(py3);

  // Distance CAN be negative --> May return Polygon, Multipolygon or empty GC
  strat.distance = -0.3;
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);
  strat.distance = -0.6;
  test_valid_input<gis::Cartesian_polygon>(c_gc, strat, expected_py, 1);
  strat.distance = -3;
  test_valid_input<gis::Cartesian_geometrycollection>(c_gc, strat, expected_gc,
                                                      0);
}

//////////////////////////////////////////////////////////////////////////////

// GEOGRAPHIC GEOMETRIES (only Point)

TEST_F(BufferTest, GeographicPoint) {
  gis::Geographic_polygon g_py;
  strat.distance = 20000;

  gis::Geographic_point g_pt{63.4451715, 10.9052167};

  // Strategy options cannot be set.
  // Distance cannot be negative --> Only possible valid return type is polygon
  test_valid_input<gis::Geographic_polygon>(g_pt, strat, g_py, 1);
}

//////////////////////////////////////////////////////////////////////////////

// BUFFER STRATEGIES

TEST_F(BufferTest, AllStrategies) {
  gis::Cartesian_linestring ls1;
  ls1.push_back(gis::Cartesian_point(0, 0));
  ls1.push_back(gis::Cartesian_point(1, 1));
  ls1.push_back(gis::Cartesian_point(2, 0));

  gis::Cartesian_polygon py2;
  py2.push_back(linearringFromVector({8, 0, 9, 0, 9, 1, 8, 1, 8, 0}));

  gis::Cartesian_geometrycollection c_gc;
  c_gc.push_back(gis::Cartesian_point(0, 2));
  c_gc.push_back(ls1);
  c_gc.push_back(py2);

  // Testing all 8 possible strategy combinations:
  strat.distance = 0.6;

  // 0
  reset_strategy_combinations(strat);
  strat.set_join_round(22);
  strat.set_end_round(22);
  strat.set_point_circle(22);
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 3);

  // 1
  reset_strategy_combinations(strat);
  strat.set_join_round(22);
  strat.set_end_flat();
  strat.set_point_circle(22);
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 3);

  // 2
  reset_strategy_combinations(strat);
  strat.set_join_miter(4.0);
  strat.set_end_round(22);
  strat.set_point_circle(22);
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 3);

  // 3
  reset_strategy_combinations(strat);
  strat.set_join_miter(4.0);
  strat.set_end_flat();
  strat.set_point_circle(22);
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 3);

  // 4
  reset_strategy_combinations(strat);
  strat.set_join_round(22);
  strat.set_end_round(22);
  strat.set_point_square();
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);

  // 5
  reset_strategy_combinations(strat);
  strat.set_join_round(22);
  strat.set_end_flat();
  strat.set_point_square();
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);

  // 6
  reset_strategy_combinations(strat);
  strat.set_join_miter(4.0);
  strat.set_end_round(22);
  strat.set_point_square();
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);

  // 7
  reset_strategy_combinations(strat);
  strat.set_join_miter(4.0);
  strat.set_end_flat();
  strat.set_point_square();
  test_valid_input<gis::Cartesian_multipolygon>(c_gc, strat, expected_mpy, 2);
}

TEST_F(BufferTest, PointStrategies) {
  // Showcasing that different strategies (for equal buffer distance) may yield
  // different results wrt. numbers of buffers returned from multi-geometries.

  gis::Cartesian_multipoint c_mpt;
  c_mpt.push_back(gis::Cartesian_point(0, 0));
  c_mpt.push_back(gis::Cartesian_point(1, 1));

  // At 0.70 circle(42) does NOT overlap, while at 0.71 it does
  // At 0.50 square() does NOT overlap, while at 0.51 it does
  strat.distance = 0.60;

  strat.set_point_circle(42);
  test_valid_input<gis::Cartesian_multipolygon>(c_mpt, strat, expected_mpy, 2);

  strat.point_is_set = false;
  strat.set_point_square();
  test_valid_input<gis::Cartesian_polygon>(c_mpt, strat, expected_py, 1);
}

}  // namespace gis_buffer_unittest

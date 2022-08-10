/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>
#include <memory>   // unique_ptr
#include <ostream>  // <<

#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/relops.h"
#include "sql/gis/setops.h"
#include "template_utils.h"  // down_cast
#include "unittest/gunit/gis_setops_testshapes.h"
#include "unittest/gunit/gis_test.h"
#include "unittest/gunit/gis_typeset.h"

namespace gis_intersectionunittest {

bool areEquals(const dd::Spatial_reference_system_impl *srs,
               const gis::Geometry &expected_result,
               const gis::Geometry &actual_result) {
  bool is_equals = false;
  bool is_null = false;
  bool equals_error = gis::equals(srs, &expected_result, &actual_result,
                                  "unittest", &is_equals, &is_null);
  EXPECT_FALSE(equals_error);
  EXPECT_FALSE(is_null);
  return is_equals;
}

template <typename Types>
struct IntersectionTest : Gis_test<Types> {
  void test_valid_input(const gis::Geometry &g1, const gis::Geometry &g2,
                        gis::Geometry &expected_result) {
    std::unique_ptr<gis::Geometry> result;
    bool is_null = false;
    bool error =
        gis::intersection(this->m_srs.get(), &g1, &g2, "unittest", &result);
    EXPECT_FALSE(error);
    EXPECT_FALSE(is_null);

    // Verify geometry return type.
    const std::string type_res(gis::type_to_name(result->type()));
    const std::string type_exp(gis::type_to_name(expected_result.type()));
    EXPECT_EQ(type_exp, type_res);

    // Verify result is correct.
    EXPECT_PRED3(areEquals, this->m_srs.get(), expected_result, *result.get());
  }
};

// The purpose of these tests is to check that the result returned from
// gis::intersection is correct. The tests test all combinations of geometries.

TYPED_TEST_SUITE(IntersectionTest, gis_typeset::Test_both);

// intersection(... ,point, *, ...)

TYPED_TEST(IntersectionTest, PointPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, pt2, empty_gc);
  this->test_valid_input(pt1, pt1, pt1);
}

TYPED_TEST(IntersectionTest, PointMultipoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt2, mpt, empty_gc);
  this->test_valid_input(mpt, pt1, pt1);
}

TYPED_TEST(IntersectionTest, PointLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt2, ls, empty_gc);
  this->test_valid_input(pt1, ls, pt1);
  this->test_valid_input(ls, pt1, pt1);
}

TYPED_TEST(IntersectionTest, PointMultiLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt2, mls, empty_gc);
  this->test_valid_input(pt1, mls, pt1);
  this->test_valid_input(mls, pt1, pt1);
}

TYPED_TEST(IntersectionTest, PointPolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt2, py, empty_gc);
  this->test_valid_input(pt1, py, pt1);
  this->test_valid_input(py, pt1, pt1);
}

TYPED_TEST(IntersectionTest, PointMultipolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt2, mpy, empty_gc);
  this->test_valid_input(pt1, mpy, pt1);
  this->test_valid_input(mpy, pt1, pt1);
}

// intersection(..., multipoint, *, ...)

TYPED_TEST(IntersectionTest, MultipointMultipoint) {
  typename TypeParam::Multipoint mpt1 = simple_mpt<TypeParam>();
  typename TypeParam::Multipoint mpt2 = simple_mpt<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.0, 0.1};

  mpt1.push_back(pt2);
  mpt2.push_back(typename TypeParam::Point{0.1, 0.1});

  this->test_valid_input(mpt1, mpt1, mpt1);
  this->test_valid_input(mpt1, mpt2, mpt1[0]);
}

TYPED_TEST(IntersectionTest, MultipointLinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();

  mpt.push_back(typename TypeParam::Point(0.1, 0.1));

  this->test_valid_input(mpt, ls, mpt[0]);
  this->test_valid_input(ls, mpt, mpt[0]);
}

TYPED_TEST(IntersectionTest, MultipointMultilinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();

  mpt.push_back(typename TypeParam::Point(0.1, 0.1));

  this->test_valid_input(mpt, mls, mpt[0]);
  this->test_valid_input(mls, mpt, mpt[0]);
}

TYPED_TEST(IntersectionTest, MultipointPolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();

  mpt.push_back(typename TypeParam::Point(0.0, 0.2));

  this->test_valid_input(mpt, py, mpt[0]);
  this->test_valid_input(py, mpt, mpt[0]);
}

TYPED_TEST(IntersectionTest, MultipointMultipolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();

  mpt.push_back(typename TypeParam::Point(0.1, 0.2));

  this->test_valid_input(mpt, mpy, mpt[0]);
  this->test_valid_input(mpy, mpt, mpt[0]);
}

// intersection(..., linestring, *, ...)

TYPED_TEST(IntersectionTest, LinestringLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Linestring expected_result{};

  expected_result.push_back(typename TypeParam::Point(0.05, 0.0));
  expected_result.push_back(typename TypeParam::Point(0.1, 0.0));

  this->test_valid_input(ls1, ls1, ls1);
  this->test_valid_input(ls1, ls2, expected_result);
}

TYPED_TEST(IntersectionTest, LinestringMultilinestring) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Linestring expected_result{};

  expected_result.push_back(typename TypeParam::Point(0.05, 0.0));
  expected_result.push_back(typename TypeParam::Point(0.1, 0.0));

  this->test_valid_input(mls[0], mls, mls[0]);
  this->test_valid_input(ls2, mls, expected_result);
}

TYPED_TEST(IntersectionTest, LinestringPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();

  this->test_valid_input(ls1, py, ls1);
  this->test_valid_input(py, ls1, ls1);
}

TYPED_TEST(IntersectionTest, LinestringMultiPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();

  this->test_valid_input(ls1, mpy, ls1);
  this->test_valid_input(mpy, ls1, ls1);
}

// intersection(..., multilinestring, *, ...)

TYPED_TEST(IntersectionTest, MultilinestringMultilinestring) {
  typename TypeParam::Multilinestring mls1 = simple_mls<TypeParam>();
  typename TypeParam::Multilinestring mls2{};
  typename TypeParam::Linestring expected_result{};

  mls2.push_back(offset_simple_ls<TypeParam>());
  expected_result.push_back(typename TypeParam::Point{0.05, 0.0});
  expected_result.push_back(typename TypeParam::Point{0.1, 0.0});

  this->test_valid_input(mls1, mls1, mls1[0]);
  this->test_valid_input(mls1, mls2, expected_result);
}

TYPED_TEST(IntersectionTest, MultilinestringPolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();

  // mls.push_back(ls_overlapping_base_py<TypeParam>());

  this->test_valid_input(mls, py, mls[0]);
  this->test_valid_input(py, mls, mls[0]);
}

TYPED_TEST(IntersectionTest, MultilinestringMultipolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();

  //  mls.push_back(ls_overlapping_base_py<TypeParam>());

  this->test_valid_input(mls, mpy, mls[0]);
  this->test_valid_input(mpy, mls, mls[0]);
}

// intersection(..., polygon, *, ...)

TYPED_TEST(IntersectionTest, PolygonPolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Linestring ls = vertical_ls<TypeParam>();

  this->test_valid_input(py1, py1, py1);
  this->test_valid_input(py1, py2, ls);
}

TYPED_TEST(IntersectionTest, PolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Multipolygon mpy{};
  typename TypeParam::Linestring ls = vertical_ls<TypeParam>();

  mpy.push_back(py2);

  this->test_valid_input(py2, mpy, py2);
  this->test_valid_input(mpy, py1, ls);
}

// intersection(..., multipolygon, *, ...)

TYPED_TEST(IntersectionTest, MultipolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Multipolygon mpy1 = simple_mpy<TypeParam>();
  typename TypeParam::Multipolygon mpy2{};
  typename TypeParam::Linestring ls = vertical_ls<TypeParam>();

  mpy2.push_back(py2);
  mpy2.push_back(py3);

  this->test_valid_input(mpy1, mpy1, mpy1[0]);
  this->test_valid_input(mpy1, mpy2, ls);
}

// intersection(..., geometrycollection, *, ...)

TYPED_TEST(IntersectionTest, GeometrycollectionPoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};

  gc.push_back(pt1);

  this->test_valid_input(gc, pt1, pt1);
  this->test_valid_input(pt2, gc, empty_gc);
}

TYPED_TEST(IntersectionTest, GeometrycollectionMultipoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();

  gc.push_back(mpt[0]);
  gc.push_back(typename TypeParam::Point(0.1, 0.1));

  this->test_valid_input(mpt, gc, mpt[0]);
  this->test_valid_input(gc, mpt, mpt[0]);
}

TYPED_TEST(IntersectionTest, GeometrycollectionLinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  auto pt = typename TypeParam::Point(0.0, 0.0);

  gc.push_back(ls1);

  this->test_valid_input(ls1, gc, ls1);
  this->test_valid_input(gc, ls2, pt);
}

TYPED_TEST(IntersectionTest, GeometrycollectionMultilinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  auto pt = typename TypeParam::Point(0.0, 0.0);

  gc.push_back(ls2);

  this->test_valid_input(mls, gc, pt);
  this->test_valid_input(gc, mls, pt);
}

TYPED_TEST(IntersectionTest, GeometrycollectionPolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = disjoint_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(py1);

  this->test_valid_input(py1, gc, py1);
  this->test_valid_input(gc, py2, empty_gc);
}

TYPED_TEST(IntersectionTest, GeometrycollectionMultipolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py2 = disjoint_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(py2);

  this->test_valid_input(mpy, gc, empty_gc);
  this->test_valid_input(gc, mpy, empty_gc);
}

TYPED_TEST(IntersectionTest, GeometrycollectionGeometrycollection) {
  typename TypeParam::Geometrycollection gc1{};
  typename TypeParam::Geometrycollection gc2{};
  typename TypeParam::Geometrycollection gc_result{};
  this->test_valid_input(gc1, gc1, gc1);

  typename TypeParam::Point pt{0, 0};
  typename TypeParam::Linestring ls = ls_overlapping_base_py<TypeParam>();
  typename TypeParam::Polygon py = overlapping_py<TypeParam>();
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  gc1.push_back(pt);
  gc1.push_back(ls);
  gc1.push_back(py);
  gc1.push_back(mpt);
  gc1.push_back(mls);
  gc1.push_back(mpy);
  gc1.push_back(gc1);

  gc2.push_back(mpy);
  gc2.push_back(pt);

  gc_result.push_back(pt);
  gc_result.push_back(mpy);

  this->test_valid_input(gc1, gc2, gc_result);
}

}  // namespace gis_intersectionunittest

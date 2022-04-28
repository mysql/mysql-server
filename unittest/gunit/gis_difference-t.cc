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

namespace gis_differenceunittest {

template <typename Types>
struct DifferenceTest : Gis_test<Types> {
  void test_valid_input(const gis::Geometry &g1, const gis::Geometry &g2,
                        gis::Geometry &expected_result) {
    std::unique_ptr<gis::Geometry> result;
    bool is_null = false;
    bool error =
        gis::difference(this->m_srs.get(), &g1, &g2, "unittest", &result);
    EXPECT_FALSE(error);
    EXPECT_FALSE(is_null);

    // Verify geometry return type.
    const std::string type_res(gis::type_to_name(result->type()));
    const std::string type_exp(gis::type_to_name(expected_result.type()));
    EXPECT_EQ(type_exp, type_res);

    // Verify result is correct.
    bool is_equals = false;
    bool equals_error =
        gis::equals(this->m_srs.get(), &expected_result, result.get(),
                    "unittest", &is_equals, &is_null);
    EXPECT_FALSE(equals_error);
    EXPECT_FALSE(is_null);
    EXPECT_TRUE(is_equals);
  }
};

// The purpose of these tests is to check that the result returned from
// gis::difference is correct. The tests test all combinations of geometries.
TYPED_TEST_SUITE(DifferenceTest, gis_typeset::Test_both);

// difference(... ,point, *, ...)

TYPED_TEST(DifferenceTest, PointPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, pt1, empty_gc);
  this->test_valid_input(pt1, pt2, pt1);
}

TYPED_TEST(DifferenceTest, PointMultipoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  mpt.push_back(pt2);

  this->test_valid_input(pt1, mpt, empty_gc);
  this->test_valid_input(mpt, pt2, pt1);
}

TYPED_TEST(DifferenceTest, PointLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, ls, empty_gc);
  this->test_valid_input(pt2, ls, pt2);
  this->test_valid_input(ls, pt1, ls);
}

TYPED_TEST(DifferenceTest, PointMultiLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, mls, empty_gc);
  this->test_valid_input(pt2, mls, pt2);
  this->test_valid_input(mls, pt1, mls[0]);
}

TYPED_TEST(DifferenceTest, PointPolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, py, empty_gc);
  this->test_valid_input(pt2, py, pt2);
  this->test_valid_input(py, pt1, py);
}

TYPED_TEST(DifferenceTest, PointMultipolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(pt1, mpy, empty_gc);
  this->test_valid_input(pt2, mpy, pt2);
  this->test_valid_input(mpy, pt1, mpy[0]);
}

// difference(..., multipoint, *, ...)

TYPED_TEST(DifferenceTest, MultipointMultipoint) {
  typename TypeParam::Multipoint mpt1 = simple_mpt<TypeParam>();
  typename TypeParam::Multipoint mpt2 = simple_mpt<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.0, 0.1};

  mpt1.push_back(pt2);
  mpt2.push_back(typename TypeParam::Point{0.1, 0.1});

  this->test_valid_input(mpt1, mpt1, empty_gc);
  this->test_valid_input(mpt1, mpt2, pt2);
}

TYPED_TEST(DifferenceTest, MultipointLinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.1, 0.1};

  this->test_valid_input(mpt, ls, empty_gc);

  mpt.push_back(pt2);

  this->test_valid_input(mpt, ls, pt2);
  this->test_valid_input(ls, mpt, ls);
}

TYPED_TEST(DifferenceTest, MultipointMultilinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.1, 0.1};

  this->test_valid_input(mpt, mls, empty_gc);

  mpt.push_back(pt2);

  this->test_valid_input(mpt, mls, pt2);
  this->test_valid_input(mls, mpt, mls[0]);
}

TYPED_TEST(DifferenceTest, MultipointPolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.0, 0.2};

  this->test_valid_input(mpt, py, empty_gc);

  mpt.push_back(pt2);

  this->test_valid_input(mpt, py, pt2);
  this->test_valid_input(py, mpt, py);
}

TYPED_TEST(DifferenceTest, MultipointMultipolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.0, 0.2};

  this->test_valid_input(mpt, mpy, empty_gc);

  mpt.push_back(pt2);

  this->test_valid_input(mpt, mpy, pt2);
  this->test_valid_input(mpy, mpt, mpy[0]);
}

// difference(..., linestring, *, ...)

TYPED_TEST(DifferenceTest, LinestringLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Linestring expected_result{};

  expected_result.push_back(typename TypeParam::Point(0.0, 0.0));
  expected_result.push_back(typename TypeParam::Point(0.05, 0.0));

  this->test_valid_input(ls1, ls1, empty_gc);
  this->test_valid_input(ls1, ls2, expected_result);
}

TYPED_TEST(DifferenceTest, LinestringMultilinestring) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Linestring expected_result{};

  expected_result.push_back(typename TypeParam::Point(0.0, 0.0));
  expected_result.push_back(typename TypeParam::Point(0.05, 0.0));

  this->test_valid_input(mls[0], mls, empty_gc);
  this->test_valid_input(mls, ls2, expected_result);
}

TYPED_TEST(DifferenceTest, LinestringPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = ls_crossing_base_py<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Multilinestring expected_result =
      ls_crossing_base_py_difference<TypeParam>();

  this->test_valid_input(ls1, py, empty_gc);
  this->test_valid_input(ls2, py, expected_result);
  this->test_valid_input(py, ls1, py);
}

TYPED_TEST(DifferenceTest, LinestringMultiPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = ls_crossing_base_py<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Multilinestring expected_result =
      ls_crossing_base_py_difference<TypeParam>();

  this->test_valid_input(ls1, mpy, empty_gc);
  this->test_valid_input(ls2, mpy, expected_result);
  this->test_valid_input(mpy, ls1, mpy[0]);
}

// difference(..., multilinestring, *, ...)

TYPED_TEST(DifferenceTest, MultilinestringMultilinestring) {
  typename TypeParam::Multilinestring mls1 = simple_mls<TypeParam>();
  typename TypeParam::Multilinestring mls2{};
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Linestring expected_result{};

  mls2.push_back(offset_simple_ls<TypeParam>());
  expected_result.push_back(typename TypeParam::Point{0.0, 0.0});
  expected_result.push_back(typename TypeParam::Point{0.05, 0.0});

  this->test_valid_input(mls1, mls1, empty_gc);
  this->test_valid_input(mls1, mls2, expected_result);
}

TYPED_TEST(DifferenceTest, MultilinestringPolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Linestring expected_result =
      ls_overlapping_base_py_difference<TypeParam>();

  this->test_valid_input(mls, py, empty_gc);

  mls.push_back(ls_overlapping_base_py<TypeParam>());

  this->test_valid_input(mls, py, expected_result);
  this->test_valid_input(py, mls, py);
}

TYPED_TEST(DifferenceTest, MultilinestringMultipolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Linestring expected_result =
      ls_overlapping_base_py_difference<TypeParam>();

  this->test_valid_input(mls, mpy, empty_gc);

  mls.push_back(ls_overlapping_base_py<TypeParam>());

  this->test_valid_input(mls, mpy, expected_result);
  this->test_valid_input(mpy, mls, mpy[0]);
}

// difference(..., polygon, *, ...)

TYPED_TEST(DifferenceTest, PolygonPolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  this->test_valid_input(py1, py1, empty_gc);
  this->test_valid_input(py1, py2, py1);
}

TYPED_TEST(DifferenceTest, PolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Multipolygon mpy{};
  typename TypeParam::Geometrycollection empty_gc{};

  mpy.push_back(py2);
  mpy.push_back(py3);

  this->test_valid_input(py2, mpy, empty_gc);
  this->test_valid_input(py1, mpy, py1);
  this->test_valid_input(mpy, py2, py3);
}

// difference(..., multipolygon, *, ...)

TYPED_TEST(DifferenceTest, MultipolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Multipolygon mpy1 = simple_mpy<TypeParam>();
  typename TypeParam::Multipolygon mpy2{};
  typename TypeParam::Geometrycollection empty_gc{};

  mpy2.push_back(py2);
  mpy2.push_back(py3);

  this->test_valid_input(mpy1, mpy1, empty_gc);
  this->test_valid_input(mpy1, mpy2, py1);
}

// difference(..., geometrycollection, *, ...)

TYPED_TEST(DifferenceTest, GeometrycollectionPoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};

  gc.push_back(pt1);

  this->test_valid_input(gc, pt1, empty_gc);
  this->test_valid_input(pt2, gc, pt2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionMultipoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};
  typename TypeParam::Point pt2{0.1, 0.1};

  gc.push_back(mpt[0]);
  this->test_valid_input(mpt, gc, empty_gc);

  gc.push_back(pt2);
  this->test_valid_input(gc, mpt, pt2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionLinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(ls1);
  this->test_valid_input(ls1, gc, empty_gc);

  gc.push_back(ls2);
  this->test_valid_input(gc, ls1, ls2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionMultilinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(mls[0]);
  this->test_valid_input(mls, gc, empty_gc);

  gc.push_back(ls2);
  this->test_valid_input(gc, mls, ls2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionPolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = disjoint_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(py1);
  this->test_valid_input(py1, gc, empty_gc);

  gc.push_back(py2);
  this->test_valid_input(gc, py1, py2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionMultipolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py2 = disjoint_py<TypeParam>();
  typename TypeParam::Geometrycollection empty_gc{};

  gc.push_back(mpy);
  this->test_valid_input(mpy, gc, empty_gc);

  gc.push_back(py2);
  this->test_valid_input(gc, mpy, py2);
}

TYPED_TEST(DifferenceTest, GeometrycollectionGeometrycollection) {
  typename TypeParam::Geometrycollection gc1{};
  typename TypeParam::Geometrycollection gc2{};
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

  gc2.push_back(py);
  gc2.push_back(pt);

  typename TypeParam::Geometrycollection expected_result{};
  expected_result.push_back(mpy[0]);
  expected_result.push_back(ls_overlapping_base_py_difference<TypeParam>());

  this->test_valid_input(gc1, gc2, expected_result);
}

}  // namespace gis_differenceunittest

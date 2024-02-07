/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

namespace gis_union_unittest {

template <typename Types>
struct UnionTest : Gis_test<Types> {
  void test_valid_input(const gis::Geometry &g1, const gis::Geometry &g2,
                        gis::Geometry &expected_result) {
    std::unique_ptr<gis::Geometry> result;
    bool is_null = false;
    bool error =
        gis::union_(this->m_srs.get(), &g1, &g2, "unittest", &result, &is_null);
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
// gis::union_ is correct. The tests test all combinations of geometries.
TYPED_TEST_SUITE(UnionTest, gis_typeset::Test_both);

// union_(... ,point, *, ...)

TYPED_TEST(UnionTest, PointPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Multipoint expected_result{};

  expected_result.push_back(pt1);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, pt1, pt1);
  this->test_valid_input(pt1, pt2, expected_result);
}

TYPED_TEST(UnionTest, PointMultipoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Point pt3{0.1, 0.1};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multipoint expected_result{};

  mpt.push_back(pt2);
  expected_result.push_back(pt1);
  expected_result.push_back(pt2);
  expected_result.push_back(pt3);

  this->test_valid_input(pt3, mpt, expected_result);
  this->test_valid_input(mpt, pt3, expected_result);
}

TYPED_TEST(UnionTest, PointLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  expected_result.push_back(ls);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, ls, ls);
  this->test_valid_input(ls, pt1, ls);
  this->test_valid_input(pt2, ls, expected_result);
  this->test_valid_input(ls, pt2, expected_result);
}

TYPED_TEST(UnionTest, PointMultiLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  expected_result.push_back(mls[0]);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, mls, mls[0]);
  this->test_valid_input(mls, pt1, mls[0]);
  this->test_valid_input(pt2, mls, expected_result);
  this->test_valid_input(mls, pt2, expected_result);
}

TYPED_TEST(UnionTest, PointPolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  expected_result.push_back(py);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, py, py);
  this->test_valid_input(py, pt1, py);
  this->test_valid_input(pt2, py, expected_result);
  this->test_valid_input(py, pt2, expected_result);
}

TYPED_TEST(UnionTest, PointMultipolygon) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.2};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  expected_result.push_back(mpy[0]);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, mpy, mpy[0]);
  this->test_valid_input(mpy, pt1, mpy[0]);
  this->test_valid_input(pt2, mpy, expected_result);
  this->test_valid_input(mpy, pt2, expected_result);
}

// union_(..., multipoint, *, ...)

TYPED_TEST(UnionTest, MultipointMultipoint) {
  typename TypeParam::Multipoint mpt1 = simple_mpt<TypeParam>();
  typename TypeParam::Multipoint mpt2 = simple_mpt<TypeParam>();
  typename TypeParam::Multipoint expected_result = simple_mpt<TypeParam>();

  mpt1.push_back(typename TypeParam::Point{0.0, 0.1});
  mpt2.push_back(typename TypeParam::Point{0.1, 0.1});
  expected_result.push_back(typename TypeParam::Point{0.0, 0.1});
  expected_result.push_back(typename TypeParam::Point{0.1, 0.1});

  this->test_valid_input(mpt1, mpt2, expected_result);
}

TYPED_TEST(UnionTest, MultipointLinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  this->test_valid_input(ls, mpt, ls);
  this->test_valid_input(mpt, ls, ls);

  mpt.push_back(typename TypeParam::Point{0.1, 0.1});
  expected_result.push_back(ls);
  expected_result.push_back(typename TypeParam::Point{0.1, 0.1});

  this->test_valid_input(mpt, ls, expected_result);
  this->test_valid_input(ls, mpt, expected_result);
}

TYPED_TEST(UnionTest, MultipointMultilinestring) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  mpt.push_back(typename TypeParam::Point{0.1, 0.1});
  expected_result.push_back(mls[0]);
  expected_result.push_back(typename TypeParam::Point{0.1, 0.1});

  this->test_valid_input(mls, mpt, expected_result);
  this->test_valid_input(mpt, mls, expected_result);
}

TYPED_TEST(UnionTest, MultipointPolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  mpt.push_back(typename TypeParam::Point{0.0, 0.2});
  expected_result.push_back(py);
  expected_result.push_back(typename TypeParam::Point{0.0, 0.2});

  this->test_valid_input(mpt, py, expected_result);
  this->test_valid_input(py, mpt, expected_result);
}

TYPED_TEST(UnionTest, MultipointMultipolygon) {
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  mpt.push_back(typename TypeParam::Point{0.0, 0.2});
  expected_result.push_back(mpy[0]);
  expected_result.push_back(typename TypeParam::Point{0.0, 0.2});

  this->test_valid_input(mpt, mpy, expected_result);
  this->test_valid_input(mpy, mpt, expected_result);
}

// union_(..., linestring, *, ...)

TYPED_TEST(UnionTest, LinestringLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  typename TypeParam::Multilinestring expected_result{};

  expected_result.push_back(ls1);
  expected_result.push_back(ls2);

  this->test_valid_input(ls1, ls1, ls1);
  this->test_valid_input(ls1, ls2, expected_result);
}

TYPED_TEST(UnionTest, LinestringMultilinestring) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Linestring ls2 = diagonal_ls<TypeParam>();
  typename TypeParam::Multilinestring expected_result{};

  this->test_valid_input(mls[0], mls, mls[0]);
  this->test_valid_input(mls, mls[0], mls[0]);

  mls.push_back(ls2);
  expected_result.push_back(mls[0]);
  expected_result.push_back(ls2);

  this->test_valid_input(ls2, mls, expected_result);
  this->test_valid_input(mls, ls2, expected_result);
}

TYPED_TEST(UnionTest, LinestringPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = ls_crossing_base_py<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};
  typename TypeParam::Multilinestring ls_result =
      ls_crossing_base_py_difference<TypeParam>();

  expected_result.push_back(py);
  for (typename TypeParam::Linestring ls : ls_result)
    expected_result.push_back(ls);

  this->test_valid_input(ls1, py, py);
  this->test_valid_input(py, ls1, py);
  this->test_valid_input(ls2, py, expected_result);
  this->test_valid_input(py, ls2, expected_result);
}

TYPED_TEST(UnionTest, LinestringMultiPolygon) {
  typename TypeParam::Linestring ls1 = diagonal_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = ls_overlapping_base_py<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  expected_result.push_back(mpy[0]);
  expected_result.push_back(ls_overlapping_base_py_difference<TypeParam>());

  this->test_valid_input(ls1, mpy, mpy[0]);
  this->test_valid_input(mpy, ls1, mpy[0]);
  this->test_valid_input(ls2, mpy, expected_result);
  this->test_valid_input(mpy, ls2, expected_result);
}

// union_(..., multilinestring, *, ...)

TYPED_TEST(UnionTest, MultilinestringMultilinestring) {
  typename TypeParam::Multilinestring mls1 = simple_mls<TypeParam>();
  typename TypeParam::Multilinestring mls2 = simple_mls<TypeParam>();
  typename TypeParam::Multilinestring expected_result{};
  typename TypeParam::Linestring expected_result_ls{};

  mls2.push_back(offset_simple_ls<TypeParam>());
  mls1.push_back(diagonal_ls<TypeParam>());
  expected_result_ls.push_back(typename TypeParam::Point{0.0, 0.0});
  expected_result_ls.push_back(typename TypeParam::Point{0.15, 0.0});
  expected_result.push_back(expected_result_ls);
  expected_result.push_back(diagonal_ls<TypeParam>());

  this->test_valid_input(mls1, mls2, expected_result);
}

TYPED_TEST(UnionTest, MultilinestringPolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  mls.push_back(ls_overlapping_base_py<TypeParam>());
  expected_result.push_back(py);
  expected_result.push_back(ls_overlapping_base_py_difference<TypeParam>());

  this->test_valid_input(py, mls, expected_result);
  this->test_valid_input(mls, py, expected_result);
}

TYPED_TEST(UnionTest, MultilinestringMultipolygon) {
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection expected_result{};

  mls.push_back(ls_overlapping_base_py<TypeParam>());
  expected_result.push_back(mpy[0]);
  expected_result.push_back(ls_overlapping_base_py_difference<TypeParam>());

  this->test_valid_input(mpy, mls, expected_result);
  this->test_valid_input(mls, mpy, expected_result);
}

// union_(..., polygon, *, ...)

TYPED_TEST(UnionTest, PolygonPolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Polygon expected_result_py =
      base_union_overlapping_py<TypeParam>();
  typename TypeParam::Multipolygon expected_result_mpy{};

  expected_result_mpy.push_back(py1);
  expected_result_mpy.push_back(py3);

  this->test_valid_input(py1, py2, expected_result_py);
  this->test_valid_input(py1, py3, expected_result_mpy);
}

TYPED_TEST(UnionTest, PolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Multipolygon mpy{};
  typename TypeParam::Multipolygon expected_result{};

  mpy.push_back(py2);
  mpy.push_back(py3);
  expected_result.push_back(base_union_overlapping_py<TypeParam>());
  expected_result.push_back(py3);

  this->test_valid_input(py1, mpy, expected_result);
  this->test_valid_input(mpy, py1, expected_result);
}

// union_(..., multipolygon, *, ...)

TYPED_TEST(UnionTest, MultipolygonMultipolygon) {
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2 = overlapping_py<TypeParam>();
  typename TypeParam::Polygon py3 = disjoint_py<TypeParam>();
  typename TypeParam::Multipolygon mpy1 = simple_mpy<TypeParam>();
  typename TypeParam::Multipolygon mpy2{};
  typename TypeParam::Polygon expected_result_py =
      base_union_overlapping_py<TypeParam>();
  typename TypeParam::Multipolygon expected_result_mpy{};

  mpy2.push_back(py2);
  mpy2.push_back(py3);
  expected_result_mpy.push_back(expected_result_py);
  expected_result_mpy.push_back(py3);

  this->test_valid_input(mpy1, mpy2, expected_result_mpy);
}

// union_(..., geometrycollection, *, ...)

TYPED_TEST(UnionTest, GeometrycollectionPoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Point pt{0, 0};
  this->test_valid_input(gc, pt, pt);
  this->test_valid_input(pt, gc, pt);
}

TYPED_TEST(UnionTest, GeometrycollectionMultipoint) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  this->test_valid_input(gc, mpt, mpt[0]);
  this->test_valid_input(mpt, gc, mpt[0]);
}

TYPED_TEST(UnionTest, GeometrycollectionLinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  this->test_valid_input(gc, ls, ls);
  this->test_valid_input(ls, gc, ls);
}

TYPED_TEST(UnionTest, GeometrycollectionMultilinestring) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  this->test_valid_input(gc, mls, mls[0]);
  this->test_valid_input(mls, gc, mls[0]);
}

TYPED_TEST(UnionTest, GeometrycollectionPolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  this->test_valid_input(gc, py, py);
  this->test_valid_input(py, gc, py);
}

TYPED_TEST(UnionTest, GeometrycollectionMultipolygon) {
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  this->test_valid_input(gc, mpy, mpy[0]);
  this->test_valid_input(mpy, gc, mpy[0]);
}

TYPED_TEST(UnionTest, GeometrycollectionGeometrycollection) {
  typename TypeParam::Geometrycollection gc{};
  this->test_valid_input(gc, gc, gc);

  typename TypeParam::Point pt{0, 0};
  typename TypeParam::Linestring ls = ls_overlapping_base_py<TypeParam>();
  typename TypeParam::Polygon py = overlapping_py<TypeParam>();
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc1{};
  gc1.push_back(typename TypeParam::Point{0.0, 0.5});
  gc.push_back(pt);
  gc.push_back(ls);
  gc.push_back(py);
  gc.push_back(mpt);
  gc.push_back(mls);
  gc.push_back(mpy);
  gc.push_back(gc1);

  typename TypeParam::Geometrycollection expected_result{};
  expected_result.push_back(base_union_overlapping_py<TypeParam>());
  expected_result.push_back(ls_overlapping_base_py_difference<TypeParam>());
  expected_result.push_back(typename TypeParam::Point{0.0, 0.5});

  this->test_valid_input(gc, gc, expected_result);
}

}  // namespace gis_union_unittest

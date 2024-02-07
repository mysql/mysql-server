/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <boost/geometry.hpp>

#ifdef GIS_SYMDIFFERENCE_TEST_DEBUG
namespace gis {
std::ostream &operator<<(std::ostream &os, const Geometry &geometry) {
  if (geometry.coordinate_system() == Coordinate_system::kGeographic) {
    switch (geometry.type()) {
      case Geometry_type::kPoint: {
        Geographic_point pt = *down_cast<const Geographic_point *>(&geometry);
        return os << "POINT(" << pt.x() << "," << pt.y() << ")";
      }
      case Geometry_type::kLinestring: {
        Geographic_linestring ls =
            *down_cast<const Geographic_linestring *>(&geometry);
        os << "LINESTRING: ";
        for (auto pt : ls) {
          os << pt;
        }
        return os;
      }
      case Geometry_type::kPolygon: {
        Geographic_polygon py =
            *down_cast<const Geographic_polygon *>(&geometry);
        Geographic_linearring lr = py.geographic_exterior_ring();
        os << "POLYGON: ";
        for (auto pt : lr) {
          os << pt;
        }
        return os;
      }
      case Geometry_type::kMultipoint: {
        Geographic_multipoint g =
            *down_cast<const Geographic_multipoint *>(&geometry);
        os << "MULTIPOINT: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kMultilinestring: {
        Geographic_multilinestring g =
            *down_cast<const Geographic_multilinestring *>(&geometry);
        os << "MULTILINESTRING: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kMultipolygon: {
        Geographic_multipolygon g =
            *down_cast<const Geographic_multipolygon *>(&geometry);
        os << "MULTIPOLYGON: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kGeometrycollection: {
        Geographic_geometrycollection g =
            *down_cast<const Geographic_geometrycollection *>(&geometry);
        os << "GEOMCOL: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kGeometry:
        break;
    }
  } else if (geometry.coordinate_system() == Coordinate_system::kCartesian) {
    switch (geometry.type()) {
      case Geometry_type::kPoint: {
        Cartesian_point pt = *down_cast<const Cartesian_point *>(&geometry);
        return os << "POINT(" << pt.x() << "," << pt.y() << ")";
      }
      case Geometry_type::kLinestring: {
        Cartesian_linestring ls =
            *down_cast<const Cartesian_linestring *>(&geometry);
        os << "LINESTRING: ";
        for (auto pt : ls) {
          os << pt;
        }
        return os;
      }
      case Geometry_type::kPolygon: {
        Cartesian_polygon py = *down_cast<const Cartesian_polygon *>(&geometry);
        Cartesian_linearring lr = py.cartesian_exterior_ring();
        os << "POLYGON: ";
        for (auto pt : lr) {
          os << pt;
        }
        return os;
      }
      case Geometry_type::kMultipoint: {
        Cartesian_multipoint g =
            *down_cast<const Cartesian_multipoint *>(&geometry);
        os << "MULTIPOINT: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kMultilinestring: {
        Cartesian_multilinestring g =
            *down_cast<const Cartesian_multilinestring *>(&geometry);
        os << "MULTILINESTRING: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kMultipolygon: {
        Cartesian_multipolygon g =
            *down_cast<const Cartesian_multipolygon *>(&geometry);
        os << "MULTIPOLYGON: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kGeometrycollection: {
        Cartesian_geometrycollection g =
            *down_cast<const Cartesian_geometrycollection *>(&geometry);
        os << "GEOMCOL: ";
        for (size_t i = 0; i < g.size(); i++) {
          os << g[i] << " ";
        }
        return os;
      }
      case Geometry_type::kGeometry:
        break;
    }
  }
  return os << "Hello " << type_to_name(geometry.type());
}
}  // namespace gis
#endif

namespace gis_symdifference_unittest {

template <typename Types>
struct SymDifferenceTest : Gis_test<Types> {
  void test_valid_input_both_orders(
      const gis::Geometry &g1, const gis::Geometry &g2,
      const gis::Geometry &expected_result) const {
    std::unique_ptr<gis::Geometry> result;
    bool is_null = false;
    bool error =
        gis::symdifference(this->m_srs.get(), &g1, &g2, "unittest", &result);
    EXPECT_FALSE(error);

#ifdef GIS_SYMDIFFERENCE_TEST_DEBUG
    std::cout << "g1 :" << g1 << std::endl;
    std::cout << "g2 :" << g2 << std::endl;
    std::cout << "Exp:" << expected_result << std::endl;
    std::cout << "Res:" << *result << std::endl;
#endif

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

  void test_valid_input(const gis::Geometry &g1, const gis::Geometry &g2,
                        const gis::Geometry &expected_result) const {
    test_valid_input_both_orders(g1, g2, expected_result);
    test_valid_input_both_orders(g2, g1, expected_result);
  }
};

// The purpose of these tests is to check that the result returned from
// gis::symdifference is correct. The tests test all combinations of geometries.
// TYPED_TEST_SUITE(SymDifferenceTest, gis_typeset::Cartesian);
TYPED_TEST_SUITE(SymDifferenceTest, gis_typeset::Test_both);

// symdifference(... ,point, *, ...)

TYPED_TEST(SymDifferenceTest, PointPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Multipoint expected_result{};
  typename TypeParam::Geometrycollection empty_result{};

  expected_result.push_back(pt1);
  expected_result.push_back(pt2);

  this->test_valid_input(pt1, pt1, empty_result);
  this->test_valid_input(pt1, pt2, expected_result);
}

TYPED_TEST(SymDifferenceTest, PointLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Geometrycollection result{};

  this->test_valid_input(pt1, ls, ls);

  result.push_back(pt2);
  result.push_back(ls);
  this->test_valid_input(pt2, ls, result);
}

TYPED_TEST(SymDifferenceTest, PointPolygon) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection result{};

  this->test_valid_input(pt1, py, py);

  result.push_back(pt2);
  result.push_back(py);
  this->test_valid_input(pt2, py, result);
}

TYPED_TEST(SymDifferenceTest, PointMultiPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Point pt3{0.1, 0.1};
  typename TypeParam::Multipoint mpt{};
  typename TypeParam::Multipoint expected_result{};
  typename TypeParam::Geometrycollection empty_result{};

  // intersect
  mpt.push_back(pt1);
  mpt.push_back(pt2);
  mpt.push_back(pt3);
  expected_result.push_back(pt2);
  expected_result.push_back(pt3);

  this->test_valid_input(pt1, mpt, expected_result);

  mpt.clear();
  expected_result.clear();

  // disjoint
  mpt.push_back(pt2);
  mpt.push_back(pt3);
  expected_result.push_back(pt1);
  expected_result.push_back(pt2);
  expected_result.push_back(pt3);

  this->test_valid_input(pt1, mpt, expected_result);

  mpt.clear();
  expected_result.clear();

  // equals
  mpt.push_back(pt1);

  this->test_valid_input(pt1, mpt, empty_result);
}

TYPED_TEST(SymDifferenceTest, PointMultiLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Point pt3{0.05, 0.05};
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = simple_ls_2<TypeParam>();
  typename TypeParam::Multilinestring mls;
  mls.push_back(ls1);
  mls.push_back(ls2);

  this->test_valid_input(pt1, mls, mls);
  this->test_valid_input(pt2, mls, mls);

  typename TypeParam::Geometrycollection result{};
  result.push_back(mls);
  result.push_back(pt3);
  this->test_valid_input(pt3, mls, result);
}

TYPED_TEST(SymDifferenceTest, PointMultiPolygon) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection result{};

  this->test_valid_input(pt1, mpy, py);

  result.push_back(pt2);
  result.push_back(mpy);
  this->test_valid_input(pt2, mpy, result);
}

TYPED_TEST(SymDifferenceTest, PointGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc{};

  this->test_valid_input(pt1, gc, pt1);

  gc.push_back(pt1);
  gc.push_back(ls1);
  gc.push_back(mpy);

  typename TypeParam::Geometrycollection result{};

  this->test_valid_input(pt1, gc, gc);

  result.push_back(pt2);
  result.push_back(gc);
  this->test_valid_input(pt2, gc, result);

  typename TypeParam::Geometrycollection gc2{};
  gc2.push_back(mpy);
  gc2.push_back(ls1);
  this->test_valid_input(pt1, gc2, gc);
}

// symdifference(... , linestring, *, ...)

TYPED_TEST(SymDifferenceTest, LinestringLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.05, 0.0};
  typename TypeParam::Point pt3{0.1, 0.0};
  typename TypeParam::Point pt4{0.15, 0.0};
  typename TypeParam::Linestring ls1_result;
  ls1_result.push_back(pt1);
  ls1_result.push_back(pt2);
  typename TypeParam::Linestring ls2_result;
  ls2_result.push_back(pt3);
  ls2_result.push_back(pt4);
  typename TypeParam::Multilinestring mls_result;
  mls_result.push_back(ls1_result);
  mls_result.push_back(ls2_result);

  typename TypeParam::Geometrycollection empty_result{};

  this->test_valid_input(ls1, ls1, empty_result);
  this->test_valid_input(ls1, ls2, mls_result);
}

TYPED_TEST(SymDifferenceTest, LinestringPolygon) {
  // TODO: needs intersection point for geographic CS
  // for a robust test
  // typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt1{0.1, 0.05};
  typename TypeParam::Point pt2{0.1, 0.05};
  ;
  typename TypeParam::Point pt3{0.15, 0.05};

  typename TypeParam::Linestring ls;
  ls.push_back(pt1);
  ls.push_back(pt3);
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(pt3);
  typename TypeParam::Polygon py = base_py<TypeParam>();

  typename TypeParam::Geometrycollection result{};
  result.push_back(py);
  result.push_back(ls_result);

  this->test_valid_input(ls, py, result);
}

TYPED_TEST(SymDifferenceTest, LinestringMultiPoint) {
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.05, 0.0};
  typename TypeParam::Point pt3{0.15, 0.05};
  typename TypeParam::Multipoint mp;
  mp.push_back(pt1);
  mp.push_back(pt2);
  mp.push_back(pt3);

  typename TypeParam::Geometrycollection result{};
  result.push_back(ls);
  result.push_back(pt3);

  this->test_valid_input(ls, mp, result);
}

TYPED_TEST(SymDifferenceTest, LinestringMultiLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.05, 0.0};
  typename TypeParam::Point pt3{0.1, 0.0};
  typename TypeParam::Point pt4{0.15, 0.0};
  typename TypeParam::Linestring ls1_result;
  ls1_result.push_back(pt1);
  ls1_result.push_back(pt2);
  typename TypeParam::Linestring ls2_result;
  ls2_result.push_back(pt3);
  ls2_result.push_back(pt4);
  typename TypeParam::Multilinestring mls_result;
  mls_result.push_back(ls1_result);
  mls_result.push_back(ls2_result);

  typename TypeParam::Multilinestring mls{};
  this->test_valid_input(ls1, mls, ls1);
  mls.push_back(ls2);
  this->test_valid_input(ls1, mls, mls_result);
}

TYPED_TEST(SymDifferenceTest, LinestringMultiPolygon) {
  // TODO: needs intersection point for geographic CS
  // for a robust test
  // typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt1{0.1, 0.05};
  typename TypeParam::Point pt2{0.1, 0.05};
  typename TypeParam::Point pt3{0.15, 0.05};
  typename TypeParam::Linestring ls;
  ls.push_back(pt1);
  ls.push_back(pt3);
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(pt3);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();

  typename TypeParam::Geometrycollection result{};
  result.push_back(mpy);
  result.push_back(ls_result);

  this->test_valid_input(ls, mpy, result);
}

TYPED_TEST(SymDifferenceTest, LinestringGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc{};
  this->test_valid_input(ls1, gc, ls1);
  gc.push_back(pt1);
  gc.push_back(ls1);
  gc.push_back(mpy);

  this->test_valid_input(ls1, gc, base_py<TypeParam>());

  typename TypeParam::Linestring ls_intersect_py;
  ls_intersect_py.push_back(pt2);
  typename TypeParam::Point pt5{0.1, 0.0};
  ls_intersect_py.push_back(pt5);
  typename TypeParam::Geometrycollection result{};
  result.push_back(mpy);
  result.push_back(ls1);
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  typename TypeParam::Point pt6{0.1, 0.1};
  ls_result.push_back(pt6);
  result.push_back(ls_result);
  this->test_valid_input(ls_intersect_py, gc, result);

  typename TypeParam::Linestring ls2;
  ls2.push_back(pt3);
  ls2.push_back(pt4);
  ls2.push_back(pt2);
  typename TypeParam::Linestring ls_result2;
  ls_result2.push_back(pt2);
  ls_result2.push_back(pt4);

  // using clear() here create memory leaks
  // e.g.
  // result.clear();
  // result.push_back(ls_result2);
  // result.push_back(mpy);
  // this->test_valid_input(ls2, gc, result);

  typename TypeParam::Geometrycollection result2;
  result2.push_back(ls_result2);
  result2.push_back(mpy);
  this->test_valid_input(ls2, gc, result2);
}

// symdifference(... , polygon, *, ...)

TYPED_TEST(SymDifferenceTest, PolygonPolygon) {
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.1, 0.0};
  typename TypeParam::Point pt3{0.0, 0.1};
  typename TypeParam::Point pt4{0.1, 0.1};
  typename TypeParam::Point pt5{0.1, 0.2};
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2;
  typename TypeParam::Linearring exterior2;
  exterior2.push_back(pt2);
  exterior2.push_back(pt5);
  exterior2.push_back(pt3);
  exterior2.push_back(pt2);
  py2.push_back(exterior2);

  typename TypeParam::Multipolygon mpy_result;
  typename TypeParam::Polygon py1_result;
  typename TypeParam::Linearring exterior1r;
  typename TypeParam::Polygon py2_result;
  typename TypeParam::Linearring exterior2r;
  exterior1r.push_back(pt1);
  exterior1r.push_back(pt2);
  exterior1r.push_back(pt3);
  exterior1r.push_back(pt1);
  py1_result.push_back(exterior1r);
  mpy_result.push_back(py1_result);
  exterior2r.push_back(pt3);
  exterior2r.push_back(pt4);
  exterior2r.push_back(pt5);
  exterior2r.push_back(pt3);
  py2_result.push_back(exterior2r);
  mpy_result.push_back(py2_result);
  typename TypeParam::Geometrycollection empty_result{};

  this->test_valid_input(py1, py1, empty_result);
  this->test_valid_input(py1, py2, mpy_result);
}

TYPED_TEST(SymDifferenceTest, PolygonMultiPoint) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Multipoint mp;
  mp.push_back(pt1);
  mp.push_back(pt2);
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection result{};
  result.push_back(pt2);
  result.push_back(py);

  this->test_valid_input(py, mp, result);
}

TYPED_TEST(SymDifferenceTest, PolygonMultiLinestring) {
  // TODO: needs intersection point for geographic CS
  // for a robust test
  // typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt1{0.1, 0.05};
  typename TypeParam::Point pt2{0.1, 0.05};
  typename TypeParam::Point pt3{0.15, 0.05};
  typename TypeParam::Linestring ls;
  ls.push_back(pt1);
  ls.push_back(pt3);
  typename TypeParam::Multilinestring mls;
  mls.push_back(ls);
  typename TypeParam::Polygon py = base_py<TypeParam>();

  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(pt3);
  typename TypeParam::Geometrycollection result{};
  result.push_back(py);
  result.push_back(ls_result);

  this->test_valid_input(py, mls, result);
}

TYPED_TEST(SymDifferenceTest, PolygonMultiPolygon) {
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.1, 0.0};
  typename TypeParam::Point pt3{0.0, 0.1};
  typename TypeParam::Point pt4{0.1, 0.1};
  typename TypeParam::Point pt5{0.1, 0.2};
  typename TypeParam::Polygon py1 = base_py<TypeParam>();
  typename TypeParam::Polygon py2;
  typename TypeParam::Linearring exterior2;
  exterior2.push_back(pt2);
  exterior2.push_back(pt5);
  exterior2.push_back(pt3);
  exterior2.push_back(pt2);
  py2.push_back(exterior2);
  typename TypeParam::Multipolygon mpy;
  mpy.push_back(py2);

  typename TypeParam::Multipolygon mpy_result;
  typename TypeParam::Polygon py1_result;
  typename TypeParam::Linearring exterior1r;
  typename TypeParam::Polygon py2_result;
  typename TypeParam::Linearring exterior2r;
  exterior1r.push_back(pt1);
  exterior1r.push_back(pt2);
  exterior1r.push_back(pt3);
  exterior1r.push_back(pt1);
  py1_result.push_back(exterior1r);
  mpy_result.push_back(py1_result);
  exterior2r.push_back(pt3);
  exterior2r.push_back(pt4);
  exterior2r.push_back(pt5);
  exterior2r.push_back(pt3);
  py2_result.push_back(exterior2r);
  mpy_result.push_back(py2_result);

  this->test_valid_input(py1, mpy, mpy_result);

  typename TypeParam::Multipolygon mpy2 = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection empty_result{};
  this->test_valid_input(py1, mpy2, empty_result);
}

TYPED_TEST(SymDifferenceTest, PolygonGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  // typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection gc{};
  this->test_valid_input(py, gc, py);
  gc.push_back(pt3);
  gc.push_back(ls1);
  gc.push_back(py);
  this->test_valid_input(py, gc, ls1);

  typename TypeParam::Linestring ls2;
  ls2.push_back(pt3);
  ls2.push_back(pt4);
  ls2.push_back(pt2);
  typename TypeParam::Linestring ls3;
  ls3.push_back(pt1);
  ls3.push_back(pt2);
  gc.push_back(ls2);
  gc.push_back(ls3);
  typename TypeParam::Multilinestring result{};
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(typename TypeParam::Point(0.0666667, 0.1));
  result.push_back(ls_result);
  result.push_back(ls1);
  result.push_back(ls2);
  this->test_valid_input(py, gc, result);
}

// symdifference(... , multipoint, *, ...)

TYPED_TEST(SymDifferenceTest, MultiPointMultiPoint) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0, 0.1};
  typename TypeParam::Point pt3{0.1, 0.1};
  typename TypeParam::Multipoint mpt1{};
  mpt1.push_back(pt1);
  typename TypeParam::Multipoint mpt2{};
  typename TypeParam::Multipoint expected_result{};
  typename TypeParam::Geometrycollection empty_result{};

  // intersect
  mpt2.push_back(pt1);
  mpt2.push_back(pt2);
  mpt2.push_back(pt3);
  expected_result.push_back(pt2);
  expected_result.push_back(pt3);

  this->test_valid_input(mpt1, mpt2, expected_result);

  mpt2.clear();
  expected_result.clear();

  // disjoint
  mpt2.push_back(pt2);
  mpt2.push_back(pt3);
  expected_result.push_back(pt1);
  expected_result.push_back(pt2);
  expected_result.push_back(pt3);

  this->test_valid_input(mpt1, mpt2, expected_result);

  mpt2.clear();
  expected_result.clear();

  // equals
  mpt2.push_back(pt1);

  this->test_valid_input(mpt1, mpt2, empty_result);
}

TYPED_TEST(SymDifferenceTest, MultiPointMultiLinestring) {
  typename TypeParam::Point pt1{0, 0};
  typename TypeParam::Point pt2{0.1, 0.1};
  typename TypeParam::Point pt3{0.05, 0.05};
  typename TypeParam::Multipoint mpt1;
  mpt1.push_back(pt1);
  typename TypeParam::Multipoint mpt2;
  mpt2.push_back(pt1);
  mpt2.push_back(pt2);
  typename TypeParam::Multipoint mpt3;
  mpt3.push_back(pt1);
  mpt3.push_back(pt2);
  mpt3.push_back(pt3);
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = simple_ls_2<TypeParam>();
  typename TypeParam::Multilinestring mls;
  mls.push_back(ls1);
  mls.push_back(ls2);

  this->test_valid_input(mpt1, mls, mls);
  this->test_valid_input(mpt2, mls, mls);

  typename TypeParam::Geometrycollection result{};
  result.push_back(mls);
  result.push_back(pt3);
  this->test_valid_input(mpt3, mls, result);
}

TYPED_TEST(SymDifferenceTest, MultiPointMultiPolygon) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Multipoint mpt1;
  mpt1.push_back(pt1);
  typename TypeParam::Multipoint mpt2;
  mpt2.push_back(pt1);
  mpt2.push_back(pt2);
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection result{};

  this->test_valid_input(mpt1, mpy, py);

  result.push_back(pt2);
  result.push_back(mpy);
  this->test_valid_input(mpt2, mpy, result);
}

TYPED_TEST(SymDifferenceTest, MultiPointGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Point pt5{0.3, 0.0};
  typename TypeParam::Point pt6{0.3, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc{};
  gc.push_back(pt2);
  gc.push_back(pt3);
  gc.push_back(pt4);
  gc.push_back(pt5);
  gc.push_back(ls1);
  gc.push_back(mpy);

  typename TypeParam::Multipoint mp;
  mp.push_back(pt1);
  mp.push_back(pt2);
  mp.push_back(pt3);
  mp.push_back(pt6);
  this->test_valid_input(mp, typename TypeParam::Geometrycollection(), mp);

  typename TypeParam::Geometrycollection result{};
  result.push_back(mpy);
  result.push_back(ls1);
  result.push_back(pt5);
  result.push_back(pt6);

  this->test_valid_input(mp, gc, result);
}

// symdifference(... , multilinestring, *, ...)

TYPED_TEST(SymDifferenceTest, MultiLinestringMultiLinestring) {
  typename TypeParam::Linestring ls1 = simple_ls<TypeParam>();
  typename TypeParam::Linestring ls2 = offset_simple_ls<TypeParam>();
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.05, 0.0};
  typename TypeParam::Point pt3{0.1, 0.0};
  typename TypeParam::Point pt4{0.15, 0.0};
  typename TypeParam::Multilinestring mls1;
  mls1.push_back(ls1);
  typename TypeParam::Linestring ls1_result;
  ls1_result.push_back(pt1);
  ls1_result.push_back(pt2);
  typename TypeParam::Linestring ls2_result;
  ls2_result.push_back(pt3);
  ls2_result.push_back(pt4);
  typename TypeParam::Multilinestring mls_result;
  mls_result.push_back(ls1_result);
  mls_result.push_back(ls2_result);

  typename TypeParam::Multilinestring mls{};
  this->test_valid_input(mls1, mls, ls1);
  mls.push_back(ls2);
  this->test_valid_input(mls1, mls, mls_result);
}

TYPED_TEST(SymDifferenceTest, MultiLinestringMultiPolygon) {
  // TODO: needs intersection point for geographic CS
  // for a robust test
  // typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt1{0.1, 0.05};
  typename TypeParam::Point pt2{0.1, 0.05};
  typename TypeParam::Point pt3{0.15, 0.05};
  typename TypeParam::Linestring ls;
  ls.push_back(pt1);
  ls.push_back(pt3);
  typename TypeParam::Multilinestring mls;
  mls.push_back(ls);
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(pt3);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();

  typename TypeParam::Geometrycollection result{};
  result.push_back(mpy);
  result.push_back(ls_result);

  this->test_valid_input(mls, mpy, result);
}

TYPED_TEST(SymDifferenceTest, MultiLinestringGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc{};
  this->test_valid_input(ls1, gc, ls1);
  gc.push_back(pt1);
  gc.push_back(ls1);
  gc.push_back(mpy);

  typename TypeParam::Multilinestring mls;
  mls.push_back(ls1);
  this->test_valid_input(mls, gc, base_py<TypeParam>());

  typename TypeParam::Linestring ls_intersect_py;
  ls_intersect_py.push_back(pt2);
  typename TypeParam::Point pt5{0.1, 0.0};
  ls_intersect_py.push_back(pt5);
  typename TypeParam::Geometrycollection result{};
  result.push_back(mpy);
  result.push_back(ls1);
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  typename TypeParam::Point pt6{0.1, 0.1};
  ls_result.push_back(pt6);
  result.push_back(ls_result);
  mls.clear();
  mls.push_back(ls_intersect_py);
  this->test_valid_input(mls, gc, result);

  typename TypeParam::Linestring ls2;
  ls2.push_back(pt3);
  ls2.push_back(pt4);
  ls2.push_back(pt2);
  typename TypeParam::Linestring ls_result2;
  ls_result2.push_back(pt2);
  ls_result2.push_back(pt4);

  typename TypeParam::Geometrycollection result2;
  result2.push_back(ls_result2);
  result2.push_back(mpy);
  mls.clear();
  mls.push_back(ls2);
  this->test_valid_input(mls, gc, result2);
}

// symdifference(... , multipolygon, *, ...)

TYPED_TEST(SymDifferenceTest, MultiPolygonMultiPolygon) {
  typename TypeParam::Point pt1{0.0, 0.0};
  typename TypeParam::Point pt2{0.1, 0.0};
  typename TypeParam::Point pt3{0.0, 0.1};
  typename TypeParam::Point pt4{0.1, 0.1};
  typename TypeParam::Point pt5{0.1, 0.2};
  typename TypeParam::Multipolygon mpy1 = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py2;
  typename TypeParam::Linearring exterior2;
  exterior2.push_back(pt2);
  exterior2.push_back(pt5);
  exterior2.push_back(pt3);
  exterior2.push_back(pt2);
  py2.push_back(exterior2);
  typename TypeParam::Multipolygon mpy2;
  mpy2.push_back(py2);

  typename TypeParam::Multipolygon mpy_result;
  typename TypeParam::Polygon py1_result;
  typename TypeParam::Linearring exterior1r;
  typename TypeParam::Polygon py2_result;
  typename TypeParam::Linearring exterior2r;
  exterior1r.push_back(pt1);
  exterior1r.push_back(pt2);
  exterior1r.push_back(pt3);
  exterior1r.push_back(pt1);
  py1_result.push_back(exterior1r);
  mpy_result.push_back(py1_result);
  exterior2r.push_back(pt3);
  exterior2r.push_back(pt4);
  exterior2r.push_back(pt5);
  exterior2r.push_back(pt3);
  py2_result.push_back(exterior2r);
  mpy_result.push_back(py2_result);

  this->test_valid_input(mpy1, mpy2, mpy_result);
}

TYPED_TEST(SymDifferenceTest, MultiPolygonGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection gc{};

  this->test_valid_input(mpy, gc, py);
  gc.push_back(pt3);
  gc.push_back(ls1);
  gc.push_back(mpy);

  this->test_valid_input(mpy, gc, ls1);

  typename TypeParam::Linestring ls2;
  ls2.push_back(pt3);
  ls2.push_back(pt4);
  ls2.push_back(pt2);
  typename TypeParam::Linestring ls3;
  ls3.push_back(pt1);
  ls3.push_back(pt2);
  gc.push_back(ls2);
  gc.push_back(ls3);
  typename TypeParam::Multilinestring result{};
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(typename TypeParam::Point(0.0666667, 0.1));
  result.push_back(ls_result);
  result.push_back(ls1);
  result.push_back(ls2);
  this->test_valid_input(mpy, gc, result);
}

// symdifference(... , geometrycollection, geometrycollection, ...)

TYPED_TEST(SymDifferenceTest, GeometryCollectionGeometryCollection) {
  typename TypeParam::Point pt1{0.05, 0.05};
  typename TypeParam::Point pt2{0.1, 0.2};
  typename TypeParam::Point pt3{0.2, 0.0};
  typename TypeParam::Point pt4{0.2, 0.1};
  typename TypeParam::Linestring ls1;
  ls1.push_back(pt3);
  ls1.push_back(pt4);
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Geometrycollection emptygc{};
  typename TypeParam::Geometrycollection gc{};
  typename TypeParam::Geometrycollection gc_py{};
  gc_py.push_back(mpy);
  this->test_valid_input(gc_py, gc_py, emptygc);
  this->test_valid_input(gc_py, gc, py);
  gc.push_back(ls1);
  this->test_valid_input(gc, gc, emptygc);
  gc.push_back(pt3);
  gc.push_back(mpy);
  this->test_valid_input(gc_py, gc, ls1);

  typename TypeParam::Linestring ls2;
  ls2.push_back(pt3);
  ls2.push_back(pt4);
  ls2.push_back(pt2);
  typename TypeParam::Linestring ls3;
  ls3.push_back(pt1);
  ls3.push_back(pt2);
  gc.push_back(ls2);
  gc.push_back(ls3);
  typename TypeParam::Multilinestring result{};
  typename TypeParam::Linestring ls_result;
  ls_result.push_back(pt2);
  ls_result.push_back(typename TypeParam::Point(0.0666667, 0.1));
  result.push_back(ls_result);
  result.push_back(ls1);
  result.push_back(ls2);
  this->test_valid_input(gc_py, gc, result);
}

}  // namespace gis_symdifference_unittest

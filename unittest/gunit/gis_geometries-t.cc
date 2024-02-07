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

#include "my_config.h"

#include <gtest/gtest.h>
#include <memory>  // unique_ptr

#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometry_visitor.h"

namespace geometries_unittest {

struct Cartesian_types {
  typedef gis::Cartesian_point Point;
  typedef gis::Cartesian_linestring Linestring;
  typedef gis::Cartesian_linearring Linearring;
  typedef gis::Cartesian_polygon Polygon;
  typedef gis::Cartesian_geometrycollection Geometrycollection;
  typedef gis::Cartesian_multipoint Multipoint;
  typedef gis::Cartesian_multilinestring Multilinestring;
  typedef gis::Cartesian_multipolygon Multipolygon;

  static gis::Coordinate_system coordinate_system() {
    return gis::Coordinate_system::kCartesian;
  }
};

struct Geographic_types {
  typedef gis::Geographic_point Point;
  typedef gis::Geographic_linestring Linestring;
  typedef gis::Geographic_linearring Linearring;
  typedef gis::Geographic_polygon Polygon;
  typedef gis::Geographic_geometrycollection Geometrycollection;
  typedef gis::Geographic_multipoint Multipoint;
  typedef gis::Geographic_multilinestring Multilinestring;
  typedef gis::Geographic_multipolygon Multipolygon;

  static gis::Coordinate_system coordinate_system() {
    return gis::Coordinate_system::kGeographic;
  }
};

template <typename Types>
class GeometriesTest : public ::testing::Test {
 public:
  GeometriesTest() = default;

  ~GeometriesTest() override = default;
};

typedef ::testing::Types<Cartesian_types, Geographic_types> Types;
TYPED_TEST_SUITE(GeometriesTest, Types);

TYPED_TEST(GeometriesTest, Point) {
  typename TypeParam::Point pt;
  EXPECT_EQ(gis::Geometry_type::kPoint, pt.type());
  EXPECT_EQ(TypeParam::coordinate_system(), pt.coordinate_system());
  EXPECT_TRUE(std::isnan(pt.x()));
  EXPECT_TRUE(std::isnan(pt.y()));
  EXPECT_TRUE(pt.is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(pt.accept(&visitor));

  typename TypeParam::Point pt2(0.0, 0.0);
  EXPECT_EQ(0.0, pt2.x());
  EXPECT_EQ(0.0, pt2.y());

  pt.x(-1.0);
  pt.y(-1.0);
  EXPECT_EQ(-1.0, pt.x());
  EXPECT_EQ(-1.0, pt.y());
  EXPECT_FALSE(pt.is_empty());

  pt.x(1.0);
  pt.y(1.0);
  EXPECT_EQ(1.0, pt.x());
  EXPECT_EQ(1.0, pt.y());

  pt.x(1.7976931348623157e308);
  pt.y(-1.7976931348623157e308);
  EXPECT_EQ(1.7976931348623157e308, pt.x());
  EXPECT_EQ(-1.7976931348623157e308, pt.y());

  std::unique_ptr<typename TypeParam::Point> pt_clone(pt.clone());
  EXPECT_FALSE(&pt == pt_clone.get());
  EXPECT_EQ(pt.type(), pt_clone.get()->type());
  EXPECT_EQ(pt.coordinate_system(), pt_clone.get()->coordinate_system());
  EXPECT_EQ(pt.x(), pt_clone.get()->x());
  EXPECT_EQ(pt.y(), pt_clone.get()->y());
}

TYPED_TEST(GeometriesTest, Curve) {}

TYPED_TEST(GeometriesTest, Linestring) {
  typename TypeParam::Linestring ls;
  EXPECT_EQ(gis::Geometry_type::kLinestring, ls.type());
  EXPECT_EQ(TypeParam::coordinate_system(), ls.coordinate_system());
  EXPECT_EQ(0U, ls.size());
  EXPECT_TRUE(ls.empty());
  EXPECT_TRUE(ls.is_empty());

  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  ls.push_back(typename TypeParam::Point(10.0, 10.0));
  ls.push_back(typename TypeParam::Point(20.0, 0.0));
  ls.push_back(typename TypeParam::Point(30.0, 10.0));
  EXPECT_EQ(4U, ls.size());
  EXPECT_FALSE(ls.empty());
  EXPECT_FALSE(ls.is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(ls.accept(&visitor));

  EXPECT_EQ(0.0, ls[0].x());
  EXPECT_EQ(0.0, ls[0].y());
  EXPECT_EQ(30.0, ls[3].x());
  EXPECT_EQ(10.0, ls[3].y());

  EXPECT_EQ(0.0, ls.front().x());
  EXPECT_EQ(0.0, ls.front().y());

  ls.pop_front();
  EXPECT_EQ(3U, ls.size());
  EXPECT_EQ(10.0, ls.front().x());
  EXPECT_EQ(10.0, ls.front().y());

  std::unique_ptr<gis::Linestring> ls_new(
      gis::Linestring::create_linestring(ls.coordinate_system()));
  EXPECT_FALSE(&ls == ls_new.get());
  EXPECT_EQ(gis::Geometry_type::kLinestring, ls_new.get()->type());
  EXPECT_EQ(ls.coordinate_system(), ls_new.get()->coordinate_system());
  EXPECT_TRUE(ls_new.get()->empty());
  EXPECT_TRUE(ls_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Linestring> ls_clone(ls.clone());
  EXPECT_FALSE(&ls == ls_clone.get());
  EXPECT_EQ(ls.type(), ls_clone.get()->type());
  EXPECT_EQ(ls.coordinate_system(), ls_clone.get()->coordinate_system());
  EXPECT_EQ(ls.size(), ls_clone.get()->size());
  for (std::size_t i = 0; i < ls.size(); i++) {
    EXPECT_EQ(ls[i].x(), (*ls_clone.get())[i].x());
    EXPECT_EQ(ls[i].y(), (*ls_clone.get())[i].y());
  }
}

TYPED_TEST(GeometriesTest, Linearring) {
  typename TypeParam::Linearring lr;
  EXPECT_EQ(gis::Geometry_type::kLinestring, lr.type());
  EXPECT_EQ(TypeParam::coordinate_system(), lr.coordinate_system());
  EXPECT_EQ(0U, lr.size());
  EXPECT_TRUE(lr.empty());
  EXPECT_TRUE(lr.is_empty());

  lr.push_back(typename TypeParam::Point(0.0, 0.0));
  lr.push_back(typename TypeParam::Point(10.0, 10.0));
  lr.push_back(typename TypeParam::Point(20.0, 0.0));
  lr.push_back(typename TypeParam::Point(0.0, 10.0));
  EXPECT_EQ(4U, lr.size());
  EXPECT_FALSE(lr.empty());
  EXPECT_FALSE(lr.is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(lr.accept(&visitor));

  EXPECT_EQ(10.0, lr[1].x());
  EXPECT_EQ(10.0, lr[1].y());
  EXPECT_EQ(20.0, lr[2].x());
  EXPECT_EQ(0.0, lr[2].y());

  EXPECT_EQ(0.0, lr.back().x());
  EXPECT_EQ(10.0, lr.back().y());

  EXPECT_EQ(0.0, lr.front().x());
  EXPECT_EQ(0.0, lr.front().y());

  lr.pop_front();
  EXPECT_EQ(3U, lr.size());
  EXPECT_EQ(10.0, lr.front().x());
  EXPECT_EQ(10.0, lr.front().y());

  std::unique_ptr<gis::Linearring> lr_new(
      gis::Linearring::create_linearring(lr.coordinate_system()));
  EXPECT_FALSE(&lr == lr_new.get());
  EXPECT_EQ(gis::Geometry_type::kLinestring, lr_new.get()->type());
  EXPECT_EQ(lr.coordinate_system(), lr_new.get()->coordinate_system());
  EXPECT_TRUE(lr_new.get()->empty());
  EXPECT_TRUE(lr_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Linearring> lr_clone(
      static_cast<typename TypeParam::Linearring *>(lr.clone()));
  EXPECT_FALSE(&lr == lr_clone.get());
  EXPECT_EQ(lr.type(), lr_clone.get()->type());
  EXPECT_EQ(lr.coordinate_system(), lr_clone.get()->coordinate_system());
  EXPECT_EQ(lr.size(), lr_clone.get()->size());
  for (std::size_t i = 0; i < lr.size(); i++) {
    EXPECT_EQ(lr[i].x(), (*lr_clone.get())[i].x());
    EXPECT_EQ(lr[i].y(), (*lr_clone.get())[i].y());
  }
  typename TypeParam::Polygon py;
  py.push_back(*lr_clone.get());
  EXPECT_EQ(1U, py.size());
}

TYPED_TEST(GeometriesTest, Surface) {}

TYPED_TEST(GeometriesTest, Polygon) {
  typename TypeParam::Polygon py;
  EXPECT_EQ(gis::Geometry_type::kPolygon, py.type());
  EXPECT_EQ(TypeParam::coordinate_system(), py.coordinate_system());
  EXPECT_EQ(0U, py.size());
  EXPECT_TRUE(py.empty());
  EXPECT_TRUE(py.is_empty());

  typename TypeParam::Linearring exterior;
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));
  py.push_back(exterior);
  EXPECT_FALSE(py.empty());
  EXPECT_FALSE(py.is_empty());

  typename TypeParam::Linearring interior;
  interior.push_back(typename TypeParam::Point(2.0, 2.0));
  interior.push_back(typename TypeParam::Point(2.0, 8.0));
  interior.push_back(typename TypeParam::Point(8.0, 8.0));
  interior.push_back(typename TypeParam::Point(8.0, 2.0));
  interior.push_back(typename TypeParam::Point(2.0, 2.0));
  py.push_back(std::move(interior));

  EXPECT_EQ(2U, py.size());
  EXPECT_FALSE(py.empty());
  EXPECT_EQ(1U, py.interior_rings().size());
  EXPECT_FALSE(py.interior_ring(0).empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(py.accept(&visitor));

  std::unique_ptr<gis::Polygon> py_new(
      gis::Polygon::create_polygon(py.coordinate_system()));
  EXPECT_FALSE(&py == py_new.get());
  EXPECT_EQ(gis::Geometry_type::kPolygon, py_new.get()->type());
  EXPECT_EQ(py.coordinate_system(), py_new.get()->coordinate_system());
  EXPECT_TRUE(py_new.get()->empty());
  EXPECT_TRUE(py_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Polygon> py_clone(py.clone());
  EXPECT_FALSE(&py == py_clone.get());
  EXPECT_EQ(py.type(), py_clone.get()->type());
  EXPECT_EQ(py.coordinate_system(), py_clone.get()->coordinate_system());
  EXPECT_EQ(py.size(), py_clone.get()->size());
  for (std::size_t i = 0; i < py.exterior_ring().size(); i++) {
    EXPECT_EQ(py.exterior_ring()[i].x(),
              py_clone.get()->exterior_ring()[i].x());
    EXPECT_EQ(py.exterior_ring()[i].y(),
              py_clone.get()->exterior_ring()[i].y());
  }
  for (std::size_t i = 0; i < py.interior_rings().size(); i++) {
    for (std::size_t j = 0; j < py.interior_ring(i).size(); j++) {
      EXPECT_EQ(py.interior_ring(i)[j].x(),
                py_clone.get()->interior_ring(i)[j].x());
      EXPECT_EQ(py.interior_ring(i)[j].x(),
                py_clone.get()->interior_ring(i)[j].x());
    }
  }
}

TYPED_TEST(GeometriesTest, Geometrycollection) {
  typename TypeParam::Geometrycollection gc;
  EXPECT_EQ(gis::Geometry_type::kGeometrycollection, gc.type());
  EXPECT_EQ(TypeParam::coordinate_system(), gc.coordinate_system());
  EXPECT_TRUE(gc.empty());
  EXPECT_TRUE(gc.is_empty());

  gc.push_back(typename TypeParam::Geometrycollection());
  EXPECT_FALSE(gc.empty());
  EXPECT_TRUE(gc.is_empty());

  typename TypeParam::Geometrycollection gc2;
  gc2.push_back(typename TypeParam::Geometrycollection());
  gc.push_back(std::move(gc2));
  EXPECT_TRUE(gc.is_empty());

  gc.push_back(typename TypeParam::Point(0.0, 0.0));
  gc.push_back(typename TypeParam::Point(10.0, 0.0));
  gc.push_back(typename TypeParam::Point(10.0, 10.0));
  gc.push_back(typename TypeParam::Point(0.0, 10.0));
  gc.push_back(typename TypeParam::Point(0.0, 0.0));

  typename TypeParam::Linestring ls;
  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  ls.push_back(typename TypeParam::Point(10.0, 0.0));
  ls.push_back(typename TypeParam::Point(10.0, 10.0));
  ls.push_back(typename TypeParam::Point(0.0, 10.0));
  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  gc.push_back(std::move(ls));

  typename TypeParam::Linearring exterior;
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));
  typename TypeParam::Polygon py;
  py.push_back(std::move(exterior));
  gc.push_back(std::move(py));

  typename TypeParam::Multipoint mpt;
  mpt.push_back(typename TypeParam::Point(0.0, 0.0));
  gc.push_back(std::move(mpt));

  typename TypeParam::Linestring ls2;
  ls2.push_back(typename TypeParam::Point(0.0, 0.0));
  ls2.push_back(typename TypeParam::Point(1.0, 1.0));
  typename TypeParam::Multilinestring mls;
  mls.push_back(std::move(ls2));
  gc.push_back(std::move(mls));

  typename TypeParam::Multipolygon mpy;
  gc.push_back(std::move(mpy));

  typename TypeParam::Geometrycollection inner_gc;
  gc.push_back(std::move(inner_gc));

  EXPECT_EQ(13U, gc.size());
  EXPECT_FALSE(gc.empty());
  EXPECT_FALSE(gc.is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(gc.accept(&visitor));

  typename TypeParam::Geometrycollection gc_copy = gc;
  EXPECT_EQ(13U, gc_copy.size());
  EXPECT_FALSE(gc.empty());
  EXPECT_FALSE(gc.is_empty());

  EXPECT_EQ(gis::Geometry_type::kGeometrycollection, gc.front().type());

  gc.pop_front();
  gc.pop_front();
  EXPECT_EQ(11U, gc.size());
  EXPECT_EQ(gis::Geometry_type::kPoint, gc.front().type());

  std::unique_ptr<gis::Geometrycollection> gc_new(
      gis::Geometrycollection::create_geometrycollection(
          gc.coordinate_system()));
  EXPECT_FALSE(&gc == gc_new.get());
  EXPECT_EQ(gis::Geometry_type::kGeometrycollection, gc_new.get()->type());
  EXPECT_EQ(gc.coordinate_system(), gc_new.get()->coordinate_system());
  EXPECT_TRUE(gc_new.get()->empty());
  EXPECT_TRUE(gc_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Geometrycollection> gc_clone(gc.clone());
  EXPECT_FALSE(&gc == gc_clone.get());
  EXPECT_EQ(gc.type(), gc_clone.get()->type());
  EXPECT_EQ(gc.coordinate_system(), gc_clone.get()->coordinate_system());
  EXPECT_EQ(gc.size(), gc_clone.get()->size());
  for (std::size_t i = 0; i < gc.size(); i++) {
    EXPECT_EQ(gc[i].type(), (*gc_clone.get())[i].type());
    EXPECT_EQ(gc[i].coordinate_system(),
              (*gc_clone.get())[i].coordinate_system());
  }
}

TYPED_TEST(GeometriesTest, Multipoint) {
  typename TypeParam::Multipoint mpt;
  EXPECT_EQ(gis::Geometry_type::kMultipoint, mpt.type());
  EXPECT_EQ(TypeParam::coordinate_system(), mpt.coordinate_system());
  EXPECT_TRUE(mpt.empty());
  EXPECT_TRUE(mpt.is_empty());

  mpt.push_back(typename TypeParam::Point(0.0, 0.0));
  EXPECT_EQ(1U, mpt.size());
  EXPECT_FALSE(mpt.empty());
  EXPECT_FALSE(mpt.is_empty());

  mpt.push_back(typename TypeParam::Point(1.0, 1.0));
  EXPECT_EQ(2U, mpt.size());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(mpt.accept(&visitor));

  EXPECT_EQ(0.0, mpt.front().x());
  EXPECT_EQ(0.0, mpt.front().y());

  mpt.pop_front();
  EXPECT_EQ(1U, mpt.size());
  EXPECT_EQ(1.0, mpt.front().x());
  EXPECT_EQ(1.0, mpt.front().y());

  std::unique_ptr<gis::Multipoint> mpt_new(
      gis::Multipoint::create_multipoint(mpt.coordinate_system()));
  EXPECT_FALSE(&mpt == mpt_new.get());
  EXPECT_EQ(gis::Geometry_type::kMultipoint, mpt_new.get()->type());
  EXPECT_EQ(mpt.coordinate_system(), mpt_new.get()->coordinate_system());
  EXPECT_TRUE(mpt_new.get()->empty());
  EXPECT_TRUE(mpt_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Multipoint> mpt_clone(mpt.clone());
  EXPECT_FALSE(&mpt == mpt_clone.get());
  EXPECT_EQ(mpt.type(), mpt_clone.get()->type());
  EXPECT_EQ(mpt.coordinate_system(), mpt_clone.get()->coordinate_system());
  EXPECT_EQ(mpt.size(), mpt_clone.get()->size());
  for (std::size_t i = 0; i < mpt.size(); i++) {
    EXPECT_EQ(mpt[i].x(), (*mpt_clone.get())[i].x());
    EXPECT_EQ(mpt[i].y(), (*mpt_clone)[i].y());
  }
}

TYPED_TEST(GeometriesTest, Multicurve) {
  std::unique_ptr<gis::Multicurve> mc(new
                                      typename TypeParam::Multilinestring());
  EXPECT_EQ(0U, mc->size());
  EXPECT_TRUE(mc->empty());
  EXPECT_TRUE(mc->is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(mc->accept(&visitor));
}

TYPED_TEST(GeometriesTest, Multilinestring) {
  typename TypeParam::Multilinestring mls;
  EXPECT_EQ(gis::Geometry_type::kMultilinestring, mls.type());
  EXPECT_EQ(TypeParam::coordinate_system(), mls.coordinate_system());
  EXPECT_TRUE(mls.empty());
  EXPECT_TRUE(mls.is_empty());

  typename TypeParam::Linestring ls;
  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  ls.push_back(typename TypeParam::Point(10.0, 0.0));
  ls.push_back(typename TypeParam::Point(10.0, 10.0));
  ls.push_back(typename TypeParam::Point(0.0, 10.0));
  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  mls.push_back(std::move(ls));
  EXPECT_EQ(1U, mls.size());
  EXPECT_FALSE(mls.empty());
  EXPECT_FALSE(mls.is_empty());

  typename TypeParam::Linestring ls2;
  ls.push_back(typename TypeParam::Point(0.0, 0.0));
  ls.push_back(typename TypeParam::Point(20.0, 20.0));
  mls.push_back(std::move(ls));
  EXPECT_EQ(2U, mls.size());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(mls.accept(&visitor));

  EXPECT_EQ(5U, mls.front().size());

  mls.pop_front();
  EXPECT_EQ(1U, mls.size());
  EXPECT_EQ(2U, mls.front().size());

  std::unique_ptr<gis::Multilinestring> mls_new(
      gis::Multilinestring::create_multilinestring(mls.coordinate_system()));
  EXPECT_FALSE(&mls == mls_new.get());
  EXPECT_EQ(gis::Geometry_type::kMultilinestring, mls_new.get()->type());
  EXPECT_EQ(mls.coordinate_system(), mls_new.get()->coordinate_system());
  EXPECT_TRUE(mls_new.get()->empty());
  EXPECT_TRUE(mls_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Multilinestring> mls_clone(mls.clone());
  EXPECT_FALSE(&mls == mls_clone.get());
  EXPECT_EQ(mls.type(), mls_clone.get()->type());
  EXPECT_EQ(mls.coordinate_system(), mls_clone.get()->coordinate_system());
  EXPECT_EQ(mls.size(), mls_clone.get()->size());
  for (std::size_t i = 0; i < mls.size(); i++) {
    for (std::size_t j = 0; j < mls[i].size(); j++) {
      EXPECT_EQ(mls[i][j].x(), (*mls_clone.get())[i][j].x());
      EXPECT_EQ(mls[i][j].y(), (*mls_clone.get())[i][j].y());
    }
  }
}

TYPED_TEST(GeometriesTest, Multisurface) {
  std::unique_ptr<gis::Multisurface> ms(new typename TypeParam::Multipolygon());
  EXPECT_EQ(0U, ms->size());
  EXPECT_TRUE(ms->empty());
  EXPECT_TRUE(ms->is_empty());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(ms->accept(&visitor));
}

TYPED_TEST(GeometriesTest, Multipolygon) {
  typename TypeParam::Multipolygon mpy;
  EXPECT_EQ(gis::Geometry_type::kMultipolygon, mpy.type());
  EXPECT_EQ(TypeParam::coordinate_system(), mpy.coordinate_system());
  EXPECT_TRUE(mpy.empty());
  EXPECT_TRUE(mpy.is_empty());

  typename TypeParam::Linearring exterior;
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 0.0));
  exterior.push_back(typename TypeParam::Point(10.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 10.0));
  exterior.push_back(typename TypeParam::Point(0.0, 0.0));

  typename TypeParam::Linearring interior;
  interior.push_back(typename TypeParam::Point(2.0, 2.0));
  interior.push_back(typename TypeParam::Point(2.0, 8.0));
  interior.push_back(typename TypeParam::Point(8.0, 8.0));
  interior.push_back(typename TypeParam::Point(8.0, 2.0));
  interior.push_back(typename TypeParam::Point(2.0, 2.0));

  typename TypeParam::Polygon py;
  py.push_back(std::move(exterior));
  py.push_back(std::move(interior));
  mpy.push_back(std::move(py));
  EXPECT_EQ(1U, mpy.size());
  EXPECT_FALSE(mpy.empty());
  EXPECT_FALSE(mpy.is_empty());

  mpy.push_back(typename TypeParam::Polygon());
  EXPECT_EQ(2U, mpy.size());

  gis::Nop_visitor visitor;
  EXPECT_FALSE(mpy.accept(&visitor));

  std::unique_ptr<gis::Multipolygon> mpy_new(
      gis::Multipolygon::create_multipolygon(mpy.coordinate_system()));
  EXPECT_FALSE(&mpy == mpy_new.get());
  EXPECT_EQ(gis::Geometry_type::kMultipolygon, mpy_new.get()->type());
  EXPECT_EQ(mpy.coordinate_system(), mpy_new.get()->coordinate_system());
  EXPECT_TRUE(mpy_new.get()->empty());
  EXPECT_TRUE(mpy_new.get()->is_empty());

  std::unique_ptr<typename TypeParam::Multipolygon> mpy_clone(mpy.clone());
  EXPECT_FALSE(&mpy == mpy_clone.get());
  EXPECT_EQ(mpy.type(), mpy_clone.get()->type());
  EXPECT_EQ(mpy.coordinate_system(), mpy_clone.get()->coordinate_system());
  EXPECT_EQ(mpy.size(), mpy_clone.get()->size());
  for (std::size_t i = 0; i < mpy.size(); i++) {
    for (std::size_t j = 0; j < mpy[i].exterior_ring().size(); j++) {
      EXPECT_EQ(mpy[i].exterior_ring()[j].x(),
                (*mpy_clone.get())[i].exterior_ring()[j].x());
      EXPECT_EQ(mpy[i].exterior_ring()[j].y(),
                (*mpy_clone.get())[i].exterior_ring()[j].y());
    }
    for (std::size_t j = 0; j < mpy[i].interior_rings().size(); j++) {
      for (std::size_t k = 0; k < mpy[i].interior_ring(j).size(); k++) {
        EXPECT_EQ(mpy[i].interior_ring(j)[k].x(),
                  (*mpy_clone.get())[i].interior_ring(j)[k].x());
        EXPECT_EQ(mpy[i].interior_ring(j)[k].y(),
                  (*mpy_clone.get())[i].interior_ring(j)[k].y());
      }
    }
  }

  EXPECT_EQ(2U, mpy.front().size());

  mpy.pop_front();
  EXPECT_EQ(1U, mpy.size());
  EXPECT_EQ(0U, mpy.front().size());
}

}  // namespace geometries_unittest

/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_config.h"
#include <gtest/gtest.h>

#include "my_global.h"
#include "gstream.h"
#include "spatial.h"

namespace gis_algo_unittest {

/*
  Testing Gis_polygon_ring::set_ring_order function.
 */
class SetRingOrderTest : public ::testing::Test
{
public:
  SetRingOrderTest() :my_flags(Geometry::wkb_linestring, 0)
  {
    latincc= &my_charset_latin1;
  }
  Geometry *geometry_from_text(const String &wkt, String *wkb,
                               Geometry_buffer *geobuf);
  void set_order_and_compare(const std::string &str, const std::string &str2,
                             bool want_ccw= true);


  const static uint32 srid= 0;
  CHARSET_INFO *latincc;
  String str, str2, wkt, wkt2;
  Geometry_buffer buffer, buffer2;
  Geometry::Flags_t my_flags;
};


Geometry *SetRingOrderTest::geometry_from_text(const String &wkt, String *wkb,
                                               Geometry_buffer *geobuf)
{
  Gis_read_stream trs(wkt.charset(), wkt.ptr(), wkt.length());

  wkb->set_charset(&my_charset_bin);
  wkb->length(0);
  return Geometry::create_from_wkt(geobuf, &trs, wkb, 1);
}

void SetRingOrderTest::set_order_and_compare(const std::string &s1,
                                             const std::string &s2,
                                             bool want_ccw)
{
  wkt.set(s1.c_str(), s1.length(), latincc);
  wkt2.set(s2.c_str(), s2.length(), latincc);

  Gis_polygon_ring *ringp= static_cast<Gis_polygon_ring *>
    (geometry_from_text(wkt, &str, &buffer));
  DBUG_ASSERT(ringp->get_geotype() == Geometry::wkb_linestring);
  Gis_polygon_ring ring(ringp->get_ptr(),
                        ringp->get_nbytes(), my_flags, 0U);
  EXPECT_EQ(ring.set_ring_order(want_ccw), false);


  ringp= static_cast<Gis_polygon_ring *>(geometry_from_text(wkt2, &str2,
                                                            &buffer2));
  DBUG_ASSERT(ringp->get_geotype() == Geometry::wkb_linestring);
  Gis_polygon_ring ring2(ringp->get_ptr(),
                         ringp->get_nbytes(), my_flags, 0U);
  EXPECT_EQ(ring2.set_ring_order(want_ccw), false);

  EXPECT_EQ(str.length(), str2.length());
  EXPECT_EQ(memcmp(str.ptr(), str2.ptr(), str.length()), 0);
}

TEST_F(SetRingOrderTest, SetRingOrderCCW)
{
  SCOPED_TRACE("SetRingOrderCCW");
  std::string geom1("linestring(0 0, 0 1, 1 1, 1 0, 0 0)");
  std::string geom2("linestring(0 0, 1 0, 1 1, 0 1, 0 0)");
  set_order_and_compare(geom1, geom2);
}

TEST_F(SetRingOrderTest, SetRingOrderCW)
{
  SCOPED_TRACE("SetRingOrderCW");
  std::string geom1("linestring(0 0, 0 1, 1 1, 1 0, 0 0)");
  std::string geom2("linestring(0 0, 1 0, 1 1, 0 1, 0 0)");
  set_order_and_compare(geom1, geom2, false);
}

TEST_F(SetRingOrderTest, SetRingOrder2CCW)
{
  SCOPED_TRACE("SetRingOrder2CCW");
  std::string geom3("linestring(0 0, 0 1, 1 0, 0 0)");
  std::string geom4("linestring(0 0, 1 0, 0 1, 0 0)");
  set_order_and_compare(geom3, geom4);
}

TEST_F(SetRingOrderTest, SetRingOrder2CW)
{
  SCOPED_TRACE("SetRingOrder2CW");
  std::string geom3("linestring(0 0, 0 1, 1 0, 0 0)");
  std::string geom4("linestring(0 0, 1 0, 0 1, 0 0)");
  set_order_and_compare(geom3, geom4, false);
}

TEST_F(SetRingOrderTest, DuplicateMinPointBeforeCCW)
{
  SCOPED_TRACE("DuplicateMinPointBeforeCCW");
  std::string geom1("linestring(0 0, 0 1, 1 1, 1 0, 0 0, 0 0, 0 0)");
  std::string geom2("linestring(0 0, 0 0, 0 0, 1 0, 1 1, 0 1, 0 0)");
  set_order_and_compare(geom1, geom2);
}

TEST_F(SetRingOrderTest, DuplicateMinPointBeforeCW)
{
  SCOPED_TRACE("DuplicateMinPointBeforeCW");
  std::string geom1("linestring(0 0, 0 1, 1 1, 1 0, 0 0, 0 0, 0 0)");
  std::string geom2("linestring(0 0, 0 0, 0 0, 1 0, 1 1, 0 1, 0 0)");
  set_order_and_compare(geom1, geom2, false);
}

TEST_F(SetRingOrderTest, DuplicateMinPointAfterCCW)
{
  SCOPED_TRACE("DuplicateMinPointAfterCCW");
  std::string geom1("linestring(0 0, 0 0, 0 0, 0 1, 1 1, 1 0, 0 0)");
  std::string geom2("linestring(0 0, 1 0, 1 1, 0 1, 0 0, 0 0, 0 0)");
  set_order_and_compare(geom1, geom2);
}

TEST_F(SetRingOrderTest, DuplicateMinPointAfterCW)
{
  SCOPED_TRACE("DuplicateMinPointAfterCW");
  std::string geom1("linestring(0 0, 0 0, 0 0, 0 1, 1 1, 1 0, 0 0)");
  std::string geom2("linestring(0 0, 1 0, 1 1, 0 1, 0 0, 0 0, 0 0)");
  set_order_and_compare(geom1, geom2, false);
}

TEST_F(SetRingOrderTest, RingDegradedToPointTest)
{
  SCOPED_TRACE("RingDegradedToPointTest");
  std::string s1("linestring(0 0, 0 0, 0 0, 0 0, 0 0)");
  wkt.set(s1.c_str(), s1.length(), latincc);

  Gis_polygon_ring *ringp= static_cast<Gis_polygon_ring *>
    (geometry_from_text(wkt, &str, &buffer));
  DBUG_ASSERT(ringp->get_geotype() == Geometry::wkb_linestring);
  Gis_polygon_ring ring(ringp->get_ptr(),
                        ringp->get_nbytes(), my_flags, 0U);
  EXPECT_EQ(ring.set_ring_order(true/*CCW*/), true);
}


/*
  Testing functions in Geometry and its children classes that are not covered
  by current BG functionalities.
 */
class GeometryManipulationTest : public SetRingOrderTest
{
public:
};


TEST_F(GeometryManipulationTest, PolygonCopyTest)
{
  SCOPED_TRACE("PolygonCopyTest");
  std::string s1("polygon((0 0, 1 0, 1 1, 0 1, 0 0))");
  wkt.set(s1.c_str(), s1.length(), latincc);

  Gis_polygon *plgn=
    static_cast<Gis_polygon *>(geometry_from_text(wkt, &str, &buffer));
  Gis_polygon plgn1(plgn->get_data_ptr(), plgn->get_data_size(),
                    plgn->get_flags(), plgn->get_srid());
  Gis_polygon plgn2(plgn1);
  Gis_polygon plgn3;

  plgn3= plgn2;

  String wkb3, wkb4, wkb5;
  plgn3.as_wkb(&wkb3, false);
  plgn3.to_wkb_unparsed();
  plgn3.as_wkb(&wkb5, true);
  EXPECT_EQ(wkb3.length(), wkb5.length());
  EXPECT_EQ(memcmp(((char *)wkb3.ptr()) + WKB_HEADER_SIZE,
                   ((char *)wkb5.ptr()) + WKB_HEADER_SIZE,
                   wkb5.length() - WKB_HEADER_SIZE), 0);

  plgn2.as_geometry(&wkb4, false);
  EXPECT_EQ(wkb3.length() + 4, wkb4.length());
  EXPECT_EQ(memcmp(GEOM_HEADER_SIZE + ((char *)wkb4.ptr()),
                   ((char *)wkb3.ptr()) + WKB_HEADER_SIZE,
                   wkb3.length() - WKB_HEADER_SIZE), 0);

  // Check they have identical data. Can only do so in wkb form.
  plgn1.to_wkb_unparsed();
  plgn2.to_wkb_unparsed();
  EXPECT_EQ(plgn1.get_data_size(), plgn2.get_data_size());
  EXPECT_EQ(memcmp(plgn->get_data_ptr(), plgn2.get_data_ptr(),
                   plgn2.get_data_size()), 0);

  EXPECT_EQ(plgn3.get_data_size(), plgn2.get_data_size());
  EXPECT_EQ(memcmp(plgn3.get_data_ptr(), plgn2.get_data_ptr(),
                   plgn2.get_data_size()), 0);
}

TEST_F(GeometryManipulationTest, PolygonManipulationTest)
{
  SCOPED_TRACE("PolygonManipulationTest");
  std::string s1("polygon((0 0, 1 0, 1 1, 0 1, 0 0))");
  std::string s2("multipolygon(((0 0, 1 0, 1 1, 0 1, 0 0)))");
  std::string s3("linestring(0.5 0.25, 0.5 0.75, 0.75 0.75, 0.5 0.25)");
  std::string s4("multipolygon(((0 0, 1 0, 1 1, 0 1, 0 0)),     \
    ((0 0, 1 0, 1 1, 0 1, 0 0), (0.5 0.25, 0.5 0.75, 0.75  0.75, 0.5 0.25)),\
    ((0 0, 1 0, 1 1, 0 1, 0 0), (0.5 0.25, 0.5 0.75, 0.75  0.75, 0.5 0.25)))");
  std::string s5("polygon((0 0, 1 0, 1 1, 0 1, 0 0),\
    (0.5 0.25, 0.5 0.75, 0.75  0.75, 0.5 0.25))");
  wkt.set(s1.c_str(), s1.length(), latincc);
  wkt2.set(s3.c_str(), s3.length(), latincc);

  Gis_polygon *plgn0=
    static_cast<Gis_polygon *>(geometry_from_text(wkt, &str, &buffer));
  Gis_line_string *ls0=
    static_cast<Gis_line_string *>(geometry_from_text(wkt2, &str2, &buffer2));
  Gis_polygon plgn(plgn0->get_data_ptr(), plgn0->get_data_size(),
                   plgn0->get_flags(), plgn0->get_srid());
  Gis_line_string ls(ls0->get_data_ptr(), ls0->get_data_size(),
                     ls0->get_flags(), ls0->get_srid());

  Geometry_buffer buffer3;
  String wkt3, str3;

  wkt3.set(s2.c_str(), s2.length(), latincc);
  Gis_multi_polygon *pmplgn= (static_cast<Gis_multi_polygon *>
                              (geometry_from_text(wkt3, &str3, &buffer3)));
  Gis_multi_polygon mplgn0(pmplgn->get_data_ptr(), pmplgn->get_data_size(),
                           pmplgn->get_flags(), pmplgn->get_srid());
  EXPECT_EQ(mplgn0.size(), 1U);
  Gis_multi_polygon mplgn= mplgn0;

  plgn.inners().resize(1);
  for (int i= 0; i < 4; i++)
    (plgn.inners())[0].push_back(ls[i]);
  mplgn.push_back(plgn);
  plgn.to_wkb_unparsed();

  Geometry_buffer buffer5;
  String wkt5, str5;
  wkt5.set(s5.c_str(), s5.length(), latincc);

  Gis_polygon *plgn20=
    static_cast<Gis_polygon *>(geometry_from_text(wkt5, &str5, &buffer5));
  Gis_polygon plgn2(plgn20->get_data_ptr(), plgn20->get_data_size(),
                    plgn20->get_flags(), plgn20->get_srid());
  EXPECT_EQ(plgn.get_data_size(), plgn2.get_nbytes());

  mplgn.push_back(plgn2);

  plgn2.to_wkb_unparsed();
  EXPECT_EQ(memcmp(plgn.get_data_ptr(), plgn2.get_data_ptr(),
                   plgn2.get_data_size()), 0);


  Geometry_buffer buffer4;
  String wkt4, str4;
  wkt4.set(s4.c_str(), s4.length(), latincc);

  Gis_multi_polygon *mplgn2=
    static_cast<Gis_multi_polygon *>(geometry_from_text(wkt4, &str4, &buffer4));

  EXPECT_EQ(mplgn.get_data_size(), mplgn2->get_data_size());
  EXPECT_EQ(memcmp(mplgn.get_data_ptr(), mplgn2->get_data_ptr(),
                   mplgn2->get_data_size()), 0);
}

}

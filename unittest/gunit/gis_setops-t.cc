/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <gtest/gtest.h>
#include <memory>  // unique_ptr

#include "sql/dd/dd.h"
#include "sql/dd/impl/types/spatial_reference_system_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/setops.h"
#include "unittest/gunit/gis_setops_testshapes.h"
#include "unittest/gunit/gis_test.h"
#include "unittest/gunit/gis_typeset.h"

namespace gis_setops_unittest {

template <typename Types>
struct SetopsTest : Gis_test<Types> {};

TYPED_TEST_SUITE(SetopsTest, gis_typeset::Test_both);

// The purpose of this test is to cover all type combinations, not to check if
// the results are correct.
TYPED_TEST(SetopsTest, CodeCoverage) {
  typename TypeParam::Geometrycollection gc;

  typename TypeParam::Point pt{0.0, 0.0};
  typename TypeParam::Linestring ls = simple_ls<TypeParam>();
  typename TypeParam::Polygon py = base_py<TypeParam>();
  typename TypeParam::Multipoint mpt = simple_mpt<TypeParam>();
  typename TypeParam::Multilinestring mls = simple_mls<TypeParam>();
  typename TypeParam::Multipolygon mpy = simple_mpy<TypeParam>();
  typename TypeParam::Geometrycollection gc_empty;
  typename TypeParam::Geometrycollection gc_inner;
  gc_inner.push_back(pt);

  gc.push_back(gc_empty);
  gc.push_back(gc_inner);
  gc.push_back(pt);
  gc.push_back(ls);
  gc.push_back(py);
  gc.push_back(mpt);
  gc.push_back(mls);
  gc.push_back(mpy);

  for (auto g1 : gc) {
    for (auto g2 : gc) {
      std::unique_ptr<gis::Geometry> result;
      bool is_null = false;
      gis::difference(this->m_srs.get(), g1, g2, "unittest", &result);
      gis::intersection(this->m_srs.get(), g1, g2, "unittest", &result);
      gis::symdifference(this->m_srs.get(), g1, g2, "unittest", &result);
      gis::union_(this->m_srs.get(), g1, g2, "unittest", &result, &is_null);
    }
  }
}

}  // namespace gis_setops_unittest

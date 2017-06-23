/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef UNITTEST_GUNIT_GIS_TEST_H_INCLUDED
#define UNITTEST_GUNIT_GIS_TEST_H_INCLUDED

#include <memory>  // std::unique_ptr

#include <gtest/gtest.h>

#include "sql/dd/types/spatial_reference_system.h"
#include "sql/gis/geometries.h"  // gis::Coordinate_system

#include "unittest/gunit/gis_srs.h"

namespace {

template <typename T_typeset>
struct Gis_test : ::testing::Test {
  std::unique_ptr<dd::Spatial_reference_system_impl> m_srs;

  Gis_test() {
    switch (T_typeset::coordinate_system()) {
      case gis::Coordinate_system::kCartesian:
        // Use SRID 0, leave m_srs empty
        break;
      case gis::Coordinate_system::kGeographic:
        m_srs = gis_srs::swapped_epsg4326();
        break;
    }
  }
};

}  // namespace

#endif  // include guard

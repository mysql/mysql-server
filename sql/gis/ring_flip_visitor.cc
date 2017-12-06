// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, 51 Franklin
// Street, Suite 500, Boston, MA 02110-1335 USA.

#include "sql/gis/ring_flip_visitor.h"

#include <boost/geometry.hpp>  // boost::geometry::correct

#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometries_traits.h"

namespace gis {

bool Ring_flip_visitor::visit_enter(Polygon *py) {
  try {
    switch (py->coordinate_system()) {
      case Coordinate_system::kCartesian:
        boost::geometry::correct(*down_cast<Cartesian_polygon *>(py));
        break;
      case Coordinate_system::kGeographic:
        boost::geometry::correct(*down_cast<Geographic_polygon *>(py),
                                 m_geographic_strategy);
        break;
    }
  } catch (...) {
    m_detected_unknown = true;
  }

  return true;  // Don't descend into each ring.
}

bool Ring_flip_visitor::visit_enter(Multipolygon *mpy) {
  try {
    switch (mpy->coordinate_system()) {
      case Coordinate_system::kCartesian:
        boost::geometry::correct(*down_cast<Cartesian_multipolygon *>(mpy));
        break;
      case Coordinate_system::kGeographic:
        boost::geometry::correct(*down_cast<Geographic_multipolygon *>(mpy),
                                 m_geographic_strategy);
        break;
    }
  } catch (...) {
    m_detected_unknown = true;
  }

  return true;  // Don't descend into each polygon.
}

}  // namespace gis

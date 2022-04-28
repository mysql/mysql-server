// Copyright (c) 2017, 2022, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file
///
/// This file implements the intersection function.

#include <memory>  // std::unique_ptr

#include "sql/gis/intersection_functor.h"
#include "sql/gis/setops.h"
#include "sql/sql_exception_handler.h"  // handle_gis_exception

namespace gis {

bool intersection(const dd::Spatial_reference_system *srs, const Geometry *g1,
                  const Geometry *g2, const char *func_name,
                  std::unique_ptr<Geometry> *result) noexcept {
  try {
    assert(g1 && g2 && g1->coordinate_system() == g2->coordinate_system());
    assert(((srs == nullptr || srs->is_cartesian()) &&
            g1->coordinate_system() == Coordinate_system::kCartesian) ||
           (srs && srs->is_geographic() &&
            g1->coordinate_system() == Coordinate_system::kGeographic));

    Intersection intersection_func(srs ? srs->semi_major_axis() : 0.0,
                                   srs ? srs->semi_minor_axis() : 0.0);
    *result = intersection_func(g1, g2);

    if (result->get()->type() != Geometry_type::kGeometrycollection &&
        result->get()->is_empty()) {
      if (result->get()->coordinate_system() == Coordinate_system::kCartesian) {
        *result = std::make_unique<Cartesian_geometrycollection>();
      } else {
        *result = std::make_unique<Geographic_geometrycollection>();
      }
    }

  } catch (...) {
    handle_gis_exception(func_name);
    return true;
  }
  return false;
}

}  // namespace gis

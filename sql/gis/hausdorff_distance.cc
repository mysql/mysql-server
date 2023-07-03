// Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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
/// This file implements the discrete Hausdorff distance functor and function.

#include <cmath>  // std::isfinite

#include <boost/geometry.hpp>
#include "my_inttypes.h"                            // MYF
#include "my_sys.h"                                 // my_error
#include "mysqld_error.h"                           // Error codes
#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/hausdorff_distance.h"
#include "sql/gis/hausdorff_distance_functor.h"
#include "sql/sql_exception_handler.h"  // handle_gis_exception

namespace bg = boost::geometry;
namespace bgs = boost::geometry::srs;
namespace bgsd = boost::geometry::strategy::distance;

namespace gis {

Hausdorff_distance::Hausdorff_distance(double major, double minor)
    : m_geographic_strategy(
          new bgsd::geographic<boost::geometry::strategy::andoyer,
                               bgs::spheroid<double>>(
              bgs::spheroid<double>(major, minor))) {}

double Hausdorff_distance::operator()(const Geometry *g1,
                                      const Geometry *g2) const {
  return apply(*this, g1, g2);
}

double Hausdorff_distance::eval(const Geometry *g1, const Geometry *g2) const {
  throw not_implemented_exception::for_non_projected(*g1, *g2);
}

double Hausdorff_distance::eval(const Cartesian_point *g1,
                                const Cartesian_multipoint *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2);
}

double Hausdorff_distance::eval(const Cartesian_multipoint *g1,
                                const Cartesian_point *g2) const {
  return bg::discrete_hausdorff_distance(*g2, *g1);
}

double Hausdorff_distance::eval(const Cartesian_linestring *g1,
                                const Cartesian_linestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2);
}

double Hausdorff_distance::eval(const Cartesian_multipoint *g1,
                                const Cartesian_multipoint *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2);
}

double Hausdorff_distance::eval(const Cartesian_linestring *g1,
                                const Cartesian_multilinestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2);
}

double Hausdorff_distance::eval(const Cartesian_multilinestring *g1,
                                const Cartesian_linestring *g2) const {
  return bg::discrete_hausdorff_distance(*g2, *g1);
}

double Hausdorff_distance::eval(const Cartesian_multilinestring *g1,
                                const Cartesian_multilinestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2);
}

double Hausdorff_distance::eval(const Geographic_point *g1,
                                const Geographic_multipoint *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_multipoint *g1,
                                const Geographic_point *g2) const {
  return bg::discrete_hausdorff_distance(*g2, *g1, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_linestring *g1,
                                const Geographic_linestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_multipoint *g1,
                                const Geographic_multipoint *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_linestring *g1,
                                const Geographic_multilinestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_multilinestring *g1,
                                const Geographic_linestring *g2) const {
  return bg::discrete_hausdorff_distance(*g2, *g1, *m_geographic_strategy);
}

double Hausdorff_distance::eval(const Geographic_multilinestring *g1,
                                const Geographic_multilinestring *g2) const {
  return bg::discrete_hausdorff_distance(*g1, *g2, *m_geographic_strategy);
}

/////////////////////////////////////////////////////////////////////////////

bool hausdorff_distance(const dd::Spatial_reference_system *srs,
                        const Geometry *g1, const Geometry *g2,
                        const char *func_name, double *hausdorff_distance,
                        bool *is_null) noexcept {
  try {
    assert(g1->coordinate_system() == g2->coordinate_system());
    assert(srs == nullptr ||
           ((srs->is_cartesian() &&
             g1->coordinate_system() == Coordinate_system::kCartesian) ||
            (srs->is_geographic() &&
             g1->coordinate_system() == Coordinate_system::kGeographic)));

    if ((*is_null = (g1->is_empty() || g2->is_empty()))) return false;

    Hausdorff_distance hd(srs ? srs->semi_major_axis() : 0.0,
                          srs ? srs->semi_minor_axis() : 0.0);
    *hausdorff_distance = hd(g1, g2);
  } catch (...) {
    handle_gis_exception(func_name);
    return true;
  }

  if (!std::isfinite(*hausdorff_distance) || *hausdorff_distance < 0.0) {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "Hausdorff_distance", func_name);
    return true;
  }

  return false;
}

}  // namespace gis

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

/// @file
///
/// This file implements the distance functor and function.

#include "distance.h"
#include "distance_functor.h"

#include <cmath>  // std::isfinite
#include <limits>

#include <boost/geometry.hpp>  // boost::geometry::distance

#include "dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "geometries.h"
#include "geometries_traits.h"
#include "my_inttypes.h"            // MYF
#include "my_sys.h"                 // my_error
#include "mysqld_error.h"           // Error codes
#include "sql_exception_handler.h"  // handle_gis_exception

namespace bg = boost::geometry;
namespace bgs = boost::geometry::srs;
namespace bgsd = boost::geometry::strategy::distance;

namespace gis {

/// Apply a Functor<double> to two geometries, which both may be geometry
/// collections, and return the minimum result of the functor applied on each
/// combination of elements in the collections.
///
/// @tparam GC Coordinate specific gometry collection type.
///
/// @param f Functor to apply.
/// @param g1 First geometry.
/// @param g2 Second geometry.
///
/// @return The minimum result of f(g1_i, g2_j), for all g1_i, element of g1,
/// and g2_j, element of g2.
template <typename GC>
static double geometry_collection_apply_min(Functor<double> *f,
                                            const Geometry *g1,
                                            const Geometry *g2) {
  double min = std::numeric_limits<double>::infinity();

  if (g1->type() == Geometry_type::kGeometrycollection) {
    const auto gc1 = down_cast<const GC *>(g1);
    for (const auto g1_i : *gc1) {
      if (g2->type() == Geometry_type::kGeometrycollection) {
        const auto gc2 = down_cast<const GC *>(g2);
        for (const auto g2_j : *gc2) {
          double res = geometry_collection_apply_min<GC>(f, g1_i, g2_j);
          if (res < min) min = res;
        }
      } else {
        double res = geometry_collection_apply_min<GC>(f, g1_i, g2);
        if (res < min) min = res;
      }
    }
  } else {
    if (g2->type() == Geometry_type::kGeometrycollection) {
      const auto gc2 = down_cast<const GC *>(g2);
      for (const auto g2_j : *gc2) {
        double res = geometry_collection_apply_min<GC>(f, g1, g2_j);
        if (res < min) min = res;
      }
    } else {
      double res = (*f)(g1, g2);
      if (res < min) min = res;
    }
  }

  return min;
}

Distance::Distance(double major, double minor) {
  m_geographic_strategy.reset(new bgsd::andoyer<bgs::spheroid<double>>(
      bgs::spheroid<double>(major, minor)));
}

double Distance::operator()(const Geometry *g1, const Geometry *g2) {
  return apply(*this, g1, g2);
}

double Distance::eval(const Geometry *g1, const Geometry *g2) {
  // Not all geographic type combinations have been implemented.
  DBUG_ASSERT(g1->coordinate_system() == Coordinate_system::kGeographic);
  throw not_implemented_exception(g1->coordinate_system(), g1->type(),
                                  g2->type());
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_point, *)

double Distance::eval(const Cartesian_point *g1, const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_point *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_point *g1, const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_point *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_point *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_point *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_point *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_linestring, *)

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_linestring *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_polygon, *)

double Distance::eval(const Cartesian_polygon *g1, const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_polygon *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_geometrycollection, *)

double Distance::eval(const Cartesian_geometrycollection *g1,
                      const Geometry *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_multipoint, *)

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipoint *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_multilinestring, *)

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multilinestring *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Cartesian_multipolygon, *)

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_point *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_linestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_polygon *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_geometrycollection *g2) {
  return geometry_collection_apply_min<Cartesian_geometrycollection>(this, g1,
                                                                     g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_multipoint *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_multilinestring *g2) {
  return bg::distance(*g1, *g2);
}

double Distance::eval(const Cartesian_multipolygon *g1,
                      const Cartesian_multipolygon *g2) {
  return bg::distance(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Geographic_point, *)

double Distance::eval(const Geographic_point *g1, const Geographic_point *g2) {
  return bg::distance(*g1, *g2, *m_geographic_strategy);
}

double Distance::eval(const Geographic_point *g1,
                      const Geographic_multipoint *g2) {
  return bg::distance(*g1, *g2, *m_geographic_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// distance(Geographic_multipoint, *)

double Distance::eval(const Geographic_multipoint *g1,
                      const Geographic_point *g2) {
  return bg::distance(*g1, *g2, *m_geographic_strategy);
}

//////////////////////////////////////////////////////////////////////////////

bool distance(const dd::Spatial_reference_system *srs, const Geometry *g1,
              const Geometry *g2, double *distance, bool *null) noexcept {
  try {
    DBUG_ASSERT(g1->coordinate_system() == g2->coordinate_system());
    DBUG_ASSERT(srs == nullptr ||
                ((srs->is_cartesian() &&
                  g1->coordinate_system() == Coordinate_system::kCartesian) ||
                 (srs->is_geographic() &&
                  g1->coordinate_system() == Coordinate_system::kGeographic)));

    if ((*null = (g1->is_empty() || g2->is_empty()))) return false;

    Distance dist(srs ? srs->semi_major_axis() : 0.0,
                  srs ? srs->semi_minor_axis() : 0.0);
    *distance = dist(g1, g2);
  } catch (...) {
    handle_gis_exception("st_distance");
    return true;
  }

  if (!std::isfinite(*distance) || *distance < 0) {
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_distance");
    return true;
  }

  return false;
}

}  // namespace gis

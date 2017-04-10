#ifndef GIS__DISTANCE_FUNCTOR_H_INCLUDED
#define GIS__DISTANCE_FUNCTOR_H_INCLUDED

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
/// This file declares the distance functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::distance() function.
///
/// @see gis::distance

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>

#include "functor.h"
#include "geometries.h"

namespace gis {

/// Distance functor that calls Boost.Geometry with the correct parameter types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement distance or other geographic functions. It should not be used
/// directly by other MySQL code.
class Distance : public Functor<double> {
 private:
  std::unique_ptr<boost::geometry::strategy::distance::andoyer<
      boost::geometry::srs::spheroid<double>>>
      m_geographic_strategy;

 public:
  Distance(double major, double minor);
  double operator()(const Geometry *g1, const Geometry *g2) override;
  double eval(const Geometry *g1, const Geometry *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_point, *)

  double eval(const Cartesian_point *g1, const Cartesian_point *g2);
  double eval(const Cartesian_point *g1, const Cartesian_linestring *g2);
  double eval(const Cartesian_point *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_point *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_point *g1, const Cartesian_multipoint *g2);
  double eval(const Cartesian_point *g1, const Cartesian_multilinestring *g2);
  double eval(const Cartesian_point *g1, const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_linestring, *)

  double eval(const Cartesian_linestring *g1, const Cartesian_point *g2);
  double eval(const Cartesian_linestring *g1, const Cartesian_linestring *g2);
  double eval(const Cartesian_linestring *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_linestring *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_linestring *g1, const Cartesian_multipoint *g2);
  double eval(const Cartesian_linestring *g1,
              const Cartesian_multilinestring *g2);
  double eval(const Cartesian_linestring *g1, const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_polygon, *)

  double eval(const Cartesian_polygon *g1, const Cartesian_point *g2);
  double eval(const Cartesian_polygon *g1, const Cartesian_linestring *g2);
  double eval(const Cartesian_polygon *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_polygon *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_polygon *g1, const Cartesian_multipoint *g2);
  double eval(const Cartesian_polygon *g1, const Cartesian_multilinestring *g2);
  double eval(const Cartesian_polygon *g1, const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_geometrycollection, *)

  double eval(const Cartesian_geometrycollection *g1, const Geometry *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_multipoint, *)

  double eval(const Cartesian_multipoint *g1, const Cartesian_point *g2);
  double eval(const Cartesian_multipoint *g1, const Cartesian_linestring *g2);
  double eval(const Cartesian_multipoint *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_multipoint *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_multipoint *g1, const Cartesian_multipoint *g2);
  double eval(const Cartesian_multipoint *g1,
              const Cartesian_multilinestring *g2);
  double eval(const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_multilinestring, *)

  double eval(const Cartesian_multilinestring *g1, const Cartesian_point *g2);
  double eval(const Cartesian_multilinestring *g1,
              const Cartesian_linestring *g2);
  double eval(const Cartesian_multilinestring *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_multilinestring *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_multilinestring *g1,
              const Cartesian_multipoint *g2);
  double eval(const Cartesian_multilinestring *g1,
              const Cartesian_multilinestring *g2);
  double eval(const Cartesian_multilinestring *g1,
              const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Cartesian_multipolygon, *)

  double eval(const Cartesian_multipolygon *g1, const Cartesian_point *g2);
  double eval(const Cartesian_multipolygon *g1, const Cartesian_linestring *g2);
  double eval(const Cartesian_multipolygon *g1, const Cartesian_polygon *g2);
  double eval(const Cartesian_multipolygon *g1,
              const Cartesian_geometrycollection *g2);
  double eval(const Cartesian_multipolygon *g1, const Cartesian_multipoint *g2);
  double eval(const Cartesian_multipolygon *g1,
              const Cartesian_multilinestring *g2);
  double eval(const Cartesian_multipolygon *g1,
              const Cartesian_multipolygon *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Geographic_point, *)

  double eval(const Geographic_point *g1, const Geographic_point *g2);
  double eval(const Geographic_point *g1, const Geographic_multipoint *g2);

  //////////////////////////////////////////////////////////////////////////////

  // distance(Geographic_multipoint, *)

  double eval(const Geographic_multipoint *g1, const Geographic_point *g2);
};

}  // namespace gis

#endif  // GIS__DISTANCE_FUNCTOR_H_INCLUDED

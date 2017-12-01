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
/// Declares the Distance_sphere functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::distance_sphere() function.
///
/// @see gis::distance_sphere

#ifndef SQL_GIS_DISTANCE_SPHERE_FUNCTOR_H_INCLUDED
#define SQL_GIS_DISTANCE_SPHERE_FUNCTOR_H_INCLUDED

#include <boost/geometry.hpp>

#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"     // gis::Geometry
#include "sql/gis/geometries_cs.h"  // gis::{Cartesian_*, Geographic_*}

namespace gis {

/// Functor that calls Boost.Geometry with the correct parameter types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement geographic functions. It should not be used directly by other
/// MySQL code.
class Distance_sphere : public Functor<double> {
  boost::geometry::strategy::distance::haversine<double> m_strategy;

 public:
  Distance_sphere(double sphere_radius) : m_strategy{sphere_radius} {};

  double operator()(const Geometry* g1, const Geometry* g2) const override;

  double eval(const Cartesian_point* g1, const Cartesian_point* g2) const;
  double eval(const Cartesian_point* g1, const Cartesian_multipoint* g2) const;
  double eval(const Cartesian_multipoint* g1, const Cartesian_point* g2) const;
  double eval(const Cartesian_multipoint* g1,
              const Cartesian_multipoint* g2) const;

  double eval(const Geographic_point* g1, const Geographic_point* g2) const;
  double eval(const Geographic_point* g1,
              const Geographic_multipoint* g2) const;
  double eval(const Geographic_multipoint* g1,
              const Geographic_point* g2) const;
  double eval(const Geographic_multipoint* g1,
              const Geographic_multipoint* g2) const;

  double eval(const Geometry* g1, const Geometry* g2) const;
};

}  // namespace gis

#endif  // SQL_GIS_DISTANCE_SPHERE_FUNCTOR_H_INCLUDED

// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
/// Declares the area functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::area() function.
///
/// @see gis::area

#ifndef SQL_GIS_AREA_FUNCTOR_H_INCLUDED
#define SQL_GIS_AREA_FUNCTOR_H_INCLUDED

#include <boost/geometry.hpp>  // boost::geometry

#include "sql/gis/functor.h"        // gis::Unary_functor
#include "sql/gis/geometries.h"     // gis::Geometry
#include "sql/gis/geometries_cs.h"  // gis::{Cartesian_*,Geographic_*}

namespace gis {

/// Area functor that calls boost::geometry::area with the correct parameter
/// types.
///
/// The functor may throw exceptions. It is intended for implementing geographic
/// functions. It should not be used directly by other MySQL code.
class Area : public Unary_functor<double> {
  double m_semi_major;
  double m_semi_minor;

  boost::geometry::strategy::area::geographic<> m_geographic_strategy;

 public:
  Area();
  Area(double semi_major, double semi_minor);

  double operator()(const Geometry &g) const override;

  double eval(const Cartesian_polygon &g) const;
  double eval(const Cartesian_multipolygon &g) const;

  double eval(const Geographic_polygon &g) const;
  double eval(const Geographic_multipolygon &g) const;

  double eval(const Geometry &g) const;
};

}  // namespace gis

#endif  // SQL_GIS_AREA_FUNCTOR_H_INCLUDED

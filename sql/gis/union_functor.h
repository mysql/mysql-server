#ifndef SQL_GIS_UNION_FUNCTOR_H_INCLUDED
#define SQL_GIS_UNION_FUNCTOR_H_INCLUDED

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
/// This file declares the union functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::union() function.
///
/// @see gis::union

#include <boost/geometry.hpp>

#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"

namespace gis {

/// Union functor that calls Boost.Geometry with the correct parameter
/// types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement union or other geographic functions. It should not be used
/// directly by other MySQL code.
class Union : public Functor<Geometry *> {
 private:
  /// Semi-major axis of ellipsoid.
  double m_semi_major;
  /// Semi-minor axis of ellipsoid.
  double m_semi_minor;
  /// Strategy used for P/L and P/A.
  boost::geometry::strategy::within::geographic_winding<Geographic_point>
      m_geographic_pl_pa_strategy;
  /// Strategy used for L/L, L/A and A/A.
  boost::geometry::strategy::intersection::geographic_segments<>
      m_geographic_ll_la_aa_strategy;

 public:
  Union(double semi_major, double semi_minor);
  Geometry *operator()(const Geometry *g1, const Geometry *g2) const override;
  Geometry *eval(const Geometry *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // union(Cartesian_multilinestring, *)

  Geometry *eval(const Cartesian_multilinestring *g1,
                 const Cartesian_linestring *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // union(Cartesian_multipolygon, *)

  Geometry *eval(const Cartesian_multipolygon *g1,
                 const Cartesian_polygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // union(Geographic_multilinestring, *)

  Geometry *eval(const Geographic_multilinestring *g1,
                 const Geographic_linestring *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // union(Geographic_multipolygon, *)

  Geometry *eval(const Geographic_multipolygon *g1,
                 const Geographic_polygon *g2) const;
};

}  // namespace gis

#endif  // SQL_GIS_UNION_FUNCTOR_H_INCLUDED

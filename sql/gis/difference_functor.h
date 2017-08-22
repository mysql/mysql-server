#ifndef GIS__DIFFERENCE_FUNCTOR_H_INCLUDED
#define GIS__DIFFERENCE_FUNCTOR_H_INCLUDED

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
/// This file declares the difference functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::difference() function.
///
/// @see gis::difference

#include <boost/geometry.hpp>

#include "functor.h"
#include "geometries.h"
#include "geometries_traits.h"

namespace gis {

/// Difference functor that calls Boost.Geometry with the correct parameter
/// types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement difference or other geographic functions. It should not be used
/// directly by other MySQL code.
class Difference : public Functor<Geometry *> {
 private:
  /// Semi-major axis of ellipsoid.
  double m_semi_major;
  /// Semi-minor axis of ellipsoid.
  double m_semi_minor;
  /// Strategy used for P/L and P/A.
  boost::geometry::strategy::within::winding<
      Geographic_point, Geographic_point,
      boost::geometry::strategy::side::geographic<>>
      m_geographic_pl_pa_strategy;
  /// Strategy used for L/L, L/A and A/A.
  boost::geometry::strategy::intersection::geographic_segments<>
      m_geographic_ll_la_aa_strategy;

 public:
  Difference(double semi_major, double semi_minor);
  Geometry *operator()(const Geometry *g1, const Geometry *g2) const override;
  Geometry *eval(const Geometry *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Cartesian_linestring, *)

  Geometry *eval(const Cartesian_linestring *g1,
                 const Cartesian_multilinestring *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Cartesian_multipoint, *)

  Geometry *eval(const Cartesian_multipoint *g1,
                 const Cartesian_multipoint *g2) const;
  Geometry *eval(const Cartesian_multipoint *g1,
                 const Cartesian_multilinestring *g2) const;
  Geometry *eval(const Cartesian_multipoint *g1,
                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Cartesian_multilinestring, *)

  Geometry *eval(const Cartesian_multilinestring *g1,
                 const Cartesian_multilinestring *g2) const;
  Geometry *eval(const Cartesian_multilinestring *g1,
                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Cartesian_multipolygon, *)

  Geometry *eval(const Cartesian_multipolygon *g1,
                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Geographic_linestring, *)

  Geometry *eval(const Geographic_linestring *g1,
                 const Geographic_multilinestring *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Geographic_multipoint, *)

  Geometry *eval(const Geographic_multipoint *g1,
                 const Geographic_multipoint *g2) const;
  Geometry *eval(const Geographic_multipoint *g1,
                 const Geographic_multilinestring *g2) const;
  Geometry *eval(const Geographic_multipoint *g1,
                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Geographic_multilinestring, *)

  Geometry *eval(const Geographic_multilinestring *g1,
                 const Geographic_multilinestring *g2) const;
  Geometry *eval(const Geographic_multilinestring *g1,
                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // difference(Geographic_multipolygon, *)

  Geometry *eval(const Geographic_multipolygon *g1,
                 const Geographic_multipolygon *g2) const;
};

}  // namespace gis

#endif  // GIS__DIFFERENCE_FUNCTOR_H_INCLUDED

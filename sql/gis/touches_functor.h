#ifndef SQL_GIS_TOUCHES_FUNCTOR_H_INCLUDED
#define SQL_GIS_TOUCHES_FUNCTOR_H_INCLUDED

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
/// This file declares the touches functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::touches() function.
///
/// @see gis::touches

#include <boost/geometry.hpp>

#include "sql/gis/box.h"
#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"

namespace gis {

/// Touches functor that calls Boost.Geometry with the correct parameter
/// types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement touches or other geographic functions. It should not be used
/// directly by other MySQL code.
class Touches : public Functor<bool> {
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
  /// Creates a new Touches functor.
  ///
  /// @param semi_major Semi-major axis of ellipsoid.
  /// @param semi_minor Semi-minor axis of ellipsoid.
  Touches(double semi_major, double semi_minor);
  double semi_major() const { return m_semi_major; }
  double semi_minor() const { return m_semi_minor; }
  bool operator()(const Geometry *g1, const Geometry *g2) const override;
  bool operator()(const Box *b1, const Box *b2) const;
  bool eval(const Geometry *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_point, *)

  bool eval(const Cartesian_point *g1, const Cartesian_point *g2) const;
  bool eval(const Cartesian_point *g1, const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_point *g1, const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_point *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_point *g1, const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_point *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_point *g1, const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_linestring, *)

  bool eval(const Cartesian_linestring *g1, const Cartesian_point *g2) const;
  bool eval(const Cartesian_linestring *g1,
            const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_linestring *g1, const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_linestring *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_linestring *g1,
            const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_linestring *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_linestring *g1,
            const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_polygon, *)

  bool eval(const Cartesian_polygon *g1, const Cartesian_point *g2) const;
  bool eval(const Cartesian_polygon *g1, const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_polygon *g1, const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_polygon *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_polygon *g1, const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_polygon *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_polygon *g1,
            const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_geometrycollection, *)

  bool eval(const Cartesian_geometrycollection *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_multipoint, *)

  bool eval(const Cartesian_multipoint *g1, const Cartesian_point *g2) const;
  bool eval(const Cartesian_multipoint *g1,
            const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_multipoint *g1, const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_multipoint *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_multipoint *g1,
            const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_multipoint *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_multipoint *g1,
            const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_multilinestring, *)

  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_point *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_multilinestring *g1,
            const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Cartesian_multipolygon, *)

  bool eval(const Cartesian_multipolygon *g1, const Cartesian_point *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_linestring *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_polygon *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_geometrycollection *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_multipoint *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_multilinestring *g2) const;
  bool eval(const Cartesian_multipolygon *g1,
            const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_point, *)

  bool eval(const Geographic_point *g1, const Geographic_point *g2) const;
  bool eval(const Geographic_point *g1, const Geographic_linestring *g2) const;
  bool eval(const Geographic_point *g1, const Geographic_polygon *g2) const;
  bool eval(const Geographic_point *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_point *g1, const Geographic_multipoint *g2) const;
  bool eval(const Geographic_point *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_point *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_linestring, *)

  bool eval(const Geographic_linestring *g1, const Geographic_point *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_linestring *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_polygon *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_multipoint *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_linestring *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_polygon, *)

  bool eval(const Geographic_polygon *g1, const Geographic_point *g2) const;
  bool eval(const Geographic_polygon *g1,
            const Geographic_linestring *g2) const;
  bool eval(const Geographic_polygon *g1, const Geographic_polygon *g2) const;
  bool eval(const Geographic_polygon *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_polygon *g1,
            const Geographic_multipoint *g2) const;
  bool eval(const Geographic_polygon *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_polygon *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_geometrycollection, *)

  bool eval(const Geographic_geometrycollection *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_multipoint, *)

  bool eval(const Geographic_multipoint *g1, const Geographic_point *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_linestring *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_polygon *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_multipoint *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_multipoint *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_multilinestring, *)

  bool eval(const Geographic_multilinestring *g1,
            const Geographic_point *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_linestring *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_polygon *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_multipoint *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_multilinestring *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Geographic_multipolygon, *)

  bool eval(const Geographic_multipolygon *g1,
            const Geographic_point *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_linestring *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_polygon *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_geometrycollection *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_multipoint *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_multilinestring *g2) const;
  bool eval(const Geographic_multipolygon *g1,
            const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // touches(Box, Box)

  bool eval(const Cartesian_box *b1, const Cartesian_box *b2) const;
  bool eval(const Geographic_box *b1, const Geographic_box *b2) const;
};

}  // namespace gis

#endif  // SQL_GIS_TOUCHES_FUNCTOR_H_INCLUDED

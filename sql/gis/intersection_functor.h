#ifndef SQL_GIS_INTERSECTION_FUNCTOR_H_INCLUDED
#define SQL_GIS_INTERSECTION_FUNCTOR_H_INCLUDED

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
/// This file declares the intersection functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::intersection() function.
///
/// @see gis::intersection

#include <boost/geometry.hpp>

#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"

namespace gis {

/// Intersection functor that calls Boost.Geometry with the correct parameter
/// types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement intersection or other geographic functions. It should not be used
/// directly by other MySQL code.
class Intersection : public Functor<std::unique_ptr<Geometry>> {
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
  Intersection(double semi_major, double semi_minor);
  double semi_minor() const { return m_semi_minor; }
  double semi_major() const { return m_semi_major; }
  std::unique_ptr<Geometry> operator()(const Geometry *g1,
                                       const Geometry *g2) const override;
  std::unique_ptr<Geometry> eval(const Geometry *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_point, *)

  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_linestring, *)

  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_polygon, *)

  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_geometrycollection, *)

  std::unique_ptr<Geometry> eval(const Cartesian_geometrycollection *g1,
                                 const Geometry *g2) const;
  std::unique_ptr<Geometry> eval(const Geometry *g1,
                                 const Cartesian_geometrycollection *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_geometrycollection *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_multipoint, *)

  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_multilinestring, *)

  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Cartesian_multipolygon, *)

  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_point, *)

  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_linestring, *)

  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_polygon, *)

  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_geometrycollection, *)

  std::unique_ptr<Geometry> eval(const Geographic_geometrycollection *g1,
                                 const Geographic_geometrycollection *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_geometrycollection *g1,
                                 const Geometry *g2) const;
  std::unique_ptr<Geometry> eval(const Geometry *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_multipoint, *)

  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_multilinestring, *)

  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_multipolygon *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // intersection(Geographic_multipolygon, *)

  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_multipolygon *g2) const;
};

}  // namespace gis

#endif  // SQL_GIS_INTERSECTION_FUNCTOR_H_INCLUDED

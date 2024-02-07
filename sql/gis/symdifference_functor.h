#ifndef SQL_GIS_SYMDIFFERENCE_FUNCTOR_H_INCLUDED
#define SQL_GIS_SYMDIFFERENCE_FUNCTOR_H_INCLUDED

// Copyright (c) 2021, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
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
/// This file declares the symdifference functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::symdifference() function.
///
/// @see gis::symdifference

#include <boost/geometry.hpp>

#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"

namespace gis {

/// SymDifference functor that calls Boost.Geometry with the correct parameter
/// types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement symdifference or other geographic functions. It should not be used
/// directly by other MySQL code.
class SymDifference : public Functor<std::unique_ptr<Geometry>> {
 private:
  /// Semi-major axis of ellipsoid.
  double m_semi_major;
  /// Semi-minor axis of ellipsoid.
  double m_semi_minor;
  /// Strategy used for P/L and P/A.
  boost::geometry::strategy::within::geographic_winding<>
      m_geographic_pl_pa_strategy;
  /// Strategy used for L/L, L/A and A/A.
  boost::geometry::strategy::intersection::geographic_segments<>
      m_geographic_ll_la_aa_strategy;

 public:
  SymDifference(double semi_major, double semi_minor);
  double semi_minor() const { return m_semi_minor; }
  double semi_major() const { return m_semi_major; }
  auto get_pl_pa_strategy() const { return m_geographic_pl_pa_strategy; }
  boost::geometry::strategy::intersection::geographic_segments<>
  get_ll_la_aa_strategy() const {
    return m_geographic_ll_la_aa_strategy;
  }
  std::unique_ptr<Geometry> operator()(const Geometry *g1,
                                       const Geometry *g2) const override;
  std::unique_ptr<Geometry> eval(const Geometry *g1, const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_point, *)

  std::unique_ptr<Cartesian_multipoint> eval(const Cartesian_point *g1,
                                             const Cartesian_point *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_point *g1, const Cartesian_linestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_point *g1, const Cartesian_polygon *g2) const;
  std::unique_ptr<Cartesian_multipoint> eval(
      const Cartesian_point *g1, const Cartesian_multipoint *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_point *g1, const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_point *g1, const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_point *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_linestring, *)

  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Cartesian_multilinestring> eval(
      const Cartesian_linestring *g1, const Cartesian_linestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_linestring *g1, const Cartesian_polygon *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_linestring *g1, const Cartesian_multipoint *g2) const;
  std::unique_ptr<Cartesian_multilinestring> eval(
      const Cartesian_linestring *g1,
      const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_linestring *g1, const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_polygon, *)

  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Cartesian_multipolygon> eval(
      const Cartesian_polygon *g1, const Cartesian_polygon *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_polygon *g1, const Cartesian_multipoint *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_polygon *g1, const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Cartesian_multipolygon> eval(
      const Cartesian_polygon *g1, const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_geometrycollection, *)

  std::unique_ptr<Geometry> eval(const Cartesian_geometrycollection *g1,
                                 const Cartesian_geometrycollection *g2) const;

  std::unique_ptr<Geometry> eval(const Cartesian_geometrycollection *g1,
                                 const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_multipoint, *)

  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Cartesian_multipoint> eval(
      const Cartesian_multipoint *g1, const Cartesian_multipoint *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_multipoint *g1,
      const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_multilinestring, *)

  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_point *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_multipoint *g2) const;
  std::unique_ptr<Cartesian_multilinestring> eval(
      const Cartesian_multilinestring *g1,
      const Cartesian_multilinestring *g2) const;
  std::unique_ptr<Cartesian_geometrycollection> eval(
      const Cartesian_multilinestring *g1,
      const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Cartesian_multipolygon, *)

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
  std::unique_ptr<Cartesian_multipolygon> eval(
      const Cartesian_multipolygon *g1, const Cartesian_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon *g1,
                                 const Cartesian_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_point, *)

  std::unique_ptr<Geographic_multipoint> eval(const Geographic_point *g1,
                                              const Geographic_point *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_point *g1, const Geographic_linestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_point *g1, const Geographic_polygon *g2) const;
  std::unique_ptr<Geographic_multipoint> eval(
      const Geographic_point *g1, const Geographic_multipoint *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_point *g1, const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_point *g1, const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_point *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_linestring, *)

  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geographic_multilinestring> eval(
      const Geographic_linestring *g1, const Geographic_linestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_linestring *g1, const Geographic_polygon *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_linestring *g1, const Geographic_multipoint *g2) const;
  std::unique_ptr<Geographic_multilinestring> eval(
      const Geographic_linestring *g1,
      const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_linestring *g1, const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_polygon, *)

  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geographic_multipolygon> eval(
      const Geographic_polygon *g1, const Geographic_polygon *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_polygon *g1, const Geographic_multipoint *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_polygon *g1, const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geographic_multipolygon> eval(
      const Geographic_polygon *g1, const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_polygon *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_geometrycollection, *)

  std::unique_ptr<Geometry> eval(const Geographic_geometrycollection *g1,
                                 const Geographic_geometrycollection *g2) const;

  std::unique_ptr<Geometry> eval(const Geographic_geometrycollection *g1,
                                 const Geometry *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_multipoint, *)

  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geographic_multipoint> eval(
      const Geographic_multipoint *g1, const Geographic_multipoint *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_multipoint *g1,
      const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_multipoint *g1, const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipoint *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_multilinestring, *)

  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_point *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_linestring *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_polygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_multipoint *g2) const;
  std::unique_ptr<Geographic_multilinestring> eval(
      const Geographic_multilinestring *g1,
      const Geographic_multilinestring *g2) const;
  std::unique_ptr<Geographic_geometrycollection> eval(
      const Geographic_multilinestring *g1,
      const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multilinestring *g1,
                                 const Geographic_geometrycollection *g2) const;

  //////////////////////////////////////////////////////////////////////////////

  // symdifference(Geographic_multipolygon, *)

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
  std::unique_ptr<Geographic_multipolygon> eval(
      const Geographic_multipolygon *g1,
      const Geographic_multipolygon *g2) const;
  std::unique_ptr<Geometry> eval(const Geographic_multipolygon *g1,
                                 const Geographic_geometrycollection *g2) const;
};

}  // namespace gis

#endif  // SQL_GIS_SYMDIFFERENCE_FUNCTOR_H_INCLUDED

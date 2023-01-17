#ifndef SQL_GIS_BUFFER_FUNCTOR_H_INCLUDED
#define SQL_GIS_BUFFER_FUNCTOR_H_INCLUDED

// Copyright (c) 2021, 2023, Oracle and/or its affiliates.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License, version 2.0,
//  as published by the Free Software Foundation.
//
//  This program is also distributed with certain software (including
//  but not limited to OpenSSL) that is licensed under separate terms,
//  as designated in a particular file or component or in included license
//  documentation.  The authors of MySQL hereby grant you an additional
//  permission to link the program and your derivative works with the
//  separately licensed software that they have included with MySQL.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License, version 2.0, for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

/// @file
///
/// This file declares the buffer functor interface.
///
/// The functor is not intended for use directly by MySQL code. It should be
/// used indirectly through the gis::buffer() function.
///
/// @see gis::buffer

#include <boost/geometry/strategies/strategies.hpp>  // bg::strategy::buffer::*
#include <memory>                                    // std::unique_ptr
#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/functor.h"                        // gis::Unary_functor
#include "sql/gis/geometries.h"                     // gis::Geometry
#include "sql/gis/geometries_cs.h"  // gis::{Cartesian_*,Geographic_point}

namespace bg = boost::geometry;

namespace gis {

/// Buffer functor that calls boost::geometry::buffer with correct geometry
/// type and strategy combination.
///
/// The functor may throw exceptions. It is intended for implementing geographic
/// functions. It should not be used directly by other MySQL code.
class Buffer : public Unary_functor<std::unique_ptr<Geometry>> {
 private:
  const dd::Spatial_reference_system *m_srs;
  const BufferStrategies &strats;
  bg::strategy::buffer::distance_symmetric<double> d_symmetric;
  bg::strategy::buffer::side_straight s_straight;

  bg::strategy::buffer::join_round j_round;
  bg::strategy::buffer::join_miter j_miter;
  bg::strategy::buffer::end_round e_round;
  bg::strategy::buffer::end_flat e_flat;
  bg::strategy::buffer::point_circle p_circle;
  bg::strategy::buffer::point_square p_square;

  bg::strategy::buffer::geographic_point_circle<> geo_point_circle;

 public:
  explicit Buffer(const BufferStrategies &strats);
  Buffer(const dd::Spatial_reference_system *srs,
         const BufferStrategies &strats);

  std::unique_ptr<Geometry> operator()(const Geometry &g) const override;

  std::unique_ptr<Geometry> eval(const Geometry &g) const;

  std::unique_ptr<Geometry> eval(const Cartesian_point &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipoint &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multilinestring &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_polygon &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_multipolygon &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_geometrycollection &g) const;

  std::unique_ptr<Geometry> eval(const Geographic_point &g) const;

  template <class T>
  std::unique_ptr<Geometry> typed_buffer(T &g) const;
};

}  // namespace gis

#endif  // SQL_GIS_BUFFER_FUNCTOR_H_INCLUDED

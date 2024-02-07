/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2.0,
    as published by the Free Software Foundation.

    This program is designed to work with certain software (including
    but not limited to OpenSSL) that is licensed under separate terms,
    as designated in a particular file or component or in included license
    documentation.  The authors of MySQL hereby grant you an additional
    permission to link the program and your derivative works with the
    separately licensed software that they have either included with
    the program or referenced in the documentation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
    @file

    This file declares the line interpolate functor interface

    The functor is not intended for use directly by MySQL code. It should be
    used indirectly through the gis::line_interpolate_point() function.

    @see gis::line_interpolate
*/

#ifndef SQL_GIS_LINE_INTERPOLATE_FUNCTOR_H_INCLUDED
#define SQL_GIS_LINE_INTERPOLATE_FUNCTOR_H_INCLUDED

#include <memory>  // std::unique_ptr

#include "sql/gis/functor.h"
#include "sql/gis/geometries.h"

namespace gis {

/// Line interpolate functor that calls boost::geometry::line_interpolate with
/// the correct parameter types.
///
/// The functor throws exceptions and is therefore only intended used to
/// implement line_interpolate or other geographic functions. It should not be
/// used directly by other MySQL code.
class Line_interpolate_point : public Unary_functor<std::unique_ptr<Geometry>> {
 private:
  double m_distance;
  bool m_return_multiple_points;
  boost::geometry::strategy::line_interpolate::geographic<>
      m_geographic_strategy;

 public:
  explicit Line_interpolate_point(double distance, bool return_multiple_points)
      : m_distance(distance),
        m_return_multiple_points(return_multiple_points) {}
  Line_interpolate_point(double distance, bool return_multiple_points,
                         double semi_major, double semi_minor)
      : m_distance(distance),
        m_return_multiple_points(return_multiple_points),
        m_geographic_strategy(
            boost::geometry::srs::spheroid<double>(semi_major, semi_minor)) {}

  std::unique_ptr<Geometry> operator()(const Geometry &g) const override;
  std::unique_ptr<Geometry> eval(const Geometry &g) const;
  std::unique_ptr<Geometry> eval(const Geographic_linestring &g) const;
  std::unique_ptr<Geometry> eval(const Cartesian_linestring &g) const;
};

}  // namespace gis

#endif  // SQL_GIS_LINE_INTERPOLATE_FUNCTOR_H_INCLUDED

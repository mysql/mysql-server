/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2.0,
    as published by the Free Software Foundation.

    This program is also distributed with certain software (including
    but not limited to OpenSSL) that is licensed under separate terms,
    as designated in a particular file or component or in included license
    documentation.  The authors of MySQL hereby grant you an additional
    permission to link the program and your derivative works with the
    separately licensed software that they have included with MySQL.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License, version 2.0, for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
    @file

    Implements the line interpolate point functor and function
*/

#include <boost/geometry.hpp>

#include "my_inttypes.h"   // MYF
#include "my_sys.h"        // my_error
#include "mysqld_error.h"  // ER_DATA_OUT_OF_RANGE
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/length.h"
#include "sql/gis/line_interpolate.h"
#include "sql/gis/line_interpolate_functor.h"
#include "sql/sql_exception_handler.h"  // handle_gis_exception

namespace bg = boost::geometry;

namespace gis {

std::unique_ptr<Geometry> Line_interpolate_point::operator()(
    const Geometry &g) const {
  return apply(*this, g);
}

std::unique_ptr<Geometry> Line_interpolate_point::eval(
    const Geometry &g) const {
  assert(false);
  throw not_implemented_exception::for_non_projected(g);
}

std::unique_ptr<Geometry> Line_interpolate_point::eval(
    const Cartesian_linestring &g) const {
  if (m_return_multiple_points) {
    Cartesian_multipoint *mpt = new Cartesian_multipoint();
    std::unique_ptr<Geometry> result(mpt);
    bg::line_interpolate(g, m_distance, *mpt);
    return result;
  }
  Cartesian_point *pt = new Cartesian_point();
  std::unique_ptr<Geometry> result(pt);
  bg::line_interpolate(g, m_distance, *pt);
  return result;
}

std::unique_ptr<Geometry> Line_interpolate_point::eval(
    const Geographic_linestring &g) const {
  if (m_return_multiple_points) {
    Geographic_multipoint *mpt = new Geographic_multipoint();
    std::unique_ptr<Geometry> result(mpt);
    bg::line_interpolate(g, m_distance, *mpt);
    return result;
  }
  Geographic_point *pt = new Geographic_point();
  std::unique_ptr<Geometry> result(pt);
  bg::line_interpolate(g, m_distance, *pt);
  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool line_interpolate_point(const dd::Spatial_reference_system *srs,
                            const Geometry *g,
                            const double interpolation_distance,
                            const bool return_multiple_points,
                            const char *func_name,
                            std::unique_ptr<Geometry> *result,
                            bool *result_null) noexcept {
  try {
    assert(((srs == nullptr || srs->is_cartesian()) &&
            g->coordinate_system() == Coordinate_system::kCartesian) ||
           (srs && srs->is_geographic() &&
            g->coordinate_system() == Coordinate_system::kGeographic));

    if (g->is_empty()) {
      *result_null = true;
      return false;
    }

    if (srs && srs->is_geographic())
      *result = Line_interpolate_point(
          interpolation_distance, return_multiple_points,
          srs->semi_major_axis(), srs->semi_minor_axis())(*g);
    else
      *result = Line_interpolate_point(interpolation_distance,
                                       return_multiple_points)(*g);

    return false;
  } catch (...) {
    handle_gis_exception(func_name);
    return true;
  }
}

}  // namespace gis

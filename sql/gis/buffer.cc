// Copyright (c) 2021, 2024, Oracle and/or its affiliates.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License, version 2.0,
//  as published by the Free Software Foundation.
//
//  This program is designed to work with certain software (including
//  but not limited to OpenSSL) that is licensed under separate terms,
//  as designated in a particular file or component or in included license
//  documentation.  The authors of MySQL hereby grant you an additional
//  permission to link the program and your derivative works with the
//  separately licensed software that they have either included with
//  the program or referenced in the documentation.
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
/// This file implements the buffer functor.

#include <boost/geometry/algorithms/buffer.hpp>  // boost::geometry::buffer
#include <memory>                                // std::unique_ptr

#include "sql/dd/types/spatial_reference_system.h"  // dd::Spatial_reference_system
#include "sql/gis/buffer.h"
#include "sql/gis/buffer_functor.h"
#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"     // gis::Geometry
#include "sql/gis/geometries_cs.h"  // gis::{Cartesian_*, Geographic_point}
#include "sql/gis/geometries_traits.h"
#include "sql/gis/longitude_range_normalizer.h"
#include "sql/sql_exception_handler.h"  // handle_gis_exception

#include "my_inttypes.h"   // MYF
#include "my_sys.h"        // my_error
#include "mysqld_error.h"  // Error codes

namespace bg = boost::geometry;

namespace gis {

Buffer::Buffer(const BufferStrategies &strategies)
    // For Cartesian geometries
    : strats(strategies),
      d_symmetric(strategies.distance),
      s_straight(),
      j_round(strategies.join_circle_value),
      j_miter(strategies.join_miter_value),
      e_round(strategies.end_circle_value),
      e_flat(),
      p_circle(strategies.point_circle_value),
      p_square() {}

Buffer::Buffer(const dd::Spatial_reference_system *srs,
               const BufferStrategies &strategies)
    // For Geographic geometries
    // Only supported geometry is Point, with only one strategy combination.
    : m_srs(srs),
      strats(strategies),
      d_symmetric(strategies.distance),
      s_straight(),
      j_round(),
      e_round(),
      geo_point_circle(bg::srs::spheroid<double>(srs->semi_major_axis(),
                                                 srs->semi_minor_axis()),
                       32) {}

std::unique_ptr<Geometry> Buffer::operator()(const Geometry &g) const {
  return apply(*this, g);
}

std::unique_ptr<Geometry> Buffer::eval(const Geometry &g) const {
  throw not_implemented_exception::for_non_projected(g);
}

//////////////////////////////////////////////////////////////////////////////

// CARTESIAN GEOMETRIES

std::unique_ptr<Geometry> Buffer::eval(const Cartesian_point &g) const {
  if (strats.end_is_set || strats.join_is_set || strats.distance < 0)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(const Cartesian_multipoint &g) const {
  if (strats.end_is_set || strats.join_is_set || strats.distance < 0)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(const Cartesian_linestring &g) const {
  if (strats.point_is_set || strats.distance < 0)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(
    const Cartesian_multilinestring &g) const {
  if (strats.point_is_set || strats.distance < 0)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(const Cartesian_polygon &g) const {
  if (strats.point_is_set || strats.end_is_set)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(const Cartesian_multipolygon &g) const {
  if (strats.point_is_set || strats.end_is_set)
    throw invalid_buffer_argument_exception();
  return typed_buffer(g);
}

std::unique_ptr<Geometry> Buffer::eval(
    const Cartesian_geometrycollection &g) const {
  // If the geometry is an empty GeometryCollection, return an empty GC.
  // Non-standard behavior kept for backwards compatibility.
  if (g.is_empty()) return std::make_unique<Cartesian_geometrycollection>();

  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  // Using split_gc to flatten the GC into three multi-geometries.
  split_gc(&g, &mpt, &mls, &mpy);
  // Using gc_union to make non-overlapping geometries, since split_gc may
  // result in geometrically invalid multipolygon.
  gc_union(0.0, 0.0, &mpt, &mls, &mpy);

  // Negative distance only allowed for GC's containing only Polygons and/or
  // Multipolygons.
  if (strats.distance < 0 && (mpt->size() > 0 || mls->size() > 0))
    throw invalid_buffer_argument_exception();

  std::unique_ptr<Cartesian_multipolygon> tmp_pt =
      std::make_unique<Cartesian_multipolygon>();
  std::unique_ptr<Cartesian_multipolygon> tmp_ls =
      std::make_unique<Cartesian_multipolygon>();
  std::unique_ptr<Cartesian_multipolygon> tmp_py =
      std::make_unique<Cartesian_multipolygon>();

  // Calling bg::buffer on each of the three multi-geometries.
  switch (strats.combination) {
    case 0:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_round, e_round, p_circle);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_round, e_round, p_circle);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_round, e_round, p_circle);
      break;
    case 1:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_round, e_flat, p_circle);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_round, e_flat, p_circle);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_round, e_flat, p_circle);
      break;
    case 2:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_miter, e_round, p_circle);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_miter, e_round, p_circle);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_miter, e_round, p_circle);
      break;
    case 3:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_miter, e_flat, p_circle);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_miter, e_flat, p_circle);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_miter, e_flat, p_circle);
      break;
    case 4:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_round, e_round, p_square);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_round, e_round, p_square);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_round, e_round, p_square);
      break;
    case 5:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_round, e_flat, p_square);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_round, e_flat, p_square);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_round, e_flat, p_square);
      break;
    case 6:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_miter, e_round, p_square);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_miter, e_round, p_square);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_miter, e_round, p_square);
      break;
    case 7:
      bg::buffer(*static_cast<Cartesian_multipoint *>(mpt.get()), *tmp_pt,
                 d_symmetric, s_straight, j_miter, e_flat, p_square);
      bg::buffer(*static_cast<Cartesian_multilinestring *>(mls.get()), *tmp_ls,
                 d_symmetric, s_straight, j_miter, e_flat, p_square);
      bg::buffer(*static_cast<Cartesian_multipolygon *>(mpy.get()), *tmp_py,
                 d_symmetric, s_straight, j_miter, e_flat, p_square);
      break;
    default:
      assert(false);
      throw std::exception();
      break;
  }

  // Using gis::Union to merge the returned MultiPolygons into one.
  Union un(0.0, 0.0);
  std::unique_ptr<Geometry> tmp = un(tmp_pt.get(), tmp_ls.get());
  tmp = un(tmp.get(), tmp_py.get());
  std::unique_ptr<Geometry> result(tmp.release());

  // If distance is negative bg::buffer may have shrunk all geometries in
  // in the GC so much they all disappeared.
  if (result->is_empty())
    return std::make_unique<Cartesian_geometrycollection>();

  return result;
}

//////////////////////////////////////////////////////////////////////////////

// GEOGRAPHIC GEOMETRIES

std::unique_ptr<Geometry> Buffer::eval(const Geographic_point &g) const {
  // Passing strategy arguments with a Geographic Point is considered invalid.
  if (strats.join_is_set || strats.end_is_set || strats.point_is_set ||
      strats.distance < 0)
    throw invalid_buffer_argument_exception();

  std::unique_ptr<Geographic_multipolygon> temp_result =
      std::make_unique<Geographic_multipolygon>();
  bg::buffer(g, *temp_result, d_symmetric, s_straight, j_round, e_round,
             geo_point_circle);

  // bg::buffer may return values outside longitude range (-180, 180]. Running
  // a normalizer to convert values into the range.
  Longitude_range_normalizer crn(m_srs);
  temp_result->accept(&crn);

  return std::make_unique<Geographic_polygon>((*temp_result)[0]);
}

//////////////////////////////////////////////////////////////////////////////

/// Templated call to bg::buffer based on geometry type. Function also switches
/// on strategy combination. PS. Not called by eval for Cartesian GC or
/// Geographic point.
template <class T>
std::unique_ptr<Geometry> Buffer::typed_buffer(T &g) const {
  std::unique_ptr<Cartesian_multipolygon> res =
      std::make_unique<Cartesian_multipolygon>();

  switch (strats.combination) {
    case 0:
      bg::buffer(g, *res, d_symmetric, s_straight, j_round, e_round, p_circle);
      break;
    case 1:
      bg::buffer(g, *res, d_symmetric, s_straight, j_round, e_flat, p_circle);
      break;
    case 2:
      bg::buffer(g, *res, d_symmetric, s_straight, j_miter, e_round, p_circle);
      break;
    case 3:
      bg::buffer(g, *res, d_symmetric, s_straight, j_miter, e_flat, p_circle);
      break;
    case 4:
      bg::buffer(g, *res, d_symmetric, s_straight, j_round, e_round, p_square);
      break;
    case 5:
      bg::buffer(g, *res, d_symmetric, s_straight, j_round, e_flat, p_square);
      break;
    case 6:
      bg::buffer(g, *res, d_symmetric, s_straight, j_miter, e_round, p_square);
      break;
    case 7:
      bg::buffer(g, *res, d_symmetric, s_straight, j_miter, e_flat, p_square);
      break;
    default:
      assert(false);
      throw std::exception();
      break;
  }

  if (res->is_empty()) {
    // With negative distance bg::buffer may shrink Polygons and Multipolygons
    // so much that they disappear.
    if (strats.distance < 0 && (g.type() == Geometry_type::kPolygon ||
                                g.type() == Geometry_type::kMultipolygon)) {
      return std::make_unique<Cartesian_geometrycollection>();
    } else {
      // bg::buffer is only supposed to return an empty answer for Polygon,
      // Multipolygon, or GC - if the distance is negative. This suggests an
      // unknown error.
      throw invalid_buffer_result_exception();
    }
  }

  if (res->size() == 1) return std::make_unique<Cartesian_polygon>((*res)[0]);

  return std::make_unique<Cartesian_multipolygon>(*res);
}

//////////////////////////////////////////////////////////////////////////////

bool buffer(const dd::Spatial_reference_system *srs, const Geometry &g,
            const BufferStrategies &strategies, const char *func_name,
            std::unique_ptr<Geometry> *result) noexcept {
  try {
    if (srs && srs->is_geographic()) {
      Buffer buffer_func(srs, strategies);
      *result = buffer_func(g);
    } else {
      Buffer buffer_func(strategies);
      *result = buffer_func(g);
    }
  } catch (...) {
    handle_gis_exception(func_name);
    return true;
  }
  return false;
}

}  // namespace gis

// Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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
/// This file implements the union functor.

#include <boost/geometry.hpp>  // boost::geometry::union_

#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/so_utils.h"
#include "sql/gis/union_functor.h"
#include "template_utils.h"  // down_cast

namespace bg = boost::geometry;

namespace gis {

template <typename GC, typename MPt, typename MLs, typename MPy>
static std::unique_ptr<Geometrycollection>
typed_geometry_collection_apply_union(const Union &f,
                                      const Geometrycollection *g1,
                                      const Geometry *g2) {
  std::unique_ptr<Geometrycollection> result = std::make_unique<GC>();
  if (g1->is_empty() && g2->is_empty()) return result;

  std::unique_ptr<GC> gc_in = std::make_unique<GC>();
  gc_in->push_back(*g1);
  gc_in->push_back(*g2);

  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(gc_in.get(), &mpt, &mls, &mpy);
  gc_union(f.semi_major(), f.semi_minor(), &mpt, &mls, &mpy);

  if (mpt->is_empty() && mls->is_empty()) {
    result.reset(mpy.release());
    return result;
  }

  if (mpy->is_empty() && mpt->is_empty()) {
    result.reset(mls.release());
    return result;
  }

  if (mpy->is_empty() && mls->is_empty()) {
    result.reset(mpt.release());
    return result;
  }

  for (auto py : *down_cast<MPy *>(mpy.get())) result->push_back(py);
  for (auto ls : *down_cast<MLs *>(mls.get())) result->push_back(ls);
  for (auto pt : *down_cast<MPt *>(mpt.get())) result->push_back(pt);

  return result;
}

/// Apply a Union functor to two geometries, where at least one is a geometry
/// collection. Return the union of all the geometries of the input geometries.
///
/// @param f Functor to apply.
/// @param g1 First geometry, of type Geometrycollection.
/// @param g2 Second geometry.
///
/// @retval unique pointer to a Geometry.
static std::unique_ptr<Geometrycollection> geometry_collection_apply_union(
    const Union &f, const Geometrycollection *g1, const Geometry *g2) {
  switch (g1->coordinate_system()) {
    case Coordinate_system::kCartesian: {
      return typed_geometry_collection_apply_union<
          Cartesian_geometrycollection, Cartesian_multipoint,
          Cartesian_multilinestring, Cartesian_multipolygon>(f, g1, g2);
    }
    case Coordinate_system::kGeographic: {
      return typed_geometry_collection_apply_union<
          Geographic_geometrycollection, Geographic_multipoint,
          Geographic_multilinestring, Geographic_multipolygon>(f, g1, g2);
    }
  }
  assert(false);
  // This should never happen.
  throw not_implemented_exception::for_non_projected(*g1);
}

Union::Union(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

std::unique_ptr<Geometry> Union::operator()(const Geometry *g1,
                                            const Geometry *g2) const {
  std::unique_ptr<Geometry> result = apply(*this, g1, g2);
  remove_duplicates(this->semi_major(), this->semi_minor(), &result);
  narrow_geometry(&result);
  return result;
}

std::unique_ptr<Geometry> Union::eval(const Geometry *g1,
                                      const Geometry *g2) const {
  assert(false);
  throw not_implemented_exception::for_non_projected(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_point, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_point *g1,
                                      const Cartesian_point *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::union_(*g1, *g2, *result);
  return std::make_unique<Cartesian_multipoint>(*result);
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_point *g1, const Cartesian_linestring *g2) const {
  // Union(Point, Linestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Linestring) or Linestring.
  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g2);
  if (bg::disjoint(*g1, *g2)) result->push_back(*g1);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_point *g1, const Cartesian_polygon *g2) const {
  // Union(Point, Polygon) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Polygon) or Polygon.
  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g2);
  if (bg::disjoint(*g1, *g2)) result->push_back(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Union::eval(
    const Cartesian_point *g1, const Cartesian_multipoint *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_point *g1, const Cartesian_multilinestring *g2) const {
  // Union(Point, Multilinestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Linestrings...) or Multilinestring.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  for (auto ls : *g2) result->push_back(ls);
  if (bg::disjoint(*g1, *g2)) result->push_back(*g1);

  if (result->size() == g2->size()) result.reset(g2->clone());
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_point *g1, const Cartesian_multipolygon *g2) const {
  // Union(Point, Multipolygon) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Multipolygon) or Multipolygon.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  for (auto py : *g2) result->push_back(py);
  if (bg::disjoint(*g1, *g2)) result->push_back(*g1);

  if (result->size() == g2->size()) result.reset(g2->clone());
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_linestring, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_linestring *g1,
                                      const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multilinestring> Union::eval(
    const Cartesian_linestring *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_linestring *g1, const Cartesian_polygon *g2) const {
  // Union(Linestring, Polygon) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Polygon, Difference(Linestring, Polygon)) or Polygon.
  std::unique_ptr<Cartesian_multilinestring> difference =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g2);
  for (auto ls : *difference) result->push_back(ls);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_linestring *g1, const Cartesian_multipoint *g2) const {
  // Union(Linestring, Multipoint) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Linestring, Difference(Multipoint, Linestring)) or
  // Linestring.
  std::unique_ptr<Cartesian_multipoint> difference =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g2, *g1, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g1);
  for (auto pt : *difference) result->push_back(pt);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Union::eval(
    const Cartesian_linestring *g1, const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_linestring *g1, const Cartesian_multipolygon *g2) const {
  // Union(Linestring, Multipolygon) isn't supported by BG, but it's equivalent
  // to GeometryCollection(Polygons..., Difference(Linestring, Multipolygon)) or
  // Multipolygon.
  std::unique_ptr<Cartesian_multilinestring> difference =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto ls : *difference) result->push_back(ls);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_polygon, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_polygon *g1,
                                      const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_polygon *g1,
                                      const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipolygon> Union::eval(
    const Cartesian_polygon *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_polygon *g1, const Cartesian_multipoint *g2) const {
  // Union(Polygon, Multipoint) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Polygon, Difference(Multipoint, Polygon)) or Polygon.
  std::unique_ptr<Cartesian_multipoint> difference =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g2, *g1, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g1);
  for (auto pt : *difference) result->push_back(pt);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> Union::eval(
    const Cartesian_polygon *g1, const Cartesian_multilinestring *g2) const {
  // Union(Polygon, MultiLineString) isn't supported by BG, but it's equivalent
  // to GeometryCollection(Polygon, Difference(MultiLineString, Polygon)) or
  // Polygon.
  std::unique_ptr<Cartesian_multilinestring> difference =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g2, *g1, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Cartesian_geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  result->push_back(*g1);
  for (auto ls : *difference) result->push_back(ls);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Union::eval(
    const Cartesian_polygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::union_(*g1, *g2, *result);
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_geometrycollection, *)

std::unique_ptr<Geometry> Union::eval(
    const Cartesian_geometrycollection *g1,
    const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_union(*this, g1, g2);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_geometrycollection *g1,
                                      const Geometry *g2) const {
  return geometry_collection_apply_union(*this, g1, g2);
}

std::unique_ptr<Geometry> Union::eval(
    const Geometry *g1, const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_union(*this, g2, g1);
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_multipoint, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipoint *g1,
                                      const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipoint *g1,
                                      const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipoint *g1,
                                      const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipoint> Union::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipoint *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_multipoint *g1, const Cartesian_multilinestring *g2) const {
  // Union(Multipoint, Multilinestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Linestrings..., Difference(Multipoint,
  // Multilinestring)) or Multilinestring.
  std::unique_ptr<Cartesian_multipoint> difference =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto ls : *g2) result->push_back(ls);
    for (auto pt : *difference) result->push_back(pt);
  }
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2) const {
  // Union(Multipoint, Multipolygon) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Polygons..., Difference(Multipoint,
  // Multipolygon)) or Multipolygon.
  std::unique_ptr<Cartesian_multipoint> difference =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto pt : *difference) result->push_back(pt);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_multilinestring, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_multilinestring *g1,
                                      const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multilinestring *g1,
                                      const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multilinestring *g1,
                                      const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multilinestring *g1,
                                      const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multilinestring> Union::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multipolygon *g2) const {
  // Union(MultiLineString, MultiPolygon) isn't supported by BG, but
  // it's equivalent to GeometryCollection(MultiPolygon,
  // Difference(MultiLineString, MultiPolygon)).
  std::unique_ptr<Cartesian_multilinestring> difference =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *difference);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Cartesian_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto ls : *difference) result->push_back(ls);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Cartesian_multipolygon, *)

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipolygon *g1,
                                      const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipolygon *g1,
                                      const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipolygon *g1,
                                      const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Cartesian_multipolygon *g1,
                                      const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipolygon> Union::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::union_(*g1, *g2, *result);
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_point, *)

std::unique_ptr<Geographic_multipoint> Union::eval(
    const Geographic_point *g1, const Geographic_point *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_point *g1, const Geographic_linestring *g2) const {
  // Union(Point, Linestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Linestring) or Linestring.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g2);
  if (bg::disjoint(*g1, *g2, m_geographic_pl_pa_strategy))
    result->push_back(*g1);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_point *g1, const Geographic_polygon *g2) const {
  // Union(Point, Polygon) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Polygon) or Polygon.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g2);
  if (bg::disjoint(*g1, *g2, m_geographic_pl_pa_strategy))
    result->push_back(*g1);
  return result;
}

std::unique_ptr<Geographic_multipoint> Union::eval(
    const Geographic_point *g1, const Geographic_multipoint *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_point *g1, const Geographic_multilinestring *g2) const {
  // Union(Point, Multilinestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Linestrings...) or Multilinestring.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  for (auto ls : *g2) result->push_back(ls);
  if (bg::disjoint(*g1, *g2, m_geographic_pl_pa_strategy))
    result->push_back(*g1);

  if (result->size() == g2->size()) result.reset(g2->clone());
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_point *g1, const Geographic_multipolygon *g2) const {
  // Union(Point, Multipolygon) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Point, Multipolygon) or Multipolygon.
  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  for (auto py : *g2) result->push_back(py);
  if (bg::disjoint(*g1, *g2, m_geographic_pl_pa_strategy))
    result->push_back(*g1);

  if (result->size() == g2->size()) result.reset(g2->clone());
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_linestring, *)

std::unique_ptr<Geometry> Union::eval(const Geographic_linestring *g1,
                                      const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multilinestring> Union::eval(
    const Geographic_linestring *g1, const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> Union::eval(
    const Geographic_linestring *g1, const Geographic_polygon *g2) const {
  // Union(Linestring, Polygon) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Polygon, Difference(Linestring, Polygon)) or Polygon.
  std::unique_ptr<Geographic_multilinestring> difference =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *difference, m_geographic_ll_la_aa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geographic_geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g2);
  for (auto ls : *difference) result->push_back(ls);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> Union::eval(
    const Geographic_linestring *g1, const Geographic_multipoint *g2) const {
  // Union(Linestring, Multipoint) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Linestring, Difference(Multipoint, Linestring)) or
  // Linestring.
  std::unique_ptr<Geographic_multipoint> difference =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g2, *g1, *difference, m_geographic_pl_pa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geographic_geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g1);
  for (auto pt : *difference) result->push_back(pt);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Union::eval(
    const Geographic_linestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_linestring *g1, const Geographic_multipolygon *g2) const {
  // Union(Linestring, Multipolygon) isn't supported by BG, but it's equivalent
  // to GeometryCollection(Polygons..., Difference(Linestring, Multipolygon)) or
  // Multipolygon.
  std::unique_ptr<Geographic_multilinestring> difference =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *difference, m_geographic_ll_la_aa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto ls : *difference) result->push_back(ls);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_polygon, *)

std::unique_ptr<Geometry> Union::eval(const Geographic_polygon *g1,
                                      const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_polygon *g1,
                                      const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipolygon> Union::eval(
    const Geographic_polygon *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> Union::eval(
    const Geographic_polygon *g1, const Geographic_multipoint *g2) const {
  // Union(Polygon, Multipoint) isn't supported by BG, but it's equivalent to
  // GeometryCollection(Polygon, Difference(Polygon, Multipoint)) or Polygon.
  std::unique_ptr<Geographic_multipoint> difference =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g2, *g1, *difference, m_geographic_pl_pa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geographic_geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g1);
  for (auto pt : *difference) result->push_back(pt);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> Union::eval(
    const Geographic_polygon *g1, const Geographic_multilinestring *g2) const {
  // Union(Polygon, MultiLineString) isn't supported by BG, but it's equivalent
  // to GeometryCollection(Polygon, Difference(MultiLineString, Polygon)) or
  // Polygon.
  std::unique_ptr<Geographic_multilinestring> difference =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g2, *g1, *difference, m_geographic_ll_la_aa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geographic_geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  result->push_back(*g1);
  for (auto ls : *difference) result->push_back(ls);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Union::eval(
    const Geographic_polygon *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_geometrycollection, *)

std::unique_ptr<Geometry> Union::eval(
    const Geographic_geometrycollection *g1,
    const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_union(*this, g1, g2);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_geometrycollection *g1,
                                      const Geometry *g2) const {
  return geometry_collection_apply_union(*this, g1, g2);
}

std::unique_ptr<Geometry> Union::eval(
    const Geometry *g1, const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_union(*this, g2, g1);
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_multipoint, *)

std::unique_ptr<Geometry> Union::eval(const Geographic_multipoint *g1,
                                      const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multipoint *g1,
                                      const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multipoint *g1,
                                      const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipoint> Union::eval(
    const Geographic_multipoint *g1, const Geographic_multipoint *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::union_(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_multipoint *g1,
    const Geographic_multilinestring *g2) const {
  // Union(Multipoint, Multilinestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Linestrings..., Difference(Multipoint,
  // Multilinestring)) or Multilinestring.
  std::unique_ptr<Geographic_multipoint> difference =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *difference, m_geographic_pl_pa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto ls : *g2) result->push_back(ls);
    for (auto pt : *difference) result->push_back(pt);
  }
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_multipoint *g1, const Geographic_multipolygon *g2) const {
  // Union(Multipoint, Multilinestring) isn't supported by BG, but it's
  // equivalent to GeometryCollection(Linestrings..., Difference(Multipoint,
  // Multilinestring)) or Multilinestring.
  std::unique_ptr<Geographic_multipoint> difference =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *difference, m_geographic_pl_pa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto pt : *difference) result->push_back(pt);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_multilinestring, *)

std::unique_ptr<Geometry> Union::eval(const Geographic_multilinestring *g1,
                                      const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multilinestring *g1,
                                      const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multilinestring *g1,
                                      const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multilinestring *g1,
                                      const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multilinestring> Union::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometrycollection> Union::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipolygon *g2) const {
  // Union(MultiLineString, MultiPolygon) isn't supported by BG, but
  // it's equivalent to GeometryCollection(MultiPolygon,
  // Difference(MultiLineString, MultiPolygon)).
  std::unique_ptr<Geographic_multilinestring> difference =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *difference, m_geographic_ll_la_aa_strategy);
  assert(difference.get() != nullptr);

  std::unique_ptr<Geometrycollection> result =
      std::make_unique<Geographic_geometrycollection>();
  if (difference->is_empty())
    result.reset(g2->clone());
  else {
    for (auto py : *g2) result->push_back(py);
    for (auto ls : *difference) result->push_back(ls);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// union(Geographic_multipolygon, *)

std::unique_ptr<Geometry> Union::eval(const Geographic_multipolygon *g1,
                                      const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multipolygon *g1,
                                      const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multipolygon *g1,
                                      const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(const Geographic_multipolygon *g1,
                                      const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Union::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipolygon> Union::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::union_(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

}  // namespace gis

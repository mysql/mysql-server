// Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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
/// This file implements the difference functor.

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>

#include "sql/gis/difference_functor.h"
#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/so_utils.h"

namespace bg = boost::geometry;

namespace gis {

template <typename GC, typename MPt, typename MLs, typename MPy>
static std::unique_ptr<Geometry> typed_geometry_collection_apply_difference(
    const Difference &f, const Geometrycollection *g1, const Geometry *g2) {
  std::unique_ptr<GC> result = std::make_unique<GC>();
  if (g1->is_empty()) return std::make_unique<GC>(*result);

  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g1, &mpt, &mls, &mpy);
  gc_union(f.semi_major(), f.semi_minor(), &mpt, &mls, &mpy);

  if (!mpy->is_empty()) {
    std::unique_ptr<Geometry> mpy_result = f(mpy.get(), g2);
    if (mpt->is_empty() && mls->is_empty()) {
      return mpy_result;
    }
    if (mpy_result->type() == Geometry_type::kPolygon)
      result->push_back(*mpy_result);
    else
      for (auto py : *down_cast<MPy *>(mpy_result.get())) result->push_back(py);
  }

  if (!mls->is_empty()) {
    std::unique_ptr<Geometry> mls_result = f(mls.get(), g2);
    if (mpy->is_empty() && mpt->is_empty()) {
      return mls_result;
    }
    if (mls_result->type() == Geometry_type::kLinestring)
      result->push_back(*mls_result);
    else
      for (auto ls : *down_cast<MLs *>(mls_result.get())) result->push_back(ls);
  }

  if (!mpt->is_empty()) {
    std::unique_ptr<Geometry> mpt_result = f(mpt.get(), g2);
    if (mpy->is_empty() && mls->is_empty()) {
      return mpt_result;
    }
    if (mpt_result->type() == Geometry_type::kPoint)
      result->push_back(*mpt_result);
    else
      for (auto pt : *down_cast<MPt *>(mpt_result.get())) result->push_back(pt);
  }

  return std::make_unique<GC>(*result);
}

/// Apply a Difference functor to two geometries, where at least one is a
/// geometry collection. Return the difference of the two geometries.
///
/// @param f Functor to apply.
/// @param g1 First geometry, of type Geometrycollection.
/// @param g2 Second geometry.
///
/// @retval unique pointer to a Geometry.
static std::unique_ptr<Geometry> geometry_collection_apply_difference(
    const Difference &f, const Geometrycollection *g1, const Geometry *g2) {
  switch (g1->coordinate_system()) {
    case Coordinate_system::kCartesian: {
      return typed_geometry_collection_apply_difference<
          Cartesian_geometrycollection, Cartesian_multipoint,
          Cartesian_multilinestring, Cartesian_multipolygon>(f, g1, g2);
    }
    case Coordinate_system::kGeographic: {
      return typed_geometry_collection_apply_difference<
          Geographic_geometrycollection, Geographic_multipoint,
          Geographic_multilinestring, Geographic_multipolygon>(f, g1, g2);
    }
  }
  assert(false);
  // This should never happen.
  throw not_implemented_exception::for_non_projected(*g1);
}

Difference::Difference(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

std::unique_ptr<Geometry> Difference::operator()(const Geometry *g1,
                                                 const Geometry *g2) const {
  std::unique_ptr<Geometry> result = apply(*this, g1, g2);
  remove_duplicates(this->semi_major(), this->semi_minor(), &result);
  narrow_geometry(&result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(const Geometry *g1,
                                           const Geometry *g2) const {
  assert(false);
  throw not_implemented_exception::for_non_projected(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_point, *)

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_point *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_multipoint *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_point *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_point *g1, const Cartesian_geometrycollection *g2) const {
  Cartesian_multipoint *result_g = new Cartesian_multipoint();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_linestring, *)

std::unique_ptr<Cartesian_linestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_linestring> result =
      std::make_unique<Cartesian_linestring>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_linestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_linestring> result =
      std::make_unique<Cartesian_linestring>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_linestring *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_linestring *g1,
    const Cartesian_geometrycollection *g2) const {
  Cartesian_multilinestring *result_g = new Cartesian_multilinestring();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_polygon, *)

std::unique_ptr<Cartesian_polygon> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_polygon> result =
      std::make_unique<Cartesian_polygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_polygon> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_linestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_polygon> result =
      std::make_unique<Cartesian_polygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_polygon> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_polygon> result =
      std::make_unique<Cartesian_polygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_polygon> Difference::eval(
    const Cartesian_polygon *g1,
    const Cartesian_multilinestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_polygon> result =
      std::make_unique<Cartesian_polygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_polygon *g1, const Cartesian_geometrycollection *g2) const {
  Cartesian_multipolygon *result_g = new Cartesian_multipolygon();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_geometrycollection, *)

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_geometrycollection *g1, const Geometry *g2) const {
  return geometry_collection_apply_difference(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multipoint, *)

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_point *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipoint *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipoint> Difference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipoint> result =
      std::make_unique<Cartesian_multipoint>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_multipoint *g1,
    const Cartesian_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multilinestring, *)

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multilinestring> Difference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multipolygon, *)

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_linestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result(new Cartesian_multipolygon());
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_multilinestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Cartesian_multipolygon> Difference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result(new Cartesian_multipolygon());
  bg::difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_point, *)

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_point *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result);  // Default strategy is OK.
  return result;
}
std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}
std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}
std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_multipoint *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result);  // Default strategy is OK.
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_point *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_point *g1, const Geographic_geometrycollection *g2) const {
  Geographic_multipoint *result_g = new Geographic_multipoint();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_linestring, *)

std::unique_ptr<Geographic_linestring> Difference::eval(
    const Geographic_linestring *g1, const Geographic_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_linestring> result =
      std::make_unique<Geographic_linestring>(*g1);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_linestring *g1, const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_linestring *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_linestring> Difference::eval(
    const Geographic_linestring *g1,
    const Geographic_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_linestring> result =
      std::make_unique<Geographic_linestring>(*g1);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_linestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_linestring *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_linestring *g1,
    const Geographic_geometrycollection *g2) const {
  Geographic_multilinestring *result_g = new Geographic_multilinestring();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_polygon, *)

std::unique_ptr<Geographic_polygon> Difference::eval(
    const Geographic_polygon *g1, const Geographic_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_polygon> result =
      std::make_unique<Geographic_polygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_polygon> Difference::eval(
    const Geographic_polygon *g1, const Geographic_linestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_polygon> result =
      std::make_unique<Geographic_polygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_polygon *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_polygon> Difference::eval(
    const Geographic_polygon *g1, const Geographic_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_polygon> result =
      std::make_unique<Geographic_polygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_polygon> Difference::eval(
    const Geographic_polygon *g1,
    const Geographic_multilinestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_polygon> result =
      std::make_unique<Geographic_polygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_polygon *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_polygon *g1,
    const Geographic_geometrycollection *g2) const {
  Geographic_multipolygon *result_g = new Geographic_multipolygon();
  std::unique_ptr<Geometry> result(result_g);
  result_g->push_back(*g1);
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_geometrycollection, *)

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_geometrycollection *g1, const Geometry *g2) const {
  return geometry_collection_apply_difference(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multipoint, *)

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1, const Geographic_point *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result);  // Default strategy is OK.
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1, const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1, const Geographic_multipoint *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result);  // Default strategy is OK.
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geographic_multipoint> Difference::eval(
    const Geographic_multipoint *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipoint> result =
      std::make_unique<Geographic_multipoint>();
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_multipoint *g1,
    const Geographic_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multilinestring, *)

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>(*g1);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>(*g1);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multilinestring> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multipolygon, *)

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1, const Geographic_point * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_linestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multipoint * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multilinestring * /*g2*/) const {
  // Given two geometries g1 and g2, where g1.dimension > g2.dimension, then
  // g1 - g2 is equal to g1, this is always true. This is how postgis works.
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>(*g1);
  return result;
}

std::unique_ptr<Geographic_multipolygon> Difference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> Difference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_geometrycollection *g2) const {
  std::unique_ptr<Geometry> result(g1->clone());
  for (auto g : *g2) {
    result = (*this)(result.get(), g);
  }
  return result;
}

}  // namespace gis

// Copyright (c) 2020, 2022, Oracle and/or its affiliates.
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
/// This file implements the intersection functor.

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>

#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/intersection_functor.h"
#include "sql/gis/so_utils.h"
#include "sql/gis/union_functor.h"

namespace bg = boost::geometry;

namespace gis {

static bool is_collection(const Geometry *g) {
  switch (g->type()) {
    case Geometry_type::kPoint:
    case Geometry_type::kLinestring:
    case Geometry_type::kPolygon:
      return false;
    default:
      return true;
  }
}

template <typename MPt, typename MLs>
static auto remove_overlapping_mpt_mls(MPt const &mpt, MLs const &mls,
                                       Geometrycollection &result) {
  std::unique_ptr<MPt> difference = std::make_unique<MPt>();
  bg::difference(mpt, mls, *difference);
  for (auto ls : mls) result.push_back(ls);
  for (auto pt : *difference) result.push_back(pt);
}

template <typename MPt, typename MLs, typename MPy>
static auto remove_overlapping_mpt_mls_mpy(MPt const &mpt, MLs const &mls,
                                           MPy const &mpy,
                                           Geometrycollection &result) {
  std::unique_ptr<MPt> mpt_mls_difference = std::make_unique<MPt>();
  bg::difference(mpt, mls, *mpt_mls_difference);

  std::unique_ptr<MPt> mpt_mls_mpy_difference = std::make_unique<MPt>();
  bg::difference(*mpt_mls_difference, mpy, *mpt_mls_mpy_difference);

  if (!mpt_mls_mpy_difference->is_empty()) {
    if (mpt_mls_mpy_difference->size() == 1) {
      result.push_back((*mpt_mls_mpy_difference)[0]);
    } else {
      result.push_back(*mpt_mls_mpy_difference);
    }
  }

  std::unique_ptr<MLs> mls_mpy_difference = std::make_unique<MLs>();
  bg::difference(mls, mpy, *mls_mpy_difference);

  if (!mls_mpy_difference->is_empty()) {
    if (mls_mpy_difference->size() == 1) {
      result.push_back((*mls_mpy_difference)[0]);
    } else {
      result.push_back(*mls_mpy_difference);
    }
  }

  if (!mpy.is_empty()) {
    if (mpy.size() == 1) {
      result.push_back(mpy[0]);
    } else {
      result.push_back(mpy);
    }
  }
}

template <typename MPt, typename MLs, typename MPy, typename Geometry1,
          typename Geometry2>
static auto apply_bg_intersection(Geometry1 const &g1, Geometry2 const &g2) {
  std::unique_ptr<gis::Geometrycollection> result(
      gis::Geometrycollection::create_geometrycollection(
          g1.coordinate_system()));

  std::tuple<MPt, MLs, MPy> bg_result;

  bg::intersection(g1, g2, bg_result);

  remove_overlapping_mpt_mls_mpy(std::get<0>(bg_result), std::get<1>(bg_result),
                                 std::get<2>(bg_result), *result);

  return result;
}

template <typename MPt, typename MLs, typename MPy, typename Geometry1,
          typename Geometry2, typename Strategy>
static auto apply_bg_intersection(Geometry1 const &g1, Geometry2 const &g2,
                                  Strategy const &strategy) {
  std::unique_ptr<gis::Geometrycollection> result(
      gis::Geometrycollection::create_geometrycollection(
          g1.coordinate_system()));

  std::tuple<MPt, MLs, MPy> bg_result;

  bg::intersection(g1, g2, bg_result, strategy);

  remove_overlapping_mpt_mls_mpy(std::get<0>(bg_result), std::get<1>(bg_result),
                                 std::get<2>(bg_result), *result);

  return result;
}

template <typename MPt, typename MLs, typename Geometry1, typename Geometry2>
static auto apply_bg_brute_force_intersection(Geometry1 const &g1,
                                              Geometry2 const &g2) {
  std::unique_ptr<gis::Geometrycollection> result(
      gis::Geometrycollection::create_geometrycollection(
          g1.coordinate_system()));

  MPt bg_result_mp;
  bg::intersection(g1, g2, bg_result_mp);

  MLs bg_result_ml;
  bg::intersection(g1, g2, bg_result_ml);

  remove_overlapping_mpt_mls(bg_result_mp, bg_result_ml, *result);

  return result;
}

template <typename MPt, typename MLs, typename Geometry1, typename Geometry2,
          typename Strategy>
static auto apply_bg_brute_force_intersection(Geometry1 const &g1,
                                              Geometry2 const &g2,
                                              Strategy const &strategy) {
  std::unique_ptr<gis::Geometrycollection> result(
      gis::Geometrycollection::create_geometrycollection(
          g1.coordinate_system()));

  MPt bg_result_mp;
  bg::intersection(g1, g2, bg_result_mp, strategy);

  MLs bg_result_ml;
  bg::intersection(g1, g2, bg_result_ml, strategy);

  remove_overlapping_mpt_mls(bg_result_mp, bg_result_ml, *result);

  return result;
}

template <typename GC, typename MPt, typename MLs, typename MPy>
static std::unique_ptr<Geometry> typed_geometry_collection_apply_intersection(
    const Intersection &f, const Geometrycollection *g1, const Geometry *g2) {
  std::unique_ptr<GC> result = std::make_unique<GC>();
  if (g1->is_empty() || g2->is_empty()) return std::make_unique<GC>(*result);

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
    if (is_collection(mpy_result.get())) {
      Geometrycollection *gc_mpy_result =
          down_cast<Geometrycollection *>(mpy_result.get());
      for (size_t i = 0; i < gc_mpy_result->size(); i++)
        result->push_back((*gc_mpy_result)[i]);
    } else
      result->push_back(*mpy_result);
  }

  if (!mls->is_empty()) {
    std::unique_ptr<Geometry> mls_result = f(mls.get(), g2);
    if (mpy->is_empty() && mpt->is_empty()) {
      return mls_result;
    }
    if (is_collection(mls_result.get())) {
      Geometrycollection *gc_mls_result =
          down_cast<Geometrycollection *>(mls_result.get());
      for (size_t i = 0; i < gc_mls_result->size(); i++)
        result->push_back((*gc_mls_result)[i]);
    } else
      result->push_back(*mls_result);
  }

  if (!mpt->is_empty()) {
    std::unique_ptr<Geometry> mpt_result = f(mpt.get(), g2);
    if (mpy->is_empty() && mls->is_empty()) {
      return mpt_result;
    }
    if (is_collection(mpt_result.get())) {
      Geometrycollection *gc_mpt_result =
          down_cast<Geometrycollection *>(mpt_result.get());
      for (size_t i = 0; i < gc_mpt_result->size(); i++)
        result->push_back((*gc_mpt_result)[i]);
    } else
      result->push_back(*mpt_result);
  }

  return std::make_unique<GC>(*result);
}

/// Apply a Intersection functor to two geometries, where at least one is a
/// geometry collection. Return the intersection of the two geometries.
///
/// @param f Functor to apply.
/// @param g1 First geometry, of type Geometrycollection.
/// @param g2 Second geometry.
///
/// @retval unique pointer to a Geometry.
static std::unique_ptr<Geometry> geometry_collection_apply_intersection(
    const Intersection &f, const Geometrycollection *g1, const Geometry *g2) {
  switch (g1->coordinate_system()) {
    case Coordinate_system::kCartesian: {
      return typed_geometry_collection_apply_intersection<
          Cartesian_geometrycollection, Cartesian_multipoint,
          Cartesian_multilinestring, Cartesian_multipolygon>(f, g1, g2);
    }
    case Coordinate_system::kGeographic: {
      return typed_geometry_collection_apply_intersection<
          Geographic_geometrycollection, Geographic_multipoint,
          Geographic_multilinestring, Geographic_multipolygon>(f, g1, g2);
    }
  }
  assert(false);
  // This should never happen.
  throw not_implemented_exception::for_non_projected(*g1);
}

Intersection::Intersection(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

std::unique_ptr<Geometry> Intersection::operator()(const Geometry *g1,
                                                   const Geometry *g2) const {
  std::unique_ptr<Geometry> result = apply(*this, g1, g2);
  remove_duplicates(this->semi_major(), this->semi_minor(), &result);
  narrow_geometry(&result);
  return result;
}

std::unique_ptr<Geometry> Intersection::eval(const Geometry *g1,
                                             const Geometry *g2) const {
  assert(false);
  throw not_implemented_exception::for_non_projected(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_point, *)

std::unique_ptr<Geometry> Intersection::eval(const Cartesian_point *g1,
                                             const Cartesian_point *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_point *g1, const Cartesian_linestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_point *g1, const Cartesian_polygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_point *g1, const Cartesian_multipoint *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_point *g1, const Cartesian_multilinestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_point *g1, const Cartesian_multipolygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_linestring, *)

std::unique_ptr<Geometry> Intersection::eval(const Cartesian_linestring *g1,
                                             const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_linestring *g1, const Cartesian_linestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_linestring *g1, const Cartesian_polygon *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Cartesian_multipoint,
                                           Cartesian_multilinestring>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_linestring *g1, const Cartesian_multipoint *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_linestring *g1, const Cartesian_multilinestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_linestring *g1, const Cartesian_multipolygon *g2) const {
  // TODO: fix in boost geometry
  return apply_bg_brute_force_intersection<Cartesian_multipoint,
                                           Cartesian_multilinestring>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_polygon, *)

std::unique_ptr<Geometry> Intersection::eval(const Cartesian_polygon *g1,
                                             const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_polygon *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_polygon *g1, const Cartesian_polygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_polygon *g1, const Cartesian_multipoint *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_polygon *g1, const Cartesian_multilinestring *g2) const {
  // TODO: fix in boost geometry
  return apply_bg_brute_force_intersection<Cartesian_multipoint,
                                           Cartesian_multilinestring>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_polygon *g1, const Cartesian_multipolygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_geometrycollection, *)

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_geometrycollection *g1, const Geometry *g2) const {
  return geometry_collection_apply_intersection(*this, g1, g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geometry *g1, const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_intersection(*this, g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_geometrycollection *g1,
    const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_intersection(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_multipoint, *)

std::unique_ptr<Geometry> Intersection::eval(const Cartesian_multipoint *g1,
                                             const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipoint *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipoint *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipoint *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipoint *g1, const Cartesian_multilinestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_multilinestring, *)

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1, const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1, const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multilinestring *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multipolygon *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Cartesian_multipoint,
                                           Cartesian_multilinestring>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Cartesian_multipolygon, *)

std::unique_ptr<Geometry> Intersection::eval(const Cartesian_multipolygon *g1,
                                             const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipolygon *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipolygon *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipolygon *g2) const {
  return apply_bg_intersection<Cartesian_multipoint, Cartesian_multilinestring,
                               Cartesian_multipolygon>(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_point, *)

std::unique_ptr<Geometry> Intersection::eval(const Geographic_point *g1,
                                             const Geographic_point *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_point *g1, const Geographic_linestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_point *g1, const Geographic_polygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_point *g1, const Geographic_multipoint *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(*g1, *g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_point *g1, const Geographic_multilinestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_point *g1, const Geographic_multipolygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_linestring, *)

std::unique_ptr<Geometry> Intersection::eval(const Geographic_linestring *g1,
                                             const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_linestring *g1, const Geographic_linestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_linestring *g1, const Geographic_polygon *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Geographic_multipoint,
                                           Geographic_multilinestring>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_linestring *g1, const Geographic_multipoint *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_linestring *g1,
    const Geographic_multilinestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_linestring *g1, const Geographic_multipolygon *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Geographic_multipoint,
                                           Geographic_multilinestring>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_polygon, *)

std::unique_ptr<Geometry> Intersection::eval(const Geographic_polygon *g1,
                                             const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_polygon *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_polygon *g1, const Geographic_polygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_polygon *g1, const Geographic_multipoint *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_polygon *g1, const Geographic_multilinestring *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Geographic_multipoint,
                                           Geographic_multilinestring>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_polygon *g1, const Geographic_multipolygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_geometrycollection, *)

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_geometrycollection *g1, const Geometry *g2) const {
  return geometry_collection_apply_intersection(*this, g1, g2);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geometry *g1, const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_intersection(*this, g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_geometrycollection *g1,
    const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_intersection(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_multipoint, *)

std::unique_ptr<Geometry> Intersection::eval(const Geographic_multipoint *g1,
                                             const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipoint *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipoint *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipoint *g1, const Geographic_multipoint *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipoint *g1,
    const Geographic_multilinestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipoint *g1, const Geographic_multipolygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_pl_pa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_multilinestring, *)

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1,
    const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multilinestring *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipolygon *g2) const {
  // TODO: fix this in boost geometry
  return apply_bg_brute_force_intersection<Geographic_multipoint,
                                           Geographic_multilinestring>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// intersection(Geographic_multipolygon, *)

std::unique_ptr<Geometry> Intersection::eval(const Geographic_multipolygon *g1,
                                             const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipolygon *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipolygon *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipolygon *g1, const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> Intersection::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multipolygon *g2) const {
  return apply_bg_intersection<Geographic_multipoint,
                               Geographic_multilinestring,
                               Geographic_multipolygon>(
      *g1, *g2, m_geographic_ll_la_aa_strategy);
}

}  // namespace gis

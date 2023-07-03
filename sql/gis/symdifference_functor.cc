// Copyright (c) 2021, 2023, Oracle and/or its affiliates.
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
/// This file implements the symdifference functor.

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>

#include "sql/gis/gc_utils.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/so_utils.h"
#include "sql/gis/symdifference_functor.h"

namespace bg = boost::geometry;

namespace gis {

template <typename PointLike, typename Geometry,
          typename std::enable_if_t<
              std::is_same<PointLike, Cartesian_point>::value ||
              std::is_same<PointLike, Cartesian_multipoint>::value> * = nullptr>
bool call_bg_disjoint(const PointLike &g1, const Geometry &g2,
                      const SymDifference &) {
  return bg::disjoint(g1, g2);
}

template <
    typename PointLike, typename Geometry,
    typename std::enable_if_t<
        std::is_same<PointLike, Geographic_point>::value ||
        std::is_same<PointLike, Geographic_multipoint>::value> * = nullptr>
bool call_bg_disjoint(const PointLike &g1, const Geometry &g2,
                      const SymDifference &f) {
  return bg::disjoint(g1, g2, f.get_pl_pa_strategy());
}

template <typename MultipointOrLinear, typename Geometry, typename GeometryOut,
          typename std::enable_if_t<
              std::is_same<MultipointOrLinear, Cartesian_linestring>::value ||
              std::is_same<MultipointOrLinear, Cartesian_multipoint>::value ||
              std::is_same<MultipointOrLinear,
                           Cartesian_multilinestring>::value> * = nullptr>
void call_bg_difference(const MultipointOrLinear &g1, const Geometry &g2,
                        GeometryOut &gout, const SymDifference &) {
  bg::difference(g1, g2, gout);
}

template <typename Geometry, typename GeometryOut>
void call_bg_difference(const Geographic_multipoint &g1, const Geometry &g2,
                        GeometryOut &gout, const SymDifference &f) {
  bg::difference(g1, g2, gout, f.get_pl_pa_strategy());
}

template <
    typename Linear, typename Geometry, typename GeometryOut,
    typename std::enable_if_t<
        std::is_same<Linear, Geographic_linestring>::value ||
        std::is_same<Linear, Geographic_multilinestring>::value> * = nullptr>
void call_bg_difference(const Linear &g1, const Geometry &g2, GeometryOut &gout,
                        const SymDifference &f) {
  bg::difference(g1, g2, gout, f.get_ll_la_aa_strategy());
}

template <typename MultiPointType, typename Geometry1, typename Geometry2>
static auto symdifference_pointlike_pointlike(Geometry1 g1, Geometry2 g2) {
  std::unique_ptr<MultiPointType> result = std::make_unique<MultiPointType>();
  std::unique_ptr<MultiPointType> result_union =
      std::make_unique<MultiPointType>();
  std::unique_ptr<MultiPointType> result_intersection =
      std::make_unique<MultiPointType>();
  bg::union_(*g1, *g2, *result_union);
  bg::intersection(*g1, *g2, *result_intersection);
  bg::difference(*result_union, *result_intersection, *result);
  return result;
}

template <typename GCType, typename Geometry1, typename Geometry2>
static auto symdifference_multipoint_linear_or_areal(const SymDifference &f,
                                                     Geometry1 g1,
                                                     Geometry2 g2) {
  std::unique_ptr<GCType> result = std::make_unique<GCType>();
  if (!g2->is_empty()) result->push_back(*g2);
  for (auto p : *g1) {
    if (call_bg_disjoint(p, *g2, f)) result->push_back(p);
  }
  return result;
}

template <typename GCType, typename Geometry1, typename Geometry2>
static auto symdifference_point_linear_or_areal(const SymDifference &f,
                                                Geometry1 g1, Geometry2 g2) {
  std::unique_ptr<GCType> result = std::make_unique<GCType>();
  if (!g2->is_empty()) result->push_back(*g2);
  if (call_bg_disjoint(*g1, *g2, f)) result->push_back(*g1);
  return result;
}

template <typename MlsType, typename GCType, typename Linear, typename Areal>
static auto symdifference_linear_areal(const SymDifference &f, Linear g1,
                                       Areal g2) {
  std::unique_ptr<MlsType> difference = std::make_unique<MlsType>();
  call_bg_difference(*g1, *g2, *difference, f);
  std::unique_ptr<GCType> result = std::make_unique<GCType>();
  if (!g2->is_empty()) result->push_back(*g2);
  for (auto ls : *difference) result->push_back(ls);
  return result;
}

template <typename MptType, typename MlsType, typename MpyType, typename PtMpt,
          typename GC>
std::unique_ptr<Geometry> symdifference_pointlike_geomcol(
    const SymDifference &f, PtMpt g1, GC *g2) {
  if (g2->is_empty())
    return std::make_unique<typename std::decay<decltype(*g1)>::type>(*g1);
  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g2, &mpt, &mls, &mpy);
  gc_union(f.semi_major(), f.semi_minor(), &mpt, &mls, &mpy);

  std::unique_ptr<MptType> mpt_typed =
      std::make_unique<MptType>(*down_cast<MptType *>(mpt.get()));

  using GCType = typename std::decay<decltype(*g2)>::type;
  std::unique_ptr<GCType> result = std::make_unique<GCType>();

  std::unique_ptr<MptType> mpt_result =
      symdifference_pointlike_pointlike<MptType>(g1, mpt_typed.get());

  std::unique_ptr<MptType> mls_result = std::make_unique<MptType>();
  call_bg_difference(*mpt_result, *down_cast<MlsType *>(mls.get()), *mls_result,
                     f);

  std::unique_ptr<MptType> mpy_result = std::make_unique<MptType>();
  call_bg_difference(*mls_result, *down_cast<MpyType *>(mpy.get()), *mpy_result,
                     f);

  if (!mpy_result->is_empty()) result->push_back(*mpy_result);
  if (!mls->is_empty()) result->push_back(*mls);
  if (!mpy->is_empty()) result->push_back(*mpy);

  return result;
}

template <typename MptType, typename MlsType, typename MpyType, typename Linear,
          typename GC>
std::unique_ptr<Geometry> symdifference_linear_geomcol(const SymDifference &f,
                                                       Linear g1, GC *g2) {
  if (g2->is_empty())
    return std::make_unique<typename std::decay<decltype(*g1)>::type>(*g1);
  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g2, &mpt, &mls, &mpy);
  gc_union(f.semi_major(), f.semi_minor(), &mpt, &mls, &mpy);

  using GCType = typename std::decay<decltype(*g2)>::type;
  std::unique_ptr<GCType> result = std::make_unique<GCType>();

  if (!mpy->is_empty()) result->push_back(*mpy);

  std::unique_ptr<MlsType> mls_result = std::make_unique<MlsType>();
  call_bg_difference(*g1, *down_cast<MpyType *>(mpy.get()), *mls_result, f);

  auto mls_symdiff_result = f(mls.get(), mls_result.get());
  if (!mls_symdiff_result->is_empty()) result->push_back(*mls_symdiff_result);

  std::unique_ptr<MptType> mpt_result = std::make_unique<MptType>();
  call_bg_difference(*down_cast<MptType *>(mpt.get()), *g1, *mpt_result, f);
  if (!mpt_result->is_empty()) result->push_back(*mpt_result);

  return result;
}

template <typename MptType, typename MlsType, typename Areal, typename GC>
std::unique_ptr<Geometry> symdifference_areal_geomcol(const SymDifference &f,
                                                      Areal g1, GC *g2) {
  if (g2->is_empty())
    return std::make_unique<typename std::decay<decltype(*g1)>::type>(*g1);
  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g2, &mpt, &mls, &mpy);
  gc_union(f.semi_major(), f.semi_minor(), &mpt, &mls, &mpy);

  using GCType = typename std::decay<decltype(*g2)>::type;
  std::unique_ptr<GCType> result = std::make_unique<GCType>();

  auto mpy_result = f(g1, mpy.get());
  if (!mpy_result->is_empty()) result->push_back(*mpy_result);

  std::unique_ptr<MlsType> mls_result = std::make_unique<MlsType>();

  call_bg_difference(*down_cast<MlsType *>(mls.get()), *g1, *mls_result, f);
  if (!mls_result->is_empty()) result->push_back(*mls_result);

  std::unique_ptr<MptType> mpt_result = std::make_unique<MptType>();
  call_bg_difference(*down_cast<MptType *>(mpt.get()), *g1, *mpt_result, f);
  if (!mpt_result->is_empty()) result->push_back(*mpt_result);

  return result;
}

SymDifference::SymDifference(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

std::unique_ptr<Geometry> SymDifference::operator()(const Geometry *g1,
                                                    const Geometry *g2) const {
  std::unique_ptr<Geometry> result = apply(*this, g1, g2);
  if (!result->is_empty()) {
    remove_duplicates(this->semi_major(), this->semi_minor(), &result);
    narrow_geometry(&result);
  }
  return result;
}

std::unique_ptr<Geometry> SymDifference::eval(const Geometry *g1,
                                              const Geometry *g2) const {
  assert(false);
  throw not_implemented_exception::for_non_projected(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_point, *)

std::unique_ptr<Cartesian_multipoint> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_point *g2) const {
  return symdifference_pointlike_pointlike<Cartesian_multipoint>(g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_linestring *g2) const {
  return symdifference_point_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_polygon *g2) const {
  return symdifference_point_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Cartesian_multipoint> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_multipoint *g2) const {
  return symdifference_pointlike_pointlike<Cartesian_multipoint>(g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_multilinestring *g2) const {
  return symdifference_point_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_multipolygon *g2) const {
  return symdifference_point_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_point *g1, const Cartesian_geometrycollection *g2) const {
  return symdifference_pointlike_geomcol<
      Cartesian_multipoint, Cartesian_multilinestring, Cartesian_multipolygon>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_linestring, *)

std::unique_ptr<Geometry> SymDifference::eval(const Cartesian_linestring *g1,
                                              const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multilinestring> SymDifference::eval(
    const Cartesian_linestring *g1, const Cartesian_linestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_linestring *g1, const Cartesian_polygon *g2) const {
  return symdifference_linear_areal<Cartesian_multilinestring,
                                    Cartesian_geometrycollection>(*this, g1,
                                                                  g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_linestring *g1, const Cartesian_multipoint *g2) const {
  return symdifference_multipoint_linear_or_areal<Cartesian_geometrycollection>(
      *this, g2, g1);
}

std::unique_ptr<Cartesian_multilinestring> SymDifference::eval(
    const Cartesian_linestring *g1, const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result =
      std::make_unique<Cartesian_multilinestring>();
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_linestring *g1, const Cartesian_multipolygon *g2) const {
  return symdifference_linear_areal<Cartesian_multilinestring,
                                    Cartesian_geometrycollection>(*this, g1,
                                                                  g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_linestring *g1,
    const Cartesian_geometrycollection *g2) const {
  return symdifference_linear_geomcol<
      Cartesian_multipoint, Cartesian_multilinestring, Cartesian_multipolygon>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_polygon, *)

std::unique_ptr<Geometry> SymDifference::eval(const Cartesian_polygon *g1,
                                              const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipolygon> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_polygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_multipoint *g2) const {
  return symdifference_multipoint_linear_or_areal<Cartesian_geometrycollection>(
      *this, g2, g1);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_multilinestring *g2) const {
  return symdifference_linear_areal<Cartesian_multilinestring,
                                    Cartesian_geometrycollection>(*this, g2,
                                                                  g1);
}

std::unique_ptr<Cartesian_multipolygon> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_polygon *g1, const Cartesian_geometrycollection *g2) const {
  return symdifference_areal_geomcol<Cartesian_multipoint,
                                     Cartesian_multilinestring>(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_geometrycollection, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_geometrycollection *g1,
    const Cartesian_geometrycollection *g2) const {
  if (g1->is_empty()) {
    if (g2->is_empty()) {
      return std::make_unique<Cartesian_geometrycollection>();
    }
    return std::make_unique<Cartesian_geometrycollection>(*g2);
  }

  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g1, &mpt, &mls, &mpy);
  gc_union(this->semi_major(), this->semi_minor(), &mpt, &mls, &mpy);

  auto mpy_result = (*this)(mpy.get(), g2);
  auto mls_result = (*this)(mls.get(), mpy_result.get());
  auto mpt_result = (*this)(mpt.get(), mls_result.get());

  return mpt_result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_geometrycollection *g1, const Geometry *g2) const {
  return (*this)(g2, g1);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_multipoint, *)

std::unique_ptr<Geometry> SymDifference::eval(const Cartesian_multipoint *g1,
                                              const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipoint *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipoint *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipoint> SymDifference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipoint *g2) const {
  return symdifference_pointlike_pointlike<Cartesian_multipoint>(g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multilinestring *g2) const {
  return symdifference_multipoint_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_multipoint *g1, const Cartesian_multipolygon *g2) const {
  return symdifference_multipoint_linear_or_areal<Cartesian_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipoint *g1,
    const Cartesian_geometrycollection *g2) const {
  return symdifference_pointlike_geomcol<
      Cartesian_multipoint, Cartesian_multilinestring, Cartesian_multipolygon>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_multilinestring, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multilinestring *g1, const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multilinestring> SymDifference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Cartesian_geometrycollection> SymDifference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_multipolygon *g2) const {
  return symdifference_linear_areal<Cartesian_multilinestring,
                                    Cartesian_geometrycollection>(*this, g1,
                                                                  g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multilinestring *g1,
    const Cartesian_geometrycollection *g2) const {
  return symdifference_linear_geomcol<
      Cartesian_multipoint, Cartesian_multilinestring, Cartesian_multipolygon>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Cartesian_multipolygon, *)

std::unique_ptr<Geometry> SymDifference::eval(const Cartesian_multipolygon *g1,
                                              const Cartesian_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Cartesian_multipolygon> SymDifference::eval(
    const Cartesian_multipolygon *g1, const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result =
      std::make_unique<Cartesian_multipolygon>();
  bg::sym_difference(*g1, *g2, *result);
  return result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Cartesian_multipolygon *g1,
    const Cartesian_geometrycollection *g2) const {
  return symdifference_areal_geomcol<Cartesian_multipoint,
                                     Cartesian_multilinestring>(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_point, *)

std::unique_ptr<Geographic_multipoint> SymDifference::eval(
    const Geographic_point *g1, const Geographic_point *g2) const {
  return symdifference_pointlike_pointlike<Geographic_multipoint>(g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_point *g1, const Geographic_linestring *g2) const {
  return symdifference_point_linear_or_areal<Geographic_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_point *g1, const Geographic_polygon *g2) const {
  return symdifference_point_linear_or_areal<Geographic_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geographic_multipoint> SymDifference::eval(
    const Geographic_point *g1, const Geographic_multipoint *g2) const {
  return symdifference_pointlike_pointlike<Geographic_multipoint>(g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_point *g1, const Geographic_multilinestring *g2) const {
  return symdifference_point_linear_or_areal<Geographic_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_point *g1, const Geographic_multipolygon *g2) const {
  return symdifference_point_linear_or_areal<Geographic_geometrycollection>(
      *this, g1, g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_point *g1, const Geographic_geometrycollection *g2) const {
  return symdifference_pointlike_geomcol<Geographic_multipoint,
                                         Geographic_multilinestring,
                                         Geographic_multipolygon>(*this, g1,
                                                                  g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_linestring, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_linestring *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multilinestring> SymDifference::eval(
    const Geographic_linestring *g1, const Geographic_linestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_linestring *g1, const Geographic_polygon *g2) const {
  return symdifference_linear_areal<Geographic_multilinestring,
                                    Geographic_geometrycollection>(*this, g1,
                                                                   g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_linestring *g1, const Geographic_multipoint *g2) const {
  return symdifference_multipoint_linear_or_areal<
      Geographic_geometrycollection>(*this, g2, g1);
}

std::unique_ptr<Geographic_multilinestring> SymDifference::eval(
    const Geographic_linestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result =
      std::make_unique<Geographic_multilinestring>();
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_linestring *g1, const Geographic_multipolygon *g2) const {
  return symdifference_linear_areal<Geographic_multilinestring,
                                    Geographic_geometrycollection>(*this, g1,
                                                                   g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_linestring *g1,
    const Geographic_geometrycollection *g2) const {
  return symdifference_linear_geomcol<Geographic_multipoint,
                                      Geographic_multilinestring,
                                      Geographic_multipolygon>(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_polygon, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipolygon> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_polygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_multipoint *g2) const {
  return symdifference_multipoint_linear_or_areal<
      Geographic_geometrycollection>(*this, g2, g1);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_multilinestring *g2) const {
  return symdifference_linear_areal<Geographic_multilinestring,
                                    Geographic_geometrycollection>(*this, g2,
                                                                   g1);
}

std::unique_ptr<Geographic_multipolygon> SymDifference::eval(
    const Geographic_polygon *g1, const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_polygon *g1,
    const Geographic_geometrycollection *g2) const {
  return symdifference_areal_geomcol<Geographic_multipoint,
                                     Geographic_multilinestring>(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_geometrycollection, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_geometrycollection *g1,
    const Geographic_geometrycollection *g2) const {
  if (g1->is_empty()) {
    if (g2->is_empty()) {
      return std::make_unique<Geographic_geometrycollection>();
    }
    return std::make_unique<Geographic_geometrycollection>(*g2);
  }

  std::unique_ptr<Multipoint> mpt;
  std::unique_ptr<Multilinestring> mls;
  std::unique_ptr<Multipolygon> mpy;
  split_gc(g1, &mpt, &mls, &mpy);
  gc_union(this->semi_major(), this->semi_minor(), &mpt, &mls, &mpy);

  auto mpy_result = (*this)(mpy.get(), g2);
  auto mls_result = (*this)(mls.get(), mpy_result.get());
  auto mpt_result = (*this)(mpt.get(), mls_result.get());

  return mpt_result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_geometrycollection *g1, const Geometry *g2) const {
  return (*this)(g2, g1);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_multipoint, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipoint *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipoint *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipoint *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipoint> SymDifference::eval(
    const Geographic_multipoint *g1, const Geographic_multipoint *g2) const {
  return symdifference_pointlike_pointlike<Geographic_multipoint>(g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_multipoint *g1,
    const Geographic_multilinestring *g2) const {
  return symdifference_multipoint_linear_or_areal<
      Geographic_geometrycollection>(*this, g1, g2);
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_multipoint *g1, const Geographic_multipolygon *g2) const {
  return symdifference_multipoint_linear_or_areal<
      Geographic_geometrycollection>(*this, g1, g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipoint *g1,
    const Geographic_geometrycollection *g2) const {
  return symdifference_pointlike_geomcol<Geographic_multipoint,
                                         Geographic_multilinestring,
                                         Geographic_multipolygon>(*this, g1,
                                                                  g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_multilinestring, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multilinestring *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multilinestring *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multilinestring> SymDifference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result(
      new Geographic_multilinestring());
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geographic_geometrycollection> SymDifference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_multipolygon *g2) const {
  return symdifference_linear_areal<Geographic_multilinestring,
                                    Geographic_geometrycollection>(*this, g1,
                                                                   g2);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multilinestring *g1,
    const Geographic_geometrycollection *g2) const {
  return symdifference_linear_geomcol<Geographic_multipoint,
                                      Geographic_multilinestring,
                                      Geographic_multipolygon>(*this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// symdifference(Geographic_multipolygon, *)

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1, const Geographic_point *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1, const Geographic_linestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1, const Geographic_polygon *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1, const Geographic_multipoint *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multilinestring *g2) const {
  return (*this)(g2, g1);
}

std::unique_ptr<Geographic_multipolygon> SymDifference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result =
      std::make_unique<Geographic_multipolygon>();
  bg::sym_difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result;
}

std::unique_ptr<Geometry> SymDifference::eval(
    const Geographic_multipolygon *g1,
    const Geographic_geometrycollection *g2) const {
  return symdifference_areal_geomcol<Geographic_multipoint,
                                     Geographic_multilinestring>(*this, g1, g2);
}

}  // namespace gis

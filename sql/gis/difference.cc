// Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, 51 Franklin
// Street, Suite 500, Boston, MA 02110-1335 USA.

/// @file
///
/// This file implements the difference functor and function.

#include <memory>  // std::unique_ptr

#include <boost/geometry.hpp>

#include "sql/gis/difference_functor.h"
#include "sql/gis/disjoint_functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_traits.h"

namespace bg = boost::geometry;

namespace gis {

Difference::Difference(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

Geometry *Difference::operator()(const Geometry *g1, const Geometry *g2) const {
  return apply(*this, g1, g2);
}

Geometry *Difference::eval(const Geometry *g1, const Geometry *g2) const {
  DBUG_ASSERT(false);
  throw not_implemented_exception(g1->coordinate_system(), g1->type(),
                                  g2->type());
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_linestring, *)

Geometry *Difference::eval(const Cartesian_linestring *g1,
                           const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multipoint, *)

Geometry *Difference::eval(const Cartesian_multipoint *g1,
                           const Cartesian_multipoint *g2) const {
  std::unique_ptr<Cartesian_multipoint> result(new Cartesian_multipoint());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

Geometry *Difference::eval(const Cartesian_multipoint *g1,
                           const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multipoint> result(new Cartesian_multipoint());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

Geometry *Difference::eval(const Cartesian_multipoint *g1,
                           const Cartesian_multipolygon *g2) const {
  Disjoint disjoint(m_semi_major, m_semi_minor);
  std::set<Cartesian_point> points;
  for (auto &pt : *g1)
    if (disjoint(&pt, g2)) points.insert(pt);

  std::unique_ptr<Cartesian_multipoint> result(new Cartesian_multipoint());
  for (auto &pt : points) result->push_back(pt);

  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multilinestring, *)

Geometry *Difference::eval(const Cartesian_multilinestring *g1,
                           const Cartesian_multilinestring *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

Geometry *Difference::eval(const Cartesian_multilinestring *g1,
                           const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multilinestring> result(
      new Cartesian_multilinestring());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Cartesian_multipolygon, *)

Geometry *Difference::eval(const Cartesian_multipolygon *g1,
                           const Cartesian_multipolygon *g2) const {
  std::unique_ptr<Cartesian_multipolygon> result(new Cartesian_multipolygon());
  bg::difference(*g1, *g2, *result);
  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_linestring, *)

Geometry *Difference::eval(const Geographic_linestring *g1,
                           const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result(
      new Geographic_multilinestring());
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multipoint, *)

Geometry *Difference::eval(const Geographic_multipoint *g1,
                           const Geographic_multipoint *g2) const {
  std::unique_ptr<Geographic_multipoint> result(new Geographic_multipoint());
  bg::difference(*g1, *g2, *result);  // Default strategy is OK.
  return result.release();
}

Geometry *Difference::eval(const Geographic_multipoint *g1,
                           const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multipoint> result(new Geographic_multipoint());
  bg::difference(*g1, *g2, *result, m_geographic_pl_pa_strategy);
  return result.release();
}

Geometry *Difference::eval(const Geographic_multipoint *g1,
                           const Geographic_multipolygon *g2) const {
  Disjoint disjoint(m_semi_major, m_semi_minor);
  std::set<Geographic_point> points;
  for (auto &pt : *g1)
    if (disjoint(&pt, g2)) points.insert(pt);

  std::unique_ptr<Geographic_multipoint> result(new Geographic_multipoint());
  for (auto &pt : points) result->push_back(pt);

  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multilinestring, *)

Geometry *Difference::eval(const Geographic_multilinestring *g1,
                           const Geographic_multilinestring *g2) const {
  std::unique_ptr<Geographic_multilinestring> result(
      new Geographic_multilinestring());
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result.release();
}

Geometry *Difference::eval(const Geographic_multilinestring *g1,
                           const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multilinestring> result(
      new Geographic_multilinestring());
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result.release();
}

//////////////////////////////////////////////////////////////////////////////

// difference(Geographic_multipolygon, *)

Geometry *Difference::eval(const Geographic_multipolygon *g1,
                           const Geographic_multipolygon *g2) const {
  std::unique_ptr<Geographic_multipolygon> result(
      new Geographic_multipolygon());
  bg::difference(*g1, *g2, *result, m_geographic_ll_la_aa_strategy);
  return result.release();
}

}  // namespace gis

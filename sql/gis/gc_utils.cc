// Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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
/// This file implements utility functions for working with geometrycollections.

#include "sql/gis/gc_utils.h"

#include <assert.h>
#include <boost/geometry.hpp>  // boost::geometry::difference

// assert
#include "sql/gis/difference_functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometries_traits.h"
#include "sql/gis/union_functor.h"
#include "template_utils.h"  // down_cast

namespace bg = boost::geometry;

namespace gis {

template <typename Pt, typename Ls, typename Py, typename GC, typename MPt,
          typename MLs, typename MPy>
static void typed_split_gc(const GC *gc, MPt *mpt, MLs *mls, MPy *mpy) {
  assert(gc->coordinate_system() == mpt->coordinate_system() &&
         gc->coordinate_system() == mls->coordinate_system() &&
         gc->coordinate_system() == mpy->coordinate_system());

  for (const auto g : *gc) {
    switch (g->type()) {
      case Geometry_type::kPoint:
        mpt->push_back(*down_cast<Pt *>(g));
        break;
      case Geometry_type::kLinestring:
        mls->push_back(*down_cast<Ls *>(g));
        break;
      case Geometry_type::kPolygon:
        mpy->push_back(*down_cast<Py *>(g));
        break;
      case Geometry_type::kGeometrycollection:
        typed_split_gc<Pt, Ls, Py, GC, MPt, MLs, MPy>(down_cast<GC *>(g), mpt,
                                                      mls, mpy);
        break;
      case Geometry_type::kMultipoint: {
        const MPt *m = down_cast<const MPt *>(g);
        for (std::size_t i = 0; i < m->size(); i++)
          mpt->push_back(static_cast<const Pt &>((*m)[i]));
        break;
      }
      case Geometry_type::kMultilinestring: {
        const MLs *m = down_cast<const MLs *>(g);
        for (std::size_t i = 0; i < m->size(); i++)
          mls->push_back(static_cast<const Ls &>((*m)[i]));
        break;
      }
      case Geometry_type::kMultipolygon: {
        const MPy *m = down_cast<const MPy *>(g);
        for (std::size_t i = 0; i < m->size(); i++)
          mpy->push_back(static_cast<const Py &>((*m)[i]));
        break;
      }
      case Geometry_type::kGeometry:
        assert(false);
        break;
    }
  }
}

void split_gc(const Geometrycollection *gc, std::unique_ptr<Multipoint> *mpt,
              std::unique_ptr<Multilinestring> *mls,
              std::unique_ptr<Multipolygon> *mpy) {
  switch (gc->coordinate_system()) {
    case Coordinate_system::kCartesian:
      mpt->reset(new Cartesian_multipoint());
      mls->reset(new Cartesian_multilinestring());
      mpy->reset(new Cartesian_multipolygon());
      typed_split_gc<Cartesian_point, Cartesian_linestring, Cartesian_polygon,
                     Cartesian_geometrycollection, Cartesian_multipoint,
                     Cartesian_multilinestring, Cartesian_multipolygon>(
          down_cast<const Cartesian_geometrycollection *>(gc),
          down_cast<Cartesian_multipoint *>(mpt->get()),
          down_cast<Cartesian_multilinestring *>(mls->get()),
          down_cast<Cartesian_multipolygon *>(mpy->get()));
      break;
    case Coordinate_system::kGeographic:
      mpt->reset(new Geographic_multipoint());
      mls->reset(new Geographic_multilinestring());
      mpy->reset(new Geographic_multipolygon());
      typed_split_gc<Geographic_point, Geographic_linestring,
                     Geographic_polygon, Geographic_geometrycollection,
                     Geographic_multipoint, Geographic_multilinestring,
                     Geographic_multipolygon>(
          down_cast<const Geographic_geometrycollection *>(gc),
          down_cast<Geographic_multipoint *>(mpt->get()),
          down_cast<Geographic_multilinestring *>(mls->get()),
          down_cast<Geographic_multipolygon *>(mpy->get()));
      break;
  }
}

template <typename MPt, typename MLs, typename MPy>
void typed_gc_union(double semi_major, double semi_minor,
                    std::unique_ptr<Multipoint> *mpt,
                    std::unique_ptr<Multilinestring> *mls,
                    std::unique_ptr<Multipolygon> *mpy) {
  Difference difference(semi_major, semi_minor);
  Union union_(semi_major, semi_minor);

  std::unique_ptr<MPy> polygons(new MPy());
  for (auto &py : *down_cast<MPy *>(mpy->get())) {
    std::unique_ptr<Geometry> union_result = union_(polygons.get(), &py);
    if (union_result->type() == Geometry_type::kPolygon) {
      polygons->clear();
      polygons->push_back(*union_result);
    } else if (union_result->type() == Geometry_type::kMultipolygon) {
      polygons.reset(new MPy(*down_cast<MPy *>(union_result.get())));
    }
    if (polygons->coordinate_system() == Coordinate_system::kGeographic &&
        polygons->is_empty()) {
      // The result of a union between a geographic multipolygon and a
      // geographic polygon is empty. There are two reasons why this may happen:
      //
      // 1. One of the polygons involved are invalid.
      // 2. One of the polygons involved covers half the globe, or more.
      //
      // Since invalid input is only reported to the extent it is explicitly
      // detected, we can simply return a too large polygon error in both cases.
      throw too_large_polygon_exception();
    }
  }

  std::unique_ptr<MLs> linestrings = std::make_unique<MLs>();
  std::unique_ptr<Geometry> ls_difference(
      difference(mls->get(), polygons.get()));
  if (ls_difference->type() == Geometry_type::kLinestring)
    linestrings->push_back(*ls_difference);
  else
    linestrings.reset(down_cast<MLs *>(ls_difference.release()));

  std::unique_ptr<MPt> points = std::make_unique<MPt>();
  std::unique_ptr<Geometry> pt_difference(
      difference(mpt->get(), linestrings.get()));
  pt_difference = difference(pt_difference.get(), polygons.get());

  if (pt_difference->type() == Geometry_type::kPoint) {
    points->push_back(*pt_difference);
  } else
    points.reset(down_cast<MPt *>(pt_difference.release()));

  mpy->reset(polygons.release());
  mls->reset(linestrings.release());
  mpt->reset(points.release());
}

void gc_union(double semi_major, double semi_minor,
              std::unique_ptr<Multipoint> *mpt,
              std::unique_ptr<Multilinestring> *mls,
              std::unique_ptr<Multipolygon> *mpy) {
  assert(mpt->get() && mls->get() && mpy->get());
  assert((*mpt)->coordinate_system() == (*mls)->coordinate_system() &&
         (*mpt)->coordinate_system() == (*mpy)->coordinate_system());
  // We're using empty GCs to detect invalid geometries, so empty geometry
  // collections should be filtered out before calling gc_union.
  assert(!(*mpt)->empty() || !(*mls)->empty() || !(*mpy)->empty());

  switch ((*mpt)->coordinate_system()) {
    case Coordinate_system::kCartesian: {
      typed_gc_union<Cartesian_multipoint, Cartesian_multilinestring,
                     Cartesian_multipolygon>(semi_major, semi_minor, mpt, mls,
                                             mpy);
      break;
    }
    case Coordinate_system::kGeographic: {
      typed_gc_union<Geographic_multipoint, Geographic_multilinestring,
                     Geographic_multipolygon>(semi_major, semi_minor, mpt, mls,
                                              mpy);
      break;
    }
  }

  // If all collections are empty, we've encountered at least one invalid
  // geometry.
  if ((*mpt)->empty() && (*mls)->empty() && (*mpy)->empty())
    throw invalid_geometry_exception();

  assert(mpt->get() && mls->get() && mpy->get());
  assert(!(*mpt)->empty() || !(*mls)->empty() || !(*mpy)->empty());
}

std::unique_ptr<gis::Geometrycollection> narrowest_multigeometry(
    std::unique_ptr<gis::Geometrycollection> geometrycollection) {
  bool pt = false;
  bool ls = false;
  bool py = false;
  for (size_t i = 0; i < geometrycollection->size(); i++) {
    switch (geometrycollection->operator[](i).type()) {
      case gis::Geometry_type::kPoint:
        if (ls || py) return geometrycollection;
        pt = true;
        break;
      case gis::Geometry_type::kLinestring:
        if (pt || py) return geometrycollection;
        ls = true;
        break;
      case gis::Geometry_type::kPolygon:
        if (pt || ls) return geometrycollection;
        py = true;
        break;
      case gis::Geometry_type::kMultipoint:
      case gis::Geometry_type::kMultilinestring:
      case gis::Geometry_type::kMultipolygon:
      case gis::Geometry_type::kGeometrycollection:
        return geometrycollection;
      default:
        break;
    }
  }

  // Otherwise create the necessary multigeometries and split the input.
  std::unique_ptr<gis::Multipoint> multipoint =
      std::unique_ptr<gis::Multipoint>(gis::Multipoint::create_multipoint(
          geometrycollection->coordinate_system()));
  std::unique_ptr<gis::Multilinestring> multilinestring =
      std::unique_ptr<gis::Multilinestring>(
          gis::Multilinestring::create_multilinestring(
              geometrycollection->coordinate_system()));
  std::unique_ptr<gis::Multipolygon> multipolygon =
      std::unique_ptr<gis::Multipolygon>(gis::Multipolygon::create_multipolygon(
          geometrycollection->coordinate_system()));
  gis::split_gc(geometrycollection.get(), &multipoint, &multilinestring,
                &multipolygon);

  if (!multipoint->empty())
    return std::unique_ptr<gis::Geometrycollection>(multipoint.release());
  else if (!multilinestring->empty())
    return std::unique_ptr<gis::Geometrycollection>(multilinestring.release());
  else if (!multipolygon->empty())
    return std::unique_ptr<gis::Geometrycollection>(multipolygon.release());
  assert(false);
  return geometrycollection;
}

}  // namespace gis

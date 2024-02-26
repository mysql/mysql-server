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
/// This file implements utility functions for spatial operations
/// (union, intersection, difference, symdifference).

#include "sql/gis/so_utils.h"
#include "sql/gis/equals_functor.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometries_traits.h"
#include "template_utils.h"  // down_cast

namespace gis {

template <typename MPt, typename MLs, typename MPy, typename GC>
static void typed_remove_duplicates(double semi_major, double semi_minor,
                                    std::unique_ptr<Geometry> *g) {
  Equals equals(semi_major, semi_minor);
  switch (g->get()->type()) {
    case Geometry_type::kPoint:
    case Geometry_type::kLinestring:
    case Geometry_type::kPolygon:
      break;
    case Geometry_type::kMultipoint: {
      std::unique_ptr<MPt> mpt = std::make_unique<MPt>();
      for (auto pt : *down_cast<MPt *>(g->get())) {
        bool include = true;
        for (auto pt2 : *mpt) {
          if (equals(&pt, &pt2)) {
            include = false;
            break;
          }
        }
        if (include) mpt->push_back(pt);
      }
      g->reset(mpt.release());
      break;
    }
    case Geometry_type::kMultilinestring: {
      std::unique_ptr<MLs> mls = std::make_unique<MLs>();
      for (auto ls : *down_cast<MLs *>(g->get())) {
        bool include = true;
        for (auto ls2 : *mls) {
          if (equals(&ls, &ls2)) {
            include = false;
            break;
          }
        }
        if (include) mls->push_back(ls);
      }
      g->reset(mls.release());
      break;
    }
    case Geometry_type::kMultipolygon: {
      std::unique_ptr<MPy> mpy = std::make_unique<MPy>();
      for (auto py : *down_cast<MPy *>(g->get())) {
        bool include = true;
        for (auto py2 : *mpy) {
          if (equals(&py, &py2)) {
            include = false;
            break;
          }
        }
        if (include) mpy->push_back(py);
      }
      g->reset(mpy.release());
      break;
    }
    case Geometry_type::kGeometrycollection: {
      std::unique_ptr<GC> gc = std::make_unique<GC>();
      for (auto g1 : *down_cast<GC *>(g->get())) {
        if (g1->is_empty()) continue;
        std::unique_ptr<Geometry> g1_ptr(g1->clone());
        typed_remove_duplicates<MPt, MLs, MPy, GC>(semi_major, semi_minor,
                                                   &g1_ptr);
        bool include = true;
        for (auto g2 : *gc) {
          if (equals(g1_ptr.get(), g2)) {
            include = false;
            break;
          }
        }
        if (include) gc->push_back(*g1_ptr);
      }
      g->reset(gc.release());
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
}

void remove_duplicates(double semi_major, double semi_minor,
                       std::unique_ptr<Geometry> *g) {
  switch (g->get()->coordinate_system()) {
    case Coordinate_system::kCartesian:
      typed_remove_duplicates<Cartesian_multipoint, Cartesian_multilinestring,
                              Cartesian_multipolygon,
                              Cartesian_geometrycollection>(semi_major,
                                                            semi_minor, g);
      break;
    case Coordinate_system::kGeographic:
      typed_remove_duplicates<Geographic_multipoint, Geographic_multilinestring,
                              Geographic_multipolygon,
                              Geographic_geometrycollection>(semi_major,
                                                             semi_minor, g);
      break;
    default:
      break;
  }
}

void narrow_geometry(std::unique_ptr<Geometry> *g) {
  switch (g->get()->type()) {
    case Geometry_type::kPoint:
    case Geometry_type::kLinestring:
    case Geometry_type::kPolygon: {
      break;
    }
    case Geometry_type::kMultipoint:
    case Geometry_type::kMultilinestring:
    case Geometry_type::kMultipolygon:
    case Geometry_type::kGeometrycollection: {
      Geometrycollection *gc = down_cast<Geometrycollection *>(g->get());
      if (gc->size() == 1) {
        g->reset((*gc)[0].clone());
        gc = nullptr;
        narrow_geometry(g);
      }
      break;
    }
    default: {
      // All Geometry types have been checked.
      assert(false);
      break;
    }
  }
}

}  // namespace gis

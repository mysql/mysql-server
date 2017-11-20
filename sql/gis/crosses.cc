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
/// This file implements the crosses functor and function.

#include <boost/geometry.hpp>
#include <memory>  // std::unique_ptr

#include "crosses_functor.h"
#include "disjoint_functor.h"
#include "gc_utils.h"
#include "geometries.h"
#include "geometries_traits.h"
#include "relops.h"
#include "sql/dd/types/spatial_reference_system.h" // dd::Spatial_reference_system
#include "sql/sql_exception_handler.h" // handle_gis_exception
#include "within_functor.h"

namespace bg = boost::geometry;

namespace gis {

/// Apply a Crosses functor to two geometries, which both may be geometry
/// collections, and return the booelan result of the functor applied on each
/// combination of elements in the collections.
///
/// @tparam GC Coordinate specific gometry collection type.
///
/// @param f Functor to apply.
/// @param g1 First geometry.
/// @param g2 Second geometry.
///
/// @retval true g1 crosses g2.
/// @retval false g1 doesn't cross g2.
template <typename GC>
static bool geometry_collection_apply_crosses(const Crosses &f,
                                              const Geometry *g1,
                                              const Geometry *g2) {
  if (g1->type() == Geometry_type::kGeometrycollection) {
    std::unique_ptr<Multipoint> g1_mpt;
    std::unique_ptr<Multilinestring> g1_mls;
    std::unique_ptr<Multipolygon> g1_mpy;
    split_gc(down_cast<const Geometrycollection *>(g1), &g1_mpt, &g1_mls,
             &g1_mpy);
    if (!g1_mpy->empty()) throw null_value_exception();
    gc_union(f.semi_major(), f.semi_minor(), &g1_mpt, &g1_mls, &g1_mpy);

    if (g2->type() == Geometry_type::kGeometrycollection) {
      std::unique_ptr<Multipoint> g2_mpt;
      std::unique_ptr<Multilinestring> g2_mls;
      std::unique_ptr<Multipolygon> g2_mpy;
      split_gc(down_cast<const Geometrycollection *>(g2), &g2_mpt, &g2_mls,
               &g2_mpy);
      if (!g2_mpt->empty() && g2_mls->empty() && g2_mpy->empty())
        throw null_value_exception();
      gc_union(f.semi_major(), f.semi_minor(), &g2_mpt, &g2_mls, &g2_mpy);

      return ((!g1_mpt->empty() && !g2_mls->empty() &&
               f(g1_mpt.get(), g2_mls.get())) ||
              (!g1_mpt->empty() && !g2_mpy->empty() &&
               f(g1_mpt.get(), g2_mpy.get())) ||
              (!g1_mls->empty() && !g2_mls->empty() &&
               f(g1_mls.get(), g2_mls.get())) ||
              (!g1_mls->empty() && !g2_mpy->empty() &&
               f(g1_mls.get(), g2_mpy.get())));
    } else {
      return (!g1_mpt->empty() && f(g1_mpt.get(), g2)) ||
             (!g1_mls->empty() && f(g1_mls.get(), g2));
    }
  } else {
    if (g2->type() == Geometry_type::kGeometrycollection) {
      std::unique_ptr<Multipoint> g2_mpt;
      std::unique_ptr<Multilinestring> g2_mls;
      std::unique_ptr<Multipolygon> g2_mpy;
      split_gc(down_cast<const Geometrycollection *>(g2), &g2_mpt, &g2_mls,
               &g2_mpy);
      if (g1->type() == Geometry_type::kPolygon ||
          g1->type() == Geometry_type::kMultipolygon ||
          (!g2_mpt->empty() && g2_mls->empty() && g2_mpy->empty()))
        throw null_value_exception();
      gc_union(f.semi_major(), f.semi_minor(), &g2_mpt, &g2_mls, &g2_mpy);

      return ((!g2_mls->empty() && f(g1, g2_mls.get())) ||
              (!g2_mpy->empty() && f(g1, g2_mpy.get())));
    } else {
      return f(g1, g2);
    }
  }
}

Crosses::Crosses(double semi_major, double semi_minor)
    : m_semi_major(semi_major),
      m_semi_minor(semi_minor),
      m_geographic_pl_pa_strategy(bg::strategy::side::geographic<>(
          bg::srs::spheroid<double>(semi_major, semi_minor))),
      m_geographic_ll_la_aa_strategy(
          bg::srs::spheroid<double>(semi_major, semi_minor)) {}

bool Crosses::operator()(const Geometry *g1, const Geometry *g2) const {
  return apply(*this, g1, g2);
}

bool Crosses::eval(const Geometry *g1, const Geometry *g2) const {
  // All parameter type combinations have been implemented.
  DBUG_ASSERT(false);
  throw not_implemented_exception(g1->coordinate_system(), g1->type(),
                                  g2->type());
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_point, *)

bool Crosses::eval(const Cartesian_point *g1, const Cartesian_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_linestring *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_polygon *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_geometrycollection *g2) const {
  // Must be evaluated in case g2 contains a single point.
  return geometry_collection_apply_crosses<Cartesian_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_multilinestring *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Cartesian_point *g1,
                   const Cartesian_multipolygon *g2) const {
  // A point may never cross another geometry.
  return false;
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_linestring, *)

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_linestring *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_polygon *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Cartesian_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_multilinestring *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_linestring *g1,
                   const Cartesian_multipolygon *g2) const {
  return bg::crosses(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_polygon, *)

bool Crosses::eval(const Cartesian_polygon *g1, const Geometry *g2) const {
  // If g1 is a 2d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_geometrycollection, *)

bool Crosses::eval(const Cartesian_geometrycollection *g1,
                   const Geometry *g2) const {
  return geometry_collection_apply_crosses<Cartesian_geometrycollection>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_multipoint, *)

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_linestring *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_polygon *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Cartesian_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_multilinestring *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Cartesian_multipoint *g1,
                   const Cartesian_multipolygon *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_multilinestring, *)

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_linestring *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_polygon *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Cartesian_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_multilinestring *g2) const {
  return bg::crosses(*g1, *g2);
}

bool Crosses::eval(const Cartesian_multilinestring *g1,
                   const Cartesian_multipolygon *g2) const {
  return bg::crosses(*g1, *g2);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Cartesian_multipolygon, *)

bool Crosses::eval(const Cartesian_multipolygon *g1, const Geometry *g2) const {
  // If g1 is a 2d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_point, *)

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_linestring *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_polygon *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_geometrycollection *g2) const {
  // Must be evaluated in case g2 contains a single point.
  return geometry_collection_apply_crosses<Geographic_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_multilinestring *g2) const {
  // A point may never cross another geometry.
  return false;
}

bool Crosses::eval(const Geographic_point *g1,
                   const Geographic_multipolygon *g2) const {
  // A point may never cross another geometry.
  return false;
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_linestring, *)

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_linestring *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_polygon *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Geographic_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_multilinestring *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_linestring *g1,
                   const Geographic_multipolygon *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_polygon, *)

bool Crosses::eval(const Geographic_polygon *g1, const Geometry *g2) const {
  // If g1 is a 2d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_geometrycollection, *)

bool Crosses::eval(const Geographic_geometrycollection *g1,
                   const Geometry *g2) const {
  return geometry_collection_apply_crosses<Geographic_geometrycollection>(
      *this, g1, g2);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_multipoint, *)

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_linestring *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_polygon *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Geographic_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_multilinestring *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

bool Crosses::eval(const Geographic_multipoint *g1,
                   const Geographic_multipolygon *g2) const {
  Within within(m_semi_major, m_semi_minor);
  Disjoint disjoint(m_semi_major, m_semi_minor);
  bool found_within = false;
  bool found_disjoint = false;

  // At least one point in g1 has to be within g2, and at least one point in g1
  // has to be disjoint from g2.
  for (auto &pt : *g1) {
    bool pt_disjoint = false;
    if (!found_disjoint) {
      pt_disjoint = disjoint(&pt, g2);
      found_disjoint = pt_disjoint;
    }
    if (!pt_disjoint && !found_within) {
      found_within = within(&pt, g2);
    }
    if (found_disjoint && found_within) break;
  }

  return found_disjoint && found_within;
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_multilinestring, *)

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_point *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_linestring *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_polygon *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_geometrycollection *g2) const {
  return geometry_collection_apply_crosses<Geographic_geometrycollection>(
      *this, g1, g2);
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_multipoint *g2) const {
  // If g2 is a 0d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_multilinestring *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

bool Crosses::eval(const Geographic_multilinestring *g1,
                   const Geographic_multipolygon *g2) const {
  return bg::crosses(*g1, *g2, m_geographic_ll_la_aa_strategy);
}

//////////////////////////////////////////////////////////////////////////////

// crosses(Geographic_multipolygon, *)

bool Crosses::eval(const Geographic_multipolygon *g1,
                   const Geometry *g2) const {
  // If g1 is a 2d geometry, return NULL (SQL/MM 2015, Sect. 5.1.51).
  throw null_value_exception();
}

//////////////////////////////////////////////////////////////////////////////

bool crosses(const dd::Spatial_reference_system *srs, const Geometry *g1,
             const Geometry *g2, const char *func_name, bool *crosses,
             bool *null) noexcept {
  try {
    DBUG_ASSERT(g1->coordinate_system() == g2->coordinate_system());
    DBUG_ASSERT(srs == nullptr ||
                ((srs->is_cartesian() &&
                  g1->coordinate_system() == Coordinate_system::kCartesian) ||
                 (srs->is_geographic() &&
                  g1->coordinate_system() == Coordinate_system::kGeographic)));

    if ((*null = (g1->is_empty() || g2->is_empty()))) return false;

    Crosses crosses_func(srs ? srs->semi_major_axis() : 0.0,
                         srs ? srs->semi_minor_axis() : 0.0);
    *crosses = crosses_func(g1, g2);
  } catch (const null_value_exception &e) {
    *null = true;
    return false;
  } catch (...) {
    handle_gis_exception(func_name);
    return true;
  }

  return false;
}

}  // namespace gis

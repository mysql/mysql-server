#ifndef SQL_GIS_BOX_TRAITS_H_INCLUDED
#define SQL_GIS_BOX_TRAITS_H_INCLUDED

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
/// This file contains Boost.Geometry type traits declarations for Cartesian and
/// geographic boxes.
///
/// @see box.h

#include <boost/geometry/geometries/concepts/box_concept.hpp>

#include "sql/gis/box.h"
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "sql/gis/geometries_traits.h"  // To get fully defined traits.

namespace boost {
namespace geometry {
namespace traits {

////////////////////////////////////////////////////////////////////////////////

// Cartesian

template <>
struct tag<gis::Cartesian_box> {
  typedef box_tag type;
};

template <>
struct point_type<gis::Cartesian_box> {
  typedef gis::Cartesian_point type;
};

template <std::size_t Dimension>
struct indexed_access<gis::Cartesian_box, min_corner, Dimension> {
  static inline double get(gis::Cartesian_box const &b) {
    return b.min_corner().get<Dimension>();
  }

  static inline void set(gis::Cartesian_box &b, double const &value) {
    b.min_corner().set<Dimension>(value);
  }
};

template <std::size_t Dimension>
struct indexed_access<gis::Cartesian_box, max_corner, Dimension> {
  static inline double get(gis::Cartesian_box const &b) {
    return b.max_corner().get<Dimension>();
  }

  static inline void set(gis::Cartesian_box &b, double const &value) {
    b.max_corner().set<Dimension>(value);
  }
};

////////////////////////////////////////////////////////////////////////////////

// Geographic

template <>
struct tag<gis::Geographic_box> {
  typedef box_tag type;
};

template <>
struct point_type<gis::Geographic_box> {
  typedef gis::Geographic_point type;
};

template <std::size_t Dimension>
struct indexed_access<gis::Geographic_box, min_corner, Dimension> {
  static inline double get(gis::Geographic_box const &b) {
    return b.min_corner().get<Dimension>();
  }

  static inline void set(gis::Geographic_box &b, double const &value) {
    b.min_corner().set<Dimension>(value);
  }
};

template <std::size_t Dimension>
struct indexed_access<gis::Geographic_box, max_corner, Dimension> {
  static inline double get(gis::Geographic_box const &b) {
    return b.max_corner().get<Dimension>();
  }

  static inline void set(gis::Geographic_box &b, double const &value) {
    b.max_corner().set<Dimension>(value);
  }
};

}  // namespace traits
}  // namespace geometry
}  // namespace boost

#endif  // SQL_GIS_BOX_H_INCLUDED

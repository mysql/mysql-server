#ifndef GIS__FUNCTOR_H_INCLUDED
#define GIS__FUNCTOR_H_INCLUDED

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
/// This file contains the superclass for GIS functors.
///
/// Each GIS function is split in two: a functor class (for internal use) and a
/// function (for external use) that uses the functor. The functor provides the
/// internal interface to GIS functions, and it may throw exceptions. Some
/// functions may need a combination of different functors to implement the
/// desired functionality.
///
/// The function, not the functor, is the interface to the rest of MySQL.
///
/// @see distance_functor.h

#include <exception>

#include "my_dbug.h"  // DBUG_ASSERT
#include "sql/gis/geometries.h"
#include "sql/gis/geometries_cs.h"
#include "template_utils.h"  // down_cast

namespace gis {

/// Function/parameter combination not implemented exception.
///
/// Thrown by GIS functors for parameter combinations that have not been
/// implemented.
class not_implemented_exception : public std::exception {
 private:
  /// Type of coordinate system.
  Coordinate_system m_coordinate_system;
  /// Type of first geometry.
  Geometry_type m_type1;
  /// Type for second geometry.
  Geometry_type m_type2;

  const char *type_to_name(Geometry_type type) const {
    switch (type) {
      case Geometry_type::kPoint:
        return "POINT";
      case Geometry_type::kLinestring:
        return "LINESTRING";
      case Geometry_type::kPolygon:
        return "POLYGON";
      case Geometry_type::kGeometrycollection:
        return "GEOMETRYCOLLECTION";
      case Geometry_type::kMultipoint:
        return "MULTIPOINT";
      case Geometry_type::kMultilinestring:
        return "MULTILINESTRING";
      case Geometry_type::kMultipolygon:
        return "MULTIPOLYGON";
      default:
        DBUG_ASSERT(false); /* purecov: inspected */
        return "UNKNOWN";
    }
  }

 public:
  not_implemented_exception(Coordinate_system cs, Geometry_type t1,
                            Geometry_type t2)
      : m_coordinate_system(cs), m_type1(t1), m_type2(t2) {}

  Coordinate_system coordinate_system() const { return m_coordinate_system; }

  const char *type_name(int geometry_number) const {
    if (geometry_number == 1)
      return type_to_name(m_type1);
    else if (geometry_number == 2)
      return type_to_name(m_type2);
    else {
      DBUG_ASSERT(false); /* purecov: inspected */
      return "UNKNOWN";
    }
  }
};

/// NULL value exception.
///
/// Thrown when the functor discovers that the result is NULL. Normally, NULL
/// returns can be detected before calling the functor, but not always.
class null_value_exception : public std::exception {};

/// The base class of all functors that takes one geometry argument.
///
/// Subclasses of this unary functor base class will implement operator()
/// and call apply() to do type dispatching. The actual body
/// of the unary functor is in the eval() member function, which must be
/// implemented for each different parameter type.
///
/// The functor may throw exceptions.
///
/// @tparam T The return type of the functor.
template <typename T>
class Unary_functor {
 public:
  virtual T operator()(const Geometry *g1) = 0;
  virtual ~Unary_functor() {}

 protected:
  template <typename F>
  static inline T apply(F &f, const Geometry *g1) {
    switch (g1->coordinate_system()) {
      case Coordinate_system::kCartesian:
        switch (g1->type()) {
          case Geometry_type::kPoint:
            return f.eval(down_cast<const Cartesian_point *>(g1));
          case Geometry_type::kLinestring:
            return f.eval(down_cast<const Cartesian_linestring *>(g1));
          case Geometry_type::kPolygon:
            return f.eval(down_cast<const Cartesian_polygon *>(g1));
          case Geometry_type::kGeometrycollection:
            return f.eval(down_cast<const Cartesian_geometrycollection *>(g1));
          case Geometry_type::kMultipoint:
            return f.eval(down_cast<const Cartesian_multipoint *>(g1));
          case Geometry_type::kMultilinestring:
            return f.eval(down_cast<const Cartesian_multilinestring *>(g1));
          case Geometry_type::kMultipolygon:
            return f.eval(down_cast<const Cartesian_multipolygon *>(g1));
          case Geometry_type::kGeometry:
            DBUG_ASSERT(false); /* purecov: inspected */
            throw std::exception();
        }
        break;
      case Coordinate_system::kGeographic:
        switch (g1->type()) {
          case Geometry_type::kPoint:
            return f.eval(down_cast<const Geographic_point *>(g1));
          case Geometry_type::kLinestring:
            return f.eval(down_cast<const Geographic_linestring *>(g1));
          case Geometry_type::kPolygon:
            return f.eval(down_cast<const Geographic_polygon *>(g1));
          case Geometry_type::kGeometrycollection:
            return f.eval(down_cast<const Geographic_geometrycollection *>(g1));
          case Geometry_type::kMultipoint:
            return f.eval(down_cast<const Geographic_multipoint *>(g1));
          case Geometry_type::kMultilinestring:
            return f.eval(down_cast<const Geographic_multilinestring *>(g1));
          case Geometry_type::kMultipolygon:
            return f.eval(down_cast<const Geographic_multipolygon *>(g1));
          case Geometry_type::kGeometry:
            DBUG_ASSERT(false); /* purecov: inspected */
            throw std::exception();
        }
        break;
    }

    DBUG_ASSERT(false); /* purecov: inspected */
    throw std::exception();
  }
};
/// The base class of all functors that takes two geometry arguments.
///
/// Subclasses of this functor base class will implement operator() and call
/// apply() to do type combination dispatching. The actual body of the functor
/// is in the eval() member function, which must be implemented for each
/// different parameter type combination.
///
/// The functor may throw exceptions.
///
/// @tparam T The return type of the functor.
template <typename T>
class Functor {
 public:
  virtual T operator()(const Geometry *g1, const Geometry *g2) const = 0;
  virtual ~Functor() {}

 protected:
  template <typename F>
  static inline T apply(F &f, const Geometry *g1, const Geometry *g2) {
    DBUG_ASSERT(g1->coordinate_system() == g2->coordinate_system());
    switch (g1->coordinate_system()) {
      case Coordinate_system::kCartesian:
        switch (g1->type()) {
          case Geometry_type::kPoint:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_point *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_point *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kLinestring:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_linestring *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_linestring *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kPolygon:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_polygon *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_polygon *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kGeometrycollection:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(
                    down_cast<const Cartesian_geometrycollection *>(g1),
                    down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultipoint:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_multipoint *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_multipoint *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultilinestring:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_multilinestring *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_multilinestring *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultipolygon:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Cartesian_multipolygon *>(g1),
                    down_cast<const Cartesian_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Cartesian_multipolygon *>(g1),
                              down_cast<const Cartesian_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kGeometry:
            DBUG_ASSERT(false); /* purecov: inspected */
            throw new not_implemented_exception(g1->coordinate_system(),
                                                g1->type(), g2->type());
        }  // switch (g1->type())
      case Coordinate_system::kGeographic:
        switch (g1->type()) {
          case Geometry_type::kPoint:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_point *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_point *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_point *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_point *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_point *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_point *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_point *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kLinestring:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_linestring *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_linestring *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_linestring *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_linestring *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_linestring *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_linestring *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_linestring *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kPolygon:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_polygon *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_polygon *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_polygon *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_polygon *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_polygon *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_polygon *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_polygon *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kGeometrycollection:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(
                    down_cast<const Geographic_geometrycollection *>(g1),
                    down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultipoint:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_multipoint *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_multipoint *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_multipoint *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_multipoint *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_multipoint *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_multipoint *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_multipoint *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultilinestring:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_multilinestring *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_multilinestring *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_multilinestring *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_multilinestring *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_multilinestring *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_multilinestring *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_multilinestring *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kMultipolygon:
            switch (g2->type()) {
              case Geometry_type::kPoint:
                return f.eval(down_cast<const Geographic_multipolygon *>(g1),
                              down_cast<const Geographic_point *>(g2));
              case Geometry_type::kLinestring:
                return f.eval(down_cast<const Geographic_multipolygon *>(g1),
                              down_cast<const Geographic_linestring *>(g2));
              case Geometry_type::kPolygon:
                return f.eval(down_cast<const Geographic_multipolygon *>(g1),
                              down_cast<const Geographic_polygon *>(g2));
              case Geometry_type::kGeometrycollection:
                return f.eval(
                    down_cast<const Geographic_multipolygon *>(g1),
                    down_cast<const Geographic_geometrycollection *>(g2));
              case Geometry_type::kMultipoint:
                return f.eval(down_cast<const Geographic_multipolygon *>(g1),
                              down_cast<const Geographic_multipoint *>(g2));
              case Geometry_type::kMultilinestring:
                return f.eval(
                    down_cast<const Geographic_multipolygon *>(g1),
                    down_cast<const Geographic_multilinestring *>(g2));
              case Geometry_type::kMultipolygon:
                return f.eval(down_cast<const Geographic_multipolygon *>(g1),
                              down_cast<const Geographic_multipolygon *>(g2));
              case Geometry_type::kGeometry:
                DBUG_ASSERT(false); /* purecov: inspected */
                throw new not_implemented_exception(g1->coordinate_system(),
                                                    g1->type(), g2->type());
            }
          case Geometry_type::kGeometry:
            DBUG_ASSERT(false); /* purecov: inspected */
            throw new not_implemented_exception(g1->coordinate_system(),
                                                g1->type(), g2->type());
        }  // switch (g1->type())
    }      // switch (g1->coordinate_system())

    DBUG_ASSERT(false); /* purecov: inspected */
    throw new not_implemented_exception(g1->coordinate_system(), g1->type(),
                                        g2->type());
  }
};

}  // namespace gis

#endif  // GIS__FUNCTOR_H_INCLUDED

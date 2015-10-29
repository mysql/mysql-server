#ifndef GEOFUNC_INTERNAL_INCLUDED
#define GEOFUNC_INTERNAL_INCLUDED

/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/**
  @file

  @brief
  This file defines common build blocks of GIS functions.
*/
#include "my_config.h"

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>

#include <m_ctype.h>
#include "item_geofunc.h"
#include "gis_bg_traits.h"

// Boost.Geometry
#include <boost/geometry/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
// Boost.Range
#include <boost/range.hpp>


// GCC requires typename whenever needing to access a type inside a template,
// but MSVC forbids this.
#ifdef HAVE_IMPLICIT_DEPENDENT_NAME_TYPING
#define TYPENAME
#else
#define TYPENAME typename
#endif


#define GIS_ZERO 0.00000000001

extern bool simplify_multi_geometry(String *str, String *result_buffer);

using std::auto_ptr;


/**
  Handle a GIS exception of any type.

  This function constitutes the exception handling barrier between
  Boost.Geometry and MySQL code. It handles all exceptions thrown in
  GIS code and raises the corresponding error in MySQL.

  Pattern for use in other functions:

  @code
  try
  {
    something_that_throws();
  }
  catch (...)
  {
    handle_gis_exception("st_foo");
  }
  @endcode

  Other exception handling code put into the catch block, before or
  after the call to handle_gis_exception(), must not throw exceptions.

  @param funcname Function name for use in error message
 */
void handle_gis_exception(const char *funcname);


/// A wrapper and interface for all geometry types used here. Make these
/// types as localized as possible. It's used as a type interface.
/// @tparam CoordinateSystemType Coordinate system type, specified using
//          those defined in boost::geometry::cs.
template<typename CoordinateSystemType>
class BG_models
{
public:
  typedef Gis_point Point;
  // An counter-clockwise, closed Polygon type. It can hold open Polygon data,
  // but not clockwise ones, otherwise things can go wrong, e.g. intersection.
  typedef Gis_polygon Polygon;
  typedef Gis_line_string Linestring;
  typedef Gis_multi_point Multipoint;
  typedef Gis_multi_line_string Multilinestring;
  typedef Gis_multi_polygon Multipolygon;

  typedef double Coordinate_type;
  typedef CoordinateSystemType Coordinate_system;
};


template<>
class BG_models<
      boost::geometry::cs::spherical_equatorial<boost::geometry::degree> >
{
public:
  typedef Gis_point_spherical Point;
  // An counter-clockwise, closed Polygon type. It can hold open Polygon data,
  // but not clockwise ones, otherwise things can go wrong, e.g. intersection.
  typedef Gis_polygon_spherical Polygon;
  typedef Gis_line_string_spherical Linestring;
  typedef Gis_multi_point_spherical Multipoint;
  typedef Gis_multi_line_string_spherical Multilinestring;
  typedef Gis_multi_polygon_spherical Multipolygon;

  typedef double Coordinate_type;
  typedef boost::geometry::cs::spherical_equatorial<boost::geometry::degree>
    Coordinate_system;
};


namespace bg= boost::geometry;
namespace bgm= boost::geometry::model;
namespace bgcs= boost::geometry::cs;
namespace bgi= boost::geometry::index;
namespace bgm= boost::geometry::model;

typedef bgm::point<double, 2, bgcs::cartesian> BG_point;
typedef bgm::box<BG_point> BG_box;
typedef std::pair<BG_box, size_t> BG_rtree_entry;
typedef std::vector<BG_rtree_entry> BG_rtree_entries;
typedef bgi::rtree<BG_rtree_entry, bgi::quadratic<64> > Rtree_index;
typedef std::vector<BG_rtree_entry> Rtree_result;


inline void make_bg_box(const Geometry *g, BG_box *box)
{
  MBR mbr;
  g->envelope(&mbr);
  box->min_corner().set<0>(mbr.xmin);
  box->min_corner().set<1>(mbr.ymin);
  box->max_corner().set<0>(mbr.xmax);
  box->max_corner().set<1>(mbr.ymax);
}


inline bool is_box_valid(const BG_box &box)
{
  return
    !(!my_isfinite(box.min_corner().get<0>()) ||
      !my_isfinite(box.min_corner().get<1>()) ||
      !my_isfinite(box.max_corner().get<0>()) ||
      !my_isfinite(box.max_corner().get<1>()) ||
      box.max_corner().get<0>() < box.min_corner().get<0>() ||
      box.max_corner().get<1>() < box.min_corner().get<1>());
}


/**
  Build an rtree set using a geometry collection.
  @param gl geometry object pointers container.
  @param [out] rtree entries which can be used to build an rtree.
 */
void
make_rtree(const BG_geometry_collection::Geometry_list &gl,
           Rtree_index *rtree);


/**
  Build an rtree set using array of Boost.Geometry objects, which are
  components of a multi geometry.
  @param mg the multi geometry.
  @param rtree the rtree to build.
 */
template <typename MultiGeometry>
void
make_rtree_bggeom(const MultiGeometry &mg,
                  Rtree_index *rtree);


inline Gis_geometry_collection *
empty_collection(String *str, uint32 srid)
{
  return new Gis_geometry_collection(srid, Geometry::wkb_invalid_type,
                                     NULL, str);
}


/*
  Check whether a geometry is an empty geometry collection, i.e. one that
  doesn't contain any geometry component of [multi]point or [multi]linestring
  or [multi]polygon type.
  @param g the geometry to check.
  @return true if g is such an empty geometry collection;
          false otherwise.
*/
bool is_empty_geocollection(const Geometry *g);


/*
  Check whether wkbres is the data of an empty geometry collection, i.e. one
  that doesn't contain any geometry component of [multi]point or
  [multi]linestring or [multi]polygon type.

  @param wkbres a piece of geometry data of GEOMETRY format, i.e. an SRID
                prefixing a WKB.
  @return true if wkbres contains such an empty geometry collection;
          false otherwise.
 */
bool is_empty_geocollection(const String &wkbres);


/**
   Less than comparator for points used by BG.
 */
struct bgpt_lt
{
  template <typename Point>
  bool operator ()(const Point &p1, const Point &p2) const
  {
    if (p1.template get<0>() != p2.template get<0>())
      return p1.template get<0>() < p2.template get<0>();
    else
      return p1.template get<1>() < p2.template get<1>();
  }
};


/**
   Equals comparator for points used by BG.
 */
struct bgpt_eq
{
  template <typename Point>
  bool operator ()(const Point &p1, const Point &p2) const
  {
    return p1.template get<0>() == p2.template get<0>() &&
      p1.template get<1>() == p2.template get<1>();
  }
};


/**
  Utility class, reset specified variable 'valref' to specified 'oldval' when
  val_resetter<valtype> instance is destroyed.
  @tparam Valtype Variable type to reset.
 */
template <typename Valtype>
class Var_resetter
{
private:
  Valtype *valref;
  Valtype oldval;

  // Forbid use, to eliminate a warning: oldval may be used uninitialized.
  Var_resetter(const Var_resetter &o);
  Var_resetter &operator=(const Var_resetter&);
public:
  Var_resetter() : valref(NULL)
  {
  }

  Var_resetter(Valtype *v, const Valtype &oval) : valref(v), oldval(oval)
  {
  }

  ~Var_resetter()
  {
    if (valref)
      *valref= oldval;
  }

  void set(Valtype *v, const Valtype &oldval)
  {
    valref= v;
    this->oldval= oldval;
  }
};


/**
  For every Geometry object write-accessed by a boost geometry function, i.e.
  those passed as out parameter into set operation functions, call this
  function before using the result object's data.

  @param resbuf_mgr tracks the result buffer
  @return true if an error occurred or if the geometry is an empty
          collection; false if no error occurred.
*/
template <typename BG_geotype>
bool post_fix_result(BG_result_buf_mgr *resbuf_mgr,
                     BG_geotype &geout, String *res);



#endif

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

#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <stack>

#include <m_ctype.h>
#include "parse_tree_helpers.h"
#include "spatial.h"
#include "item_geofunc.h"
#include "gis_bg_traits.h"

// Boost.Geometry
#include <boost/geometry/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
// Boost.Range
#include <boost/range.hpp>
// adaptors
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>


// GCC requires typename whenever needing to access a type inside a template,
// but MSVC forbids this.
#ifdef HAVE_IMPLICIT_DEPENDENT_NAME_TYPING
#define TYPENAME
#else
#define TYPENAME typename
#endif


#define CATCH_ALL(funcname, expr) \
  catch (const boost::geometry::centroid_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_CENTROID_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::overlay_invalid_input_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_OVERLAY_INVALID_INPUT_EXCEPTION, MYF(0),\
             (funcname));\
  }\
  catch (const boost::geometry::turn_info_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_TURN_INFO_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::detail::self_get_turn_points::self_ip_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_SELF_INTERSECTION_POINT_EXCEPTION, MYF(0),\
             (funcname));\
  }\
  catch (const boost::geometry::empty_input_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_EMPTY_INPUT_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_UNKNOWN_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const std::bad_alloc &e)\
  {\
    expr;\
    my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::domain_error &e)\
  {\
    expr;\
    my_error(ER_STD_DOMAIN_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::length_error &e)\
  {\
    expr;\
    my_error(ER_STD_LENGTH_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::invalid_argument &e)\
  {\
    expr;\
    my_error(ER_STD_INVALID_ARGUMENT, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::out_of_range &e)\
  {\
    expr;\
    my_error(ER_STD_OUT_OF_RANGE_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::overflow_error &e)\
  {\
    expr;\
    my_error(ER_STD_OVERFLOW_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::range_error &e)\
  {\
    expr;\
    my_error(ER_STD_RANGE_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::underflow_error &e)\
  {\
    expr;\
    my_error(ER_STD_UNDERFLOW_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::logic_error &e)\
  {\
    expr;\
    my_error(ER_STD_LOGIC_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::runtime_error &e)\
  {\
    expr;\
    my_error(ER_STD_RUNTIME_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::exception &e)\
  {\
    expr;\
    my_error(ER_STD_UNKNOWN_EXCEPTION, MYF(0), e.what(), (funcname));\
  }\
  catch (...)\
  {\
    expr;\
    my_error(ER_GIS_UNKNOWN_EXCEPTION, MYF(0), (funcname));\
  }


using std::auto_ptr;

/// A wrapper and interface for all geometry types used here. Make these
/// types as localized as possible. It's used as a type interface.
/// @tparam CoordinateElementType The numeric type for a coordinate value,
///         most often it's double.
/// @tparam CoordinateSystemType Coordinate system type, specified using
//          those defined in boost::geometry::cs.
template<typename CoordinateElementType, typename CoordinateSystemType>
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

  typedef CoordinateElementType Coord_type;
  typedef CoordinateSystemType Coordsys;
};

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


struct Rtree_value_maker
{
  typedef std::pair<BG_box, size_t> result_type;
  template<typename  T>
  result_type operator()(T const &v) const
  {
    BG_box box;
    make_bg_box(v.value(), &box);
    return result_type(box, v.index());
  }
};


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


struct Is_rtree_box_valid
{
  typedef std::pair<BG_box, size_t> Rtree_entry;
  bool operator()(Rtree_entry const& re) const
  {
    return is_box_valid(re.first);
  }
};


/**
  Build an rtree set using a geometry collection.
  @param gl geometry object pointers container.
  @param vals[out] rtree entries which can be used to build an rtree.
 */
inline void
make_rtree(const BG_geometry_collection::Geometry_list &gl,
           Rtree_index *rtree)
{
  Rtree_index temp_rtree(gl | boost::adaptors::indexed() |
                         boost::adaptors::transformed(Rtree_value_maker()) |
                         boost::adaptors::filtered(Is_rtree_box_valid()));

  rtree->swap(temp_rtree);
}


inline Gis_geometry_collection *
empty_collection(String *str, uint32 srid)
{
  return new Gis_geometry_collection(srid, Geometry::wkb_invalid_type,
                                     NULL, str);
}


/*
  Check whether g is an empty geometry collection.
*/
inline bool is_empty_geocollection(const Geometry *g)
{
  if (g->get_geotype() != Geometry::wkb_geometrycollection)
    return false;

  uint32 num= uint4korr(g->get_cptr());
  return num == 0;
}


/*
  Here wkbres is a piece of geometry data of GEOMETRY format,
  i.e. an SRID prefixing a WKB.
  Check if wkbres is the data of an empty geometry collection.
 */
inline bool is_empty_geocollection(const String &wkbres)
{
  if (wkbres.ptr() == NULL)
    return true;

  uint32 geotype= uint4korr(wkbres.ptr() + SRID_SIZE + 1);

  return (geotype == static_cast<uint32>(Geometry::wkb_geometrycollection) &&
          uint4korr(wkbres.ptr() + SRID_SIZE + WKB_HEADER_SIZE) == 0);
}


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


inline void reassemble_geometry(Geometry *g)
{
  Geometry::wkbType gtype= g->get_geotype();
  if (gtype == Geometry::wkb_polygon)
    down_cast<Gis_polygon *>(g)->to_wkb_unparsed();
  else if (gtype == Geometry::wkb_multilinestring)
    down_cast<Gis_multi_line_string *>(g)->reassemble();
  else if (gtype == Geometry::wkb_multipolygon)
    down_cast<Gis_multi_polygon *>(g)->reassemble();
}


inline Geometry::wkbType base_type(Geometry::wkbType gt)
{
  Geometry::wkbType ret;

  switch (gt)
  {
  case Geometry::wkb_multipoint:
    ret= Geometry::wkb_point;
    break;
  case Geometry::wkb_multilinestring:
    ret= Geometry::wkb_linestring;
    break;
  case Geometry::wkb_multipolygon:
    ret= Geometry::wkb_polygon;
    break;
  default:
    ret= gt;
  }
  return ret;
}


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
  Var_resetter();
  Var_resetter(const Var_resetter &o);
  Var_resetter &operator=(const Var_resetter&);
public:
  Var_resetter(Valtype *v, Valtype oldval) : valref(v)
  {
    this->oldval= oldval;
  }

  ~Var_resetter() { *valref= oldval; }
};


inline bool is_areal(const Geometry *g)
{
  return g != NULL && (g->get_type() == Geometry::wkb_polygon ||
                       g->get_type() == Geometry::wkb_multipolygon);
}


/**
  For every Geometry object write-accessed by a boost geometry function, i.e.
  those passed as out parameter into set operation functions, call this
  function before using the result object's data.

  @param resbuf_mgr tracks the result buffer
  @return true if got error; false if no error occured.
*/
template <typename BG_geotype>
bool post_fix_result(BG_result_buf_mgr *resbuf_mgr,
                     BG_geotype &geout, String *res)
{
  DBUG_ASSERT(geout.has_geom_header_space());
  reassemble_geometry(&geout);

  // Such objects returned by BG never have overlapped components.
  if (geout.get_type() == Geometry::wkb_multilinestring ||
      geout.get_type() == Geometry::wkb_multipolygon)
    geout.set_components_no_overlapped(true);
  if (geout.get_ptr() == NULL)
    return true;
  if (res)
  {
    const char *resptr= geout.get_cptr() - GEOM_HEADER_SIZE;
    size_t len= geout.get_nbytes();

    /*
      The resptr buffer is now owned by resbuf_mgr and used by res, resptr
      will be released properly by resbuf_mgr.
     */
    resbuf_mgr->add_buffer(const_cast<char *>(resptr));
    /*
      Pass resptr as const pointer so that the memory space won't be reused
      by res object. Reuse is forbidden because the memory comes from BG
      operations and will be freed upon next same val_str call.
    */
    res->set(resptr, len + GEOM_HEADER_SIZE, &my_charset_bin);

    // Prefix the GEOMETRY header.
    write_geometry_header(const_cast<char *>(resptr), geout.get_srid(),
                          geout.get_geotype());

    /*
      Give up ownership because the buffer may have to live longer than
      the object.
    */
    geout.set_ownmem(false);
  }

  return false;
}


/**
  Merge all components as appropriate so that the object contains only
  components that don't overlap.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param ifso the Item_func_spatial_operation object, we here rely on it to
         do union operation.
  @param[out] pdone takes back whether the merge operation is completed.
  @param[out] pnull_value takes back null_value set during the operation.
 */
template<typename Coord_type, typename Coordsys>
void BG_geometry_collection::
merge_components(bool *pdone, my_bool *pnull_value)
{
  if (is_comp_no_overlapped())
    return;

  POS pos;
  Item_func_spatial_operation ifso(pos, NULL, NULL, Gcalc_function::op_union);
  while (merge_one_run<Coord_type, Coordsys>(&ifso, pdone, pnull_value))
    ;
}


/**
  Create this class for exception safety --- destroy the objects referenced
  by the pointers in the set when destroying the container.
 */
template<typename T>
class Pointer_vector : public std::vector<T *>
{
  typedef std::vector<T*> parent;
public:
  ~Pointer_vector()
  {
    for (typename parent::iterator i= this->begin(); i != this->end(); ++i)
      delete (*i);
  }
};


// A unary predicate to locate a target Geometry object pointer from a sequence.
class Is_target_geometry
{
  Geometry *m_target;
public:
  Is_target_geometry(Geometry *t) :m_target(t)
  {
  }

  bool operator()(Geometry *g)
  {
    return g == m_target;
  }
};


class Rtree_entry_compare
{
public:
  Rtree_entry_compare()
  {
  }

  bool operator()(const BG_rtree_entry &re1, const BG_rtree_entry &re2) const
  {
    return re1.second < re2.second;
  }
};


/**
  One run of merging components.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param ifso the Item_func_spatial_operation object, we here rely on it to
         do union operation.
  @param[out] pdone takes back whether the merge operation is completed.
  @param[out] pnull_value takes back null_value set during the operation.
  @return whether need another call of this function.
 */
template<typename Coord_type, typename Coordsys>
bool BG_geometry_collection::merge_one_run(Item_func_spatial_operation *ifso,
                                           bool *pdone, my_bool *pnull_value)
{
  Geometry *gres= NULL;
  bool isdone= false;
  bool has_new= false;
  bool opdone= false;
  my_bool &null_value= *pnull_value;
  String wkbres;
  Pointer_vector<Geometry> added;

  added.reserve(16);

  Rtree_index rtree;
  make_rtree(m_geos, &rtree);
  Rtree_result rtree_result;
  *pdone= false;

  for (Geometry_list::iterator i= m_geos.begin(); i != m_geos.end(); ++i)
  {
    if (*i == NULL)
      continue;

    BG_box box;
    make_bg_box(*i, &box);
    if (!is_box_valid(box))
      continue;

    rtree_result.clear();
    rtree.query(bgi::intersects(box), std::back_inserter(rtree_result));
    /*
      Normally the rtree should be non-empty because at least there is *i
      itself. But if box has NaN coordinates, the rtree can be empty since
      all coordinate comparisons with NaN numbers are false. also if the
      min corner point have greater coordinates than the max corner point,
      the box isn't valid and the rtree can be empty.
     */
    DBUG_ASSERT(rtree_result.size() != 0);

    // Sort rtree_result by Rtree_entry::second in order to make
    // components in fixed order.
    Rtree_entry_compare rtree_entry_compare;
    std::sort(rtree_result.begin(), rtree_result.end(), rtree_entry_compare);

    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      Geometry *geom2= m_geos[j->second];
      if (*i == geom2 || geom2 == NULL)
        continue;

      isdone= false;
      opdone= false;

      /*
        TODO: in future when BG::covered_by has full support for all type
        combinations, replace below 3 checks with it. So far below 3 checks
        don't catch the point on border of linestring/polygon or linestring
        on border of polygon cases, and are much slower than one check.
      */

      // Equals is much easier and faster to check, so put it first.
      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (geom2, *i, &isdone, Item_func::SP_EQUALS_FUNC, &null_value) &&
          isdone && !null_value)
      {
        *i= NULL;
        break;
      }

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (*i, geom2, &isdone, Item_func::SP_WITHIN_FUNC, &null_value) &&
          isdone && !null_value)
      {
        *i= NULL;
        break;
      }

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (geom2, *i, &isdone, Item_func::SP_WITHIN_FUNC, &null_value) &&
          isdone && !null_value)
      {
        m_geos[j->second]= NULL;
        continue;
      }

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (*i, geom2, &isdone, Item_func::SP_OVERLAPS_FUNC, &null_value) &&
          isdone && !null_value)
      {
        // Free before using it, wkbres may have WKB data from last execution.
        wkbres.mem_free();
        gres= ifso->bg_geo_set_op<Coord_type, Coordsys>(*i, geom2,
                                                        &wkbres, &opdone);
        null_value= ifso->null_value;

        if (!opdone || null_value)
        {
          if (gres != NULL && gres != *i && gres != geom2)
            delete gres;
          return !null_value;
        }

        if (gres != *i)
          *i= NULL;
        if (gres != geom2)
          m_geos[j->second]= NULL;
        if (gres != NULL && gres != *i && gres != geom2)
        {
          added.push_back(gres);
          has_new= true;
          gres= NULL;
        }
        /*
          Done with *i, it's either adopted, or removed or merged to a new
          geometry.
         */
        break;
      }

      /*
        Proceed if !isdone to leave the two geometries as is, this is OK.
       */
      if (null_value)
        return false;

    } // for (*i)
  } // for (*j)

  // Remove deleted Geometry object pointers, then append new components if any.
  Is_target_geometry pred(NULL);
  Geometry_list::iterator jj= std::remove_if(m_geos.begin(),
                                             m_geos.end(), pred);
  m_geos.resize(jj - m_geos.begin());

  for (Pointer_vector<Geometry>::iterator i= added.begin();
       i != added.end(); ++i)
  {
    /*
      Fill rather than directly use *i for consistent memory management.
      The objects pointed by pointers in added will be automatically destroyed.
     */
    fill(*i);
  }

  *pdone= true;
  return has_new;
}

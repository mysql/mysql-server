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
  catch (const boost::geometry::inconsistent_turns_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_INCONSISTENT_TURNS_EXCEPTION, MYF(0)); \
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


#define GIS_ZERO 0.00000000001

extern bool simplify_multi_geometry(String *str);

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

  typedef CoordinateElementType Coordinate_type;
  typedef CoordinateSystemType Coordinate_system;
};


template<typename CoordinateElementType>
class BG_models<CoordinateElementType,
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

  typedef CoordinateElementType Coordinate_type;
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


/*
  A functor to make an rtree value entry from an array element of
  Boost.Geometry model type.
 */
struct Rtree_value_maker_bggeom
{
  typedef std::pair<BG_box, size_t> result_type;
  template<typename  T>
  result_type operator()(T const &v) const
  {
    BG_box box;
    boost::geometry::envelope(v.value(), box);

    return result_type(box, v.index());
  }
};


/**
  Build an rtree set using array of Boost.Geometry objects, which are
  components of a multi geometry.
  @param mg the multi geometry.
  @param rtree the rtree to build.
 */
template <typename MultiGeometry>
inline void
make_rtree_bggeom(const MultiGeometry &mg,
                  Rtree_index *rtree)
{
  Rtree_index temp_rtree(mg | boost::adaptors::indexed() |
                         boost::adaptors::
                         transformed(Rtree_value_maker_bggeom()) |
                         boost::adaptors::filtered(Is_rtree_box_valid()));

  rtree->swap(temp_rtree);
}


inline Gis_geometry_collection *
empty_collection(String *str, uint32 srid)
{
  return new Gis_geometry_collection(srid, Geometry::wkb_invalid_type,
                                     NULL, str);
}


class Is_empty_geometry : public WKB_scanner_event_handler
{
public:
  bool is_empty;

  Is_empty_geometry() :is_empty(true)
  {
  }

  virtual void on_wkb_start(Geometry::wkbByteOrder bo,
                            Geometry::wkbType geotype,
                            const void *wkb, uint32 len, bool has_hdr)
  {
    if (is_empty && geotype != Geometry::wkb_geometrycollection)
      is_empty= false;
  }

  virtual void on_wkb_end(const void *wkb)
  {
  }

  virtual bool continue_scan() const
  {
    return is_empty;
  }
};

/*
  Check whether a geometry is an empty geometry collection, i.e. one that
  doesn't contain any geometry component of [multi]point or [multi]linestring
  or [multi]polygon type.
  @param g the geometry to check.
  @return true if g is such an empty geometry collection;
          false otherwise.
*/
inline bool is_empty_geocollection(const Geometry *g)
{
  if (g->get_geotype() != Geometry::wkb_geometrycollection)
    return false;

  uint32 num= uint4korr(g->get_cptr());
  if (num == 0)
    return true;

  Is_empty_geometry checker;
  uint32 len= g->get_data_size();
  wkb_scanner(g->get_cptr(), &len, Geometry::wkb_geometrycollection,
              false, &checker);
  return checker.is_empty;

}


/*
  Check whether wkbres is the data of an empty geometry collection, i.e. one
  that doesn't contain any geometry component of [multi]point or
  [multi]linestring or [multi]polygon type.

  @param wkbres a piece of geometry data of GEOMETRY format, i.e. an SRID
                prefixing a WKB.
  @return true if wkbres contains such an empty geometry collection;
          false otherwise.
 */
inline bool is_empty_geocollection(const String &wkbres)
{
  if (wkbres.ptr() == NULL)
    return true;

  uint32 geotype= uint4korr(wkbres.ptr() + SRID_SIZE + 1);

  if (geotype != static_cast<uint32>(Geometry::wkb_geometrycollection))
    return false;

  if (uint4korr(wkbres.ptr() + SRID_SIZE + WKB_HEADER_SIZE) == 0)
    return true;

  Is_empty_geometry checker;
  uint32 len= static_cast<uint32>(wkbres.length()) - GEOM_HEADER_SIZE;
  wkb_scanner(wkbres.ptr() + GEOM_HEADER_SIZE, &len,
              Geometry::wkb_geometrycollection, false, &checker);
  return checker.is_empty;
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
  @return true if an error occurred or if the geometry is an empty
          collection; false if no error occured.
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
  @param[out] pnull_value takes back null_value set during the operation.
 */
template<typename Coord_type, typename Coordsys>
void BG_geometry_collection::
merge_components(my_bool *pnull_value)
{
  if (is_comp_no_overlapped())
    return;

  POS pos;
  Item_func_spatial_operation ifso(pos, NULL, NULL, Gcalc_function::op_union);
  bool do_again= true;
  uint32 last_composition[6]= {0}, num_unchanged_composition= 0;
  size_t last_num_geos= 0;

  /*
    After each merge_one_run call, see whether the two indicators change:
    1. total number of geometry components;
    2. total number of each of the 6 types of geometries

    If they don't change for N*N/4 times, break out of the loop. Here N is
    the total number of geometry components.

    There is the rationale:

    Given a geometry collection, it's likely that one effective merge_one_run
    merges a polygon P and the linestring that crosses it (L) to a
    polygon P'(the same one) and another linestring L', the 2 indicators above
    don't change but the merge is actually done. If we merge P'
    and L' again, they should not be considered cross, but given certain data
    BG somehow believes L` still crosses P` even the P and P` are valid, and
    it will give us a L'' and P'' which is different from L' and P'
    respectively, and L'' is still considered crossing P'',
    hence the loop here never breaks out.

    If the collection has N components, and we have X [multi]linestrings and
    N-X polygons, the number of pairs that can be merged is Y = X * (N-X),
    so the largest Y is N*N/4. If the above 2 indicators stay unchanged more
    than N*N/4 times the loop runs, we believe all possible combinations in
    the collection are enumerated and no effective merge is being done any more.

    Note that the L'' and P'' above is different from L' and P' so we can't
    compare GEOMETRY byte string, and geometric comparison is expensive and may
    still compare unequal and we would still be stuck in the endless loop.
  */
  while (!*pnull_value && do_again)
  {
    do_again= merge_one_run<Coord_type, Coordsys>(&ifso, pnull_value);
    if (!*pnull_value && do_again)
    {
      const size_t num_geos= m_geos.size();
      uint32 composition[6]= {0};

      for (size_t i= 0; i < num_geos; ++i)
        composition[m_geos[i]->get_type() - 1]++;

      if (num_geos != last_num_geos ||
          memcmp(composition, last_composition, sizeof(composition)))
      {
        memcpy(last_composition, composition, sizeof(composition));
        last_num_geos= num_geos;
        num_unchanged_composition= 0;
      }
      else
        num_unchanged_composition++;

      if (num_unchanged_composition > (last_num_geos * last_num_geos / 4 + 2))
        break;
    }
  }
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


template<typename Coord_type, typename Coordsys>
inline bool
linestring_overlaps_polygon_outerring(const Gis_line_string &ls,
                                      const Gis_polygon &plgn)
{

  Gis_polygon_ring &oring= plgn.outer();
  Gis_line_string ls2(oring.get_ptr(), oring.get_nbytes(),
                      oring.get_flags(), oring.get_srid());
  return boost::geometry::overlaps(ls, ls2);
}


template<typename Coord_type, typename Coordsys>
bool linear_areal_intersect_infinite(Geometry *g1, Geometry *g2,
                                     my_bool *pnull_value)
{
  bool res= false;

  /*
    If crosses check succeeds, make sure g2 is a valid [multi]polygon, invalid
    ones can be accepted by BG and the cross check would be considered true,
    we should reject such result and return false in this case.
  */
  if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
      (g1, g2, Item_func::SP_CROSSES_FUNC, pnull_value) && !*pnull_value)
  {
    Geometry::wkbType g2_type= g2->get_type();
    if (g2_type == Geometry::wkb_polygon)
    {
      Gis_polygon plgn(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      res= bg::is_valid(plgn);
    }
    else if (g2_type == Geometry::wkb_multipolygon)
    {
      Gis_multi_polygon mplgn(g2->get_data_ptr(), g2->get_data_size(),
                              g2->get_flags(), g2->get_srid());
      res= bg::is_valid(mplgn);
    }
    else
      DBUG_ASSERT(false);

    return res;
  }

  if (*pnull_value)
    return false;

  if (g1->get_type() == Geometry::wkb_linestring)
  {
    Gis_line_string ls(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
    if (g2->get_type() == Geometry::wkb_polygon)
    {
      Gis_polygon plgn(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      res= linestring_overlaps_polygon_outerring
        <Coord_type, Coordsys>(ls, plgn);
    }
    else
    {
      Gis_multi_polygon mplgn(g2->get_data_ptr(), g2->get_data_size(),
                              g2->get_flags(), g2->get_srid());
      for (size_t i= 0; i < mplgn.size(); i++)
      {
        if (linestring_overlaps_polygon_outerring<Coord_type, Coordsys>
            (ls, mplgn[i]))
          return true;
      }
    }
  }
  else
  {
    Gis_multi_line_string mls(g1->get_data_ptr(), g1->get_data_size(),
                              g1->get_flags(), g1->get_srid());
    if (g2->get_type() == Geometry::wkb_polygon)
    {
      Gis_polygon plgn(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      for (size_t i= 0; i < mls.size(); i++)
      {
        if (linestring_overlaps_polygon_outerring<Coord_type, Coordsys>
            (mls[i], plgn))
          return true;
      }
    }
    else
    {
      Gis_multi_polygon mplgn(g2->get_data_ptr(), g2->get_data_size(),
                              g2->get_flags(), g2->get_srid());
      for (size_t j= 0; j < mls.size(); j++)
      {
        for (size_t i= 0; i < mplgn.size(); i++)
        {
          if (linestring_overlaps_polygon_outerring<Coord_type, Coordsys>
              (mls[j], mplgn[i]))
            return true;
        }
      }
    }
  }

  return res;
}


/**
  One run of merging components.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param ifso the Item_func_spatial_operation object, we here rely on it to
         do union operation.
  @param[out] pnull_value takes back null_value set during the operation.
  @return whether need another call of this function.
 */
template<typename Coord_type, typename Coordsys>
bool BG_geometry_collection::merge_one_run(Item_func_spatial_operation *ifso,
                                           my_bool *pnull_value)
{
  Geometry *gres= NULL;
  bool has_new= false;
  my_bool &null_value= *pnull_value;
  Pointer_vector<Geometry> added;
  std::vector<String> added_wkbbufs;

  added.reserve(16);
  added_wkbbufs.reserve(16);

  Rtree_index rtree;
  make_rtree(m_geos, &rtree);
  Rtree_result rtree_result;

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

    // Used to stop the nested loop.
    bool stop_it= false;

    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      Geometry *geom2= m_geos[j->second];
      if (*i == geom2 || geom2 == NULL)
        continue;

      // Equals is much easier and faster to check, so check it first.
      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (geom2, *i, Item_func::SP_EQUALS_FUNC, &null_value) && !null_value)
      {
        *i= NULL;
        break;
      }

      if (null_value)
      {
        stop_it= true;
        break;
      }

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (*i, geom2, Item_func::SP_WITHIN_FUNC, &null_value) && !null_value)
      {
        *i= NULL;
        break;
      }

      if (null_value)
      {
        stop_it= true;
        break;
      }

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (geom2, *i, Item_func::SP_WITHIN_FUNC, &null_value) && !null_value)
      {
        m_geos[j->second]= NULL;
        continue;
      }

      if (null_value)
      {
        stop_it= true;
        break;
      }

      /*
        If *i and geom2 is a polygon and a linestring that intersect only
        finite number of points, the union result is the same as the two
        geometries, and we would be stuck in an infinite loop. So we must
        detect and exclude this case. All other argument type combinations
        always will get a geometry different from the two arguments.
      */
      char d11= (*i)->feature_dimension();
      char d12= geom2->feature_dimension();
      Geometry *geom_d1= NULL;
      Geometry *geom_d2= NULL;
      bool is_linear_areal= false;

      if (((d11 == 1 && d12 == 2) || (d12 == 1 && d11 == 2)))
      {
        geom_d1= (d11 == 1 ? *i : geom2);
        geom_d2= (d11 == 2 ? *i : geom2);
        if (d11 != 1)
        {
          const char tmp_dim= d11;
          d11= d12;
          d12= tmp_dim;
        }
        is_linear_areal= true;
      }

      /*
        As said above, if one operand is linear, the other is areal, then we
        only proceed the union of them if they intersect infinite number of
        points, i.e. L crosses A or L touches A's outer ring. Note that if L
        touches some of A's inner rings, L must be crossing A, so not gonna
        check the inner rings.
      */
      if ((!is_linear_areal &&
           Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
           (*i, geom2, Item_func::SP_INTERSECTS_FUNC, &null_value) &&
           !null_value) ||
          (is_linear_areal && linear_areal_intersect_infinite
           <Coord_type, Coordsys>(geom_d1, geom_d2, &null_value)))
      {
        String wkbres;

        if (null_value)
        {
          stop_it= true;
          break;
        }

        bool opdone= false;
        gres= ifso->bg_geo_set_op<Coord_type, Coordsys>(*i, geom2,
                                                        &wkbres, &opdone);
        null_value= ifso->null_value;

        if (!opdone || null_value)
        {
          if (gres != NULL && gres != *i && gres != geom2)
            delete gres;
          stop_it= true;
          break;
        }

        if (gres != *i)
          *i= NULL;
        if (gres != geom2)
          m_geos[j->second]= NULL;
        if (gres != NULL && gres != *i && gres != geom2)
        {
          added.push_back(gres);
          String tmp_wkbbuf;
          added_wkbbufs.push_back(tmp_wkbbuf);
          added_wkbbufs.back().takeover(wkbres);
          has_new= true;
          gres= NULL;
        }
        /*
          Done with *i, it's either adopted, or removed or merged to a new
          geometry.
         */
        break;
      } // intersects

      if (null_value)
      {
        stop_it= true;
        break;
      }

    } // for (*j)

    if (stop_it)
      break;

  } // for (*i)

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

  // The added and added_wkbbufs arrays are destroyed and the Geometry objects
  // in 'added' are freed, and memory buffers in added_wkbbufs are freed too.
  return has_new;
}

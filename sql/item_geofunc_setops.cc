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
  This file defines implementations of GIS set operation functions.
*/
#include "my_config.h"
#include "item_geofunc_internal.h"


#define BGOPCALL(GeoOutType, geom_out, bgop,                            \
                 GeoType1, g1, GeoType2, g2, wkbres, nullval)           \
do                                                                      \
{                                                                       \
  const void *pg1= g1->normalize_ring_order();                          \
  const void *pg2= g2->normalize_ring_order();                          \
  geom_out= NULL;                                                       \
  if (pg1 != NULL && pg2 != NULL)                                       \
  {                                                                     \
    GeoType1 geo1(pg1, g1->get_data_size(), g1->get_flags(),            \
                  g1->get_srid());                                      \
    GeoType2 geo2(pg2, g2->get_data_size(), g2->get_flags(),            \
                  g2->get_srid());                                      \
    auto_ptr<GeoOutType>geout(new GeoOutType());                        \
    geout->set_srid(g1->get_srid());                                    \
    boost::geometry::bgop(geo1, geo2, *geout);                          \
    (nullval)= false;                                                   \
    if (geout->size() == 0 ||                                           \
        (nullval= post_fix_result(&(m_ifso->bg_resbuf_mgr),             \
                                  *geout, wkbres)))                     \
    {                                                                   \
      if (nullval)                                                      \
        return NULL;                                                    \
    }                                                                   \
    else                                                                \
      geom_out= geout.release();                                        \
  }                                                                     \
  else                                                                  \
  {                                                                     \
    (nullval)= true;                                                    \
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_" #bgop);                 \
    return NULL;                                                        \
  }                                                                     \
} while (0)


Item_func_spatial_operation::~Item_func_spatial_operation()
{
}


/*
  Write an empty geometry collection's wkb encoding into str, and create a
  geometry object for this empty geometry colletion.
 */
Geometry *Item_func_spatial_operation::empty_result(String *str, uint32 srid)
{
  if ((null_value= str->reserve(GEOM_HEADER_SIZE + 4 + 16, 256)))
    return 0;

  write_geometry_header(str, srid, Geometry::wkb_geometrycollection, 0);
  Gis_geometry_collection *gcol= new Gis_geometry_collection();
  gcol->set_data_ptr(str->ptr() + GEOM_HEADER_SIZE, 4);
  gcol->has_geom_header_space(true);
  return gcol;
}


/**
  Wraps and dispatches type specific BG function calls according to operation
  type and the 1st or both operand type(s), depending on code complexity.

  We want to isolate boost header file inclusion only inside this file, so we
  can't put this class declaration in any header file. And we want to make the
  methods static since no state is needed here.
  @tparam Geom_types A wrapper for all geometry types.
*/
template<typename Geom_types>
class BG_setop_wrapper
{
  // Some computation in this class may rely on functions in
  // Item_func_spatial_operation.
  Item_func_spatial_operation *m_ifso;
  my_bool null_value; // Whether computation has error.

  // Some computation in this class may rely on functions in
  // Item_func_spatial_operation, after each call of its functions, copy its
  // null_value, we don't want to miss errors.
  void copy_ifso_state()
  {
    null_value= m_ifso->null_value;
  }

public:
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;
  typedef Item_func_spatial_rel Ifsr;
  typedef Item_func_spatial_operation Ifso;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  BG_setop_wrapper(Item_func_spatial_operation *ifso)
  {
    m_ifso= ifso;
    null_value= 0;
  }


  my_bool get_null_value() const
  {
    return null_value;
  }


  /**
    Do point insersection point operation.
    @param g1 First Geometry operand, must be a Point.
    @param g2 Second Geometry operand, must be a Point.
    @param[out] result Holds WKB data of the result.
    @param[out] pdone Whether the operation is performed successfully.
    @return the result Geometry whose WKB data is in result.
    */
  Geometry *point_intersection_point(Geometry *g1, Geometry *g2,
                                     String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;

    *pdone= false;
    Point pt1(g1->get_data_ptr(),
              g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Point pt2(g2->get_data_ptr(),
              g2->get_data_size(), g2->get_flags(), g2->get_srid());

    if (bgpt_eq()(pt1, pt2))
    {
      retgeo= g1;
      null_value= retgeo->as_geometry(result, true);
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  /*
    Do point intersection Multipoint operation.
    The parameters and return value has identical/similar meaning as to above
    function, which can be inferred from the function name, we won't repeat
    here or for the rest of the functions in this class.
  */
  Geometry *point_intersection_multipoint(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;

    *pdone= false;
    Point pt(g1->get_data_ptr(),
             g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts(g2->get_data_ptr(),
                    g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point_set ptset(mpts.begin(), mpts.end());

    if (ptset.find(pt) != ptset.end())
    {
      retgeo= g1;
      null_value= retgeo->as_geometry(result, true);
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_intersection_geometry(Geometry *g1, Geometry *g2,
                                        String *result, bool *pdone)
  {
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Geometry *retgeo= NULL;
    *pdone= false;
    /*
      Whether Ifsr::bg_geo_relation_check or this function is
      completed. Only check this variable immediately after calling the two
      functions. If !isdone, unable to proceed, simply return 0.

      This is also true for other uses of this variable in other member
      functions of this class.
     */
    bool isdone= false;

    bool is_out= !Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
      (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value);

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    if (isdone && !null_value)
    {
      if (is_out)
      {
        null_value= g1->as_geometry(result, true);
        retgeo= g1;
      }
      else
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
      *pdone= true;
    }
    return retgeo;
  }


  Geometry *multipoint_intersection_multipoint(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;
    Point_set ptset1, ptset2;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    *pdone= false;
    mpts->set_srid(g1->get_srid());

    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());

    ptset1.insert(mpts1.begin(), mpts1.end());
    ptset2.insert(mpts2.begin(), mpts2.end());

    Point_vector respts;
    TYPENAME Point_vector::iterator endpos;
    size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size();
    respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);

    endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                  ptset2.begin(), ptset2.end(),
                                  respts.begin(), bgpt_lt());
    std::copy(respts.begin(), endpos, std::back_inserter(*mpts));
    if (mpts->size() > 0)
    {
      null_value= m_ifso->assign_result(mpts, result);
      retgeo= mpts;
      guard.release();
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_intersection_geometry(Geometry *g1, Geometry *g2,
                                             String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Point_set ptset;
    Multipoint mpts(g1->get_data_ptr(),
                    g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint *mpts2= new Multipoint();
    auto_ptr<Multipoint> guard(mpts2);
    bool isdone= false;

    *pdone= false;
    mpts2->set_srid(g1->get_srid());

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    ptset.insert(mpts.begin(), mpts.end());

    for (TYPENAME Point_set::iterator i= ptset.begin(); i != ptset.end(); ++i)
    {
      Point &pt= const_cast<Point&>(*i);
      if (!Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&pt, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
          isdone && !null_value)
      {
        mpts2->push_back(pt);
      }

      if (null_value || !isdone)
        return 0;
    }

    if (mpts2->size() > 0)
    {
      null_value= m_ifso->assign_result(mpts2, result);
      retgeo= mpts2;
      guard.release();
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_intersection_polygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    Geometry::wkbType gt2= g2->get_type();
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;
    *pdone= false;
    // It is likely for there to be discrete intersection Points.
    if (gt2 == Geometry::wkb_multipolygon)
    {
      BGOPCALL(Multilinestring, tmp1, intersection,
               Linestring, g1, Multipolygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Linestring, g1, Multipolygon, g2, NULL, null_value);
    }
    else
    {
      BGOPCALL(Multilinestring, tmp1, intersection,
               Linestring, g1, Polygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Linestring, g1, Polygon, g2, NULL, null_value);
    }

    // Need merge, exclude Points that are on the result Linestring.
    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_intersection_multilinestring(Geometry *g1, Geometry *g2,
                                                 String *result, bool *pdone)
  {
    Geometry *retgeo= NULL, *tmp1= NULL;
    Multipoint *tmp2= NULL;
    auto_ptr<Geometry> guard1;

    *pdone= false;

    BGOPCALL(Multilinestring, tmp1, intersection,
             Polygon, g1, Multilinestring, g2, NULL, null_value);
    guard1.reset(tmp1);

    Multilinestring mlstr(g2->get_data_ptr(), g2->get_data_size(),
                          g2->get_flags(), g2->get_srid());
    Multipoint mpts;
    Point_set ptset;

    const void *data_ptr= g1->normalize_ring_order();
    if (data_ptr == NULL)
    {
      null_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_intersection");
      return NULL;
    }

    Polygon plgn(data_ptr, g1->get_data_size(),
                 g1->get_flags(), g1->get_srid());

    for (TYPENAME Multilinestring::iterator i= mlstr.begin();
         i != mlstr.end(); ++i)
    {
      boost::geometry::intersection(plgn, *i, mpts);
      if (mpts.size() > 0)
      {
        ptset.insert(mpts.begin(), mpts.end());
        mpts.clear();
      }
    }

    auto_ptr<Multipoint> guard2;
    if (ptset.size() > 0)
    {
      tmp2= new Multipoint;
      tmp2->set_srid(g1->get_srid());
      guard2.reset(tmp2);
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*tmp2));
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (guard1.release(), guard2.release(), result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_intersection_polygon(Geometry *g1, Geometry *g2,
                                         String *result, bool *pdone)
  {
    *pdone= false;
    Geometry::wkbType gt2= g2->get_type();
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;

    if (gt2 == Geometry::wkb_polygon)
    {
      BGOPCALL(Multipolygon, tmp1, intersection,
               Polygon, g1, Polygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Polygon, g1, Polygon, g2, NULL, null_value);
    }
    else
    {
      BGOPCALL(Multipolygon, tmp1, intersection,
               Polygon, g1, Multipolygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Polygon, g1, Multipolygon, g2, NULL, null_value);
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_intersection_multipolygon(Geometry *g1,
                                                      Geometry *g2,
                                                      String *result,
                                                      bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL, *tmp1= NULL;
    Multipoint *tmp2= NULL;

    auto_ptr<Geometry> guard1;
    BGOPCALL(Multilinestring, tmp1, intersection,
             Multilinestring, g1, Multipolygon, g2,
             NULL, null_value);
    guard1.reset(tmp1);

    Multilinestring mlstr(g1->get_data_ptr(), g1->get_data_size(),
                          g1->get_flags(), g1->get_srid());
    Multipoint mpts;

    const void *data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      null_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_intersection");
      return NULL;
    }

    Multipolygon mplgn(data_ptr, g2->get_data_size(),
                       g2->get_flags(), g2->get_srid());
    Point_set ptset;

    for (TYPENAME Multilinestring::iterator i= mlstr.begin();
         i != mlstr.end(); ++i)
    {
      boost::geometry::intersection(*i, mplgn, mpts);
      if (mpts.size() > 0)
      {
        ptset.insert(mpts.begin(), mpts.end());
        mpts.clear();
      }
    }

    auto_ptr<Multipoint> guard2;
    if (ptset.empty() == false)
    {
      tmp2= new Multipoint;
      tmp2->set_srid(g1->get_srid());
      guard2.reset(tmp2);
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*tmp2));
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (guard1.release(), guard2.release(), result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_intersection_multipolygon(Geometry *g1, Geometry *g2,
                                                   String *result,
                                                   bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;
    BGOPCALL(Multipolygon, tmp1, intersection,
             Multipolygon, g1, Multipolygon, g2, NULL, null_value);

    BGOPCALL(Multipoint, tmp2, intersection,
             Multipolygon, g1, Multipolygon, g2, NULL, null_value);

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_union_point(Geometry *g1, Geometry *g2,
                              String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Geometry::wkbType gt2= g2->get_type();
    Point_set ptset;// Use set to make Points unique.

    Point pt1(g1->get_data_ptr(),
              g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    ptset.insert(pt1);
    if (gt2 == Geometry::wkb_point)
    {
      Point pt2(g2->get_data_ptr(),
                g2->get_data_size(), g2->get_flags(), g2->get_srid());
      ptset.insert(pt2);
    }
    else
    {
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      ptset.insert(mpts2.begin(), mpts2.end());
    }

    std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));
    if (mpts->size() > 0)
    {
      retgeo= mpts;
      null_value= m_ifso->assign_result(mpts, result);
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_union_geometry(Geometry *g1, Geometry *g2,
                                 String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    bool isdone= false;

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
        (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
        isdone && !null_value)
    {
      Gis_geometry_collection *geocol= new Gis_geometry_collection(g2, result);
      null_value= (geocol == NULL || geocol->append_geometry(g1, result));
      retgeo= geocol;
    }
    else if (!isdone || null_value)
    {
      retgeo= NULL;
      return 0;
    }
    else
    {
      retgeo= g2;
      null_value= retgeo->as_geometry(result, true);
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_union_multipoint(Geometry *g1, Geometry *g2,
                                        String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Point_set ptset;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());

    ptset.insert(mpts1.begin(), mpts1.end());
    ptset.insert(mpts2.begin(), mpts2.end());
    std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));

    if (mpts->size() > 0)
    {
      retgeo= mpts;
      null_value= m_ifso->assign_result(mpts, result);
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_union_geometry(Geometry *g1, Geometry *g2,
                                      String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Point_set ptset;
    Multipoint mpts(g1->get_data_ptr(),
                    g1->get_data_size(), g1->get_flags(), g1->get_srid());
    bool isdone= false;

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    ptset.insert(mpts.begin(), mpts.end());

    Gis_geometry_collection *geocol= new Gis_geometry_collection(g2, result);
    auto_ptr<Gis_geometry_collection> guard(geocol);
    bool added= false;

    for (TYPENAME Point_set::iterator i= ptset.begin(); i != ptset.end(); ++i)
    {
      Point &pt= const_cast<Point&>(*i);
      if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&pt, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
          isdone)
      {
        if (null_value || (null_value= geocol->append_geometry(&pt, result)))
          break;
        added= true;
      }

      if (!isdone)
        break;
    }

    if (null_value || !isdone)
      return 0;

    if (added)
    {
      // Result is already filled above.
      retgeo= geocol;
      guard.release();
    }
    else
    {
      retgeo= g2;
      null_value= g2->as_geometry(result, true);
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_union_polygon(Geometry *g1, Geometry *g2,
                                  String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_, Polygon, g1, Polygon, g2,
             result, null_value);
    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_union_multipolygon(Geometry *g1, Geometry *g2,
                                       String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_,
             Polygon, g1, Multipolygon, g2, result, null_value);
    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_union_multipolygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_difference_geometry(Geometry *g1, Geometry *g2,
                                      String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    bool isdone= false;
    bool is_out= Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
      (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value);

    if (isdone && !null_value)
    {
      if (is_out)
      {
        retgeo= g1;
        null_value= retgeo->as_geometry(result, true);
      }
      else
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
      if (!null_value)
        *pdone= true;
    }
    return retgeo;
  }


  Geometry *multipoint_difference_geometry(Geometry *g1, Geometry *g2,
                                           String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Point_set ptset;
    bool isdone= false;

    for (TYPENAME Multipoint::iterator i= mpts1.begin();
         i != mpts1.end(); ++i)
    {
      if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&(*i), g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) && isdone)
      {
        if (null_value)
          return 0;
        ptset.insert(*i);
      }

      if (!isdone)
        return 0;
    }

    if (ptset.empty() == false)
    {
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));
      null_value= m_ifso->assign_result(mpts, result);
      retgeo= mpts;
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_difference_polygon(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Linestring, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_difference_multipolygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Linestring, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_difference_polygon(Geometry *g1, Geometry *g2,
                                       String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Polygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_difference_multipolygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Polygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_difference_polygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Multilinestring, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_difference_multipolygon(Geometry *g1, Geometry *g2,
                                                    String *result,
                                                    bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Multilinestring, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_difference_polygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Multipolygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_difference_multipolygon(Geometry *g1, Geometry *g2,
                                                 String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_symdifference_polygon(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Polygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_symdifference_multipolygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Polygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_symdifference_polygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Multipolygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_symdifference_multipolygon(Geometry *g1, Geometry *g2,
                                                    String *result,
                                                    bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }
};


/**
  Do intersection operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
intersection_operation(Geometry *g1, Geometry *g2,
                       String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_intersection_point(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipoint:
      retgeo= wrap.point_intersection_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_intersection_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }

    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_intersection_multipoint(g2, g1, result, pdone);
      break;

    case Geometry::wkb_multipoint:
      retgeo= wrap.multipoint_intersection_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_intersection_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      /*
        The Multilinestring call isn't supported for these combinations,
        but such a result is quite likely, thus can't use bg for
        this combination.
       */
      break;
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.linestring_intersection_polygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      retgeo= wrap.polygon_intersection_multilinestring(g1, g2,
                                                        result, pdone);
      break;
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      // Note: for now BG's set operations don't allow returning a
      // Multilinestring, thus this result isn't complete.
      retgeo= wrap.polygon_intersection_polygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The Multilinestring call isn't supported for these combinations,
        but such a result is quite likely, thus can't use bg for
        this combination.
       */
      break;

    case Geometry::wkb_multipolygon:
      retgeo= wrap.multilinestring_intersection_multipolygon(g1, g2,
                                                             result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_intersection_multipolygon(g1, g2,
                                                          result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do union operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
union_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  // Note that union can't produce empty point set unless given two empty
  // point set arguments.
  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= wrap.point_union_point(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_union_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_union_point(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipoint:
      retgeo= wrap.multipoint_union_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_union_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
    /*
      boost geometry doesn't support union with either parameter being
      Linestring or Multilinestring, and we can't do simple calculation
      to Linestring as Points above. In following code this is denoted
      as NOT_SUPPORTED_BY_BG.

      Note: Also, current bg::union functions don't allow result being
      Multilinestring, thus these calculation isn't possible.
     */
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      // NOT_SUPPORTED_BY_BG
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_union_polygon(g1, g2, result, pdone);
      // Union can't produce empty point set.
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_union_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
      break;
    case Geometry::wkb_multilinestring:
      // NOT_SUPPORTED_BY_BG
      break;
    case Geometry::wkb_multipolygon:
      // NOT_SUPPORTED_BY_BG
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_union_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do difference operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
difference_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  /*
    Given two geometries g1 and g2, where g1.dimension < g2.dimension, then
    g2 - g1 is equal to g2, this is always true. This is how postgis works.
    Below implementation uses this fact.
   */
  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_difference_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_difference_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);
      if (!null_value)
        *pdone= true;
      break;
    case Geometry::wkb_linestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.linestring_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.linestring_difference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_difference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_linestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.multilinestring_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multilinestring_difference_multipolygon(g1, g2,
                                                           result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.multipolygon_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_difference_multipolygon(g1, g2,
                                                        result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do symdifference operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
symdifference_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  /*
    Note: g1 sym-dif g2 <==> (g1 union g2) dif (g1 intersection g2), so
    theoretically we can compute symdifference results for any type
    combination using the other 3 kinds of set operations. We need to use
    geometry collection set operations to implement symdifference of any
    two geometry, because the return values of them may be
    geometry-collections.

    Boost geometry explicitly and correctly supports symdifference for the
    following four type combinations.
   */
  bool do_geocol_setop= false;

  switch (gt1)
  {
  case Geometry::wkb_polygon:

    switch (gt2)
    {
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_symdifference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_symdifference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      do_geocol_setop= true;
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      retgeo= wrap.multipolygon_symdifference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_symdifference_multipolygon(g1, g2,
                                                           result, pdone);
      break;
    default:
      do_geocol_setop= true;
      break;
    }
    break;
  default:
    do_geocol_setop= true;
    break;
  }

  if (do_geocol_setop)
    retgeo= geometry_collection_set_operation<Coord_type,
      Coordsys>(g1, g2, result, pdone);
  else
    null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Call boost geometry set operations to compute set operation result, and
  returns the result as a Geometry object.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param opdone takes back whether the set operation is successfully completed,
  failures include:
    1. boost geometry doesn't support a type combination for a set operation.
    2. gis computation(not mysql code) got error, null_value isn't set to true.
    3. the relation check called isn't completed successfully, unable to proceed
       the set operation, and null_value isn't true.
  It is used in order to distinguish the types of errors above. And when caller
  got an false 'opdone', it should fallback to old gis set operation.

  @param[out] result buffer containing the GEOMETRY byte string of
  the returned geometry.

  @return If the set operation results in an empty point set, return a
  geometry collection containing 0 objects. If opdone or null_value is set to
  true, always returns 0. The returned geometry object can be used in the same
  val_str call.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
bg_geo_set_op(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef BG_models<Coord_type, Coordsys> Geom_types;

  Geometry *retgeo= NULL;

  if (g1->get_coordsys() != g2->get_coordsys())
    return 0;

  *pdone= false;

  switch (spatial_op)
  {
  case Gcalc_function::op_intersection:
    retgeo= intersection_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_union:
    retgeo= union_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_difference:
    retgeo= difference_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_symdifference:
    retgeo= symdifference_operation<Geom_types>(g1, g2, result, pdone);
    break;
  default:
    // Other operations are not set operations.
    DBUG_ASSERT(false);
    break;
  }

  /*
    null_value is set in above xxx_operatoin calls if error occured.
  */
  if (null_value)
  {
    error_str();
    *pdone= false;
    DBUG_ASSERT(retgeo == NULL);
  }

  // If we got effective result, the wkb encoding is written to 'result', and
  // the retgeo is effective Geometry object whose data Points into
  // 'result''s data.
  return retgeo;

}


/**
  Combine sub-results of set operation into a geometry collection.
  This function eliminates points in geo2 that are within
  geo1(polygons or linestrings). We have to do so
  because BG set operations return results in 3 forms --- multipolygon,
  multilinestring and multipoint, however given a type of set operation and
  the operands, the returned 3 types of results may intersect, and we want to
  eliminate the points already in the polygons/linestrings. And in future we
  also need to remove the linestrings that are already in the polygons, this
  isn't done now because there are no such set operation results to combine.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param geo1 First operand, a Multipolygon or Multilinestring object
              computed by BG set operation.
  @param geo2 Second operand, a Multipoint object
              computed by BG set operation.
  @param result Holds result geometry's WKB data in GEOMETRY format.
  @return A geometry combined from geo1 and geo2. Either or both of
  geo1 and geo2 can be NULL, so we may end up with a multipoint,
  a multipolygon/multilinestring, a geometry collection, or an
  empty geometry collection.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
combine_sub_results(Geometry *geo1, Geometry *geo2, String *result)
{
  typedef BG_models<Coord_type, Coordsys> Geom_types;
  typedef typename Geom_types::Multipoint Multipoint;
  Geometry *retgeo= NULL;
  bool isin= false, isdone= false, added= false;

  if (null_value)
  {
    delete geo1;
    delete geo2;
    return NULL;
  }

  auto_ptr<Geometry> guard1(geo1), guard2(geo2);

  Gis_geometry_collection *geocol= NULL;
  if (geo1 == NULL && geo2 == NULL)
    retgeo= empty_result(result, Geometry::default_srid);
  else if (geo1 != NULL && geo2 == NULL)
  {
    retgeo= geo1;
    null_value= assign_result(geo1, result);
    guard1.release();
  }
  else if (geo1 == NULL && geo2 != NULL)
  {
    retgeo= geo2;
    null_value= assign_result(geo2, result);
    guard2.release();
  }

  if (geo1 == NULL || geo2 == NULL)
  {
    if (null_value)
      retgeo= NULL;
    return retgeo;
  }

  DBUG_ASSERT((geo1->get_type() == Geometry::wkb_multilinestring ||
               geo1->get_type() == Geometry::wkb_multipolygon) &&
              geo2->get_type() == Geometry::wkb_multipoint);
  Multipoint mpts(geo2->get_data_ptr(), geo2->get_data_size(),
                  geo2->get_flags(), geo2->get_srid());
  geocol= new Gis_geometry_collection(geo1, result);
  geocol->set_components_no_overlapped(geo1->is_components_no_overlapped());
  auto_ptr<Gis_geometry_collection> guard3(geocol);
  my_bool had_error= false;

  for (TYPENAME Multipoint::iterator i= mpts.begin();
       i != mpts.end(); ++i)
  {
    isin= !Item_func_spatial_rel::bg_geo_relation_check<Coord_type,
      Coordsys>(&(*i), geo1, &isdone, SP_DISJOINT_FUNC, &had_error);

    // The bg_geo_relation_check can't handle pt intersects/within/disjoint ls
    // for now(isdone == false), so we have no points in mpts. When BG's
    // missing feature is completed, we will work correctly here.
    if (had_error)
    {
      error_str();
      return NULL;
    }

    if (!isin)
    {
      geocol->append_geometry(&(*i), result);
      added= true;
    }
  }

  if (added)
  {
    retgeo= geocol;
    guard3.release();
  }
  else
  {
    retgeo= geo1;
    guard1.release();
    null_value= assign_result(geo1, result);
  }

  if (null_value)
    error_str();

  return retgeo;
}


/**
  Extract a basic geometry component from a multi geometry or a geometry
  collection, if it's the only one in it.
 */
class Singleton_extractor : public WKB_scanner_event_handler
{
  /*
    If we see the nested geometries as a forest, seeing the outmost one as the
    ground where the trees grow, and seeing each of its components
    as a tree, then the search for a singleton in a geometry collection(GC) or
    multi-geometry(i.e. multipoint, multilinestring, multipolygon) is identical
    to searching on the ground to see if there is only one tree on the ground,
    if so we also need to record its starting address within the root node's
    memory buffer.

    Some details complicate the problem:
    1. GCs can be nested into another GC, a nested GC should be see also as
       the 'ground' rather than a tree.
    2. A single multi-geometry contained in a GC may be a singleton or not.
       a. When it has only one component in itself, that component is
          the singleton;
       b. Otherwise itself is the singleton.
    3. Basic geometries are always atomic(undevidible).
    4. A multi-geometry can't be nested into another multigeometry, it can
       only be a component of a GC.

    Below comment for the data members are based on this context information.
  */
  // The number of trees on the ground.
  int ntrees;
  // The number of trees inside all multi-geometries.
  int nsubtrees;
  // Current tree travasal stack depth, i.e. tree height.
  int depth;
  // The depth of the multi-geometry, if any.
  int mg_depth;
  // The effective stack depth, i.e. excludes the nested GCs.
  int levels;
  // The stack depth of heighest GC in current ground.
  int gc_depth;
  // Starting and ending address of tree on ground.
  const char *start, *end;
  // Starting address of and type of the basic geometry which is on top of the
  // multi-geometry.
  const char *bg_start;
  Geometry::wkbType bg_type;

  // The type of the geometry on the ground.
  Geometry::wkbType gtype;
public:
  Singleton_extractor()
  {
    ntrees= nsubtrees= depth= mg_depth= levels= gc_depth= 0;
    bg_start= start= end= NULL;
    bg_type= gtype= Geometry::wkb_invalid_type;
  }

  static bool is_basic_type(const Geometry::wkbType t)
  {
    return t == Geometry::wkb_point || t == Geometry::wkb_linestring ||
      t == Geometry::wkb_polygon;
  }

  bool has_single_component() const
  {
    return ntrees == 1;
  }

  // Functions to get singleton information.

  /*
    Returns start of singleton. If only one sub-tree, the basic geometry
    is returned instead of the multi-geometry, otherwise the multi-geometry
    is returned.
   */
  const char *get_start() const
  {
    return nsubtrees == 1 ? bg_start : start;
  }

  /*
    Returns the end of the singleton geometry. For a singleton,
    its end is always also the end of the root geometry, so this function
    is correct only when the root geometry really contains a singleton.
   */
  const char *get_end() const
  {
    return end;
  }

  Geometry::wkbType get_type() const
  {
    return nsubtrees == 1 ? bg_type : gtype;
  }


  virtual void on_wkb_start(Geometry::wkbByteOrder bo,
                            Geometry::wkbType geotype,
                            const void *wkb, uint32 len, bool has_hdr)
  {
    if (geotype != Geometry::wkb_geometrycollection)
    {
      if (gc_depth == 0)
      {
        gc_depth= depth;
        start= static_cast<const char *>(wkb);
        end= start + len;
        gtype= geotype;
      }

      if (!is_basic_type(geotype))
        mg_depth= depth;

      if (mg_depth + 1 == depth)
      {
        bg_type= geotype;
        bg_start= static_cast<const char *>(wkb);
      }

      levels++;
    }
    else
      gc_depth= 0;

    depth++;
  }


  virtual void on_wkb_end(const void *wkb)
  {
    depth--;
    DBUG_ASSERT(depth >= 0);

    if (levels > 0)
    {
      levels--;
      if (levels == 0)
      {
        DBUG_ASSERT(depth == gc_depth);
        ntrees++;
        end= static_cast<const char *>(wkb);
        mg_depth= 0;
        gc_depth= 0;
      }
    }

    // The subtree is either a multi-geometry or a basic geometry.
    if (mg_depth != 0 && levels == 1)
      nsubtrees++;
  }
};


/**
  Simplify multi-geometry data. If str contains a multi-geometry or geometry
  collection with one component, the component is made as content of str.
  If str contains a nested geometry collection, the effective concrete geometry
  object is returned.
  @param str A string buffer containing a GEOMETRY byte string.
  @return whether the geometry is simplified or not.
 */
static bool simplify_multi_geometry(String *str)
{
  if (str->length() < GEOM_HEADER_SIZE)
    return false;

  char *p= const_cast<char *>(str->ptr());
  Geometry::wkbType gtype= get_wkb_geotype(p + 5);
  bool ret= false;

  if (gtype == Geometry::wkb_multipoint ||
      gtype == Geometry::wkb_multilinestring ||
      gtype == Geometry::wkb_multipolygon)
  {
    if (uint4korr(p + GEOM_HEADER_SIZE) == 1)
    {
      DBUG_ASSERT((str->length() - GEOM_HEADER_SIZE - 4 - WKB_HEADER_SIZE) > 0);
      int4store(p + 5, static_cast<uint32>(base_type(gtype)));
      memmove(p + GEOM_HEADER_SIZE, p + GEOM_HEADER_SIZE + 4 + WKB_HEADER_SIZE,
              str->length() - GEOM_HEADER_SIZE - 4 - WKB_HEADER_SIZE);
      str->length(str->length() - 4 - WKB_HEADER_SIZE);
      ret= true;
    }
  }
  else if (gtype == Geometry::wkb_geometrycollection)
  {
    Singleton_extractor ex;
    uint32 wkb_len= str->length() - GEOM_HEADER_SIZE;
    wkb_scanner(p + GEOM_HEADER_SIZE, &wkb_len,
                Geometry::wkb_geometrycollection, false, &ex);
    if (ex.has_single_component())
    {
      p= write_wkb_header(p + 4, ex.get_type());
      ptrdiff_t len= ex.get_end() - ex.get_start();
      DBUG_ASSERT(len > 0);
      memmove(p, ex.get_start(), len);
      str->length(GEOM_HEADER_SIZE + len);
      ret= true;
    }
  }

  return ret;
}


/*
  Do set operations on geometries.
  Writes geometry set operation result into str_value_arg in wkb format.
 */
String *Item_func_spatial_operation::val_str(String *str_value_arg)
{
  DBUG_ENTER("Item_func_spatial_operation::val_str");
  DBUG_ASSERT(fixed == 1);
  tmp_value1.length(0);
  tmp_value2.length(0);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1= NULL, *g2= NULL, *gres= NULL;
  uint32 srid= 0;
  Gcalc_operation_transporter trn(&func, &collector);
  bool opdone= false;
  bool had_except1= false, had_except2= false;

  // Release last call's result buffer.
  bg_resbuf_mgr.free_result_buffer();

  // Clean up the result first, since caller may give us one with non-NULL
  // buffer, we don't need it here.
  str_value_arg->set(NullS, 0, &my_charset_bin);

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);
  func.add_operation(spatial_op, 2);

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    goto exit;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_str());
  }

  // The two geometry operand must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    DBUG_RETURN(error_str());
  }

  str_value_arg->set_charset(&my_charset_bin);
  str_value_arg->length(0);


  /*
    Catch all exceptions to make sure no exception can be thrown out of
    current function. Put all and any code that calls Boost.Geometry functions,
    STL functions into this try block. Code out of the try block should never
    throw any exception.
  */
  try
  {
    if (g1->get_type() != Geometry::wkb_geometrycollection &&
        g2->get_type() != Geometry::wkb_geometrycollection)
      gres= bg_geo_set_op<double, bgcs::cartesian>(g1, g2, str_value_arg,
                                                   &opdone);
    else
      gres= geometry_collection_set_operation<double, bgcs::cartesian>
        (g1, g2, str_value_arg, &opdone);

  }
  CATCH_ALL(func_name(), had_except1= true)

  try
  {
    /*
      The buffers in res1 and res2 either belong to argument Item_xxx objects
      or simply belong to tmp_value1 or tmp_value2, they will be deleted
      properly by their owners, not by our bg_resbuf_mgr, so here we must
      forget them in order not to free the buffers before the Item_xxx
      owner nodes are destroyed.
    */
    bg_resbuf_mgr.forget_buffer(const_cast<char *>(res1->ptr()));
    bg_resbuf_mgr.forget_buffer(const_cast<char *>(res2->ptr()));
    bg_resbuf_mgr.forget_buffer(const_cast<char *>(tmp_value1.ptr()));
    bg_resbuf_mgr.forget_buffer(const_cast<char *>(tmp_value2.ptr()));

    /*
      Release intermediate geometry data buffers accumulated during execution
      of this set operation.
    */
    if (!str_value_arg->is_alloced() && gres != g1 && gres != g2)
      bg_resbuf_mgr.set_result_buffer(const_cast<char *>(str_value_arg->ptr()));
    bg_resbuf_mgr.free_intermediate_result_buffers();
  }
  CATCH_ALL(func_name(), had_except2= true)

  if (had_except1 || had_except2 || null_value)
  {
    opdone= false;
    if (gres != NULL && gres != g1 && gres != g2)
    {
      delete gres;
      gres= NULL;
    }
    DBUG_RETURN(error_str());
  }

  if (gres != NULL)
  {
    DBUG_ASSERT(!null_value && opdone && str_value_arg->length() > 0);

    /*
      There are 4 ways to create the result geometry object and allocate
      memory for the result String object:
      1. Created in BGOPCALL and allocated by BG code using gis_wkb_alloc
         functions; The geometry result object's memory is took over by
         str_value_arg, thus not allocated by str_value_arg.
      2. Created as a Gis_geometry_collection object and allocated by
         str_value_arg's String member functions.
      3. One of g1 or g2 used as result and g1/g2's String object is used as
         final result without duplicating their byte strings. Also, g1 and/or
         g2 may be used as intermediate result and their byte strings are
         assigned to intermediate String objects without giving the ownerships
         to them, so they are always owned by tmp_value1 and/or tmp_value2.
      4. A geometry duplicated from a component of BG_geometry_collection.
         when both GCs have 1 member, we do set operation for the two members
         directly, and if such a component is the result we have to duplicate
         it and its WKB String buffer.

      Among above 4 ways, #1, #2 and #4 write the byte string only once without
      any data copying, #3 doesn't write any byte strings.

      And here we always have a GEOMETRY byte string in str_value_arg, although
      in some cases gres->has_geom_header_space() is false.
     */
    if (!str_value_arg->is_alloced() && gres != g1 && gres != g2)
    {
      DBUG_ASSERT(gres->has_geom_header_space() || gres->is_bg_adapter());
    }
    else
    {
      DBUG_ASSERT(gres->has_geom_header_space() || (gres == g1 || gres == g2));
      if (gres == g1)
        str_value_arg= res1;
      else if (gres == g2)
        str_value_arg= res2;
    }
    simplify_multi_geometry(str_value_arg);
    goto exit;
  }
  else if (opdone)
  {
    /*
      It's impossible to arrive here because the code calling BG features only
      returns NULL if not done, otherwise if result is empty, it returns an
      empty geometry collection whose pointer isn't NULL.
     */
    DBUG_ASSERT(false);
    goto exit;
  }

  DBUG_ASSERT(!opdone && gres == NULL);
  // We caught error, don't proceed with old GIS algorithm but error out.
  if (null_value)
    goto exit;

  /* Fall back to old GIS algorithm. */
  null_value= true;

  str_value_arg->set(NullS, 0U, &my_charset_bin);
  if (str_value_arg->reserve(SRID_SIZE, 512))
    goto exit;
  str_value_arg->q_append(srid);

  if (g1->store_shapes(&trn) || g2->store_shapes(&trn))
    goto exit;
#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  if (func.alloc_states())
    goto exit;

  operation.init(&func);

  if (operation.count_all(&collector) ||
      operation.get_result(&res_receiver))
    goto exit;


  if (!Geometry::create_from_opresult(&buffer1, str_value_arg, res_receiver))
    goto exit;

  /*
    If got some result, it's not NULL, note that we prepended an
    srid above(4 bytes).
  */
  if (str_value_arg->length() > 4)
    null_value= false;

exit:
  collector.reset();
  func.reset();
  res_receiver.reset();
  if (gres != g1 && gres != g2 && gres != NULL)
    delete gres;
  DBUG_RETURN(null_value ? NULL : str_value_arg);
}


/**
  Do set operation on geometry collections.
  BG doesn't directly support geometry collections in any function, so we
  have to do so by computing the set operation result of all two operands'
  components, which must be the 6 basic types of geometries, and then we
  combine the sub-results.

  This function dispatches to specific set operation types.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
  two geometry collections. We rely on some Boost Geometry functions to do
  geometry relation checks and set operations to the components of g1 and g2,
  and since BG now doesn't support some type combinations for each type of
  operaton, we may not be able to perform the operation. If so, old GIS
  algorithm is called to do so.
  @return The set operation result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geometry_collection_set_operation(Geometry *g1, Geometry *g2,
                                  String *result, bool *pdone)
{
  bool opdone= false;
  Geometry *gres= NULL;
  BG_geometry_collection bggc1, bggc2;

  *pdone= false;
  bggc1.set_srid(g1->get_srid());
  bggc2.set_srid(g2->get_srid());
  bool empty1= is_empty_geocollection(g1);
  bool empty2= is_empty_geocollection(g2);

  /* Short cut for either one operand being empty. */
  if (empty1 || empty2)
  {
    if (spatial_op == Gcalc_function::op_intersection ||
        (empty1 && empty2 && (spatial_op == Gcalc_function::op_symdifference ||
                              spatial_op == Gcalc_function::op_union)) ||
        (empty1 && spatial_op == Gcalc_function::op_difference))
    {
      *pdone= true;
      return empty_result(result, g1->get_srid());
    }

    if (empty1 && (spatial_op == Gcalc_function::op_union ||
                   spatial_op == Gcalc_function::op_symdifference))
    {
      *pdone= true;
      null_value= g2->as_geometry(result, true/* shallow copy */);
      return g2;
    }

    if (empty2 && (spatial_op == Gcalc_function::op_difference ||
                   spatial_op == Gcalc_function::op_union ||
                   spatial_op == Gcalc_function::op_symdifference))
    {
      *pdone= true;
      null_value= g1->as_geometry(result, true/* shallow copy */);
      return g1;
    }
  }

  bggc1.fill(g1);
  bggc2.fill(g2);
  if (spatial_op != Gcalc_function::op_union)
  {
    bggc1.merge_components<Coord_type, Coordsys>(&opdone, &null_value);
    if (null_value)
      return gres;
    bggc2.merge_components<Coord_type, Coordsys>(&opdone, &null_value);
    if (null_value)
      return gres;
  }

  BG_geometry_collection::Geometry_list &gv1= bggc1.get_geometries();
  BG_geometry_collection::Geometry_list &gv2= bggc2.get_geometries();

  /*
    If both collections have only one basic component, do basic set operation.
    The exception is symdifference with at least one operand being not a
    polygon or multipolygon, in which case this exact function is called to
    perform symdifference for the two basic components.
   */
  if (gv1.size() == 1 && gv2.size() == 1 &&
      (spatial_op != Gcalc_function::op_symdifference ||
       (is_areal(*(gv1.begin())) && is_areal(*(gv2.begin())))))
  {
    gres= bg_geo_set_op<Coord_type, Coordsys>(*(gv1.begin()), *(gv2.begin()),
                                              result, pdone);
    /*
      If this set operation gives us a gres that's a component/member of either
      bggc1 or bggc2, we have to duplicate the object and its buffer because
      they will be destroyed when bggc1/bggc2 goes out of scope.
     */
    bool do_dup= false;
    for (BG_geometry_collection::Geometry_list::iterator i= gv1.begin();
         i != gv1.end(); ++i)
      if (*i == gres)
        do_dup= true;
    if (!do_dup)
      for (BG_geometry_collection::Geometry_list::iterator i= gv2.begin();
           i != gv2.end(); ++i)
        if (*i == gres)
          do_dup= true;

    if (do_dup)
    {
      String tmpres;
      Geometry *gres2= NULL;
      tmpres.append(result->ptr(), result->length());
      const void *data_start= static_cast<const char *>(tmpres.ptr()) +
        GEOM_HEADER_SIZE;

      switch (gres->get_geotype())
      {
      case Geometry::wkb_point:
        gres2= new Gis_point;
        break;
      case Geometry::wkb_linestring:
        gres2= new Gis_line_string;
        break;
      case Geometry::wkb_polygon:
        gres2= new Gis_polygon;
        break;
      case Geometry::wkb_multipoint:
        gres2= new Gis_multi_point;
        break;
      case Geometry::wkb_multilinestring:
        gres2= new Gis_multi_line_string;
        break;
      case Geometry::wkb_multipolygon:
        gres2= new Gis_multi_polygon;
        break;
      default:
        DBUG_ASSERT(false);
      }

      gres2->set_data_ptr(data_start, tmpres.length() - GEOM_HEADER_SIZE);
      gres2->has_geom_header_space(true);
      gres2->set_bg_adapter(false);
      result->takeover(tmpres);
      gres= gres2;
    }

    return gres;
  }


  switch (this->spatial_op)
  {
  case Gcalc_function::op_intersection:
    gres= geocol_intersection<Coord_type, Coordsys>(bggc1, bggc2,
                                                    result, pdone);
    break;
  case Gcalc_function::op_union:
    gres= geocol_union<Coord_type, Coordsys>(bggc1, bggc2, result, pdone);
    break;
  case Gcalc_function::op_difference:
    gres= geocol_difference<Coord_type, Coordsys>(bggc1, bggc2, result, pdone);
    break;
  case Gcalc_function::op_symdifference:
    gres= geocol_symdifference<Coord_type, Coordsys>(bggc1, bggc2,
                                                     result, pdone);
    break;
  default:
    /* Only above four supported. */
    DBUG_ASSERT(false);
    break;
  }

  if (gres == NULL && *pdone && !null_value)
    gres= empty_result(result, g1->get_srid());
  return gres;
}


/**
  Do intersection operation on geometry collections. We do intersection for
  all pairs of components in g1 and g2, put the results in a geometry
  collection. If all subresults can be computed successfully, the geometry
  collection is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The intersection result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_intersection(const BG_geometry_collection &bggc1,
                    const BG_geometry_collection &bggc2,
                    String *result, bool *pdone)
{
  Geometry *gres= NULL;
  String wkbres;
  bool opdone= false;
  Geometry *g0= NULL;
  BG_geometry_collection bggc;
  const BG_geometry_collection::Geometry_list &gv1= bggc1.get_geometries();
  const BG_geometry_collection::Geometry_list &gv2= bggc2.get_geometries();
  *pdone= false;
  bggc.set_srid(bggc1.get_srid());

  if (gv1.size() == 0 || gv2.size() == 0)
  {
    *pdone= true;
    return empty_result(result, bggc1.get_srid());
  }

  const typename BG_geometry_collection::Geometry_list *gv= NULL, *gvr= NULL;

  if (gv1.size() > gv2.size())
  {
    gv= &gv2;
    gvr= &gv1;
  }
  else
  {
    gv= &gv1;
    gvr= &gv2;
  }

  Rtree_index rtree;
  make_rtree(*gvr, &rtree);
  Rtree_result rtree_result;

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv->begin();
       i != gv->end(); ++i)
  {
    BG_box box;
    make_bg_box(*i, &box);
    rtree_result.clear();
    rtree.query(bgi::intersects(box), std::back_inserter(rtree_result));
    if (rtree_result.size() == 0)
      continue;

    Rtree_entry_compare rtree_entry_compare;
    std::sort(rtree_result.begin(), rtree_result.end(), rtree_entry_compare);

    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      Geometry *geom= (*gvr)[j->second];
      // Free before using it, wkbres may have WKB data from last execution.
      wkbres.mem_free();
      opdone= false;
      g0= bg_geo_set_op<Coord_type, Coordsys>(*i, geom, &wkbres, &opdone);

      if (!opdone || null_value)
      {
        if (g0 != NULL && g0 != *i && g0 != geom)
          delete g0;
        return 0;
      }

      if (g0 && !is_empty_geocollection(wkbres))
        bggc.fill(g0);
      if (g0 != NULL && g0 != *i && g0 != geom)
      {
        delete g0;
        g0= NULL;
      }
    }
  }

  /*
    Note: result unify and merge

    The result may have geometry elements that overlap, caused by overlap
    geos in either or both gc1 and/or gc2. Also, there may be geometries
    that can be merged into a larger one of the same type in the result.
    We will need to figure out how to make such enhancements.
   */
  bggc.merge_components<Coord_type, Coordsys>(pdone, &null_value);
  if (null_value)
    return NULL;
  gres= bggc.as_geometry_collection(result);
  if (!null_value)
    *pdone= true;

  return gres;
}


/**
  Do union operation on geometry collections. We do union for
  all pairs of components in g1 and g2, whenever a union can be done, we do
  so and put the results in a geometry collection GC and remove the two
  components from g1 and g2 respectively. Finally no components in g1 and g2
  overlap and GC is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The union result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_union(const BG_geometry_collection &bggc1,
             const BG_geometry_collection &bggc2,
             String *result, bool *pdone)
{
  Geometry *gres= NULL;
  BG_geometry_collection bggc;
  BG_geometry_collection::Geometry_list &gv= bggc.get_geometries();
  gv.insert(gv.end(), bggc1.get_geometries().begin(),
            bggc1.get_geometries().end());
  gv.insert(gv.end(), bggc2.get_geometries().begin(),
            bggc2.get_geometries().end());
  bggc.set_srid(bggc1.get_srid());
  *pdone= false;

  // It's likely that there are overlapping components in bggc because it
  // has components from both bggc1 and bggc2.
  bggc.merge_components<Coord_type, Coordsys>(pdone, &null_value);
  if (!null_value && *pdone)
    gres= bggc.as_geometry_collection(result);

  return gres;
}


/**
  Do difference operation on geometry collections. For each component CX in g1,
  we do CX:= CX difference CY for all components CY in g2. When at last CX isn't
  empty, it's put into result geometry collection GC.
  If all subresults can be computed successfully, the geometry
  collection GC is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The difference result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_difference(const BG_geometry_collection &bggc1,
                  const BG_geometry_collection &bggc2,
                  String *result, bool *pdone)
{
  Geometry *gres= NULL;
  bool opdone= false;
  String *wkbres= NULL;
  BG_geometry_collection bggc;
  const BG_geometry_collection::Geometry_list *gv1= &(bggc1.get_geometries());
  const BG_geometry_collection::Geometry_list *gv2= &(bggc2.get_geometries());

  bggc.set_srid(bggc1.get_srid());
  *pdone= false;

  // Difference isn't symetric so we have to always build rtree tndex on gv2.
  Rtree_index rtree;
  make_rtree(*gv2, &rtree);
  Rtree_result rtree_result;

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv1->begin();
       i != gv1->end(); ++i)
  {
    bool g11_isempty= false;
    auto_ptr<Geometry> guard11;
    Geometry *g11= NULL;
    g11= *i;
    Inplace_vector<String> wkbstrs(PSI_INSTRUMENT_ME);

    BG_box box;
    make_bg_box(*i, &box);
    rtree_result.clear();
    rtree.query(bgi::intersects(box), std::back_inserter(rtree_result));

    Rtree_entry_compare rtree_entry_compare;
    std::sort(rtree_result.begin(), rtree_result.end(), rtree_entry_compare);

    /*
      Above theory makes sure all results are in rtree_result, the logic
      here is sufficient when rtree_result is empty.
    */
    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      Geometry *geom= (*gv2)[j->second];

      wkbres= wkbstrs.append_object();
      if (wkbres == NULL)
        return NULL;
      opdone= false;
      Geometry *g0= bg_geo_set_op<Coord_type, Coordsys>(g11, geom,
                                                        wkbres, &opdone);
      auto_ptr<Geometry> guard0(g0);

      if (!opdone || null_value)
      {
        if (!(g0 != NULL && g0 != *i && g0 != geom))
          guard0.release();
        if (!(g11 != NULL && g11 != g0 && g11 != *i && g11 != geom))
          guard11.release();
        return NULL;
      }

      if (g0 != NULL && !is_empty_geocollection(*wkbres))
      {
        if (g11 != NULL && g11 != *i && g11 != geom && g11 != g0)
          delete guard11.release();
        else
          guard11.release();
        guard0.release();
        g11= g0;
        if (g0 != NULL && g0 != *i && g0 != geom)
          guard11.reset(g11);
      }
      else
      {
        g11_isempty= true;
        if (!(g0 != NULL && g0 != *i && g0 != geom && g0 != g11))
          guard0.release();
        break;
      }
    }

    if (!g11_isempty)
      bggc.fill(g11);
    if (!(g11 != NULL && g11 != *i))
      guard11.release();
    else
      guard11.reset(NULL);
  }

  bggc.merge_components<Coord_type, Coordsys>(pdone, &null_value);
  if (null_value)
    return NULL;
  gres= bggc.as_geometry_collection(result);
  if (!null_value)
    *pdone= true;

  return gres;
}


/**
  Do symdifference operation on geometry collections. We do so according to
  this formula:
  g1 symdifference g2 <==> (g1 union g2) difference (g1 intersection g2).
  Since we've implemented the other 3 types of set operations for geometry
  collections, we can do so.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The symdifference result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_symdifference(const BG_geometry_collection &bggc1,
                     const BG_geometry_collection &bggc2,
                     String *result, bool *pdone)
{
  Geometry *gres= NULL;
  String wkbres;
  bool isdone1= false, isdone2= false, isdone3= false;
  String union_res, dif_res, isct_res;
  Geometry *gc_union= NULL, *gc_isct= NULL;

  *pdone= false;
  Var_resetter<Gcalc_function::op_type>
    var_reset(&spatial_op, Gcalc_function::op_symdifference);

  spatial_op= Gcalc_function::op_union;
  gc_union= geocol_union<Coord_type, Coordsys>(bggc1, bggc2,
                                               &union_res, &isdone1);
  auto_ptr<Geometry> guard_union(gc_union);

  if (!isdone1 || null_value)
    return NULL;
  DBUG_ASSERT(gc_union != NULL);

  spatial_op= Gcalc_function::op_intersection;
  gc_isct= geocol_intersection<Coord_type, Coordsys>(bggc1, bggc2,
                                                     &isct_res, &isdone2);
  auto_ptr<Geometry> guard_isct(gc_isct);

  if (!isdone2 || null_value)
    return NULL;

  auto_ptr<Geometry> guard_dif;
  if (gc_isct != NULL && !is_empty_geocollection(isct_res))
  {
    spatial_op= Gcalc_function::op_difference;
    gres= geometry_collection_set_operation<Coord_type, Coordsys>
      (gc_union, gc_isct, result, &isdone3);
    guard_dif.reset(gres);

    if (!isdone3 || null_value)
      return NULL;
  }
  else
  {
    gres= gc_union;
    result->takeover(union_res);
    guard_union.release();
  }

  *pdone= true;
  guard_dif.release();
  return gres;
}


bool Item_func_spatial_operation::assign_result(Geometry *geo, String *result)
{
  DBUG_ASSERT(geo->has_geom_header_space());
  char *p= geo->get_cptr() - GEOM_HEADER_SIZE;
  write_geometry_header(p, geo->get_srid(), geo->get_geotype());
  result->set(p, GEOM_HEADER_SIZE + geo->get_nbytes(), &my_charset_bin);
  bg_resbuf_mgr.add_buffer(p);
  geo->set_ownmem(false);

  return false;
}


const char *Item_func_spatial_operation::func_name() const
{
  switch (spatial_op) {
    case Gcalc_function::op_intersection:
      return "st_intersection";
    case Gcalc_function::op_difference:
      return "st_difference";
    case Gcalc_function::op_union:
      return "st_union";
    case Gcalc_function::op_symdifference:
      return "st_symdifference";
    default:
      DBUG_ASSERT(0);  // Should never happen
      return "sp_unknown";
  }
}

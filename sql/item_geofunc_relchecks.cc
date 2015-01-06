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
  This file defines implementations of GIS relation check functions.
*/
#include "my_config.h"
#include "item_geofunc_internal.h"


/*
  Functions for spatial relations
*/

const char *Item_func_spatial_mbr_rel::func_name() const
{
  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "mbrcontains";
    case SP_WITHIN_FUNC:
      return "mbrwithin";
    case SP_EQUALS_FUNC:
      return "mbrequals";
    case SP_DISJOINT_FUNC:
      return "mbrdisjoint";
    case SP_INTERSECTS_FUNC:
      return "mbrintersects";
    case SP_TOUCHES_FUNC:
      return "mbrtouches";
    case SP_CROSSES_FUNC:
      return "mbrcrosses";
    case SP_OVERLAPS_FUNC:
      return "mbroverlaps";
    case SP_COVERS_FUNC:
      return "mbrcovers";
    case SP_COVEREDBY_FUNC:
      return "mbrcoveredby";
    default:
      DBUG_ASSERT(0);  // Should never happened
      return "mbrsp_unknown";
  }
}


longlong Item_func_spatial_mbr_rel::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&cmp.value1);
  String *res2= args[1]->val_str(&cmp.value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  MBR mbr1, mbr2;

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    return 0;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  if ((null_value= (g1->get_mbr(&mbr1) || g2->get_mbr(&mbr2))))
    return 0;

  // The two geometry operands must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    null_value= true;
    return 0;
  }

  int ret= 0;

  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      ret= mbr1.contains(&mbr2);
      break;
    case SP_WITHIN_FUNC:
      ret= mbr1.within(&mbr2);
      break;
    case SP_EQUALS_FUNC:
      ret= mbr1.equals(&mbr2);
      break;
    case SP_DISJOINT_FUNC:
      ret= mbr1.disjoint(&mbr2);
      break;
    case SP_INTERSECTS_FUNC:
      ret= mbr1.intersects(&mbr2);
      break;
    case SP_TOUCHES_FUNC:
      ret= mbr1.touches(&mbr2);
      break;
    case SP_OVERLAPS_FUNC:
      ret= mbr1.overlaps(&mbr2);
      break;
    case SP_CROSSES_FUNC:
      DBUG_ASSERT(false);
      ret= 0;
      null_value= true;
      break;
    case SP_COVERS_FUNC:
      ret= mbr1.covers(&mbr2);
      break;
    case SP_COVEREDBY_FUNC:
      ret= mbr1.covered_by(&mbr2);
      break;
    default:
      break;
  }

  if (ret == -1)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }

  return ret;
}


Item_func_spatial_rel::Item_func_spatial_rel(const POS &pos, Item *a,Item *b,
                                             enum Functype sp_rel) :
    Item_bool_func2(pos, a,b), collector()
{
  spatial_rel= sp_rel;
}


Item_func_spatial_rel::~Item_func_spatial_rel()
{
}


const char *Item_func_spatial_rel::func_name() const
{
  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "st_contains";
    case SP_WITHIN_FUNC:
      return "st_within";
    case SP_EQUALS_FUNC:
      return "st_equals";
    case SP_DISJOINT_FUNC:
      return "st_disjoint";
    case SP_INTERSECTS_FUNC:
      return "st_intersects";
    case SP_TOUCHES_FUNC:
      return "st_touches";
    case SP_CROSSES_FUNC:
      return "st_crosses";
    case SP_OVERLAPS_FUNC:
      return "st_overlaps";
    default:
      DBUG_ASSERT(0);  // Should never happened
      return "sp_unknown";
  }
}


longlong Item_func_spatial_rel::val_int()
{
  DBUG_ENTER("Item_func_spatial_rel::val_int");
  DBUG_ASSERT(fixed == 1);
  String *res1= NULL;
  String *res2= NULL;
  Geometry_buffer buffer1, buffer2;
  Geometry *g1= NULL, *g2= NULL;
  int result= 0;
  int mask= 0;
  int tres= 0;
  bool bgdone= false;
  bool had_except= false;
  my_bool had_error= false;
  String wkt1, wkt2;
  Gcalc_operation_transporter trn(&func, &collector);

  res1= args[0]->val_str(&tmp_value1);
  res2= args[1]->val_str(&tmp_value2);
  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    goto exit;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    tres= error_int();
    goto exit;
  }

  // The two geometry operands must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    tres= error_int();
    goto exit;
  }

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
    {
      // Must use double, otherwise may lose valid result, not only precision.
      tres= bg_geo_relation_check<double, bgcs::cartesian>
        (g1, g2, &bgdone, spatial_rel, &had_error);
    }
    else
      tres= geocol_relation_check<double, bgcs::cartesian>(g1, g2, &bgdone);
  }
  CATCH_ALL(func_name(), { had_except= true; })

  if (had_except || had_error || null_value)
  {
    bgdone= false;
    DBUG_RETURN(error_int());
  }

  if (bgdone)
    DBUG_RETURN(tres);

  // Start of old GIS algorithms for geometry relationship checks.
  if (spatial_rel == SP_TOUCHES_FUNC)
    DBUG_RETURN(func_touches());

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);

  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_backdifference, 2);
      break;
    case SP_WITHIN_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_difference, 2);
      break;
    case SP_EQUALS_FUNC:
      break;
    case SP_DISJOINT_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    case SP_INTERSECTS_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    case SP_OVERLAPS_FUNC:
      func.add_operation(Gcalc_function::op_backdifference, 2);
      break;
    case SP_CROSSES_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    default:
      DBUG_ASSERT(FALSE);
      break;
  }
  if ((null_value= (g1->store_shapes(&trn) || g2->store_shapes(&trn))))
    goto exit;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  scan_it.init(&collector);
  /* Note: other functions might be checked here as well. */
  if (spatial_rel == SP_EQUALS_FUNC ||
      spatial_rel == SP_WITHIN_FUNC ||
      spatial_rel == SP_CONTAINS_FUNC)
  {
    result= (g1->get_class_info()->m_type_id ==
             g1->get_class_info()->m_type_id) && func_equals();
    if (spatial_rel == SP_EQUALS_FUNC ||
        result) // for SP_WITHIN_FUNC and SP_CONTAINS_FUNC
      goto exit;
  }

  if (func.alloc_states())
    goto exit;

  result= func.find_function(scan_it) ^ mask;

exit:
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
}


/**
  Do geometry collection relation check. Boost geometry doesn't support
  geometry collections directly, we have to treat them as a collection of basic
  geometries and use BG features to compute.
  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::geocol_relation_check(Geometry *g1, Geometry *g2,
                                                 bool *pbgdone)
{
  String gcbuf;
  Geometry *tmpg= NULL;
  int tres= 0;
  const typename BG_geometry_collection::Geometry_list *gv1= NULL, *gv2= NULL;
  BG_geometry_collection bggc1, bggc2;
  bool empty1= is_empty_geocollection(g1);
  bool empty2= is_empty_geocollection(g2);

  *pbgdone= false;

  /*
    An empty geometry collection is an empty point set, according to OGC
    specifications and set theory we make below conclusion.
   */
  if (empty1 || empty2)
  {
    if (spatial_rel == SP_DISJOINT_FUNC)
      tres= 1;
    else if (empty1 && empty2 && spatial_rel == SP_EQUALS_FUNC)
      tres= 1;
    *pbgdone= true;
    return tres;
  }

  if (spatial_rel == SP_CONTAINS_FUNC)
  {
    tmpg= g2;
    g2= g1;
    g1= tmpg;
    spatial_rel= SP_WITHIN_FUNC;
  }

  bool opdone= false;

  bggc1.fill(g1);
  bggc2.fill(g2);
  bggc1.merge_components<Coord_type, Coordsys>(&opdone, &null_value);
  if (null_value)
    return tres;
  bggc2.merge_components<Coord_type, Coordsys>(&opdone, &null_value);
  if (null_value)
    return tres;

  gv1= &(bggc1.get_geometries());
  gv2= &(bggc2.get_geometries());

  if (gv1->size() == 0 || gv2->size() == 0)
  {
    null_value= true;
    *pbgdone= true;
    return tres;
  }
  else if (gv1->size() == 1 && gv2->size() == 1)
  {
    tres= bg_geo_relation_check<Coord_type, Coordsys>
      (*(gv1->begin()), *(gv2->begin()), pbgdone, spatial_rel, &null_value);
    return tres;
  }

  if (spatial_rel == SP_OVERLAPS_FUNC ||
      spatial_rel == SP_CROSSES_FUNC || spatial_rel == SP_TOUCHES_FUNC)
  {
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    *pbgdone= true;
    return 0;
  }

  if (spatial_rel == SP_DISJOINT_FUNC || spatial_rel == SP_INTERSECTS_FUNC)
    tres= geocol_relcheck_intersect_disjoint<Coord_type, Coordsys>
      (gv1, gv2, pbgdone);
  else if (spatial_rel == SP_WITHIN_FUNC)
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2, pbgdone);
  else if (spatial_rel == SP_EQUALS_FUNC)
    tres= geocol_equals_check<Coord_type, Coordsys>(gv1, gv2, pbgdone);
  else
    DBUG_ASSERT(false);

  /* If doing contains check, need to switch back the two operands. */
  if (tmpg)
  {
    DBUG_ASSERT(spatial_rel == SP_WITHIN_FUNC);
    spatial_rel= SP_CONTAINS_FUNC;
    tmpg= g2;
    g2= g1;
    g1= tmpg;
  }

  return tres;
}


/**
  Geometry collection relation checks for disjoint and intersects operations.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_intersect_disjoint(const typename BG_geometry_collection::
                                   Geometry_list *gv1,
                                   const typename BG_geometry_collection::
                                   Geometry_list *gv2,
                                   bool *pbgdone)
{
  int tres= 0;
  *pbgdone= false;

  DBUG_ASSERT(spatial_rel == SP_DISJOINT_FUNC ||
              spatial_rel == SP_INTERSECTS_FUNC);
  const typename BG_geometry_collection::Geometry_list *gv= NULL, *gvr= NULL;

  if (gv1->size() > gv2->size())
  {
    gv= gv2;
    gvr= gv1;
  }
  else
  {
    gv= gv1;
    gvr= gv2;
  }

  Rtree_index rtree;
  make_rtree(*gvr, &rtree);

  Rtree_result rtree_result;
  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv->begin();
       i != gv->end(); ++i)
  {
    tres= 0;

    BG_box box;
    make_bg_box(*i, &box);
    rtree_result.clear();
    rtree.query(bgi::intersects(box), std::back_inserter(rtree_result));

    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, (*gvr)[j->second], pbgdone, spatial_rel, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error)
      {
        *pbgdone= false;
        return error_int();
      }

      if (!*pbgdone || null_value)
        return tres;

      /*
        If a pair of geometry intersect or don't disjoint, the two
        geometry collections intersect or don't disjoint, in both cases the
        check is completed.
       */
      if ((spatial_rel == SP_INTERSECTS_FUNC && tres) ||
          (spatial_rel == SP_DISJOINT_FUNC && !tres))
      {
        *pbgdone= true;
        return tres;
      }
    }
  }

  /*
    When we arrive here, the disjoint check must have succeeded and
    intersects check must have failed, otherwise control would
    have gone out of this function.

    The reason we can derive the relation check result is that if
    any two geometries from the two collections intersect, the two
    geometry collections intersect; and disjoint is true
    only when any(and every) combination of geometries from
    the two collections are disjoint.

    tres can be either true or false for DISJOINT check because the inner
    loop may never executed and tres woule be false.
   */
  DBUG_ASSERT(spatial_rel == SP_DISJOINT_FUNC ||
              (!tres && spatial_rel == SP_INTERSECTS_FUNC));
  *pbgdone= true;
  return tres;
}


/**
  Geometry collection relation checks for within and equals(half) checks.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_within(const typename BG_geometry_collection::
                       Geometry_list *gv1,
                       const typename BG_geometry_collection::
                       Geometry_list *gv2,
                       bool *pbgdone)
{
  int tres= 0;

  *pbgdone= false;
  DBUG_ASSERT(spatial_rel == SP_WITHIN_FUNC || spatial_rel == SP_EQUALS_FUNC);

  // Within isn't symetric so we have to always build rtree tndex on gv2.
  Rtree_index rtree;
  make_rtree(*gv2, &rtree);
  Rtree_result rtree_result;

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv1->begin();
       i != gv1->end(); ++i)
  {
    bool innerOK= false;
    tres= 0;
    /*
      Why it works to scan rtree index for within check? Because of the below
      conclusions.

      1. g1 within g2 => MBR(g1) within MBR(g2)
      Proof:
      Suppose MBR(g1) not within MBR(g2), then there exists a point P in g1
      such that either P.x not in MBR(g2)'s horizontal range, or P.y not in
      MBR(g2)'s vertical range. Since both ranges are limits of g2 too,
      that means P isn't in g2. Similarly we can have below conclusion for
      contains.

      2. g1 contains g2 => MBR(g1) contains MBR(g2)

      That is to say, MBR(g1) within/contains MBR(g2) is the necessary but not
      sufficient condition for g1 within/contains g2. All possible final result
      are in the ones returned by the rtree query.
     */

    BG_box box;
    make_bg_box(*i, &box);
    rtree_result.clear();
    rtree.query(bgi::covers(box), std::back_inserter(rtree_result));

    /*
      Above theory makes sure all results are in rtree_result, the logic
      here is sufficient when rtree_result is empty.
    */
    for (Rtree_result::iterator j= rtree_result.begin();
         j != rtree_result.end(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, (*gv2)[j->second], pbgdone, spatial_rel, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error || null_value)
      {
        *pbgdone= false;
        return error_int();
      }

      if (!*pbgdone)
        return tres;

      /*
        We've found a geometry j in gv2 so that current geometry element i
        in gv1 is within j, or i is equal to j. This means i in gv1
        passes the test, proceed to next geometry in gv1.
       */
      if ((spatial_rel == SP_WITHIN_FUNC ||
           spatial_rel == SP_EQUALS_FUNC) && tres)
      {
        innerOK= true;
        break;
      }
    }

    /*
      For within and equals check, if we can't find a geometry j in gv2
      so that current geometry element i in gv1 is with j or i is equal to j,
      gv1 is not within or equal to gv2.
     */
    if (!innerOK)
    {
      *pbgdone= true;
      DBUG_ASSERT(tres == 0);
      return tres;
    }
  }

  /*
    When we arrive here, within or equals checks must have
    succeeded, otherwise control would go out of this function.
    The reason we can derive the relation check result is that
    within and equals are true only when any(and every) combination of
    geometries from the two collections are true for the relation check.
   */
  DBUG_ASSERT(tres);
  *pbgdone= true;

  return tres;
}

/**
  Geometry collection equality check.
  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_equals_check(const typename BG_geometry_collection::Geometry_list *gv1,
                    const typename BG_geometry_collection::Geometry_list *gv2,
                    bool *pbgdone)
{
  int tres= 0, num_try= 0;
  *pbgdone= false;
  DBUG_ASSERT(spatial_rel == SP_EQUALS_FUNC);

  do
  {
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2, pbgdone);
    if (!tres || !*pbgdone || null_value)
      return tres;
    /*
      Two sets A and B are equal means A is a subset of B and B is a
      subset of A. Thus we need to check twice, each successful check
      means half truth. Switch gv1 and gv2 for 2nd check.
     */
    std::swap(gv1, gv2);
    num_try++;
  }
  while (num_try < 2);

  return tres;
}


/**
  Wraps and dispatches type specific BG function calls according to operation
  type and both operands' types.

  We want to isolate boost header file inclusion only inside this file, so we
  can't put this class declaration in any header file. And we want to make the
  methods static since no state is needed here.

  @tparam Geom_types Geometry types definitions.
*/
template<typename Geom_types>
class BG_wrap {
public:

  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  // For abbrievation.
  typedef Item_func_spatial_rel Ifsr;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  static int point_within_geometry(Geometry *g1, Geometry *g2,
                                   bool *pbgdone, my_bool *pnull_value);

  static int multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value);

  static int multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value);

  static int point_disjoint_geometry(Geometry *g1, Geometry *g2,
                                     bool *pbgdone, my_bool *pnull_value);
  static int multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value);

  static int linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value);
  static int multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                               bool *pbgdone,
                                               my_bool *pnull_value);
  static int polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                       bool *pbgdone, my_bool *pnull_value);
  static int multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int point_intersects_geometry(Geometry *g1, Geometry *g2,
                                       bool *pbgdone, my_bool *pnull_value);
  static int multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                                 bool *pbgdone,
                                                 my_bool *pnull_value);
  static int polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value);
  static int multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                              bool *pbgdone,
                                              my_bool *pnull_value);
  static int multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value);
  static int multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
};// bg_wrapper


/*
  Call a BG function with specified types of operands. We have to create
  geo1 and geo2 because operands g1 and g2 are created without their WKB data
  parsed, so not suitable for BG to use. geo1 will share the same copy of WKB
  data with g1, also true for geo2.
 */
#define BGCALL(res, bgfunc, GeoType1, g1, GeoType2, g2, pnullval) do {  \
  const void *pg1= g1->normalize_ring_order();                          \
  const void *pg2= g2->normalize_ring_order();                          \
  if (pg1 != NULL && pg2 != NULL)                                       \
  {                                                                     \
    GeoType1 geo1(pg1, g1->get_data_size(), g1->get_flags(),            \
                  g1->get_srid());                                      \
    GeoType2 geo2(pg2, g2->get_data_size(), g2->get_flags(),            \
                  g2->get_srid());                                      \
    res= boost::geometry::bgfunc(geo1, geo2);                           \
  }                                                                     \
  else                                                                  \
  {                                                                     \
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_" #bgfunc);               \
    (*(pnullval))= 1;                                                   \
  }                                                                     \
} while (0)


/**
  Dispatcher for 'point WITHIN xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::point_within_geometry(Geometry *g1, Geometry *g2,
                                               bool *pbgdone,
                                               my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  if (gt2 == Geometry::wkb_polygon)
  {
    BGCALL(result, within, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipolygon)
  {
    BGCALL(result, within, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_point)
  {
    BGCALL(result, equals, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipoint)
  {
    Multipoint mpts(g2->get_data_ptr(),
                    g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point pt(g1->get_data_ptr(),
             g1->get_data_size(), g1->get_flags(), g1->get_srid());

    Point_set ptset(mpts.begin(), mpts.end());
    result= ((ptset.find(pt) != ptset.end()));
    *pbgdone= true;
  }
  return result;
}


/**
  Dispatcher for 'multipoint WITHIN xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                                    bool *pbgdone,
                                                    my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

  *pbgdone= false;

  Multipoint mpts(g1->get_data_ptr(), g1->get_data_size(),
                  g1->get_flags(), g1->get_srid());
  if (gt2 == Geometry::wkb_polygon)
  {
    data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_within");
      *pnull_value= true;
      return result;
    }

    Polygon plg(data_ptr, g2->get_data_size(),
                g2->get_flags(), g2->get_srid());

    for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
    {
      result= boost::geometry::within(*i, plg);
      if (result == 0)
        break;
    }
    *pbgdone= true;

  }
  else if (gt2 == Geometry::wkb_multipolygon)
  {
    data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      *pnull_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_within");
      return result;
    }

    Multipolygon mplg(data_ptr, g2->get_data_size(),
                      g2->get_flags(), g2->get_srid());
    for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
    {
      result= boost::geometry::within(*i, mplg);
      if (result == 0)
        break;
    }
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_point)
  {
    /* There may be duplicate Points, thus use a set to make them unique*/
    Point_set ptset1(mpts.begin(), mpts.end());
    Point pt(g2->get_data_ptr(),
             g2->get_data_size(), g2->get_flags(), g2->get_srid());
    result= ((ptset1.size() == 1) &&
             boost::geometry::equals(*ptset1.begin(), pt));
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipoint)
  {
    /* There may be duplicate Points, thus use a set to make them unique*/
    Point_set ptset1(mpts.begin(), mpts.end());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point_set ptset2(mpts2.begin(), mpts2.end());
    Point_vector respts;
    TYPENAME Point_vector::iterator endpos;
    respts.resize(std::max(ptset1.size(), ptset2.size()));
    endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                  ptset2.begin(), ptset2.end(),
                                  respts.begin(), bgpt_lt());
    result= (ptset1.size() == static_cast<size_t>(endpos - respts.begin()));
    *pbgdone= true;
  }
  return result;
}


/**
  Dispatcher for 'multipoint EQUALS xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                                    bool *pbgdone,
                                                    my_bool *pnull_value)
{
  *pbgdone= false;
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    result= Ifsr::equals_check<Geom_types>(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());

      Point_set ptset1(mpts1.begin(), mpts1.end());
      Point_set ptset2(mpts2.begin(), mpts2.end());
      result= (ptset1.size() == ptset2.size() &&
               std::equal(ptset1.begin(), ptset1.end(),
                          ptset2.begin(), bgpt_eq()));
    }
    break;
  default:
    result= 0;
    break;
  }
  *pbgdone= true;
  return result;
}


/**
  Dispatcher for 'multipoint disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                             bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    result= point_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      Point_set ptset1(mpts1.begin(), mpts1.end());
      Point_set ptset2(mpts2.begin(), mpts2.end());
      Point_vector respts;
      TYPENAME Point_vector::iterator endpos;
      size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size();

      respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);
      endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                    ptset2.begin(), ptset2.end(),
                                    respts.begin(), bgpt_lt());
      result= (endpos == respts.begin());
      *pbgdone= true;
    }
    break;
  case Geometry::wkb_polygon:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Polygon plg(data_ptr, g2->get_data_size(),
                  g2->get_flags(), g2->get_srid());

      for (TYPENAME Multipoint::iterator i= mpts1.begin();
           i != mpts1.end(); ++i)
      {
        result= boost::geometry::disjoint(*i, plg);

        if (!result)
          break;
      }

      *pbgdone= true;
    }
    break;
  case Geometry::wkb_multipolygon:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Multipolygon mplg(data_ptr, g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());

      for (TYPENAME Multipoint::iterator i= mpts1.begin();
           i != mpts1.end(); ++i)
      {
        result= boost::geometry::disjoint(*i, mplg);

        if (!result)
          break;
      }

      *pbgdone= true;
    }
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'linestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                             bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
  {
    BGCALL(result, disjoint, Linestring, g1, Linestring, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    Multilinestring mls(g2->get_data_ptr(), g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());
    Linestring ls(g1->get_data_ptr(),
                  g1->get_data_size(), g1->get_flags(), g1->get_srid());

    for (TYPENAME Multilinestring::iterator i= mls.begin();
         i != mls.end(); ++i)
    {
      result= boost::geometry::disjoint(ls, *i);

      if (!result)
        break;
    }
    *pbgdone= true;

  }
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'multilinestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                  bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g2, g1, pbgdone, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    Multilinestring mls1(g1->get_data_ptr(), g1->get_data_size(),
                         g1->get_flags(), g1->get_srid());
    Multilinestring mls2(g2->get_data_ptr(), g2->get_data_size(),
                         g2->get_flags(), g2->get_srid());

    for (TYPENAME Multilinestring::iterator i= mls1.begin();
         i != mls1.end(); ++i)
    {
      for (TYPENAME Multilinestring::iterator j= mls2.begin();
           j != mls2.end(); ++j)
      {
        result= boost::geometry::disjoint(*i, *j);
        if (!result)
          break;
      }

      if (!result)
        break;
    }

    *pbgdone= true;
  }
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'point disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_disjoint_geometry(Geometry *g1, Geometry *g2,
                        bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts(g2->get_data_ptr(),
                      g2->get_data_size(), g2->get_flags(), g2->get_srid());
      Point pt(g1->get_data_ptr(),
               g1->get_data_size(), g1->get_flags(), g1->get_srid());

      Point_set ptset(mpts.begin(), mpts.end());
      result= (ptset.find(pt) == ptset.end());
      *pbgdone= true;
    }
    break;
  default:
    *pbgdone= false;
    break;
  }
  return result;
}


/**
  Dispatcher for 'polygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Polygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Polygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Polygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    *pbgdone= false;
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipolygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Multipolygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Multipolygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Multipolygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    *pbgdone= false;
    break;
  }

  return result;
}


/**
  Dispatcher for 'point intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_intersects_geometry(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !point_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipoint intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch(gt2)
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    result= !multipoint_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'linestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  if (gt2 == Geometry::wkb_linestring)
  {
    BGCALL(result, intersects, Linestring, g1, Linestring, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    result= !linestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
  }

  return result;
}


/**
  Dispatcher for 'multilinestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                    bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring ||
      gt2 == Geometry::wkb_multilinestring)
    result= (!BG_wrap<Geom_types>::
             multilinestring_disjoint_geometry(g1, g2,
                                               pbgdone, pnull_value) ? 1 : 0);
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'polygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Polygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Polygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Polygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipolygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                 bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Multipolygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Multipolygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Multipolygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipoint crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    {
      bool isdone= false, has_in= false, has_out= false;
      int res= 0;

      Multipoint mpts(g1->get_data_ptr(),
                      g1->get_data_size(), g1->get_flags(), g1->get_srid());
      /*
        According to OGC's definition to crosses, if some Points of
        g1 is in g2 and some are not, g1 crosses g2, otherwise not.
       */
      for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end() &&
           !(has_in && has_out); ++i)
      {
        res= point_disjoint_geometry(&(*i), g2, &isdone, pnull_value);

        if (isdone && !*pnull_value)
        {
          if (!res)
            has_in= true;
          else
            has_out= true;
        }
        else
        {
          *pbgdone= false;
          return 0;
        }
      }

      *pbgdone= true;

      if (has_in && has_out)
        result= 1;
      else
        result= 0;
    }
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipoint crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;

  *pbgdone= false;

  Multipoint mpts1(g1->get_data_ptr(),
                   g1->get_data_size(), g1->get_flags(), g1->get_srid());
  Multipoint mpts2(g2->get_data_ptr(),
                   g2->get_data_size(), g2->get_flags(), g2->get_srid());
  Point_set ptset1, ptset2;

  ptset1.insert(mpts1.begin(), mpts1.end());
  ptset2.insert(mpts2.begin(), mpts2.end());

  // They overlap if they intersect and also each has some points that the other
  // one doesn't have.
  Point_vector respts;
  TYPENAME Point_vector::iterator endpos;
  size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size(), resptssz;

  respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);
  endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                ptset2.begin(), ptset2.end(),
                                respts.begin(), bgpt_lt());
  resptssz= endpos - respts.begin();
  if (resptssz > 0 && resptssz < ptset1.size() &&
      resptssz < ptset2.size())
    result= 1;
  else
    result= 0;

  *pbgdone= true;

  return result;
}


/**
  Do within relation check of two geometries.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::within_check(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  gt1= g1->get_type();

  if (gt1 == Geometry::wkb_point)
    result= BG_wrap<Geom_types>::point_within_geometry(g1, g2,
                                                       pbgdone, pnull_value);
  else if (gt1 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_within_geometry(g1, g2, pbgdone, pnull_value);
  /*
    Can't do above if gt1 is Linestring or Polygon, because g2 can be
    an concave Polygon.
    Note: need within(lstr, plgn), within(pnt, lstr), within(lstr, lstr),
    within(plgn, plgn), (lstr, multiplgn), (lstr, multilstr),
    (multilstr, multilstr), (multilstr, multiplgn), (multiplgn, multiplgn),
    (plgn, multiplgn).

    Note that we can't iterate geometries in multiplgn, multilstr one by one
    and use within(lstr, plgn)(plgn, plgn) to do within computation for them
    because it's possible for a lstr to be not in any member plgn but in the
    multiplgn.
   */
  return result;
}


/**
  Do equals relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::equals_check(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef std::set<Point, bgpt_lt> Point_set;

  *pbgdone= false;
  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  /*
    Only geometries of the same base type can be equal, any other
    combinations always result as false. This is different from all other types
    of geometry relation checks.
   */
  *pbgdone= true;
  if (gt1 == Geometry::wkb_point)
  {
    if (gt2 == Geometry::wkb_point)
      BGCALL(result, equals, Point, g1, Point, g2, pnull_value);
    else if (gt2 == Geometry::wkb_multipoint)
    {
      Point pt(g1->get_data_ptr(),
               g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts(g2->get_data_ptr(),
                      g2->get_data_size(), g2->get_flags(), g2->get_srid());

      Point_set ptset(mpts.begin(), mpts.end());

      result= (ptset.size() == 1 &&
               boost::geometry::equals(pt, *ptset.begin()));
    }
    else
      result= 0;
  }
  else if (gt1 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_equals_geometry(g1, g2, pbgdone, pnull_value);
  else if (gt1 == Geometry::wkb_linestring &&
           gt2 == Geometry::wkb_linestring)
    BGCALL(result, equals, Linestring, g1, Linestring, g2, pnull_value);
  else if ((gt1 == Geometry::wkb_linestring &&
            gt2 == Geometry::wkb_multilinestring) ||
           (gt2 == Geometry::wkb_linestring &&
            gt1 == Geometry::wkb_multilinestring) ||
           (gt2 == Geometry::wkb_multilinestring &&
            gt1 == Geometry::wkb_multilinestring))
  {
    *pbgdone= false;
    /*
      Note: can't handle this case simply like Multipoint&point above,
      because multiple line segments can form a longer linesegment equal
      to a single line segment.
     */
  }
  else if (gt1 == Geometry::wkb_polygon && gt2 == Geometry::wkb_polygon)
    BGCALL(result, equals, Polygon, g1, Polygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_polygon && gt2 ==Geometry::wkb_multipolygon)
    BGCALL(result, equals, Polygon, g1, Multipolygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipolygon && gt2 ==Geometry::wkb_polygon)
    BGCALL(result, equals, Multipolygon, g1, Polygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipolygon &&
           gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, equals, Multipolygon, g1, Multipolygon, g2, pnull_value);
  else
    result= 0;
  return result;
}


/**
  Do disjoint relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::disjoint_check(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  *pbgdone= false;
  gt1= g1->get_type();

  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    break;
  }

  /*
    Note: need disjoint(point, Linestring) and disjoint(linestring, Polygon)
   */
  return result;
}


/**
  Do interesects relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::intersects_check(Geometry *g1, Geometry *g2,
                                            bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  *pbgdone= false;
  int result= 0;

  gt1= g1->get_type();
  /*
    According to OGC SFA, intersects is identical to !disjoint, but
    boost geometry has functions to compute intersects, so we still call
    them.
   */
  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    *pbgdone= false;
    break;
  }
  /*
    Note: need intersects(pnt, lstr), (lstr, plgn)
   */
  return result;
}


/**
  Do overlaps relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::overlaps_check(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (g1->feature_dimension() != g2->feature_dimension())
  {
    *pbgdone= true;
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }

  if (gt1 == Geometry::wkb_point || gt2 == Geometry::wkb_point)
  {
    *pbgdone= true;
    result= 0;
  }

  if (gt1 == Geometry::wkb_multipoint && gt2 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_overlaps_multipoint(g1, g2, pbgdone, pnull_value);

  /*
    Note: Need overlaps([m]ls, [m]ls), overlaps([m]plgn, [m]plgn).
   */
  return result;
}


/**
  Do touches relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::touches_check(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipolygon Multipolygon;

  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if ((gt1 == Geometry::wkb_point || gt1 == Geometry::wkb_multipoint) &&
      (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint))
  {
    *pbgdone= true;
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }
  /*
    Touches is symetric, and one argument is allowed to be a Point/multipoint.
   */
  switch (gt1)
  {
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, touches, Polygon, g1, Polygon, g2, pnull_value);
      *pbgdone= true;
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, touches, Polygon, g1, Multipolygon, g2, pnull_value);
      *pbgdone= true;
      break;
    default:
      *pbgdone= false;
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, touches, Multipolygon, g1, Polygon, g2, pnull_value);
      *pbgdone= true;
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, touches, Multipolygon, g1, Multipolygon, g2, pnull_value);
      *pbgdone= true;
      break;
    default:
      *pbgdone= false;
      break;
    }
    break;
  default:
    *pbgdone= false;
    break;
  }
  /*
    Note: need touches(pnt, lstr), (pnt, plgn), (lstr, lstr), (lstr, plgn).
    for multi geometry, can iterate geos in it and compute for
    each geo separately.
   */
  return result;
}


/**
  Do crosses relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::crosses_check(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (gt1 == Geometry::wkb_polygon || gt2 == Geometry::wkb_point ||
      (gt1 == Geometry::wkb_multipolygon || gt2 == Geometry::wkb_multipoint))
  {
    *pbgdone= true;
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }

  if (gt1 == Geometry::wkb_point)
  {
    *pbgdone= true;
    result= 0;
    return result;
  }

  switch (gt1)
  {
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_crosses_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  /*
    Note: needs crosses([m]ls, [m]ls), crosses([m]ls, [m]plgn).
   */
  return result;
}


/**
  Entry point to call Boost Geometry functions to check geometry relations.
  This function is static so that it can be called without the
  Item_func_spatial_rel object --- we do so to implement a few functionality
  for other classes in this file, e.g. Item_func_spatial_operation::val_str.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pisdone Returns whether the specified relation check operation is
        performed by BG. For now BG doesn't support many type combinatioons
        for each type of relation check. If isdone returns false, old GIS
        algorithms will be called to do the check.
  @param relchk_type The type of relation check.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::bg_geo_relation_check(Geometry *g1, Geometry *g2,
                                                 bool *pisdone,
                                                 Functype relchk_type,
                                                 my_bool *pnull_value)
{
  int result= 0;
  bool bgdone= false;

  typedef BG_models<Coord_type, Coordsys> Geom_types;

  *pisdone= false;
  /*
    Dispatch calls to all specific type combinations for each relation check
    function.

    Boost.Geometry doesn't have dynamic polymorphism,
    e.g. the above Point, Linestring, and Polygon templates don't have a common
    base class template, so we have to dispatch by types.

    The checking functions should set bgdone to true if the relation check is
    performed, they should also set null_value to true if there is error.
   */

  switch (relchk_type) {
  case SP_CONTAINS_FUNC:
    result= within_check<Geom_types>(g2, g1, &bgdone, pnull_value);
    break;
  case SP_WITHIN_FUNC:
    result= within_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_EQUALS_FUNC:
    result= equals_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_DISJOINT_FUNC:
    result= disjoint_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_INTERSECTS_FUNC:
    result= intersects_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_OVERLAPS_FUNC:
    result= overlaps_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_TOUCHES_FUNC:
    result= touches_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_CROSSES_FUNC:
    result= crosses_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }

  *pisdone= bgdone;
  return result;
}

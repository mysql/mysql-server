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
        (g1, g2, spatial_rel, &had_error);
    }
    else
      tres= geocol_relation_check<double, bgcs::cartesian>(g1, g2);
  }
  CATCH_ALL(func_name(), { had_except= true; })

  if (had_except || had_error || null_value)
    DBUG_RETURN(error_int());

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
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::geocol_relation_check(Geometry *g1, Geometry *g2)
{
  String gcbuf;
  Geometry *tmpg= NULL;
  int tres= 0;
  const typename BG_geometry_collection::Geometry_list *gv1= NULL, *gv2= NULL;
  BG_geometry_collection bggc1, bggc2;
  bool empty1= is_empty_geocollection(g1);
  bool empty2= is_empty_geocollection(g2);

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
    return tres;
  }

  if (spatial_rel == SP_CONTAINS_FUNC)
  {
    tmpg= g2;
    g2= g1;
    g1= tmpg;
    spatial_rel= SP_WITHIN_FUNC;
  }

  bggc1.fill(g1);
  bggc2.fill(g2);

  /*
    When checking GC1 within GC2, we want GC1 to be disintegrated pieces
    rather than merging its components to larger pieces, because a
    multi-geometry of GC1 may consist of multiple components which are within
    different components of GC2, but if merged, it would not be within any
    component of GC2.
   */
  if (spatial_rel != SP_WITHIN_FUNC)
    bggc1.merge_components<Coord_type, Coordsys>(&null_value);
  if (null_value)
    return tres;
  bggc2.merge_components<Coord_type, Coordsys>(&null_value);
  if (null_value)
    return tres;

  gv1= &(bggc1.get_geometries());
  gv2= &(bggc2.get_geometries());

  if (gv1->size() == 0 || gv2->size() == 0)
  {
    null_value= true;
    return tres;
  }
  else if (gv1->size() == 1 && gv2->size() == 1)
  {
    tres= bg_geo_relation_check<Coord_type, Coordsys>
      (*(gv1->begin()), *(gv2->begin()), spatial_rel, &null_value);
    return tres;
  }

  if (spatial_rel == SP_OVERLAPS_FUNC ||
      spatial_rel == SP_CROSSES_FUNC || spatial_rel == SP_TOUCHES_FUNC)
  {
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }

  if (spatial_rel == SP_DISJOINT_FUNC || spatial_rel == SP_INTERSECTS_FUNC)
    tres= geocol_relcheck_intersect_disjoint<Coord_type, Coordsys>(gv1, gv2);
  else if (spatial_rel == SP_WITHIN_FUNC)
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2);
  else if (spatial_rel == SP_EQUALS_FUNC)
    tres= geocol_equals_check<Coord_type, Coordsys>(gv1, gv2);
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
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_intersect_disjoint(const typename BG_geometry_collection::
                                   Geometry_list *gv1,
                                   const typename BG_geometry_collection::
                                   Geometry_list *gv2)
{
  int tres= 0;

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

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv->begin();
       i != gv->end(); ++i)
  {
    tres= 0;

    BG_box box;
    make_bg_box(*i, &box);
    for (Rtree_index::const_query_iterator
         j= rtree.qbegin(bgi::intersects(box));
         j != rtree.qend(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, (*gvr)[j->second], spatial_rel, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error)
        return error_int();

      if (null_value)
        return tres;

      /*
        If a pair of geometry intersect or don't disjoint, the two
        geometry collections intersect or don't disjoint, in both cases the
        check is completed.
       */
      if ((spatial_rel == SP_INTERSECTS_FUNC && tres) ||
          (spatial_rel == SP_DISJOINT_FUNC && !tres))
        return tres;
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
  return tres;
}


/**
  Multipoint need special handling because for a multipoint MP to be
  within geometry G, only one point in MP has to be 'within' G,
  the rest only need to intersect G.

  @param pmpts the multipoint to check.
  @param gv2 the geometry collection's component list.
  @param prtree the rtree index built on gv2. We can't expose the
  Rtree_index type in item_geofunc.h so have to use the generic void* type.
  This function is called where an rtree index on gv2 is already built so
  we want to pass it in to avoid unnecessarily build the same one again.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
multipoint_within_geometry_collection(Gis_multi_point *pmpts,
                                      const typename BG_geometry_collection::
                                      Geometry_list *gv2,
                                      const void *prtree)
{
  int has_inner= 0;
  int tres= 0;
  my_bool had_error= false;

  Rtree_index &rtree= *((Rtree_index *)prtree);

  TYPENAME BG_models<Coord_type, Coordsys>::
    Multipoint mpts(pmpts->get_data_ptr(), pmpts->get_data_size(),
                    pmpts->get_flags(), pmpts->get_srid());

  for (TYPENAME BG_models<Coord_type, Coordsys>::Multipoint::iterator
       k= mpts.begin(); k != mpts.end(); ++k)
  {
    bool already_in= false;
    BG_box box;
    make_bg_box(&(*k), &box);

    /*
      Search for geometries in gv2 that may intersect *k point using the
      rtree index.
      All geometries that possibly intersect *k point are given by the
      rtree iteration below.
    */
    for (Rtree_index::const_query_iterator
         j= rtree.qbegin(bgi::intersects(box));
         j != rtree.qend(); ++j)
    {
      /*
        If we don't have a point in mpts that's within a component of gv2 yet,
        check whether *k is within *j.
        If *k is within *j, it's already in the geometry collection gv2,
        so no need for more checks for the point *k, get out of the iteration.
      */
      if (!has_inner)
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (&(*k), (*gv2)[j->second], SP_WITHIN_FUNC, &had_error);
        if (had_error || null_value)
          return error_int();
        if ((has_inner= tres))
        {
          already_in= true;
          break;
        }
      }

      /*
        If we already have a point within gv2, OR if *k is checked above to
        be not within *j, check whether *k intersects *j.
        *k has to intersect one of the components in this loop, otherwise *k
        is out of gv2.
       */
      tres= bg_geo_relation_check<Coord_type, Coordsys>
        (&(*k), (*gv2)[j->second], SP_INTERSECTS_FUNC, &had_error);
      if (had_error || null_value)
        return error_int();

      if (tres)
      {
        already_in= true;
        /*
          It's likely that *k is within another geometry, so only stop the
          iteration if we already have a point that's within gv2,
          in order not to miss the potential geometry containing *k.
        */
        if (has_inner)
          break;
      }
    }

    /*
      The *k point isn't within or intersects any geometry compoennt of gv2,
      so mpts isn't within geom.
    */
    if (!already_in)
      return 0;
  }

  /*
    All points in mpts at least intersects geom, so the result is determined
    by whether there is at least one point in mpts that's within geom.
  */
  return has_inner;
}


/**
  Geometry collection relation checks for within and equals(half) checks.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_within(const typename BG_geometry_collection::
                       Geometry_list *gv1,
                       const typename BG_geometry_collection::
                       Geometry_list *gv2)
{
  int tres= 0;

  /*
    When this function is called by geocol_equals_check,this is true:
    spatial_rel == SP_EQUALS_FUNC
    But even in this case, in this function we still want to check each
    component of gv1 is within gv2, so in this function we always assume
    with check and and use SP_WITHIN_FUNC.
  */
  DBUG_ASSERT(spatial_rel == SP_WITHIN_FUNC || spatial_rel == SP_EQUALS_FUNC);

  // Within isn't symetric so we have to always build rtree tndex on gv2.
  Rtree_index rtree;
  make_rtree(*gv2, &rtree);

  BG_geometry_collection bggc;
  bool no_fill= true;

  /*
    We have to break any multi-geometry into its components before the within
    check, because the components of some multi-geometry MG in gv1 may be in
    different geometries of gv2, and in all the MG is still in gv2.
    Without the disintegration, MG would be seen as not within gv2.

    Multipoint need special handling because for a multipoint MP to be within
    geometry G, only one point in MP has to be 'within' G, the rest only need
    to intersect G.
  */
  for (size_t i= 0; i < gv1->size(); i++)
  {
    Geometry::wkbType gtype= (*gv1)[i]->get_type();
    if (gtype == Geometry::wkb_multipolygon ||
        gtype == Geometry::wkb_multilinestring)
    {
      if (no_fill)
      {
        for (size_t j= 0; j < i; j++)
          bggc.fill((*gv1)[j]);
        no_fill= false;
      }

      bggc.fill((*gv1)[i], true/* break multi-geometry. */);
    }
    else if (!no_fill)
      bggc.fill((*gv1)[i]);
  }

  if (!no_fill)
    gv1= &(bggc.get_geometries());

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv1->begin();
       i != gv1->end(); ++i)
  {
    bool innerOK= false;
    tres= 0;

    if ((*i)->get_type() == Geometry::wkb_multipoint)
    {
      Gis_multi_point *mpts= static_cast<Gis_multi_point *>(*i);
      tres= multipoint_within_geometry_collection<Coord_type, Coordsys>
        (mpts, gv2, &rtree);
      if (null_value)
        return error_int();
      if (tres)
        continue;
      else
        return tres;
    }

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

    /*
      Above theory makes sure all results are in rtree search result, the logic
      here is sufficient when the result is empty.
    */
    for (Rtree_index::const_query_iterator
         j= rtree.qbegin(bgi::covers(box));
         j != rtree.qend(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, (*gv2)[j->second], SP_WITHIN_FUNC, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error || null_value)
        return error_int();

      /*
        We've found a geometry j in gv2 so that current geometry element i
        in gv1 is within j, or i is equal to j. This means i in gv1
        passes the test, proceed to next geometry in gv1.
       */
      if (tres)
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
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_equals_check(const typename BG_geometry_collection::Geometry_list *gv1,
                    const typename BG_geometry_collection::Geometry_list *gv2)
{
  int tres= 0, num_try= 0;
  DBUG_ASSERT(spatial_rel == SP_EQUALS_FUNC);

  do
  {
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2);
    if (!tres || null_value)
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
                                   my_bool *pnull_value);

  static int multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                        my_bool *pnull_value);

  static int linestring_within_geometry(Geometry *g1, Geometry *g2,
                                        my_bool *pnull_value);
  static int multilinestring_within_geometry(Geometry *g1, Geometry *g2,
                                             my_bool *pnull_value);
  static int polygon_within_geometry(Geometry *g1, Geometry *g2,
                                     my_bool *pnull_value);
  static int multipolygon_within_geometry(Geometry *g1, Geometry *g2,
                                          my_bool *pnull_value);

  static int multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                        my_bool *pnull_value);

  static int point_disjoint_geometry(Geometry *g1, Geometry *g2,
                                     my_bool *pnull_value);
  static int multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          my_bool *pnull_value);

  static int linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          my_bool *pnull_value);
  static int multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                               my_bool *pnull_value);
  static int polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                       my_bool *pnull_value);
  static int multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                            my_bool *pnull_value);
  static int point_intersects_geometry(Geometry *g1, Geometry *g2,
                                       my_bool *pnull_value);
  static int multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                                            my_bool *pnull_value);
  static int linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                            my_bool *pnull_value);
  static int multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                                 my_bool *pnull_value);
  static int polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value);
  static int multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                              my_bool *pnull_value);
  static int linestring_crosses_geometry(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value);
  static int multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value);
  static int multilinestring_crosses_geometry(Geometry *g1, Geometry *g2,
                                              my_bool *pnull_value);
  static int multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                                            my_bool *pnull_value);
  static int point_touches_geometry(Geometry *g1, Geometry *g2,
                                    my_bool *pnull_value);
  static int multipoint_touches_geometry(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value);
  static int linestring_touches_geometry(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value);
  static int multilinestring_touches_polygon(Geometry *g1, Geometry *g2,
                                             my_bool *pnull_value);
  static int multilinestring_touches_geometry(Geometry *g1, Geometry *g2,
                                              my_bool *pnull_value);
  static int polygon_touches_geometry(Geometry *g1, Geometry *g2,
                                      my_bool *pnull_value);
  static int multipolygon_touches_geometry(Geometry *g1, Geometry *g2,
                                           my_bool *pnull_value);

private:
  template<typename Geom_type>
  static int multipoint_disjoint_geometry_internal(const Multipoint &mpts1,
                                                   const Geom_type &geom);
  template<typename Geom_type>
  static int multipoint_disjoint_multi_geometry(const Multipoint &mpts,
                                                const Geom_type &geom);
  template <typename GeomType>
  static int multipoint_within_geometry_internal(const Multipoint &mpts,
                                                 const GeomType &geom);
  static int multipoint_within_multipolygon(const Multipoint &mpts,
                                            const Multipolygon &mplgn);
};// BG_wrap


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
  @param g1 First Geometry operand, a point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::point_within_geometry(Geometry *g1, Geometry *g2,
                                               my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, within, Point, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, within, Point, g1, Multipolygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point)
    BGCALL(result, within, Point, g1, Point, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipoint)
    BGCALL(result, within, Point, g1, Multipoint, g2, pnull_value);
  else if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, within, Point, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, within, Point, g1, Multilinestring, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


/**
  Dispatcher for 'multipoint WITHIN xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                                    my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

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

    result= multipoint_within_geometry_internal(mpts, plg);
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

    /*
      One may want to build the rtree index on mpts when mpts has more
      components than mplg, but then one would have to track the points that
      are already known to be in one of mplg's polygons and avoid checking
      again (which may fail and cause false alarm) for other polygon components.
      Such maintenance brings extra cost and performance test prooves that
      it's not desirable.
      The containers tried for such maintenance including std::vector<bool>,
      std::set<array_index>, mpts[i].set_props().

      Also, even if the mplg has only one polygon, i.e. the worst case for
      building rtree index on mplg, the performance is still very very close to
      the linear search done in multipoint_within_geometry_internal.

      So always build index on mplg as below.
    */
    result= multipoint_within_multipolygon(mpts, mplg);
  }
  else if (gt2 == Geometry::wkb_point)
  {
    /* There may be duplicate Points, thus use a set to make them unique*/
    Point_set ptset1(mpts.begin(), mpts.end());
    Point pt(g2->get_data_ptr(),
             g2->get_data_size(), g2->get_flags(), g2->get_srid());
    result= ((ptset1.size() == 1) &&
             boost::geometry::equals(*ptset1.begin(), pt));
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
  }
  else if (gt2 == Geometry::wkb_linestring)
  {
    Linestring ls(g2->get_data_ptr(), g2->get_data_size(),
                  g2->get_flags(), g2->get_srid());
    result= multipoint_within_geometry_internal(mpts, ls);
  }
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    Multilinestring mls(g2->get_data_ptr(), g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());
    /*
      Here we can't separate linestrings of a multilinstring MLS to do within
      check one by one because if N (N > 1) linestrings share the same boundary
      point P, P may or may not be a boundary point of MLS, depending on N%2,
      if N is an even number P is an internal point of MLS, otherwise P is a
      boundary point of MLS.
    */
    result= multipoint_within_geometry_internal(mpts, mls);
  }
  else
    DBUG_ASSERT(false);

  return result;
}


template <typename Geom_types>
template <typename GeomType>
int BG_wrap<Geom_types>::
multipoint_within_geometry_internal(const Multipoint &mpts,
                                    const GeomType &geom)
{
  bool has_inner= false;

  for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
  {
    /*
      Checking for intersects is faster than within, so if there is at least
      one point within geom, only check that the rest points intersects geom.
     */
    if (!has_inner && (has_inner= boost::geometry::within(*i, geom)))
      continue;

    if (!boost::geometry::intersects(*i, geom))
      return 0;
  }

  return has_inner;
}


template <typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_within_multipolygon(const Multipoint &mpts,
                               const Multipolygon &mplgn)
{
  bool has_inner= false;

  Rtree_index rtree;
  make_rtree_bggeom(mplgn, &rtree);
  BG_box box;

  for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
  {
    bool already_in= false;
    // Search for polygons that may intersect *i point using the rtree index.
    boost::geometry::envelope(*i, box);
    Rtree_index::const_query_iterator j= rtree.qbegin(bgi::intersects(box));
    if (j == rtree.qend())
      return 0;
    /*
      All polygons that possibly intersect *i point are given by the
      rtree iteration below.
    */
    for (; j != rtree.qend(); ++j)
    {
      /*
        Checking for intersects is faster than within, so if there is at least
        one point within geom, only check that the rest points intersects geom.
      */
      const Polygon &plgn= mplgn[j->second];
      /*
        If we don't have a point in mpts that's within mplgn yet,
        check whether *i is within plgn.
        If *i is within plgn, it's already in the multipolygon, so no need
        for more checks.
      */
      if (!has_inner && (has_inner= boost::geometry::within(*i, plgn)))
      {
        already_in= true;
        break;
      }

      /*
        If we already have a point within mplgn, OR if *i is checked above to
        be not within plgn, check whether *i intersects plgn.
        *i has to intersect one of the components in this loop, otherwise *i
        is out of mplgn.
       */
      if (boost::geometry::intersects(*i, plgn))
      {
        already_in= true;
        /*
          It's likely that *i is within another plgn, so only stop the
          iteration if we already have a point that's within the multipolygon,
          in order not to miss the polygon containing *i.
        */
        if (has_inner)
          break;
      }
    }

    /*
      The *i point isn't within or intersects any polygon of mplgn,
      so mpts isn't within geom.
    */
    if (!already_in)
      return 0;
  }

  /*
    All points in mpts at least intersects geom, so the result is determined
    by whether there is at least one point in mpts that's within geom.
  */
  return has_inner;
}


template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_within_geometry(Geometry *g1, Geometry *g2,
                           my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, within, Linestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, within, Linestring, g1, Multipolygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint)
    return 0;
  else if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, within, Linestring, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, within, Linestring, g1, Multilinestring, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_within_geometry(Geometry *g1, Geometry *g2,
                                my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, within, Multilinestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, within, Multilinestring, g1, Multipolygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint)
    return 0;
  else if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, within, Multilinestring, g1,
           Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, within, Multilinestring, g1,
           Multilinestring, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_within_geometry(Geometry *g1, Geometry *g2,
                        my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, within, Polygon, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, within, Polygon, g1, Multipolygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint ||
           gt2 == Geometry::wkb_linestring ||
           gt2 == Geometry::wkb_multilinestring)
    return 0;
  else
    DBUG_ASSERT(false);

  return result;
}


template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_within_geometry(Geometry *g1, Geometry *g2,
                             my_bool *pnull_value)
{

  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, within, Multipolygon, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, within, Multipolygon, g1, Multipolygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint ||
           gt2 == Geometry::wkb_linestring ||
           gt2 == Geometry::wkb_multilinestring)
    return 0;
  else
    DBUG_ASSERT(false);

  return result;
}


/**
  Dispatcher for 'multipoint EQUALS xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                                    my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    result= Ifsr::equals_check<Geom_types>(g2, g1, pnull_value);
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
  return result;
}


/**
  Dispatcher for 'multipoint disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                             my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

  Multipoint mpts1(g1->get_data_ptr(),
                   g1->get_data_size(), g1->get_flags(), g1->get_srid());
  switch (gt2)
  {
  case Geometry::wkb_point:
    result= point_disjoint_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
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
    }
    break;
  case Geometry::wkb_polygon:
    {
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Polygon plg(data_ptr, g2->get_data_size(),
                  g2->get_flags(), g2->get_srid());
      result= multipoint_disjoint_geometry_internal(mpts1, plg);
    }
    break;
  case Geometry::wkb_multipolygon:
    {
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Multipolygon mplg(data_ptr, g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());
      result= multipoint_disjoint_multi_geometry(mpts1, mplg);
    }
    break;
  case Geometry::wkb_linestring:
    {
      Linestring ls(g2->get_data_ptr(), g2->get_data_size(),
                    g2->get_flags(), g2->get_srid());
      result= multipoint_disjoint_geometry_internal(mpts1, ls);
    }
    break;
  case Geometry::wkb_multilinestring:
    {
      Multilinestring mls(g2->get_data_ptr(), g2->get_data_size(),
                          g2->get_flags(), g2->get_srid());
      result= multipoint_disjoint_multi_geometry(mpts1, mls);
    }
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


template<typename Geom_types>
template<typename Geom_type>
int BG_wrap<Geom_types>::
multipoint_disjoint_geometry_internal(const Multipoint &mpts,
                                      const Geom_type &geom)
{
  for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
  {
    if (!boost::geometry::disjoint(*i, geom))
      return 0;
  }

  return 1;
}


template<typename Geom_types>
template<typename Geom_type>
int BG_wrap<Geom_types>::
multipoint_disjoint_multi_geometry(const Multipoint &mpts,
                                   const Geom_type &geom)
{
  Rtree_index rtree;

  // Choose the one with more components to build rtree index on, to get more
  // performance improvement.
  if (mpts.size() > geom.size())
  {
    make_rtree_bggeom(mpts, &rtree);
    for (TYPENAME Geom_type::iterator j= geom.begin(); j != geom.end(); ++j)
    {
      BG_box box;
      boost::geometry::envelope(*j, box);

      /*
        For each component *j in geom, find points in mpts who intersect
        with MBR(*j), such points are likely to intersect *j, the rest are
        for sure disjoint *j thus no need to check precisely.
      */
      for (Rtree_index::const_query_iterator
           i= rtree.qbegin(bgi::intersects(box));
           i != rtree.qend(); ++i)
      {
        /*
          If *i really intersect *j, we have the result as false;
          If no *i intersects *j, *j disjoint mpts.
          And if no *j intersect mpts, we can conclude that mpts disjoint geom.
        */
        if (!boost::geometry::disjoint(mpts[i->second], *j))
          return 0;
      }
    }
  }
  else
  {
    make_rtree_bggeom(geom, &rtree);
    for (TYPENAME Multipoint::iterator j= mpts.begin(); j != mpts.end(); ++j)
    {
      BG_box box;
      boost::geometry::envelope(*j, box);

      /*
        For each point *j in mpts, find components *i in geom such that
        MBR(*i) intersect *j, such *i are likely to intersect *j, the rest are
        for sure disjoint *j thus no need to check precisely.
      */
      for (Rtree_index::const_query_iterator
           i= rtree.qbegin(bgi::intersects(box));
           i != rtree.qend(); ++i)
      {
        /*
          If *i really intersect *j, we have the result as false;
          If no *i intersects *j, *j disjoint geom.
          And if no *j intersect geom, we can conclude that mpts disjoint geom.
        */
        if (!boost::geometry::disjoint(geom[i->second], *j))
          return 0;
      }
    }
  }

  return 1;
}


/**
  Dispatcher for 'linestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a linestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                             my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, disjoint, Linestring, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, disjoint, Linestring, g1, Multilinestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point)
    BGCALL(result, disjoint, Linestring, g1, Point, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipoint)
    result= multipoint_disjoint_geometry(g2, g1, pnull_value);
  else if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, disjoint, Linestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, disjoint, Linestring, g1, Multipolygon, g2, pnull_value);
  else
    DBUG_ASSERT(false);
  return result;
}


/**
  Dispatcher for 'multilinestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multilinestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                  my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g2, g1, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, disjoint, Multilinestring, g1, Multilinestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_point)
    BGCALL(result, disjoint, Multilinestring, g1, Point, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipoint)
    result= multipoint_disjoint_geometry(g2, g1, pnull_value);
  else if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, disjoint, Multilinestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, disjoint, Multilinestring, g1, Multipolygon, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


/**
  Dispatcher for 'point disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_disjoint_geometry(Geometry *g1, Geometry *g2,
                        my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Point, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Point, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Point, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts(g2->get_data_ptr(),
                      g2->get_data_size(), g2->get_flags(), g2->get_srid());
      Point pt(g1->get_data_ptr(),
               g1->get_data_size(), g1->get_flags(), g1->get_srid());

      Point_set ptset(mpts.begin(), mpts.end());
      result= (ptset.find(pt) == ptset.end());
    }
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, disjoint, Point, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, disjoint, Point, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'polygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a polygon.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                          my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Polygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Polygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Polygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, disjoint, Polygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, disjoint, Polygon, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipolygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipolygon.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                               my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Multipolygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Multipolygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Multipolygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, disjoint, Multipolygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, disjoint, Multipolygon, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'point intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_intersects_geometry(Geometry *g1, Geometry *g2,
                          my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Point, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
    result= !point_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Point, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Point, g1, Multipolygon, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipoint intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                               my_bool *pnull_value)
{
  return !multipoint_disjoint_geometry(g1, g2, pnull_value);
}


/**
  Dispatcher for 'linestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a linestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                               my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_point)
    BGCALL(result, intersects, Linestring, g1, Point, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipoint)
    result= multipoint_intersects_geometry(g2, g1, pnull_value);
  else if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, intersects, Linestring, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, intersects, Linestring, g1, Multilinestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, intersects, Linestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, intersects, Linestring, g1, Multipolygon, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


/**
  Dispatcher for 'multilinestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multilinestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                    my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_point)
    BGCALL(result, intersects, Multilinestring, g1, Point, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipoint)
    result= multipoint_intersects_geometry(g2, g1, pnull_value);
  else if (gt2 == Geometry::wkb_linestring)
    BGCALL(result, intersects, Multilinestring, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, intersects, Multilinestring, g1, Multilinestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_polygon)
    BGCALL(result, intersects, Multilinestring, g1, Polygon, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, intersects, Multilinestring, g1, Multipolygon, g2, pnull_value);
  else
    DBUG_ASSERT(false);

  return result;
}


/**
  Dispatcher for 'polygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a polygon.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                            my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Polygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Polygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Polygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, intersects, Polygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, intersects, Polygon, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipolygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipolygon.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                 my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Multipolygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Multipolygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Multipolygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, intersects, Multipolygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, intersects, Multipolygon, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'linestring crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a linestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_crosses_geometry(Geometry *g1, Geometry *g2,
                            my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_linestring:
    BGCALL(result, crosses, Linestring, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, crosses, Linestring, g1,
           Multilinestring, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, crosses, Linestring, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, crosses, Linestring, g1, Multipolygon, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'multilinestring crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multilinestring.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_crosses_geometry(Geometry *g1, Geometry *g2,
                                 my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_linestring:
    BGCALL(result, crosses, Multilinestring, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, crosses, Multilinestring, g1,
           Multilinestring, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, crosses, Multilinestring, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, crosses, Multilinestring, g1, Multipolygon, g2, pnull_value);
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
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                            my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    {
      bool has_in= false, has_out= false;
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
        if (!has_out)
        {
          res= point_disjoint_geometry(&(*i), g2, pnull_value);

          if (!*pnull_value)
          {
            has_out= res;
            if (has_out)
              continue;
          }
          else
            return 0;
        }

        if (!has_in)
        {
          res= point_within_geometry(&(*i), g2, pnull_value);
          if (!*pnull_value)
            has_in= res;
          else
            return 0;
        }
      }

      result= (has_in && has_out);
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
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                               my_bool *pnull_value)
{
  int result= 0;

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

  return result;
}


/**
  Dispatcher for 'multilinestring touches polygon'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multilinestring.
  @param g2 Second Geometry operand, a polygon.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_touches_polygon(Geometry *g1, Geometry *g2,
                                my_bool *pnull_value)
{

  const void *data_ptr= g2->normalize_ring_order();
  if (data_ptr == NULL)
  {
    *pnull_value= true;
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_touches");
    return 0;
  }

  Polygon plgn(data_ptr, g2->get_data_size(),
               g2->get_flags(), g2->get_srid());
  Multilinestring mls(g1->get_data_ptr(), g1->get_data_size(),
                      g1->get_flags(), g1->get_srid());

  Multipolygon mplgn;
  mplgn.push_back(plgn);

  int result= boost::geometry::touches(mls, mplgn);

  return result;
}


/**
  Dispatcher for 'point touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a point.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_touches_geometry(Geometry *g1, Geometry *g2,
                       my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_linestring:
    BGCALL(result, touches, Point, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, touches, Point, g1, Multilinestring, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, touches, Point, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, touches, Point, g1, Multipolygon, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipoint touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipoint.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_touches_geometry(Geometry *g1, Geometry *g2,
                            my_bool *pnull_value)
{
  int has_touches= 0;

  Multipoint mpts(g1->get_data_ptr(), g1->get_data_size(),
                  g1->get_flags(), g1->get_srid());
  for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
  {
    int ptg= point_touches_geometry(&(*i), g2, pnull_value);
    if (*pnull_value)
      return 0;
    if (ptg)
      has_touches= 1;
    else if (!point_disjoint_geometry(&(*i), g2, pnull_value))
      return 0;
  }

  return has_touches;
}


/**
  Dispatcher for 'linestring touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a linestring.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_touches_geometry(Geometry *g1, Geometry *g2,
                            my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, touches, Linestring, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_touches_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, touches, Linestring, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, touches, Linestring, g1, Multilinestring, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, touches, Linestring, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, touches, Linestring, g1, Multipolygon, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'multilinestring touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multilinestring.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_touches_geometry(Geometry *g1, Geometry *g2,
                                 my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, touches, Multilinestring, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_touches_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, touches, Multilinestring, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, touches, Multilinestring, g1, Multilinestring, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      multilinestring_touches_polygon(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, touches, Multilinestring, g1, Multipolygon, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'polygon touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a polygon.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_touches_geometry(Geometry *g1, Geometry *g2,
                         my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, touches, Polygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_touches_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, touches, Polygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, touches, Polygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, touches, Polygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_touches_polygon(g2, g1, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipolygon touches geometry'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a multipolygon.
  @param g2 Second Geometry operand, a geometry other than geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_touches_geometry(Geometry *g1, Geometry *g2,
                              my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, touches, Multipolygon, g1, Point, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_touches_geometry(g2, g1, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, touches, Multipolygon, g1, Polygon, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, touches, Multipolygon, g1, Multipolygon, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    BGCALL(result, touches, Multipolygon, g1, Linestring, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    BGCALL(result, touches, Multipolygon, g1, Multilinestring, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Do within relation check of two geometries.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::within_check(Geometry *g1, Geometry *g2,
                                        my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  gt1= g1->get_type();

  if (gt1 == Geometry::wkb_point)
    result= BG_wrap<Geom_types>::point_within_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_within_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_linestring)
    result= BG_wrap<Geom_types>::
      linestring_within_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multilinestring)
    result= BG_wrap<Geom_types>::
      multilinestring_within_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_polygon)
    result= BG_wrap<Geom_types>::
      polygon_within_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipolygon)
    result= BG_wrap<Geom_types>::
      multipolygon_within_geometry(g1, g2, pnull_value);
  else
    DBUG_ASSERT(false);
  return result;
}


/**
  Do equals relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::equals_check(Geometry *g1, Geometry *g2,
                                        my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef std::set<Point, bgpt_lt> Point_set;

  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  /*
    Only geometries of the same base type can be equal, any other
    combinations always result as false. This is different from all other types
    of geometry relation checks.
   */
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
      multipoint_equals_geometry(g1, g2, pnull_value);
  else if (gt1 == Geometry::wkb_linestring &&
           gt2 == Geometry::wkb_linestring)
    BGCALL(result, equals, Linestring, g1, Linestring, g2, pnull_value);
  else if (gt1 == Geometry::wkb_linestring &&
           gt2 == Geometry::wkb_multilinestring)
    BGCALL(result, equals, Linestring, g1, Multilinestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_linestring &&
           gt1 == Geometry::wkb_multilinestring)
    BGCALL(result, equals, Multilinestring, g1, Linestring, g2, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring &&
           gt1 == Geometry::wkb_multilinestring)
    BGCALL(result, equals, Multilinestring, g1, Multilinestring,
           g2, pnull_value);
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
    /* This branch covers all the unequal dimension combinations. */
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
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::disjoint_check(Geometry *g1, Geometry *g2,
                                          my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  gt1= g1->get_type();

  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_disjoint_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_disjoint_geometry(g1, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
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
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::intersects_check(Geometry *g1, Geometry *g2,
                                            my_bool *pnull_value)
{
  Geometry::wkbType gt1;
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
      point_intersects_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_intersects_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_intersects_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_intersects_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_intersects_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_intersects_geometry(g1, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
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
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::overlaps_check(Geometry *g1, Geometry *g2,
                                          my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (g1->feature_dimension() != g2->feature_dimension())
  {
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }

  if (gt1 == Geometry::wkb_point || gt2 == Geometry::wkb_point)
    return 0;

  if (gt1 == Geometry::wkb_multipoint && gt2 == Geometry::wkb_multipoint)
  {
    result= BG_wrap<Geom_types>::
      multipoint_overlaps_multipoint(g1, g2, pnull_value);
    return result;
  }

  switch (gt1)
  {
  case Geometry::wkb_linestring:
  {
    switch (gt2)
    {
    case Geometry::wkb_linestring:
      BGCALL(result, overlaps, Linestring, g1, Linestring, g2, pnull_value);
      break;
    case Geometry::wkb_multilinestring:
      BGCALL(result, overlaps, Linestring, g1, Multilinestring, g2, pnull_value);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
    break;
  }
  case Geometry::wkb_multilinestring:
  {
    switch (gt2)
    {
    case Geometry::wkb_linestring:
      BGCALL(result, overlaps, Multilinestring, g1, Linestring, g2, pnull_value);
      break;
    case Geometry::wkb_multilinestring:
      BGCALL(result, overlaps, Multilinestring, g1, Multilinestring, g2, pnull_value);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
    break;
  }
  case Geometry::wkb_polygon:
  {
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, overlaps, Polygon, g1, Polygon, g2, pnull_value);
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, overlaps, Polygon, g1, Multipolygon, g2, pnull_value);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
    break;
  }
  case Geometry::wkb_multipolygon:
  {
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, overlaps, Multipolygon, g1, Polygon, g2, pnull_value);
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, overlaps, Multipolygon, g1, Multipolygon, g2, pnull_value);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
    break;
  }
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Do touches relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::touches_check(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value)
{
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipolygon Multipolygon;

  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if ((gt1 == Geometry::wkb_point || gt1 == Geometry::wkb_multipoint) &&
      (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint))
  {
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }
  /*
    Touches is symetric, and one argument is allowed to be a point/multipoint.
   */
  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_touches_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_touches_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_touches_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_touches_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_touches_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_touches_geometry(g1, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  return result;
}


/**
  Do crosses relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::crosses_check(Geometry *g1, Geometry *g2,
                                         my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (gt1 == Geometry::wkb_polygon || gt2 == Geometry::wkb_point ||
      (gt1 == Geometry::wkb_multipolygon || gt2 == Geometry::wkb_multipoint))
  {
    /*
      OGC says this is not applicable, and we always return false for
      inapplicable situations.
    */
    return 0;
  }

  if (gt1 == Geometry::wkb_point)
  {
    result= 0;
    return result;
  }

  switch (gt1)
  {
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_crosses_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_crosses_geometry(g1, g2, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_crosses_geometry(g1, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

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
  @param relchk_type The type of relation check.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::bg_geo_relation_check(Geometry *g1, Geometry *g2,
                                                 Functype relchk_type,
                                                 my_bool *pnull_value)
{
  int result= 0;

  typedef BG_models<Coord_type, Coordsys> Geom_types;

  /*
    Dispatch calls to all specific type combinations for each relation check
    function.

    Boost.Geometry doesn't have dynamic polymorphism,
    e.g. the above Point, Linestring, and Polygon templates don't have a common
    base class template, so we have to dispatch by types.

    The checking functions should set null_value to true if there is error.
   */

  switch (relchk_type) {
  case SP_CONTAINS_FUNC:
    result= within_check<Geom_types>(g2, g1, pnull_value);
    break;
  case SP_WITHIN_FUNC:
    result= within_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_EQUALS_FUNC:
    result= equals_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_DISJOINT_FUNC:
    result= disjoint_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_INTERSECTS_FUNC:
    result= intersects_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_OVERLAPS_FUNC:
    result= overlaps_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_TOUCHES_FUNC:
    result= touches_check<Geom_types>(g1, g2, pnull_value);
    break;
  case SP_CROSSES_FUNC:
    result= crosses_check<Geom_types>(g1, g2, pnull_value);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }

  return result;
}

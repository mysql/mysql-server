/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file defines all spatial functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>

String *Item_func_geometry_from_text::val_str(String *str)
{
  Geometry geom;
  String arg_val;
  String *wkt= args[0]->val_str(&arg_val);
  GTextReadStream trs(wkt->ptr(), wkt->length());
  uint32 srid;

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();
  else
    srid= 0;

  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  str->q_append(srid);
  if ((null_value=(args[0]->null_value || geom.create_from_wkt(&trs, str, 0))))
    return 0;
  return str;
}


void Item_func_geometry_from_text::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
}


String *Item_func_geometry_from_wkb::val_str(String *str)
{
  String arg_val;
  String *wkb= args[0]->val_str(&arg_val);
  Geometry geom;
  uint32 srid;

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();
  else
    srid= 0;

  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  str->q_append(srid);
  if ((null_value= (args[0]->null_value ||
		    geom.create_from_wkb(wkb->ptr(), wkb->length()))))
    return 0;

  str->append(*wkb);
  return str;
}


void Item_func_geometry_from_wkb::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
}


String *Item_func_as_wkt::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value= (args[0]->null_value ||
		    geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					 swkb->length() - SRID_SIZE))))
    return 0;

  str->length(0);

  if ((null_value= geom.as_wkt(str)))
    return 0;

  return str;
}

void Item_func_as_wkt::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
}

String *Item_func_as_wkb::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value= (args[0]->null_value ||
		    geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					 swkb->length() - SRID_SIZE))))
    return 0;

  str->copy(swkb->ptr() + SRID_SIZE, swkb->length() - SRID_SIZE,
	    &my_charset_bin);
  return str;
}

void Item_func_as_wkb::fix_length_and_dec()
{
  max_length= MAX_BLOB_WIDTH;
}

String *Item_func_geometry_type::val_str(String *str)
{
  String *swkb= args[0]->val_str(str);
  Geometry geom;

  if ((null_value= (args[0]->null_value ||
		    geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					 swkb->length() - SRID_SIZE))))
    return 0;
  str->copy(geom.get_class_info()->m_name,
	    strlen(geom.get_class_info()->m_name),
	    default_charset());
  return str;
}


String *Item_func_envelope::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry geom;
  
  if ((null_value= args[0]->null_value ||
		   geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					swkb->length() - SRID_SIZE)))
    return 0;
  
  uint32 srid= uint4korr(swkb->ptr());
  str->length(0);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->q_append(srid);
  return (null_value= geom.envelope(str)) ? 0 : str;
}


String *Item_func_centroid::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value= args[0]->null_value ||
		   geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					swkb->length() - SRID_SIZE) ||
		   !GEOM_METHOD_PRESENT(geom, centroid)))
    return 0;

  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  uint32 srid= uint4korr(swkb->ptr());
  str->q_append(srid);

  return (null_value= geom.centroid(str)) ? 0 : str;
}


/*
  Spatial decomposition functions
*/

String *Item_func_spatial_decomp::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry geom;

  if ((null_value= (args[0]->null_value ||
		    geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					 swkb->length() - SRID_SIZE))))
    return 0;

  null_value= 1;
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  uint32 srid= uint4korr(swkb->ptr());
  str->q_append(srid);
  switch(decomp_func)
  {
    case SP_STARTPOINT:
      if (!GEOM_METHOD_PRESENT(geom,start_point) || geom.start_point(str))
        goto ret;
      break;

    case SP_ENDPOINT:
      if (!GEOM_METHOD_PRESENT(geom,end_point) || geom.end_point(str))
        goto ret;
      break;

    case SP_EXTERIORRING:
      if (!GEOM_METHOD_PRESENT(geom,exterior_ring) || geom.exterior_ring(str))
        goto ret;
      break;

    default:
      goto ret;
  }
  null_value= 0;

ret:
  return null_value ? 0 : str;
}


String *Item_func_spatial_decomp_n::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  long n= (long) args[1]->val_int();
  Geometry geom;

  if ((null_value= (args[0]->null_value || args[1]->null_value ||
		    geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
					 swkb->length() - SRID_SIZE))))
    return 0;

  null_value= 1;
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  uint32 srid= uint4korr(swkb->ptr());
  str->q_append(srid);
  switch(decomp_func_n)
  {
    case SP_POINTN:
      if (!GEOM_METHOD_PRESENT(geom,point_n) || geom.point_n(n,str))
        goto ret;
      break;

    case SP_GEOMETRYN:
      if (!GEOM_METHOD_PRESENT(geom,geometry_n) || geom.geometry_n(n,str))
        goto ret;
      break;

    case SP_INTERIORRINGN:
      if (!GEOM_METHOD_PRESENT(geom,interior_ring_n) ||
          geom.interior_ring_n(n,str))
        goto ret;
      break;

    default:
      goto ret;
  }
  null_value= 0;

ret:
  return null_value ? 0 : str;
}


/*
  Functions to concatinate various spatial objects
*/


/*
*  Concatinate doubles into Point
*/


String *Item_func_point::val_str(String *str)
{
  double x= args[0]->val();
  double y= args[1]->val();

  if ( (null_value= (args[0]->null_value ||
		     args[1]->null_value ||
		     str->realloc(1 + 4 + 8 + 8))))
    return 0;

  str->length(0);
  str->q_append((char)Geometry::wkbNDR);
  str->q_append((uint32)Geometry::wkbPoint);
  str->q_append(x);
  str->q_append(y);
  return str;
}


/*
  Concatinates various items into various collections
  with checkings for valid wkb type of items.
  For example, MultiPoint can be a collection of Points only.
  coll_type contains wkb type of target collection.
  item_type contains a valid wkb type of items.
  In the case when coll_type is wkbGeometryCollection,
  we do not check wkb type of items, any is valid.
*/

String *Item_func_spatial_collection::val_str(String *str)
{
  String arg_value;
  uint i;

  null_value= 1;

  str->length(0);
  if (str->reserve(1 + 4 + 4, 512))
    return 0;

  str->q_append((char) Geometry::wkbNDR);
  str->q_append((uint32) coll_type);
  str->q_append((uint32) arg_count);

  for (i= 0; i < arg_count; ++i)
  {
    String *res= args[i]->val_str(&arg_value);
    if (args[i]->null_value)
      goto ret;

    if ( coll_type == Geometry::wkbGeometryCollection )
    {
      /*
         In the case of GeometryCollection we don't need
         any checkings for item types, so just copy them
         into target collection
      */
      if ((null_value= str->reserve(res->length(), 512)))
        goto ret;

      str->q_append(res->ptr(), res->length());
    }
    else
    {
      enum Geometry::wkbType wkb_type;
      uint32 len=res->length();
      const char *data= res->ptr() + 1;

      /*
         In the case of named collection we must to
         check that items are of specific type, let's
         do this checking now
      */

      if (len < 5)
        goto ret;
      wkb_type= (Geometry::wkbType) uint4korr(data);
      data+= 4;
      len-= 5;
      if (wkb_type != item_type)
        goto ret;

      switch (coll_type) {
      case Geometry::wkbMultiPoint:
      case Geometry::wkbMultiLineString:
      case Geometry::wkbMultiPolygon:
	if (len < WKB_HEADER_SIZE)
	  goto ret;

	data-= WKB_HEADER_SIZE;
	len+= WKB_HEADER_SIZE;
	if (str->reserve(len, 512))
	  goto ret;
	str->q_append(data, len);
	break;

      case Geometry::wkbLineString:
	if (str->reserve(POINT_DATA_SIZE, 512))
	  goto ret;
	str->q_append(data, POINT_DATA_SIZE);
	break;

      case Geometry::wkbPolygon:
      {
	uint32 n_points;
	double x1, y1, x2, y2;

	if (len < 4 + 2 * POINT_DATA_SIZE)
	  goto ret;

	uint32 llen= len;
	const char *ldata= data;

	n_points= uint4korr(data);
	data+= 4;
	float8get(x1, data);
	data+= 8;
	float8get(y1, data);
	data+= 8;

	data+= (n_points - 2) * POINT_DATA_SIZE;

	float8get(x2, data);
	float8get(y2, data + 8);

	if ((x1 != x2) || (y1 != y2))
	  goto ret;

	if (str->reserve(llen, 512))
	  goto ret;
	str->q_append(ldata, llen);
      }
      break;

      default:
	goto ret;
      }
    }
  }

  if (str->length() > current_thd->variables.max_allowed_packet)
    goto ret;

  null_value = 0;

ret:
  return null_value ? 0 : str;
}

/*
  Functions for spatial relations
*/

longlong Item_func_spatial_rel::val_int()
{
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry g1, g2;
  MBR mbr1, mbr2;

  if ((null_value= (args[0]->null_value ||
		    args[1]->null_value ||
		    g1.create_from_wkb(res1->ptr() + SRID_SIZE,
				       res1->length() - SRID_SIZE) || 
		    g2.create_from_wkb(res2->ptr() + SRID_SIZE,
				       res2->length() - SRID_SIZE) ||
		    g1.get_mbr(&mbr1) || 
		    g2.get_mbr(&mbr2))))
   return 0;

  switch (spatial_rel)
  {
    case SP_CONTAINS_FUNC:
      return mbr1.contains(&mbr2);
    case SP_WITHIN_FUNC:
      return mbr1.within(&mbr2);
    case SP_EQUALS_FUNC:
      return mbr1.equals(&mbr2);
    case SP_DISJOINT_FUNC:
      return mbr1.disjoint(&mbr2);
    case SP_INTERSECTS_FUNC:
      return mbr1.intersects(&mbr2);
    case SP_TOUCHES_FUNC:
      return mbr1.touches(&mbr2);
    case SP_OVERLAPS_FUNC:
      return mbr1.overlaps(&mbr2);
    case SP_CROSSES_FUNC:
      return 0;
    default:
      break;
  }

  null_value=1;
  return 0;
}

longlong Item_func_isempty::val_int()
{
  String tmp; 
  null_value=0;
  return args[0]->null_value ? 1 : 0;
}

longlong Item_func_issimple::val_int()
{
  String tmp;
  String *wkb=args[0]->val_str(&tmp);

  if ((null_value= (!wkb || args[0]->null_value )))
    return 0;
  /* TODO: Ramil or Holyfoot, add real IsSimple calculation */
  return 0;
}

longlong Item_func_isclosed::val_int()
{
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry geom;
  int isclosed;

  null_value= (!swkb || 
	       args[0]->null_value ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) ||
	       !GEOM_METHOD_PRESENT(geom,is_closed) ||
	       geom.is_closed(&isclosed));

  return (longlong) isclosed;
}

/*
  Numerical functions
*/

longlong Item_func_dimension::val_int()
{
  uint32 dim;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       args[0]->null_value ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       geom.dimension(&dim));
  return (longlong) dim;
}

longlong Item_func_numinteriorring::val_int()
{
  uint32 num;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, num_interior_ring) || 
	       geom.num_interior_ring(&num));
  return (longlong) num;
}

longlong Item_func_numgeometries::val_int()
{
  uint32 num= 0;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, num_geometries) || 
	       geom.num_geometries(&num));
  return (longlong) num;
}

longlong Item_func_numpoints::val_int()
{
  uint32 num;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       args[0]->null_value ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) ||
	       !GEOM_METHOD_PRESENT(geom, num_points) ||
	       geom.num_points(&num));
  return (longlong) num;
}

double Item_func_x::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, get_x) || 
	       geom.get_x(&res));
  return res;
}

double Item_func_y::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, get_y) || 
	       geom.get_y(&res));
  return res;
}

double Item_func_area::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, area) || 
	       geom.area(&res));
  return res;
}

double Item_func_glength::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, length) || 
	       geom.length(&res));
  return res;
}

longlong Item_func_srid::val_int()
{
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE));
  uint32 res= uint4korr(swkb->ptr());
  return (longlong) res;
}

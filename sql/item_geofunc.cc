/* Copyright (C) 2003-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file defines all spatial functions */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_SPATIAL
#include <m_ctype.h>

Field *Item_geometry_func::tmp_table_field(TABLE *t_arg)
{
  Field *result;
  if ((result= new Field_geom(max_length, maybe_null, name, t_arg->s,
                              get_geometry_type())))
    result->init(t_arg);
  return result;
}

void Item_geometry_func::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0;
  max_length= max_field_size;
  maybe_null= 1;
}


String *Item_func_geometry_from_text::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  Geometry_buffer buffer;
  String arg_val;
  String *wkt= args[0]->val_str(&arg_val);

  if ((null_value= args[0]->null_value))
    return 0;

  Gis_read_stream trs(wkt->charset(), wkt->ptr(), wkt->length());
  uint32 srid= 0;

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  str->q_append(srid);
  if ((null_value= !Geometry::create_from_wkt(&buffer, &trs, str, 0)))
    return 0;
  return str;
}


String *Item_func_geometry_from_wkb::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *wkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  uint32 srid= 0;

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  str->q_append(srid);
  if ((null_value= 
       (args[0]->null_value ||
        !Geometry::create_from_wkb(&buffer, wkb->ptr(), wkb->length(), str))))
    return 0;
  return str;
}


String *Item_func_as_wkt::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  const char *dummy;

  if ((null_value=
       (args[0]->null_value ||
	!(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length())))))
    return 0;

  str->length(0);
  if ((null_value= geom->as_wkt(str, &dummy)))
    return 0;

  return str;
}


void Item_func_as_wkt::fix_length_and_dec()
{
  max_length=MAX_BLOB_WIDTH;
  maybe_null= 1;
}


String *Item_func_as_wkb::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;

  if ((null_value=
       (args[0]->null_value ||
	!(Geometry::construct(&buffer, swkb->ptr(), swkb->length())))))
    return 0;

  str->copy(swkb->ptr() + SRID_SIZE, swkb->length() - SRID_SIZE,
	    &my_charset_bin);
  return str;
}


String *Item_func_geometry_type::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *swkb= args[0]->val_str(str);
  Geometry_buffer buffer;
  Geometry *geom= NULL;

  if ((null_value=
       (args[0]->null_value ||
	!(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length())))))
    return 0;
  /* String will not move */
  str->copy(geom->get_class_info()->m_name.str,
	    geom->get_class_info()->m_name.length,
	    default_charset());
  return str;
}


Field::geometry_type Item_func_envelope::get_geometry_type() const
{
  return Field::GEOM_POLYGON;
}


String *Item_func_envelope::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;
  
  if ((null_value=
       args[0]->null_value ||
       !(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length()))))
    return 0;
  
  srid= uint4korr(swkb->ptr());
  str->set_charset(&my_charset_bin);
  str->length(0);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->q_append(srid);
  return (null_value= geom->envelope(str)) ? 0 : str;
}


Field::geometry_type Item_func_centroid::get_geometry_type() const
{
  return Field::GEOM_POINT;
}


String *Item_func_centroid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value= args[0]->null_value ||
       !(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length()))))
    return 0;

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->length(0);
  srid= uint4korr(swkb->ptr());
  str->q_append(srid);

  return (null_value= test(geom->centroid(str))) ? 0 : str;
}


/*
  Spatial decomposition functions
*/

String *Item_func_spatial_decomp::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value=
       (args[0]->null_value ||
	!(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length())))))
    return 0;

  srid= uint4korr(swkb->ptr());
  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    goto err;
  str->length(0);
  str->q_append(srid);
  switch (decomp_func) {
    case SP_STARTPOINT:
      if (geom->start_point(str))
        goto err;
      break;

    case SP_ENDPOINT:
      if (geom->end_point(str))
        goto err;
      break;

    case SP_EXTERIORRING:
      if (geom->exterior_ring(str))
        goto err;
      break;

    default:
      goto err;
  }
  return str;

err:
  null_value= 1;
  return 0;
}


String *Item_func_spatial_decomp_n::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  long n= (long) args[1]->val_int();
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value=
       (args[0]->null_value || args[1]->null_value ||
	!(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length())))))
    return 0;

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    goto err;
  srid= uint4korr(swkb->ptr());
  str->length(0);
  str->q_append(srid);
  switch (decomp_func_n)
  {
    case SP_POINTN:
      if (geom->point_n(n,str))
        goto err;
      break;

    case SP_GEOMETRYN:
      if (geom->geometry_n(n,str))
        goto err;
      break;

    case SP_INTERIORRINGN:
      if (geom->interior_ring_n(n,str))
        goto err;
      break;

    default:
      goto err;
  }
  return str;

err:
  null_value=1;
  return 0;
}


/*
  Functions to concatenate various spatial objects
*/


/*
*  Concatenate doubles into Point
*/


Field::geometry_type Item_func_point::get_geometry_type() const
{
  return Field::GEOM_POINT;
}


String *Item_func_point::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double x= args[0]->val_real();
  double y= args[1]->val_real();

  if ((null_value= (args[0]->null_value ||
		    args[1]->null_value ||
		    str->realloc(1 + 4 + SIZEOF_STORED_DOUBLE*2))))
    return 0;

  str->set_charset(&my_charset_bin);
  str->length(0);
  str->q_append((char)Geometry::wkb_ndr);
  str->q_append((uint32)Geometry::wkb_point);
  str->q_append(x);
  str->q_append(y);
  return str;
}


/*
  Concatenates various items into various collections
  with checkings for valid wkb type of items.
  For example, MultiPoint can be a collection of Points only.
  coll_type contains wkb type of target collection.
  item_type contains a valid wkb type of items.
  In the case when coll_type is wkbGeometryCollection,
  we do not check wkb type of items, any is valid.
*/

String *Item_func_spatial_collection::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_value;
  uint i;

  str->set_charset(&my_charset_bin);
  str->length(0);
  if (str->reserve(1 + 4 + 4, 512))
    goto err;

  str->q_append((char) Geometry::wkb_ndr);
  str->q_append((uint32) coll_type);
  str->q_append((uint32) arg_count);

  for (i= 0; i < arg_count; ++i)
  {
    String *res= args[i]->val_str(&arg_value);
    uint32 len;
    if (args[i]->null_value || ((len= res->length()) < WKB_HEADER_SIZE))
      goto err;

    if (coll_type == Geometry::wkb_geometrycollection)
    {
      /*
	In the case of GeometryCollection we don't need any checkings
	for item types, so just copy them into target collection
      */
      if (str->append(res->ptr(), len, (uint32) 512))
        goto err;
    }
    else
    {
      enum Geometry::wkbType wkb_type;
      const char *data= res->ptr() + 1;

      /*
	In the case of named collection we must check that items
	are of specific type, let's do this checking now
      */

      wkb_type= (Geometry::wkbType) uint4korr(data);
      data+= 4;
      len-= 5;
      if (wkb_type != item_type)
        goto err;

      switch (coll_type) {
      case Geometry::wkb_multipoint:
      case Geometry::wkb_multilinestring:
      case Geometry::wkb_multipolygon:
	if (len < WKB_HEADER_SIZE ||
	    str->append(data-WKB_HEADER_SIZE, len+WKB_HEADER_SIZE, 512))
	  goto err;
	break;

      case Geometry::wkb_linestring:
	if (str->append(data, POINT_DATA_SIZE, 512))
	  goto err;
	break;
      case Geometry::wkb_polygon:
      {
	uint32 n_points;
	double x1, y1, x2, y2;
	const char *org_data= data;

	if (len < 4 + 2 * POINT_DATA_SIZE)
	  goto err;

	n_points= uint4korr(data);
	data+= 4;
	float8get(x1, data);
	data+= SIZEOF_STORED_DOUBLE;
	float8get(y1, data);
	data+= SIZEOF_STORED_DOUBLE;

	data+= (n_points - 2) * POINT_DATA_SIZE;

	float8get(x2, data);
	float8get(y2, data + SIZEOF_STORED_DOUBLE);

	if ((x1 != x2) || (y1 != y2) ||
	    str->append(org_data, len, 512))
	  goto err;
      }
      break;

      default:
	goto err;
      }
    }
  }
  if (str->length() > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  null_value = 0;
  return str;

err:
  null_value= 1;
  return 0;
}


/*
  Functions for spatial relations
*/

longlong Item_func_spatial_rel::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  MBR mbr1, mbr2;
  const char *dummy;

  if ((null_value=
       (args[0]->null_value ||
	args[1]->null_value ||
	!(g1= Geometry::construct(&buffer1, res1->ptr(), res1->length())) ||
	!(g2= Geometry::construct(&buffer2, res2->ptr(), res2->length())) ||
	g1->get_mbr(&mbr1, &dummy) ||
	g2->get_mbr(&mbr2, &dummy))))
   return 0;

  switch (spatial_rel) {
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
  DBUG_ASSERT(fixed == 1);
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  
  null_value= args[0]->null_value ||
              !(Geometry::construct(&buffer, swkb->ptr(), swkb->length()));
  return null_value ? 1 : 0;
}


longlong Item_func_issimple::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  
  null_value= args[0]->null_value ||
              !(Geometry::construct(&buffer, swkb->ptr(), swkb->length()));
  /* TODO: Ramil or Holyfoot, add real IsSimple calculation */
  return 0;
}


longlong Item_func_isclosed::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  Geometry *geom;
  int isclosed= 0;				// In case of error

  null_value= (!swkb || 
	       args[0]->null_value ||
	       !(geom=
		 Geometry::construct(&buffer, swkb->ptr(), swkb->length())) ||
	       geom->is_closed(&isclosed));

  return (longlong) isclosed;
}

/*
  Numerical functions
*/


longlong Item_func_dimension::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 dim= 0;				// In case of error
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;
  const char *dummy;

  null_value= (!swkb || 
	       args[0]->null_value ||
	       !(geom= Geometry::construct(&buffer, swkb->ptr(), swkb->length())) ||
	       geom->dimension(&dim, &dummy));
  return (longlong) dim;
}


longlong Item_func_numinteriorring::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of error
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb || 
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->num_interior_ring(&num));
  return (longlong) num;
}


longlong Item_func_numgeometries::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb ||
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->num_geometries(&num));
  return (longlong) num;
}


longlong Item_func_numpoints::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb ||
	       args[0]->null_value ||
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->num_points(&num));
  return (longlong) num;
}


double Item_func_x::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0.0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb ||
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->get_x(&res));
  return res;
}


double Item_func_y::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb ||
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->get_y(&res));
  return res;
}


double Item_func_area::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;
  const char *dummy;

  null_value= (!swkb ||
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(), swkb->length())) ||
	       geom->area(&res, &dummy));
  return res;
}

double Item_func_glength::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  null_value= (!swkb || 
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(),
                                           swkb->length())) ||
	       geom->geom_length(&res));
  return res;
}

longlong Item_func_srid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  
  null_value= (!swkb || 
	       !Geometry::construct(&buffer,
                                    swkb->ptr(), swkb->length()));
  if (null_value)
    return 0;

  return (longlong) (uint4korr(swkb->ptr()));
}

#endif /*HAVE_SPATIAL*/

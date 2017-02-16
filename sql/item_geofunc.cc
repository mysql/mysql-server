/* Copyright (c) 2003, 2016, Oracle and/or its affiliates.
   Copyright (c) 2011, 2016, MariaDB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/**
  @file

  @brief
  This file defines all spatial functions
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // THD, set_var.h: THD
#include "set_var.h"
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
  max_length= (uint32) 4294967295U;
  maybe_null= 1;
}


String *Item_func_geometry_from_text::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  Geometry_buffer buffer;
  String arg_val;
  String *wkt= args[0]->val_str_ascii(&arg_val);

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
  String *wkb;
  Geometry_buffer buffer;
  uint32 srid= 0;

  if (args[0]->field_type() == MYSQL_TYPE_GEOMETRY)
  {
    String *str_ret= args[0]->val_str(str);
    null_value= args[0]->null_value;
    return str_ret;
  }

  wkb= args[0]->val_str(&arg_val);

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
  {
    null_value= TRUE;                           /* purecov: inspected */
    return 0;                                   /* purecov: inspected */
  }
  str->length(0);
  str->q_append(srid);
  if ((null_value= 
        (args[0]->null_value ||
         !Geometry::create_from_wkb(&buffer, wkb->ptr(), wkb->length(), str))))
    return 0;
  return str;
}


String *Item_func_as_wkt::val_str_ascii(String *str)
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
  str->set_charset(&my_charset_latin1);
  if ((null_value= geom->as_wkt(str, &dummy)))
    return 0;

  return str;
}


void Item_func_as_wkt::fix_length_and_dec()
{
  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
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


String *Item_func_geometry_type::val_str_ascii(String *str)
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
            &my_charset_latin1);
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
  uint32 srid= 0;

  if ((null_value= (args[0]->null_value ||
                    args[1]->null_value ||
                    str->realloc(4/*SRID*/ + 1 + 4 + SIZEOF_STORED_DOUBLE * 2))))
    return 0;

  str->set_charset(&my_charset_bin);
  str->length(0);
  str->q_append(srid);
  str->q_append((char)Geometry::wkb_ndr);
  str->q_append((uint32)Geometry::wkb_point);
  str->q_append(x);
  str->q_append(y);
  return str;
}


/**
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
  uint32 srid= 0;

  str->set_charset(&my_charset_bin);
  str->length(0);
  if (str->reserve(4/*SRID*/ + 1 + 4 + 4, 512))
    goto err;

  str->q_append(srid);
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
      if (str->append(res->ptr() + 4/*SRID*/, len - 4/*SRID*/, (uint32) 512))
        goto err;
    }
    else
    {
      enum Geometry::wkbType wkb_type;
      const uint data_offset= 4/*SRID*/ + 1;
      if (res->length() < data_offset + sizeof(uint32))
        goto err;
      const char *data= res->ptr() + data_offset;

      /*
	In the case of named collection we must check that items
	are of specific type, let's do this checking now
      */

      wkb_type= (Geometry::wkbType) uint4korr(data);
      data+= 4;
      len-= 5 + 4/*SRID*/;
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
	if (len < POINT_DATA_SIZE || str->append(data, POINT_DATA_SIZE, 512))
	  goto err;
	break;
      case Geometry::wkb_polygon:
      {
	uint32 n_points;
	double x1, y1, x2, y2;
	const char *org_data= data;

	if (len < 4)
	  goto err;

	n_points= uint4korr(data);
	data+= 4;

        if (n_points < 2 || len < 4 + n_points * POINT_DATA_SIZE)
          goto err;
        
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
  const char *dummy;

  if ((null_value=
       (args[0]->null_value ||
	args[1]->null_value ||
	!(g1= Geometry::construct(&buffer1, res1->ptr(), res1->length())) ||
	!(g2= Geometry::construct(&buffer2, res2->ptr(), res2->length())) ||
	g1->get_mbr(&mbr1, &dummy) || !mbr1.valid() ||
	g2->get_mbr(&mbr2, &dummy) || !mbr2.valid())))
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


Item_func_spatial_rel::Item_func_spatial_rel(Item *a,Item *b,
                                             enum Functype sp_rel) :
    Item_bool_func2(a,b), collector()
{
  spatial_rel = sp_rel;
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


static double count_edge_t(const Gcalc_heap::Info *ea,
                           const Gcalc_heap::Info *eb,
                           const Gcalc_heap::Info *v,
                           double &ex, double &ey, double &vx, double &vy,
                           double &e_sqrlen)
{
  ex= eb->node.shape.x - ea->node.shape.x;
  ey= eb->node.shape.y - ea->node.shape.y;
  vx= v->node.shape.x - ea->node.shape.x;
  vy= v->node.shape.y - ea->node.shape.y;
  e_sqrlen= ex * ex + ey * ey;
  return (ex * vx + ey * vy) / e_sqrlen;
}


static double distance_to_line(double ex, double ey, double vx, double vy,
                               double e_sqrlen)
{
  return fabs(vx * ey - vy * ex) / sqrt(e_sqrlen);
}


static double distance_points(const Gcalc_heap::Info *a,
                              const Gcalc_heap::Info *b)
{
  double x= a->node.shape.x - b->node.shape.x;
  double y= a->node.shape.y - b->node.shape.y;
  return sqrt(x * x + y * y);
}


#define GIS_ZERO 0.00000000001

longlong Item_func_spatial_rel::val_int()
{
  DBUG_ENTER("Item_func_spatial_rel::val_int");
  DBUG_ASSERT(fixed == 1);
  String *res1;
  String *res2;
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  int result= 0;
  int mask= 0;
  uint shape_a, shape_b;
  MBR umbr, mbr1, mbr2;
  const char *c_end;

  res1= args[0]->val_str(&tmp_value1);
  res2= args[1]->val_str(&tmp_value2);
  Gcalc_operation_transporter trn(&func, &collector);

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);

  if ((null_value=
       (args[0]->null_value || args[1]->null_value ||
	!(g1= Geometry::construct(&buffer1, res1->ptr(), res1->length())) ||
	!(g2= Geometry::construct(&buffer2, res2->ptr(), res2->length())) ||
        g1->get_mbr(&mbr1, &c_end) || !mbr1.valid() ||
        g2->get_mbr(&mbr2, &c_end) || !mbr2.valid())))
    goto exit;

  umbr= mbr1;
  umbr.add_mbr(&mbr2);
  collector.set_extent(umbr.xmin, umbr.xmax, umbr.ymin, umbr.ymax);

  mbr1.buffer(1e-5);

  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      if (!mbr1.contains(&mbr2))
        goto exit;
      mask= 1;
      func.add_operation(Gcalc_function::op_difference, 2);
      /* Mind the g2 goes first. */
      null_value= g2->store_shapes(&trn) || g1->store_shapes(&trn);
      break;
    case SP_WITHIN_FUNC:
      mbr2.buffer(2e-5);
      if (!mbr1.within(&mbr2))
        goto exit;
      mask= 1;
      func.add_operation(Gcalc_function::op_difference, 2);
      null_value= g1->store_shapes(&trn) || g2->store_shapes(&trn);
      break;
    case SP_EQUALS_FUNC:
      if (!mbr1.contains(&mbr2))
        goto exit;
      mask= 1;
      func.add_operation(Gcalc_function::op_symdifference, 2);
      null_value= g1->store_shapes(&trn) || g2->store_shapes(&trn);
      break;
    case SP_DISJOINT_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_intersection, 2);
      null_value= g1->store_shapes(&trn) || g2->store_shapes(&trn);
      break;
    case SP_INTERSECTS_FUNC:
      if (!mbr1.intersects(&mbr2))
        goto exit;
      func.add_operation(Gcalc_function::op_intersection, 2);
      null_value= g1->store_shapes(&trn) || g2->store_shapes(&trn);
      break;
    case SP_OVERLAPS_FUNC:
    case SP_CROSSES_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      func.add_operation(Gcalc_function::v_find_t |
                         Gcalc_function::op_intersection, 2);
      shape_a= func.get_next_expression_pos();
      if ((null_value= g1->store_shapes(&trn)))
        break;
      shape_b= func.get_next_expression_pos();
      if ((null_value= g2->store_shapes(&trn)))
        break;
      func.add_operation(Gcalc_function::v_find_t |
                         Gcalc_function::op_intersection, 2);
      func.add_operation(Gcalc_function::v_find_t |
                         Gcalc_function::op_difference, 2);
      func.repeat_expression(shape_a);
      func.repeat_expression(shape_b);
      func.add_operation(Gcalc_function::v_find_t |
                         Gcalc_function::op_difference, 2);
      func.repeat_expression(shape_b);
      func.repeat_expression(shape_a);
      break;
    case SP_TOUCHES_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      func.add_operation(Gcalc_function::v_find_f |
                         Gcalc_function::op_not |
                         Gcalc_function::op_intersection, 2);
      func.add_operation(Gcalc_function::op_internals, 1);
      shape_a= func.get_next_expression_pos();
      if ((null_value= g1->store_shapes(&trn)))
        break;
      func.add_operation(Gcalc_function::op_internals, 1);
      shape_b= func.get_next_expression_pos();
      if ((null_value= g2->store_shapes(&trn)))
        break;
      func.add_operation(Gcalc_function::v_find_t |
                         Gcalc_function::op_intersection, 2);
      func.add_operation(Gcalc_function::op_border, 1);
      func.repeat_expression(shape_a);
      func.add_operation(Gcalc_function::op_border, 1);
      func.repeat_expression(shape_b);
      break;
    default:
      DBUG_ASSERT(FALSE);
      break;
  }

  if (null_value)
    goto exit;

  collector.prepare_operation();
  scan_it.init(&collector);
  scan_it.killed= (int *) &(current_thd->killed);

  if (func.alloc_states())
    goto exit;

  result= func.check_function(scan_it) ^ mask;

exit:
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
}


Item_func_spatial_operation::~Item_func_spatial_operation()
{
}


String *Item_func_spatial_operation::val_str(String *str_value)
{
  DBUG_ENTER("Item_func_spatial_operation::val_str");
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  uint32 srid= 0;
  Gcalc_operation_transporter trn(&func, &collector);
  MBR mbr1, mbr2;
  const char *c_end;

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);
  func.add_operation(spatial_op, 2);

  if ((null_value=
       (args[0]->null_value || args[1]->null_value ||
	!(g1= Geometry::construct(&buffer1, res1->ptr(), res1->length())) ||
	!(g2= Geometry::construct(&buffer2, res2->ptr(), res2->length())) ||
        g1->get_mbr(&mbr1, &c_end) || !mbr1.valid() ||
        g2->get_mbr(&mbr2, &c_end) || !mbr2.valid())))
  {
    str_value= 0;
    goto exit;
  }

  mbr1.add_mbr(&mbr2);
  collector.set_extent(mbr1.xmin, mbr1.xmax, mbr1.ymin, mbr1.ymax);
  
  if ((null_value= g1->store_shapes(&trn) || g2->store_shapes(&trn)))
  {
    str_value= 0;
    goto exit;
  }

  collector.prepare_operation();
  if (func.alloc_states())
    goto exit;

  operation.init(&func);

  if (operation.count_all(&collector) ||
      operation.get_result(&res_receiver))
    goto exit;


  str_value->set_charset(&my_charset_bin);
  if (str_value->reserve(SRID_SIZE, 512))
    goto exit;
  str_value->length(0);
  str_value->q_append(srid);

  if (Geometry::create_from_opresult(&buffer1, str_value, res_receiver))
    goto exit;

exit:
  collector.reset();
  func.reset();
  res_receiver.reset();
  DBUG_RETURN(str_value);
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


static const int SINUSES_CALCULATED= 32;
static double n_sinus[SINUSES_CALCULATED+1]=
{
  0,
  0.04906767432741802,
  0.0980171403295606,
  0.1467304744553618,
  0.1950903220161283,
  0.2429801799032639,
  0.2902846772544623,
  0.3368898533922201,
  0.3826834323650898,
  0.4275550934302821,
  0.4713967368259976,
  0.5141027441932217,
  0.5555702330196022,
  0.5956993044924334,
  0.6343932841636455,
  0.6715589548470183,
  0.7071067811865475,
  0.7409511253549591,
  0.773010453362737,
  0.8032075314806448,
  0.8314696123025452,
  0.8577286100002721,
  0.8819212643483549,
  0.9039892931234433,
  0.9238795325112867,
  0.9415440651830208,
  0.9569403357322089,
  0.970031253194544,
  0.9807852804032304,
  0.989176509964781,
  0.9951847266721968,
  0.9987954562051724,
  1
};


static void get_n_sincos(int n, double *sinus, double *cosinus)
{
  DBUG_ASSERT(n > 0 && n < SINUSES_CALCULATED*2+1);
  if (n < (SINUSES_CALCULATED + 1))
  {
    *sinus= n_sinus[n];
    *cosinus= n_sinus[SINUSES_CALCULATED - n];
  }
  else
  {
    n-= SINUSES_CALCULATED;
    *sinus= n_sinus[SINUSES_CALCULATED - n];
    *cosinus= -n_sinus[n];
  }
}


static int fill_half_circle(Gcalc_shape_transporter *trn, double x, double y,
                            double ax, double ay)
{
  double n_sin, n_cos;
  double x_n, y_n;
  for (int n = 1; n < (SINUSES_CALCULATED * 2 - 1); n++)
  {
    get_n_sincos(n, &n_sin, &n_cos);
    x_n= ax * n_cos - ay * n_sin;
    y_n= ax * n_sin + ay * n_cos;
    if (trn->add_point(x_n + x, y_n + y))
      return 1;
  }
  return 0;
}


static int fill_gap(Gcalc_shape_transporter *trn,
                    double x, double y,
                    double ax, double ay, double bx, double by, double d,
                    bool *empty_gap)
{
  double ab= ax * bx + ay * by;
  double cosab= ab / (d * d) + GIS_ZERO;
  double n_sin, n_cos;
  double x_n, y_n;
  int n=1;

  *empty_gap= true;
  for (;;)
  {
    get_n_sincos(n++, &n_sin, &n_cos);
    if (n_cos <= cosab)
      break;
    *empty_gap= false;
    x_n= ax * n_cos - ay * n_sin;
    y_n= ax * n_sin + ay * n_cos;
    if (trn->add_point(x_n + x, y_n + y))
      return 1;
  }
  return 0;
}


/*
  Calculates the vector (p2,p1) and
  negatively orthogonal to it with the length of d.
  The result is (ex,ey) - the vector, (px,py) - the orthogonal.
*/

static void calculate_perpendicular(
    double x1, double y1, double x2, double y2, double d,
    double *ex, double *ey,
    double *px, double *py)
{
  double q;
  *ex= x1 - x2;
  *ey= y1 - y2;
  q= d / sqrt((*ex) * (*ex) + (*ey) * (*ey));
  *px= (*ey) * q;
  *py= -(*ex) * q;
}


int Item_func_buffer::Transporter::single_point(double x, double y)
{
  if (buffer_op == Gcalc_function::op_difference)
  {
    m_fn->add_operation(Gcalc_function::op_false, 0);
    return 0;
  }
  
  m_nshapes= 0;
  return add_point_buffer(x, y);
}


int Item_func_buffer::Transporter::add_edge_buffer(
  double x3, double y3, bool round_p1, bool round_p2)
{
  Gcalc_operation_transporter trn(m_fn, m_heap);
  double e1_x, e1_y, e2_x, e2_y, p1_x, p1_y, p2_x, p2_y;
  double e1e2;
  double sin1, cos1;
  double x_n, y_n;
  bool empty_gap1, empty_gap2;

  ++m_nshapes;
  if (trn.start_simple_poly())
    return 1;

  calculate_perpendicular(x1, y1, x2, y2, m_d, &e1_x, &e1_y, &p1_x, &p1_y);
  calculate_perpendicular(x3, y3, x2, y2, m_d, &e2_x, &e2_y, &p2_x, &p2_y);

  e1e2= e1_x * e2_y - e2_x * e1_y;
  sin1= n_sinus[1];
  cos1= n_sinus[31];
  if (e1e2 < 0)
  {
    empty_gap2= false;
    x_n= x2 + p2_x * cos1 - p2_y * sin1;
    y_n= y2 + p2_y * cos1 + p2_x * sin1;
    if (fill_gap(&trn, x2, y2, -p1_x,-p1_y, p2_x,p2_y, m_d, &empty_gap1) ||
        trn.add_point(x2 + p2_x, y2 + p2_y) ||
        trn.add_point(x_n, y_n))
      return 1;
  }
  else
  {
    x_n= x2 - p2_x * cos1 - p2_y * sin1;
    y_n= y2 - p2_y * cos1 + p2_x * sin1;
    if (trn.add_point(x_n, y_n) ||
        trn.add_point(x2 - p2_x, y2 - p2_y) ||
        fill_gap(&trn, x2, y2, -p2_x, -p2_y, p1_x, p1_y, m_d, &empty_gap2))
      return 1;
    empty_gap1= false;
  }
  if ((!empty_gap2 && trn.add_point(x2 + p1_x, y2 + p1_y)) ||
      trn.add_point(x1 + p1_x, y1 + p1_y))
    return 1;

  if (round_p1 && fill_half_circle(&trn, x1, y1, p1_x, p1_y))
    return 1;

  if (trn.add_point(x1 - p1_x, y1 - p1_y) ||
      (!empty_gap1 && trn.add_point(x2 - p1_x, y2 - p1_y)))
    return 1;
  return trn.complete_simple_poly();
}


int Item_func_buffer::Transporter::add_last_edge_buffer()
{
  Gcalc_operation_transporter trn(m_fn, m_heap);
  double e1_x, e1_y, p1_x, p1_y;

  ++m_nshapes;
  if (trn.start_simple_poly())
    return 1;

  calculate_perpendicular(x1, y1, x2, y2, m_d, &e1_x, &e1_y, &p1_x, &p1_y);

  if (trn.add_point(x1 + p1_x, y1 + p1_y) ||
      trn.add_point(x1 - p1_x, y1 - p1_y) ||
      trn.add_point(x2 - p1_x, y2 - p1_y) ||
      fill_half_circle(&trn, x2, y2, -p1_x, -p1_y) ||
      trn.add_point(x2 + p1_x, y2 + p1_y))
    return 1;
  return trn.complete_simple_poly();
}


int Item_func_buffer::Transporter::add_point_buffer(double x, double y)
{
  Gcalc_operation_transporter trn(m_fn, m_heap);

  m_nshapes++;
  if (trn.start_simple_poly())
    return 1;
  if (trn.add_point(x - m_d, y) ||
      fill_half_circle(&trn, x, y, -m_d, 0.0) ||
      trn.add_point(x + m_d, y) ||
      fill_half_circle(&trn, x, y, m_d, 0.0))
    return 1;
  return trn.complete_simple_poly();
}


int Item_func_buffer::Transporter::start_line()
{
  if (buffer_op == Gcalc_function::op_difference)
  {
    if (m_fn->reserve_op_buffer(1))
      return 1;
    m_fn->add_operation(Gcalc_function::op_false, 0);
    skip_line= TRUE;
    return 0;
  }
  
  m_nshapes= 0;

  if (m_fn->reserve_op_buffer(2))
    return 1;
  last_shape_pos= m_fn->get_next_expression_pos();
  m_fn->add_operation(buffer_op, 0);
  m_npoints= 0;
  int_start_line();
  return 0;
}


int Item_func_buffer::Transporter::start_poly()
{
  m_nshapes= 1;

  if (m_fn->reserve_op_buffer(2))
    return 1;
  last_shape_pos= m_fn->get_next_expression_pos();
  m_fn->add_operation(buffer_op, 0);
  return Gcalc_operation_transporter::start_poly();
}


int Item_func_buffer::Transporter::complete_poly()
{
  if (Gcalc_operation_transporter::complete_poly())
    return 1;
  m_fn->add_operands_to_op(last_shape_pos, m_nshapes);
  return 0;
}


int Item_func_buffer::Transporter::start_ring()
{
  m_npoints= 0;
  return Gcalc_operation_transporter::start_ring();
}


int Item_func_buffer::Transporter::start_collection(int n_objects)
{
  if (m_fn->reserve_op_buffer(1))
    return 1;
  m_fn->add_operation(Gcalc_function::op_union, n_objects);
  return 0;
}


int Item_func_buffer::Transporter::add_point(double x, double y)
{
  if (skip_line)
    return 0;

  if (m_npoints && x == x2 && y == y2)
    return 0;

  ++m_npoints;

  if (m_npoints == 1)
  {
    x00= x;
    y00= y;
  }
  else if (m_npoints == 2)
  {
    x01= x;
    y01= y;
  }
  else if (add_edge_buffer(x, y, (m_npoints == 3) && line_started(), false))
    return 1;

  x1= x2;
  y1= y2;
  x2= x;
  y2= y;

  return line_started() ? 0 : Gcalc_operation_transporter::add_point(x, y);
}


int Item_func_buffer::Transporter::complete()
{
  if (m_npoints)
  {
    if (m_npoints == 1)
    {
      if (add_point_buffer(x2, y2))
        return 1;
    }
    else if (m_npoints == 2)
    {
      if (add_edge_buffer(x1, y1, true, true))
        return 1;
    }
    else if (line_started())
    {
      if (add_last_edge_buffer())
        return 1;
    }
    else
    {
      if (x2 != x00 || y2 != y00)
      {
        if (add_edge_buffer(x00, y00, false, false))
          return 1;
        x1= x2;
        y1= y2;
        x2= x00;
        y2= y00;
      }
      if (add_edge_buffer(x01, y01, false, false))
        return 1;
    }
  }

  return 0;
}


int Item_func_buffer::Transporter::complete_line()
{
  if (!skip_line)
  {
    if (complete())
      return 1;
    int_complete_line();
    m_fn->add_operands_to_op(last_shape_pos, m_nshapes);
  }
  skip_line= FALSE;
  return 0;
}


int Item_func_buffer::Transporter::complete_ring()
{
  return complete() ||
         Gcalc_operation_transporter::complete_ring();
}


String *Item_func_buffer::val_str(String *str_value)
{
  DBUG_ENTER("Item_func_buffer::val_str");
  DBUG_ASSERT(fixed == 1);
  String *obj= args[0]->val_str(&tmp_value);
  double dist= args[1]->val_real();
  Geometry_buffer buffer;
  Geometry *g;
  uint32 srid= 0;
  String *str_result= NULL;
  Transporter trn(&func, &collector, dist);
  MBR mbr;
  const char *c_end;

  null_value= 1;
  if (args[0]->null_value || args[1]->null_value ||
      !(g= Geometry::construct(&buffer, obj->ptr(), obj->length())) ||
      g->get_mbr(&mbr, &c_end))
    goto mem_error;

  if (dist > 0.0)
    mbr.buffer(dist);
  else
  {
    /* This happens when dist is too far negative. */
    if (mbr.xmax + dist < mbr.xmin || mbr.ymax + dist < mbr.ymin)
      goto return_empty_result;
  }

  collector.set_extent(mbr.xmin, mbr.xmax, mbr.ymin, mbr.ymax);
  /*
    If the distance given is 0, the Buffer function is in fact NOOP,
    so it's natural just to return the argument1.
    Besides, internal calculations here can't handle zero distance anyway.
  */
  if (fabs(dist) < GIS_ZERO)
  {
    null_value= 0;
    str_result= obj;
    goto mem_error;
  }

  if (g->store_shapes(&trn))
    goto mem_error;

  collector.prepare_operation();
  if (func.alloc_states())
    goto mem_error;
  operation.init(&func);
  operation.killed= (int *) &(current_thd->killed);

  if (operation.count_all(&collector) ||
      operation.get_result(&res_receiver))
    goto mem_error;


return_empty_result:
  str_value->set_charset(&my_charset_bin);
  if (str_value->reserve(SRID_SIZE, 512))
    goto mem_error;
  str_value->length(0);
  str_value->q_append(srid);

  if (Geometry::create_from_opresult(&buffer, str_value, res_receiver))
    goto mem_error;

  null_value= 0;
  str_result= str_value;
mem_error:
  collector.reset();
  func.reset();
  res_receiver.reset();
  DBUG_RETURN(str_result);
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
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  Gcalc_operation_transporter trn(&func, &collector);
  Geometry *g;
  int result= 1;
  const Gcalc_scan_iterator::event_point *ev;
  MBR mbr;
  const char *c_end;

  DBUG_ENTER("Item_func_issimple::val_int");
  DBUG_ASSERT(fixed == 1);
  
  if ((null_value= (args[0]->null_value ||
          !(g= Geometry::construct(&buffer, swkb->ptr(), swkb->length())) ||
          g->get_mbr(&mbr, &c_end))))
    DBUG_RETURN(0);

  collector.set_extent(mbr.xmin, mbr.xmax, mbr.ymin, mbr.ymax);

  if (g->get_class_info()->m_type_id == Geometry::wkb_point)
    DBUG_RETURN(1);

  if (g->store_shapes(&trn))
    goto mem_error;

  collector.prepare_operation();
  scan_it.init(&collector);

  while (scan_it.more_points())
  {
    if (scan_it.step())
      goto mem_error;

    ev= scan_it.get_events();
    if (ev->simple_event())
      continue;

    if ((ev->event == scev_thread || ev->event == scev_single_point) &&
        !ev->get_next())
      continue;

    if (ev->event == scev_two_threads && !ev->get_next()->get_next())
      continue;

    result= 0;
    break;
  }

  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
mem_error:
  null_value= 1;
  DBUG_RETURN(0);
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
  const char *end;

  null_value= (!swkb || 
	       !(geom= Geometry::construct(&buffer,
                                           swkb->ptr(),
                                           swkb->length())) ||
	       geom->geom_length(&res, &end));
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


double Item_func_distance::val_real()
{
  bool cur_point_edge;
  const Gcalc_scan_iterator::point *evpos;
  const Gcalc_heap::Info *cur_point, *dist_point;
  const Gcalc_scan_iterator::event_point *ev;
  double t, distance, cur_distance;
  double x1, x2, y1, y2;
  double ex, ey, vx, vy, e_sqrlen;
  uint obj2_si;
  Gcalc_operation_transporter trn(&func, &collector);

  DBUG_ENTER("Item_func_distance::val_real");
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  MBR mbr1, mbr2;
  const char *c_end;


  if ((null_value= (args[0]->null_value || args[1]->null_value ||
          !(g1= Geometry::construct(&buffer1, res1->ptr(), res1->length())) ||
          !(g2= Geometry::construct(&buffer2, res2->ptr(), res2->length())) ||
          g1->get_mbr(&mbr1, &c_end) ||
          g2->get_mbr(&mbr2, &c_end))))
    goto mem_error;

  mbr1.add_mbr(&mbr2);
  collector.set_extent(mbr1.xmin, mbr1.xmax, mbr1.ymin, mbr1.ymax);

  if ((g1->get_class_info()->m_type_id == Geometry::wkb_point) &&
      (g2->get_class_info()->m_type_id == Geometry::wkb_point))
  {
    if (((Gis_point *) g1)->get_xy(&x1, &y1) ||
        ((Gis_point *) g2)->get_xy(&x2, &y2))
      goto mem_error;
    ex= x2 - x1;
    ey= y2 - y1;
    DBUG_RETURN(sqrt(ex * ex + ey * ey));
  }

  if (func.reserve_op_buffer(1))
    goto mem_error;
  func.add_operation(Gcalc_function::op_intersection, 2);

  if (g1->store_shapes(&trn))
    goto mem_error;
  obj2_si= func.get_nshapes();
  if (g2->store_shapes(&trn) || func.alloc_states())
    goto mem_error;

  if (obj2_si == 0 || func.get_nshapes() == obj2_si)
  {
    distance= 0.0;
    null_value= 1;
    goto exit;
  }


  collector.prepare_operation();
  scan_it.init(&collector);

  distance= DBL_MAX;
  while (scan_it.more_points())
  {
    if (scan_it.step())
      goto mem_error;
    evpos= scan_it.get_event_position();
    ev= scan_it.get_events();

    if (ev->simple_event())
    {
      cur_point= ev->pi;
      goto count_distance;
    }
    /*
       handling intersection we only need to check if it's the intersecion
       of objects 1 and 2. In this case distance is 0
    */
    cur_point= NULL;

    /*
       having these events we need to check for possible intersection
       of objects
       scev_thread | scev_two_threads | scev_single_point
    */
    func.clear_i_states();
    for (Gcalc_point_iterator pit(&scan_it); pit.point() != evpos; ++pit)
    {
      gcalc_shape_info si= pit.point()->get_shape();
      if ((func.get_shape_kind(si) == Gcalc_function::shape_polygon))
        func.invert_i_state(si);
    }

    func.clear_b_states();
    for (; ev; ev= ev->get_next())
    {
      if (ev->event != scev_intersection)
        cur_point= ev->pi;
      func.set_b_state(ev->get_shape());
      if (func.count())
      {
        /* Point of one object is inside the other - intersection found */
        distance= 0;
        goto exit;
      }
    }

    if (!cur_point)
      continue;

count_distance:
    if (cur_point->node.shape.shape >= obj2_si)
      continue;
    cur_point_edge= !cur_point->is_bottom();

    for (dist_point= collector.get_first(); dist_point; dist_point= dist_point->get_next())
    {
      /* We only check vertices of object 2 */
      if (dist_point->type != Gcalc_heap::nt_shape_node ||
          dist_point->node.shape.shape < obj2_si)
        continue;

      /* if we have an edge to check */
      if (dist_point->node.shape.left)
      {
        t= count_edge_t(dist_point, dist_point->node.shape.left, cur_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t>0.0) && (t<1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      if (cur_point_edge)
      {
        t= count_edge_t(cur_point, cur_point->node.shape.left, dist_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t>0.0) && (t<1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      cur_distance= distance_points(cur_point, dist_point);
      if (distance > cur_distance)
        distance= cur_distance;
    }
  }
exit:
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(distance);
mem_error:
  null_value= 1;
  DBUG_RETURN(0);
}


#endif /*HAVE_SPATIAL*/

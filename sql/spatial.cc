/*
   Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "sql_string.h"                         // String
#include "my_global.h"                          // REQUIRED for HAVE_* below
#include "gstream.h"                            // Gis_read_stream
#include "spatial.h"
#include <mysqld_error.h>

#ifdef HAVE_SPATIAL

/* 
  exponential notation :
  1   sign
  1   number before the decimal point
  1   decimal point
  14  number of significant digits (see String::qs_append(double))
  1   'e' sign
  1   exponent sign
  3   exponent digits
  ==
  22

  "f" notation :
  1   optional 0
  1   sign
  14  number significant digits (see String::qs_append(double) )
  1   decimal point
  ==
  17
*/

#define MAX_DIGITS_IN_DOUBLE 30

/***************************** Gis_class_info *******************************/

String Geometry::bad_geometry_data("Bad object", &my_charset_bin);

Geometry::Class_info *Geometry::ci_collection[Geometry::wkb_last+1]=
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static Geometry::Class_info **ci_collection_end=
                                Geometry::ci_collection+Geometry::wkb_last + 1;

Geometry::Class_info::Class_info(const char *name, int type_id,
                                 create_geom_t create_func):
  m_type_id(type_id), m_create_func(create_func)
{
  m_name.str= (char *) name;
  m_name.length= strlen(name);

  ci_collection[type_id]= this;
}

static Geometry *create_point(char *buffer)
{
  return new (buffer) Gis_point;
}

static Geometry *create_linestring(char *buffer)
{
  return new (buffer) Gis_line_string;
}

static Geometry *create_polygon(char *buffer)
{
  return new (buffer) Gis_polygon;
}

static Geometry *create_multipoint(char *buffer)
{
  return new (buffer) Gis_multi_point;
}

static Geometry *create_multipolygon(char *buffer)
{
  return new (buffer) Gis_multi_polygon;
}

static Geometry *create_multilinestring(char *buffer)
{
  return new (buffer) Gis_multi_line_string;
}

static Geometry *create_geometrycollection(char *buffer)
{
  return new (buffer) Gis_geometry_collection;
}



static Geometry::Class_info point_class("POINT",
					Geometry::wkb_point, create_point);

static Geometry::Class_info linestring_class("LINESTRING",
					     Geometry::wkb_linestring,
					     create_linestring);
static Geometry::Class_info polygon_class("POLYGON",
					      Geometry::wkb_polygon,
					      create_polygon);
static Geometry::Class_info multipoint_class("MULTIPOINT",
						 Geometry::wkb_multipoint,
						 create_multipoint);
static Geometry::Class_info 
multilinestring_class("MULTILINESTRING",
		      Geometry::wkb_multilinestring, create_multilinestring);
static Geometry::Class_info multipolygon_class("MULTIPOLYGON",
						   Geometry::wkb_multipolygon,
						   create_multipolygon);
static Geometry::Class_info 
geometrycollection_class("GEOMETRYCOLLECTION",Geometry::wkb_geometrycollection,
			 create_geometrycollection);

/***************************** Geometry *******************************/

Geometry::Class_info *Geometry::find_class(const char *name, uint32 len)
{
  for (Class_info **cur_rt= ci_collection;
       cur_rt < ci_collection_end; cur_rt++)
  {
    if (*cur_rt &&
	((*cur_rt)->m_name.length == len) &&
	(my_strnncoll(&my_charset_latin1,
		      (const uchar*) (*cur_rt)->m_name.str, len,
		      (const uchar*) name, len) == 0))
      return *cur_rt;
  }
  return 0;
}


Geometry *Geometry::create_by_typeid(Geometry_buffer *buffer, int type_id)
{
  Class_info *ci;
  if (!(ci= find_class(type_id)))
    return NULL;
  return (*ci->m_create_func)(buffer->data);
}


Geometry *Geometry::construct(Geometry_buffer *buffer,
                              const char *data, uint32 data_len)
{
  uint32 geom_type;
  Geometry *result;

  if (data_len < SRID_SIZE + WKB_HEADER_SIZE)   // < 4 + (1 + 4)
    return NULL;
  /* + 1 to skip the byte order (stored in position SRID_SIZE). */
  geom_type= uint4korr(data + SRID_SIZE + 1);
  if (!(result= create_by_typeid(buffer, (int) geom_type)))
    return NULL;
  result->set_data_ptr(data + SRID_SIZE + WKB_HEADER_SIZE,
                       data_len - SRID_SIZE - WKB_HEADER_SIZE);
  return result;
}


Geometry *Geometry::create_from_wkt(Geometry_buffer *buffer,
				    Gis_read_stream *trs, String *wkt,
				    bool init_stream)
{
  LEX_STRING name;
  Class_info *ci;

  if (trs->get_next_word(&name))
  {
    trs->set_error_msg("Geometry name expected");
    return NULL;
  }
  if (!(ci= find_class(name.str, name.length)) ||
      wkt->reserve(WKB_HEADER_SIZE, 512))
    return NULL;
  Geometry *result= (*ci->m_create_func)(buffer->data);
  wkt->q_append((char) wkb_ndr);
  wkt->q_append((uint32) result->get_class_info()->m_type_id);
  if (trs->check_next_symbol('(') ||
      result->init_from_wkt(trs, wkt) ||
      trs->check_next_symbol(')'))
    return NULL;
  if (init_stream)  
    result->set_data_ptr(wkt->ptr() + WKB_HEADER_SIZE,
                         wkt->length() - WKB_HEADER_SIZE);
  return result;
}


static double wkb_get_double(const char *ptr, Geometry::wkbByteOrder bo)
{
  double res;
  if (bo != Geometry::wkb_xdr)
  {
    float8get(res, ptr);
  }
  else
  {
    char inv_array[8];
    inv_array[0]= ptr[7];
    inv_array[1]= ptr[6];
    inv_array[2]= ptr[5];
    inv_array[3]= ptr[4];
    inv_array[4]= ptr[3];
    inv_array[5]= ptr[2];
    inv_array[6]= ptr[1];
    inv_array[7]= ptr[0];
    float8get(res, inv_array);
  }
  return res;
}


static uint32 wkb_get_uint(const char *ptr, Geometry::wkbByteOrder bo)
{
  if (bo != Geometry::wkb_xdr)
    return uint4korr(ptr);
  /* else */
  {
    char inv_array[4], *inv_array_p= inv_array;
    inv_array[0]= ptr[3];
    inv_array[1]= ptr[2];
    inv_array[2]= ptr[1];
    inv_array[3]= ptr[0];
    return uint4korr(inv_array_p);
  }
}


Geometry *Geometry::create_from_wkb(Geometry_buffer *buffer,
                                    const char *wkb, uint32 len, String *res)
{
  uint32 geom_type;
  Geometry *geom;

  if (len < WKB_HEADER_SIZE)
    return NULL;
  geom_type= wkb_get_uint(wkb+1, (wkbByteOrder)wkb[0]);
  if (!(geom= create_by_typeid(buffer, (int) geom_type)) ||
      res->reserve(WKB_HEADER_SIZE, 512))
    return NULL;

  res->q_append((char) wkb_ndr);
  res->q_append(geom_type);

  return geom->init_from_wkb(wkb + WKB_HEADER_SIZE, len - WKB_HEADER_SIZE,
                             (wkbByteOrder) wkb[0], res) ? geom : NULL;
}


int Geometry::create_from_opresult(Geometry_buffer *g_buf,
                                   String *res, Gcalc_result_receiver &rr)
{
  uint32 geom_type= rr.get_result_typeid();
  Geometry *obj= create_by_typeid(g_buf, geom_type);

  if (!obj || res->reserve(WKB_HEADER_SIZE, 512))
    return 1;

  res->q_append((char) wkb_ndr);
  res->q_append(geom_type);
  return obj->init_from_opresult(res, rr.result(), rr.length());
}


bool Geometry::envelope(String *result) const
{
  MBR mbr;
  wkb_parser wkb(&m_wkb_data);

  if (get_mbr(&mbr, &wkb) ||
      result->reserve(1 + 4 * 3 + SIZEOF_STORED_DOUBLE * 10))
    return true;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_polygon);
  result->q_append((uint32) 1);
  result->q_append((uint32) 5);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymin);
  result->q_append(mbr.xmax);
  result->q_append(mbr.ymin);
  result->q_append(mbr.xmax);
  result->q_append(mbr.ymax);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymax);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymin);

  return false;
}


/**
  Create a point from data.

  @param OUT result   Put result here
  @param wkb          Data for point is here.

  @return             false on success, true on error
*/

bool Geometry::create_point(String *result, wkb_parser *wkb) const
{
  if (wkb->no_data(POINT_DATA_SIZE) ||
      result->reserve(WKB_HEADER_SIZE + POINT_DATA_SIZE))
    return true;
  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_point);
  /* Copy two double in same format */
  result->q_append(wkb->data(), POINT_DATA_SIZE);
  return false;
}

/**
  Create a point from coordinates.

  @param OUT result
  @param x  x coordinate for point
  @param y  y coordinate for point
 
  @return  false on success, true on error
*/

bool Geometry::create_point(String *result, point_xy p) const
{
  if (result->reserve(1 + 4 + POINT_DATA_SIZE))
    return true;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_point);
  result->q_append(p.x);
  result->q_append(p.y);
  return false;
 }

/**
  Append N points from packed format to text

  @param OUT txt        Append points here
  @param     n_points   Number of points  
  @param     wkb        Packed data
  @param     offset     Offset between points
*/

void Geometry::append_points(String *txt, uint32 n_points,
                             wkb_parser *wkb, uint32 offset) const
{			     
  while (n_points--)
  {
    point_xy p;
    wkb->skip_unsafe(offset);
    wkb->scan_xy_unsafe(&p); 
    txt->qs_append(p.x);
    txt->qs_append(' ');
    txt->qs_append(p.y);
    txt->qs_append(',');
  }
}


/**
  Get most bounding rectangle (mbr) for X points

  @param OUT mbr      Result
  @param wkb          Data for point is here.
  @param offset       Offset between points
  
  @return             false on success, true on error
*/

bool Geometry::get_mbr_for_points(MBR *mbr, wkb_parser *wkb,
                                  uint offset) const
{
  uint32 n_points;

  if (wkb->scan_n_points_and_check_data(&n_points, offset))
    return true;

  /* Calculate MBR for points */
  while (n_points--)
  {
    wkb->skip_unsafe(offset);
    mbr->add_xy(wkb->data(), wkb->data() + SIZEOF_STORED_DOUBLE);
    wkb->skip_unsafe(POINT_DATA_SIZE);
  }
  return false;
}


int Geometry::collection_store_shapes(Gcalc_shape_transporter *trn,
                                      Gcalc_shape_status *st,
                                      Geometry *collection_item) const
{
  uint32 n_objects;
  wkb_parser wkb(&m_wkb_data);
  Geometry_buffer buffer;

  if (wkb.scan_non_zero_uint4(&n_objects) ||
      trn->start_collection(st, n_objects))
    return 1;

  while (n_objects--)
  {
    Geometry *geom;
    if (!(geom= collection_item))
    {
      /*
        Item type is not known in advance, e.g. GeometryCollection.
        Create an item object in every iteration,
        according to the current wkb type.
      */
      if (!(geom= scan_header_and_create(&wkb, &buffer)))
        return 1;
    }
    else
    {
      if (wkb.skip_wkb_header())
        return 1;
      geom->set_data_ptr(&wkb);
    }
    Gcalc_shape_status item_status;
    if (geom->store_shapes(trn, &item_status) ||
        trn->collection_add_item(st, &item_status))
      return 1;
    wkb.skip_unsafe(geom->get_data_size());
  }
  trn->complete_collection(st);
  return 0;
}


bool Geometry::collection_area(double *ar, wkb_parser *wkb,
                               Geometry *collection_item) const
{
  uint32 n_objects;
  Geometry_buffer buffer;

  if (wkb->scan_non_zero_uint4(&n_objects))
    return true;

  for (*ar= 0; n_objects; n_objects--)
  {
    Geometry *geom;
    if (!(geom= collection_item))
    {
      /*
        Item type is not known in advance, e.g. GeometryCollection.
        Create an item object according to the wkb type.
      */
      if (!(geom= scan_header_and_create(wkb, &buffer)))
        return true;
    }   
    else
    {
      if (wkb->skip_wkb_header())
        return true;
      geom->set_data_ptr(wkb);
    }
    double item_area;
    if (geom->area(&item_area, wkb))
      return true;
    *ar+= item_area;
  }
  return false;
}


uint Geometry::collection_init_from_opresult(String *bin,
                                             const char *opres,
                                             uint opres_length,
                                             Geometry *collection_item)
{
  Geometry_buffer buffer;
  const char *opres_orig= opres;
  int n_items_offs= bin->length();
  uint n_items= 0;

  if (bin->reserve(4, 512))
    return 0;
  bin->q_append((uint32) 0);

  while (opres_length)
  {
    int item_len;

    if (bin->reserve(WKB_HEADER_SIZE, 512))
      return 0;

    Geometry *item;
    if (collection_item)
    {
      /*
        MultiPoint, MultiLineString, or MultiPolygon pass
        a pre-created collection item. Let's use it.
      */
      item= collection_item;
    }
    else
    {
      /*
       GeometryCollection passes NULL. Let's create an item
       according to wkb_type on every interation step.
      */
      uint32 wkb_type;
      switch ((Gcalc_function::shape_type) uint4korr(opres))   
      {
        case Gcalc_function::shape_point:   wkb_type= wkb_point; break;
        case Gcalc_function::shape_line:    wkb_type= wkb_linestring; break;
        case Gcalc_function::shape_polygon: wkb_type= wkb_polygon; break;
        default: 
          /*
            Something has gone really wrong while performing a spatial operation.
            For now we'll return an error.
            TODO: should be properly fixed.
          */
          my_error(ER_NOT_SUPPORTED_YET, MYF(0), "spatial self-intersecting operands");
          return 0;
      };
      if (!(item= create_by_typeid(&buffer, wkb_type)))
        return 0;
    }

    bin->q_append((char) wkb_ndr);
    bin->q_append((uint32) item->get_class_info()->m_type_id);

    if (!(item_len= item->init_from_opresult(bin, opres, opres_length)))
      return 0;
    opres+= item_len;
    opres_length-= item_len;
    n_items++;
  }
  bin->write_at_position(n_items_offs, n_items);
  return (uint) (opres - opres_orig);
}


/***************************** Point *******************************/

uint32 Gis_point::get_data_size() const
{
  return POINT_DATA_SIZE;
}


bool Gis_point::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  double x, y;
  if (trs->get_next_number(&x) || trs->get_next_number(&y) ||
      wkb->reserve(POINT_DATA_SIZE))
    return true;
  wkb->q_append(x);
  wkb->q_append(y);
  return false;
}


uint Gis_point::init_from_wkb(const char *wkb, uint len,
                              wkbByteOrder bo, String *res)
{
  double x, y;
  if (len < POINT_DATA_SIZE || res->reserve(POINT_DATA_SIZE))
    return 0;
  x= wkb_get_double(wkb, bo);
  y= wkb_get_double(wkb + SIZEOF_STORED_DOUBLE, bo);
  res->q_append(x);
  res->q_append(y);
  return POINT_DATA_SIZE;
}


bool Gis_point::get_data_as_wkt(String *txt, wkb_parser *wkb) const
{
  point_xy p;
  if (wkb->scan_xy(&p))
    return true;
  if (txt->reserve(MAX_DIGITS_IN_DOUBLE * 2 + 1))
    return true;
  txt->qs_append(p.x);
  txt->qs_append(' ');
  txt->qs_append(p.y);
  return false;
}


bool Gis_point::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  point_xy p;
  if (wkb->scan_xy(&p))
    return true;
  mbr->add_xy(p);
  return false;
}


int Gis_point::store_shapes(Gcalc_shape_transporter *trn,
                            Gcalc_shape_status *st) const
{
  if (trn->skip_point())
    return 0;
  wkb_parser wkb(&m_wkb_data);
  point_xy p;
  return wkb.scan_xy(&p) || trn->single_point(st, p.x, p.y);
}


const Geometry::Class_info *Gis_point::get_class_info() const
{
  return &point_class;
}


/***************************** LineString *******************************/

uint32 Gis_line_string::get_data_size() const 
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);
  if (wkb.scan_n_points_and_check_data(&n_points))
    return GET_SIZE_ERROR;

  return 4 + n_points * POINT_DATA_SIZE;
}


bool Gis_line_string::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_points= 0;
  uint32 np_pos= wkb->length();
  Gis_point p;

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points  

  for (;;)
  {
    if (p.init_from_wkt(trs, wkb))
      return true;
    n_points++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  if (n_points < 1)
  {
    trs->set_error_msg("Too few points in LINESTRING");
    return true;
  }
  wkb->write_at_position(np_pos, n_points);
  return false;
}


uint Gis_line_string::init_from_wkb(const char *wkb, uint len,
                                    wkbByteOrder bo, String *res)
{
  uint32 n_points, proper_length;
  const char *wkb_end;
  Gis_point p;

  if (len < 4 ||
      (n_points= wkb_get_uint(wkb, bo)) < 1 ||
      n_points > max_n_points)
    return 0;
  proper_length= 4 + n_points * POINT_DATA_SIZE;

  if (len < proper_length || res->reserve(proper_length))
    return 0;

  res->q_append(n_points);
  wkb_end= wkb + proper_length;
  for (wkb+= 4; wkb<wkb_end; wkb+= POINT_DATA_SIZE)
  {
    if (!p.init_from_wkb(wkb, POINT_DATA_SIZE, bo, res))
      return 0;
  }

  return proper_length;
}


bool Gis_line_string::get_data_as_wkt(String *txt, wkb_parser *wkb) const
{
  uint32 n_points;
  if (wkb->scan_n_points_and_check_data(&n_points) ||
      txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
    return true;

  while (n_points--)
  {
    point_xy p;
    wkb->scan_xy_unsafe(&p);
    txt->qs_append(p.x);
    txt->qs_append(' ');
    txt->qs_append(p.y);
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);		// Remove end ','
  return false;
}


bool Gis_line_string::get_mbr(MBR *mbr, wkb_parser *wkb) const 
{
  return get_mbr_for_points(mbr, wkb, 0);
}


int Gis_line_string::geom_length(double *len) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);

  *len= 0;					// In case of errors
  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;

  point_xy prev;
  wkb.scan_xy_unsafe(&prev);
  while (--n_points)
  {
    point_xy p;
    wkb.scan_xy_unsafe(&p);
    *len+= prev.distance(p);
    prev= p;
  }
  return 0;
}


int Gis_line_string::is_closed(int *closed) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;

  if (n_points == 1)
  {
    *closed=1;
    return 0;
  }

  point_xy p1, p2;

  /* Get first point. */
  wkb.scan_xy_unsafe(&p1); 

  /* Get last point. */
  wkb.skip_unsafe((n_points - 2) * POINT_DATA_SIZE);
  wkb.scan_xy_unsafe(&p2);

  *closed= p1.eq(p2);
  return 0;
}


int Gis_line_string::num_points(uint32 *n_points) const
{
  wkb_parser wkb(&m_wkb_data);
  return wkb.scan_uint4(n_points) ? 1 : 0;
}


int Gis_line_string::start_point(String *result) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);
  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;
  return create_point(result, &wkb);
}


int Gis_line_string::end_point(String *result) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);
  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;
  wkb.skip_unsafe((n_points - 1) * POINT_DATA_SIZE);
  return create_point(result, &wkb);
}


int Gis_line_string::point_n(uint32 num, String *result) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);
  if (num < 1 ||
      wkb.scan_n_points_and_check_data(&n_points) ||
      num > n_points)
    return 1;
  wkb.skip_unsafe((num - 1) * POINT_DATA_SIZE);
  return create_point(result, &wkb);
}


int Gis_line_string::store_shapes(Gcalc_shape_transporter *trn,
                                  Gcalc_shape_status *st) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);

  if (trn->skip_line_string())
    return 0;

  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;

  trn->start_line(st);
  while (n_points--)
  {
    point_xy p;
    wkb.scan_xy_unsafe(&p);
    if (trn->add_point(st, p.x, p.y))
      return 1;
  }
  return trn->complete_line(st);
}


const Geometry::Class_info *Gis_line_string::get_class_info() const
{
  return &linestring_class;
}


/***************************** Polygon *******************************/

uint32 Gis_polygon::get_data_size() const 
{
  uint32 n_linear_rings;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_linear_rings))
    return GET_SIZE_ERROR;

  while (n_linear_rings--)
  {
    uint32 n_points;
    if (wkb.scan_n_points_and_check_data(&n_points))
      return GET_SIZE_ERROR;
    wkb.skip_unsafe(n_points * POINT_DATA_SIZE);
  }
  return (uint32) (wkb.data() - m_wkb_data.data());
}


bool Gis_polygon::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_linear_rings= 0;
  uint32 lr_pos= wkb->length();
  int closed;

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points
  for (;;)  
  {
    Gis_line_string ls;
    uint32 ls_pos=wkb->length();
    if (trs->check_next_symbol('(') ||
	ls.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return true;

    ls.set_data_ptr(wkb->ptr() + ls_pos, wkb->length() - ls_pos);
    if (ls.is_closed(&closed) || !closed)
    {
      trs->set_error_msg("POLYGON's linear ring isn't closed");
      return true;
    }
    n_linear_rings++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(lr_pos, n_linear_rings);
  return false;
}


uint Gis_polygon::init_from_opresult(String *bin,
                                     const char *opres, uint opres_length)
{
  const char *opres_orig= opres;
  const char *opres_end= opres + opres_length;
  uint32 position= bin->length();
  uint32 poly_shapes= 0;

  if (bin->reserve(4, 512))
    return 0;
  bin->q_append(poly_shapes);

  while (opres < opres_end)
  {
    uint32 n_points, proper_length;
    const char *op_end, *p1_position;
    Gis_point p;
    Gcalc_function::shape_type st;

    st= (Gcalc_function::shape_type) uint4korr(opres);
    if (poly_shapes && st != Gcalc_function::shape_hole)
      break;
    poly_shapes++;
    n_points= uint4korr(opres + 4) + 1; /* skip shape type id */
    proper_length= 4 + n_points * POINT_DATA_SIZE;

    if (bin->reserve(proper_length, 512))
      return 0;

    bin->q_append(n_points);
    op_end= opres + 8 + (n_points-1) * 8 * 2;
    p1_position= (opres+= 8);
    for (; opres<op_end; opres+= POINT_DATA_SIZE)
    {
      if (!p.init_from_wkb(opres, POINT_DATA_SIZE, wkb_ndr, bin))
        return 0;
    }
    if (!p.init_from_wkb(p1_position, POINT_DATA_SIZE, wkb_ndr, bin))
      return 0;
  }

  bin->write_at_position(position, poly_shapes);

  return (uint) (opres - opres_orig);
}


uint Gis_polygon::init_from_wkb(const char *wkb, uint len, wkbByteOrder bo,
                                String *res)
{
  uint32 n_linear_rings;
  const char *wkb_orig= wkb;

  if (len < 4)
    return 0;

  if (0 == (n_linear_rings= wkb_get_uint(wkb, bo)) ||
      res->reserve(4, 512))
    return 0;
  wkb+= 4;
  len-= 4;
  res->q_append(n_linear_rings);

  while (n_linear_rings--)
  {
    Gis_line_string ls;
    uint32 ls_pos= res->length();
    int ls_len;
    int closed;

    if (!(ls_len= ls.init_from_wkb(wkb, len, bo, res)))
      return 0;

    ls.set_data_ptr(res->ptr() + ls_pos, res->length() - ls_pos);

    if (ls.is_closed(&closed) || !closed)
      return 0;
    wkb+= ls_len;
  }

  return (uint) (wkb - wkb_orig);
}


bool Gis_polygon::get_data_as_wkt(String *txt, wkb_parser *wkb) const
{
  uint32 n_linear_rings;

  if (wkb->scan_non_zero_uint4(&n_linear_rings)) 
    return true;

  while (n_linear_rings--)
  {
    uint32 n_points;
    if (wkb->scan_n_points_and_check_data(&n_points) ||
	txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return true;
    txt->qs_append('(');
    append_points(txt, n_points, wkb, 0);
    (*txt) [txt->length() - 1]= ')';		// Replace end ','
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);		// Remove end ','
  return false;
}


bool Gis_polygon::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  uint32 n_linear_rings;

  if (wkb->scan_non_zero_uint4(&n_linear_rings))
    return true;

  while (n_linear_rings--)
  {
    if (get_mbr_for_points(mbr, wkb, 0))
      return true;
  }
  return false;
}


bool Gis_polygon::area(double *ar, wkb_parser *wkb) const
{
  uint32 n_linear_rings;
  double result= -1.0;

  if (wkb->scan_non_zero_uint4(&n_linear_rings))
    return true;

  while (n_linear_rings--)
  {
    double lr_area= 0;
    uint32 n_points;

    if (wkb->scan_n_points_and_check_data(&n_points))
      return true;
    point_xy prev;
    wkb->scan_xy_unsafe(&prev);

    while (--n_points)				// One point is already read
    {
      point_xy p;
      wkb->scan_xy_unsafe(&p);
      lr_area+= (prev.x + p.x) * (prev.y - p.y);
      prev= p;
    }
    lr_area= fabs(lr_area)/2;
    if (result == -1.0)
      result= lr_area;
    else
      result-= lr_area;
  }
  *ar= fabs(result);
  return false;
}


int Gis_polygon::exterior_ring(String *result) const
{
  uint32 n_points, n_linear_rings, length;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_linear_rings) ||
      wkb.scan_n_points_and_check_data(&n_points))
    return 1;
  length= n_points * POINT_DATA_SIZE;
  if (result->reserve(1 + 4 + 4 + length))
    return 1;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_linestring);
  result->q_append(n_points);
  result->q_append(wkb.data(), length);
  return 0;
}


int Gis_polygon::num_interior_ring(uint32 *n_int_rings) const
{
  wkb_parser wkb(&m_wkb_data);
  if (wkb.scan_non_zero_uint4(n_int_rings))
    return 1;
  *n_int_rings-= 1;
  return 0;
}


int Gis_polygon::interior_ring_n(uint32 num, String *result) const
{
  wkb_parser wkb(&m_wkb_data);
  uint32 n_linear_rings;
  uint32 n_points;
  uint32 points_size;

  if (num < 1 ||
      wkb.scan_non_zero_uint4(&n_linear_rings) ||
      num >= n_linear_rings)
    return 1;

  while (num--)
  {
    if (wkb.scan_n_points_and_check_data(&n_points))
      return 1;
    wkb.skip_unsafe(n_points * POINT_DATA_SIZE);
  }
  if (wkb.scan_n_points_and_check_data(&n_points))
    return 1;
  points_size= n_points * POINT_DATA_SIZE;
  if (result->reserve(1 + 4 + 4 + points_size))
    return 1;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_linestring);
  result->q_append(n_points);
  result->q_append(wkb.data(), points_size);
  return 0;
}


bool Gis_polygon::centroid_xy(point_xy *p) const
{
  uint32 n_linear_rings;
  double UNINIT_VAR(res_area);
  point_xy res(0, 0);              // Initialized only to make compiler happy
  wkb_parser wkb(&m_wkb_data);
  bool first_loop= 1;

  if (wkb.scan_non_zero_uint4(&n_linear_rings))
    return true;

  while (n_linear_rings--)
  {
    uint32 n_points, org_n_points;
    double cur_area= 0;
    point_xy prev, cur(0, 0);

   if (wkb.scan_n_points_and_check_data(&n_points))
     return true;

    org_n_points= n_points - 1;
    wkb.scan_xy_unsafe(&prev);

    while (--n_points)				// One point is already read
    {
      point_xy tmp;  
      wkb.scan_xy_unsafe(&tmp);
      cur_area+= (prev.x + tmp.x) * (prev.y - tmp.y);
      cur.x+= tmp.x;
      cur.y+= tmp.y;
      prev= tmp;
    }
    cur_area= fabs(cur_area) / 2;
    cur.x= cur.x / org_n_points;
    cur.y= cur.y / org_n_points;
 
    if (!first_loop)
    {
      double d_area= fabs(res_area - cur_area);
      res.x= (res_area * res.x - cur_area * cur.x) / d_area;   
      res.y= (res_area * res.y - cur_area * cur.y) / d_area;   
    }
    else
    {
      first_loop= 0;
      res_area= cur_area;
      res= cur;
    }
  }

  *p= res;   
  return false;
}


int Gis_polygon::centroid(String *result) const
{
  point_xy p;
  if (centroid_xy(&p))
    return 1;
  return create_point(result, p);
}


int Gis_polygon::store_shapes(Gcalc_shape_transporter *trn,
                              Gcalc_shape_status *st) const
{
  uint32 n_linear_rings;
  wkb_parser wkb(&m_wkb_data);

  if (trn->skip_poly())
    return 0;

  if (trn->start_poly(st))
    return 1;

  if (wkb.scan_non_zero_uint4(&n_linear_rings))
    return 1;

  while (n_linear_rings--)
  {
    uint32 n_points;

    if (wkb.scan_n_points_and_check_data(&n_points))
      return 1;

    trn->start_ring(st);
    while (--n_points)
    {
      point_xy p;
      wkb.scan_xy_unsafe(&p);
      if (trn->add_point(st, p.x, p.y))
        return 1;
    }
    wkb.skip_unsafe(POINT_DATA_SIZE); // Skip the last point in the ring.
    trn->complete_ring(st);
  }

  trn->complete_poly(st);
  return 0;
}


const Geometry::Class_info *Gis_polygon::get_class_info() const
{
  return &polygon_class;
}


/***************************** MultiPoint *******************************/

uint32 Gis_multi_point::get_data_size() const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);
  if (wkb.scan_n_points_and_check_data(&n_points, WKB_HEADER_SIZE))
    return GET_SIZE_ERROR;

  return 4 + n_points * (POINT_DATA_SIZE + WKB_HEADER_SIZE);
}


bool Gis_multi_point::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_points= 0;
  uint32 np_pos= wkb->length();
  Gis_point p;

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)
  {
    if (wkb->reserve(1 + 4, 512))
      return 1;
    wkb->q_append((char) wkb_ndr);
    wkb->q_append((uint32) wkb_point);
    if (p.init_from_wkt(trs, wkb))
      return true;
    n_points++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(np_pos, n_points);	// Store number of found points
  return false;
}


uint Gis_multi_point::init_from_opresult(String *bin,
                                         const char *opres, uint opres_length)
{
  uint bin_size, n_points;
  Gis_point p;
  const char *opres_end;

  n_points= opres_length / (4 + 8 * 2);
  bin_size= n_points * (WKB_HEADER_SIZE + POINT_DATA_SIZE) + 4;
 
  if (bin->reserve(bin_size, 512))
    return 0;
    
  bin->q_append(n_points);
  opres_end= opres + opres_length;
  for (; opres < opres_end; opres+= (4 + 8*2))
  {
    bin->q_append((char)wkb_ndr);
    bin->q_append((uint32)wkb_point);
    if (!p.init_from_wkb(opres + 4, POINT_DATA_SIZE, wkb_ndr, bin))
      return 0;
  }
  return opres_length;
}


uint Gis_multi_point::init_from_wkb(const char *wkb, uint len, wkbByteOrder bo,
                                    String *res)
{
  uint32 n_points;
  uint proper_size;
  Gis_point p;
  const char *wkb_end;

  if (len < 4 ||
      (n_points= wkb_get_uint(wkb, bo)) > max_n_points)
    return 0;
  proper_size= 4 + n_points * (WKB_HEADER_SIZE + POINT_DATA_SIZE);
 
  if (len < proper_size || res->reserve(proper_size))
    return 0;
    
  res->q_append(n_points);
  wkb_end= wkb + proper_size;
  for (wkb+=4; wkb < wkb_end; wkb+= (WKB_HEADER_SIZE + POINT_DATA_SIZE))
  {
    res->q_append((char)wkb_ndr);
    res->q_append((uint32)wkb_point);
    if (!p.init_from_wkb(wkb + WKB_HEADER_SIZE,
                         POINT_DATA_SIZE, (wkbByteOrder) wkb[0], res))
      return 0;
  }
  return proper_size;
}


bool Gis_multi_point::get_data_as_wkt(String *txt, wkb_parser *wkb) const 
{
  uint32 n_points;

  if (wkb->scan_n_points_and_check_data(&n_points, WKB_HEADER_SIZE) ||
      txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
    return true;

  append_points(txt, n_points, wkb, WKB_HEADER_SIZE);
  txt->length(txt->length()-1);			// Remove end ','
  return false;
}


bool Gis_multi_point::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  return get_mbr_for_points(mbr, wkb, WKB_HEADER_SIZE);
}


int Gis_multi_point::num_geometries(uint32 *num) const
{
  wkb_parser wkb(&m_wkb_data);
  return wkb.scan_non_zero_uint4(num) ? 1 : 0;
}


int Gis_multi_point::geometry_n(uint32 num, String *result) const
{
  uint32 n_points;
  wkb_parser wkb(&m_wkb_data);

  if (num < 1 ||
      wkb.scan_n_points_and_check_data(&n_points, WKB_HEADER_SIZE) ||
      num > n_points ||
      result->reserve(WKB_HEADER_SIZE + POINT_DATA_SIZE))
    return 1;
  wkb.skip_unsafe((num - 1) * (WKB_HEADER_SIZE + POINT_DATA_SIZE));

  result->q_append(wkb.data(), WKB_HEADER_SIZE + POINT_DATA_SIZE);
  return 0;
}


int Gis_multi_point::store_shapes(Gcalc_shape_transporter *trn,
                                  Gcalc_shape_status *st) const
{
  if (trn->skip_point())
    return 0;
  Gis_point pt;
  return collection_store_shapes(trn, st, &pt);
}


const Geometry::Class_info *Gis_multi_point::get_class_info() const
{
  return &multipoint_class;
}


/***************************** MultiLineString *******************************/

uint32 Gis_multi_line_string::get_data_size() const 
{
  uint32 n_line_strings;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_line_strings))
    return GET_SIZE_ERROR;

  while (n_line_strings--)
  {
    uint32 n_points;

    if (wkb.skip_wkb_header() ||
        wkb.scan_n_points_and_check_data(&n_points))
      return GET_SIZE_ERROR;

    wkb.skip_unsafe(n_points * POINT_DATA_SIZE);
  }
  return (uint32) (wkb.data() - m_wkb_data.data());
}


bool Gis_multi_line_string::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_line_strings= 0;
  uint32 ls_pos= wkb->length();

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points
  
  for (;;)
  {
    Gis_line_string ls;

    if (wkb->reserve(1 + 4, 512))
      return true;
    wkb->q_append((char) wkb_ndr); wkb->q_append((uint32) wkb_linestring);

    if (trs->check_next_symbol('(') ||
	ls.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return true;
    n_line_strings++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(ls_pos, n_line_strings);
  return false;
}


uint Gis_multi_line_string::init_from_opresult(String *bin,
                                               const char *opres,
                                               uint opres_length)
{
  Gis_line_string item; 
  return collection_init_from_opresult(bin, opres, opres_length, &item);
}


uint Gis_multi_line_string::init_from_wkb(const char *wkb, uint len,
                                          wkbByteOrder bo, String *res)
{
  uint32 n_line_strings;
  const char *wkb_orig= wkb;

  if (len < 4 ||
      (n_line_strings= wkb_get_uint(wkb, bo))< 1)
    return 0;

  if (res->reserve(4, 512))
    return 0;
  res->q_append(n_line_strings);
  
  wkb+= 4;
  while (n_line_strings--)
  {
    Gis_line_string ls;
    int ls_len;

    if ((len < WKB_HEADER_SIZE) ||
        res->reserve(WKB_HEADER_SIZE, 512))
      return 0;

    res->q_append((char) wkb_ndr);
    res->q_append((uint32) wkb_linestring);

    if (!(ls_len= ls.init_from_wkb(wkb + WKB_HEADER_SIZE, len,
                                   (wkbByteOrder) wkb[0], res)))
      return 0;
    ls_len+= WKB_HEADER_SIZE;;
    wkb+= ls_len;
    len-= ls_len;
  }
  return (uint) (wkb - wkb_orig);
}


bool Gis_multi_line_string::get_data_as_wkt(String *txt, wkb_parser *wkb) const
{
  uint32 n_line_strings;

  if (wkb->scan_non_zero_uint4(&n_line_strings))
    return true;

  while (n_line_strings--)
  {
    uint32 n_points;
    
    if (wkb->skip_wkb_header() ||
        wkb->scan_n_points_and_check_data(&n_points) ||
	txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return true;
    txt->qs_append('(');
    append_points(txt, n_points, wkb, 0);
    (*txt) [txt->length() - 1]= ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return false;
}


bool Gis_multi_line_string::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  uint32 n_line_strings;

  if (wkb->scan_non_zero_uint4(&n_line_strings))
    return true;

  while (n_line_strings--)
  {
    if (wkb->skip_wkb_header() ||
        get_mbr_for_points(mbr, wkb, 0))
      return true;
  }
  return false;
}


int Gis_multi_line_string::num_geometries(uint32 *num) const
{
  wkb_parser wkb(&m_wkb_data);
  return wkb.scan_non_zero_uint4(num) ? 1 : 0;
}


int Gis_multi_line_string::geometry_n(uint32 num, String *result) const
{
  uint32 n_line_strings, n_points, length;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_line_strings))
    return 1;

  if ((num > n_line_strings) || (num < 1))
    return 1;

  for (;;)
  {
    if (wkb.skip_wkb_header() ||
        wkb.scan_n_points_and_check_data(&n_points))
      return 1;
    length= POINT_DATA_SIZE * n_points;
    if (!--num)
      break;
    wkb.skip_unsafe(length);
  }
  return result->append(wkb.data() - 4 - WKB_HEADER_SIZE,
                        length + 4 + WKB_HEADER_SIZE, (uint32) 0);
}


int Gis_multi_line_string::geom_length(double *len) const
{
  uint32 n_line_strings;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_line_strings))
    return 1;

  *len=0;
  while (n_line_strings--)
  {
    double ls_len;
    Gis_line_string ls;
    if (wkb.skip_wkb_header())
      return 1;
    ls.set_data_ptr(&wkb);
    if (ls.geom_length(&ls_len))
      return 1;
    *len+= ls_len;
    /*
      We know here that ls was ok, so we can call the trivial function
      Gis_line_string::get_data_size without error checking
    */
    wkb.skip_unsafe(ls.get_data_size());
  }
  return 0;
}


int Gis_multi_line_string::is_closed(int *closed) const
{
  uint32 n_line_strings;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_line_strings))
    return 1;

  while (n_line_strings--)
  {
    Gis_line_string ls;
    if (wkb.skip_wkb_header())
      return 1;
    ls.set_data_ptr(&wkb); 
    if (ls.is_closed(closed))
      return 1;
    if (!*closed)
      return 0;
    wkb.skip_unsafe(ls.get_data_size());
  }
  return 0;
}


int Gis_multi_line_string::store_shapes(Gcalc_shape_transporter *trn,
                                        Gcalc_shape_status *st) const
{
  if (trn->skip_line_string())
    return 0;
  Gis_line_string ls;
  return collection_store_shapes(trn, st, &ls);
}


const Geometry::Class_info *Gis_multi_line_string::get_class_info() const
{
  return &multilinestring_class;
}


/***************************** MultiPolygon *******************************/

uint32 Gis_multi_polygon::get_data_size() const 
{
  uint32 n_polygons;
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_polygons))
    return GET_SIZE_ERROR;

  while (n_polygons--)
  {
    uint32 n_linear_rings;
    if (wkb.skip_wkb_header() ||
        wkb.scan_non_zero_uint4(&n_linear_rings))
      return GET_SIZE_ERROR;

    while (n_linear_rings--)
    {
      uint32 n_points;

      if (wkb.scan_n_points_and_check_data(&n_points))
        return GET_SIZE_ERROR;

      wkb.skip_unsafe(n_points * POINT_DATA_SIZE);
    }
  }
  return (uint32) (wkb.data() - m_wkb_data.data());
}


bool Gis_multi_polygon::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_polygons= 0;
  int np_pos= wkb->length();
  Gis_polygon p;

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)  
  {
    if (wkb->reserve(1 + 4, 512))
      return true;
    wkb->q_append((char) wkb_ndr);
    wkb->q_append((uint32) wkb_polygon);

    if (trs->check_next_symbol('(') ||
	p.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return true;
    n_polygons++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(np_pos, n_polygons);
  return false;
}


uint Gis_multi_polygon::init_from_wkb(const char *wkb, uint len,
                                      wkbByteOrder bo, String *res)
{
  uint32 n_poly;
  const char *wkb_orig= wkb;

  if (len < 4)
    return 0;
  n_poly= wkb_get_uint(wkb, bo);

  if (res->reserve(4, 512))
    return 0;
  res->q_append(n_poly);
  
  wkb+=4;
  while (n_poly--)
  {
    Gis_polygon p;
    int p_len;

    if (len < WKB_HEADER_SIZE ||
        res->reserve(WKB_HEADER_SIZE, 512))
      return 0;
    res->q_append((char) wkb_ndr);
    res->q_append((uint32) wkb_polygon);

    if (!(p_len= p.init_from_wkb(wkb + WKB_HEADER_SIZE, len,
                                 (wkbByteOrder) wkb[0], res)))
      return 0;
    p_len+= WKB_HEADER_SIZE;
    wkb+= p_len;
    len-= p_len;
  }
  return (uint) (wkb - wkb_orig);
}


uint Gis_multi_polygon::init_from_opresult(String *bin,
                                           const char *opres, uint opres_length)
{
  Gis_polygon item;
  return collection_init_from_opresult(bin, opres, opres_length, &item);
}


bool Gis_multi_polygon::get_data_as_wkt(String *txt, wkb_parser *wkb) const 
{
  uint32 n_polygons;

  if (wkb->scan_non_zero_uint4(&n_polygons))
    return true;

  while (n_polygons--)
  {
    uint32 n_linear_rings;

    if (wkb->skip_wkb_header() ||
        wkb->scan_non_zero_uint4(&n_linear_rings) ||
        txt->reserve(1, 512))
      return true;
    txt->q_append('(');

    while (n_linear_rings--)
    {
      uint32 n_points;
      if (wkb->scan_n_points_and_check_data(&n_points) ||
          txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points, 512))
	return true;
      txt->qs_append('(');
      append_points(txt, n_points, wkb, 0);
      (*txt) [txt->length() - 1]= ')';
      txt->qs_append(',');
    }
    (*txt) [txt->length() - 1]= ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return false;
}


bool Gis_multi_polygon::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  uint32 n_polygons;

  if (wkb->scan_non_zero_uint4(&n_polygons))
    return true;
    
  while (n_polygons--)
  {
    uint32 n_linear_rings;
    if (wkb->skip_wkb_header() ||
        wkb->scan_non_zero_uint4(&n_linear_rings))
      return true;

    while (n_linear_rings--)
    {
      if (get_mbr_for_points(mbr, wkb, 0))
        return true;
    }
  }
  return false;
}


int Gis_multi_polygon::num_geometries(uint32 *num) const
{
  wkb_parser wkb(&m_wkb_data);
  return wkb.scan_non_zero_uint4(num) ? 1 : 0;
}


int Gis_multi_polygon::geometry_n(uint32 num, String *result) const
{
  uint32 n_polygons;
  wkb_parser wkb(&m_wkb_data);
  const char *start_of_polygon= wkb.data();

  if (wkb.scan_non_zero_uint4(&n_polygons))
    return 1;

  if (num > n_polygons || num < 1)
    return -1;

  do
  {
    uint32 n_linear_rings;
    start_of_polygon= wkb.data();

    if (wkb.skip_wkb_header() ||
        wkb.scan_non_zero_uint4(&n_linear_rings))
      return 1;

    while (n_linear_rings--)
    {
      uint32 n_points;
      if (wkb.scan_n_points_and_check_data(&n_points))
	return 1;
      wkb.skip_unsafe(POINT_DATA_SIZE * n_points);
    }
  } while (--num);
  if (wkb.no_data(0))                          // We must check last segment
    return 1;
  return result->append(start_of_polygon,
                        (uint32) (wkb.data() - start_of_polygon),
                        (uint32) 0);
}


bool Gis_multi_polygon::area(double *ar, wkb_parser *wkb) const
{
  Gis_polygon p;
  return collection_area(ar, wkb, &p);
}


int Gis_multi_polygon::centroid(String *result) const
{
  uint32 n_polygons;
  bool first_loop= 1;
  Gis_polygon p;
  double UNINIT_VAR(res_area);
  point_xy res(0, 0);              // Initialized only to make compiler happy
  wkb_parser wkb(&m_wkb_data);

  if (wkb.scan_non_zero_uint4(&n_polygons))
    return 1;

  while (n_polygons--)
  {
    double cur_area;
    point_xy cur;
    if (wkb.skip_wkb_header())  
      return 1;
    p.set_data_ptr(&wkb);
    if (p.area(&cur_area, &wkb) ||
        p.centroid_xy(&cur))
      return 1;

    if (!first_loop)
    {
      double sum_area= res_area + cur_area;
      res.x= (res_area * res.x + cur_area * cur.x) / sum_area;
      res.y= (res_area * res.y + cur_area * cur.y) / sum_area;   
    }
    else
    {
      first_loop= 0;
      res_area= cur_area;
      res= cur;
    }
  }
  return create_point(result, res);
}


int Gis_multi_polygon::store_shapes(Gcalc_shape_transporter *trn,
                                    Gcalc_shape_status *st) const
{
  if (trn->skip_poly())
    return 0;
  Gis_polygon p;
  return collection_store_shapes(trn, st, &p);
}


const Geometry::Class_info *Gis_multi_polygon::get_class_info() const
{
  return &multipolygon_class;
}


/************************* GeometryCollection ****************************/

uint32 Gis_geometry_collection::get_data_size() const 
{
  uint32 n_objects;
  wkb_parser wkb(&m_wkb_data);
  Geometry_buffer buffer;
  Geometry *geom;

  if (wkb.scan_non_zero_uint4(&n_objects))
    return GET_SIZE_ERROR;

  while (n_objects--)
  {
    if (!(geom= scan_header_and_create(&wkb, &buffer)))
      return GET_SIZE_ERROR;

    uint32 object_size;
    if ((object_size= geom->get_data_size()) == GET_SIZE_ERROR)
      return GET_SIZE_ERROR;
    wkb.skip_unsafe(object_size);
  }
  return (uint32) (wkb.data() - m_wkb_data.data());
}


bool Gis_geometry_collection::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_objects= 0;
  uint32 no_pos= wkb->length();
  Geometry_buffer buffer;
  Geometry *g;

  if (wkb->reserve(4, 512))
    return true;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)
  {
    if (!(g= create_from_wkt(&buffer, trs, wkb)))
      return true;

    if (g->get_class_info()->m_type_id == wkb_geometrycollection)
    {
      trs->set_error_msg("Unexpected GEOMETRYCOLLECTION");
      return true;
    }
    n_objects++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }

  wkb->write_at_position(no_pos, n_objects);
  return false;
}


uint Gis_geometry_collection::init_from_opresult(String *bin,
                                                 const char *opres,
                                                 uint opres_length)
{
  return collection_init_from_opresult(bin, opres, opres_length, NULL);
}


uint Gis_geometry_collection::init_from_wkb(const char *wkb, uint len,
                                            wkbByteOrder bo, String *res)
{
  uint32 n_geom;
  const char *wkb_orig= wkb;

  if (len < 4)
    return 0;
  n_geom= wkb_get_uint(wkb, bo);

  if (res->reserve(4, 512))
    return 0;
  res->q_append(n_geom);
  
  wkb+= 4;
  while (n_geom--)
  {
    Geometry_buffer buffer;
    Geometry *geom;
    int g_len;
    uint32 wkb_type;

    if (len < WKB_HEADER_SIZE ||
        res->reserve(WKB_HEADER_SIZE, 512))
      return 0;

    res->q_append((char) wkb_ndr);
    wkb_type= wkb_get_uint(wkb+1, (wkbByteOrder) wkb[0]);
    res->q_append(wkb_type);

    if (!(geom= create_by_typeid(&buffer, wkb_type)) ||
        !(g_len= geom->init_from_wkb(wkb + WKB_HEADER_SIZE, len,
                                     (wkbByteOrder)  wkb[0], res)))
      return 0;
    g_len+= WKB_HEADER_SIZE;
    wkb+= g_len;
    len-= g_len;
  }
  return (uint) (wkb - wkb_orig);
}


bool Gis_geometry_collection::get_data_as_wkt(String *txt,
                                              wkb_parser *wkb) const
{
  uint32 n_objects;
  Geometry_buffer buffer;
  Geometry *geom;

  if (wkb->scan_non_zero_uint4(&n_objects))
    return true;

  while (n_objects--)
  {
    if (!(geom= scan_header_and_create(wkb, &buffer)) ||
        geom->as_wkt(txt, wkb) ||
        txt->append(STRING_WITH_LEN(","), 512))
      return true;
  }
  txt->length(txt->length() - 1);
  return false;
}


bool Gis_geometry_collection::get_mbr(MBR *mbr, wkb_parser *wkb) const
{
  uint32 n_objects;
  Geometry_buffer buffer;
  Geometry *geom;

  if (wkb->scan_non_zero_uint4(&n_objects))
    return true;

  while (n_objects--)
  {
    if (!(geom= scan_header_and_create(wkb, &buffer)) ||
        geom->get_mbr(mbr, wkb))
      return true;
  }
  return false;
}


bool Gis_geometry_collection::area(double *ar, wkb_parser *wkb) const
{
  return collection_area(ar, wkb, NULL);
}


int Gis_geometry_collection::num_geometries(uint32 *num) const
{
  wkb_parser wkb(&m_wkb_data);
  return wkb.scan_non_zero_uint4(num) ? 1 : 0;
}


int Gis_geometry_collection::geometry_n(uint32 num, String *result) const
{
  uint32 n_objects, length;
  wkb_parser wkb(&m_wkb_data);
  Geometry_buffer buffer;
  Geometry *geom;

  if (wkb.scan_non_zero_uint4(&n_objects))
    return 1;

  if (num > n_objects || num < 1)
    return 1;

  wkb_header header;
  do
  {
    if (wkb.scan_wkb_header(&header) ||
        !(geom= create_by_typeid(&buffer, header.wkb_type)))
      return 1;
    geom->set_data_ptr(&wkb);
    if ((length= geom->get_data_size()) == GET_SIZE_ERROR)
      return 1;
    wkb.skip_unsafe(length);
  } while (--num);

  /* Copy found object to result */
  if (result->reserve(1 + 4 + length))
    return 1;
  result->q_append((char) wkb_ndr);
  result->q_append((uint32) header.wkb_type);
  result->q_append(wkb.data() - length, length); // data-length = start_of_data
  return 0;
}


/*
  Return dimension for object

  SYNOPSIS
    dimension()
    res_dim		Result dimension
    end			End of object will be stored here. May be 0 for
			simple objects!
  RETURN
    0	ok
    1	error
*/

bool Gis_geometry_collection::dimension(uint32 *res_dim,
                                        wkb_parser *wkb) const
{
  uint32 n_objects;
  Geometry_buffer buffer;
  Geometry *geom;

  if (wkb->scan_non_zero_uint4(&n_objects))
    return true;

  *res_dim= 0;
  while (n_objects--)
  {
    uint32 dim;
    if (!(geom= scan_header_and_create(wkb, &buffer)) ||
        geom->dimension(&dim, wkb))
      return true;
    set_if_bigger(*res_dim, dim);
  }
  return false;
}


int Gis_geometry_collection::store_shapes(Gcalc_shape_transporter *trn,
                                          Gcalc_shape_status *st) const
{
  return collection_store_shapes(trn, st, NULL);
}


const Geometry::Class_info *Gis_geometry_collection::get_class_info() const
{
  return &geometrycollection_class;
}

#endif /*HAVE_SPATIAL*/

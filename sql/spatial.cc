/* Copyright (C) 2004 MySQL AB

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

#include "mysql_priv.h"

#ifdef HAVE_SPATIAL

#define MAX_DIGITS_IN_DOUBLE 16

/***************************** Gis_class_info *******************************/

Geometry::Class_info *Geometry::ci_collection[Geometry::wkb_end+1]=
{
  NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static Geometry::Class_info **ci_collection_end=
                                Geometry::ci_collection+Geometry::wkb_end + 1;

Geometry::Class_info::Class_info(const char *name, int type_id,
					 void(*create_func)(void *)):
  m_name(name, strlen(name)), m_type_id(type_id), m_create_func(create_func)
{
  ci_collection[type_id]= this;
}

static void create_point(void *buffer)
{
  new(buffer) Gis_point;
}

static void create_linestring(void *buffer)
{
  new(buffer) Gis_line_string;
}

static void create_polygon(void *buffer)
{
  new(buffer) Gis_polygon;
}

static void create_multipoint(void *buffer)
{
  new(buffer) Gis_multi_point;
}

static void create_multipolygon(void *buffer)
{
  new(buffer) Gis_multi_polygon;
}

static void create_multilinestring(void *buffer)
{
  new(buffer) Gis_multi_line_string;
}

static void create_geometrycollection(void *buffer)
{
  new(buffer) Gis_geometry_collection;
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

Geometry *Geometry::create_from_wkb(Geometry_buffer *buffer,
				    const char *data, uint32 data_len)
{
  uint32 geom_type;
  Geometry *result;

  if (data_len < 1 + 4)
    return NULL;
  data++;
  /*
    FIXME: check byte ordering
    Also check if we could replace this with one byte
  */
  geom_type= uint4korr(data);
  data+= 4;
  if (!(result= create_by_typeid(buffer, (int) geom_type)))
    return NULL;
  result->m_data= data;
  result->m_data_end= data + data_len;
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
      wkt->reserve(1 + 4, 512))
    return NULL;
  (*ci->m_create_func)((void *)buffer);
  Geometry *result= (Geometry *)buffer;
  
  wkt->q_append((char) wkb_ndr);
  wkt->q_append((uint32) result->get_class_info()->m_type_id);
  if (trs->check_next_symbol('(') ||
      result->init_from_wkt(trs, wkt) ||
      trs->check_next_symbol(')'))
    return NULL;
  if (init_stream)  
  {
    result->init_from_wkb(wkt->ptr(), wkt->length());
    result->shift_wkb_header();
  }
  return result;
}


bool Geometry::envelope(String *result) const
{
  MBR mbr;
  const char *end;

  if (get_mbr(&mbr, &end) || result->reserve(1+4*3+SIZEOF_STORED_DOUBLE*10))
    return 1;

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

  return 0;
}


/*
  Create a point from data.

  SYNPOSIS
    create_point()
    result		Put result here
    data		Data for point is here.

  RETURN
    0	ok
    1	Can't reallocate 'result'
*/

bool Geometry::create_point(String *result, const char *data) const
{
  if (no_data(data, SIZEOF_STORED_DOUBLE * 2) ||
      result->reserve(1 + 4 + SIZEOF_STORED_DOUBLE * 2))
    return 1;
  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_point);
  /* Copy two double in same format */
  result->q_append(data, SIZEOF_STORED_DOUBLE*2);
  return 0;
}

/*
  Create a point from coordinates.

  SYNPOSIS
    create_point()
    result		Put result here
    x			x coordinate for point
    y			y coordinate for point

  RETURN
    0	ok
    1	Can't reallocate 'result'
*/

bool Geometry::create_point(String *result, double x, double y) const
{
  if (result->reserve(1 + 4 + SIZEOF_STORED_DOUBLE * 2))
    return 1;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_point);
  result->q_append(x);
  result->q_append(y);
  return 0;
}

/*
  Append N points from packed format to text

  SYNOPSIS
    append_points()
    txt			Append points here
    n_points		Number of points
    data		Packed data
    offset		Offset between points

  RETURN
    # end of data
*/

const char *Geometry::append_points(String *txt, uint32 n_points,
				    const char *data, uint32 offset) const
{			     
  while (n_points--)
  {
    double d;
    data+= offset;
    float8get(d, data);
    txt->qs_append(d);
    txt->qs_append(' ');
    float8get(d, data + SIZEOF_STORED_DOUBLE);
    data+= SIZEOF_STORED_DOUBLE * 2;
    txt->qs_append(d);
    txt->qs_append(',');
  }
  return data;
}


/*
  Get most bounding rectangle (mbr) for X points

  SYNOPSIS
    get_mbr_for_points()
    mbr			MBR (store rectangle here)
    points		Number of points
    data		Packed data
    offset		Offset between points

  RETURN
    0	Wrong data
    #	end of data
*/

const char *Geometry::get_mbr_for_points(MBR *mbr, const char *data,
					 uint offset) const
{
  uint32 points;
  /* read number of points */
  if (no_data(data, 4))
    return 0;
  points= uint4korr(data);
  data+= 4;

  if (no_data(data, (SIZEOF_STORED_DOUBLE * 2 + offset) * points))
    return 0;

  /* Calculate MBR for points */
  while (points--)
  {
    data+= offset;
    mbr->add_xy(data, data + SIZEOF_STORED_DOUBLE);
    data+= SIZEOF_STORED_DOUBLE * 2;
  }
  return data;
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
      wkb->reserve(SIZEOF_STORED_DOUBLE * 2))
    return 1;
  wkb->q_append(x);
  wkb->q_append(y);
  return 0;
}


bool Gis_point::get_data_as_wkt(String *txt, const char **end) const
{
  double x, y;
  if (get_xy(&x, &y))
    return 1;
  if (txt->reserve(MAX_DIGITS_IN_DOUBLE * 2 + 1))
    return 1;
  txt->qs_append(x);
  txt->qs_append(' ');
  txt->qs_append(y);
  *end= m_data+ POINT_DATA_SIZE;
  return 0;
}


bool Gis_point::get_mbr(MBR *mbr, const char **end) const
{
  double x, y;
  if (get_xy(&x, &y))
    return 1;
  mbr->add_xy(x, y);
  *end= m_data+ POINT_DATA_SIZE;
  return 0;
}

const Geometry::Class_info *Gis_point::get_class_info() const
{
  return &point_class;
}


/***************************** LineString *******************************/

uint32 Gis_line_string::get_data_size() const 
{
  if (no_data(m_data, 4))
    return GET_SIZE_ERROR;
  return 4 + uint4korr(m_data) * POINT_DATA_SIZE;
}


bool Gis_line_string::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_points= 0;
  uint32 np_pos= wkb->length();
  Gis_point p;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points  

  for (;;)
  {
    if (p.init_from_wkt(trs, wkb))
      return 1;
    n_points++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  if (n_points < 1)
  {
    trs->set_error_msg("Too few points in LINESTRING");
    return 1;
  }
  wkb->write_at_position(np_pos, n_points);
  return 0;
}


bool Gis_line_string::get_data_as_wkt(String *txt, const char **end) const
{
  uint32 n_points;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  data += 4;

  if (n_points < 1 ||
      no_data(data, SIZEOF_STORED_DOUBLE * 2 * n_points) ||
      txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1)*2 + 1) * n_points))
    return 1;

  while (n_points--)
  {
    double x, y;
    float8get(x, data);
    float8get(y, data + SIZEOF_STORED_DOUBLE);
    data+= SIZEOF_STORED_DOUBLE * 2;
    txt->qs_append(x);
    txt->qs_append(' ');
    txt->qs_append(y);
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);		// Remove end ','
  *end= data;
  return 0;
}


bool Gis_line_string::get_mbr(MBR *mbr, const char **end) const
{
  return (*end=get_mbr_for_points(mbr, m_data, 0)) == 0;
}


int Gis_line_string::length(double *len) const
{
  uint32 n_points;
  double prev_x, prev_y;
  const char *data= m_data;

  *len= 0;					// In case of errors
  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  data+= 4;
  if (n_points < 1 || no_data(data, SIZEOF_STORED_DOUBLE * 2 * n_points))
    return 1;

  float8get(prev_x, data);
  float8get(prev_y, data + SIZEOF_STORED_DOUBLE);
  data+= SIZEOF_STORED_DOUBLE*2;

  while (--n_points)
  {
    double x, y;
    float8get(x, data);
    float8get(y, data + SIZEOF_STORED_DOUBLE);
    data+= SIZEOF_STORED_DOUBLE * 2;
    *len+= sqrt(pow(prev_x-x,2)+pow(prev_y-y,2));
    prev_x= x;
    prev_y= y;
  }
  return 0;
}


int Gis_line_string::is_closed(int *closed) const
{
  uint32 n_points;
  double x1, y1, x2, y2;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  if (n_points == 1)
  {
    *closed=1;
    return 0;
  }
  data+= 4;
  if (no_data(data, SIZEOF_STORED_DOUBLE * 2 * n_points))
    return 1;

  /* Get first point */
  float8get(x1, data);
  float8get(y1, data + SIZEOF_STORED_DOUBLE);

  /* get last point */
  data+= SIZEOF_STORED_DOUBLE*2 + (n_points-2)*POINT_DATA_SIZE;
  float8get(x2, data);
  float8get(y2, data + SIZEOF_STORED_DOUBLE);

  *closed= (x1==x2) && (y1==y2);
  return 0;
}


int Gis_line_string::num_points(uint32 *n_points) const
{
  *n_points= uint4korr(m_data);
  return 0;
}


int Gis_line_string::start_point(String *result) const
{
  /* +4 is for skipping over number of points */
  return create_point(result, m_data + 4);
}


int Gis_line_string::end_point(String *result) const
{
  uint32 n_points;
  if (no_data(m_data, 4))
    return 1;
  n_points= uint4korr(m_data);
  return create_point(result, m_data + 4 + (n_points - 1) * POINT_DATA_SIZE);
}


int Gis_line_string::point_n(uint32 num, String *result) const
{
  uint32 n_points;
  if (no_data(m_data, 4))
    return 1;
  n_points= uint4korr(m_data);
  if ((uint32) (num - 1) >= n_points) // means (num > n_points || num < 1)
    return 1;

  return create_point(result, m_data + 4 + (num - 1) * POINT_DATA_SIZE);
}

const Geometry::Class_info *Gis_line_string::get_class_info() const
{
  return &linestring_class;
}


/***************************** Polygon *******************************/

uint32 Gis_polygon::get_data_size() const 
{
  uint32 n_linear_rings;
  const char *data= m_data;

  if (no_data(data, 4))
    return GET_SIZE_ERROR;
  n_linear_rings= uint4korr(data);
  data+= 4;

  while (n_linear_rings--)
  {
    if (no_data(data, 4))
      return GET_SIZE_ERROR;
    data+= 4 + uint4korr(data)*POINT_DATA_SIZE;
  }
  return (uint32) (data - m_data);
}


bool Gis_polygon::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_linear_rings= 0;
  uint32 lr_pos= wkb->length();
  int closed;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points
  for (;;)  
  {
    Gis_line_string ls;
    uint32 ls_pos=wkb->length();
    if (trs->check_next_symbol('(') ||
	ls.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return 1;

    ls.init_from_wkb(wkb->ptr()+ls_pos, wkb->length()-ls_pos);
    if (ls.is_closed(&closed) || !closed)
    {
      trs->set_error_msg("POLYGON's linear ring isn't closed");
      return 1;
    }
    n_linear_rings++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(lr_pos, n_linear_rings);
  return 0;
}


bool Gis_polygon::get_data_as_wkt(String *txt, const char **end) const
{
  uint32 n_linear_rings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;

  n_linear_rings= uint4korr(data);
  data+= 4;

  while (n_linear_rings--)
  {
    uint32 n_points;
    if (no_data(data, 4))
      return 1;
    n_points= uint4korr(data);
    data+= 4;
    if (no_data(data, (SIZEOF_STORED_DOUBLE*2) * n_points) ||
	txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return 1;
    txt->qs_append('(');
    data= append_points(txt, n_points, data, 0);
    (*txt) [txt->length() - 1]= ')';		// Replace end ','
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);		// Remove end ','
  *end= data;
  return 0;
}


bool Gis_polygon::get_mbr(MBR *mbr, const char **end) const
{
  uint32 n_linear_rings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_linear_rings= uint4korr(data);
  data+= 4;

  while (n_linear_rings--)
  {
    if (!(data= get_mbr_for_points(mbr, data, 0)))
      return 1;
  }
  *end= data;
  return 0;
}


int Gis_polygon::area(double *ar, const char **end_of_data) const
{
  uint32 n_linear_rings;
  double result= -1.0;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_linear_rings= uint4korr(data);
  data+= 4;

  while (n_linear_rings--)
  {
    double prev_x, prev_y;
    double lr_area= 0;
    uint32 n_points;

    if (no_data(data, 4))
      return 1;
    n_points= uint4korr(data);
    if (no_data(data, (SIZEOF_STORED_DOUBLE*2) * n_points))
      return 1;
    float8get(prev_x, data+4);
    float8get(prev_y, data+(4+SIZEOF_STORED_DOUBLE));
    data+= (4+SIZEOF_STORED_DOUBLE*2);

    while (--n_points)				// One point is already read
    {
      double x, y;
      float8get(x, data);
      float8get(y, data + SIZEOF_STORED_DOUBLE);
      data+= (SIZEOF_STORED_DOUBLE*2);
      /* QQ: Is the following prev_x+x right ? */
      lr_area+= (prev_x + x)* (prev_y - y);
      prev_x= x;
      prev_y= y;
    }
    lr_area= fabs(lr_area)/2;
    if (result == -1.0)
      result= lr_area;
    else
      result-= lr_area;
  }
  *ar= fabs(result);
  *end_of_data= data;
  return 0;
}


int Gis_polygon::exterior_ring(String *result) const
{
  uint32 n_points, length;
  const char *data= m_data + 4; // skip n_linerings

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  data+= 4;
  length= n_points * POINT_DATA_SIZE;
  if (no_data(data, length) || result->reserve(1+4+4+ length))
    return 1;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_linestring);
  result->q_append(n_points);
  result->q_append(data, n_points * POINT_DATA_SIZE); 
  return 0;
}


int Gis_polygon::num_interior_ring(uint32 *n_int_rings) const
{
  if (no_data(m_data, 4))
    return 1;
  *n_int_rings= uint4korr(m_data)-1;
  return 0;
}


int Gis_polygon::interior_ring_n(uint32 num, String *result) const
{
  const char *data= m_data;
  uint32 n_linear_rings;
  uint32 n_points;
  uint32 points_size;

  if (no_data(data, 4))
    return 1;
  n_linear_rings= uint4korr(data);
  data+= 4;

  if (num >= n_linear_rings || num < 1)
    return 1;

  while (num--)
  {
    if (no_data(data, 4))
      return 1;
    data+= 4 + uint4korr(data) * POINT_DATA_SIZE;
  }
  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  points_size= n_points * POINT_DATA_SIZE;
  data+= 4;
  if (no_data(data, points_size) || result->reserve(1+4+4+ points_size))
    return 1;

  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_linestring);
  result->q_append(n_points);
  result->q_append(data, points_size); 

  return 0;
}


int Gis_polygon::centroid_xy(double *x, double *y) const
{
  uint32 n_linear_rings;
  double res_area, res_cx, res_cy;
  const char *data= m_data;
  bool first_loop= 1;
  LINT_INIT(res_area);
  LINT_INIT(res_cx);
  LINT_INIT(res_cy);

  if (no_data(data, 4))
    return 1;
  n_linear_rings= uint4korr(data);
  data+= 4;

  while (n_linear_rings--)
  {
    uint32 n_points, org_n_points;
    double prev_x, prev_y;
    double cur_area= 0;
    double cur_cx= 0;
    double cur_cy= 0;

    if (no_data(data, 4))
      return 1;
    org_n_points= n_points= uint4korr(data);
    data+= 4;
    if (no_data(data, (SIZEOF_STORED_DOUBLE*2) * n_points))
      return 1;
    float8get(prev_x, data);
    float8get(prev_y, data+SIZEOF_STORED_DOUBLE);
    data+= (SIZEOF_STORED_DOUBLE*2);

    while (--n_points)				// One point is already read
    {
      double x, y;
      float8get(x, data);
      float8get(y, data + SIZEOF_STORED_DOUBLE);
      data+= (SIZEOF_STORED_DOUBLE*2);
      /* QQ: Is the following prev_x+x right ? */
      cur_area+= (prev_x + x) * (prev_y - y);
      cur_cx+= x;
      cur_cy+= y;
      prev_x= x;
      prev_y= y;
    }
    cur_area= fabs(cur_area) / 2;
    cur_cx= cur_cx / (org_n_points - 1);
    cur_cy= cur_cy / (org_n_points - 1);

    if (!first_loop)
    {
      double d_area= fabs(res_area - cur_area);
      res_cx= (res_area * res_cx - cur_area * cur_cx) / d_area;
      res_cy= (res_area * res_cy - cur_area * cur_cy) / d_area;
    }
    else
    {
      first_loop= 0;
      res_area= cur_area;
      res_cx= cur_cx;
      res_cy= cur_cy;
    }
  }

  *x= res_cx;
  *y= res_cy;
  return 0;
}


int Gis_polygon::centroid(String *result) const
{
  double x, y;
  if (centroid_xy(&x, &y))
    return 1;
  return create_point(result, x, y);
}

const Geometry::Class_info *Gis_polygon::get_class_info() const
{
  return &polygon_class;
}


/***************************** MultiPoint *******************************/

uint32 Gis_multi_point::get_data_size() const 
{
  if (no_data(m_data, 4))
    return GET_SIZE_ERROR;
  return 4 + uint4korr(m_data)*(POINT_DATA_SIZE + WKB_HEADER_SIZE);
}


bool Gis_multi_point::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_points= 0;
  uint32 np_pos= wkb->length();
  Gis_point p;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)
  {
    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char) wkb_ndr);
    wkb->q_append((uint32) wkb_point);
    if (p.init_from_wkt(trs, wkb))
      return 1;
    n_points++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(np_pos, n_points);	// Store number of found points
  return 0;
}


bool Gis_multi_point::get_data_as_wkt(String *txt, const char **end) const
{
  uint32 n_points;
  if (no_data(m_data, 4))
    return 1;

  n_points= uint4korr(m_data);
  if (no_data(m_data+4,
	      n_points * (SIZEOF_STORED_DOUBLE * 2 + WKB_HEADER_SIZE)) ||
      txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
    return 1;
  *end= append_points(txt, n_points, m_data+4, WKB_HEADER_SIZE);
  txt->length(txt->length()-1);			// Remove end ','
  return 0;
}


bool Gis_multi_point::get_mbr(MBR *mbr, const char **end) const
{
  return (*end= get_mbr_for_points(mbr, m_data, WKB_HEADER_SIZE)) == 0;
}


int Gis_multi_point::num_geometries(uint32 *num) const
{
  *num= uint4korr(m_data);
  return 0;
}


int Gis_multi_point::geometry_n(uint32 num, String *result) const
{
  const char *data= m_data;
  uint32 n_points;

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  data+= 4+ (num - 1) * (WKB_HEADER_SIZE + POINT_DATA_SIZE);

  if (num > n_points || num < 1 ||
      no_data(data, WKB_HEADER_SIZE + POINT_DATA_SIZE) ||
      result->reserve(WKB_HEADER_SIZE + POINT_DATA_SIZE))
    return 1;

  result->q_append(data, WKB_HEADER_SIZE + POINT_DATA_SIZE);
  return 0;
}

const Geometry::Class_info *Gis_multi_point::get_class_info() const
{
  return &multipoint_class;
}


/***************************** MultiLineString *******************************/

uint32 Gis_multi_line_string::get_data_size() const 
{
  uint32 n_line_strings;
  const char *data= m_data;

  if (no_data(data, 4))
    return GET_SIZE_ERROR;
  n_line_strings= uint4korr(data);
  data+= 4;

  while (n_line_strings--)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return GET_SIZE_ERROR;
    data+= (WKB_HEADER_SIZE + 4 + uint4korr(data + WKB_HEADER_SIZE) *
	    POINT_DATA_SIZE);
  }
  return (uint32) (data - m_data);
}


bool Gis_multi_line_string::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_line_strings= 0;
  uint32 ls_pos= wkb->length();

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points
  
  for (;;)
  {
    Gis_line_string ls;

    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char) wkb_ndr);
    wkb->q_append((uint32) wkb_linestring);

    if (trs->check_next_symbol('(') ||
	ls.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return 1;
    n_line_strings++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(ls_pos, n_line_strings);
  return 0;
}


bool Gis_multi_line_string::get_data_as_wkt(String *txt, 
					     const char **end) const
{
  uint32 n_line_strings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;

  while (n_line_strings--)
  {
    uint32 n_points;
    if (no_data(data, (WKB_HEADER_SIZE + 4)))
      return 1;
    n_points= uint4korr(data + WKB_HEADER_SIZE);
    data+= WKB_HEADER_SIZE + 4;
    if (no_data(data, n_points * (SIZEOF_STORED_DOUBLE*2)) ||
	txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return 1;
    txt->qs_append('(');
    data= append_points(txt, n_points, data, 0);
    (*txt) [txt->length() - 1]= ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  *end= data;
  return 0;
}


bool Gis_multi_line_string::get_mbr(MBR *mbr, const char **end) const
{
  uint32 n_line_strings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;

  while (n_line_strings--)
  {
    data+= WKB_HEADER_SIZE;
    if (!(data= get_mbr_for_points(mbr, data, 0)))
      return 1;
  }
  *end= data;
  return 0;
}


int Gis_multi_line_string::num_geometries(uint32 *num) const
{
  *num= uint4korr(m_data);
  return 0;
}


int Gis_multi_line_string::geometry_n(uint32 num, String *result) const
{
  uint32 n_line_strings, n_points, length;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;

  if ((num > n_line_strings) || (num < 1))
    return 1;
 
  for (;;)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    n_points= uint4korr(data + WKB_HEADER_SIZE);
    length= WKB_HEADER_SIZE + 4+ POINT_DATA_SIZE * n_points;
    if (no_data(data, length))
      return 1;
    if (!--num)
      break;
    data+= length;
  }
  return result->append(data, length, (uint32) 0);
}


int Gis_multi_line_string::length(double *len) const
{
  uint32 n_line_strings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;

  *len=0;
  while (n_line_strings--)
  {
    double ls_len;
    Gis_line_string ls;
    data+= WKB_HEADER_SIZE;
    ls.init_from_wkb(data, (uint32) (m_data_end - data));
    if (ls.length(&ls_len))
      return 1;
    *len+= ls_len;
    /*
      We know here that ls was ok, so we can call the trivial function
      Gis_line_string::get_data_size without error checking
    */
    data+= ls.get_data_size();
  }
  return 0;
}


int Gis_multi_line_string::is_closed(int *closed) const
{
  uint32 n_line_strings;
  const char *data= m_data;

  if (no_data(data, 4 + WKB_HEADER_SIZE))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4 + WKB_HEADER_SIZE;

  while (n_line_strings--)
  {
    Gis_line_string ls;
    if (no_data(data, 0))
      return 1;
    ls.init_from_wkb(data, (uint32) (m_data_end - data));
    if (ls.is_closed(closed))
      return 1;
    if (!*closed)
      return 0;
    /*
      We know here that ls was ok, so we can call the trivial function
      Gis_line_string::get_data_size without error checking
    */
    data+= ls.get_data_size() + WKB_HEADER_SIZE;
  }
  return 0;
}

const Geometry::Class_info *Gis_multi_line_string::get_class_info() const
{
  return &multilinestring_class;
}


/***************************** MultiPolygon *******************************/

uint32 Gis_multi_polygon::get_data_size() const 
{
  uint32 n_polygons;
  const char *data= m_data;

  if (no_data(data, 4))
    return GET_SIZE_ERROR;
  n_polygons= uint4korr(data);
  data+= 4;

  while (n_polygons--)
  {
    uint32 n_linear_rings;
    if (no_data(data, 4 + WKB_HEADER_SIZE))
      return GET_SIZE_ERROR;

    n_linear_rings= uint4korr(data + WKB_HEADER_SIZE);
    data+= 4 + WKB_HEADER_SIZE;

    while (n_linear_rings--)
    {
      if (no_data(data, 4))
	return GET_SIZE_ERROR;
      data+= 4 + uint4korr(data) * POINT_DATA_SIZE;
    }
  }
  return (uint32) (data - m_data);
}


bool Gis_multi_polygon::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_polygons= 0;
  int np_pos= wkb->length();
  Gis_polygon p;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)  
  {
    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char) wkb_ndr);
    wkb->q_append((uint32) wkb_polygon);

    if (trs->check_next_symbol('(') ||
	p.init_from_wkt(trs, wkb) ||
	trs->check_next_symbol(')'))
      return 1;
    n_polygons++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }
  wkb->write_at_position(np_pos, n_polygons);
  return 0;
}


bool Gis_multi_polygon::get_data_as_wkt(String *txt, const char **end) const
{
  uint32 n_polygons;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  while (n_polygons--)
  {
    uint32 n_linear_rings;
    if (no_data(data, 4 + WKB_HEADER_SIZE) ||
	txt->reserve(1, 512))
      return 1;
    n_linear_rings= uint4korr(data+WKB_HEADER_SIZE);
    data+= 4 + WKB_HEADER_SIZE;
    txt->q_append('(');

    while (n_linear_rings--)
    {
      if (no_data(data, 4))
        return 1;
      uint32 n_points= uint4korr(data);
      data+= 4;
      if (no_data(data, (SIZEOF_STORED_DOUBLE * 2) * n_points) ||
	  txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points,
		       512))
	return 1;
      txt->qs_append('(');
      data= append_points(txt, n_points, data, 0);
      (*txt) [txt->length() - 1]= ')';
      txt->qs_append(',');
    }
    (*txt) [txt->length() - 1]= ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  *end= data;
  return 0;
}


bool Gis_multi_polygon::get_mbr(MBR *mbr, const char **end) const
{
  uint32 n_polygons;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  while (n_polygons--)
  {
    uint32 n_linear_rings;
    if (no_data(data, 4+WKB_HEADER_SIZE))
      return 1;
    n_linear_rings= uint4korr(data + WKB_HEADER_SIZE);
    data+= WKB_HEADER_SIZE + 4;

    while (n_linear_rings--)
    {
      if (!(data= get_mbr_for_points(mbr, data, 0)))
	return 1;
    }
  }
  *end= data;
  return 0;
}


int Gis_multi_polygon::num_geometries(uint32 *num) const
{
  *num= uint4korr(m_data);
  return 0;
}


int Gis_multi_polygon::geometry_n(uint32 num, String *result) const
{
  uint32 n_polygons;
  const char *data= m_data, *start_of_polygon;

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  if (num > n_polygons || num < 1)
    return -1;

  do
  {
    uint32 n_linear_rings;
    start_of_polygon= data;

    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    n_linear_rings= uint4korr(data + WKB_HEADER_SIZE);
    data+= WKB_HEADER_SIZE + 4;

    while (n_linear_rings--)
    {
      uint32 n_points;
      if (no_data(data, 4))
	return 1;
      n_points= uint4korr(data);
      data+= 4 + POINT_DATA_SIZE * n_points;
    }
  } while (--num);
  if (no_data(data, 0))				// We must check last segment
    return 1;
  return result->append(start_of_polygon, (uint32) (data - start_of_polygon),
			(uint32) 0);
}


int Gis_multi_polygon::area(double *ar,  const char **end_of_data) const
{
  uint32 n_polygons;
  const char *data= m_data;
  double result= 0;

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  while (n_polygons--)
  {
    double p_area;
    Gis_polygon p;

    data+= WKB_HEADER_SIZE;
    p.init_from_wkb(data, (uint32) (m_data_end - data));
    if (p.area(&p_area, &data))
      return 1;
    result+= p_area;
  }
  *ar= result;
  *end_of_data= data;
  return 0;
}


int Gis_multi_polygon::centroid(String *result) const
{
  uint32 n_polygons;
  bool first_loop= 1;
  Gis_polygon p;
  double res_area, res_cx, res_cy;
  double cur_area, cur_cx, cur_cy;
  const char *data= m_data;

  LINT_INIT(res_area);
  LINT_INIT(res_cx);
  LINT_INIT(res_cy);

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  while (n_polygons--)
  {
    data+= WKB_HEADER_SIZE;
    p.init_from_wkb(data, (uint32) (m_data_end - data));
    if (p.area(&cur_area, &data) ||
	p.centroid_xy(&cur_cx, &cur_cy))
      return 1;

    if (!first_loop)
    {
      double sum_area= res_area + cur_area;
      res_cx= (res_area * res_cx + cur_area * cur_cx) / sum_area;
      res_cy= (res_area * res_cy + cur_area * cur_cy) / sum_area;
    }
    else
    {
      first_loop= 0;
      res_area= cur_area;
      res_cx= cur_cx;
      res_cy= cur_cy;
    }
  }

  return create_point(result, res_cx, res_cy);
}

const Geometry::Class_info *Gis_multi_polygon::get_class_info() const
{
  return &multipolygon_class;
}


/************************* GeometryCollection ****************************/

uint32 Gis_geometry_collection::get_data_size() const 
{
  uint32 n_objects;
  const char *data= m_data;
  Geometry_buffer buffer;
  Geometry *geom;

  if (no_data(data, 4))
    return GET_SIZE_ERROR;
  n_objects= uint4korr(data);
  data+= 4;

  while (n_objects--)
  {
    uint32 wkb_type,object_size;

    if (no_data(data, WKB_HEADER_SIZE))
      return GET_SIZE_ERROR;
    wkb_type= uint4korr(data + 1);
    data+= WKB_HEADER_SIZE;

    if (!(geom= create_by_typeid(&buffer, wkb_type)))
      return GET_SIZE_ERROR;
    geom->init_from_wkb(data, (uint) (m_data_end - data));
    if ((object_size= geom->get_data_size()) == GET_SIZE_ERROR)
      return GET_SIZE_ERROR;
    data+= object_size;
  }
  return (uint32) (data - m_data);
}


bool Gis_geometry_collection::init_from_wkt(Gis_read_stream *trs, String *wkb)
{
  uint32 n_objects= 0;
  uint32 no_pos= wkb->length();
  Geometry_buffer buffer;
  Geometry *g;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->length(wkb->length()+4);			// Reserve space for points

  for (;;)
  {
    if (!(g= create_from_wkt(&buffer, trs, wkb)))
      return 1;

    if (g->get_class_info()->m_type_id == wkb_geometrycollection)
    {
      trs->set_error_msg("Unexpected GEOMETRYCOLLECTION");
      return 1;
    }
    n_objects++;
    if (trs->skip_char(','))			// Didn't find ','
      break;
  }

  wkb->write_at_position(no_pos, n_objects);
  return 0;
}


bool Gis_geometry_collection::get_data_as_wkt(String *txt,
					     const char **end) const
{
  uint32 n_objects;
  Geometry_buffer buffer;
  Geometry *geom;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;
  n_objects= uint4korr(data);
  data+= 4;

  while (n_objects--)
  {
    uint32 wkb_type;

    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    wkb_type= uint4korr(data + 1);
    data+= WKB_HEADER_SIZE;

    if (!(geom= create_by_typeid(&buffer, wkb_type)))
      return 1;
    geom->init_from_wkb(data, (uint) (m_data_end - data));
    if (geom->as_wkt(txt, &data))
      return 1;
    if (txt->append(",", 1, 512))
      return 1;
  }
  txt->length(txt->length() - 1);
  *end= data;
  return 0;
}


bool Gis_geometry_collection::get_mbr(MBR *mbr, const char **end) const
{
  uint32 n_objects;
  const char *data= m_data;
  Geometry_buffer buffer;
  Geometry *geom;

  if (no_data(data, 4))
    return 1;
  n_objects= uint4korr(data);
  data+= 4;

  while (n_objects--)
  {
    uint32 wkb_type;

    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    wkb_type= uint4korr(data + 1);
    data+= WKB_HEADER_SIZE;

    if (!(geom= create_by_typeid(&buffer, wkb_type)))
      return 1;
    geom->init_from_wkb(data, (uint32) (m_data_end - data));
    if (geom->get_mbr(mbr, &data))
      return 1;
  }
  *end= data;
  return 0;
}


int Gis_geometry_collection::num_geometries(uint32 *num) const
{
  if (no_data(m_data, 4))
    return 1;
  *num= uint4korr(m_data);
  return 0;
}


int Gis_geometry_collection::geometry_n(uint32 num, String *result) const
{
  uint32 n_objects, wkb_type, length;
  const char *data= m_data;
  Geometry_buffer buffer;
  Geometry *geom;

  if (no_data(data, 4))
    return 1;
  n_objects= uint4korr(data);
  data+= 4;
  if (num > n_objects || num < 1)
    return 1;

  do
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    wkb_type= uint4korr(data + 1);
    data+= WKB_HEADER_SIZE;

    if (!(geom= create_by_typeid(&buffer, wkb_type)))
      return 1;
    geom->init_from_wkb(data, (uint) (m_data_end - data));
    if ((length= geom->get_data_size()) == GET_SIZE_ERROR)
      return 1;
    data+= length;
  } while (--num);

  /* Copy found object to result */
  if (result->reserve(1+4+length))
    return 1;
  result->q_append((char) wkb_ndr);
  result->q_append((uint32) wkb_type);
  result->q_append(data-length, length);	// data-length = start_of_data
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

bool Gis_geometry_collection::dimension(uint32 *res_dim, const char **end) const
{
  uint32 n_objects;
  const char *data= m_data;
  Geometry_buffer buffer;
  Geometry *geom;

  if (no_data(data, 4))
    return 1;
  n_objects= uint4korr(data);
  data+= 4;

  *res_dim= 0;
  while (n_objects--)
  {
    uint32 wkb_type, length, dim;
    const char *end_data;

    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    wkb_type= uint4korr(data + 1);
    data+= WKB_HEADER_SIZE;
    if (!(geom= create_by_typeid(&buffer, wkb_type)))
      return 1;
    geom->init_from_wkb(data, (uint32) (m_data_end - data));
    if (geom->dimension(&dim, &end_data))
      return 1;
    set_if_bigger(*res_dim, dim);
    if (end_data)				// Complex object
      data= end_data;
    else if ((length= geom->get_data_size()) == GET_SIZE_ERROR)
      return 1;
    else
      data+= length;
  }
  *end= data;
  return 0;
}

const Geometry::Class_info *Gis_geometry_collection::get_class_info() const
{
  return &geometrycollection_class;
}

#endif /*HAVE_SPATIAL*/

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

#ifndef _spatial_h
#define _spatial_h

const uint SRID_SIZE= 4;
const uint SIZEOF_STORED_DOUBLE= 8;
const uint POINT_DATA_SIZE= SIZEOF_STORED_DOUBLE*2; 
const uint WKB_HEADER_SIZE= 1+4;
const uint32 GET_SIZE_ERROR= ((uint32) -1);

struct st_point_2d
{
  double x;
  double y;
};

struct st_linear_ring
{
  uint32 n_points;
  st_point_2d points;
};

/***************************** MBR *******************************/


/*
  It's ok that a lot of the functions are inline as these are only used once
  in MySQL
*/

struct MBR
{
  double xmin, ymin, xmax, ymax;

  MBR()
  {
    xmin= ymin= DBL_MAX;
    xmax= ymax= -DBL_MAX;
  }

  MBR(const double xmin_arg, const double ymin_arg,
      const double xmax_arg, const double ymax_arg)
    :xmin(xmin_arg), ymin(ymin_arg), xmax(xmax_arg), ymax(ymax_arg)
  {}

  MBR(const st_point_2d &min, const st_point_2d &max)
    :xmin(min.x), ymin(min.y), xmax(max.x), ymax(max.y)
  {}
 
  inline void add_xy(double x, double y)
  {
    /* Not using "else" for proper one point MBR calculation */
    if (x < xmin)
      xmin= x;
    if (x > xmax)
      xmax= x;
    if (y < ymin)
      ymin= y;
    if (y > ymax)
      ymax= y;
  }
  void add_xy(const char *px, const char *py)
  {
    double x, y;
    float8get(x, px);
    float8get(y, py);
    add_xy(x,y);
  }
  void add_mbr(const MBR *mbr)
  {
    if (mbr->xmin < xmin)
      xmin= mbr->xmin;
    if (mbr->xmax > xmax)
      xmax= mbr->xmax;
    if (mbr->ymin < ymin)
      ymin= mbr->ymin;
    if (mbr->ymax > ymax)
      ymax= mbr->ymax;
  }

  int equals(const MBR *mbr)
  {
    return ((mbr->xmin == xmin) && (mbr->ymin == ymin) &&
	    (mbr->xmax == xmax) && (mbr->ymax == ymax));
  }

  int disjoint(const MBR *mbr)
  {
    return ((mbr->xmin > xmax) || (mbr->ymin > ymax) ||
	    (mbr->xmax < xmin) || (mbr->ymax < ymin));
  }

  int intersects(const MBR *mbr)
  {
    return !disjoint(mbr);
  }

  int touches(const MBR *mbr)
  {
    return ((((mbr->xmin == xmax) || (mbr->xmax == xmin)) && 
	     ((mbr->ymin >= ymin) && (mbr->ymin <= ymax) || 
	      (mbr->ymax >= ymin) && (mbr->ymax <= ymax))) ||
	    (((mbr->ymin == ymax) || (mbr->ymax == ymin)) &&
	     ((mbr->xmin >= xmin) && (mbr->xmin <= xmax) ||
	      (mbr->xmax >= xmin) && (mbr->xmax <= xmax))));
  }

  int within(const MBR *mbr)
  {
    return ((mbr->xmin <= xmin) && (mbr->ymin <= ymin) &&
	    (mbr->xmax >= xmax) && (mbr->ymax >= ymax));
  }

  int contains(const MBR *mbr)
  {
    return ((mbr->xmin >= xmin) && (mbr->ymin >= ymin) &&
	    (mbr->xmax <= xmax) && (mbr->ymax <= ymax));
  }

  bool inner_point(double x, double y) const
  {
    return (xmin<x) && (xmax>x) && (ymin<y) && (ymax>x);
  }

  int overlaps(const MBR *mbr)
  {
    int lb= mbr->inner_point(xmin, ymin);
    int rb= mbr->inner_point(xmax, ymin);
    int rt= mbr->inner_point(xmax, ymax);
    int lt= mbr->inner_point(xmin, ymax);

    int a = lb+rb+rt+lt;
    return (a>0) && (a<4) && (!within(mbr));
  }
};


/***************************** Geometry *******************************/

class Geometry;

typedef bool (Geometry::*GF_InitFromText)(Gis_read_stream *, String *);
typedef bool (Geometry::*GF_GetDataAsText)(String *, const char **);
typedef uint32 (Geometry::*GF_GetDataSize)() const;
typedef bool (Geometry::*GF_GetMBR)(MBR *, const char **end) const;

typedef bool (Geometry::*GF_GetD)(double *) const;
typedef bool (Geometry::*GF_GetD_AND_END)(double *, const char **) const;
typedef bool (Geometry::*GF_GetI)(int *) const;
typedef bool (Geometry::*GF_GetUI)(uint32 *) const;
typedef bool (Geometry::*GF_GetUI_AND_END)(uint32 *, const char **) const;
typedef bool (Geometry::*GF_GetWS)(String *);
typedef bool (Geometry::*GF_GetUIWS)(uint32, String *) const;

#define GEOM_METHOD_PRESENT(geom_obj, method)\
    (geom_obj.m_vmt->method != &Geometry::method)

class Geometry
{
public:
  enum wkbType
  {
    wkbPoint= 1,
    wkbLineString= 2,
    wkbPolygon= 3,
    wkbMultiPoint= 4,
    wkbMultiLineString= 5,
    wkbMultiPolygon= 6,
    wkbGeometryCollection= 7,
    wkb_end=8
  };
  enum wkbByteOrder
  {
    wkbXDR= 0,    /* Big Endian */
    wkbNDR= 1     /* Little Endian */
  };                                    

  class Gis_class_info
  {
  public:
    GF_InitFromText init_from_wkt;
    GF_GetDataAsText get_data_as_wkt;
    GF_GetDataSize get_data_size;
    GF_GetMBR get_mbr;
    GF_GetD get_x;
    GF_GetD get_y;
    GF_GetD length;
    GF_GetD_AND_END area;

    GF_GetI is_closed;

    GF_GetUI num_interior_ring;
    GF_GetUI num_points;
    GF_GetUI num_geometries;
    GF_GetUI_AND_END dimension;

    GF_GetWS start_point;
    GF_GetWS end_point;
    GF_GetWS exterior_ring;
    GF_GetWS centroid;

    GF_GetUIWS point_n;
    GF_GetUIWS interior_ring_n;
    GF_GetUIWS geometry_n;

    LEX_STRING m_name;
    int m_type_id;
    Gis_class_info *m_next_rt;
  };
  Gis_class_info *m_vmt;

  const Gis_class_info *get_class_info() const { return m_vmt; }
  uint32 get_data_size() const { return (this->*m_vmt->get_data_size)(); }
  
  bool init_from_wkt(Gis_read_stream *trs, String *wkb) 
  { return (this->*m_vmt->init_from_wkt)(trs, wkb); }

  bool get_data_as_wkt(String *txt, const char **end)
  { return (this->*m_vmt->get_data_as_wkt)(txt, end); }

  int get_mbr(MBR *mbr, const char **end) const
  { return (this->*m_vmt->get_mbr)(mbr, end); }
  bool dimension(uint32 *dim, const char **end) const
  {
    return (this->*m_vmt->dimension)(dim, end);
  }

  bool get_x(double *x) const { return (this->*m_vmt->get_x)(x); }
  bool get_y(double *y) const { return (this->*m_vmt->get_y)(y); }
  bool length(double *len) const  { return (this->*m_vmt->length)(len); }
  bool area(double *ar, const char **end) const
  {
    return (this->*m_vmt->area)(ar, end);
  }

  bool is_closed(int *closed) const
  { return (this->*m_vmt->is_closed)(closed); }

  bool num_interior_ring(uint32 *n_int_rings) const 
  { return (this->*m_vmt->num_interior_ring)(n_int_rings); }
  bool num_points(uint32 *n_points) const
  { return (this->*m_vmt->num_points)(n_points); }

  bool num_geometries(uint32 *num) const
  { return (this->*m_vmt->num_geometries)(num); }

  bool start_point(String *point)
  { return (this->*m_vmt->start_point)(point); }
  bool end_point(String *point)
  { return (this->*m_vmt->end_point)(point); }
  bool exterior_ring(String *ring)
  { return (this->*m_vmt->exterior_ring)(ring); }
  bool centroid(String *point)
  { return (this->*m_vmt->centroid)(point); }

  bool point_n(uint32 num, String *result) const
  { return (this->*m_vmt->point_n)(num, result); }
  bool interior_ring_n(uint32 num, String *result) const
  { return (this->*m_vmt->interior_ring_n)(num, result); }
  bool geometry_n(uint32 num, String *result) const
  { return (this->*m_vmt->geometry_n)(num, result); }

public:
  int create_from_wkb(const char *data, uint32 data_len);
  int create_from_wkt(Gis_read_stream *trs, String *wkt, bool init_stream=1);
  int init(int type_id)
  {
    m_vmt= find_class(type_id);
    return !m_vmt;
  }
  int new_geometry(const char *name, uint32 len)
  {
    m_vmt= find_class(name, len);
    return !m_vmt;
  }
  int as_wkt(String *wkt, const char **end)
  {
    uint32 len= get_class_info()->m_name.length;
    if (wkt->reserve(len + 2, 512))
      return 1;
    wkt->qs_append(get_class_info()->m_name.str, len);
    wkt->qs_append('(');
    if (get_data_as_wkt(wkt, end))
      return 1;
    wkt->qs_append(')');
    return 0;
  }

  inline void init_from_wkb(const char *data, uint32 data_len)
  {
    m_data= data;
    m_data_end= data + data_len;
  }

  inline void shift_wkb_header()
  {
    m_data+= WKB_HEADER_SIZE;
  }

  bool envelope(String *result) const;

protected:
  static Gis_class_info *find_class(int type_id);
  static Gis_class_info *find_class(const char *name, uint32 len);
  const char *append_points(String *txt, uint32 n_points,
			    const char *data, uint32 offset);
  bool create_point(String *result, const char *data);
  bool create_point(String *result, double x, double y);
  const char *get_mbr_for_points(MBR *mbr, const char *data, uint offset)
    const;

  inline bool no_data(const char *cur_data, uint32 data_amount) const
  {
    return (cur_data + data_amount > m_data_end);
  }
  const char *m_data;
  const char *m_data_end;
};


/***************************** Point *******************************/
 
class Gis_point: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  int get_mbr(MBR *mbr, const char **end) const;
  
  bool get_xy(double *x, double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2))
      return 1;
    float8get(*x, data);
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  bool get_x(double *x) const
  {
    if (no_data(m_data, SIZEOF_STORED_DOUBLE))
      return 1;
    float8get(*x, m_data);
    return 0;
  }

  bool get_y(double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2)) return 1;
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
};


/***************************** LineString *******************************/

class Gis_line_string: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool length(double *len) const;
  bool is_closed(int *closed) const;
  bool num_points(uint32 *n_points) const;
  bool start_point(String *point);
  bool end_point(String *point);
  bool point_n(uint32 n, String *result);
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
};


/***************************** Polygon *******************************/

class Gis_polygon: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool area(double *ar, const char **end) const;
  bool exterior_ring(String *result);
  bool num_interior_ring(uint32 *n_int_rings) const;
  bool interior_ring_n(uint32 num, String *result) const;
  bool centroid_xy(double *x, double *y) const;
  bool centroid(String *result);
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }
};


/***************************** MultiPoint *******************************/

class Gis_multi_point: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool num_geometries(uint32 *num) const;
  bool geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
};


/***************************** MultiLineString *******************************/

class Gis_multi_line_stringg: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool num_geometries(uint32 *num) const;
  bool geometry_n(uint32 num, String *result) const;
  bool length(double *len) const;
  bool is_closed(int *closed) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
};


/***************************** MultiPolygon *******************************/

class Gis_multi_polygon: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool num_geometries(uint32 *num) const;
  bool geometry_n(uint32 num, String *result) const;
  bool area(double *ar, const char **end) const;
  bool centroid(String *result);
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }

};


/*********************** GeometryCollection *******************************/

class Gis_geometry_collection: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  bool get_data_as_wkt(String *txt, const char **end);
  bool get_mbr(MBR *mbr, const char **end) const;
  bool num_geometries(uint32 *num) const;
  bool geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const;
};

#endif

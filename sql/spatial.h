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

const uint POINT_DATA_SIZE = 8+8; 
const uint WKB_HEADER_SIZE = 1+4;

struct stPoint2D
{
  double x;
  double y;
};

struct stLinearRing
{
  size_t n_points;
  stPoint2D points;
};

/***************************** MBR *******************************/

struct MBR
{
  MBR()
  {
    xmin=DBL_MAX;
    ymin=DBL_MAX;
    xmax=-DBL_MAX;
    ymax=-DBL_MAX;
  }

  MBR(const double &_xmin, const double &_ymin,
      const double &_xmax, const double &_ymax)
  {
    xmin=_xmin;
    ymin=_ymin;
    xmax=_xmax;
    ymax=_ymax;
  }

  MBR(const stPoint2D &min, const stPoint2D &max)
  {
    xmin=min.x;
    ymin=min.y;
    xmax=max.x;
    ymax=max.y;
  }
  
  double xmin;
  double ymin;
  double xmax;
  double ymax;
  
  void add_xy(double x, double y)
  {
    /* Not using "else" for proper one point MBR calculation */
    if (x<xmin)
    {
      xmin=x;
    }
    if (x>xmax)
    {
      xmax=x;
    }
    if (y<ymin)
    {
      ymin=y;
    }
    if (y>ymax)
    {
      ymax=y;
    }
  }

  void add_xy(const char *px, const char *py)
  {
    double x, y;
    float8get(x, px);
    float8get(y, py);
    /* Not using "else" for proper one point MBR calculation */
    if (x<xmin)
    {
      xmin=x;
    }
    if (x>xmax)
    {
      xmax=x;
    }
    if (y<ymin)
    {
      ymin=y;
    }
    if (y>ymax)
    {
      ymax=y;
    }
  }

  void add_mbr(const MBR *mbr)
  {
    if (mbr->xmin<xmin)
    {
      xmin=mbr->xmin;
    }
    if (mbr->xmax>xmax)
    {
      xmax=mbr->xmax;
    }
    if (mbr->ymin<ymin)
    {
      ymin=mbr->ymin;
    }
    if (mbr->ymax>ymax)
    {
      ymax=mbr->ymax;
    }
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
    int lb = mbr->inner_point(xmin, ymin);
    int rb = mbr->inner_point(xmax, ymin);
    int rt = mbr->inner_point(xmax, ymax);
    int lt = mbr->inner_point(xmin, ymax);

    int a  = lb+rb+rt+lt;
    return (a>0) && (a<4) && (!within(mbr));
  }
};


/***************************** Geometry *******************************/

class Geometry;

typedef int (Geometry::*GF_InitFromText)(GTextReadStream *, String *);
typedef int (Geometry::*GF_GetDataAsText)(String *) const;
typedef size_t (Geometry::*GF_GetDataSize)() const;
typedef int (Geometry::*GF_GetMBR)(MBR *) const;

typedef int (Geometry::*GF_GetD)(double *) const;
typedef int (Geometry::*GF_GetI)(int *) const;
typedef int (Geometry::*GF_GetUI)(uint32 *) const;
typedef int (Geometry::*GF_GetWS)(String *) const;
typedef int (Geometry::*GF_GetUIWS)(uint32, String *) const;

#define GEOM_METHOD_PRESENT(geom_obj, method)\
    (geom_obj.m_vmt->method != &Geometry::method)

class Geometry
{
public:
  enum wkbType
  {
    wkbPoint = 1,
    wkbLineString = 2,
    wkbPolygon = 3,
    wkbMultiPoint = 4,
    wkbMultiLineString = 5,
    wkbMultiPolygon = 6,
    wkbGeometryCollection = 7
  };
  enum wkbByteOrder
  {
    wkbXDR = 0,    /* Big Endian */
    wkbNDR = 1     /* Little Endian */
  };                                    

  class GClassInfo
  {
  public:
    GF_InitFromText init_from_wkt;
    GF_GetDataAsText get_data_as_wkt;
    GF_GetDataSize get_data_size;
    GF_GetMBR get_mbr;
    GF_GetD get_x;
    GF_GetD get_y;
    GF_GetD length;
    GF_GetD area;

    GF_GetI is_closed;

    GF_GetUI num_interior_ring;
    GF_GetUI num_points;
    GF_GetUI num_geometries;
    GF_GetUI dimension;

    GF_GetWS start_point;
    GF_GetWS end_point;
    GF_GetWS exterior_ring;
    GF_GetWS centroid;

    GF_GetUIWS point_n;
    GF_GetUIWS interior_ring_n;
    GF_GetUIWS geometry_n;

    int m_type_id;
    const char *m_name;
    GClassInfo *m_next_rt;
  };
  GClassInfo *m_vmt;

  const GClassInfo *get_class_info() const { return m_vmt; }
  size_t get_data_size() const { return (this->*m_vmt->get_data_size)(); }
  
  int init_from_wkt(GTextReadStream *trs, String *wkb) 
  { return (this->*m_vmt->init_from_wkt)(trs, wkb); }

  int get_data_as_wkt(String *txt) const
  { return (this->*m_vmt->get_data_as_wkt)(txt); }

  int get_mbr(MBR *mbr) const { return (this->*m_vmt->get_mbr)(mbr); }
  int dimension(uint32 *dim) const
  { return (this->*m_vmt->dimension)(dim); }

  int get_x(double *x) const { return (this->*m_vmt->get_x)(x); }
  int get_y(double *y) const { return (this->*m_vmt->get_y)(y); }
  int length(double *len) const  { return (this->*m_vmt->length)(len); }
  int area(double *ar) const  { return (this->*m_vmt->area)(ar); }

  int is_closed(int *closed) const
  { return (this->*m_vmt->is_closed)(closed); }

  int num_interior_ring(uint32 *n_int_rings) const 
  { return (this->*m_vmt->num_interior_ring)(n_int_rings); }
  int num_points(uint32 *n_points) const
  { return (this->*m_vmt->num_points)(n_points); }

  int num_geometries(uint32 *num) const
  { return (this->*m_vmt->num_geometries)(num); }

  int start_point(String *point) const
  { return (this->*m_vmt->start_point)(point); }
  int end_point(String *point) const
  { return (this->*m_vmt->end_point)(point); }
  int exterior_ring(String *ring) const
  { return (this->*m_vmt->exterior_ring)(ring); }
  int centroid(String *point) const
  { return (this->*m_vmt->centroid)(point); }

  int point_n(uint32 num, String *result) const
  { return (this->*m_vmt->point_n)(num, result); }
  int interior_ring_n(uint32 num, String *result) const
  { return (this->*m_vmt->interior_ring_n)(num, result); }
  int geometry_n(uint32 num, String *result) const
  { return (this->*m_vmt->geometry_n)(num, result); }

public:
  int create_from_wkb(const char *data, uint32 data_len);
  int create_from_wkt(GTextReadStream *trs, String *wkt, int init_stream=1);
  int init(int type_id)
  {
    m_vmt = find_class(type_id);
    return !m_vmt;
  }
  int new_geometry(const char *name, size_t len)
  {
    m_vmt = find_class(name, len);
    return !m_vmt;
  }

  int as_wkt(String *wkt) const
  {
    if (wkt->reserve(strlen(get_class_info()->m_name) + 2, 512))
      return 1;
    wkt->qs_append(get_class_info()->m_name);
    wkt->qs_append('(');
    if (get_data_as_wkt(wkt))
      return 1;
    wkt->qs_append(')');
    return 0;
  }

  void init_from_wkb(const char *data, uint32 data_len)
  {
    m_data = data;
    m_data_end = data + data_len;
  }

  void shift_wkb_header()
  {
    m_data += WKB_HEADER_SIZE;
  }

  int envelope(String *result) const;

protected:
  static GClassInfo *find_class(int type_id);
  static GClassInfo *find_class(const char *name, size_t len);

  bool no_data(const char *cur_data, uint32 data_amount) const
  {
    return (cur_data + data_amount > m_data_end);
  }

  const char *m_data;
  const char *m_data_end;
};

#define SIZEOF_STORED_DOUBLE 8

/***************************** Point *******************************/
 
class GPoint: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;
  
  int get_xy(double *x, double *y) const
  {
    const char *data = m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2)) return 1;
    float8get(*x, data);
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  int get_x(double *x) const
  {
    if (no_data(m_data, SIZEOF_STORED_DOUBLE)) return 1;
    float8get(*x, m_data);
    return 0;
  }

  int get_y(double *y) const
  {
    const char *data = m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2)) return 1;
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  int dimension(uint32 *dim) const { *dim = 0; return 0; }
};

/***************************** LineString *******************************/

class GLineString: public Geometry
{
public:
   size_t get_data_size() const;
   int init_from_wkt(GTextReadStream *trs, String *wkb);
   int get_data_as_wkt(String *txt) const;
   int get_mbr(MBR *mbr) const;
  
  int length(double *len) const;
  int is_closed(int *closed) const;
  int num_points(uint32 *n_points) const;
  int start_point(String *point) const;
  int end_point(String *point) const;
  int point_n(uint32 n, String *result) const;
  int dimension(uint32 *dim) const { *dim = 1; return 0; }
};

/***************************** Polygon *******************************/

class GPolygon: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;

  int area(double *ar) const;
  int exterior_ring(String *result) const;
  int num_interior_ring(uint32 *n_int_rings) const;
  int interior_ring_n(uint32 num, String *result) const;
  int centroid_xy(double *x, double *y) const;
  int centroid(String *result) const;
  int dimension(uint32 *dim) const { *dim = 2; return 0; }
};

/***************************** MultiPoint *******************************/

class GMultiPoint: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;

  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int dimension(uint32 *dim) const { *dim = 0; return 0; }
};

/***************************** MultiLineString *******************************/

class GMultiLineString: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;

  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int length(double *len) const;
  int is_closed(int *closed) const;
  int dimension(uint32 *dim) const { *dim = 1; return 0; }
};

/***************************** MultiPolygon *******************************/

class GMultiPolygon: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;

  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int area(double *ar) const;
  int centroid(String *result) const;
  int dimension(uint32 *dim) const { *dim = 2; return 0; }
};

/***************************** GeometryCollection *******************************/

class GGeometryCollection: public Geometry
{
public:
  size_t get_data_size() const;
  int init_from_wkt(GTextReadStream *trs, String *wkb);
  int get_data_as_wkt(String *txt) const;
  int get_mbr(MBR *mbr) const;

  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int dimension(uint32 *dim) const;
};

#endif

/*
   Copyright (c) 2002, 2013, Oracle and/or its affiliates.
   Copyright (c) 2009, 2013, Monty Program Ab.

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

#ifndef _spatial_h
#define _spatial_h

#include "sql_string.h"                         /* String, LEX_STRING */
#include <my_compiler.h>

#ifdef HAVE_SPATIAL

class Gis_read_stream;

#include "gcalc_tools.h"

const uint SRID_SIZE= 4;
const uint SIZEOF_STORED_DOUBLE= 8;
const uint POINT_DATA_SIZE= (SIZEOF_STORED_DOUBLE * 2); 
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
  void buffer(double d)
  {
    xmin-= d;
    ymin-= d;
    xmax+= d;
    ymax+= d;
  }

  int equals(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin == xmin) && (mbr->ymin == ymin) &&
	    (mbr->xmax == xmax) && (mbr->ymax == ymax));
  }

  int disjoint(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin > xmax) || (mbr->ymin > ymax) ||
	    (mbr->xmax < xmin) || (mbr->ymax < ymin));
  }

  int intersects(const MBR *mbr)
  {
    return !disjoint(mbr);
  }

  int touches(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin == xmax || mbr->xmax == xmin) &&
            ((mbr->ymin >= ymin && mbr->ymin <= ymax) ||
             (mbr->ymax >= ymin && mbr->ymax <= ymax))) ||
           ((mbr->ymin == ymax || mbr->ymax == ymin) &&
            ((mbr->xmin >= xmin && mbr->xmin <= xmax) ||
             (mbr->xmax >= xmin && mbr->xmax <= xmax)));
  }

  int within(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin <= xmin) && (mbr->ymin <= ymin) &&
	    (mbr->xmax >= xmax) && (mbr->ymax >= ymax));
  }

  int contains(const MBR *mbr)
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin >= xmin) && (mbr->ymin >= ymin) &&
	    (mbr->xmax <= xmax) && (mbr->ymax <= ymax));
  }

  bool inner_point(double x, double y) const
  {
    /* The following should be safe, even if we compare doubles */
    return (xmin<x) && (xmax>x) && (ymin<y) && (ymax>y);
  }

  /**
    The dimension maps to an integer as:
    - Polygon -> 2
    - Horizontal or vertical line -> 1
    - Point -> 0
    - Invalid MBR -> -1
  */
  int dimension() const
  {
    int d= 0;

    if (xmin > xmax)
      return -1;
    else if (xmin < xmax)
      d++;

    if (ymin > ymax)
      return -1;
    else if (ymin < ymax)
      d++;

    return d;
  }

  int overlaps(const MBR *mbr)
  {
    /*
      overlaps() requires that some point inside *this is also inside
      *mbr, and that both geometries and their intersection are of the
      same dimension.
    */
    int d = dimension();

    if (d != mbr->dimension() || d <= 0 || contains(mbr) || within(mbr))
      return 0;

    MBR intersection(max(xmin, mbr->xmin), max(ymin, mbr->ymin),
                     min(xmax, mbr->xmax), min(ymax, mbr->ymax));

    return (d == intersection.dimension());
  }

  int valid() const
  { return xmin <= xmax && ymin <= ymax; }
};


/***************************** Geometry *******************************/

struct Geometry_buffer;

class Geometry
{
public:
  Geometry() {}                               /* Remove gcc warning */
  virtual ~Geometry() {}                        /* Remove gcc warning */
  static void *operator new(size_t size, void *buffer)
  {
    return buffer;
  }

  static void operator delete(void *ptr, void *buffer)
  {}

  static void operator delete(void *buffer)
  {}

  static String bad_geometry_data;

  enum wkbType
  {
    wkb_point= 1,
    wkb_linestring= 2,
    wkb_polygon= 3,
    wkb_multipoint= 4,
    wkb_multilinestring= 5,
    wkb_multipolygon= 6,
    wkb_geometrycollection= 7,
    wkb_last=7
  };
  enum wkbByteOrder
  {
    wkb_xdr= 0,    /* Big Endian */
    wkb_ndr= 1     /* Little Endian */
  };

  /** Callback which creates Geometry objects on top of a given placement. */
  typedef Geometry *(*create_geom_t)(char *);

  class Class_info
  {
  public:
    LEX_STRING m_name;
    int m_type_id;
    create_geom_t m_create_func;
    Class_info(const char *name, int type_id, create_geom_t create_func);
  };

  virtual const Class_info *get_class_info() const=0;
  virtual uint32 get_data_size() const=0;
  virtual bool init_from_wkt(Gis_read_stream *trs, String *wkb)=0;
  /* returns the length of the wkb that was read */
  virtual uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo,
                             String *res)=0;
  virtual uint init_from_opresult(String *bin,
                                  const char *opres, uint res_len)
  { return init_from_wkb(opres + 4, UINT_MAX32, wkb_ndr, bin) + 4; }

  virtual bool get_data_as_wkt(String *txt, const char **end) const=0;
  virtual bool get_mbr(MBR *mbr, const char **end) const=0;
  virtual bool dimension(uint32 *dim, const char **end) const=0;
  virtual int get_x(double *x) const { return -1; }
  virtual int get_y(double *y) const { return -1; }
  virtual int geom_length(double *len, const char **end) const  { return -1; }
  virtual int area(double *ar, const char **end) const { return -1;}
  virtual int is_closed(int *closed) const { return -1; }
  virtual int num_interior_ring(uint32 *n_int_rings) const { return -1; }
  virtual int num_points(uint32 *n_points) const { return -1; }
  virtual int num_geometries(uint32 *num) const { return -1; }
  virtual int start_point(String *point) const { return -1; }
  virtual int end_point(String *point) const { return -1; }
  virtual int exterior_ring(String *ring) const { return -1; }
  virtual int centroid(String *point) const { return -1; }
  virtual int point_n(uint32 num, String *result) const { return -1; }
  virtual int interior_ring_n(uint32 num, String *result) const { return -1; }
  virtual int geometry_n(uint32 num, String *result) const { return -1; }
  virtual int store_shapes(Gcalc_shape_transporter *trn) const=0;

public:
  static Geometry *create_by_typeid(Geometry_buffer *buffer, int type_id);

  static Geometry *construct(Geometry_buffer *buffer,
                             const char *data, uint32 data_len);
  static Geometry *create_from_wkt(Geometry_buffer *buffer,
				   Gis_read_stream *trs, String *wkt,
				   bool init_stream=1);
  static Geometry *create_from_wkb(Geometry_buffer *buffer,
                                   const char *wkb, uint32 len, String *res);
  static int create_from_opresult(Geometry_buffer *g_buf,
                                  String *res, Gcalc_result_receiver &rr);
  int as_wkt(String *wkt, const char **end);

  inline void set_data_ptr(const char *data, uint32 data_len)
  {
    m_data= data;
    m_data_end= data + data_len;
  }

  inline void shift_wkb_header()
  {
    m_data+= WKB_HEADER_SIZE;
  }

  bool envelope(String *result) const;
  static Class_info *ci_collection[wkb_last+1];

protected:
  static Class_info *find_class(int type_id)
  {
    return ((type_id < wkb_point) || (type_id > wkb_last)) ?
      NULL : ci_collection[type_id];
  }  
  static Class_info *find_class(const char *name, uint32 len);
  const char *append_points(String *txt, uint32 n_points,
			    const char *data, uint32 offset) const;
  bool create_point(String *result, const char *data) const;
  bool create_point(String *result, double x, double y) const;
  const char *get_mbr_for_points(MBR *mbr, const char *data, uint offset)
    const;

  /**
     Check if there're enough data remaining as requested

     @arg cur_data     pointer to the position in the binary form
     @arg data_amount  number of points expected
     @return           true if not enough data
  */
  inline bool no_data(const char *cur_data, size_t data_amount) const
  {
    return (cur_data + data_amount > m_data_end);
  }

  /**
     Check if there're enough points remaining as requested

     Need to perform the calculation in logical units, since multiplication
     can overflow the size data type.

     @arg data              pointer to the begining of the points array
     @arg expected_points   number of points expected
     @arg extra_point_space extra space for each point element in the array
     @return               true if there are not enough points
  */
  inline bool not_enough_points(const char *data, uint32 expected_points,
                                uint32 extra_point_space = 0) const
  {
    return (m_data_end < data ||
            (expected_points > ((m_data_end - data) /
                                (POINT_DATA_SIZE + extra_point_space))));
  }
  const char *m_data;
  const char *m_data_end;
};


/***************************** Point *******************************/
 
class Gis_point: public Geometry
{
public:
  Gis_point() {}                              /* Remove gcc warning */
  virtual ~Gis_point() {}                     /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  
  int get_xy(double *x, double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2))
      return 1;
    float8get(*x, data);
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  int get_x(double *x) const
  {
    if (no_data(m_data, SIZEOF_STORED_DOUBLE))
      return 1;
    float8get(*x, m_data);
    return 0;
  }

  int get_y(double *y) const
  {
    const char *data= m_data;
    if (no_data(data, SIZEOF_STORED_DOUBLE * 2)) return 1;
    float8get(*y, data + SIZEOF_STORED_DOUBLE);
    return 0;
  }

  int geom_length(double *len, const char **end) const;
  int area(double *ar, const char **end) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};


/***************************** LineString *******************************/

class Gis_line_string: public Geometry
{
public:
  Gis_line_string() {}                        /* Remove gcc warning */
  virtual ~Gis_line_string() {}               /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int geom_length(double *len, const char **end) const;
  int area(double *ar, const char **end) const;
  int is_closed(int *closed) const;
  int num_points(uint32 *n_points) const;
  int start_point(String *point) const;
  int end_point(String *point) const;
  int point_n(uint32 n, String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};


/***************************** Polygon *******************************/

class Gis_polygon: public Geometry
{
public:
  Gis_polygon() {}                            /* Remove gcc warning */
  virtual ~Gis_polygon() {}                   /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  uint init_from_opresult(String *bin, const char *opres, uint res_len);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int area(double *ar, const char **end) const;
  int exterior_ring(String *result) const;
  int num_interior_ring(uint32 *n_int_rings) const;
  int interior_ring_n(uint32 num, String *result) const;
  int centroid_xy(double *x, double *y) const;
  int centroid(String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};


/***************************** MultiPoint *******************************/

class Gis_multi_point: public Geometry
{
  // Maximum number of points in MultiPoint that can fit into String
  static const uint32 max_n_points=
    (uint32) (UINT_MAX32 - WKB_HEADER_SIZE - 4 /* n_points */) /
    (WKB_HEADER_SIZE + POINT_DATA_SIZE);
public:
  Gis_multi_point() {}                        /* Remove gcc warning */
  virtual ~Gis_multi_point() {}               /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  uint init_from_opresult(String *bin, const char *opres, uint res_len);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 0;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};


/***************************** MultiLineString *******************************/

class Gis_multi_line_string: public Geometry
{
public:
  Gis_multi_line_string() {}                  /* Remove gcc warning */
  virtual ~Gis_multi_line_string() {}         /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  uint init_from_opresult(String *bin, const char *opres, uint res_len);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int geom_length(double *len, const char **end) const;
  int is_closed(int *closed) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 1;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};


/***************************** MultiPolygon *******************************/

class Gis_multi_polygon: public Geometry
{
public:
  Gis_multi_polygon() {}                      /* Remove gcc warning */
  virtual ~Gis_multi_polygon() {}             /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int area(double *ar, const char **end) const;
  int centroid(String *result) const;
  bool dimension(uint32 *dim, const char **end) const
  {
    *dim= 2;
    *end= 0;					/* No default end */
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
  uint init_from_opresult(String *bin, const char *opres, uint res_len);
};


/*********************** GeometryCollection *******************************/

class Gis_geometry_collection: public Geometry
{
public:
  Gis_geometry_collection() {}                /* Remove gcc warning */
  virtual ~Gis_geometry_collection() {}       /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  uint init_from_opresult(String *bin, const char *opres, uint res_len);
  bool get_data_as_wkt(String *txt, const char **end) const;
  bool get_mbr(MBR *mbr, const char **end) const;
  int area(double *ar, const char **end) const;
  int geom_length(double *len, const char **end) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, const char **end) const;
  int store_shapes(Gcalc_shape_transporter *trn) const;
  const Class_info *get_class_info() const;
};

struct Geometry_buffer : public
  my_aligned_storage<sizeof(Gis_point), MY_ALIGNOF(Gis_point)> {};

#endif /*HAVE_SPATIAL*/
#endif

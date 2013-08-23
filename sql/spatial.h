/* Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _spatial_h
#define _spatial_h

#include "sql_string.h"                         /* String, LEX_STRING */
#include <my_compiler.h>

#ifdef HAVE_SPATIAL

#include "gcalc_tools.h"

#include <algorithm>

const uint SRID_SIZE= 4;
const uint SIZEOF_STORED_DOUBLE= 8;
const uint POINT_DATA_SIZE= (SIZEOF_STORED_DOUBLE * 2); 
const uint WKB_HEADER_SIZE= 1+4;
const uint32 GET_SIZE_ERROR= ((uint32) -1);

   
/**
  Point with coordinates X and Y.
*/ 
class point_xy
{
public:
  double x;
  double y;
  point_xy() { }
  point_xy(double x_arg, double y_arg): x(x_arg), y(y_arg) { } 
  /**
    Distance to another point.
  */
  double distance(point_xy p)
  {
    return sqrt(pow(x - p.x, 2) + pow(y - p.y, 2));
  }
  /**
    Compare to another point.
    Return true if equal, false if not equal.
  */
  bool eq(point_xy p)
  {  
    return (x == p.x) && (y == p.y);
  }
};

typedef struct wkb_header_st
{
  uchar byte_order;
  uint32 wkb_type;
} wkb_header;


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

  MBR(const point_xy &min, const point_xy &max)
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
  void add_xy(point_xy p)
  {
    add_xy(p.x, p.y);
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

    using std::min;
    using std::max;
    MBR intersection(max(xmin, mbr->xmin), max(ymin, mbr->ymin),
                     min(xmax, mbr->xmax), min(ymax, mbr->ymax));

    return (d == intersection.dimension());
  }
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

  /**
    Constant storage for WKB.
    Encapsulation and the available methods make it impossible
    to update the members of wkb_container once it is initialized.
    The only allowed modification method is set(),
    which fully replaces the previous buffer.  
  */ 
  class wkb_container
  {  
  protected:
    const char *m_data;
    const char *m_data_end;
  public:
    wkb_container() { }
    wkb_container(const char *data, const char *data_end)
    {
      set(data, data_end);
    }
    void set(const char *data, const char *data_end)
    {
      m_data= data;
      m_data_end= data_end;
    }
    const char *data() const
    {
      return m_data;
    }
    const char *data_end() const
    {
      return m_data_end;
    }
    uint32 length() const
    {
      return (uint32) (m_data_end - m_data);   
    }
    /**
      Check if there's enough data remaining as requested.

      @arg data_amount  data requested

      @return           true if not enough data
    */
    inline bool no_data(size_t data_amount) const
    {
      return (m_data + data_amount > m_data_end);
    }

    /**
      Check if there're enough points remaining as requested.

      Need to perform the calculation in logical units, since multiplication
      can overflow the size data type.

      @arg expected_points   number of points expected
      @arg extra_point_space extra space for each point element in the array

      @return               true if there are not enough points
    */
    inline bool not_enough_points(uint32 expected_points,
                                  uint32 extra_point_space= 0) const
    {
      return (m_data_end < m_data ||
              expected_points > ((m_data_end - m_data) /
                                 (POINT_DATA_SIZE + extra_point_space)));
    }
  };

  /**
    WKB parser, designed to traverse through WKB data from
    beginning of the buffer towards the end using a set   
    of scan_xxx(), get_xxx() and skip_xxx() routines,
    with safety tests to avoid going beyond the buffer end.
  */
  class wkb_parser: public wkb_container
  {
    /* Low level routines to get data of various types */
    void get_uint4(uint32 *number)
    {
      *number= uint4korr(m_data); //GIS-TODO: byte order
    }
    void get_float8(double *x)
    {
      float8get(*x, m_data);      //GIS-TODO: byte order
    }
  public:
    wkb_parser(const char *data, const char *data_end):
      wkb_container(data, data_end) { }
    wkb_parser(const wkb_container *container):
      wkb_container(*container) { }

    /* Routines to skip non-interesting data */
    void skip_unsafe(size_t nbytes)
    {
      DBUG_ASSERT(!no_data(nbytes));
      m_data+= nbytes;
    }
    bool skip(size_t nbytes)
    {
      if (no_data(nbytes))
        return true;
      m_data+= nbytes;
      return false;   
    }
    bool skip_wkb_header()
    {
      return skip(WKB_HEADER_SIZE);
    }
    bool skip_coord()
    {
      return skip(SIZEOF_STORED_DOUBLE);
    }

    /* Routines to scan wkb header information */
    bool scan_wkb_header(wkb_header *header)
    {
      if (no_data(WKB_HEADER_SIZE))
        return true;
      header->byte_order= (uchar) (*m_data);
      m_data++;
      get_uint4(&header->wkb_type);
      m_data+= 4;
      return false;
    }

    /* Routines to scan uint4 information */
    bool scan_uint4(uint32 *number)
    {
      if (no_data(4))
        return true; 
      get_uint4(number);
      m_data+= 4;
      return false;
    }
    bool scan_non_zero_uint4(uint32 *number)
    {
      return (scan_uint4(number) || 0 == *number);
    }
    bool scan_n_points_and_check_data(uint32 *n_points,
                                      uint32 extra_point_space= 0)
    {
      return scan_non_zero_uint4(n_points) ||
             not_enough_points(*n_points, extra_point_space);
    }

    /* Routines to scan coordinate information */
    void scan_xy_unsafe(point_xy *p)
    {
      DBUG_ASSERT(!no_data(POINT_DATA_SIZE));
      get_float8(&p->x);
      m_data+= SIZEOF_STORED_DOUBLE;
      get_float8(&p->y);
      m_data+= SIZEOF_STORED_DOUBLE;
    }
    bool scan_xy(point_xy *p)
    {
     if (no_data(SIZEOF_STORED_DOUBLE * 2))
        return true;
      scan_xy_unsafe(p);
      return false;
    }
    bool scan_coord(double *x)
    {
      if (no_data(SIZEOF_STORED_DOUBLE))
        return true;
      get_float8(x);
      m_data+= SIZEOF_STORED_DOUBLE;
      return false;
    }
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
                                  const char *opres, uint opres_length)
  { return init_from_wkb(opres + 4, UINT_MAX32, wkb_ndr, bin) + 4; }

  virtual bool get_data_as_wkt(String *txt, wkb_parser *wkb) const=0;
  virtual bool get_mbr(MBR *mbr, wkb_parser *wkb) const=0;
  bool get_mbr(MBR *mbr)
  {
    wkb_parser wkb(&m_wkb_data);
    return get_mbr(mbr, &wkb);
  }
  virtual bool dimension(uint32 *dim, wkb_parser *wkb) const
  {
    *dim= feature_dimension();
    uint32 length;
    if ((length= get_data_size()) == GET_SIZE_ERROR)
      return true;
    wkb->skip(length);
    return false;   
  }
  bool dimension(uint32 *dim)
  {  
    wkb_parser wkb(&m_wkb_data);
    return dimension(dim, &wkb);
  }
  virtual uint32 feature_dimension() const= 0;
  virtual int get_x(double *x) const { return -1; }
  virtual int get_y(double *y) const { return -1; }
  virtual int geom_length(double *len) const  { return -1; }
  /**
    Calculate area of a Geometry.
    This default implementation returns 0 for the types that have zero area:
    Point, LineString, MultiPoint, MultiLineString.
    The over geometry types (Polygon, MultiPolygon, GeometryCollection)
    override the default method.
  */
  virtual bool area(double *ar, wkb_parser *wkb) const
  {
    uint32 data_size= get_data_size();
    if (data_size == GET_SIZE_ERROR || wkb->no_data(data_size))
      return true;
    wkb->skip_unsafe(data_size);
    *ar= 0;
    return false;
  }
  bool area(double *ar) const
  {
    wkb_parser wkb(&m_wkb_data);
    return area(ar, &wkb);
  }
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
  virtual int store_shapes(Gcalc_shape_transporter *trn,
                           Gcalc_shape_status *st) const=0;
  int store_shapes(Gcalc_shape_transporter *trn) const
  {
    Gcalc_shape_status dummy;
    return store_shapes(trn, &dummy);
  }

public:
  static Geometry *create_by_typeid(Geometry_buffer *buffer, int type_id);
  static Geometry *scan_header_and_create(wkb_parser *wkb, Geometry_buffer *buffer)
  {
    Geometry *geom;
    wkb_header header;
    
    if (wkb->scan_wkb_header(&header) ||
        !(geom= create_by_typeid(buffer, header.wkb_type)))
      return NULL;
    geom->set_data_ptr(wkb);
    return geom;
  }

  static Geometry *construct(Geometry_buffer *buffer,
                             const char *data, uint32 data_len);
  static Geometry *construct(Geometry_buffer *buffer, const String *str)
  {
    return construct(buffer, str->ptr(), str->length());
  }
  static Geometry *create_from_wkt(Geometry_buffer *buffer,
				   Gis_read_stream *trs, String *wkt,
				   bool init_stream=1);
  static Geometry *create_from_wkb(Geometry_buffer *buffer,
                                   const char *wkb, uint32 len, String *res);
  static int create_from_opresult(Geometry_buffer *g_buf,
                                  String *res, Gcalc_result_receiver &rr);
  bool as_wkt(String *wkt, wkb_parser *wkb)
  {
    uint32 len= (uint) get_class_info()->m_name.length;
    if (wkt->reserve(len + 2, 512))
      return true;
    wkt->qs_append(get_class_info()->m_name.str, len);
    wkt->qs_append('(');
    if (get_data_as_wkt(wkt, wkb))
      return true;
    wkt->qs_append(')');
    return false;
  }
  bool as_wkt(String *wkt)
  {
    wkb_parser wkb(&m_wkb_data);
    return as_wkt(wkt, &wkb);   
  }

  inline void set_data_ptr(const char *data, uint32 data_len)
  {
    m_wkb_data.set(data, data + data_len);
  }

  inline void set_data_ptr(const wkb_container *wkb)
  {
    m_wkb_data= *wkb;
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
  void append_points(String *txt, uint32 n_points,
                     wkb_parser *wkb, uint32 offset) const;
  bool create_point(String *result, wkb_parser *wkb) const; 
  bool create_point(String *result, point_xy p) const;
  bool get_mbr_for_points(MBR *mbr, wkb_parser *wkb, uint offset) const;
  wkb_container m_wkb_data;
  /**
    Store shapes of a collection:
    GeometryCollection, MultiPoint, MultiLineString or MultiPolygon.

    In case when collection is GeometryCollection, NULL should be passed as 
    "collection_item" argument. Proper collection item objects will be
    created inside collection_store_shapes, according to the geometry type of
    every item in the collection.

    For MultiPoint, MultiLineString or MultiPolygon, an address of a
    pre-allocated item object of Gis_point, Gis_line_string or Gis_polygon
    can be passed for better performance.
  */
  int collection_store_shapes(Gcalc_shape_transporter *trn,
                              Gcalc_shape_status *st,
                              Geometry *collection_item) const;
  /**
    Calculate area of a collection:
    GeometryCollection, MultiPoint, MultiLineString or MultiPolygon.
    
    The meaning of the "collection_item" is the same to
    the similar argument in collection_store_shapes().
  */
  bool collection_area(double *ar, wkb_parser *wkb, Geometry *it) const;

  /**
    Initialize a collection from an operation result.
    Share between: GeometryCollection, MultiLineString, MultiPolygon.
    The meaning of the "collection_item" is the same to
    the similare agument in collection_store_shapes().
  */
  uint collection_init_from_opresult(String *bin,
                                     const char *opres, uint opres_length,
                                     Geometry *collection_item);

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
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const; 
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;

  int get_xy(point_xy *p) const
  {
    wkb_parser wkb(&m_wkb_data);
    return wkb.scan_xy(p);
  }
  int get_x(double *x) const
  {
    wkb_parser wkb(&m_wkb_data);
    return wkb.scan_coord(x);
  }
  int get_y(double *y) const
  {
    wkb_parser wkb(&m_wkb_data);
    return wkb.skip_coord() || wkb.scan_coord(y);
  }
  uint32 feature_dimension() const { return 0; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
  const Class_info *get_class_info() const;
};


/***************************** LineString *******************************/

class Gis_line_string: public Geometry
{
  // Maximum number of points in LineString that can fit into String
  static const uint32 max_n_points=
    (uint32) (UINT_MAX32 - WKB_HEADER_SIZE - 4 /* n_points */) /
    POINT_DATA_SIZE;
public:
  Gis_line_string() {}                        /* Remove gcc warning */
  virtual ~Gis_line_string() {}               /* Remove gcc warning */
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  int geom_length(double *len) const;
  int is_closed(int *closed) const;
  int num_points(uint32 *n_points) const;
  int start_point(String *point) const;
  int end_point(String *point) const;
  int point_n(uint32 n, String *result) const;
  uint32 feature_dimension() const { return 1; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
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
  uint init_from_opresult(String *bin, const char *opres, uint opres_length);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  bool area(double *ar, wkb_parser *wkb) const;
  int exterior_ring(String *result) const;
  int num_interior_ring(uint32 *n_int_rings) const;
  int interior_ring_n(uint32 num, String *result) const;
  bool centroid_xy(point_xy *p) const;
  int centroid(String *result) const;
  uint32 feature_dimension() const { return 2; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
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
  uint init_from_opresult(String *bin, const char *opres, uint opres_length);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  uint32 feature_dimension() const { return 0; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
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
  uint init_from_opresult(String *bin, const char *opres, uint opres_length);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  int geom_length(double *len) const;
  int is_closed(int *closed) const;
  uint32 feature_dimension() const { return 1; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
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
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool area(double *ar, wkb_parser *wkb) const;
  int centroid(String *result) const;
  uint32 feature_dimension() const { return 2; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
  const Class_info *get_class_info() const;
  uint init_from_opresult(String *bin, const char *opres, uint opres_length);
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
  uint init_from_opresult(String *bin, const char *opres, uint opres_length);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;
  bool area(double *ar, wkb_parser *wkb) const; 
  int num_geometries(uint32 *num) const;
  int geometry_n(uint32 num, String *result) const;
  bool dimension(uint32 *dim, wkb_parser *wkb) const; 
  uint32 feature_dimension() const
  {
    DBUG_ASSERT(0);
    return 0;
  }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
  const Class_info *get_class_info() const;
};

struct Geometry_buffer : public
  my_aligned_storage<sizeof(Gis_point), MY_ALIGNOF(Gis_point)> {};

#endif /*HAVE_SPATAIAL*/
#endif

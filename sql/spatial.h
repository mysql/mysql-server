/* Copyright (c) 2002, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SPATIAL_INCLUDED
#define SPATIAL_INCLUDED

#include "my_global.h"
#include "mysql/mysql_lex_string.h"     // LEX_STRING
#include "gcalc_tools.h"
#include "mysqld.h"
#include "sql_string.h"                 // String

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <utility>
#include <memory>
#include "inplace_vector.h"


class Gis_read_stream;

const uint GEOM_DIM= 2;
const uint SRID_SIZE= 4;
const uint SIZEOF_STORED_DOUBLE= 8;
const uint POINT_DATA_SIZE= (SIZEOF_STORED_DOUBLE * 2);
const uint WKB_HEADER_SIZE= (1+4);
const uint GEOM_HEADER_SIZE= (SRID_SIZE + WKB_HEADER_SIZE);

const uint32 GET_SIZE_ERROR= 0xFFFFFFFFU;


inline bool is_little_endian()
{
#ifdef WORDS_BIGENDIAN
  return false;
#else
  return true;
#endif
}


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
  double distance(const point_xy &p) const;
  /**
    Compare to another point.
    Return true if equal, false if not equal.
  */
  bool eq(point_xy p) const
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

  void add_xy(double x, double y)
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
    float8get(&x, px);
    float8get(&y, py);
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

  int equals(const MBR *mbr) const
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin == xmin) && (mbr->ymin == ymin) &&
	    (mbr->xmax == xmax) && (mbr->ymax == ymax));
  }

  int disjoint(const MBR *mbr) const
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin > xmax) || (mbr->ymin > ymax) ||
	    (mbr->xmax < xmin) || (mbr->ymax < ymin));
  }

  int intersects(const MBR *mbr) const
  {
    return !disjoint(mbr);
  }

  int touches(const MBR *mbr) const;

  int within(const MBR *mbr) const;

  int contains(const MBR *mbr) const
  {
    return mbr->within(this);
  }

  int covered_by(const MBR *mbr) const
  {
    /* The following should be safe, even if we compare doubles */
    return ((mbr->xmin <= xmin) && (mbr->ymin <= ymin) &&
            (mbr->xmax >= xmax) && (mbr->ymax >= ymax));
  }

  int covers(const MBR *mbr) const
  {
    return mbr->covered_by(this);
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

  int overlaps(const MBR *mbr) const
  {
    /*
      overlaps() requires that some point inside *this is also inside
      *mbr, and that both geometries and their intersection are of the
      same dimension.
    */
    int d= dimension();
    DBUG_ASSERT(d >= 0 && d <= 2);

    if (d != mbr->dimension() || d == 0 || contains(mbr) || within(mbr))
      return 0;

    MBR intersection(std::max(xmin, mbr->xmin), std::max(ymin, mbr->ymin),
                     std::min(xmax, mbr->xmax), std::min(ymax, mbr->ymax));

    return (d == intersection.dimension());
  }
};


/***************************** Geometry *******************************/

struct Geometry_buffer;

/*
  Memory management functions for BG adapter code. Allocate extra space for
  GEOMETRY header so that we can later prefix the header if needed.
 */
inline void *gis_wkb_alloc(size_t sz)
{
  sz+= GEOM_HEADER_SIZE;
  char *p= static_cast<char *>(my_malloc(key_memory_Geometry_objects_data,
                                         sz, MYF(MY_FAE)));
  p+= GEOM_HEADER_SIZE;
  return p;
}


inline void *gis_wkb_fixed_alloc(size_t sz)
{
  return gis_wkb_alloc(sz);
}


inline void *gis_wkb_realloc(void *p, size_t sz)
{
  char *cp= static_cast<char *>(p);
  if (cp)
    cp-= GEOM_HEADER_SIZE;
  sz+= GEOM_HEADER_SIZE;

  p= my_realloc(key_memory_Geometry_objects_data, cp, sz, MYF(MY_FAE));
  cp= static_cast<char *>(p);
  return cp + GEOM_HEADER_SIZE;
}


inline void gis_wkb_free(void *p)
{
  if (p == NULL)
    return;
  char *cp= static_cast<char *>(p);
  my_free(cp - GEOM_HEADER_SIZE);
}


inline void gis_wkb_raw_free(void *p)
{
  my_free(p);
}


class Geometry
{
  friend void parse_wkb_data(Geometry *g, const char *p, size_t num_geoms);
protected:
  // Flag bits for m_flags.props.

  /*
    Whether the linestring is a polygon's outer ring, or inner ring.
   */
  const static int POLYGON_OUTER_RING= 0x1;
  const static int POLYGON_INNER_RING= 0x2;

  /*
    Whether the Geometry object is created to be used by Boost Geometry or
    only by MySQL. There are some operations that only work for one type and
    can or must be skipped otherwise. This state is transient and mutable, we
    set it even to a const geometry object.
   */
  const static int IS_BOOST_GEOMETRY_ADAPTER= 0x4;

  /*
    Whether the geometry length is verified, so that we can return the stored
    length without having to parse the WKB again.
   */
  const static int GEOM_LENGTH_VERIFIED= 0x8;

  /*
    Whether the geometry has components stored out of line, see
    Gis_wkb_vector<>::resize for details.
   */
  const static int HAS_OUT_OF_LINE_COMPONENTS= 0x10;

  /*
    Whether the polygon's data is in WKB form, as is so in MySQL, or it's in
    BG form, where the m_ptr points to an outer ring object, and m_inn_rings
    points to the inner rings. See Gis_polygon for more information.
   */
  const static int POLYGON_IN_WKB_FORM= 0x20;

  /*
    whether the geometry's data buffer has space for a GEOMETRY header.
    BG adapter code use gis_wkb_alloc to allocate WKB buffer for Geometry
    objects, they always has such space. Gis_geometry_collection created
    from a single geometry and then appended with more geometries also have
    such space. Those with such space we can simply prefix the GEOMETRY header
    into its buffer without copying its WKB data.
   */
  const static int HAS_GEOM_HEADER_SPACE= 0x40;

  /*
    Whether the multi geometry has overlapped components, if false(the bit set)
    this geometry will be skipped from merge-component operation.
    Effective only for multipolygons, multilinestrings and geometry collections.
    Such geometries returned by BG always has this bit set, i.e. their
    components don't overlap.
  */
  const static int MULTIPOLYGON_NO_OVERLAPPED_COMPS= 0x80;
public:
  // Check user's transmitted data against these limits.
  const static uint32 MAX_GEOM_WKB_LENGTH= 0x3fffffff;

  typedef uint32 srid_t;
  const static srid_t default_srid= 0;

  virtual ~Geometry();


  /*
    We have to define a wkb_first and wkb_invalid_type and set them to 0
    because Geometry objects stored in m_geo_vect vector can be constructed
    using the default constructur Geometry() which sets geotype to 0, and
    there are asserts in BG adapter code which asserts geotype is in valid
    range [wkb_first, wkb_last]. Neither items will be treated as valid
    geometry types.

    wkb_first and wkb_last are only intended to be used to express a valid
    range of wkbType values, other items are to be used as real type values.
   */
  enum wkbType
  {
    wkb_invalid_type= 0,
    wkb_first= 1,
    wkb_point= 1,
    wkb_linestring= 2,
    wkb_polygon= 3,
    wkb_multipoint= 4,
    wkb_multilinestring= 5,
    wkb_multipolygon= 6,
    wkb_geometrycollection= 7,
    /*
      OGC defines 10 more basic geometry types for values 8 to 17, we don't
      support them now so don't define them. And there may be more of
      them defined in the future. Since we will need 5 bits anyway, we grow
      from 31 down to 18 for our extra private types instead of from 18 to 31,
      to avoid potential data file format binary compatibility issues, which
      would occur if OGC defined more basic types and we would support them.
     */
    wkb_polygon_inner_rings= 31,
    wkb_last=31
  };
  enum wkbByteOrder
  {
    wkb_xdr= 0,    /* Big Endian */
    wkb_ndr= 1,    /* Little Endian */
    wkb_invalid
  };
  enum enum_coordinate_reference_system
  {
    coord_first= 1,
    cartesian= 1,
    coord_last= 1
  };


  static String bad_geometry_data;

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
    bool no_data(size_t data_amount) const
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
    bool not_enough_points(uint32 expected_points,
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
      float8get(x, m_data);      //GIS-TODO: byte order
    }
  public:
    wkb_parser(const char *data, const char *data_end)
      :wkb_container(data, data_end) { }

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

  virtual const Class_info *get_class_info() const { return NULL; }

  virtual uint32 get_data_size() const { return -1; }

  /* read from trs the wkt string and write into wkb as wkb encoded data. */
  virtual bool init_from_wkt(Gis_read_stream *trs, String *wkb) { return true;}

  /* read from wkb the wkb data and write into res as wkb encoded data. */
  /* returns the length of the wkb that was read */
  virtual uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo,
                             String *res) { return 0; }

  virtual uint init_from_opresult(String *bin,
                                  const char *opres, uint opres_length)
  { return init_from_wkb(opres + 4, UINT_MAX32, wkb_ndr, bin) + 4; }

  virtual bool get_data_as_wkt(String *txt, wkb_parser *wkb) const
  { return true;}
  virtual bool get_mbr(MBR *mbr, wkb_parser *wkb) const
  { return true;}
  bool get_mbr(MBR *mbr)
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
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
  bool dimension(uint32 *dim) const
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return dimension(dim, &wkb);
  }
  wkbType get_type() const
  {
    return static_cast<Geometry::wkbType>(get_class_info()->m_type_id);
  }
  enum_coordinate_reference_system get_coordsys() const
  {
    return cartesian;
  }
  virtual uint32 feature_dimension() const
  {
    DBUG_ASSERT(false);
    return 0;
  }

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
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return area(ar, &wkb);
  }
  virtual int is_closed(int *closed) const { return -1; }
  virtual int num_interior_ring(uint32 *n_int_rings) const { return -1; }
  virtual int num_points(uint32 *n_points) const { return -1; }
  virtual int num_geometries(uint32 *num) const { return -1; }
  virtual int copy_points(String *result) const { return -1; }
  /* The following 7 functions return geometries in wkb format. */
  virtual int start_point(String *point) const { return -1; }
  virtual int end_point(String *point) const { return -1; }
  virtual int exterior_ring(String *ring) const { return -1; }
  virtual int centroid(String *point) const { return -1; }
  virtual int point_n(uint32 num, String *result) const { return -1; }
  virtual int interior_ring_n(uint32 num, String *result) const { return -1; }
  virtual int geometry_n(uint32 num, String *result) const { return -1; }

  virtual int store_shapes(Gcalc_shape_transporter *trn,
                           Gcalc_shape_status *st) const { return -1;}
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
    geom->set_data_ptr(wkb->data(), wkb->length());
    return geom;
  }

  static Geometry *construct(Geometry_buffer *buffer,
                             const char *data, uint32 data_len,
                             bool has_srid= true);
  static Geometry *construct(Geometry_buffer *buffer, const String *str,
                             bool has_srid= true)
  {
    return construct(buffer, str->ptr(),
                     static_cast<uint32>(str->length()), has_srid);
  }
  static Geometry *create_from_wkt(Geometry_buffer *buffer,
				   Gis_read_stream *trs, String *wkt,
				   bool init_stream=1);
  static Geometry *create_from_wkb(Geometry_buffer *buffer, const char *wkb,
                                   uint32 len, String *res, bool init);
  static int create_from_opresult(Geometry_buffer *g_buf,
                                  String *res, Gcalc_result_receiver &rr);
  bool as_wkt(String *wkt, wkb_parser *wkb) const
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
  bool as_wkt(String *wkt) const
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return as_wkt(wkt, &wkb);
  }

  bool as_wkb(String *wkb, bool shallow_copy) const;
  bool as_geometry(String *wkb, bool shallow_copy) const;

  void set_data_ptr(const void *data, size_t data_len)
  {
    m_ptr= const_cast<void *>(data);
    set_nbytes(data_len);
  }

  void set_data_ptr(const wkb_container *c)
  {
    m_ptr= const_cast<void *>(static_cast<const void *>(c->data()));
    set_nbytes(c->length());
  }
  void *get_data_ptr() const
  {
    return m_ptr;
  }

  bool envelope(String *result) const;
  bool envelope(MBR *mbr) const;

  static Class_info *ci_collection[wkb_last+1];

  bool is_polygon_ring() const
  {
    return m_flags.props & (POLYGON_OUTER_RING | POLYGON_INNER_RING);
  }

  bool is_polygon_outer_ring() const
  {
    return m_flags.props & POLYGON_OUTER_RING;
  }

  bool is_polygon_inner_ring() const
  {
    return m_flags.props & POLYGON_INNER_RING;
  }

  bool has_geom_header_space() const
  {
    return (m_flags.props & HAS_GEOM_HEADER_SPACE) ||
      (m_flags.props & IS_BOOST_GEOMETRY_ADAPTER);
  }

  void has_geom_header_space(bool b)
  {
    if (b)
      m_flags.props|= HAS_GEOM_HEADER_SPACE;
    else
      m_flags.props&= ~HAS_GEOM_HEADER_SPACE;
  }

  bool is_components_no_overlapped() const
  {
    return (m_flags.props & MULTIPOLYGON_NO_OVERLAPPED_COMPS);
  }

  void set_components_no_overlapped(bool b)
  {
    DBUG_ASSERT(get_type() == wkb_multilinestring ||
                get_type() == wkb_multipolygon ||
                get_type() == wkb_geometrycollection);
    if (b)
      m_flags.props|= MULTIPOLYGON_NO_OVERLAPPED_COMPS;
    else
      m_flags.props&= ~MULTIPOLYGON_NO_OVERLAPPED_COMPS;
  }

  void set_props(uint16 flag)
  {
    DBUG_ASSERT(0xfff >= flag);
    m_flags.props |= flag;
  }

  uint16 get_props() const { return (uint16)m_flags.props; }

  void set_srid(srid_t id)
  {
    m_srid= id;
  }

  srid_t get_srid() const { return m_srid; }

  const void *normalize_ring_order();
protected:
  static Class_info *find_class(int type_id)
  {
    return ((type_id < wkb_first) || (type_id > wkb_last)) ?
      NULL : ci_collection[type_id];
  }
  static Class_info *find_class(const char *name, size_t len);
  void append_points(String *txt, uint32 n_points,
                     wkb_parser *wkb, uint32 offset) const;
  bool create_point(String *result, wkb_parser *wkb) const;
  bool create_point(String *result, point_xy p) const;
  bool get_mbr_for_points(MBR *mbr, wkb_parser *wkb, uint offset) const;
  bool is_length_verified() const {return m_flags.props & GEOM_LENGTH_VERIFIED;}

  // Have to make this const because it's called in a const member function.
  void set_length_verified(bool b) const
  {
    if (b)
      m_flags.props |= GEOM_LENGTH_VERIFIED;
    else
      m_flags.props &= ~GEOM_LENGTH_VERIFIED;
  }

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


  /***************************** Boost Geometry Adapter Interface ************/
public:
  /**
    Highest byte is stores byte order, dimension, nomem and geotype as follows:
    bo: byte order, 1 for little endian(ndr), 0 for big endian(xdr); Currently
        it must be always wkb_ndr since it is MySQL's portable geometry format.
    dimension: 0~3 for 1~4 dimensions;
    nomem: indicating whether this object has its own memory.
           If so, the memory is released when the object is destroyed. Some
           objects may refer to an existing WKB buffer and access it read only.
    geotype: stores the wkbType enum numbers, at most 32 values, valid range
             so far: [0, 7] and 31.

    nybytes: takes the following 30 bits, stores number of effective and valid
             data bytes of current object's wkb data.

    props: bits OR'ed for various other runtime properties of the geometry
           object. Bits are defined above. No properties are stored
           permanently, all properties here are specified/used at runtime
           while the Geometry object is alive.
    zm: not used now, always be 0, i.e. always 2D geometries. In future,
        they represent Z and/or M settings, 1: Z, 2: M, 3: ZM.
    unused: reserved for future use, it's unused now.
  */
  class Flags_t
  {
  public:
    Flags_t(const Flags_t &o)
    {
      compile_time_assert(sizeof(*this) == sizeof(uint64));
      memcpy(this, &o, sizeof(o));
    }

    Flags_t()
    {
      compile_time_assert(sizeof(*this) == sizeof(uint64));
      memset(this, 0, sizeof(*this));
      bo= wkb_ndr;
      dim= GEOM_DIM - 1;
      nomem= 1;
    }

    Flags_t(wkbType type, size_t len)
    {
      compile_time_assert(sizeof(*this) == sizeof(uint64));
      memset(this, 0, sizeof(*this));
      geotype= type;
      nbytes= len;
      bo= wkb_ndr;
      dim= GEOM_DIM - 1;
      nomem= 1;
    }

    Flags_t &operator=(const Flags_t &rhs)
    {
      compile_time_assert(sizeof(*this) == sizeof(uint64));
      memcpy(this, &rhs, sizeof(rhs));
      return *this;
    }


    uint64 bo:1;
    uint64 dim:2;
    uint64 nomem:1;
    uint64 geotype:5;
    uint64 nbytes:30;
    uint64 props:12;
    uint64 zm:2;
    uint64 unused:11;
  };

  Geometry()
  {
    m_ptr= NULL;
    m_owner= NULL;
    set_ownmem(false);
    set_byte_order(Geometry::wkb_ndr);
    set_srid(default_srid);
  }

  /**
    Constructor used as BG adapter or by default constructors of children
    classes.
    @param ptr WKB buffer address, or NULL for an empty object.
    @param len WKB buffer length in bytes.
    @param flags the flags to set, no field is used for now except geotype.
    @param srid srid of the geometry.
  */
  Geometry(const void *ptr, size_t len, const Flags_t &flags, srid_t srid)
  {
    m_ptr= const_cast<void *>(ptr);
    m_flags.nbytes= len;
    set_srid(srid);
    m_flags.geotype= flags.geotype;
    m_owner= NULL;
    set_ownmem(false);
  }

  Geometry(const Geometry &geo);

  Geometry &operator=(const Geometry &rhs);

  /* Getters and setters. */
  void *get_ptr() const
  {
    return m_ptr;
  }

  char *get_cptr() const
  {
    return static_cast<char *>(m_ptr);
  }

  uchar *get_ucptr() const
  {
    return static_cast<uchar *>(m_ptr);
  }

  Geometry *get_owner() const { return m_owner;}

  void set_owner(Geometry *o) { m_owner= o; }

  void set_byte_order(Geometry::wkbByteOrder bo)
  {
    DBUG_ASSERT(bo == Geometry::wkb_ndr);
    m_flags.bo= static_cast<char>(bo);
  }

  void set_dimension(char dim)
  {
    // Valid dim is one of [1, 2, 3, 4].
    DBUG_ASSERT(dim >0 && dim <5);
    m_flags.dim= dim - 1;
  }

  static bool is_valid_geotype(uint32 gtype)
  {
    wkbType gt= static_cast<wkbType>(gtype);

    /*
      Stricter check, outside only checks for [wkb_first, wkb_last],
      they don't have to know about the details.
     */
    return ((gt >= wkb_first && gt <= wkb_geometrycollection) ||
            gt == wkb_polygon_inner_rings);
  }

  static bool is_valid_geotype(Geometry::wkbType gt)
  {
    /*
      Stricter check, outside only checks for [wkb_first, wkb_last],
      they don't have to know about the details.
     */
    return ((gt >= wkb_first && gt <= wkb_geometrycollection) ||
            gt == wkb_polygon_inner_rings);
  }

  /**
    Verify that a string is a well-formed GEOMETRY string.
   
    This does not check if the geometry is geometrically valid.

    @see Geometry_well_formed_checker
   
    @param from String to check
    @param length Length of string
    @param type Expected type of geometry, or
           Geoemtry::wkb_invalid_type if any type is allowed

    @return True if the string is a well-formed GEOMETRY string,
            false otherwise
   */
  static bool is_well_formed(const char *from, size_t length,
                             wkbType type, wkbByteOrder bo);

  void set_geotype(Geometry::wkbType gt)
  {
    is_valid_geotype(gt);
    m_flags.geotype= static_cast<char>(gt);
  }

  // Have to make this const because it's called in a const member function.
  void set_nbytes(size_t n) const
  {
    if (get_nbytes() != n)
    {
      set_length_verified(false);
      m_flags.nbytes= n;
    }
  }

  /**
    Set whether this object has its own memory. If so, the memory is released
    when this object is destroyed.
    @param b true if this object has its own memory, false otherwise.

   */
  void set_ownmem(bool b)
  {
    m_flags.nomem= (b ? 0 : 1);
  }

  /**
    Returns whether this object has its own memory. If so, the memory is
    released when this object is destroyed.
    */
  bool get_ownmem() const
  {
    return !m_flags.nomem;
  }

  Geometry::wkbByteOrder get_byte_order() const
  {
    DBUG_ASSERT(m_flags.bo == 1);
    return Geometry::wkb_ndr;
  }

  char get_dimension() const
  {
    return static_cast<char>(m_flags.dim) + 1;
  }

  Geometry::wkbType get_geotype() const
  {
    char gt= static_cast<char>(m_flags.geotype);
    return static_cast<Geometry::wkbType>(gt);
  }

  /**
    Build an OGC standard type value from m_flags.zm and m_flags.geotype. For
    now m_flags.zm is always 0 so simply call get_geotype(). We don't
    directly store the OGC defined values in order to save more bits
    of m_flags for other purposes; and also separating zm settings from basic
    geometry types is easier for coding and geometry type identification.

    When we start to support Z/M settings we need to modify all code which call
    write_wkb_header and write_geometry_header to pass to them an OGC standard
    type value returned by this function or built similarly. And by doing so
    our internal runtime geometry type encoding will work consistently with
    OGC defined standard geometry type values in byte strings of WKB format.

    @return OGC standard geometry type value.
   */
  uint32 get_ogc_geotype() const
  {
    return static_cast<uint32>(get_geotype());
  }


  size_t get_nbytes() const
  {
    return static_cast<size_t>(m_flags.nbytes);
  }

  /*
    Only sets m_ptr, different from the overloaded one in Gis_wkb_vector<>
    which also does WKB parsing.
   */
  void set_ptr(const void *ptr)
  {
    m_ptr= const_cast<void *>(ptr);
  }

  /**
    Whether the Geometry object is created to be used by Boost Geometry or
    only by MySQL. There are some operations that only work for one type and
    can or must be skipped otherwise.
    @return true if it's a BG adapter, false otherwise.
   */
  bool is_bg_adapter() const
  {
    return m_flags.props & IS_BOOST_GEOMETRY_ADAPTER;
  }

  /**
    Set whether this object is a BG adapter.
    @param b true if it's a BG adapter, false otherwise.
    Have to declare this as const because even when a Geometry object's const
    adapter member function is called, it's regarded as a BG adapter object.
   */
  void set_bg_adapter(bool b) const
  {
    if (b)
      m_flags.props |= IS_BOOST_GEOMETRY_ADAPTER;
    else
      m_flags.props &= ~IS_BOOST_GEOMETRY_ADAPTER;
  }

  /*
    Give up ownership of m_ptr, so as not to release them when
    this object is destroyed, to be called when the two member is shallow
    assigned to another geometry object.
   */
  virtual void donate_data()
  {
    set_ownmem(false);
    set_nbytes(0);
    m_ptr= NULL;
  }
protected:
  /**
    In a polygon usable by boost geometry, the m_ptr points to the outer ring
    object, and m_inn_rings points to the inner rings, thus the polygon's data
    isn't stored in a single WKB. Users should call
    Gis_polygon::to_wkb_unparsed() before getting the polygon's wkb data,
    Gis_polygon::to_wkb_unparsed() will form a single WKB for the polygon
    and refer to it with m_ptr, and release the outer ring object
    and the inner rings objects, and such an polygon isn't usable by BG any
    more, it's exactly what we got with
    Geometry::create_from_wkt/Geometry::create_from_wkt.
   */
  bool polygon_is_wkb_form() const
  {
    return m_flags.props & POLYGON_IN_WKB_FORM;
  }

  void polygon_is_wkb_form(bool b)
  {
    if (b)
      m_flags.props|= POLYGON_IN_WKB_FORM;
    else
      m_flags.props&= ~POLYGON_IN_WKB_FORM;
  }

  /**
    If call Gis_wkb_vector<T>::resize() to add a component to X, the
    geometry may have a geometry not stored inside the WKB buffer of X, hence
    X has out of line component. For such an X, user should call
    Gis_wkb_vector<T>::reassemble() before fetching its WKB data.
   */
  bool has_out_of_line_components() const
  {
    return m_flags.props & HAS_OUT_OF_LINE_COMPONENTS;
  }

  void has_out_of_line_components(bool b)
  {
    if (b)
      m_flags.props|= HAS_OUT_OF_LINE_COMPONENTS;
    else
      m_flags.props&= ~HAS_OUT_OF_LINE_COMPONENTS;
  }

  void clear_wkb_data();
  virtual void shallow_push(const Geometry *)
  {
    DBUG_ASSERT(false);
  }

protected:

  /**
    The topmost (root) geometry object, whose m_ptr is the 1st byte of a
    wkb memory buffer. other geometry objects hold m_ptr which points
    inside somewhere in the memory buffer. when updating a geometry object,
    need to ask m_owner to reallocate memory if needed for new data.
   */
  Geometry *m_owner;


  /**
    Pointer to the geometry's wkb data's 1st byte, right after its
    wkb header if any.
    If the geometry is wkb_polygon, this field is a
    Gis_polygon_ring* pointer, pointing to the outer ring. Outer ring's wkb data
    is in the same wkb buffer as the inner rings, so we can get the wkb header
    from the outer ring like ((Geometry*)m_ptr)->get_ptr().
   */
  void *m_ptr;
private:

  /// Flags and meta information about this object.
  /// Make it mutable to modify some flags in const member functions.
  mutable Flags_t m_flags;

  /// Srid of this object.
  srid_t m_srid;
public:

  Flags_t get_flags() const {return m_flags;}

  void set_flags(const Flags_t &flags)
  {
    m_flags= flags;
  }
};


inline Geometry::wkbByteOrder get_byte_order(const void *p0)
{
  const char *p= static_cast<const char *>(p0);

  if (!(*p == 0 || *p == 1))
    return Geometry::wkb_invalid;
  return *p == 0 ? Geometry::wkb_xdr : Geometry::wkb_ndr;
}

inline void set_byte_order(void *p0, Geometry::wkbByteOrder bo)
{
  char *p= static_cast<char *>(p0);
  *p= (bo == Geometry::wkb_ndr ? 1 : 0);
}

/**
  Get wkbType value from WKB, the WKB is always little endian, so need
  platform specific conversion.
  @param p0 WKB geometry type field address.
  @return geometry type.
 */
inline Geometry::wkbType get_wkb_geotype(const void *p0)
{
  const char *p= static_cast<const char *>(p0);
  uint32 gt= uint4korr(p);
  DBUG_ASSERT(Geometry::is_valid_geotype(gt));
  return static_cast<Geometry::wkbType>(gt);
}

/*
  Functions to write a GEOMETRY or WKB header into a piece of allocated and
  big enough raw memory or into a String object with enough reserved memory,
  and optionally append the object count right after the header.
 */
inline char *write_wkb_header(void *p0, Geometry::wkbType geotype)
{
  char *p= static_cast<char *>(p0);
  *p= static_cast<char>(Geometry::wkb_ndr);
  p++;
  int4store(p, static_cast<uint32>(geotype));
  return p + 4;
}


inline char *write_wkb_header(void *p0, Geometry::wkbType geotype,
                              uint32 obj_count)
{
  char *p= static_cast<char *>(p0);
  p= write_wkb_header(p, geotype);
  int4store(p, obj_count);
  return p + 4;
}


inline char *write_geometry_header(void *p0, uint32 srid,
                                   Geometry::wkbType geotype)
{
  char *p= static_cast<char *>(p0);
  int4store(p, srid);
  return write_wkb_header(p + 4, geotype);
}


inline char *write_geometry_header(void *p0, uint32 srid,
                                   Geometry::wkbType geotype, uint32 obj_count)
{
  char *p= static_cast<char *>(p0);
  int4store(p, srid);
  return write_wkb_header(p + 4, geotype, obj_count);
}


inline void write_wkb_header(String *str, Geometry::wkbType geotype)
{
  str->q_append(static_cast<char>(Geometry::wkb_ndr));
  str->q_append(static_cast<uint32>(geotype));
}


inline void write_wkb_header(String *str, Geometry::wkbType geotype,
                             uint32 obj_count)
{
  write_wkb_header(str, geotype);
  str->q_append(obj_count);
}


inline void write_geometry_header(String *str, uint32 srid,
                                  Geometry::wkbType geotype)
{
  str->q_append(srid);
  write_wkb_header(str, geotype);
}


inline void write_geometry_header(String *str, uint32 srid,
                                  Geometry::wkbType geotype, uint32 obj_count)
{
  write_geometry_header(str, srid, geotype);
  str->q_append(obj_count);
}


/***************************** Point *******************************/

class Gis_point: public Geometry
{
public:
  uint32 get_data_size() const;
  bool init_from_wkt(Gis_read_stream *trs, String *wkb);
  uint init_from_wkb(const char *wkb, uint len, wkbByteOrder bo, String *res);
  bool get_data_as_wkt(String *txt, wkb_parser *wkb) const;
  bool get_mbr(MBR *mbr, wkb_parser *wkb) const;

  int get_xy(point_xy *p) const
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return wkb.scan_xy(p);
  }
  int get_x(double *x) const
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return wkb.scan_coord(x);
  }
  int get_y(double *y) const
  {
    wkb_parser wkb(get_cptr(), get_cptr() + get_nbytes());
    return wkb.skip_coord() || wkb.scan_coord(y);
  }
  uint32 feature_dimension() const { return 0; }
  int store_shapes(Gcalc_shape_transporter *trn, Gcalc_shape_status *st) const;
  const Class_info *get_class_info() const;


  /************* Boost Geometry Adapter Interface *************/


  typedef Gis_point self;
  typedef Geometry base;

  explicit Gis_point(bool is_bg_adapter= true)
    :Geometry(NULL, 0, Flags_t(wkb_point, 0), default_srid)
  {
    set_ownmem(false);
    set_bg_adapter(is_bg_adapter);
  }

  /// @brief Default constructor, no initialization.
  Gis_point(const void *ptr, size_t nbytes, const Flags_t &flags, srid_t srid)
    :Geometry(ptr, nbytes, flags, srid)
  {
    set_geotype(wkb_point);
    DBUG_ASSERT((ptr != NULL &&
                 get_nbytes() == SIZEOF_STORED_DOUBLE * GEOM_DIM) ||
                (ptr == NULL && get_nbytes() == 0));
    set_ownmem(false);
    set_bg_adapter(true);
  }

  Gis_point(const self &pt);

  virtual ~Gis_point()
  {}


  Gis_point &operator=(const Gis_point &rhs);

  void set_ptr(void *ptr, size_t len);


  /// @brief Get a coordinate
  /// @tparam K coordinate to get
  /// @return the coordinate
  template <std::size_t K>
  double get() const
  {
    DBUG_ASSERT(K < static_cast<size_t>(get_dimension()) &&
                ((m_ptr != NULL &&
                  get_nbytes() == SIZEOF_STORED_DOUBLE * GEOM_DIM) ||
                 (m_ptr == NULL && get_nbytes() == 0)));

    set_bg_adapter(true);
    const char *p= static_cast<char *>(m_ptr) + K * SIZEOF_STORED_DOUBLE;
    double val;

    /*
      Boost Geometry may use a point that is only default constructed that
      has not specified with any meaningful value, and in such a case the
      default value are expected to be all zeros.
     */
    if (m_ptr == NULL)
      return 0;

    float8get(&val, p);
    return val;
  }


  /// @brief Set a coordinate
  /// @tparam K coordinate to set
  /// @param value value to set
  // Deep assignment, not only allow assigning to a point owning its memory,
  // but also a point not own memory, since points are of same size.
  template <std::size_t K>
  void set(double const &value)
  {
    /* Allow assigning to others' memory. */
    DBUG_ASSERT((m_ptr != NULL && K < static_cast<size_t>(get_dimension()) &&
                 get_nbytes() == SIZEOF_STORED_DOUBLE * GEOM_DIM) ||
                (!get_ownmem() && get_nbytes() == 0 && m_ptr == NULL));
    set_bg_adapter(true);
    if (m_ptr == NULL)
    {
      m_ptr= gis_wkb_fixed_alloc(SIZEOF_STORED_DOUBLE * GEOM_DIM);
      if (m_ptr == NULL)
      {
        set_ownmem(false);
        set_nbytes(0);
        return;
      }
      set_ownmem(true);
      set_nbytes(SIZEOF_STORED_DOUBLE * GEOM_DIM);
    }

    char *p= get_cptr() + K * SIZEOF_STORED_DOUBLE;
    float8store(p, value);
  }


  bool operator<(const Gis_point &pt) const
  {
    bool x= get<0>(), px= pt.get<0>();
    return x == px ? get<1>() < pt.get<1>() : x < px;
  }

  bool operator==(const Gis_point &pt) const
  {
    return (get<0>() == pt.get<0>() && get<1>() == pt.get<1>());
  }
};


/******************************** Gis_wkb_vector **************************/


template <typename T>
class Gis_wkb_vector;

/// @ingroup iterators
/// @{
/// @defgroup Gis_wkb_vector_iterators Iterator classes for Gis_wkb_vector.
/// Gis_wkb_vector has two iterator classes --- Gis_wkb_vector_const_iterator
/// and Gis_wkb_vector_iterator. The differences
/// between the two classes are that the Gis_wkb_vector_const_iterator
/// can only be used to read its referenced value, so it is intended as
/// Gis_wkb_vector's const iterator; While the other class allows both read and
/// write access. If your access pattern is readonly, it is strongly
/// recommended that you use the const iterator because it is faster
/// and more efficient.
/// The two classes have identical behaviors to std::vector::const_iterator and
/// std::vector::iterator respectively.
//@{
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//
// Gis_wkb_vector_const_iterator class template definition
//
/// Gis_wkb_vector_const_iterator is const_iterator class for Gis_wkb_vector,
/// and base class of Gis_wkb_vector_iterator -- iterator class for
/// Gis_wkb_vector.
/// @tparam T Vector element type
template <typename T>
class Gis_wkb_vector_const_iterator
{
protected:
  typedef Gis_wkb_vector_const_iterator<T> self;
  typedef Gis_wkb_vector<T> owner_t;
  typedef ptrdiff_t index_type;
public:
  ////////////////////////////////////////////////////////////////////
  //
  // Begin public type definitions.
  //
  typedef T value_type;
  typedef ptrdiff_t difference_type;
  typedef difference_type distance_type;
  typedef typename owner_t::size_type size_type;

  /// This is the return type for operator[].
  typedef value_type& reference;
  typedef value_type* pointer;
  // Use the STL tag, to ensure compatability with interal STL functions.
  //
  typedef std::random_access_iterator_tag iterator_category;
  ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
  // Begin public constructors and destructor.
  /// @name Constructors and destroctor
  /// Do not construct iterators explictily using these constructors,
  /// but call Gis_wkb_vector::begin() const to get an valid iterator.
  /// @sa Gis_wkb_vector::begin() const
  //@{
  Gis_wkb_vector_const_iterator(const self &vi)
  {
    m_curidx= vi.m_curidx;
    m_owner= vi.m_owner;
  }


  Gis_wkb_vector_const_iterator()
  {
    m_curidx= -1;
    m_owner= NULL;
  }


  Gis_wkb_vector_const_iterator(index_type idx, const owner_t *owner)
  {
    m_curidx= idx;
    m_owner= const_cast<owner_t *>(owner);
  }


  ~Gis_wkb_vector_const_iterator()
  {
  }
  //@}

  ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
  //
  // Begin functions that compare iterator positions.
  //
  /// @name Iterator comparison operators
  /// The way to compare two iterators is to compare the index values
  /// of the two elements they point to. The iterator sitting on an
  /// element with less index is regarded to be smaller. And the invalid
  /// iterator sitting after last element is greater than any other
  /// iterators, because it is assumed to have an index equal to last
  /// element's index plus one; The invalid iterator sitting before first
  /// element is less than any other iterators because it is assumed to
  /// have an index -1.
  //@{
  /// @brief Equality comparison operator.
  ///
  /// Invalid iterators are equal; Valid iterators
  /// sitting on the same key/data pair equal; Otherwise not equal.
  /// @param itr The iterator to compare against.
  /// @return True if this iterator equals to itr; False otherwise.
  bool operator==(const self &itr) const
  {
    DBUG_ASSERT(m_owner == itr.m_owner);
    return m_curidx == itr.m_curidx;
  }


  /// @brief Unequal compare, identical to !operator(==itr)
  /// @param itr The iterator to compare against.
  /// @return False if this iterator equals to itr; True otherwise.
  bool operator!=(const self &itr) const
  {
    return !(*this == itr) ;
  }


  // The end() iterator is largest. If both are end() iterator return false.
  /// @brief Less than comparison operator.
  /// @param itr The iterator to compare against.
  /// @return True if this iterator is less than itr.
  bool operator < (const self &itr) const
  {
    DBUG_ASSERT(m_owner == itr.m_owner);
    return m_curidx < itr.m_curidx;
  }


  /// @brief Less equal comparison operator.
  /// @param itr The iterator to compare against.
  /// @return True if this iterator is less than or equal to itr.
  bool operator <= (const self &itr) const
  {
    return !(this->operator>(itr));
  }


  /// @brief Greater equal comparison operator.
  /// @param itr The iterator to compare against.
  /// @return True if this iterator is greater than or equal to itr.
  bool operator >= (const self &itr) const
  {
    return !(this->operator<(itr));
  }


  // The end() iterator is largest. If both are end() iterator return false.
  /// @brief Greater comparison operator.
  /// @param itr The iterator to compare against.
  /// @return True if this iterator is greater than itr.
  bool operator > (const self &itr) const
  {
    DBUG_ASSERT(m_owner == itr.m_owner);
    return m_curidx > itr.m_curidx;
  }
  //@} // vctitr_cmp
  ////////////////////////////////////////////////////////////////////


  ////////////////////////////////////////////////////////////////////
  //
  // Begin functions that shift the iterator position.
  //
  /// @name Iterator movement operators.
  /// When we talk about iterator movement, we think the
  /// container is a uni-directional range, represented by [begin, end),
  /// and this is true no matter we are using iterators or reverse
  /// iterators. When an iterator is moved closer to "begin", we say it
  /// is moved backward, otherwise we say it is moved forward.
  //@{
  /// @brief Pre-increment.
  ///
  /// Move the iterator one element forward, so that
  /// the element it sits on has a bigger index.
  /// Use ++iter rather than iter++ where possible to avoid two useless
  /// iterator copy constructions.
  /// @return This iterator after incremented.
  self &operator++()
  {
    move_by(*this, 1, false);
    return *this;
  }


  /// @brief Post-increment.
  /// Move the iterator one element forward, so that
  /// the element it sits on has a bigger index.
  /// Use ++iter rather than iter++ where possible to avoid two useless
  /// iterator copy constructions.
  /// @return A new iterator not incremented.
  self operator++(int)
  {
    self itr(*this);
    move_by(*this, 1, false);

    return itr;
  }


  /// @brief Pre-decrement.
  /// Move the iterator one element backward, so
  /// that the element it  sits on has a smaller index.
  /// Use --iter rather than iter-- where possible to avoid two useless
  /// iterator copy constructions.
  /// @return This iterator after decremented.
  self &operator--()
  {
    move_by(*this, 1, true);
    return *this;
  }


  /// @brief Post-decrement.
  ///
  /// Move the iterator one element backward, so
  /// that the element it  sits on has a smaller index.
  /// Use --iter rather than iter-- where possible to avoid two useless
  /// iterator copy constructions.
  /// @return A new iterator not decremented.
  self operator--(int)
  {
    self itr= *this;
    move_by(*this, 1, true);
    return itr;
  }


  /// @brief Assignment operator.
  ///
  /// This iterator will point to the same key/data
  /// pair as itr, and have the same configurations as itr.
  /// @param itr The right value of the assignment.
  /// @return This iterator's reference.
  const self &operator=(const self &itr)
  {
    m_curidx= itr.m_curidx;
    m_owner= itr.m_owner;
    return itr;
  }


  /// Iterator movement operator.
  /// Return another iterator by moving this iterator forward by n
  /// elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move backward by |n| element.
  /// @return The new iterator at new position.
  self operator+(difference_type n) const
  {
    self itr(*this);
    move_by(itr, n, false);
    return itr;
  }


  /// @brief Move this iterator forward by n elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move backward by |n| element.
  /// @return Reference to this iterator at new position.
  const self &operator+=(difference_type n)
  {
    move_by(*this, n, false);
    return *this;
  }


  /// @brief Iterator movement operator.
  ///
  /// Return another iterator by moving this iterator backward by n
  /// elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move forward by |n| element.
  /// @return The new iterator at new position.
  self operator-(difference_type n) const
  {
    self itr(*this);
    move_by(itr, n, true);

    return itr;
  }


  /// @brief Move this iterator backward by n elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move forward by |n| element.
  /// @return Reference to this iterator at new position.
  const self &operator-=(difference_type n)
  {
    move_by(*this, n, true);
    return *this;
  }
  //@} //itr_movement


  /// @brief Iterator distance operator.
  ///
  /// Return the index difference of this iterator and itr, so if this
  /// iterator sits on an element with a smaller index, this call will
  /// return a negative number.
  /// @param itr The other iterator to substract. itr can be the invalid
  /// iterator after last element or before first element, their index
  /// will be regarded as last element's index + 1 and -1 respectively.
  /// @return The index difference.
  difference_type operator-(const self &itr) const
  {
    DBUG_ASSERT(m_owner == itr.m_owner);
    return (m_curidx - itr.m_curidx);
  }


  ////////////////////////////////////////////////////////////////////
  //
  // Begin functions that retrieve values from the iterator.
  //
  /// @name Functions that retrieve values from the iterator.
  //@{
  /// @brief Dereference operator.
  ///
  /// Return the reference to the cached data element.
  /// The returned value can only be used to read its referenced
  /// element.
  /// @return The reference to the element this iterator points to.
  reference operator*() const
  {
    DBUG_ASSERT(this->m_owner != NULL && this->m_curidx >= 0 &&
                this->m_curidx <
                static_cast<index_type>(this->m_owner->size()));
    return (*m_owner)[m_curidx];
  }


  /// @brief Arrow operator.
  ///
  /// Return the pointer to the cached data element.
  /// The returned value can only be used to read its referenced
  /// element.
  /// @return The address of the referenced object.
  pointer operator->() const
  {
    DBUG_ASSERT(this->m_owner != NULL && this->m_curidx >= 0 &&
                this->m_curidx <
                static_cast<index_type>(this->m_owner->size()));
    return &(*m_owner)[m_curidx];
  }


  /// @brief Iterator index operator.
  ///
  /// @param offset The offset of target element relative to this iterator.
  /// @return Return the reference of the element which is at
  /// position *this + offset.
  /// The returned value can only be used to read its referenced
  /// element.
  reference operator[](difference_type offset) const
  {
    self itr= *this;
    move_by(itr, offset, false);

    DBUG_ASSERT(itr.m_owner != NULL && itr.m_curidx >= 0 &&
                itr.m_curidx < static_cast<index_type>(itr.m_owner->size()));
    return (*m_owner)[itr.m_curidx];
  }
  //@}
  ////////////////////////////////////////////////////////////////////


protected:
  // The 'back' parameter indicates whether to decrease or
  // increase the index when moving. The default is to decrease.
  //
  void move_by(self &itr, difference_type n, bool back) const
  {
    if (back)
      n= -n;

    index_type newidx= itr.m_curidx + n;
    size_t sz= 0;

    if (newidx < 0)
      newidx= -1;
    else if (newidx >= static_cast<index_type>((sz= m_owner->size())))
      newidx= sz;

    itr.m_curidx= newidx;
  }


protected:
  /// Current element's index, starting from 0.
  index_type m_curidx;
  /// The owner container of this iteraotr.
  owner_t *m_owner;
}; // Gis_wkb_vector_const_iterator<>


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//
// Gis_wkb_vector_iterator class template definition
/// This class is the iterator class for Gis_wkb_vector, its instances can
/// be used to mutate their referenced data element.
/// @tparam T Vector element type
//
template <class T>
class Gis_wkb_vector_iterator :public Gis_wkb_vector_const_iterator<T>
{
protected:
  typedef Gis_wkb_vector_iterator<T> self;
  typedef Gis_wkb_vector_const_iterator<T> base;
  typedef Gis_wkb_vector<T> owner_t;
public:
  typedef ptrdiff_t index_type;
  typedef T value_type;
  typedef ptrdiff_t difference_type;
  typedef difference_type distance_type;
  typedef value_type& reference;
  typedef value_type* pointer;
  // Use the STL tag, to ensure compatability with interal STL functions.
  typedef std::random_access_iterator_tag iterator_category;


  ////////////////////////////////////////////////////////////////////
  /// Begin public constructors and destructor.
  //
  /// @name Constructors and destructor
  /// Do not construct iterators explictily using these constructors,
  /// but call Gis_wkb_vector::begin to get an valid iterator.
  /// @sa Gis_wkb_vector::begin
  //@{
  Gis_wkb_vector_iterator(const self &vi) :base(vi)
  {
  }


  Gis_wkb_vector_iterator() :base()
  {
  }


  Gis_wkb_vector_iterator(const base &obj) :base(obj)
  {
  }


  Gis_wkb_vector_iterator(index_type idx, const owner_t *owner)
    :base(idx, owner)
  {}


  ~Gis_wkb_vector_iterator()
  {
  }
  //@}


  ////////////////////////////////////////////////////////////////////


  ////////////////////////////////////////////////////////////////////
  //
  /// Begin functions that shift the iterator position.
  //
  /// These functions are identical to those defined in
  /// Gis_wkb_vector_const_iterator, but we have to redefine them here because
  /// the "self" have different definitions.
  //
  /// @name Iterator movement operators.
  /// These functions have identical behaviors and semantics as those of
  /// Gis_wkb_vector_const_iterator, so please refer to equivalent in that
  /// class.
  //@{
  /// @brief Pre-increment.
  /// @return This iterator after incremented.
  /// @sa Gis_wkb_vector_const_iterator::operator++()
  self &operator++()
  {
    this->move_by(*this, 1, false);
    return *this;
  }


  /// @brief Post-increment.
  /// @return A new iterator not incremented.
  /// @sa Gis_wkb_vector_const_iterator::operator++(int)
  self operator++(int)
  {
    self itr(*this);
    this->move_by(*this, 1, false);

    return itr;
  }


  /// @brief Pre-decrement.
  /// @return This iterator after decremented.
  /// @sa Gis_wkb_vector_const_iterator::operator--()
  self &operator--()
  {
    this->move_by(*this, 1, true);
    return *this;
  }


  /// @brief Post-decrement.
  /// @return A new iterator not decremented.
  /// @sa Gis_wkb_vector_const_iterator::operator--(int)
  self operator--(int)
  {
    self itr= *this;
    this->move_by(*this, 1, true);
    return itr;
  }


  /// @brief Assignment operator.
  ///
  /// This iterator will point to the same key/data
  /// pair as itr, and have the same configurations as itr.
  /// @param itr The right value of the assignment.
  /// @return This iterator's reference.
  const self &operator=(const self &itr)
  {
    base::operator=(itr);

    return itr;
  }


  /// @brief Iterator movement operator.
  ///
  /// Return another iterator by moving this iterator backward by n
  /// elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move forward by |n| element.
  /// @return The new iterator at new position.
  /// @sa Gis_wkb_vector_const_iterator::operator+(difference_type n) const
  self operator+(difference_type n) const
  {
    self itr(*this);
    this->move_by(itr, n, false);
    return itr;
  }


  /// @brief Move this iterator backward by n elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move forward by |n| element.
  /// @return Reference to this iterator at new position.
  /// @sa Gis_wkb_vector_const_iterator::operator+=(difference_type n)
  const self &operator+=(difference_type n)
  {
    this->move_by(*this, n, false);
    return *this;
  }


  /// @brief Iterator movement operator.
  ///
  /// Return another iterator by moving this iterator forward by n
  /// elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move backward by |n| element.
  /// @return The new iterator at new position.
  /// @sa Gis_wkb_vector_const_iterator::operator-(difference_type n) const
  self operator-(difference_type n) const
  {
    self itr(*this);
    this->move_by(itr, n, true);

    return itr;
  }


  /// @brief Move this iterator forward by n elements.
  /// @param n The amount and direction of movement. If negative, will
  /// move backward by |n| element.
  /// @return Reference to this iterator at new position.
  /// @sa Gis_wkb_vector_const_iterator::operator-=(difference_type n)
  const self &operator-=(difference_type n)
  {
    this->move_by(*this, n, true);
    return *this;
  }
  //@} // itr_movement

  /// @brief Iterator distance operator.
  ///
  /// Return the index difference of this iterator and itr, so if this
  /// iterator sits on an element with a smaller index, this call will
  /// return a negative number.
  /// @param itr The other iterator to substract. itr can be the invalid
  /// iterator after last element or before first element, their index
  /// will be regarded as last element's index + 1 and -1 respectively.
  /// @return The index difference.
  /// @sa Gis_wkb_vector_const_iterator::operator-(const self &itr) const
  difference_type operator-(const self &itr) const
  {
    return base::operator-(itr);
  }
  ////////////////////////////////////////////////////////////////////


  ////////////////////////////////////////////////////////////////////
  //
  // Begin functions that retrieve values from the iterator.
  //
  /// @name Functions that retrieve values from the iterator.
  //@{
  /// @brief Dereference operator.
  ///
  /// Return the reference to the cached data element
  /// The returned value can be used to read or update its referenced
  /// element.
  /// @return The reference to the element this iterator points to.
  reference operator*() const
  {
    DBUG_ASSERT(this->m_owner != NULL && this->m_curidx >= 0 &&
                this->m_curidx <
                static_cast<index_type>(this->m_owner->size()));
    return (*this->m_owner)[this->m_curidx];
  }


  /// @brief Arrow operator.
  ///
  /// Return the pointer to the cached data element
  /// The returned value can be used to read or update its referenced
  /// element.
  /// @return The address of the referenced object.
  pointer operator->() const
  {
    DBUG_ASSERT(this->m_owner != NULL && this->m_curidx >= 0 &&
                this->m_curidx <
                static_cast<index_type>(this->m_owner->size()));
    return &(*this->m_owner)[this->m_curidx];
  }


  /// @brief Iterator index operator.
  ///
  /// @param offset The offset of target element relative to this iterator.
  /// @return Return the element which is at position *this + offset.
  /// The returned value can be used to read or update its referenced
  /// element.
  reference operator[](difference_type offset) const
  {
    self itr= *this;
    this->move_by(itr, offset, false);
    DBUG_ASSERT(itr.m_owner != NULL && itr.m_curidx >= 0 &&
                itr.m_curidx < static_cast<index_type>(this->m_owner->size()));
    return (*this->m_owner)[itr.m_curidx];
  }
  //@} // funcs_val
  ////////////////////////////////////////////////////////////////////

}; // Gis_wkb_vector_iterator
//@} // Gis_wkb_vector_iterators
//@} // iterators


// These operators make "n + itr" expressions valid. Without it, you can only
// use "itr + n"
template <typename T>
Gis_wkb_vector_const_iterator<T>
operator+(typename Gis_wkb_vector_const_iterator<T>::difference_type n,
          const Gis_wkb_vector_const_iterator<T> &itr)
{
  Gis_wkb_vector_const_iterator<T> itr2= itr;

  itr2+= n;
  return itr2;
}


template <typename T>
Gis_wkb_vector_iterator<T>
operator+(typename Gis_wkb_vector_iterator<T>::difference_type n,
          const Gis_wkb_vector_iterator<T>& itr)
{
  Gis_wkb_vector_iterator<T> itr2= itr;

  itr2+= n;
  return itr2;
}


void *get_packed_ptr(const Geometry *geo, size_t *pnbytes);
const char *get_packed_ptr(Geometry *geo);
bool polygon_is_packed(Geometry *plgn, Geometry *mplgn);
void own_rings(Geometry *geo);
void parse_wkb_data(Geometry *g, const char *p, size_t num_geoms= 0);

/**
   Geometry vector class.
   @tparam T Vector element type.
 */
template<typename T>
class Geometry_vector : public Inplace_vector<T>
{
typedef Inplace_vector<T> base;
public:
  Geometry_vector() :base(PSI_INSTRUMENT_ME)
  {}
};


/// @ingroup containers
//@{
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
/// Gis_wkb_vector class template definition
/// @tparam T Vector element type
//
template <typename T>
class Gis_wkb_vector : public Geometry
{
private:
  typedef Gis_wkb_vector<T> self;
  typedef ptrdiff_t index_type;
  typedef Geometry base;
public:
  typedef T value_type;
  typedef Gis_wkb_vector_const_iterator<T> const_iterator;
  typedef Gis_wkb_vector_iterator<T> iterator;
  typedef size_t size_type;
  typedef const T* const_pointer;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef T& reference;
  typedef ptrdiff_t difference_type;

  typedef Geometry_vector<T> Geo_vector;

private:

  /**
    The geometry vector of this geometry object's components, each of which
    is an object of Geometry or its children classes where appropriate.
   */
  Geo_vector *m_geo_vect;
public:
  /////////////////////////////////////////////////////////////////////
  // Begin functions that create iterators.
  /// @name Iterator functions.
  //@{
  iterator begin()
  {
    set_bg_adapter(true);
    iterator itr(m_geo_vect ? 0 : -1, this);
    return itr;
  }


  /// @brief Create a const iterator.
  ///
  /// The created iterator can only be used to read its referenced
  /// data element. Can only be called when using a const reference to
  /// the contaienr object.
  const_iterator begin()const
  {
    set_bg_adapter(true);
    const_iterator itr(m_geo_vect ? 0 : -1, this);
    return itr;
  }


  /// @brief Create an open boundary iterator.
  /// @return Returns an invalid iterator denoting the position after
  /// the last valid element of the container.
  iterator end()
  {
    iterator itr(m_geo_vect ? m_geo_vect->size() : -1, this);
    return itr;
  }


  /// @brief Create an open boundary iterator.
  /// @return Returns an invalid const iterator denoting the position
  /// after the last valid element of the container.
  const_iterator end() const
  {
    const_iterator itr(m_geo_vect ? m_geo_vect->size() : -1, this);
    return itr;
  }


  //@} // iterator_funcs
  /////////////////////////////////////////////////////////////////////

  /// @brief Get container size.
  /// @return Return the number of elements in this container.
  size_type size() const
  {
    set_bg_adapter(true);
    return m_geo_vect ? m_geo_vect->size() : 0;
  }


  bool empty() const
  {
    return size() == 0;
  }


  const_reference back() const
  {
    set_bg_adapter(true);
    /*
      Carefully crafted to avoid invoking any copy constructor using pointer
      cast. Also true for the two operator[] member functions below.
     */
    const Geometry *p= &(get_geo_vect()->back());
    return *((const T*)p);
  }


  reference back()
  {
    set_bg_adapter(true);
    /*
      Carefully crafted to avoid invoking any copy constructor using pointer
      cast. Also true for the two operator[] member functions below.
     */
    Geometry *p= &(get_geo_vect()->back());
    return *((T*)p);
  }


  const_reference operator[](index_type i) const
  {
    DBUG_ASSERT(!(i < 0 || i >= (index_type)size()));
    set_bg_adapter(true);

    const Geometry *p= &((*m_geo_vect)[i]);
    return *((const T*)p);
  }


  reference operator[](index_type i)
  {
    DBUG_ASSERT(!(i < 0 || i >= (index_type)size()));
    set_bg_adapter(true);

    Geometry *p= &((*m_geo_vect)[i]);
    return *((T*)p);
  }


  Gis_wkb_vector(const void *ptr, size_t nbytes, const Flags_t &flags,
                 srid_t srid, bool is_bg_adapter= true);
  Gis_wkb_vector(const self &v);

  Gis_wkb_vector() :Geometry()
  {
    m_geo_vect= NULL;
  }


  ~Gis_wkb_vector()
  {
    /*
      See ~Geometry() for why we do try-catch like this.

      Note that although ~Inplace_vector() calls std::vector member functions,
      all of them have no-throw guarantees, so this function won't throw any
      exception now. We do so nonetheless for potential mis-use of exceptions
      in futher code.
    */
#if !defined(DBUG_OFF)
    try
    {
#endif
      if (!is_bg_adapter())
        return;
      if (m_geo_vect != NULL)
        clear_wkb_data();
#if !defined(DBUG_OFF)
    }
    catch (...)
    {
      // Should never throw exceptions in destructor.
      DBUG_ASSERT(false);
    }
#endif
  }


  void clear_wkb_data()
  {
    delete m_geo_vect;
    m_geo_vect= NULL;
  }


  self &operator=(const self &rhs);
  virtual void shallow_push(const Geometry *g);

  Geo_vector *get_geo_vect(bool create_if_null= false)
  {
    if (m_geo_vect == NULL && create_if_null)
      m_geo_vect= new Geo_vector;
    return m_geo_vect;
  }


  Geo_vector *get_geo_vect() const
  {
    return m_geo_vect;
  }

  void set_geo_vect(Geo_vector *ptr)
  {
    m_geo_vect= ptr;
  }

  /*
    Give up ownership of m_ptr and m_geo_vect, so as not to release them when
    this object is destroyed, to be called when the two member is shallow
    assigned to another geometry object.
   */
  virtual void donate_data()
  {
    set_ownmem(false);
    set_nbytes(0);
    m_ptr= NULL;
    m_geo_vect= NULL;
  }

  void set_ptr(void *ptr, size_t len);
  void clear();
  size_t get_nbytes_free() const;
  size_t current_size() const;
  void push_back(const T &val);
  void resize(size_t sz);
  void reassemble();
private:
  typedef Gis_wkb_vector<Gis_point> Linestring;
  typedef Gis_wkb_vector<Linestring> Multi_linestrings;

}; // Gis_wkb_vector

/// @brief Constructor.
/// @param ptr points to the geometry's wkb data's 1st byte, right after its
/// wkb header if any.
/// @param bo the byte order indicated by ptr's wkb header.
/// @param geotype the geometry type indicated by ptr's wkb header
/// @param dim dimension of the geometry
/// @param is_bg_adapter Whether this object is created to be used by
///        Boost Geometry, or to be only used in MySQL code.
template <typename T>
Gis_wkb_vector<T>::
Gis_wkb_vector(const void *ptr, size_t nbytes, const Flags_t &flags,
               srid_t srid, bool is_bg_adapter)
  :Geometry(ptr, nbytes, flags, srid)
{
  DBUG_ASSERT((ptr != NULL && nbytes > 0) || (ptr == NULL && nbytes == 0));
  set_ownmem(false); // We use existing WKB data and don't own that memory.
  set_bg_adapter(is_bg_adapter);
  m_geo_vect= NULL;

  if (!is_bg_adapter)
    return;

  std::auto_ptr<Geo_vector> guard;

  wkbType geotype= get_geotype();
  // Points don't need it, polygon creates it when parsing.
  if (geotype != Geometry::wkb_point &&
      geotype != Geometry::wkb_polygon && ptr != NULL)
    guard.reset(m_geo_vect= new Geo_vector());
  // For polygon parsing to work
  if (geotype == Geometry::wkb_polygon)
    m_ptr= NULL;

  // Why: wkb_polygon_inner_rings should parse in polygon as a whole.
  // Don't call get_cptr() here, it returns NULL.
  if (geotype != Geometry::wkb_polygon_inner_rings && ptr != NULL)
    parse_wkb_data(this, static_cast<const char *>(ptr));

  guard.release();
}


template <typename T>
Gis_wkb_vector<T>::
Gis_wkb_vector(const Gis_wkb_vector<T> &v) :Geometry(v), m_geo_vect(NULL)
{
  DBUG_ASSERT((v.get_ptr() != NULL && v.get_nbytes() > 0) ||
              (v.get_ptr() == NULL && !v.get_ownmem() &&
               v.get_nbytes() == 0));
  if (v.is_bg_adapter() == false || v.get_ptr() == NULL)
    return;
  m_geo_vect= new Geo_vector();
  std::auto_ptr<Geo_vector> guard(m_geo_vect);

  const_cast<self &>(v).reassemble();
  set_flags(v.get_flags());
  set_nbytes(v.get_nbytes());
  if (get_nbytes() > 0)
  {
    m_ptr= gis_wkb_alloc(v.get_nbytes() + 2);
    if (m_ptr == NULL)
    {
      m_geo_vect= NULL;
      set_ownmem(false);
      set_nbytes(0);
      return;
    }
    memcpy(m_ptr, v.get_ptr(), v.get_nbytes());
    /*
      The extra 2 bytes makes the buffer usable by get_nbytes_free.
      It's hard to know how many more space will be needed so let's
      allocate more later.
    */
    get_cptr()[get_nbytes()]= '\xff';
    get_cptr()[get_nbytes() + 1]= '\0';
    parse_wkb_data(this, get_cptr(), v.get_geo_vect()->size());
    set_ownmem(true);
  }
  guard.release();
}


/**
  Deep assignment from vector 'rhs' to this object.
  @param p the Gis_wkb_vector<T> instance to duplicate from.
  */
template <typename T>
Gis_wkb_vector<T> &Gis_wkb_vector<T>::operator=(const Gis_wkb_vector<T> &rhs)
{
  if (this == &rhs)
    return *this;
  Geometry::operator=(rhs);

  DBUG_ASSERT((m_ptr != NULL && get_ownmem() && get_nbytes() > 0) ||
              (m_ptr == NULL && !get_ownmem() && get_nbytes() == 0));
  DBUG_ASSERT((rhs.get_ptr() != NULL && rhs.get_nbytes() > 0) ||
              (rhs.get_ptr() == NULL && !rhs.get_ownmem() &&
               rhs.get_nbytes() == 0));

  if (m_owner == NULL)
    m_owner= rhs.get_owner();

  size_t nbytes_free= get_nbytes_free();
  clear_wkb_data();

  if (rhs.get_ptr() == NULL)
  {
    if (m_ptr != NULL)
      gis_wkb_free(m_ptr);
    m_ptr= NULL;
    set_flags(rhs.get_flags());
    return *this;
  }

  /*
    Geometry v may have out of line components, need to reassemble first.
   */
  const_cast<self &>(rhs).reassemble();

  /*
    If have no enough space, reallocate with extra space padded with required
    bytes;
   */
  if (m_ptr == NULL || get_nbytes() + nbytes_free < rhs.get_nbytes())
  {
    gis_wkb_free(m_ptr);
    m_ptr= gis_wkb_alloc(rhs.get_nbytes() + 32/* some extra space. */);
    if (m_ptr == NULL)
    {
      /*
        This object in this case is valid although it doesn't have any data.
       */
      set_nbytes(0);
      set_ownmem(false);
      return *this;
    }

    // Fill extra space with pattern defined by
    // Gis_wkb_vector<>::get_nbytes_free().
    char *cp= get_cptr();
    memset(cp + rhs.get_nbytes(), 0xFF, 32);
    cp[rhs.get_nbytes() + 31]= '\0';
  }

  /*
    If need less space than before, set remaining bytes to 0xFF as requred
    by Gis_wkb_vector<>::get_nbytes_free.
   */
  if (get_nbytes() > rhs.get_nbytes())
    memset(get_cptr() + rhs.get_nbytes(), 0xFF,
           get_nbytes() - rhs.get_nbytes());

  memcpy(m_ptr, rhs.get_ptr(), rhs.get_nbytes());

  set_flags(rhs.get_flags());
  set_ownmem(true);

  m_geo_vect= new Geo_vector();
  parse_wkb_data(this, get_cptr());
  return *this;
}


/**
  The copy constructors of Geometry classes always do deep copy, but when
  pushing a Geometry object into its owner's geo.m_geo_vect, we want to do
  shallow copy because we want all elements in geo.m_geo_vect vector point
  into locations in the geo.m_ptr buffer. In such situations call this
  function.
  @param vec A Geometry object's m_geo_vect vector, into which we want to
             push a Geometry object.
  @param geo The Geometry object to push into vec.
  @return The address of the Geometry object stored in vec.
 */
template <typename T>
void Gis_wkb_vector<T>::shallow_push(const Geometry *g)
{
  const T &geo= *(static_cast<const T *>(g));
  T *pgeo= NULL;

  if (m_geo_vect == NULL)
    m_geo_vect= new Geo_vector();
  // Allocate space and create an object with its default constructor.
  pgeo= static_cast<T *>(m_geo_vect->append_object());
  DBUG_ASSERT(pgeo != NULL);
  if (pgeo == NULL)
    return;

  pgeo->set_flags(geo.get_flags());
  pgeo->set_srid(geo.get_srid());
  pgeo->set_bg_adapter(true);
  // Such a shallow copied object never has its own memory regardless of geo.
  pgeo->set_ownmem(false);

  // This will parse and set up pgeo->m_geo_vect properly.
  // Do not copy elements from geo.m_geo_vect into that of pgeo
  // otherwise STL does deep copy using the Geometry copy constructor.
  pgeo->set_ptr(geo.get_ptr(), geo.get_nbytes());
  pgeo->set_owner(geo.get_owner());
}


template <typename T>
void Gis_wkb_vector<T>::set_ptr(void *ptr, size_t len)
{
  DBUG_ASSERT(!(ptr == NULL && len > 0));
  set_bg_adapter(true);
  if (get_geotype() != Geometry::wkb_polygon)
  {
    if (get_ownmem() && m_ptr != NULL)
      gis_wkb_free(m_ptr);
    m_ptr= ptr;
    if (m_geo_vect)
      clear_wkb_data();
  }
  set_nbytes(len);
  /* When invoked, this object may or may not have its own memory. */
  if (get_geotype() != Geometry::wkb_polygon_inner_rings && m_ptr != NULL)
  {
    if (m_geo_vect == NULL)
      m_geo_vect= new Geo_vector();
    parse_wkb_data(this, get_cptr());
  }
}


/**
  Update support
  We suppose updating a geometry can happen in the following ways:
  1. create an empty geo, then append components into it, the geo must
     be a topmost one; a complex geometry such as a multilinestring can be
     seen as a tree of geometry components, and the mlstr is the topmost
     geometry, i.e. the root of the tree, its lstrs are next layer of nodes,
     their points are the 3rd layer of tree nodes. Only the root owns the
     wkb buffer, other components point somewhere into the buffer, and can
     only read the data.

     Polygons are only used by getting its exterior ring or inner rings and
     then work on that/those rings, never used as a whole.

  2. *itr=value, each geo::m_owner can be used to track the topmost
     memory owner, and do reallocation to accormodate the value. This is
     for now not supported, will be if needed.

     So far geometry assignment are only used for point objects in boost
     geometry, thus only Geometry and Gis_point have operator=, no other
     classes need so, and thus there is no need for reallocation.
  3. call resize() to append some objects at the end, then assign/append
     values to the added objects using push_back. Objects added this way
     are out of line(unless the object is a point), and user need to call
     reassemble() to make them inline, i.e. stored in its owner's memory.
*/

/// Clear geometry data of this object.
template <typename T>
void Gis_wkb_vector<T>::clear()
{
  if (!m_geo_vect)
  {
    DBUG_ASSERT(m_ptr == NULL);
    return;
  }

  DBUG_ASSERT(m_geo_vect && get_geotype() != Geometry::wkb_polygon);

  // Keep the component vector because this object can be reused again.
  const void *ptr= get_ptr();
  set_bg_adapter(true);

  if (ptr && get_ownmem())
  {
    gis_wkb_free(const_cast<void *>(ptr));
    set_ownmem(false);
  }

  m_ptr= NULL;
  clear_wkb_data();
  set_nbytes(0);
}


/// Returns payload number of bytes of the topmost geometry holding this
/// geometry, i.e. the memory owner.
template <typename T>
size_t Gis_wkb_vector<T>::current_size() const
{
  // Polygon's data may not stay in a continuous chunk, and we update
  // its data using the outer/inner rings.
  DBUG_ASSERT(get_geotype() != Geometry::wkb_polygon);
  set_bg_adapter(true);
  if (m_geo_vect == NULL || m_geo_vect->empty())
    return 0;

  return get_nbytes();
}


/// Get number of free bytes in the buffer held by m_ptr. this object must be
/// an topmost geometry which owns memory.
template <typename T>
size_t Gis_wkb_vector<T>::get_nbytes_free() const
{
  DBUG_ASSERT((this->get_ownmem() && m_ptr) || (!get_ownmem() && !m_ptr));

  size_t cap= current_size();
  if (cap == 0)
  {
    DBUG_ASSERT(m_ptr == NULL);
    return 0;
  }

  const char *p= NULL, *ptr= get_cptr();
  DBUG_ASSERT(ptr != NULL);

  /*
    There will always be remaining free space because in push_back, when
    number of free bytes equals needed bytes we will do a realloc.
   */
  for (p= ptr + cap; *p != 0; p++)
    ;

  return p - ptr - cap + 1;
}


template <typename T>
void Gis_wkb_vector<T>::push_back(const T &val)
{
  Geometry::wkbType geotype= get_geotype();

  DBUG_ASSERT(geotype != Geometry::wkb_polygon &&
              ((m_ptr && get_ownmem()) || (!m_ptr && !get_ownmem())));

  // Only three possible types of geometries for val, thus no need to
  // do val.reassemble().
  DBUG_ASSERT(val.get_geotype() == wkb_point ||
              val.get_geotype() == wkb_polygon ||
              val.get_geotype() == wkb_linestring);

  DBUG_ASSERT(val.get_ptr() != NULL);

  size_t cap= 0, nalloc= 0;
  size_t vallen, needed;
  void *src_val= val.get_ptr();

  if (m_geo_vect == NULL)
    m_geo_vect= new Geo_vector;
  set_bg_adapter(true);
  vallen= val.get_nbytes();
  /*
    Often inside bg, a polygon is created with no data, then append points
    into outer ring and inner rings, such a polygon is a 'constructed'
    polygon, and in this case we need to assemble
    its data into a continuous chunk.
   */
  if (val.get_geotype() == Geometry::wkb_polygon)
    src_val= get_packed_ptr(&val, &vallen);

  // The 4 types can be resized and have out-of-line components,
  // reassemble first in case we lose them when doing m_geo_vect->clear().
  if (geotype == Geometry::wkb_multilinestring ||
      geotype == Geometry::wkb_geometrycollection ||
      geotype == Geometry::wkb_polygon_inner_rings ||
      geotype == Geometry::wkb_multipolygon)
    reassemble();

  // Get cap only after reassemble().
  cap= current_size();

  needed= vallen + WKB_HEADER_SIZE;
  // Use >= instead of > because we always want to have trailing free bytes.
  if (needed >= this->get_nbytes_free())
  {
    nalloc= cap + ((needed * 2 > 256) ? needed * 2 : 256);
    void *ptr= get_ptr();
    m_ptr= gis_wkb_realloc(m_ptr, nalloc);
    if (m_ptr == NULL)
    {
      set_nbytes(0);
      set_ownmem(0);
      clear_wkb_data();
      return;
    }

    // Set unused space to -1, and last unused byte to 0.
    // Function get_nbytes_free relies on this format.
    memset(get_cptr() + cap, 0xff, nalloc - cap);
    get_cptr()[nalloc - 1]= '\0';
    memset(get_cptr() + cap, 0, sizeof(uint32));

    bool replaced= (ptr != m_ptr);
    set_ownmem(true);
    if (m_owner && m_owner->get_geotype() == Geometry::wkb_polygon)
      m_owner->set_ownmem(true);

    // After reallocation we need to parse again.
    if (cap > 0 && replaced)
    {
      size_t ngeos= 0;
      if (geotype == Geometry::wkb_polygon_inner_rings)
        ngeos= size();
      clear_wkb_data();
      parse_wkb_data(this, get_cptr(), ngeos);
    }
  }

  size_t wkb_header_size= 0;
  /* Offset for obj count, if needed. */
  size_t obj_count_len=
    ((cap == 0 && geotype != Geometry::wkb_polygon_inner_rings) ?
     sizeof(uint32) : 0);
  char *val_ptr= get_cptr() + cap + obj_count_len;

  // Append WKB header first, if needed.
  if (geotype == Geometry::wkb_multipoint ||
      geotype == Geometry::wkb_multipolygon ||
      geotype == Geometry::wkb_multilinestring ||
      geotype == Geometry::wkb_geometrycollection)
  {
    Geometry::wkbType vgt= val.get_geotype();
    DBUG_ASSERT((geotype == Geometry::wkb_multipoint &&
                 vgt == Geometry::wkb_point) ||
                (geotype == Geometry::wkb_multipolygon &&
                 vgt == Geometry::wkb_polygon) ||
                (geotype == Geometry::wkb_multilinestring &&
                 vgt == Geometry::wkb_linestring) ||
                geotype == Geometry::wkb_geometrycollection);

    val_ptr= write_wkb_header(val_ptr, vgt);
    wkb_header_size= WKB_HEADER_SIZE;
  }

  // Copy val's data into buffer, then parse it.
  memcpy(val_ptr, src_val, vallen);
  set_nbytes(get_nbytes() + wkb_header_size + obj_count_len + vallen);

  // Append geometry component into m_geo_vect vector. Try to avoid
  // unnecessary parse by calling the right version of set_ptr. And do
  // shallow push so that the element in m_geo_vect point to WKB buffer
  // rather than have its own copy of the same WKB data.
  T val2;
  val2.set_flags(val.get_flags());
  val2.set_srid(val.get_srid());
  val2.Geometry::set_ptr(val_ptr);
  val2.set_nbytes(vallen);
  val2.set_owner(this);
  val2.set_ownmem(false);

  shallow_push(&val2);
  val2.Geometry::set_ptr(NULL);

  if (val2.get_geotype() == Geometry::wkb_polygon)
    own_rings(&(m_geo_vect->back()));
  if (geotype != Geometry::wkb_polygon_inner_rings)
  {
    int4store(get_ucptr(), uint4korr(get_ucptr()) + 1);
    DBUG_ASSERT(uint4korr(get_ucptr()) == this->m_geo_vect->size());
  }

  if (val.get_geotype() == Geometry::wkb_polygon)
    gis_wkb_free(src_val);
}


/*
  Resize as in std::vector<>::resize().

  Because resize can be called to append an empty geometry into its owner,
  we have to allow pushing into an empty geo and its memory will not
  be in the same chunk as its owner, which is OK for bg since the
  Boost Range concept doesn't forbid so. But inside MySQL we should
  reassemble the geometries into one chunk before using the WKB buffer
  directly, by calling reassemble().
*/
template<typename T>
void Gis_wkb_vector<T>::resize(size_t sz)
{
  if (m_geo_vect == NULL)
    m_geo_vect= new Geo_vector;
  Geometry::wkbType geotype= get_geotype();
  size_t ngeo= m_geo_vect->size();
  size_t dim= GEOM_DIM;
  size_t ptsz= SIZEOF_STORED_DOUBLE * dim;
  bool is_mpt= (geotype == Geometry::wkb_multipoint);

  // Can resize a topmost geometry or a out of line geometry which has
  // or will have its own memory(i.e. one that's not using others' memory).
  // Points are fixed size, polygon doesn't hold data directly.
  DBUG_ASSERT(!(m_ptr != NULL && !get_ownmem()) &&
              geotype != Geometry::wkb_point &&
              geotype != Geometry::wkb_polygon);
  set_bg_adapter(true);
  if (sz == ngeo)
    return;
  // Shrinking the vector.
  if (sz < ngeo)
  {
    // Some elements may be out of line, must do so otherwise we don't
    // know how much to shrink in m_ptr.
    reassemble();
    size_t sublen= 0;
    for (size_t i= ngeo; i > sz; i--)
      sublen+= (*m_geo_vect)[i - 1].get_nbytes();

    // '\0' not allowed in middle and no need for ending '\0' because it's
    // at the end of the original free chunk which is right after this chunk.
    memset((get_cptr() + get_nbytes() - sublen), 0xff, sublen);
    set_nbytes(get_nbytes() - sublen);

#if !defined(DBUG_OFF)
    bool rsz_ret= m_geo_vect->resize(sz);
    DBUG_ASSERT(rsz_ret == false);
#else
    m_geo_vect->resize(sz);
#endif
    if (get_geotype() != Geometry::wkb_polygon_inner_rings)
    {
      DBUG_ASSERT(uint4korr(get_ucptr()) == ngeo);
      int4store(get_ucptr(), static_cast<uint32>(sz));
    }
    return;
  }

  char *ptr= NULL, *ptr2= NULL;

  // We can store points directly into its owner, points are fixed length,
  // thus don't need its own memory.
  if (geotype == Geometry::wkb_linestring ||
      geotype == Geometry::wkb_multipoint)
  {
    size_t left= get_nbytes_free(),
           needed= (sz - ngeo) * (ptsz + (is_mpt ? WKB_HEADER_SIZE : 0)),
           nalloc, cap= get_nbytes();

    if (left <= needed)
    {
      nalloc= cap + 32 * (left + needed);
      ptr= get_cptr();
      m_ptr= gis_wkb_realloc(m_ptr, nalloc);
      if (m_ptr == NULL)
      {
        set_nbytes(0);
        set_ownmem(0);
        clear_wkb_data();
        return;
      }
      ptr2= get_cptr();
      memset((ptr2 + cap), 0xff, nalloc - cap);
      ptr2[nalloc - 1]= '\0';
      /*
        Only set when cap is 0, otherwise after this call get_nbytes_free()
        will work wrong, this is different from push_back because push_back
        always put data here more than 4 bytes inside itself.
      */
      if (cap == 0)
        int4store(get_ucptr(), 0);// obj count
      set_ownmem(true);

      if (cap > 0 && ptr != m_ptr)
      {
        clear_wkb_data();
        // Note: flags_.nbytes doesn't change.
        parse_wkb_data(this, get_cptr());
      }
    }
    ptr2= get_cptr();
    ptr= ptr2 + (cap ? cap : sizeof(uint32)/* obj count */);
    if (cap == 0)
      set_nbytes(sizeof(uint32));
  }
  else
    has_out_of_line_components(true);


  /*
    Because the pushed objects have their own memory, here we won't modify
    m_ptr memory at all.
  */
  for (size_t cnt= sz - ngeo; cnt; cnt--)
  {
    T tmp;
    tmp.set_owner(this);
    tmp.set_ownmem(false);
    // Points are directly put into owner's buffer, no need for own memory.
    if (tmp.get_geotype() == Geometry::wkb_point)
    {
      if (is_mpt)
      {
        ptr= write_wkb_header(ptr, Geometry::wkb_point);
        set_nbytes(get_nbytes() + WKB_HEADER_SIZE);
      }
      tmp.set_ptr(ptr, ptsz);
      set_nbytes(get_nbytes() + ptsz);
      ptr+= ptsz;
      int4store(get_ucptr(), uint4korr(get_ucptr()) + 1);
      DBUG_ASSERT(uint4korr(get_ucptr()) == m_geo_vect->size() + 1);
    }
    else
      DBUG_ASSERT(ptr == NULL && ptr2 == NULL);

    shallow_push(&tmp);
    if (tmp.get_geotype() == Geometry::wkb_polygon)
      own_rings(&(m_geo_vect->back()));

    // tmp will be filled by push_back after this call, which will make
    // tmp own its own memory, different from other geos in m_geo_vect,
    // this is OK, users should call reassemble() to put them into
    // a single chunk of memory.
  }
}


/**
   Because of resize, a geometry's components may reside not in one chunk,
   some may in the m_ptr's chunk; others have their own memory and only exist
   in m_geo_vect vector, not in ptr's chunk. Also, a constructed polygon's
   data is always not in a chunk and needs to be so when it's pushed into a
   multipolygon/geometry collection.
   Thus in mysql before using the returned geometry, also inside the
   container classes before using the wkb data or clearing m_geo_vect,
   we need to make them inline, i.e. reside in one chunk of memory.
   Can only resize a topmost geometry, thus no recursive reassemling
   to do for now.

   Algorithm:

   Step 1. Structure analysis

   Scan this geometry's components, see whether each of them has its own
   memory, if so it's 'out of line', otherwise it's 'inline'. Note down
   those owning memory in a map M1, for each entry X in the map M1, the
   component's index in the component vector m_geo_vect is used as key;
   The inline chunk of memory right before it which may have any number
   of inline components, and the inline chunk's start and end address pair
   is used as value of the inserted item X. If there is no inline chunk
   before the component, X's pointer range is (0, 0). The inline chunk's
   starting address is well maintained during the scan.


   Step 2. Reassembling

   Allocate enough memory space (the length is accumulated in step 1) as WKB
   buffer and call it GBuf here, then copy the WKB of inline and out-of-line
   geometries into GBuf in original order:
   Go through the map by index order, for each item, copy the WKB chunk
   before it into the WKB buffer, then copy this out-of-line geometry's WKB
   into GBuf.

   Special treatment of polygon: we have to pack its value and store their
   WKB separately into a map GP in step 1, and in step 2 for a polygon,
   get its WKB from GP, and at the end release WKB memory buffers held by
   items of GP.
 */
template<typename T>
void Gis_wkb_vector<T>::reassemble()
{
  set_bg_adapter(true);
  Geometry::wkbType geotype= get_geotype();
  if (geotype == Geometry::wkb_point || geotype == Geometry::wkb_polygon ||
      geotype == Geometry::wkb_multipoint || m_geo_vect == NULL ||
      geotype == Geometry::wkb_linestring || m_geo_vect->size() == 0 ||
      !has_out_of_line_components())
    return;

  if (m_geo_vect == NULL)
    m_geo_vect= new Geo_vector;
  typedef std::map<size_t, std::pair<const char *, const char *> > segs_t;
  segs_t segs;
  size_t hdrsz= 0, num= m_geo_vect->size(), prev_in= 0, totlen= 0, segsz= 0;
  Geo_vector &vec= *m_geo_vect;
  const char *start= get_cptr(), *end= NULL, *prev_start= get_cptr();
  std::map<size_t, std::pair<void *, size_t> > plgn_data;
  std::map<size_t, std::pair<void *, size_t> >::iterator plgn_data_itr;
  bool is_inns= (geotype == Geometry::wkb_polygon_inner_rings);

  // True if just passed by a geometry having its own memory and not stored
  // inside owner's memory during the scan.
  bool out= false;
  if (geotype != Geometry::wkb_polygon_inner_rings)
    hdrsz= WKB_HEADER_SIZE;

  uint32 none= 0;// Used when all components are out of line.

  // Starting step one of the algorithm --- Structure Analysis.
  for (size_t i= 0; i < num; i++)
  {
    T *veci= &(vec[i]);
    // Polygons are always(almost) out of line. One with its own memory is
    // always out of line.
    if (veci->get_geotype() == Geometry::wkb_polygon || veci->get_ownmem())
    {
      // In case of a polygon, see if it's already inline in a different
      // way from other types of geometries.
      if (veci->get_geotype() == Geometry::wkb_polygon &&
          polygon_is_packed(veci, this))
      {
        if (out)
        {
          out= false;
          DBUG_ASSERT(prev_start == veci->get_ptr());
        }
        prev_in= i;
        continue;
      }

      // Record the bytes before 1st geometry component.
      if (i == 0)
      {
        if (m_ptr)
        {
          start= get_cptr();
          end= start + sizeof(uint32)/* num geometrys*/ ;
        }
        else if (!is_inns)
        {
          start= reinterpret_cast<char *>(&none);
          end= start + sizeof(none);
        }
        else
          start= end= NULL;
      }
      // The previous geometry is already out of line, or no m_ptr allocated.
      else if (out || !prev_start)
      {
        start= NULL;
        end= NULL;
      }
      else // The previous geometry is inline, note down the inline range.
      {
        start= prev_start;
        if (veci->get_geotype() == Geometry::wkb_polygon)
          end= get_packed_ptr(&(vec[prev_in])) + vec[prev_in].get_nbytes();
        else
          end= vec[prev_in].get_cptr() + vec[prev_in].get_nbytes();
        prev_start= end;
        // The 'end' points to the 1st byte of next geometry stored in its
        // owner's memory.
      }

      if (veci->get_geotype() != Geometry::wkb_polygon)
      {
        // When this geometry is a geometry collection, we need to make its
        // components in one chunk first. Not gonna implement this yet since
        // BG doesn't use geometry collection yet, and consequently no
        // component can be a multipoint/multilinestring/multipolygon or a
        // geometrycollection. And multipoint components are already supported
        // so not forbidding them here.
#if !defined(DBUG_OFF)
        Geometry::wkbType veci_gt= veci->get_geotype();
#endif
        DBUG_ASSERT(veci_gt != wkb_geometrycollection &&
                    veci_gt != wkb_multilinestring &&
                    veci_gt != wkb_multipolygon);
        /* A point/multipoint/linestring is always in one memory chunk. */
        totlen+= veci->get_nbytes() + hdrsz;
      }
      else
      {
        // Must be a polygon out of line.
        size_t nbytes= 0;
        void *plgn_base= get_packed_ptr(veci, &nbytes);
        DBUG_ASSERT(veci->get_nbytes() == 0 || veci->get_nbytes() == nbytes);
        veci->set_nbytes(nbytes);
        plgn_data.insert(std::make_pair(i, std::make_pair(plgn_base,nbytes)));
        totlen+= nbytes + hdrsz;
      }

      segs.insert(std::make_pair(i, std::make_pair(start, end)));
      out= true;
    }
    else
    {
      if (out)
      {
        out= false;
        DBUG_ASSERT(prev_start == veci->get_ptr());
      }
      prev_in= i;
    }
  }

  segsz= segs.size();
  if (segsz == 0)
  {
    has_out_of_line_components(false);
    return;
  }

  size_t nbytes= get_nbytes();
  DBUG_ASSERT((nbytes == 0 && m_ptr == NULL && num == segsz) ||
              (nbytes > 0 && num >= segsz));

  // If all are out of line, m_ptr is 0 and no room for ring count, otherwise
  // the space for ring count is already counted above.
  totlen+= (nbytes ? nbytes : (is_inns ? 0 : sizeof(uint32)));

  size_t len= 0, total_len= 0, last_i= 0, numgeoms= 0;
  // Allocate extra space as free space for the WKB buffer, and write it as
  // defined pattern.
  const size_t extra_wkb_free_space= 32;
  char *ptr= static_cast<char *>(gis_wkb_alloc(totlen + extra_wkb_free_space));
  // The header(object count) is already copied.
  char *q= ptr;

  if (ptr == NULL)
  {
    clear_wkb_data();
    m_ptr= NULL;
    set_nbytes(0);
    set_ownmem(false);
    goto exit;
  }
  memset(ptr + totlen, 0xff, extra_wkb_free_space - 1);
  ptr[totlen + extra_wkb_free_space - 1]= '\0';

  // Starting step two of the algorithm --- Reassembling.
  // Assemble the ins and outs into a single chunk.
  for (segs_t::iterator itr= segs.begin(); itr != segs.end(); ++itr)
  {
    size_t i= itr->first;
    start= itr->second.first;
    end= itr->second.second;
    const Geometry *veci= &(vec[i]);
    last_i= i;

    // Copy the inline geometries before veci into buffer.
    if (start)
    {
      memcpy(q, start, len= end - start);
      q+= len;
      total_len+= len;
    }

    // Set WKB header. This geometry must be one of multilinestring,
    // multipolygon or a polygon's inner rings.
    if (get_geotype() != Geometry::wkb_polygon_inner_rings)
    {
      q= write_wkb_header(q, veci->get_geotype());
      total_len+= hdrsz;
    }

    // Copy the out of line geometry into buffer. A polygon's data isn't
    // packed inside itself, we've packed it and recorded it in plgn_data.
    plgn_data_itr= plgn_data.find(i);
    if (veci->get_geotype() != Geometry::wkb_polygon)
    {
      DBUG_ASSERT(plgn_data_itr == plgn_data.end());
      len= veci->get_nbytes();
      memcpy(q, veci->get_ptr(), len);
    }
    else
    {
      DBUG_ASSERT(plgn_data_itr != plgn_data.end());
      len= plgn_data_itr->second.second;
      memcpy(q, plgn_data_itr->second.first, len);
    }
    q+= len;
    total_len+= len;
  }

  // There may be trailing inline geometries to copy at old tail.
  if (last_i < vec.size() - 1)
  {
    len= get_cptr() + get_nbytes() - prev_start;
    memcpy(q, prev_start, len);
    total_len+= len;
  }
  DBUG_ASSERT(total_len == totlen);

  // Inner rings doesn't have ring count.
  if (!is_inns)
  {
    DBUG_ASSERT(segsz + uint4korr(ptr) <= 0xFFFFFFFF);
    int4store(reinterpret_cast<uchar *>(ptr),
              uint4korr(ptr) + static_cast<uint32>(segsz));
  }

  numgeoms= m_geo_vect->size();
  clear_wkb_data();
  set_ptr(ptr, totlen);
  // An inner ring isn't parsed in set_ptr, has to parse separately since
  // we don't know its number of rings.
  if (is_inns)
    parse_wkb_data(this, get_cptr(), numgeoms);
  set_ownmem(true);
exit:
  for (plgn_data_itr= plgn_data.begin();
      plgn_data_itr != plgn_data.end(); ++plgn_data_itr)
    gis_wkb_free(plgn_data_itr->second.first);

  has_out_of_line_components(false);
}

//@} //

/***************************** LineString *******************************/

class Gis_line_string: public Gis_wkb_vector<Gis_point>
{
  // Maximum number of points in LineString that can fit into String
  static const uint32 max_n_points=
    (uint32) (UINT_MAX32 - WKB_HEADER_SIZE - 4 /* n_points */) /
    POINT_DATA_SIZE;
public:
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

  /**** Boost Geometry Adapter Interface ******/

  typedef Gis_wkb_vector<Gis_point> base_type;
  typedef Gis_line_string self;

  explicit Gis_line_string(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_linestring, 0), default_srid, is_bg_adapter)
  {}

  Gis_line_string(const void *wkb, size_t len,
                  const Flags_t &flags, srid_t srid)
    :base_type(wkb, len, flags, srid, true)
  {
    set_geotype(wkb_linestring);
  }

  Gis_line_string(const self &ls) :base_type(ls)
  {}
};

/*
  We have to use such an independent class in order to meet Ring Concept of
  Boost Geometry --- there must be a specialization of traits::tag defining
  ring_tag as type.
  If directly use Gis_line_string, we would have defined that tag twice.
*/
class Gis_polygon_ring : public Gis_wkb_vector<Gis_point>
{
public:
  typedef Gis_wkb_vector<Gis_point> base;
  typedef Gis_polygon_ring self;

  virtual ~Gis_polygon_ring()
  {}
  Gis_polygon_ring(const void *wkb, size_t nbytes,
                   const Flags_t &flags, srid_t srid)
    :base(wkb, nbytes, flags, srid, true)
  {
    set_geotype(wkb_linestring);
  }

  // Coordinate data type, closed-ness and direction will never change, thus no
  // need for the template version of copy constructor.
  Gis_polygon_ring(const self &r) :base(r)
  {}

  Gis_polygon_ring()
    :base(NULL, 0, Flags_t(Geometry::wkb_linestring, 0),
          default_srid, true)
  {}

  bool set_ring_order(bool want_ccw);
};

/***************************** Polygon *******************************/

// For internal use only, only convert types, don't create rings.
inline Gis_polygon_ring *outer_ring(const Geometry *g)
{
  DBUG_ASSERT(g->get_geotype() == Geometry::wkb_polygon);
  Gis_polygon_ring *out= static_cast<Gis_polygon_ring *>(g->get_ptr());

  return out;
}


class Gis_polygon: public Geometry
{
public:
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


  /**** Boost Geometry Adapter Interface ******/
  typedef Gis_polygon self;
  typedef Gis_polygon_ring ring_type;
  typedef Gis_wkb_vector<ring_type> inner_container_type;

  ring_type &outer() const
  {
    DBUG_ASSERT(!polygon_is_wkb_form());
    set_bg_adapter(true);
    // Create outer ring if none, although read only, calller may just want
    // to traverse the outer ring if any.
    if (this->m_ptr == NULL)
      const_cast<self *>(this)->make_rings();

    return *(outer_ring(this));
  }

  inner_container_type &inners() const
  {
    DBUG_ASSERT(!polygon_is_wkb_form());
    set_bg_adapter(true);
    // Create inner rings if none, although read only, calller may just want
    // to traverse the inner rings if any.
    if (m_inn_rings == NULL)
      const_cast<self *>(this)->make_rings();

    return *m_inn_rings;
  }

  /// Clears outer and inner rings.
  void clear()
  {
    set_bg_adapter(true);
    outer_ring(this)->clear();
    if (m_inn_rings)
      m_inn_rings->clear();
  }

  Gis_polygon(const void *wkb, size_t nbytes,
              const Flags_t &flags, srid_t srid);


  /*
    We can't require boost geometry use the 'polygon' in any particular way,
    so we have to default to true.
  */
  explicit Gis_polygon(bool isbgadapter= true)
    :Geometry(NULL, 0, Flags_t(Geometry::wkb_polygon, 0), default_srid)
  {
    m_inn_rings= NULL;
    set_bg_adapter(isbgadapter);
  }

  Gis_polygon(const self &r);
  Gis_polygon &operator=(const Gis_polygon &rhs);
  ~Gis_polygon();

  void to_wkb_unparsed();
  void set_ptr(void *ptr, size_t len);

  /*
    Give up ownership of m_ptr and m_inn_rings, so as not to release them when
    this object is destroyed, to be called when the two member is shallow
    assigned to another geometry object.
   */
  void donate_data()
  {
    set_ownmem(false);
    set_nbytes(0);
    m_ptr= NULL;
    m_inn_rings= NULL;
  }

  bool set_polygon_ring_order();

  inner_container_type *inner_rings() const
  {
    return m_inn_rings;
  }

  void set_inner_rings(inner_container_type *inns)
  {
    m_inn_rings= inns;
  }

private:
  inner_container_type *m_inn_rings;

  void make_rings();
};


/***************************** MultiPoint *******************************/

class Gis_multi_point: public Gis_wkb_vector<Gis_point>
{
  // Maximum number of points in MultiPoint that can fit into String
  static const uint32 max_n_points=
    (uint32) (UINT_MAX32 - WKB_HEADER_SIZE - 4 /* n_points */) /
    (WKB_HEADER_SIZE + POINT_DATA_SIZE);
public:
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


  /**** Boost Geometry Adapter Interface ******/

  typedef Gis_wkb_vector<Gis_point> base_type;
  typedef Gis_multi_point self;

  explicit Gis_multi_point(bool is_bg_adapter= true)
    :base_type(NULL, 0, Flags_t(wkb_multipoint, 0),
               default_srid, is_bg_adapter)
  {}

  Gis_multi_point(const void *ptr, size_t nbytes,
                  const Flags_t &flags, srid_t srid)
    :base_type(ptr, nbytes, flags, srid, true)
  {
    set_geotype(wkb_multipoint);
  }

  Gis_multi_point(const self &mpts) :base_type(mpts)
  {}
};


/***************************** MultiLineString *******************************/

class Gis_multi_line_string : public Gis_wkb_vector<Gis_line_string>
{
public:
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

  /**** Boost Geometry Adapter Interface ******/

  typedef Gis_wkb_vector<Gis_line_string> base;
  typedef Gis_multi_line_string self;

  explicit Gis_multi_line_string(bool is_bg_adapter= true)
    :base(NULL, 0, Flags_t(wkb_multilinestring, 0),
          default_srid, is_bg_adapter)
  {}

  Gis_multi_line_string(const void *ptr, size_t nbytes,
                        const Flags_t &flags, srid_t srid)
    :base(ptr, nbytes, Flags_t(wkb_multilinestring, nbytes), srid, true)
  {
    set_geotype(wkb_multilinestring);
  }

  Gis_multi_line_string(const self &mls) :base(mls)
  {}
};


/***************************** MultiPolygon *******************************/

class Gis_multi_polygon: public Gis_wkb_vector<Gis_polygon>
{
public:
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


  /**** Boost Geometry Adapter Interface ******/
  typedef Gis_multi_polygon self;
  typedef Gis_wkb_vector<Gis_polygon> base;

  explicit Gis_multi_polygon(bool is_bg_adapter= true)
    :base(NULL, 0, Flags_t(wkb_multipolygon, 0), default_srid, is_bg_adapter)
  {}

  Gis_multi_polygon(const void *ptr, size_t nbytes,
                    const Flags_t &flags, srid_t srid)
    :base(ptr, nbytes, flags, srid, true)
  {
    set_geotype(wkb_multipolygon);
  }

  Gis_multi_polygon(const self &mpl) :base(mpl)
  {}
};


/*********************** GeometryCollection *******************************/
class Gis_geometry_collection: public Geometry
{
public:
  Gis_geometry_collection()
    :Geometry(NULL, 0, Flags_t(wkb_geometrycollection, 0), default_srid)
  {
    set_bg_adapter(false);
  }
  Gis_geometry_collection(Geometry *geo, String *gcbuf);
  Gis_geometry_collection(srid_t srid, wkbType gtype, const String *gbuf,
                          String *gcbuf);
  virtual ~Gis_geometry_collection() {}       /* Remove gcc warning */
  bool append_geometry(const Geometry *geo, String *gcbuf);
  bool append_geometry(srid_t srid, wkbType gtype,
                       const String *gbuf, String *gcbuf);
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


/**
  Gis_polygon objects and Gis_wkb_vector<> objects are of same size, and
  Gis_point and Geometry objects are smaller. They are always allocated
  inside a Geometry_buffer object, unless used as boost geometry adapter,
  in which case the object may simply placed on stack or new'ed on heap.
 */
struct Geometry_buffer : public
  my_aligned_storage<sizeof(Gis_polygon), MY_ALIGNOF(Gis_polygon)> {};


class WKB_scanner_event_handler
{
public:

  /**
    Notified when scanner sees the start of a geometry WKB.
    @param bo byte order of the WKB.
    @param geotype geometry type of the WKB;
    @param wkb WKB byte string, the first byte after the WKB header if any.
    @param len NO. of bytes of the WKB byte string starting from wkb.
               There can be many geometries in the [wkb, wkb+len) buffer.
    @param has_hdr whether there is a WKB header right before 'wkb' in the
                   byte string.
   */
  virtual void on_wkb_start(Geometry::wkbByteOrder bo,
                            Geometry::wkbType geotype,
                            const void *wkb, uint32 len, bool has_hdr)= 0;

  /**
    Notified when scanner sees the end of a geometry WKB.
    @param wkb the position of the first byte after the WKB byte string which
               the scanner just scanned.
   */
  virtual void on_wkb_end(const void *wkb)= 0;

  /*
    Called after each on_wkb_start/end call, if returns false, wkb_scanner
    will stop scanning.
   */
  virtual bool continue_scan() const
  {
    return true;
  }
};


const char*
wkb_scanner(const char *wkb, uint32 *len, uint32 geotype, bool has_hdr,
            WKB_scanner_event_handler *handler);
#endif

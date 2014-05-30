#ifndef ITEM_GEOFUNC_INCLUDED
#define ITEM_GEOFUNC_INCLUDED

/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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


/* This file defines all spatial functions */

#include "gcalc_slicescan.h"
#include <vector>
#include <list>
#include <set>
#include "inplace_vector.h"


class Item_func_spatial_operation;

/**
  A utility class to flatten any hierarchy of geometry collection into one
  with no nested geometry collections. All components are stored separately
  and all their data stored in this class, in order to easily manipulate them.
 */
class BG_geometry_collection
{
  size_t m_num_isolated;
  std::list<Geometry*> m_geos;
  Inplace_vector<Geometry_buffer> m_geobufs;
  Inplace_vector<String> m_geosdata;
public:
  typedef std::list<Geometry *> Geometry_list;

  BG_geometry_collection()
    :m_num_isolated(0), m_geobufs(key_memory_Geometry_objects_data),
    m_geosdata(key_memory_Geometry_objects_data)
  {}

  bool fill(const Geometry *geo)
  {
    return store_geometry(geo);
  }

  const Geometry_list &get_geometries() const
  {
    return m_geos;
  }

  Geometry_list &get_geometries()
  {
    return m_geos;
  }

  bool all_isolated() const
  {
    return m_num_isolated == m_geos.size();
  }

  size_t num_isolated() const
  {
    return m_num_isolated;
  }

  Gis_geometry_collection *as_geometry_collection(String *geodata) const;
  template<typename Coord_type, typename Coordsys>
  void merge_components(Item_func_spatial_operation *ifso,
                        bool *pdone, my_bool *pnull_value);
private:
  template<typename Coord_type, typename Coordsys>
  bool merge_one_run(Item_func_spatial_operation *ifso,
                     bool *pdone, my_bool *pnull_value);
  bool store_geometry(const Geometry *geo);
  Geometry *store(const Geometry *geo);
};


class Item_geometry_func: public Item_str_func
{
public:
  Item_geometry_func() :Item_str_func() {}

  Item_geometry_func(Item *a) :Item_str_func(a) {}
  Item_geometry_func(const POS &pos, Item *a) :Item_str_func(pos, a) {}

  Item_geometry_func(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_geometry_func(const POS &pos, Item *a,Item *b) :Item_str_func(pos, a,b) {}

  Item_geometry_func(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  Item_geometry_func(const POS &pos, PT_item_list *list);

  void fix_length_and_dec();
  enum_field_types field_type() const  { return MYSQL_TYPE_GEOMETRY; }
  Field *tmp_table_field(TABLE *t_arg);
  bool is_null() { (void) val_int(); return null_value; }
};

class Item_func_geometry_from_text: public Item_geometry_func
{
  typedef Item_geometry_func super;
public:
  Item_func_geometry_from_text(const POS &pos, Item *a)
    :Item_geometry_func(pos, a)
  {}
  Item_func_geometry_from_text(const POS &pos, Item *a, Item *srid)
    :Item_geometry_func(pos, a, srid)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  const char *func_name() const { return "st_geometryfromtext"; }
  String *val_str(String *);
};

class Item_func_geometry_from_wkb: public Item_geometry_func
{
  typedef Item_geometry_func super;
  String tmp_value;
public:
  Item_func_geometry_from_wkb(const POS &pos, Item *a)
    : Item_geometry_func(pos, a)
  {}
  Item_func_geometry_from_wkb(const POS &pos, Item *a, Item *srid):
    Item_geometry_func(pos, a, srid)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);
  const char *func_name() const { return "st_geometryfromwkb"; }
  String *val_str(String *);
};

class Item_func_as_wkt: public Item_str_ascii_func
{
public:
  Item_func_as_wkt(const POS &pos, Item *a): Item_str_ascii_func(pos, a) {}
  const char *func_name() const { return "st_astext"; }
  String *val_str_ascii(String *);
  void fix_length_and_dec();
};

class Item_func_as_wkb: public Item_geometry_func
{
public:
  Item_func_as_wkb(const POS &pos, Item *a): Item_geometry_func(pos, a) {}
  const char *func_name() const { return "st_aswkb"; }
  String *val_str(String *);
  enum_field_types field_type() const  { return MYSQL_TYPE_BLOB; }
};

class Item_func_geometry_type: public Item_str_ascii_func
{
public:
  Item_func_geometry_type(const POS &pos, Item *a): Item_str_ascii_func(pos, a)
  {}
  String *val_str_ascii(String *);
  const char *func_name() const { return "st_geometrytype"; }
  void fix_length_and_dec()
  {
    // "GeometryCollection" is the longest
    fix_length_and_charset(20, default_charset());
    maybe_null= 1;
  };
};

class Item_func_centroid: public Item_geometry_func
{
public:
  Item_func_centroid(const POS &pos, Item *a): Item_geometry_func(pos, a) {}
  const char *func_name() const { return "st_centroid"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_envelope: public Item_geometry_func
{
public:
  Item_func_envelope(const POS &pos, Item *a): Item_geometry_func(pos, a) {}
  const char *func_name() const { return "st_envelope"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_point: public Item_geometry_func
{
public:
  Item_func_point(const POS &pos, Item *a, Item *b)
    : Item_geometry_func(pos, a, b)
  {}
  const char *func_name() const { return "point"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_spatial_decomp: public Item_geometry_func
{
  enum Functype decomp_func;
public:
  Item_func_spatial_decomp(const POS &pos, Item *a, Item_func::Functype ft) :
    Item_geometry_func(pos, a)
  { decomp_func = ft; }
  const char *func_name() const
  {
    switch (decomp_func)
    {
      case SP_STARTPOINT:
        return "st_startpoint";
      case SP_ENDPOINT:
        return "st_endpoint";
      case SP_EXTERIORRING:
        return "st_exteriorring";
      default:
	DBUG_ASSERT(0);  // Should never happened
        return "spatial_decomp_unknown";
    }
  }
  String *val_str(String *);
};

class Item_func_spatial_decomp_n: public Item_geometry_func
{
  enum Functype decomp_func_n;
public:
  Item_func_spatial_decomp_n(const POS &pos, Item *a, Item *b, Item_func::Functype ft):
    Item_geometry_func(pos, a, b)
  { decomp_func_n = ft; }
  const char *func_name() const
  {
    switch (decomp_func_n)
    {
      case SP_POINTN:
        return "st_pointn";
      case SP_GEOMETRYN:
        return "st_geometryn";
      case SP_INTERIORRINGN:
        return "st_interiorringn";
      default:
	DBUG_ASSERT(0);  // Should never happened
        return "spatial_decomp_n_unknown";
    }
  }
  String *val_str(String *);
};

class Item_func_spatial_collection: public Item_geometry_func
{
  String tmp_value;
  enum Geometry::wkbType coll_type;
  enum Geometry::wkbType item_type;
public:
  Item_func_spatial_collection(const POS &pos,
     PT_item_list *list, enum Geometry::wkbType ct, enum Geometry::wkbType it):
  Item_geometry_func(pos, list)
  {
    coll_type=ct;
    item_type=it;
  }
  String *val_str(String *);
  void fix_length_and_dec()
  {
    Item_geometry_func::fix_length_and_dec();
    for (unsigned int i= 0; i < arg_count; ++i)
    {
      if (args[i]->fixed && args[i]->field_type() != MYSQL_TYPE_GEOMETRY)
      {
        String str;
        args[i]->print(&str, QT_ORDINARY);
        str.append('\0');
        my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "non geometric",
                 str.ptr());
      }
    }
  }

  const char *func_name() const;
};


/*
  Spatial relations
*/

class Item_func_spatial_mbr_rel: public Item_bool_func2
{
  enum Functype spatial_rel;
public:
  Item_func_spatial_mbr_rel(Item *a, Item *b, enum Functype sp_rel) :
    Item_bool_func2(a, b) { spatial_rel = sp_rel; }
  Item_func_spatial_mbr_rel(const POS &pos, Item *a, Item *b,
                            enum Functype sp_rel)
  : Item_bool_func2(pos, a, b) { spatial_rel = sp_rel; }
  longlong val_int();
  enum Functype functype() const
  {
    return spatial_rel;
  }
  enum Functype rev_functype() const
  {
    switch (spatial_rel)
    {
      case SP_CONTAINS_FUNC:
        return SP_WITHIN_FUNC;
      case SP_WITHIN_FUNC:
        return SP_CONTAINS_FUNC;
      default:
        return spatial_rel;
    }
  }

  const char *func_name() const;
  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
  void fix_length_and_dec() { maybe_null= 1; }
  bool is_null() { (void) val_int(); return null_value; }
};


class Item_func_spatial_rel: public Item_bool_func2
{
  enum Functype spatial_rel;
  Gcalc_heap collector;
  Gcalc_scan_iterator scan_it;
  Gcalc_function func;
  String tmp_value1,tmp_value2;
public:
  Item_func_spatial_rel(const POS &pos, Item *a,Item *b, enum Functype sp_rel);
  virtual ~Item_func_spatial_rel();
  longlong val_int();
  enum Functype functype() const
  {
    return spatial_rel;
  }
  enum Functype rev_functype() const
  {
    switch (spatial_rel)
    {
      case SP_CONTAINS_FUNC:
        return SP_WITHIN_FUNC;
      case SP_WITHIN_FUNC:
        return SP_CONTAINS_FUNC;
      default:
        return spatial_rel;
    }
  }

  const char *func_name() const;
  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }

  void fix_length_and_dec() { maybe_null= 1; }
  bool is_null() { (void) val_int(); return null_value; }

  template<typename CoordinateElementType, typename CoordinateSystemType>
  static int bg_geo_relation_check(Geometry *g1, Geometry *g2, bool *pisdone,
                                   Functype relchk_type, my_bool *);

protected:

  template<typename Geom_types>
  friend class BG_wrap;

  template<typename Geotypes>
  static int within_check(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int equals_check(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int disjoint_check(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int intersects_check(Geometry *g1, Geometry *g2,
                              bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int overlaps_check(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int touches_check(Geometry *g1, Geometry *g2,
                           bool *pbgdone, my_bool *pnull_value);
  template<typename Geotypes>
  static int crosses_check(Geometry *g1, Geometry *g2,
                           bool *pbgdone, my_bool *pnull_value);

  template<typename Coord_type, typename Coordsys>
  int geocol_relation_check(Geometry *g1, Geometry *g2, bool *pbgdone);
  template<typename Coord_type, typename Coordsys>
  int geocol_relcheck_intersect_disjoint(const typename BG_geometry_collection::
                                         Geometry_list *gv1,
                                         const typename BG_geometry_collection::
                                         Geometry_list *gv2,
                                         bool *pbgdone);
  template<typename Coord_type, typename Coordsys>
  int geocol_relcheck_within(const typename BG_geometry_collection::
                             Geometry_list *gv1,
                             const typename BG_geometry_collection::
                             Geometry_list *gv2,
                             bool *pbgdone);
  template<typename Coord_type, typename Coordsys>
  int geocol_equals_check(const typename BG_geometry_collection::
                          Geometry_list *gv1,
                          const typename BG_geometry_collection::
                          Geometry_list *gv2,
                          bool *pbgdone);

  int func_touches();
  int func_equals();

};


/*
  Spatial operations
*/

class Item_func_spatial_operation: public Item_geometry_func
{
protected:
  // It will call the protected member functions in this class,
  // no data member accessed directly.
  template<typename Geotypes>
  friend class BG_setop_wrapper;

  // Calls bg_geo_set_op.
  friend class BG_geometry_collection;

  template<typename Coord_type, typename Coordsys>
  Geometry *bg_geo_set_op(Geometry *g1, Geometry *g2, String *result, bool *);

  template<typename Coord_type, typename Coordsys>
  Geometry *combine_sub_results(Geometry *g1, Geometry *g2, String *result);

  Geometry *empty_result(String *str, uint32 srid);
  template<typename Coord_type, typename Coordsys>
  Geometry *geometry_collection_set_operation(Geometry *g1, Geometry *g2,
                                              String *result, bool *);

  Gcalc_function::op_type spatial_op;
  Gcalc_heap collector;
  Gcalc_function func;

  Gcalc_result_receiver res_receiver;
  Gcalc_operation_reducer operation;
  String tmp_value1,tmp_value2;

  /**
    We have to hold result buffers here because a set operation result
    geometry's buffer is directly used and set to String result object,
    so we have to release them properly manually since they won't be released
    at String object destruction, hence the need for bg_result_buf and
    bg_results.

    Hold data buffer of this set operation's final result geometry which is
    freed next time val_str is called since it can be used by upper Item nodes.
  */
  void *bg_result_buf;

  /**
    Result buffers for intermediate set operation results, which are freed
    before val_str returns.
  */
  std::set<void *> bg_results;

  bool assign_result(Geometry *geo, String *result);

  template <typename Geotypes>
  Geometry *intersection_operation(Geometry *g1, Geometry *g2,
                                   String *result, bool *opdone);
  template <typename Geotypes>
  Geometry *union_operation(Geometry *g1, Geometry *g2,
                            String *result, bool *opdone);
  template <typename Geotypes>
  Geometry *difference_operation(Geometry *g1, Geometry *g2,
                                 String *result, bool *opdone);
  template <typename Geotypes>
  Geometry *symdifference_operation(Geometry *g1, Geometry *g2,
                                    String *result, bool *opdone);
  template<typename Coord_type, typename Coordsys>
  Geometry *geocol_symdifference(Geometry *g1, Geometry *g2,
                                 String *result, bool *opdone);
  template<typename Coord_type, typename Coordsys>
  Geometry *geocol_difference(Geometry *g1, Geometry *g2,
                              String *result, bool *opdone);
  template<typename Coord_type, typename Coordsys>
  Geometry *geocol_intersection(Geometry *g1, Geometry *g2,
                                String *result, bool *opdone);
  template<typename Coord_type, typename Coordsys>
  Geometry *geocol_union(Geometry *g1, Geometry *g2,
                         String *result, bool *opdone);
public:
  Item_func_spatial_operation(const POS &pos, Item *a, Item *b,
                              Gcalc_function::op_type sp_op) :
    Item_geometry_func(pos, a, b), spatial_op(sp_op), bg_result_buf(NULL)
  {
  }
  virtual ~Item_func_spatial_operation();
  String *val_str(String *);
  const char *func_name() const;
  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
};


class Item_func_buffer: public Item_geometry_func
{
protected:
  class Transporter : public Gcalc_operation_transporter
  {
    int m_npoints;
    double m_d;
    Gcalc_function::op_type m_buffer_op;
    double x1,y1,x2,y2;
    double x00,y00,x01,y01;
    int add_edge_buffer(Gcalc_shape_status *st,
                        double x3, double y3, bool round_p1, bool round_p2);
    int add_last_edge_buffer(Gcalc_shape_status *st);
    int add_point_buffer(Gcalc_shape_status *st, double x, double y);
    int complete(Gcalc_shape_status *st);
  public:
    Transporter(Gcalc_function *fn, Gcalc_heap *heap, double d) :
      Gcalc_operation_transporter(fn, heap), m_npoints(0), m_d(d)
    {
      m_buffer_op= d > 0.0 ? Gcalc_function::op_union :
                             Gcalc_function::op_difference;
    }
    int single_point(Gcalc_shape_status *st, double x, double y);
    int start_line(Gcalc_shape_status *st);
    int complete_line(Gcalc_shape_status *st);
    int start_poly(Gcalc_shape_status *st);
    int complete_poly(Gcalc_shape_status *st);
    int start_ring(Gcalc_shape_status *st);
    int complete_ring(Gcalc_shape_status *st);
    int add_point(Gcalc_shape_status *st, double x, double y);
    int start_collection(Gcalc_shape_status *st, int nshapes);
    int complete_collection(Gcalc_shape_status *st);
    int collection_add_item(Gcalc_shape_status *st_collection,
                            Gcalc_shape_status *st_item);

    bool skip_point() const
    { return m_buffer_op == Gcalc_function::op_difference; }
    bool skip_line_string() const
    { return m_buffer_op == Gcalc_function::op_difference; }
    bool skip_poly() const
    { return false; }
  };
  Gcalc_heap collector;
  Gcalc_function func;

  Gcalc_result_receiver res_receiver;
  Gcalc_operation_reducer operation;
  String tmp_value;

public:
  Item_func_buffer(const POS &pos, Item *obj, Item *distance):
    Item_geometry_func(pos, obj, distance)
  {}
  const char *func_name() const { return "st_buffer"; }
  String *val_str(String *);
};


class Item_func_isempty: public Item_bool_func
{
public:
  Item_func_isempty(const POS &pos, Item *a): Item_bool_func(pos, a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "st_isempty"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_issimple: public Item_bool_func
{
  Gcalc_heap collector;
  Gcalc_function func;
  Gcalc_scan_iterator scan_it;
  String tmp;
public:
  Item_func_issimple(const POS &pos, Item *a): Item_bool_func(pos, a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "st_issimple"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_isclosed: public Item_bool_func
{
public:
  Item_func_isclosed(const POS &pos, Item *a): Item_bool_func(pos, a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "st_isclosed"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_dimension: public Item_int_func
{
  String value;
public:
  Item_func_dimension(const POS &pos, Item *a): Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "st_dimension"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};

class Item_func_x: public Item_real_func
{
  String value;
public:
  Item_func_x(const POS &pos, Item *a): Item_real_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "st_x"; }
  void fix_length_and_dec()
  {
    Item_real_func::fix_length_and_dec();
    maybe_null= 1;
  }
};


class Item_func_y: public Item_real_func
{
  String value;
public:
  Item_func_y(const POS &pos, Item *a): Item_real_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "st_y"; }
  void fix_length_and_dec()
  {
    Item_real_func::fix_length_and_dec();
    maybe_null= 1;
  }
};


class Item_func_numgeometries: public Item_int_func
{
  String value;
public:
  Item_func_numgeometries(const POS &pos, Item *a): Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "st_numgeometries"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_numinteriorring: public Item_int_func
{
  String value;
public:
  Item_func_numinteriorring(const POS &pos, Item *a): Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "st_numinteriorrings"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_numpoints: public Item_int_func
{
  String value;
public:
  Item_func_numpoints(const POS &pos, Item *a): Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "st_numpoints"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_area: public Item_real_func
{
  String value;
public:
  Item_func_area(const POS &pos, Item *a): Item_real_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "st_area"; }
  void fix_length_and_dec()
  {
    Item_real_func::fix_length_and_dec();
    maybe_null= 1;
  }
};


class Item_func_glength: public Item_real_func
{
  String value;
public:
  Item_func_glength(const POS &pos, Item *a): Item_real_func(pos, a) {}
  double val_real();
  const char *func_name() const { return "st_length"; }
  void fix_length_and_dec()
  {
    Item_real_func::fix_length_and_dec();
    maybe_null= 1;
  }
};


class Item_func_srid: public Item_int_func
{
  String value;
public:
  Item_func_srid(const POS &pos, Item *a): Item_int_func(pos, a) {}
  longlong val_int();
  const char *func_name() const { return "srid"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_distance: public Item_real_func
{
  String tmp_value1;
  String tmp_value2;
  Gcalc_heap collector;
  Gcalc_function func;
  Gcalc_scan_iterator scan_it;
public:
  Item_func_distance(const POS &pos, Item *a, Item *b): Item_real_func(pos, a, b) {}
  double val_real();
  const char *func_name() const { return "st_distance"; }
};


#ifndef DBUG_OFF
class Item_func_gis_debug: public Item_int_func
{
public:
  Item_func_gis_debug(const POS &pos, Item *a) :Item_int_func(pos, a)
  { null_value= false; }
  const char *func_name() const  { return "st_gis_debug"; }
  longlong val_int();
};
#endif

#endif /*ITEM_GEOFUNC_INCLUDED*/

#ifndef ITEM_GEOFUNC_INCLUDED
#define ITEM_GEOFUNC_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifdef HAVE_SPATIAL

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

class Item_geometry_func: public Item_str_func
{
public:
  Item_geometry_func() :Item_str_func() {}
  Item_geometry_func(Item *a) :Item_str_func(a) {}
  Item_geometry_func(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_geometry_func(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  Item_geometry_func(List<Item> &list) :Item_str_func(list) {}
  void fix_length_and_dec();
  enum_field_types field_type() const  { return MYSQL_TYPE_GEOMETRY; }
  Field *tmp_table_field(TABLE *t_arg);
  bool is_null() { (void) val_int(); return null_value; }
};

class Item_func_geometry_from_text: public Item_geometry_func
{
public:
  Item_func_geometry_from_text(Item *a) :Item_geometry_func(a) {}
  Item_func_geometry_from_text(Item *a, Item *srid) :Item_geometry_func(a, srid) {}
  const char *func_name() const { return "geometryfromtext"; }
  String *val_str(String *);
};

class Item_func_geometry_from_wkb: public Item_geometry_func
{
public:
  Item_func_geometry_from_wkb(Item *a): Item_geometry_func(a) {}
  Item_func_geometry_from_wkb(Item *a, Item *srid): Item_geometry_func(a, srid) {}
  const char *func_name() const { return "geometryfromwkb"; }
  String *val_str(String *);
};

class Item_func_as_wkt: public Item_str_ascii_func
{
public:
  Item_func_as_wkt(Item *a): Item_str_ascii_func(a) {}
  const char *func_name() const { return "astext"; }
  String *val_str_ascii(String *);
  void fix_length_and_dec();
};

class Item_func_as_wkb: public Item_geometry_func
{
public:
  Item_func_as_wkb(Item *a): Item_geometry_func(a) {}
  const char *func_name() const { return "aswkb"; }
  String *val_str(String *);
  enum_field_types field_type() const  { return MYSQL_TYPE_BLOB; }
};

class Item_func_geometry_type: public Item_str_ascii_func
{
public:
  Item_func_geometry_type(Item *a): Item_str_ascii_func(a) {}
  String *val_str_ascii(String *);
  const char *func_name() const { return "geometrytype"; }
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
  Item_func_centroid(Item *a): Item_geometry_func(a) {}
  const char *func_name() const { return "centroid"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_envelope: public Item_geometry_func
{
public:
  Item_func_envelope(Item *a): Item_geometry_func(a) {}
  const char *func_name() const { return "envelope"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_point: public Item_geometry_func
{
public:
  Item_func_point(Item *a, Item *b): Item_geometry_func(a, b) {}
  Item_func_point(Item *a, Item *b, Item *srid): Item_geometry_func(a, b, srid) {}
  const char *func_name() const { return "point"; }
  String *val_str(String *);
  Field::geometry_type get_geometry_type() const;
};

class Item_func_spatial_decomp: public Item_geometry_func
{
  enum Functype decomp_func;
public:
  Item_func_spatial_decomp(Item *a, Item_func::Functype ft) :
  	Item_geometry_func(a) { decomp_func = ft; }
  const char *func_name() const 
  { 
    switch (decomp_func)
    {
      case SP_STARTPOINT:
        return "startpoint";
      case SP_ENDPOINT:
        return "endpoint";
      case SP_EXTERIORRING:
        return "exteriorring";
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
  Item_func_spatial_decomp_n(Item *a, Item *b, Item_func::Functype ft):
  	Item_geometry_func(a, b) { decomp_func_n = ft; }
  const char *func_name() const 
  { 
    switch (decomp_func_n)
    {
      case SP_POINTN:
        return "pointn";
      case SP_GEOMETRYN:
        return "geometryn";
      case SP_INTERIORRINGN:
        return "interiorringn";
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
  Item_func_spatial_collection(
     List<Item> &list, enum Geometry::wkbType ct, enum Geometry::wkbType it):
  Item_geometry_func(list)
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
 
  const char *func_name() const { return "multipoint"; }
};

/*
  Spatial relations
*/

class Item_func_spatial_rel: public Item_bool_func2
{
  enum Functype spatial_rel;
public:
  Item_func_spatial_rel(Item *a,Item *b, enum Functype sp_rel) :
    Item_bool_func2(a,b) { spatial_rel = sp_rel; }
  longlong val_int();
  enum Functype functype() const 
  { 
    switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return SP_WITHIN_FUNC;
    case SP_WITHIN_FUNC:
      return SP_CONTAINS_FUNC;
    default:
      return spatial_rel;
    }
  }
  enum Functype rev_functype() const { return spatial_rel; }
  const char *func_name() const 
  { 
    switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "contains";
    case SP_WITHIN_FUNC:
      return "within";
    case SP_EQUALS_FUNC:
      return "equals";
    case SP_DISJOINT_FUNC:
      return "disjoint";
    case SP_INTERSECTS_FUNC:
      return "intersects";
    case SP_TOUCHES_FUNC:
      return "touches";
    case SP_CROSSES_FUNC:
      return "crosses";
    case SP_OVERLAPS_FUNC:
      return "overlaps";
    default:
      DBUG_ASSERT(0);  // Should never happened
      return "sp_unknown"; 
    }
  }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }

  void fix_length_and_dec() { maybe_null= 1; }
  bool is_null() { (void) val_int(); return null_value; }
};

class Item_func_isempty: public Item_bool_func
{
public:
  Item_func_isempty(Item *a): Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "isempty"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_issimple: public Item_bool_func
{
public:
  Item_func_issimple(Item *a): Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "issimple"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_isclosed: public Item_bool_func
{
public:
  Item_func_isclosed(Item *a): Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "isclosed"; }
  void fix_length_and_dec() { maybe_null= 1; }
};

class Item_func_dimension: public Item_int_func
{
  String value;
public:
  Item_func_dimension(Item *a): Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "dimension"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};

class Item_func_x: public Item_real_func
{
  String value;
public:
  Item_func_x(Item *a): Item_real_func(a) {}
  double val_real();
  const char *func_name() const { return "x"; }
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
  Item_func_y(Item *a): Item_real_func(a) {}
  double val_real();
  const char *func_name() const { return "y"; }
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
  Item_func_numgeometries(Item *a): Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "numgeometries"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_numinteriorring: public Item_int_func
{
  String value;
public:
  Item_func_numinteriorring(Item *a): Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "numinteriorrings"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_numpoints: public Item_int_func
{
  String value;
public:
  Item_func_numpoints(Item *a): Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "numpoints"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};


class Item_func_area: public Item_real_func
{
  String value;
public:
  Item_func_area(Item *a): Item_real_func(a) {}
  double val_real();
  const char *func_name() const { return "area"; }
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
  Item_func_glength(Item *a): Item_real_func(a) {}
  double val_real();
  const char *func_name() const { return "glength"; }
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
  Item_func_srid(Item *a): Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "srid"; }
  void fix_length_and_dec() { max_length= 10; maybe_null= 1; }
};

#define GEOM_NEW(thd, obj_constructor) new (thd->mem_root) obj_constructor

#else /*HAVE_SPATIAL*/

#define GEOM_NEW(thd, obj_constructor) NULL

#endif

#endif /* ITEM_GEOFUNC_INCLUDED */

/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
  Functions to create an item. Used by sql_yac.yy
*/

#include "item_create.h"

#include "item_cmpfunc.h"        // Item_func_any_value
#include "item_func.h"           // Item_func_udf_str
#include "item_geofunc.h"        // Item_func_area
#include "item_inetfunc.h"       // Item_func_inet_ntoa
#include "item_json_func.h"      // Item_func_json
#include "item_strfunc.h"        // Item_func_aes_encrypt
#include "item_sum.h"            // Item_sum_udf_str
#include "item_timefunc.h"       // Item_func_add_time
#include "item_xmlfunc.h"        // Item_func_xml_extractvalue
#include "parse_tree_helpers.h"  // PT_item_list
#include "sql_class.h"           // THD
#include "sql_time.h"            // str_to_datetime

/*
=============================================================================
  LOCAL DECLARATIONS
=============================================================================
*/

/**
  Adapter for native functions with a variable number of arguments.
  The main use of this class is to discard the following calls:
  <code>foo(expr1 AS name1, expr2 AS name2, ...)</code>
  which are syntactically correct (the syntax can refer to a UDF),
  but semantically invalid for native functions.
*/

class Create_native_func : public Create_func
{
public:
  virtual Item *create_func(THD *thd, LEX_STRING name, PT_item_list *item_list);

  /**
    Builder method, with no arguments.
    @param thd The current thread
    @param name The native function name
    @param item_list The function parameters, none of which are named
    @return An item representing the function call
  */
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list) = 0;

protected:
  /** Constructor. */
  Create_native_func() {}
  /** Destructor. */
  virtual ~Create_native_func() {}
};


/**
  Adapter for functions that takes exactly zero arguments.
*/

class Create_func_arg0 : public Create_func
{
public:
  virtual Item *create_func(THD *thd, LEX_STRING name, PT_item_list *item_list);

  /**
    Builder method, with no arguments.
    @param thd The current thread
    @return An item representing the function call
  */
  virtual Item *create(THD *thd) = 0;

protected:
  /** Constructor. */
  Create_func_arg0() {}
  /** Destructor. */
  virtual ~Create_func_arg0() {}
};


/**
  Adapter for functions that takes exactly one argument.
*/

class Create_func_arg1 : public Create_func
{
public:
  virtual Item *create_func(THD *thd, LEX_STRING name, PT_item_list *item_list);

  /**
    Builder method, with one argument.
    @param thd The current thread
    @param arg1 The first argument of the function
    @return An item representing the function call
  */
  virtual Item *create(THD *thd, Item *arg1) = 0;

protected:
  /** Constructor. */
  Create_func_arg1() {}
  /** Destructor. */
  virtual ~Create_func_arg1() {}
};


/**
  Adapter for functions that takes exactly two arguments.
*/

class Create_func_arg2 : public Create_func
{
public:
  virtual Item *create_func(THD *thd, LEX_STRING name, PT_item_list *item_list);

  /**
    Builder method, with two arguments.
    @param thd The current thread
    @param arg1 The first argument of the function
    @param arg2 The second argument of the function
    @return An item representing the function call
  */
  virtual Item *create(THD *thd, Item *arg1, Item *arg2) = 0;

protected:
  /** Constructor. */
  Create_func_arg2() {}
  /** Destructor. */
  virtual ~Create_func_arg2() {}
};


/**
  Adapter for functions that takes exactly three arguments.
*/

class Create_func_arg3 : public Create_func
{
public:
  virtual Item *create_func(THD *thd, LEX_STRING name, PT_item_list *item_list);

  /**
    Builder method, with three arguments.
    @param thd The current thread
    @param arg1 The first argument of the function
    @param arg2 The second argument of the function
    @param arg3 The third argument of the function
    @return An item representing the function call
  */
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3) = 0;

protected:
  /** Constructor. */
  Create_func_arg3() {}
  /** Destructor. */
  virtual ~Create_func_arg3() {}
};


/**
  Function builder for Stored Functions.
*/

class Create_sp_func : public Create_qfunc
{
public:
  virtual Item *create(THD *thd, LEX_STRING db, LEX_STRING name,
                       bool use_explicit_name, PT_item_list *item_list);

  static Create_sp_func s_singleton;

protected:
  /** Constructor. */
  Create_sp_func() {}
  /** Destructor. */
  virtual ~Create_sp_func() {}
};




/*
  Concrete functions builders (native functions).
  Please keep this list sorted in alphabetical order,
  it helps to compare code between versions, and helps with merges conflicts.
*/

class Create_func_abs : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_abs s_singleton;

protected:
  Create_func_abs() {}
  virtual ~Create_func_abs() {}
};


class Create_func_acos : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_acos s_singleton;

protected:
  Create_func_acos() {}
  virtual ~Create_func_acos() {}
};


class Create_func_addtime : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_addtime s_singleton;

protected:
  Create_func_addtime() {}
  virtual ~Create_func_addtime() {}
};

class Create_func_aes_base : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    Item *func= NULL, *p1, *p2, *p3;
    int arg_count= 0;

    if (item_list != NULL)
      arg_count= item_list->elements();

    switch (arg_count)
    {
    case 2:
      {
        p1= item_list->pop_front();
        p2= item_list->pop_front();
        func= create_aes(thd, p1, p2);
        break;
      }
    case 3:
      {
        p1= item_list->pop_front();
        p2= item_list->pop_front();
        p3= item_list->pop_front();
        func= create_aes(thd, p1, p2, p3);
        break;
      }
    default:
      {
        my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
        break;
      }
    }
    return func;

  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2)= 0;
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3)= 0;
protected:
  Create_func_aes_base()
  {}
  virtual ~Create_func_aes_base()
  {}

};


class Create_func_aes_encrypt : public Create_func_aes_base
{
public:
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2)
  {
    return new (thd->mem_root) Item_func_aes_encrypt(POS(), arg1, arg2);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3)
  {
    return new (thd->mem_root) Item_func_aes_encrypt(POS(), arg1, arg2, arg3);
  }

  static Create_func_aes_encrypt s_singleton;

protected:
  Create_func_aes_encrypt() {}
  virtual ~Create_func_aes_encrypt() {}
};


class Create_func_aes_decrypt : public Create_func_aes_base
{
public:
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2)
  {
    return new (thd->mem_root) Item_func_aes_decrypt(POS(), arg1, arg2);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3)
  {
    return new (thd->mem_root) Item_func_aes_decrypt(POS(), arg1, arg2, arg3);
  }

  static Create_func_aes_decrypt s_singleton;

protected:
  Create_func_aes_decrypt() {}
  virtual ~Create_func_aes_decrypt() {}
};


class Create_func_random_bytes : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    return new (thd->mem_root) Item_func_random_bytes(POS(), arg1);
  }
  static Create_func_random_bytes s_singleton;

protected:
  Create_func_random_bytes()
  {}
  virtual ~Create_func_random_bytes()
  {}
};


class Create_func_any_value : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  { return new (thd->mem_root) Item_func_any_value(POS(), arg1); }

  static Create_func_any_value s_singleton;

protected:
  Create_func_any_value() {}
  virtual ~Create_func_any_value() {}
};


class Create_func_area : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_area s_singleton;

protected:
  Create_func_area() {}
  virtual ~Create_func_area() {}
};


class Create_func_area_deprecated : public Create_func_area
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "AREA", "ST_AREA");
    return Create_func_area::create(thd, arg1);
  }

  static Create_func_area_deprecated s_singleton;
};
Create_func_area_deprecated Create_func_area_deprecated::s_singleton;


class Create_func_as_geojson : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_as_geojson s_singleton;

protected:
  Create_func_as_geojson() {}
  virtual ~Create_func_as_geojson() {}
};


class Create_func_as_wkb : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_as_wkb s_singleton;

protected:
  Create_func_as_wkb() {}
  virtual ~Create_func_as_wkb() {}
};


class Create_func_as_binary_deprecated : public Create_func_as_wkb
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ASBINARY", "ST_ASBINARY");
    return Create_func_as_wkb::create(thd, arg1);
  }

  static Create_func_as_binary_deprecated s_singleton;
};
Create_func_as_binary_deprecated Create_func_as_binary_deprecated::s_singleton;


class Create_func_as_wkb_deprecated : public Create_func_as_wkb
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ASWKB", "ST_ASWKB");
    return Create_func_as_wkb::create(thd, arg1);
  }

  static Create_func_as_wkb_deprecated s_singleton;
};
Create_func_as_wkb_deprecated Create_func_as_wkb_deprecated::s_singleton;


class Create_func_as_wkt : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_as_wkt s_singleton;

protected:
  Create_func_as_wkt() {}
  virtual ~Create_func_as_wkt() {}
};


class Create_func_as_text_deprecated : public Create_func_as_wkt
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ASTEXT", "ST_ASTEXT");
    return Create_func_as_wkt::create(thd, arg1);
  }

  static Create_func_as_text_deprecated s_singleton;
};
Create_func_as_text_deprecated Create_func_as_text_deprecated::s_singleton;


class Create_func_as_wkt_deprecated : public Create_func_as_wkt
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ASWKT", "ST_ASWKT");
    return Create_func_as_wkt::create(thd, arg1);
  }

  static Create_func_as_wkt_deprecated s_singleton;
};
Create_func_as_wkt_deprecated Create_func_as_wkt_deprecated::s_singleton;


class Create_func_asin : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_asin s_singleton;

protected:
  Create_func_asin() {}
  virtual ~Create_func_asin() {}
};


class Create_func_atan : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_atan s_singleton;

protected:
  Create_func_atan() {}
  virtual ~Create_func_atan() {}
};


class Create_func_benchmark : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_benchmark s_singleton;

protected:
  Create_func_benchmark() {}
  virtual ~Create_func_benchmark() {}
};


class Create_func_bin : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_bin s_singleton;

protected:
  Create_func_bin() {}
  virtual ~Create_func_bin() {}
};


class Create_func_bit_count : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_bit_count s_singleton;

protected:
  Create_func_bit_count() {}
  virtual ~Create_func_bit_count() {}
};


class Create_func_bit_length : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_bit_length s_singleton;

protected:
  Create_func_bit_length() {}
  virtual ~Create_func_bit_length() {}
};


class Create_func_buffer_strategy : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_buffer_strategy s_singleton;

protected:
  Create_func_buffer_strategy() {}
  virtual ~Create_func_buffer_strategy() {}
};


class Create_func_ceiling : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_ceiling s_singleton;

protected:
  Create_func_ceiling() {}
  virtual ~Create_func_ceiling() {}
};


class Create_func_centroid : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_centroid s_singleton;

protected:
  Create_func_centroid() {}
  virtual ~Create_func_centroid() {}
};


class Create_func_centroid_deprecated : public Create_func_centroid
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "CENTROID", "ST_CENTROID");
    return Create_func_centroid::create(thd, arg1);
  }

  static Create_func_centroid_deprecated s_singleton;
};
Create_func_centroid_deprecated Create_func_centroid_deprecated::s_singleton;


class Create_func_char_length : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_char_length s_singleton;

protected:
  Create_func_char_length() {}
  virtual ~Create_func_char_length() {}
};


class Create_func_coercibility : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_coercibility s_singleton;

protected:
  Create_func_coercibility() {}
  virtual ~Create_func_coercibility() {}
};


class Create_func_compress : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_compress s_singleton;

protected:
  Create_func_compress() {}
  virtual ~Create_func_compress() {}
};


class Create_func_concat : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_concat s_singleton;

protected:
  Create_func_concat() {}
  virtual ~Create_func_concat() {}
};


class Create_func_concat_ws : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_concat_ws s_singleton;

protected:
  Create_func_concat_ws() {}
  virtual ~Create_func_concat_ws() {}
};


class Create_func_connection_id : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_connection_id s_singleton;

protected:
  Create_func_connection_id() {}
  virtual ~Create_func_connection_id() {}
};


class Create_func_convex_hull : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_convex_hull s_singleton;

protected:
  Create_func_convex_hull() {}
  virtual ~Create_func_convex_hull() {}
};


class Create_func_mbr_covered_by : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_covered_by s_singleton;

protected:
  Create_func_mbr_covered_by() {}
  virtual ~Create_func_mbr_covered_by() {}
};


class Create_func_mbr_covers : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_covers s_singleton;

protected:
  Create_func_mbr_covers() {}
  virtual ~Create_func_mbr_covers() {}
};


class Create_func_convex_hull_deprecated : public Create_func_convex_hull
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "CONVEXHULL", "ST_CONVEXHULL");
    return Create_func_convex_hull::create(thd, arg1);
  }

  static Create_func_convex_hull_deprecated s_singleton;
};
Create_func_convex_hull_deprecated Create_func_convex_hull_deprecated::s_singleton;


class Create_func_mbr_contains : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_contains s_singleton;

protected:
  Create_func_mbr_contains() {}
  virtual ~Create_func_mbr_contains() {}
};


class Create_func_contains : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_contains s_singleton;

protected:
  Create_func_contains() {}
  virtual ~Create_func_contains() {}
};


class Create_func_conv : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_conv s_singleton;

protected:
  Create_func_conv() {}
  virtual ~Create_func_conv() {}
};


class Create_func_convert_tz : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_convert_tz s_singleton;

protected:
  Create_func_convert_tz() {}
  virtual ~Create_func_convert_tz() {}
};


class Create_func_cos : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_cos s_singleton;

protected:
  Create_func_cos() {}
  virtual ~Create_func_cos() {}
};


class Create_func_cot : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_cot s_singleton;

protected:
  Create_func_cot() {}
  virtual ~Create_func_cot() {}
};


class Create_func_crc32 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_crc32 s_singleton;

protected:
  Create_func_crc32() {}
  virtual ~Create_func_crc32() {}
};


class Create_func_crosses : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_crosses s_singleton;

protected:
  Create_func_crosses() {}
  virtual ~Create_func_crosses() {}
};


class Create_func_crosses_deprecated : public Create_func_crosses
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "CROSSES", "ST_CROSSES");
    return Create_func_crosses::create(thd, arg1, arg2);
  }

  static Create_func_crosses_deprecated s_singleton;
};
Create_func_crosses_deprecated Create_func_crosses_deprecated::s_singleton;


class Create_func_date_format : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_date_format s_singleton;

protected:
  Create_func_date_format() {}
  virtual ~Create_func_date_format() {}
};


class Create_func_datediff : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_datediff s_singleton;

protected:
  Create_func_datediff() {}
  virtual ~Create_func_datediff() {}
};


class Create_func_dayname : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_dayname s_singleton;

protected:
  Create_func_dayname() {}
  virtual ~Create_func_dayname() {}
};


class Create_func_dayofmonth : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_dayofmonth s_singleton;

protected:
  Create_func_dayofmonth() {}
  virtual ~Create_func_dayofmonth() {}
};


class Create_func_dayofweek : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_dayofweek s_singleton;

protected:
  Create_func_dayofweek() {}
  virtual ~Create_func_dayofweek() {}
};


class Create_func_dayofyear : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_dayofyear s_singleton;

protected:
  Create_func_dayofyear() {}
  virtual ~Create_func_dayofyear() {}
};


class Create_func_decode : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_decode s_singleton;

protected:
  Create_func_decode() {}
  virtual ~Create_func_decode() {}
};


class Create_func_degrees : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_degrees s_singleton;

protected:
  Create_func_degrees() {}
  virtual ~Create_func_degrees() {}
};


class Create_func_des_decrypt : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_des_decrypt s_singleton;

protected:
  Create_func_des_decrypt() {}
  virtual ~Create_func_des_decrypt() {}
};


class Create_func_des_encrypt : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_des_encrypt s_singleton;

protected:
  Create_func_des_encrypt() {}
  virtual ~Create_func_des_encrypt() {}
};


class Create_func_dimension : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_dimension s_singleton;

protected:
  Create_func_dimension() {}
  virtual ~Create_func_dimension() {}
};


class Create_func_dimension_deprecated : public Create_func_dimension
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(current_thd, "DIMENSION", "ST_DIMENSION");
    return Create_func_dimension::create(thd, arg1);
  }

  static Create_func_dimension_deprecated s_singleton;
};
Create_func_dimension_deprecated Create_func_dimension_deprecated::s_singleton;


class Create_func_mbr_disjoint : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_disjoint s_singleton;

protected:
  Create_func_mbr_disjoint() {}
  virtual ~Create_func_mbr_disjoint() {}
};


class Create_func_disjoint : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_disjoint s_singleton;

protected:
  Create_func_disjoint() {}
  virtual ~Create_func_disjoint() {}
};


class Create_func_distance : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_distance s_singleton;

protected:
  Create_func_distance() {}
  virtual ~Create_func_distance() {}
};


class Create_func_distance_sphere : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_distance_sphere s_singleton;

protected:
  Create_func_distance_sphere() {}
  virtual ~Create_func_distance_sphere() {}
};


class Create_func_distance_deprecated : public Create_func_distance
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "DISTANCE", "ST_DISTANCE");
    return Create_func_distance::create_native(thd, name, item_list);
  }

  static Create_func_distance_deprecated s_singleton;
};
Create_func_distance_deprecated Create_func_distance_deprecated::s_singleton;


class Create_func_elt : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_elt s_singleton;

protected:
  Create_func_elt() {}
  virtual ~Create_func_elt() {}
};


class Create_func_encode : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_encode s_singleton;

protected:
  Create_func_encode() {}
  virtual ~Create_func_encode() {}
};


class Create_func_encrypt : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_encrypt s_singleton;

protected:
  Create_func_encrypt() {}
  virtual ~Create_func_encrypt() {}
};


class Create_func_endpoint : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_endpoint s_singleton;

protected:
  Create_func_endpoint() {}
  virtual ~Create_func_endpoint() {}
};


class Create_func_endpoint_deprecated : public Create_func_endpoint
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ENDPOINT", "ST_ENDPOINT");
    return Create_func_endpoint::create(thd, arg1);
  }

  static Create_func_endpoint_deprecated s_singleton;
};
Create_func_endpoint_deprecated Create_func_endpoint_deprecated::s_singleton;


class Create_func_envelope : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_envelope s_singleton;

protected:
  Create_func_envelope() {}
  virtual ~Create_func_envelope() {}
};


class Create_func_envelope_deprecated : public Create_func_envelope
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ENVELOPE", "ST_ENVELOPE");
    return Create_func_envelope::create(thd, arg1);
  }

  static Create_func_envelope_deprecated s_singleton;
};
Create_func_envelope_deprecated Create_func_envelope_deprecated::s_singleton;


class Create_func_mbr_equals : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_equals s_singleton;

protected:
  Create_func_mbr_equals() {}
  virtual ~Create_func_mbr_equals() {}
};


class Create_func_mbr_equal_deprecated : public Create_func_mbr_equals
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "MBREQUAL", "MBREQUALS");
    return Create_func_mbr_equals::create(thd, arg1, arg2);
  }

  static Create_func_mbr_equal_deprecated s_singleton;
};
Create_func_mbr_equal_deprecated Create_func_mbr_equal_deprecated::s_singleton;


class Create_func_equals_deprecated : public Create_func_mbr_equals
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "EQUALS", "MBREQUALS");
    return Create_func_mbr_equals::create(thd, arg1, arg2);
  }

  static Create_func_equals_deprecated s_singleton;
};
Create_func_equals_deprecated Create_func_equals_deprecated::s_singleton;


class Create_func_equals : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_equals s_singleton;

protected:
  Create_func_equals() {}
  virtual ~Create_func_equals() {}
};


class Create_func_exp : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_exp s_singleton;

protected:
  Create_func_exp() {}
  virtual ~Create_func_exp() {}
};


class Create_func_export_set : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_export_set s_singleton;

protected:
  Create_func_export_set() {}
  virtual ~Create_func_export_set() {}
};


class Create_func_exteriorring : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_exteriorring s_singleton;

protected:
  Create_func_exteriorring() {}
  virtual ~Create_func_exteriorring() {}
};


class Create_func_exteriorring_deprecated : public Create_func_exteriorring
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "EXTERIORRING", "ST_EXTERIORRING");
    return Create_func_exteriorring::create(thd, arg1);
  }

  static Create_func_exteriorring_deprecated s_singleton;
};
Create_func_exteriorring_deprecated Create_func_exteriorring_deprecated::s_singleton;


class Create_func_field : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_field s_singleton;

protected:
  Create_func_field() {}
  virtual ~Create_func_field() {}
};


class Create_func_find_in_set : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_find_in_set s_singleton;

protected:
  Create_func_find_in_set() {}
  virtual ~Create_func_find_in_set() {}
};


class Create_func_floor : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_floor s_singleton;

protected:
  Create_func_floor() {}
  virtual ~Create_func_floor() {}
};


class Create_func_found_rows : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_found_rows s_singleton;

protected:
  Create_func_found_rows() {}
  virtual ~Create_func_found_rows() {}
};


class Create_func_from_base64 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_from_base64 s_singleton;

protected:
  Create_func_from_base64() {}
  virtual ~Create_func_from_base64() {}
};


class Create_func_from_days : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_from_days s_singleton;

protected:
  Create_func_from_days() {}
  virtual ~Create_func_from_days() {}
};


class Create_func_from_unixtime : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_from_unixtime s_singleton;

protected:
  Create_func_from_unixtime() {}
  virtual ~Create_func_from_unixtime() {}
};


class Create_func_geohash : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_geohash s_singleton;

protected:
  Create_func_geohash() {}
  virtual ~Create_func_geohash() {}
};


class Create_func_geometry_from_text : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_geometry_from_text s_singleton;

protected:
  Create_func_geometry_from_text() {}
  virtual ~Create_func_geometry_from_text() {}
};


class Create_func_geomcollfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMCOLLFROMTEXT", "ST_GEOMCOLLFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_geomcollfromtext_deprecated s_singleton;
};
Create_func_geomcollfromtext_deprecated Create_func_geomcollfromtext_deprecated::s_singleton;


class Create_func_geometrycollectionfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMETRYCOLLECTIONFROMTEXT", "ST_GEOMETRYCOLLECTIONFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_geometrycollectionfromtext_deprecated s_singleton;
};
Create_func_geometrycollectionfromtext_deprecated Create_func_geometrycollectionfromtext_deprecated::s_singleton;


class Create_func_geometryfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMETRYFROMTEXT", "ST_GEOMETRYFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_geometryfromtext_deprecated s_singleton;
};
Create_func_geometryfromtext_deprecated Create_func_geometryfromtext_deprecated::s_singleton;


class Create_func_geomfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMFROMTEXT", "ST_GEOMFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_geomfromtext_deprecated s_singleton;
};
Create_func_geomfromtext_deprecated Create_func_geomfromtext_deprecated::s_singleton;


class Create_func_linefromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "LINEFROMTEXT", "ST_LINEFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_linefromtext_deprecated s_singleton;
};
Create_func_linefromtext_deprecated Create_func_linefromtext_deprecated::s_singleton;


class Create_func_linestringfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "LINESTRINGFROMTEXT", "ST_LINESTRINGFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_linestringfromtext_deprecated s_singleton;
};
Create_func_linestringfromtext_deprecated Create_func_linestringfromtext_deprecated::s_singleton;


class Create_func_mlinefromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MLINEFROMTEXT", "ST_MLINEFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_mlinefromtext_deprecated s_singleton;
};
Create_func_mlinefromtext_deprecated Create_func_mlinefromtext_deprecated::s_singleton;


class Create_func_mpointfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MPOINTFROMTEXT", "ST_MPOINTFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_mpointfromtext_deprecated s_singleton;
};
Create_func_mpointfromtext_deprecated Create_func_mpointfromtext_deprecated::s_singleton;


class Create_func_mpolyfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MPOLYFROMTEXT", "ST_MPOLYFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_mpolyfromtext_deprecated s_singleton;
};
Create_func_mpolyfromtext_deprecated Create_func_mpolyfromtext_deprecated::s_singleton;


class Create_func_multilinestringfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTILINESTRINGFROMTEXT", "ST_MULTILINESTRINGFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_multilinestringfromtext_deprecated s_singleton;
};
Create_func_multilinestringfromtext_deprecated Create_func_multilinestringfromtext_deprecated::s_singleton;


class Create_func_multipointfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTIPOINTFROMTEXT", "ST_MULTIPOINTFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_multipointfromtext_deprecated s_singleton;
};
Create_func_multipointfromtext_deprecated Create_func_multipointfromtext_deprecated::s_singleton;


class Create_func_multipolygonfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTIPOLYGONFROMTEXT", "ST_MULTIPOLYGONFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_multipolygonfromtext_deprecated s_singleton;
};
Create_func_multipolygonfromtext_deprecated Create_func_multipolygonfromtext_deprecated::s_singleton;


class Create_func_pointfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POINTFROMTEXT", "ST_POINTFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_pointfromtext_deprecated s_singleton;
};
Create_func_pointfromtext_deprecated Create_func_pointfromtext_deprecated::s_singleton;


class Create_func_polyfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POLYFROMTEXT", "ST_POLYFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_polyfromtext_deprecated s_singleton;
};
Create_func_polyfromtext_deprecated Create_func_polyfromtext_deprecated::s_singleton;


class Create_func_polygonfromtext_deprecated : public Create_func_geometry_from_text
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POLYGONFROMTEXT", "ST_POLYGONFROMTEXT");
    return Create_func_geometry_from_text::create_native(thd, name, item_list);
  }

  static Create_func_polygonfromtext_deprecated s_singleton;
};
Create_func_polygonfromtext_deprecated Create_func_polygonfromtext_deprecated::s_singleton;


class Create_func_geometry_from_wkb : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_geometry_from_wkb s_singleton;

protected:
  Create_func_geometry_from_wkb() {}
  virtual ~Create_func_geometry_from_wkb() {}
};


class Create_func_geomcollfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMCOLLFROMWKB", "ST_GEOMCOLLFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_geomcollfromwkb_deprecated s_singleton;
};
Create_func_geomcollfromwkb_deprecated Create_func_geomcollfromwkb_deprecated::s_singleton;


class Create_func_geometrycollectionfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMETRYCOLLECTIONFROMWKB", "ST_GEOMETRYCOLLECTIONFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_geometrycollectionfromwkb_deprecated s_singleton;
};
Create_func_geometrycollectionfromwkb_deprecated Create_func_geometrycollectionfromwkb_deprecated::s_singleton;


class Create_func_geometryfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMETRYFROMWKB", "ST_GEOMETRYFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_geometryfromwkb_deprecated s_singleton;
};
Create_func_geometryfromwkb_deprecated Create_func_geometryfromwkb_deprecated::s_singleton;


class Create_func_geomfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "GEOMFROMWKB", "ST_GEOMFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_geomfromwkb_deprecated s_singleton;
};
Create_func_geomfromwkb_deprecated Create_func_geomfromwkb_deprecated::s_singleton;


class Create_func_linefromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "LINEFROMWKB", "ST_LINEFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_linefromwkb_deprecated s_singleton;
};
Create_func_linefromwkb_deprecated Create_func_linefromwkb_deprecated::s_singleton;


class Create_func_linestringfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "LINESTRINGFROMWKB", "ST_LINESTRINGFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_linestringfromwkb_deprecated s_singleton;
};
Create_func_linestringfromwkb_deprecated Create_func_linestringfromwkb_deprecated::s_singleton;


class Create_func_mlinefromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MLINEFROMWKB", "ST_MLINEFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_mlinefromwkb_deprecated s_singleton;
};
Create_func_mlinefromwkb_deprecated Create_func_mlinefromwkb_deprecated::s_singleton;


class Create_func_mpointfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MPOINTFROMWKB", "ST_MPOINTFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_mpointfromwkb_deprecated s_singleton;
};
Create_func_mpointfromwkb_deprecated Create_func_mpointfromwkb_deprecated::s_singleton;


class Create_func_mpolyfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MPOLYFROMWKB", "ST_MPOLYFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_mpolyfromwkb_deprecated s_singleton;
};
Create_func_mpolyfromwkb_deprecated Create_func_mpolyfromwkb_deprecated::s_singleton;


class Create_func_multilinestringfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTILINESTRINGFROMWKB", "ST_MULTILINESTRINGFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_multilinestringfromwkb_deprecated s_singleton;
};
Create_func_multilinestringfromwkb_deprecated Create_func_multilinestringfromwkb_deprecated::s_singleton;


class Create_func_multipointfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTIPOINTFROMWKB", "ST_MULTIPOINTFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_multipointfromwkb_deprecated s_singleton;
};
Create_func_multipointfromwkb_deprecated Create_func_multipointfromwkb_deprecated::s_singleton;


class Create_func_multipolygonfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "MULTIPOLYGONFROMWKB", "ST_MULTIPOLYGONFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_multipolygonfromwkb_deprecated s_singleton;
};
Create_func_multipolygonfromwkb_deprecated Create_func_multipolygonfromwkb_deprecated::s_singleton;


class Create_func_pointfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POINTFROMWKB", "ST_POINTFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_pointfromwkb_deprecated s_singleton;
};
Create_func_pointfromwkb_deprecated Create_func_pointfromwkb_deprecated::s_singleton;


class Create_func_polyfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POLYFROMWKB", "ST_POLYFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_polyfromwkb_deprecated s_singleton;
};
Create_func_polyfromwkb_deprecated Create_func_polyfromwkb_deprecated::s_singleton;


class Create_func_polygonfromwkb_deprecated : public Create_func_geometry_from_wkb
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(thd, "POLYGONFROMWKB", "ST_POLYGONFROMWKB");
    return Create_func_geometry_from_wkb::create_native(thd, name, item_list);
  }

  static Create_func_polygonfromwkb_deprecated s_singleton;
};
Create_func_polygonfromwkb_deprecated Create_func_polygonfromwkb_deprecated::s_singleton;


class Create_func_geometry_type : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_geometry_type s_singleton;

protected:
  Create_func_geometry_type() {}
  virtual ~Create_func_geometry_type() {}
};


class Create_func_geometry_type_deprecated : public Create_func_geometry_type
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "GEOMETRYTYPE", "ST_GEOMETRYTYPE");
    return Create_func_geometry_type::create(thd, arg1);
  }

  static Create_func_geometry_type_deprecated s_singleton;
};
Create_func_geometry_type_deprecated Create_func_geometry_type_deprecated::s_singleton;


class Create_func_geometryn : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_geometryn s_singleton;

protected:
  Create_func_geometryn() {}
  virtual ~Create_func_geometryn() {}
};


class Create_func_geometryn_deprecated : public Create_func_geometryn
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "GEOMETRYN", "ST_GEOMETRYN");
    return Create_func_geometryn::create(thd, arg1, arg2);
  }

  static Create_func_geometryn_deprecated s_singleton;
};
Create_func_geometryn_deprecated Create_func_geometryn_deprecated::s_singleton;


class Create_func_geomfromgeojson : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_geomfromgeojson s_singleton;

protected:
  Create_func_geomfromgeojson() {}
  virtual ~Create_func_geomfromgeojson() {}
};


class Create_func_get_lock : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_get_lock s_singleton;

protected:
  Create_func_get_lock() {}
  virtual ~Create_func_get_lock() {}
};


class Create_func_glength : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_glength s_singleton;

protected:
  Create_func_glength() {}
  virtual ~Create_func_glength() {}
};


class Create_func_glength_deprecated : public Create_func_glength
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "GLENGTH", "ST_LENGTH");
    return Create_func_glength::create(thd, arg1);
  }

  static Create_func_glength_deprecated s_singleton;
};
Create_func_glength_deprecated Create_func_glength_deprecated::s_singleton;


class Create_func_greatest : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_greatest s_singleton;

protected:
  Create_func_greatest() {}
  virtual ~Create_func_greatest() {}
};


class Create_func_gtid_subtract : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_gtid_subtract s_singleton;

protected:
  Create_func_gtid_subtract() {}
  virtual ~Create_func_gtid_subtract() {}
};


class Create_func_gtid_subset : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_gtid_subset s_singleton;

protected:
  Create_func_gtid_subset() {}
  virtual ~Create_func_gtid_subset() {}
};


class Create_func_hex : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_hex s_singleton;

protected:
  Create_func_hex() {}
  virtual ~Create_func_hex() {}
};


class Create_func_ifnull : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_ifnull s_singleton;

protected:
  Create_func_ifnull() {}
  virtual ~Create_func_ifnull() {}
};


class Create_func_inet_ntoa : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_inet_ntoa s_singleton;

protected:
  Create_func_inet_ntoa() {}
  virtual ~Create_func_inet_ntoa() {}
};


class Create_func_inet_aton : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_inet_aton s_singleton;

protected:
  Create_func_inet_aton() {}
  virtual ~Create_func_inet_aton() {}
};


class Create_func_inet6_aton : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_inet6_aton s_singleton;

protected:
  Create_func_inet6_aton() {}
  virtual ~Create_func_inet6_aton() {}
};


class Create_func_inet6_ntoa : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_inet6_ntoa s_singleton;

protected:
  Create_func_inet6_ntoa() {}
  virtual ~Create_func_inet6_ntoa() {}
};


class Create_func_is_ipv4 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_ipv4 s_singleton;

protected:
  Create_func_is_ipv4() {}
  virtual ~Create_func_is_ipv4() {}
};


class Create_func_is_ipv6 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_ipv6 s_singleton;

protected:
  Create_func_is_ipv6() {}
  virtual ~Create_func_is_ipv6() {}
};


class Create_func_is_ipv4_compat : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_ipv4_compat s_singleton;

protected:
  Create_func_is_ipv4_compat() {}
  virtual ~Create_func_is_ipv4_compat() {}
};


class Create_func_is_ipv4_mapped : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_ipv4_mapped s_singleton;

protected:
  Create_func_is_ipv4_mapped() {}
  virtual ~Create_func_is_ipv4_mapped() {}
};


class Create_func_instr : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_instr s_singleton;

protected:
  Create_func_instr() {}
  virtual ~Create_func_instr() {}
};


class Create_func_interiorringn : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_interiorringn s_singleton;

protected:
  Create_func_interiorringn() {}
  virtual ~Create_func_interiorringn() {}
};


class Create_func_interiorringn_deprecated : public Create_func_interiorringn
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "INTERIORRINGN", "ST_INTERIORRINGN");
    return Create_func_interiorringn::create(thd, arg1, arg2);
  }

  static Create_func_interiorringn_deprecated s_singleton;
};
Create_func_interiorringn_deprecated Create_func_interiorringn_deprecated::s_singleton;


class Create_func_mbr_intersects : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_intersects s_singleton;

protected:
  Create_func_mbr_intersects() {}
  virtual ~Create_func_mbr_intersects() {}
};


class Create_func_intersects_deprecated : public Create_func_mbr_intersects
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "INTERSECTS", "MBRINTERSECTS");
    return Create_func_mbr_intersects::create(thd, arg1, arg2);
  }

  static Create_func_intersects_deprecated s_singleton;
};
Create_func_intersects_deprecated Create_func_intersects_deprecated::s_singleton;


class Create_func_intersects : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_intersects s_singleton;

protected:
  Create_func_intersects() {}
  virtual ~Create_func_intersects() {}
};


class Create_func_intersection : public Create_func_arg2
{
public:
  virtual Item* create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_intersection s_singleton;

protected:
  Create_func_intersection() {}
  virtual ~Create_func_intersection() {}
};


class Create_func_difference : public Create_func_arg2
{
public:
  virtual Item* create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_difference s_singleton;

protected:
  Create_func_difference() {}
  virtual ~Create_func_difference() {}
};


class Create_func_union : public Create_func_arg2
{
public:
  virtual Item* create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_union s_singleton;

protected:
  Create_func_union() {}
  virtual ~Create_func_union() {}
};


class Create_func_symdifference : public Create_func_arg2
{
public:
  virtual Item* create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_symdifference s_singleton;

protected:
  Create_func_symdifference() {}
  virtual ~Create_func_symdifference() {}
};


class Create_func_buffer : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_buffer s_singleton;

protected:
  Create_func_buffer() {}
  virtual ~Create_func_buffer() {}
};


class Create_func_buffer_deprecated : public Create_func_buffer
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    push_deprecated_warn(current_thd, "BUFFER", "ST_BUFFER");
    return Create_func_buffer::create_native(thd, name, item_list);
  }

  static Create_func_buffer_deprecated s_singleton;
};
Create_func_buffer_deprecated Create_func_buffer_deprecated::s_singleton;


class Create_func_is_free_lock : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_free_lock s_singleton;

protected:
  Create_func_is_free_lock() {}
  virtual ~Create_func_is_free_lock() {}
};


class Create_func_is_used_lock : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_is_used_lock s_singleton;

protected:
  Create_func_is_used_lock() {}
  virtual ~Create_func_is_used_lock() {}
};


class Create_func_isclosed : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_isclosed s_singleton;

protected:
  Create_func_isclosed() {}
  virtual ~Create_func_isclosed() {}
};


class Create_func_isclosed_deprecated : public Create_func_isclosed
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ISCLOSED", "ST_ISCLOSED");
    return Create_func_isclosed::create(thd, arg1);
  }

  static Create_func_isclosed_deprecated s_singleton;
};
Create_func_isclosed_deprecated Create_func_isclosed_deprecated::s_singleton;


class Create_func_isempty : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_isempty s_singleton;

protected:
  Create_func_isempty() {}
  virtual ~Create_func_isempty() {}
};


class Create_func_isempty_deprecated : public Create_func_isempty
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ISEMPTY", "ST_ISEMPTY");
    return Create_func_isempty::create(thd, arg1);
  }

  static Create_func_isempty_deprecated s_singleton;
};
Create_func_isempty_deprecated Create_func_isempty_deprecated::s_singleton;


class Create_func_isnull : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_isnull s_singleton;

protected:
  Create_func_isnull() {}
  virtual ~Create_func_isnull() {}
};


class Create_func_issimple : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_issimple s_singleton;

protected:
  Create_func_issimple() {}
  virtual ~Create_func_issimple() {}
};

class Create_func_json_valid : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_json_valid s_singleton;
protected:
  Create_func_json_valid() {}
  virtual ~Create_func_json_valid() {}
};

class Create_func_json_contains : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_json_contains s_singleton;
protected:
  Create_func_json_contains() {}
  virtual ~Create_func_json_contains() {}
};

class Create_func_json_contains_path : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_json_contains_path s_singleton;
protected:
  Create_func_json_contains_path() {}
  virtual ~Create_func_json_contains_path() {}
};

class Create_func_json_length : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_json_length s_singleton;

protected:
  Create_func_json_length() {}
  virtual ~Create_func_json_length() {}
};

class Create_func_json_depth : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name, PT_item_list *item_list);

  static Create_func_json_depth s_singleton;

protected:
  Create_func_json_depth() {}
  virtual ~Create_func_json_depth() {}
};

class Create_func_json_pretty : public Create_func_arg1
{
public:
  static Create_func_json_pretty s_singleton;
  virtual Item *create(THD *thd, Item *arg1)
  {
    return new (thd->mem_root) Item_func_json_pretty(POS(), arg1);
  }
};
Create_func_json_pretty Create_func_json_pretty::s_singleton;

class Create_func_json_type : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_json_type s_singleton;
protected:
  Create_func_json_type() {}
  virtual ~Create_func_json_type() {}
};

class Create_func_json_keys : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_keys s_singleton;
protected:
  Create_func_json_keys() {}
  virtual ~Create_func_json_keys() {}
};

class Create_func_json_extract : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_extract s_singleton;
protected:
  Create_func_json_extract() {}
  virtual ~Create_func_json_extract() {}
};

class Create_func_json_array_append : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_array_append s_singleton;

protected:
  Create_func_json_array_append() {}
  virtual ~Create_func_json_array_append() {}

};

class Create_func_json_insert : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_insert s_singleton;

protected:
  Create_func_json_insert() {}
  virtual ~Create_func_json_insert() {}

};

class Create_func_json_array_insert : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_array_insert s_singleton;

protected:
  Create_func_json_array_insert() {}
  virtual ~Create_func_json_array_insert() {}

};

class Create_func_json_row_object : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_row_object s_singleton;

protected:
  Create_func_json_row_object() {}
  virtual ~Create_func_json_row_object() {}

};

class Create_func_json_search : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_search s_singleton;

protected:
  Create_func_json_search() {}
  virtual ~Create_func_json_search() {}

};

class Create_func_json_set : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_set s_singleton;

protected:
  Create_func_json_set() {}
  virtual ~Create_func_json_set() {}

};

class Create_func_json_replace : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_replace s_singleton;

protected:
  Create_func_json_replace() {}
  virtual ~Create_func_json_replace() {}

};

class Create_func_json_array : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_array s_singleton;

protected:
  Create_func_json_array() {}
  virtual ~Create_func_json_array() {}

};

class Create_func_json_remove : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_remove s_singleton;

protected:
  Create_func_json_remove() {}
  virtual ~Create_func_json_remove() {}
};

class Create_func_isvalid : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_isvalid s_singleton;

protected:
  Create_func_isvalid() {}
  virtual ~Create_func_isvalid() {}
};


class Create_func_validate : public Create_func_arg1
{
public:
  virtual Item* create(THD *thd, Item *arg1);

  static Create_func_validate s_singleton;

protected:
  Create_func_validate() {}
  virtual ~Create_func_validate() {}
};


class Create_func_issimple_deprecated : public Create_func_issimple
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "ISSIMPLE", "ST_ISSIMPLE");
    return Create_func_issimple::create(thd, arg1);
  }

  static Create_func_issimple_deprecated s_singleton;
};
Create_func_issimple_deprecated Create_func_issimple_deprecated::s_singleton;

class Create_func_json_merge_patch : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_merge_patch s_singleton;
};
Create_func_json_merge_patch Create_func_json_merge_patch::s_singleton;

class Create_func_json_merge_preserve : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_merge_preserve s_singleton;

protected:
  Create_func_json_merge_preserve() {}
  virtual ~Create_func_json_merge_preserve() {}
};
Create_func_json_merge_preserve Create_func_json_merge_preserve::s_singleton;

class Create_func_json_merge : public Create_func_json_merge_preserve
{
public:
  static Create_func_json_merge s_singleton;
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
  {
    Item *func= Create_func_json_merge_preserve::create_native(thd, name,
                                                               item_list);
    /*
      JSON_MERGE is a deprecated alias for JSON_MERGE_PRESERVE. Warn
      the users and recommend that they specify explicitly what kind
      of merge operation they want.
    */
    if (func != NULL)
      push_deprecated_warn(thd, "JSON_MERGE",
                           "JSON_MERGE_PRESERVE/JSON_MERGE_PATCH");

    return func;
  }
};
Create_func_json_merge Create_func_json_merge::s_singleton;

class Create_func_json_quote : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_quote s_singleton;

protected:
  Create_func_json_quote() {}
  virtual ~Create_func_json_quote() {}
};

class Create_func_json_storage_size : public Create_func_arg1
{
public:
  static Create_func_json_storage_size s_singleton;
  virtual Item *create(THD *thd, Item *arg1)
  {
    return new (thd->mem_root) Item_func_json_storage_size(POS(), arg1);
  }
};
Create_func_json_storage_size Create_func_json_storage_size::s_singleton;

class Create_func_json_unquote : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_unquote s_singleton;

protected:
  Create_func_json_unquote() {}
  virtual ~Create_func_json_unquote() {}
};

class Create_func_latfromgeohash : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_latfromgeohash s_singleton;

protected:
  Create_func_latfromgeohash() {}
  virtual ~Create_func_latfromgeohash() {}
};


class Create_func_longfromgeohash : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_longfromgeohash s_singleton;

protected:
  Create_func_longfromgeohash() {}
  virtual ~Create_func_longfromgeohash() {}
};


class Create_func_last_day : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_last_day s_singleton;

protected:
  Create_func_last_day() {}
  virtual ~Create_func_last_day() {}
};


class Create_func_last_insert_id : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_last_insert_id s_singleton;

protected:
  Create_func_last_insert_id() {}
  virtual ~Create_func_last_insert_id() {}
};


class Create_func_lower : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_lower s_singleton;

protected:
  Create_func_lower() {}
  virtual ~Create_func_lower() {}
};


class Create_func_least : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_least s_singleton;

protected:
  Create_func_least() {}
  virtual ~Create_func_least() {}
};


class Create_func_length : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_length s_singleton;

protected:
  Create_func_length() {}
  virtual ~Create_func_length() {}
};


#ifndef DBUG_OFF
class Create_func_like_range_min : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_like_range_min s_singleton;

protected:
  Create_func_like_range_min() {}
  virtual ~Create_func_like_range_min() {}
};


class Create_func_like_range_max : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_like_range_max s_singleton;

protected:
  Create_func_like_range_max() {}
  virtual ~Create_func_like_range_max() {}
};
#endif


class Create_func_ln : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_ln s_singleton;

protected:
  Create_func_ln() {}
  virtual ~Create_func_ln() {}
};


class Create_func_load_file : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_load_file s_singleton;

protected:
  Create_func_load_file() {}
  virtual ~Create_func_load_file() {}
};


class Create_func_locate : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_locate s_singleton;

protected:
  Create_func_locate() {}
  virtual ~Create_func_locate() {}
};


class Create_func_log : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_log s_singleton;

protected:
  Create_func_log() {}
  virtual ~Create_func_log() {}
};


class Create_func_log10 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_log10 s_singleton;

protected:
  Create_func_log10() {}
  virtual ~Create_func_log10() {}
};


class Create_func_log2 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_log2 s_singleton;

protected:
  Create_func_log2() {}
  virtual ~Create_func_log2() {}
};


class Create_func_lpad : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_lpad s_singleton;

protected:
  Create_func_lpad() {}
  virtual ~Create_func_lpad() {}
};


class Create_func_ltrim : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_ltrim s_singleton;

protected:
  Create_func_ltrim() {}
  virtual ~Create_func_ltrim() {}
};


class Create_func_makedate : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_makedate s_singleton;

protected:
  Create_func_makedate() {}
  virtual ~Create_func_makedate() {}
};


class Create_func_make_envelope : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_make_envelope s_singleton;

protected:
  Create_func_make_envelope() {}
  virtual ~Create_func_make_envelope() {}
};


class Create_func_maketime : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_maketime s_singleton;

protected:
  Create_func_maketime() {}
  virtual ~Create_func_maketime() {}
};


class Create_func_make_set : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_make_set s_singleton;

protected:
  Create_func_make_set() {}
  virtual ~Create_func_make_set() {}
};


class Create_func_master_pos_wait : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_master_pos_wait s_singleton;

protected:
  Create_func_master_pos_wait() {}
  virtual ~Create_func_master_pos_wait() {}
};

class Create_func_executed_gtid_set_wait : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_executed_gtid_set_wait s_singleton;

protected:
  Create_func_executed_gtid_set_wait() {}
  virtual ~Create_func_executed_gtid_set_wait() {}
};

class Create_func_master_gtid_set_wait : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_master_gtid_set_wait s_singleton;

protected:
  Create_func_master_gtid_set_wait() {}
  virtual ~Create_func_master_gtid_set_wait() {}
};

class Create_func_md5 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_md5 s_singleton;

protected:
  Create_func_md5() {}
  virtual ~Create_func_md5() {}
};


class Create_func_monthname : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_monthname s_singleton;

protected:
  Create_func_monthname() {}
  virtual ~Create_func_monthname() {}
};


class Create_func_name_const : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_name_const s_singleton;

protected:
  Create_func_name_const() {}
  virtual ~Create_func_name_const() {}
};


class Create_func_nullif : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_nullif s_singleton;

protected:
  Create_func_nullif() {}
  virtual ~Create_func_nullif() {}
};


class Create_func_numgeometries : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_numgeometries s_singleton;

protected:
  Create_func_numgeometries() {}
  virtual ~Create_func_numgeometries() {}
};


class Create_func_numgeometries_deprecated : public Create_func_numgeometries
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "NUMGEOMETRIES", "ST_NUMGEOMETRIES");
    return Create_func_numgeometries::create(thd, arg1);
  }

  static Create_func_numgeometries_deprecated s_singleton;
};
Create_func_numgeometries_deprecated Create_func_numgeometries_deprecated::s_singleton;


class Create_func_numinteriorring : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_numinteriorring s_singleton;

protected:
  Create_func_numinteriorring() {}
  virtual ~Create_func_numinteriorring() {}
};


class Create_func_numinteriorring_deprecated : public Create_func_numinteriorring
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "NUMINTERIORRINGS", "ST_NUMINTERIORRINGS");
    return Create_func_numinteriorring::create(thd, arg1);
  }

  static Create_func_numinteriorring_deprecated s_singleton;
};
Create_func_numinteriorring_deprecated Create_func_numinteriorring_deprecated::s_singleton;


class Create_func_numpoints : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_numpoints s_singleton;

protected:
  Create_func_numpoints() {}
  virtual ~Create_func_numpoints() {}
};


class Create_func_numpoints_deprecated : public Create_func_numpoints
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "NUMPOINTS", "ST_NUMPOINTS");
    return Create_func_numpoints::create(thd, arg1);
  }

  static Create_func_numpoints_deprecated s_singleton;
};
Create_func_numpoints_deprecated Create_func_numpoints_deprecated::s_singleton;


class Create_func_oct : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_oct s_singleton;

protected:
  Create_func_oct() {}
  virtual ~Create_func_oct() {}
};


class Create_func_ord : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_ord s_singleton;

protected:
  Create_func_ord() {}
  virtual ~Create_func_ord() {}
};


class Create_func_mbr_overlaps : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_overlaps s_singleton;

protected:
  Create_func_mbr_overlaps() {}
  virtual ~Create_func_mbr_overlaps() {}
};


class Create_func_mbr_overlaps_deprecated : public Create_func_mbr_overlaps
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "OVERLAPS", "MBROVERLAPS");
    return Create_func_mbr_overlaps::create(thd, arg1, arg2);
  }

  static Create_func_mbr_overlaps_deprecated s_singleton;
};
Create_func_mbr_overlaps_deprecated Create_func_mbr_overlaps_deprecated::s_singleton;


class Create_func_overlaps : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_overlaps s_singleton;

protected:
  Create_func_overlaps() {}
  virtual ~Create_func_overlaps() {}
};


class Create_func_period_add : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_period_add s_singleton;

protected:
  Create_func_period_add() {}
  virtual ~Create_func_period_add() {}
};


class Create_func_period_diff : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_period_diff s_singleton;

protected:
  Create_func_period_diff() {}
  virtual ~Create_func_period_diff() {}
};


class Create_func_pi : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_pi s_singleton;

protected:
  Create_func_pi() {}
  virtual ~Create_func_pi() {}
};


class Create_func_pointfromgeohash : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_pointfromgeohash s_singleton;

protected:
  Create_func_pointfromgeohash() {}
  virtual ~Create_func_pointfromgeohash() {}
};


class Create_func_pointn : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_pointn s_singleton;

protected:
  Create_func_pointn() {}
  virtual ~Create_func_pointn() {}
};


class Create_func_pointn_deprecated : public Create_func_pointn
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "POINTN", "ST_POINTN");
    return Create_func_pointn::create(thd, arg1, arg2);
  }

  static Create_func_pointn_deprecated s_singleton;
};
Create_func_pointn_deprecated Create_func_pointn_deprecated::s_singleton;


class Create_func_pow : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_pow s_singleton;

protected:
  Create_func_pow() {}
  virtual ~Create_func_pow() {}
};


class Create_func_quote : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_quote s_singleton;

protected:
  Create_func_quote() {}
  virtual ~Create_func_quote() {}
};


class Create_func_radians : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_radians s_singleton;

protected:
  Create_func_radians() {}
  virtual ~Create_func_radians() {}
};


class Create_func_rand : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_rand s_singleton;

protected:
  Create_func_rand() {}
  virtual ~Create_func_rand() {}
};


class Create_func_release_all_locks : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_release_all_locks s_singleton;

protected:
  Create_func_release_all_locks() {}
  virtual ~Create_func_release_all_locks() {}
};


class Create_func_release_lock : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_release_lock s_singleton;

protected:
  Create_func_release_lock() {}
  virtual ~Create_func_release_lock() {}
};


class Create_func_reverse : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_reverse s_singleton;

protected:
  Create_func_reverse() {}
  virtual ~Create_func_reverse() {}
};


class Create_func_round : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_round s_singleton;

protected:
  Create_func_round() {}
  virtual ~Create_func_round() {}
};


class Create_func_rpad : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_rpad s_singleton;

protected:
  Create_func_rpad() {}
  virtual ~Create_func_rpad() {}
};


class Create_func_rtrim : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_rtrim s_singleton;

protected:
  Create_func_rtrim() {}
  virtual ~Create_func_rtrim() {}
};


class Create_func_sec_to_time : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sec_to_time s_singleton;

protected:
  Create_func_sec_to_time() {}
  virtual ~Create_func_sec_to_time() {}
};


class Create_func_sha : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sha s_singleton;

protected:
  Create_func_sha() {}
  virtual ~Create_func_sha() {}
};


class Create_func_sha2 : public Create_func_arg2
{
public:
  virtual Item* create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_sha2 s_singleton;

protected:
  Create_func_sha2() {}
  virtual ~Create_func_sha2() {}
};


class Create_func_sign : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sign s_singleton;

protected:
  Create_func_sign() {}
  virtual ~Create_func_sign() {}
};


class Create_func_sin : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sin s_singleton;

protected:
  Create_func_sin() {}
  virtual ~Create_func_sin() {}
};


class Create_func_sleep : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sleep s_singleton;

protected:
  Create_func_sleep() {}
  virtual ~Create_func_sleep() {}
};


class Create_func_soundex : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_soundex s_singleton;

protected:
  Create_func_soundex() {}
  virtual ~Create_func_soundex() {}
};


class Create_func_space : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_space s_singleton;

protected:
  Create_func_space() {}
  virtual ~Create_func_space() {}
};


class Create_func_sqrt : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_sqrt s_singleton;

protected:
  Create_func_sqrt() {}
  virtual ~Create_func_sqrt() {}
};


class Create_func_simplify : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_simplify s_singleton;

protected:
  Create_func_simplify() {}
  virtual ~Create_func_simplify() {}
};


class Create_func_srid : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_srid s_singleton;

protected:
  Create_func_srid() {}
  virtual ~Create_func_srid() {}
};


class Create_func_srid_deprecated : public Create_func_srid
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "SRID", "ST_SRID");
    return Create_func_srid::create(thd, arg1);
  }

  static Create_func_srid_deprecated s_singleton;
};
Create_func_srid_deprecated Create_func_srid_deprecated::s_singleton;


class Create_func_startpoint : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_startpoint s_singleton;

protected:
  Create_func_startpoint() {}
  virtual ~Create_func_startpoint() {}
};


class Create_func_startpoint_deprecated : public Create_func_startpoint
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "STARTPOINT", "ST_STARTPOINT");
    return Create_func_startpoint::create(thd, arg1);
  }

  static Create_func_startpoint_deprecated s_singleton;
};
Create_func_startpoint_deprecated Create_func_startpoint_deprecated::s_singleton;


class Create_func_str_to_date : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_str_to_date s_singleton;

protected:
  Create_func_str_to_date() {}
  virtual ~Create_func_str_to_date() {}
};


class Create_func_strcmp : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_strcmp s_singleton;

protected:
  Create_func_strcmp() {}
  virtual ~Create_func_strcmp() {}
};


class Create_func_substr_index : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_substr_index s_singleton;

protected:
  Create_func_substr_index() {}
  virtual ~Create_func_substr_index() {}
};


class Create_func_subtime : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_subtime s_singleton;

protected:
  Create_func_subtime() {}
  virtual ~Create_func_subtime() {}
};


class Create_func_tan : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_tan s_singleton;

protected:
  Create_func_tan() {}
  virtual ~Create_func_tan() {}
};


class Create_func_time_format : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_time_format s_singleton;

protected:
  Create_func_time_format() {}
  virtual ~Create_func_time_format() {}
};


class Create_func_time_to_sec : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_time_to_sec s_singleton;

protected:
  Create_func_time_to_sec() {}
  virtual ~Create_func_time_to_sec() {}
};


class Create_func_timediff : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_timediff s_singleton;

protected:
  Create_func_timediff() {}
  virtual ~Create_func_timediff() {}
};


class Create_func_to_base64 : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_to_base64 s_singleton;

protected:
  Create_func_to_base64() {}
  virtual ~Create_func_to_base64() {}
};


class Create_func_to_days : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_to_days s_singleton;

protected:
  Create_func_to_days() {}
  virtual ~Create_func_to_days() {}
};

class Create_func_to_seconds : public Create_func_arg1
{
public:
  virtual Item* create(THD *thd, Item *arg1);

  static Create_func_to_seconds s_singleton;

protected:
  Create_func_to_seconds() {}
  virtual ~Create_func_to_seconds() {}
};


class Create_func_touches : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_touches s_singleton;

protected:
  Create_func_touches() {}
  virtual ~Create_func_touches() {}
};


class Create_func_mbr_touches : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_touches s_singleton;

protected:
  Create_func_mbr_touches() {}
  virtual ~Create_func_mbr_touches() {}
};


class Create_func_touches_deprecated : public Create_func_touches
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "TOUCHES", "ST_TOUCHES");
    return Create_func_touches::create(thd, arg1, arg2);
  }

  static Create_func_touches_deprecated s_singleton;
};
Create_func_touches_deprecated Create_func_touches_deprecated::s_singleton;


class Create_func_upper : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_upper s_singleton;

protected:
  Create_func_upper() {}
  virtual ~Create_func_upper() {}
};


class Create_func_uncompress : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_uncompress s_singleton;

protected:
  Create_func_uncompress() {}
  virtual ~Create_func_uncompress() {}
};


class Create_func_uncompressed_length : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_uncompressed_length s_singleton;

protected:
  Create_func_uncompressed_length() {}
  virtual ~Create_func_uncompressed_length() {}
};


class Create_func_unhex : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_unhex s_singleton;

protected:
  Create_func_unhex() {}
  virtual ~Create_func_unhex() {}
};


class Create_func_unix_timestamp : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_unix_timestamp s_singleton;

protected:
  Create_func_unix_timestamp() {}
  virtual ~Create_func_unix_timestamp() {}
};


class Create_func_uuid : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_uuid s_singleton;

protected:
  Create_func_uuid() {}
  virtual ~Create_func_uuid() {}
};


class Create_func_uuid_short : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_uuid_short s_singleton;

protected:
  Create_func_uuid_short() {}
  virtual ~Create_func_uuid_short() {}
};


class Create_func_validate_password_strength : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_validate_password_strength s_singleton;

protected:
  Create_func_validate_password_strength() {}
  virtual ~Create_func_validate_password_strength() {}
};


class Create_func_version : public Create_func_arg0
{
public:
  virtual Item *create(THD *thd);

  static Create_func_version s_singleton;

protected:
  Create_func_version() {}
  virtual ~Create_func_version() {}
};


class Create_func_weekday : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_weekday s_singleton;

protected:
  Create_func_weekday() {}
  virtual ~Create_func_weekday() {}
};


class Create_func_weekofyear : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_weekofyear s_singleton;

protected:
  Create_func_weekofyear() {}
  virtual ~Create_func_weekofyear() {}
};


class Create_func_mbr_within : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_mbr_within s_singleton;

protected:
  Create_func_mbr_within() {}
  virtual ~Create_func_mbr_within() {}
};


class Create_func_within_deprecated : public Create_func_mbr_within
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "WITHIN", "MBRWITHIN");
    return Create_func_mbr_within::create(thd, arg1, arg2);
  }

  static Create_func_within_deprecated s_singleton;
};
Create_func_within_deprecated Create_func_within_deprecated::s_singleton;


class Create_func_within : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_within s_singleton;

protected:
  Create_func_within() {}
  virtual ~Create_func_within() {}
};


class Create_func_x : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_x s_singleton;

protected:
  Create_func_x() {}
  virtual ~Create_func_x() {}
};


class Create_func_x_deprecated : public Create_func_x
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "X", "ST_X");
    return Create_func_x::create(thd, arg1);
  }

  static Create_func_x_deprecated s_singleton;
};
Create_func_x_deprecated Create_func_x_deprecated::s_singleton;


class Create_func_xml_extractvalue : public Create_func_arg2
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2);

  static Create_func_xml_extractvalue s_singleton;

protected:
  Create_func_xml_extractvalue() {}
  virtual ~Create_func_xml_extractvalue() {}
};


class Create_func_xml_update : public Create_func_arg3
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2, Item *arg3);

  static Create_func_xml_update s_singleton;

protected:
  Create_func_xml_update() {}
  virtual ~Create_func_xml_update() {}
};


class Create_func_y : public Create_func_arg1
{
public:
  virtual Item *create(THD *thd, Item *arg1);

  static Create_func_y s_singleton;

protected:
  Create_func_y() {}
  virtual ~Create_func_y() {}
};


class Create_func_y_deprecated : public Create_func_y
{
public:
  virtual Item *create(THD *thd, Item *arg1)
  {
    push_deprecated_warn(thd, "Y", "ST_Y");
    return Create_func_y::create(thd, arg1);
  }

  static Create_func_y_deprecated s_singleton;
};
Create_func_y_deprecated Create_func_y_deprecated::s_singleton;


class Create_func_year_week : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_year_week s_singleton;

protected:
  Create_func_year_week() {}
  virtual ~Create_func_year_week() {}
};


/*
=============================================================================
  IMPLEMENTATION
=============================================================================
*/

Item*
Create_qfunc::create_func(THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  return create(thd, NULL_STR, name, false, item_list);
}


#ifdef HAVE_DLOPEN
Create_udf_func Create_udf_func::s_singleton;

Item*
Create_udf_func::create_func(THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  udf_func *udf= find_udf(name.str, name.length);
  DBUG_ASSERT(udf);
  return create(thd, udf, item_list);
}


Item*
Create_udf_func::create(THD *thd, udf_func *udf, PT_item_list *item_list)
{
  DBUG_ENTER("Create_udf_func::create");

  DBUG_ASSERT(   (udf->type == UDFTYPE_FUNCTION)
              || (udf->type == UDFTYPE_AGGREGATE));

  Item *func= NULL;
  POS pos;

  switch(udf->returns) {
  case STRING_RESULT:
    if (udf->type == UDFTYPE_FUNCTION)
      func= new (thd->mem_root) Item_func_udf_str(pos, udf, item_list);
    else
      func= new (thd->mem_root) Item_sum_udf_str(pos, udf, item_list);
    break;
  case REAL_RESULT:
    if (udf->type == UDFTYPE_FUNCTION)
      func= new (thd->mem_root) Item_func_udf_float(pos, udf, item_list);
    else
      func= new (thd->mem_root) Item_sum_udf_float(pos, udf, item_list);
    break;
  case INT_RESULT:
    if (udf->type == UDFTYPE_FUNCTION)
      func= new (thd->mem_root) Item_func_udf_int(pos, udf, item_list);
    else
      func= new (thd->mem_root) Item_sum_udf_int(pos, udf, item_list);
    break;
  case DECIMAL_RESULT:
    if (udf->type == UDFTYPE_FUNCTION)
      func= new (thd->mem_root) Item_func_udf_decimal(pos, udf, item_list);
    else
      func= new (thd->mem_root) Item_sum_udf_decimal(pos, udf, item_list);
    break;
  default:
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "UDF return type");
  }
  DBUG_RETURN(func);
}
#endif /* HAVE_DLOPEN */


Create_sp_func Create_sp_func::s_singleton;

Item*
Create_sp_func::create(THD *thd, LEX_STRING db, LEX_STRING name,
                       bool use_explicit_name, PT_item_list *item_list)
{

  return new (thd->mem_root) Item_func_sp(POS(), db, name,
                                          use_explicit_name, item_list);
}


Item*
Create_native_func::create_func(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  return create_native(thd, name, item_list);
}


Item*
Create_func_arg0::create_func(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
{
  if (item_list != NULL)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return create(thd);
}


Item*
Create_func_arg1::create_func(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  Item *param_1= item_list->pop_front();
  return create(thd, param_1);
}


Item*
Create_func_arg2::create_func(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  if (arg_count != 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  return create(thd, param_1, param_2);
}


Item*
Create_func_arg3::create_func(THD *thd, LEX_STRING name,
                              PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  if (arg_count != 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  return create(thd, param_1, param_2, param_3);
}


Create_func_abs Create_func_abs::s_singleton;

Item*
Create_func_abs::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_abs(POS(), arg1);
}


Create_func_acos Create_func_acos::s_singleton;

Item*
Create_func_acos::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_acos(POS(), arg1);
}


Create_func_addtime Create_func_addtime::s_singleton;

Item*
Create_func_addtime::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_add_time(POS(), arg1, arg2, 0, 0);
}


Create_func_aes_encrypt Create_func_aes_encrypt::s_singleton;


Create_func_aes_decrypt Create_func_aes_decrypt::s_singleton;


Create_func_random_bytes Create_func_random_bytes::s_singleton;


Create_func_any_value Create_func_any_value::s_singleton;

Create_func_area Create_func_area::s_singleton;

Item*
Create_func_area::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_area(POS(), arg1);
}


Create_func_as_geojson Create_func_as_geojson::s_singleton;

Item*
Create_func_as_geojson::create_native(THD *thd, LEX_STRING name,
                                      PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count)
  {
  case 1:
    {
      Item *geometry= item_list->pop_front();
      func= new (thd->mem_root) Item_func_as_geojson(thd, POS(), geometry);
      break;
    }
  case 2:
    {
      Item *geometry= item_list->pop_front();
      Item *maxdecimaldigits= item_list->pop_front();
      func= new (thd->mem_root) Item_func_as_geojson(thd, POS(), geometry,
                                                     maxdecimaldigits);
      break;
    }
  case 3:
    {
      Item *geometry= item_list->pop_front();
      Item *maxdecimaldigits= item_list->pop_front();
      Item *options= item_list->pop_front();
      func= new (thd->mem_root) Item_func_as_geojson(thd, POS(), geometry,
                                                     maxdecimaldigits, options);
      break;
    }
  default:
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
      break;
    }
  }

  return func;
}


Create_func_as_wkb Create_func_as_wkb::s_singleton;

Item*
Create_func_as_wkb::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_as_wkb(POS(), arg1);
}


Create_func_as_wkt Create_func_as_wkt::s_singleton;

Item*
Create_func_as_wkt::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_as_wkt(POS(), arg1);
}


Create_func_asin Create_func_asin::s_singleton;

Item*
Create_func_asin::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_asin(POS(), arg1);
}


Create_func_atan Create_func_atan::s_singleton;

Item*
Create_func_atan::create_native(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_atan(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_atan(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_benchmark Create_func_benchmark::s_singleton;

Item*
Create_func_benchmark::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_benchmark(POS(), arg1, arg2);
}


Create_func_bin Create_func_bin::s_singleton;

Item*
Create_func_bin::create(THD *thd, Item *arg1)
{
  POS pos;
  Item *i10= new (thd->mem_root) Item_int(pos, 10, 2);
  Item *i2= new (thd->mem_root) Item_int(pos, 2, 1);
  return new (thd->mem_root) Item_func_conv(pos, arg1, i10, i2);
}


Create_func_bit_count Create_func_bit_count::s_singleton;

Item*
Create_func_bit_count::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_bit_count(POS(), arg1);
}


Create_func_bit_length Create_func_bit_length::s_singleton;

Item*
Create_func_bit_length::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_bit_length(POS(), arg1);
}


Create_func_buffer_strategy Create_func_buffer_strategy::s_singleton;

Item*
Create_func_buffer_strategy::create_native(THD *thd, LEX_STRING name,
                                           PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 1 || arg_count > 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_buffer_strategy(POS(), item_list);
}


Create_func_ceiling Create_func_ceiling::s_singleton;

Item*
Create_func_ceiling::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_ceiling(POS(), arg1);
}


Create_func_centroid Create_func_centroid::s_singleton;

Item*
Create_func_centroid::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_centroid(POS(), arg1);
}


Create_func_convex_hull Create_func_convex_hull::s_singleton;

Item*
Create_func_convex_hull::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_convex_hull(POS(), arg1);
}

Create_func_char_length Create_func_char_length::s_singleton;

Item*
Create_func_char_length::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_char_length(POS(), arg1);
}


Create_func_coercibility Create_func_coercibility::s_singleton;

Item*
Create_func_coercibility::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_coercibility(POS(), arg1);
}


Create_func_concat Create_func_concat::s_singleton;

Item*
Create_func_concat::create_native(THD *thd, LEX_STRING name,
                                  PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_concat(POS(), item_list);
}


Create_func_concat_ws Create_func_concat_ws::s_singleton;

Item*
Create_func_concat_ws::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  /* "WS" stands for "With Separator": this function takes 2+ arguments */
  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_concat_ws(POS(), item_list);
}


Create_func_compress Create_func_compress::s_singleton;

Item*
Create_func_compress::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_compress(POS(), arg1);
}


Create_func_connection_id Create_func_connection_id::s_singleton;

Item*
Create_func_connection_id::create(THD *thd)
{
  return new (thd->mem_root) Item_func_connection_id(POS());
}


Create_func_mbr_covered_by Create_func_mbr_covered_by::s_singleton;

Item*
Create_func_mbr_covered_by::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root)
    Item_func_spatial_mbr_rel(POS(), arg1, arg2, Item_func::SP_COVEREDBY_FUNC);
}


Create_func_mbr_covers Create_func_mbr_covers::s_singleton;

Item*
Create_func_mbr_covers::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root)
    Item_func_spatial_mbr_rel(POS(), arg1, arg2, Item_func::SP_COVERS_FUNC);
}


Create_func_mbr_contains Create_func_mbr_contains::s_singleton;

Item*
Create_func_mbr_contains::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_CONTAINS_FUNC);
}


Create_func_contains Create_func_contains::s_singleton;

Item*
Create_func_contains::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_CONTAINS_FUNC);
}


Create_func_conv Create_func_conv::s_singleton;

Item*
Create_func_conv::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_conv(POS(), arg1, arg2, arg3);
}


Create_func_convert_tz Create_func_convert_tz::s_singleton;

Item*
Create_func_convert_tz::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_convert_tz(POS(), arg1, arg2, arg3);
}


Create_func_cos Create_func_cos::s_singleton;

Item*
Create_func_cos::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_cos(POS(), arg1);
}


Create_func_cot Create_func_cot::s_singleton;

Item*
Create_func_cot::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_cot(POS(), arg1);
}


Create_func_crc32 Create_func_crc32::s_singleton;

Item*
Create_func_crc32::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_crc32(POS(), arg1);
}


Create_func_crosses Create_func_crosses::s_singleton;

Item*
Create_func_crosses::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_CROSSES_FUNC);
}


Create_func_date_format Create_func_date_format::s_singleton;

Item*
Create_func_date_format::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_date_format(POS(), arg1, arg2, 0);
}


Create_func_datediff Create_func_datediff::s_singleton;

Item*
Create_func_datediff::create(THD *thd, Item *arg1, Item *arg2)
{
  Item *i1= new (thd->mem_root) Item_func_to_days(POS(), arg1);
  Item *i2= new (thd->mem_root) Item_func_to_days(POS(), arg2);

  return new (thd->mem_root) Item_func_minus(POS(), i1, i2);
}


Create_func_dayname Create_func_dayname::s_singleton;

Item*
Create_func_dayname::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_dayname(POS(), arg1);
}


Create_func_dayofmonth Create_func_dayofmonth::s_singleton;

Item*
Create_func_dayofmonth::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_dayofmonth(POS(), arg1);
}


Create_func_dayofweek Create_func_dayofweek::s_singleton;

Item*
Create_func_dayofweek::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_weekday(POS(), arg1, 1);
}


Create_func_dayofyear Create_func_dayofyear::s_singleton;

Item*
Create_func_dayofyear::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_dayofyear(POS(), arg1);
}


Create_func_decode Create_func_decode::s_singleton;

Item*
Create_func_decode::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_decode(POS(), arg1, arg2);
}


Create_func_degrees Create_func_degrees::s_singleton;

Item*
Create_func_degrees::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_units(POS(), (char*) "degrees", arg1,
                                             180/M_PI, 0.0);
}


Create_func_des_decrypt Create_func_des_decrypt::s_singleton;

Item*
Create_func_des_decrypt::create_native(THD *thd, LEX_STRING name,
                                       PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_des_decrypt(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_des_decrypt(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  if (!thd->is_error())
    push_deprecated_warn(thd, "DES_DECRYPT", "AES_DECRYPT");

  return func;
}


Create_func_des_encrypt Create_func_des_encrypt::s_singleton;

Item*
Create_func_des_encrypt::create_native(THD *thd, LEX_STRING name,
                                       PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_des_encrypt(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_des_encrypt(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  if (!thd->is_error())
    push_deprecated_warn(thd, "DES_ENCRYPT", "AES_ENCRYPT");

  return func;
}


Create_func_dimension Create_func_dimension::s_singleton;

Item*
Create_func_dimension::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_dimension(POS(), arg1);
}


Create_func_mbr_disjoint Create_func_mbr_disjoint::s_singleton;

Item*
Create_func_mbr_disjoint::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_DISJOINT_FUNC);
}


class Create_func_disjoint_deprecated : public Create_func_mbr_disjoint
{
public:
  virtual Item *create(THD *thd, Item *arg1, Item *arg2)
  {
    push_deprecated_warn(thd, "DISJOINT", "MBRDISJOINT");
    return Create_func_mbr_disjoint::create(thd, arg1, arg2);
  }

  static Create_func_disjoint_deprecated s_singleton;
};
Create_func_disjoint_deprecated Create_func_disjoint_deprecated::s_singleton;


Create_func_disjoint Create_func_disjoint::s_singleton;

Item*
Create_func_disjoint::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_DISJOINT_FUNC);
}


Create_func_distance Create_func_distance::s_singleton;

Item *
Create_func_distance::create_native(THD *thd, LEX_STRING name,
                                    PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();
  if (arg_count != 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }
  return new (thd->mem_root) Item_func_distance(POS(), item_list, false);
}


Create_func_distance_sphere Create_func_distance_sphere::s_singleton;

Item *
Create_func_distance_sphere::create_native(THD *thd, LEX_STRING name,
                                           PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();
  if (arg_count < 2 || arg_count > 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }
  return new (thd->mem_root) Item_func_distance(POS(), item_list, true);
}


Create_func_elt Create_func_elt::s_singleton;

Item*
Create_func_elt::create_native(THD *thd, LEX_STRING name,
                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_elt(POS(), item_list);
}


Create_func_encode Create_func_encode::s_singleton;

Item*
Create_func_encode::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_encode(POS(), arg1, arg2);
}


Create_func_encrypt Create_func_encrypt::s_singleton;

Item*
Create_func_encrypt::create_native(THD *thd, LEX_STRING name,
                                   PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_encrypt(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_encrypt(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  if (!thd->is_error())
    push_deprecated_warn(thd, "ENCRYPT", "AES_ENCRYPT");

  return func;
}


Create_func_endpoint Create_func_endpoint::s_singleton;

Item*
Create_func_endpoint::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_spatial_decomp(POS(), arg1,
                                                      Item_func::SP_ENDPOINT);
}


Create_func_envelope Create_func_envelope::s_singleton;

Item*
Create_func_envelope::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_envelope(POS(), arg1);
}


Create_func_mbr_equals Create_func_mbr_equals::s_singleton;

Item*
Create_func_mbr_equals::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_EQUALS_FUNC);
}


Create_func_equals Create_func_equals::s_singleton;

Item*
Create_func_equals::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                               Item_func::SP_EQUALS_FUNC);
}


Create_func_exp Create_func_exp::s_singleton;

Item*
Create_func_exp::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_exp(POS(), arg1);
}


Create_func_export_set Create_func_export_set::s_singleton;

Item*
Create_func_export_set::create_native(THD *thd, LEX_STRING name,
                                      PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 3:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    func= new (thd->mem_root) Item_func_export_set(pos, param_1, param_2,
                                                   param_3);
    break;
  }
  case 4:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    Item *param_4= item_list->pop_front();
    func= new (thd->mem_root) Item_func_export_set(pos, param_1, param_2,
                                                   param_3, param_4);
    break;
  }
  case 5:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    Item *param_4= item_list->pop_front();
    Item *param_5= item_list->pop_front();
    func= new (thd->mem_root) Item_func_export_set(pos, param_1,
                                                   param_2, param_3,
                                                   param_4, param_5);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_exteriorring Create_func_exteriorring::s_singleton;

Item*
Create_func_exteriorring::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_spatial_decomp(POS(), arg1,
                                                      Item_func::SP_EXTERIORRING);
}


Create_func_field Create_func_field::s_singleton;

Item*
Create_func_field::create_native(THD *thd, LEX_STRING name,
                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_field(POS(), item_list);
}


Create_func_find_in_set Create_func_find_in_set::s_singleton;

Item*
Create_func_find_in_set::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_find_in_set(POS(), arg1, arg2);
}


Create_func_floor Create_func_floor::s_singleton;

Item*
Create_func_floor::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_floor(POS(), arg1);
}


Create_func_found_rows Create_func_found_rows::s_singleton;

Item*
Create_func_found_rows::create(THD *thd)
{
  return new (thd->mem_root) Item_func_found_rows(POS());
}


Create_func_from_base64 Create_func_from_base64::s_singleton;

Item*
Create_func_from_base64::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_from_base64(POS(), arg1);
}


Create_func_from_days Create_func_from_days::s_singleton;

Item*
Create_func_from_days::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_from_days(POS(), arg1);
}


Create_func_from_unixtime Create_func_from_unixtime::s_singleton;

Item*
Create_func_from_unixtime::create_native(THD *thd, LEX_STRING name,
                                         PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_from_unixtime(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *ut= new (thd->mem_root) Item_func_from_unixtime(POS(), param_1);
    func= new (thd->mem_root) Item_func_date_format(POS(), ut, param_2, 0);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_geohash Create_func_geohash::s_singleton;

Item*
Create_func_geohash::create_native(THD *thd, LEX_STRING name,
PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count)
  {
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geohash(POS(), param_1, param_2);
      break;
    }
  case 3:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      Item *param_3= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geohash(POS(), param_1, param_2,
                                                  param_3);
      break;
    }
  default:
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
      break;
    }
  }

  return func;
}


Create_func_geometry_from_text Create_func_geometry_from_text::s_singleton;

Item*
Create_func_geometry_from_text::create_native(THD *thd, LEX_STRING name,
                                              PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_geometry_from_text(pos, param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_geometry_from_text(pos, param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_geometry_from_wkb Create_func_geometry_from_wkb::s_singleton;

Item*
Create_func_geometry_from_wkb::create_native(THD *thd, LEX_STRING name,
                                             PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_geometry_from_wkb(pos, param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_geometry_from_wkb(pos, param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_geometry_type Create_func_geometry_type::s_singleton;

Item*
Create_func_geometry_type::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_geometry_type(POS(), arg1);
}


Create_func_geometryn Create_func_geometryn::s_singleton;

Item*
Create_func_geometryn::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_decomp_n(POS(), arg1, arg2,
                                                        Item_func::SP_GEOMETRYN);
}


Create_func_geomfromgeojson Create_func_geomfromgeojson::s_singleton;

Item*
Create_func_geomfromgeojson::create_native(THD *thd, LEX_STRING name,
                                           PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count)
  {
  case 1:
    {
      Item *geojson_str= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geomfromgeojson(pos, geojson_str);
      break;
    }
  case 2:
    {
      Item *geojson_str= item_list->pop_front();
      Item *options= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geomfromgeojson(pos, geojson_str,
                                                          options);
      break;
    }
  case 3:
    {
      Item *geojson_str= item_list->pop_front();
      Item *options= item_list->pop_front();
      Item *srid= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geomfromgeojson(pos, geojson_str,
                                                          options, srid);
      break;
    }
  default:
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
      break;
    }
  }

  return func;
}


Create_func_get_lock Create_func_get_lock::s_singleton;

Item*
Create_func_get_lock::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_get_lock(POS(), arg1, arg2);
}


Create_func_glength Create_func_glength::s_singleton;

Item*
Create_func_glength::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_glength(POS(), arg1);
}


Create_func_greatest Create_func_greatest::s_singleton;

Item*
Create_func_greatest::create_native(THD *thd, LEX_STRING name,
                                    PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_max(POS(), item_list);
}


Create_func_gtid_subtract Create_func_gtid_subtract::s_singleton;

Item*
Create_func_gtid_subtract::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_gtid_subtract(POS(), arg1, arg2);
}


Create_func_gtid_subset Create_func_gtid_subset::s_singleton;

Item*
Create_func_gtid_subset::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_gtid_subset(POS(), arg1, arg2);
}


Create_func_hex Create_func_hex::s_singleton;

Item*
Create_func_hex::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_hex(POS(), arg1);
}


Create_func_ifnull Create_func_ifnull::s_singleton;

Item*
Create_func_ifnull::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_ifnull(POS(), arg1, arg2);
}


Create_func_inet_ntoa Create_func_inet_ntoa::s_singleton;

Item*
Create_func_inet_ntoa::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_inet_ntoa(POS(), arg1);
}


Create_func_inet6_aton Create_func_inet6_aton::s_singleton;

Item*
Create_func_inet6_aton::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_inet6_aton(POS(), arg1);
}


Create_func_inet6_ntoa Create_func_inet6_ntoa::s_singleton;

Item*
Create_func_inet6_ntoa::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_inet6_ntoa(POS(), arg1);
}


Create_func_inet_aton Create_func_inet_aton::s_singleton;

Item*
Create_func_inet_aton::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_inet_aton(POS(), arg1);
}


Create_func_is_ipv4 Create_func_is_ipv4::s_singleton;

Item*
Create_func_is_ipv4::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_ipv4(POS(), arg1);
}


Create_func_is_ipv6 Create_func_is_ipv6::s_singleton;

Item*
Create_func_is_ipv6::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_ipv6(POS(), arg1);
}


Create_func_is_ipv4_compat Create_func_is_ipv4_compat::s_singleton;

Item*
Create_func_is_ipv4_compat::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_ipv4_compat(POS(), arg1);
}


Create_func_is_ipv4_mapped Create_func_is_ipv4_mapped::s_singleton;

Item*
Create_func_is_ipv4_mapped::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_ipv4_mapped(POS(), arg1);
}


Create_func_instr Create_func_instr::s_singleton;

Item*
Create_func_instr::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_instr(POS(), arg1, arg2);
}


Create_func_interiorringn Create_func_interiorringn::s_singleton;

Item*
Create_func_interiorringn::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_decomp_n(POS(), arg1, arg2,
                                                        Item_func::SP_INTERIORRINGN);
}


Create_func_mbr_intersects Create_func_mbr_intersects::s_singleton;

Item*
Create_func_mbr_intersects::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_INTERSECTS_FUNC);
}


Create_func_intersects Create_func_intersects::s_singleton;

Item*
Create_func_intersects::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_INTERSECTS_FUNC);
}


Create_func_intersection Create_func_intersection::s_singleton;

Item*
Create_func_intersection::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_operation(POS(), arg1, arg2,
                               Item_func_spatial_operation::op_intersection);
}


Create_func_difference Create_func_difference::s_singleton;

Item*
Create_func_difference::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_operation(POS(), arg1, arg2,
                               Item_func_spatial_operation::op_difference);
}


Create_func_union Create_func_union::s_singleton;

Item*
Create_func_union::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_operation(POS(), arg1, arg2,
                               Item_func_spatial_operation::op_union);
}


Create_func_symdifference Create_func_symdifference::s_singleton;

Item*
Create_func_symdifference::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_operation(POS(), arg1, arg2,
                               Item_func_spatial_operation::op_symdifference);
}


Create_func_buffer Create_func_buffer::s_singleton;

Item*
Create_func_buffer::create_native(THD *thd, LEX_STRING name,
                                  PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2 || arg_count > 5)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }
  return new (thd->mem_root) Item_func_buffer(POS(), item_list);

}


Create_func_is_free_lock Create_func_is_free_lock::s_singleton;

Item*
Create_func_is_free_lock::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_free_lock(POS(), arg1);
}


Create_func_is_used_lock Create_func_is_used_lock::s_singleton;

Item*
Create_func_is_used_lock::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_is_used_lock(POS(), arg1);
}


Create_func_isclosed Create_func_isclosed::s_singleton;

Item*
Create_func_isclosed::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_isclosed(POS(), arg1);
}


Create_func_isempty Create_func_isempty::s_singleton;

Item*
Create_func_isempty::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_isempty(POS(), arg1);
}


Create_func_isnull Create_func_isnull::s_singleton;

Item*
Create_func_isnull::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_isnull(POS(), arg1);
}


Create_func_issimple Create_func_issimple::s_singleton;

Item*
Create_func_issimple::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_issimple(POS(), arg1);
}


Create_func_json_valid Create_func_json_valid::s_singleton;

Item*
Create_func_json_valid::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_json_valid(POS(), arg1);
}

Create_func_json_contains Create_func_json_contains::s_singleton;

Item*
Create_func_json_contains::create_native(THD *thd, LEX_STRING name,
                                         PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count == 2 || arg_count == 3)
  {
    func= new (thd->mem_root) Item_func_json_contains(thd, POS(), item_list);
  }
  else
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }

  return func;
}

Create_func_json_contains_path Create_func_json_contains_path::s_singleton;

Item*
Create_func_json_contains_path::create_native(THD *thd, LEX_STRING name,
                                              PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (!(arg_count >= 3))
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_contains_path(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_length Create_func_json_length::s_singleton;

Item*
Create_func_json_length::create_native(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_json_length(thd, POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_json_length(thd, POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}

Create_func_json_depth Create_func_json_depth::s_singleton;

Item*
Create_func_json_depth::create_native(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
  {
    arg_count= item_list->elements();
  }

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    Item *param_1= item_list->pop_front();

    func= new (thd->mem_root) Item_func_json_depth(POS(), param_1);
  }

  return func;
}

Create_func_json_type Create_func_json_type::s_singleton;

Item*
Create_func_json_type::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_json_type(POS(), arg1);
}

Create_func_json_keys Create_func_json_keys::s_singleton;

Item*
Create_func_json_keys::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count)
  {
  case 1:
    {
      Item *param_1= item_list->pop_front();
      func= new (thd->mem_root) Item_func_json_keys(thd, POS(), param_1);
      break;
    }
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_json_keys(thd, POS(),
                                                    param_1, param_2);
      break;
    }
  default:
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }

  return func;
}

Create_func_json_extract Create_func_json_extract::s_singleton;

Item*
Create_func_json_extract::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_extract(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_array_append Create_func_json_array_append::s_singleton;

Item*
Create_func_json_array_append::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }

  if (arg_count % 2 == 0) // 3,5,7, ..., (k*2)+1 args allowed
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_array_append(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_insert Create_func_json_insert::s_singleton;

Item*
Create_func_json_insert::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }

  if (arg_count % 2 == 0) // 3,5,7, ..., (k*2)+1 args allowed
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_insert(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_array_insert Create_func_json_array_insert::s_singleton;

Item*
Create_func_json_array_insert::create_native(THD *thd,
                                             LEX_STRING name,
                                             PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  if (arg_count % 2 == 0) // 3,5,7, ..., (k*2)+1 args allowed
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_array_insert(thd,
                                                          POS(),
                                                          item_list);
  }

  return func;
}

Create_func_json_row_object Create_func_json_row_object::s_singleton;

Item*
Create_func_json_row_object::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count % 2 != 0) // arguments come in pairs
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_row_object(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_search Create_func_json_search::s_singleton;

Item*
Create_func_json_search::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_search(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_set Create_func_json_set::s_singleton;

Item*
Create_func_json_set::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }

  if (arg_count % 2 == 0) // 3,5,7, ..., (k*2)+1 args allowed
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_set(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_replace Create_func_json_replace::s_singleton;

Item*
Create_func_json_replace::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }

  if (arg_count % 2 == 0) // 3,5,7, ..., (k*2)+1 args allowed
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_replace(thd, POS(), item_list);
  }

  return func;
}

Create_func_json_array Create_func_json_array::s_singleton;

Item*
Create_func_json_array::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  return new (thd->mem_root) Item_func_json_array(thd, POS(), item_list);
}

Create_func_json_remove Create_func_json_remove::s_singleton;

Item*
Create_func_json_remove::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
  {
    arg_count= item_list->elements();
  }

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_remove(thd, POS(), item_list);
  }

  return func;
}

Create_func_isvalid Create_func_isvalid::s_singleton;

Item*
Create_func_isvalid::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_isvalid(POS(), arg1);
}


Create_func_validate Create_func_validate::s_singleton;

Item*
Create_func_validate::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_validate(POS(), arg1);
}

Item*
Create_func_json_merge_patch::create_native(THD *thd, LEX_STRING name,
                                            PT_item_list *item_list)
{
  int arg_count= item_list ? item_list->elements() : 0;
  if (arg_count < 2)
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  return new (thd->mem_root) Item_func_json_merge_patch(thd, POS(), item_list);
}

Item*
Create_func_json_merge_preserve::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
  {
    arg_count= item_list->elements();
  }

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_merge_preserve(thd, POS(),
                                                            item_list);
  }

  return func;
}

Create_func_json_quote Create_func_json_quote::s_singleton;

Item*
Create_func_json_quote::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
  {
    arg_count= item_list->elements();
  }

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_quote(POS(), item_list);
  }

  return func;
}

Create_func_json_unquote Create_func_json_unquote::s_singleton;

Item*
Create_func_json_unquote::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item* func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
  {
    arg_count= item_list->elements();
  }

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
  }
  else
  {
    func= new (thd->mem_root) Item_func_json_unquote(POS(), item_list);
  }

  return func;
}

Create_func_last_day Create_func_last_day::s_singleton;

Item*
Create_func_last_day::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_last_day(POS(), arg1);
}


Create_func_latfromgeohash Create_func_latfromgeohash::s_singleton;

Item*
Create_func_latfromgeohash::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_latfromgeohash(POS(), arg1);
}


Create_func_last_insert_id Create_func_last_insert_id::s_singleton;

Item*
Create_func_last_insert_id::create_native(THD *thd, LEX_STRING name,
                                          PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 0:
  {
    func= new (thd->mem_root) Item_func_last_insert_id(pos);
    break;
  }
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_last_insert_id(pos, param_1);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_lower Create_func_lower::s_singleton;

Item*
Create_func_lower::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_lower(POS(), arg1);
}


Create_func_least Create_func_least::s_singleton;

Item*
Create_func_least::create_native(THD *thd, LEX_STRING name,
                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return new (thd->mem_root) Item_func_min(POS(), item_list);
}


Create_func_length Create_func_length::s_singleton;

Item*
Create_func_length::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_length(POS(), arg1);
}


#ifndef DBUG_OFF
Create_func_like_range_min Create_func_like_range_min::s_singleton;

Item*
Create_func_like_range_min::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_like_range_min(POS(), arg1, arg2);
}


Create_func_like_range_max Create_func_like_range_max::s_singleton;

Item*
Create_func_like_range_max::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_like_range_max(POS(), arg1, arg2);
}
#endif


Create_func_ln Create_func_ln::s_singleton;

Item*
Create_func_ln::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_ln(POS(), arg1);
}


Create_func_load_file Create_func_load_file::s_singleton;

Item*
Create_func_load_file::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_load_file(POS(), arg1);
}


Create_func_locate Create_func_locate::s_singleton;

Item*
Create_func_locate::create_native(THD *thd, LEX_STRING name,
                                  PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    /* Yes, parameters in that order : 2, 1 */
    func= new (thd->mem_root) Item_func_locate(pos, param_2, param_1);
    break;
  }
  case 3:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    /* Yes, parameters in that order : 2, 1, 3 */
    func= new (thd->mem_root) Item_func_locate(pos, param_2, param_1, param_3);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_log Create_func_log::s_singleton;

Item*
Create_func_log::create_native(THD *thd, LEX_STRING name,
                               PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_log(POS(), param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_log(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_log10 Create_func_log10::s_singleton;

Item*
Create_func_log10::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_log10(POS(), arg1);
}


Create_func_log2 Create_func_log2::s_singleton;

Item*
Create_func_log2::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_log2(POS(), arg1);
}


Create_func_longfromgeohash Create_func_longfromgeohash::s_singleton;

Item*
Create_func_longfromgeohash::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_longfromgeohash(POS(), arg1);
}


Create_func_lpad Create_func_lpad::s_singleton;

Item*
Create_func_lpad::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_lpad(POS(), arg1, arg2, arg3);
}


Create_func_ltrim Create_func_ltrim::s_singleton;

Item*
Create_func_ltrim::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_trim(POS(), arg1,
                                            Item_func_trim::TRIM_LTRIM);
}


Create_func_makedate Create_func_makedate::s_singleton;

Item*
Create_func_makedate::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_makedate(POS(), arg1, arg2);
}


Create_func_make_envelope Create_func_make_envelope::s_singleton;

Item*
Create_func_make_envelope::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_make_envelope(POS(), arg1, arg2);
}


Create_func_maketime Create_func_maketime::s_singleton;

Item*
Create_func_maketime::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_maketime(POS(), arg1, arg2, arg3);
}


Create_func_make_set Create_func_make_set::s_singleton;

Item*
Create_func_make_set::create_native(THD *thd, LEX_STRING name,
                                    PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  if (arg_count < 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  Item *param_1= item_list->pop_front();
  return new (thd->mem_root) Item_func_make_set(POS(), param_1,
                                                item_list);
}


Create_func_master_pos_wait Create_func_master_pos_wait::s_singleton;

Item*
Create_func_master_pos_wait::create_native(THD *thd, LEX_STRING name,
                                           PT_item_list *item_list)

{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_master_pos_wait(pos, param_1, param_2);
    break;
  }
  case 3:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    func= new (thd->mem_root) Item_master_pos_wait(pos, param_1, param_2, param_3);
    break;
  }
  case 4:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    Item *param_4= item_list->pop_front();
    func= new (thd->mem_root) Item_master_pos_wait(pos, param_1, param_2, param_3,
                                                   param_4);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}

Create_func_master_gtid_set_wait Create_func_master_gtid_set_wait::s_singleton;

Item*
Create_func_master_gtid_set_wait::create_native(THD *thd, LEX_STRING name,
                                                PT_item_list *item_list)

{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_master_gtid_set_wait(pos, param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_master_gtid_set_wait(pos, param_1, param_2);
    break;
  }
  case 3:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    Item *param_3= item_list->pop_front();
    func= new (thd->mem_root) Item_master_gtid_set_wait(pos, param_1, param_2,
                                                        param_3);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}

Create_func_executed_gtid_set_wait Create_func_executed_gtid_set_wait::s_singleton;

Item*
Create_func_executed_gtid_set_wait::create_native(THD *thd, LEX_STRING name,
                                                  PT_item_list *item_list)

{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_wait_for_executed_gtid_set(pos, param_1);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_wait_for_executed_gtid_set(pos, param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}

Create_func_md5 Create_func_md5::s_singleton;

Item*
Create_func_md5::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_md5(POS(), arg1);
}


Create_func_monthname Create_func_monthname::s_singleton;

Item*
Create_func_monthname::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_monthname(POS(), arg1);
}


Create_func_name_const Create_func_name_const::s_singleton;

Item*
Create_func_name_const::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_name_const(POS(), arg1, arg2);
}


Create_func_nullif Create_func_nullif::s_singleton;

Item*
Create_func_nullif::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_nullif(POS(), arg1, arg2);
}


Create_func_numgeometries Create_func_numgeometries::s_singleton;

Item*
Create_func_numgeometries::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_numgeometries(POS(), arg1);
}


Create_func_numinteriorring Create_func_numinteriorring::s_singleton;

Item*
Create_func_numinteriorring::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_numinteriorring(POS(), arg1);
}


Create_func_numpoints Create_func_numpoints::s_singleton;

Item*
Create_func_numpoints::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_numpoints(POS(), arg1);
}


Create_func_oct Create_func_oct::s_singleton;

Item*
Create_func_oct::create(THD *thd, Item *arg1)
{
  Item *i10= new (thd->mem_root) Item_int(POS(), 10,2);
  Item *i8= new (thd->mem_root) Item_int(POS(), 8,1);
  return new (thd->mem_root) Item_func_conv(POS(), arg1, i10, i8);
}


Create_func_ord Create_func_ord::s_singleton;

Item*
Create_func_ord::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_ord(POS(), arg1);
}


Create_func_mbr_overlaps Create_func_mbr_overlaps::s_singleton;

Item*
Create_func_mbr_overlaps::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_OVERLAPS_FUNC);
}


Create_func_overlaps Create_func_overlaps::s_singleton;

Item*
Create_func_overlaps::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_OVERLAPS_FUNC);
}


Create_func_period_add Create_func_period_add::s_singleton;

Item*
Create_func_period_add::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_period_add(POS(), arg1, arg2);
}


Create_func_period_diff Create_func_period_diff::s_singleton;

Item*
Create_func_period_diff::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_period_diff(POS(), arg1, arg2);
}


Create_func_pi Create_func_pi::s_singleton;

Item*
Create_func_pi::create(THD *thd)
{
  return new (thd->mem_root) Item_static_float_func(POS(),
                                                    NAME_STRING("pi()"),
                                                    M_PI, 6, 8);
}


Create_func_pointfromgeohash Create_func_pointfromgeohash::s_singleton;

Item*
Create_func_pointfromgeohash::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_pointfromgeohash(POS(), arg1, arg2);
}


Create_func_pointn Create_func_pointn::s_singleton;

Item*
Create_func_pointn::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_decomp_n(POS(), arg1, arg2,
                                                        Item_func::SP_POINTN);
}


Create_func_pow Create_func_pow::s_singleton;

Item*
Create_func_pow::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_pow(POS(), arg1, arg2);
}


Create_func_quote Create_func_quote::s_singleton;

Item*
Create_func_quote::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_quote(POS(), arg1);
}


Create_func_radians Create_func_radians::s_singleton;

Item*
Create_func_radians::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_units(POS(), (char*) "radians", arg1,
                                             M_PI/180, 0.0);
}


Create_func_rand Create_func_rand::s_singleton;

Item*
Create_func_rand::create_native(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 0:
  {
    func= new (thd->mem_root) Item_func_rand(POS());
    break;
  }
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_rand(POS(), param_1);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_release_all_locks Create_func_release_all_locks::s_singleton;

Item*
Create_func_release_all_locks::create(THD *thd)
{
  return new (thd->mem_root) Item_func_release_all_locks(POS());
}


Create_func_release_lock Create_func_release_lock::s_singleton;

Item*
Create_func_release_lock::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_release_lock(POS(), arg1);
}


Create_func_reverse Create_func_reverse::s_singleton;

Item*
Create_func_reverse::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_reverse(POS(), arg1);
}


Create_func_round Create_func_round::s_singleton;

Item*
Create_func_round::create_native(THD *thd, LEX_STRING name,
                                 PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    Item *i0 = new (thd->mem_root) Item_int_0(POS());
    func= new (thd->mem_root) Item_func_round(POS(), param_1, i0, 0);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_round(POS(), param_1, param_2, 0);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_rpad Create_func_rpad::s_singleton;

Item*
Create_func_rpad::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_rpad(POS(), arg1, arg2, arg3);
}


Create_func_rtrim Create_func_rtrim::s_singleton;

Item*
Create_func_rtrim::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_trim(POS(), arg1,
                                            Item_func_trim::TRIM_RTRIM);
}


Create_func_sec_to_time Create_func_sec_to_time::s_singleton;

Item*
Create_func_sec_to_time::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sec_to_time(POS(), arg1);
}


Create_func_sha Create_func_sha::s_singleton;

Item*
Create_func_sha::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sha(POS(), arg1);
}


Create_func_sha2 Create_func_sha2::s_singleton;

Item*
Create_func_sha2::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_sha2(POS(), arg1, arg2);
}


Create_func_sign Create_func_sign::s_singleton;

Item*
Create_func_sign::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sign(POS(), arg1);
}


Create_func_simplify Create_func_simplify::s_singleton;

Item*
Create_func_simplify::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_simplify(POS(), arg1, arg2);
}


Create_func_sin Create_func_sin::s_singleton;

Item*
Create_func_sin::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sin(POS(), arg1);
}


Create_func_sleep Create_func_sleep::s_singleton;

Item*
Create_func_sleep::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sleep(POS(), arg1);
}


Create_func_soundex Create_func_soundex::s_singleton;

Item*
Create_func_soundex::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_soundex(POS(), arg1);
}


Create_func_space Create_func_space::s_singleton;

Item*
Create_func_space::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_space(POS(), arg1);
}


Create_func_sqrt Create_func_sqrt::s_singleton;

Item*
Create_func_sqrt::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_sqrt(POS(), arg1);
}


Create_func_srid Create_func_srid::s_singleton;

Item*
Create_func_srid::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_srid(POS(), arg1);
}


Create_func_startpoint Create_func_startpoint::s_singleton;

Item*
Create_func_startpoint::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_spatial_decomp(POS(), arg1,
                                                      Item_func::SP_STARTPOINT);
}


Create_func_str_to_date Create_func_str_to_date::s_singleton;

Item*
Create_func_str_to_date::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_str_to_date(POS(), arg1, arg2);
}


Create_func_strcmp Create_func_strcmp::s_singleton;

Item*
Create_func_strcmp::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_strcmp(POS(), arg1, arg2);
}


Create_func_substr_index Create_func_substr_index::s_singleton;

Item*
Create_func_substr_index::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_substr_index(POS(), arg1, arg2,
                                                    arg3);
}


Create_func_subtime Create_func_subtime::s_singleton;

Item*
Create_func_subtime::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_add_time(POS(), arg1, arg2, 0, 1);
}


Create_func_tan Create_func_tan::s_singleton;

Item*
Create_func_tan::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_tan(POS(), arg1);
}


Create_func_time_format Create_func_time_format::s_singleton;

Item*
Create_func_time_format::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_date_format(POS(), arg1, arg2, 1);
}


Create_func_time_to_sec Create_func_time_to_sec::s_singleton;

Item*
Create_func_time_to_sec::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_time_to_sec(POS(), arg1);
}


Create_func_timediff Create_func_timediff::s_singleton;

Item*
Create_func_timediff::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_timediff(POS(), arg1, arg2);
}


Create_func_to_base64 Create_func_to_base64::s_singleton;

Item*
Create_func_to_base64::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_to_base64(POS(), arg1);
}


Create_func_to_days Create_func_to_days::s_singleton;

Item*
Create_func_to_days::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_to_days(POS(), arg1);
}


Create_func_to_seconds Create_func_to_seconds::s_singleton;

Item*
Create_func_to_seconds::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_to_seconds(POS(), arg1);
}


Create_func_mbr_touches Create_func_mbr_touches::s_singleton;

Item*
Create_func_mbr_touches::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root)
    Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                              Item_func::SP_TOUCHES_FUNC);
}


Create_func_touches Create_func_touches::s_singleton;

Item*
Create_func_touches::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_TOUCHES_FUNC);
}


Create_func_upper Create_func_upper::s_singleton;

Item*
Create_func_upper::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_upper(POS(), arg1);
}


Create_func_uncompress Create_func_uncompress::s_singleton;

Item*
Create_func_uncompress::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_uncompress(POS(), arg1);
}


Create_func_uncompressed_length Create_func_uncompressed_length::s_singleton;

Item*
Create_func_uncompressed_length::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_uncompressed_length(POS(), arg1);
}


Create_func_unhex Create_func_unhex::s_singleton;

Item*
Create_func_unhex::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_unhex(POS(), arg1);
}


Create_func_unix_timestamp Create_func_unix_timestamp::s_singleton;

Item*
Create_func_unix_timestamp::create_native(THD *thd, LEX_STRING name,
                                          PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 0:
  {
    func= new (thd->mem_root) Item_func_unix_timestamp(POS());
    break;
  }
  case 1:
  {
    Item *param_1= item_list->pop_front();
    func= new (thd->mem_root) Item_func_unix_timestamp(POS(), param_1);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


Create_func_uuid Create_func_uuid::s_singleton;

Item*
Create_func_uuid::create(THD *thd)
{
  return new (thd->mem_root) Item_func_uuid(POS());
}


Create_func_uuid_short Create_func_uuid_short::s_singleton;

Item*
Create_func_uuid_short::create(THD *thd)
{
  return new (thd->mem_root) Item_func_uuid_short(POS());
}


Create_func_validate_password_strength
                     Create_func_validate_password_strength::s_singleton;

Item*
Create_func_validate_password_strength::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_validate_password_strength(POS(),
                                                                  arg1);
}


Create_func_version Create_func_version::s_singleton;

Item*
Create_func_version::create(THD *thd)
{
  return new (thd->mem_root) Item_func_version(POS());
}


Create_func_weekday Create_func_weekday::s_singleton;

Item*
Create_func_weekday::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_weekday(POS(), arg1, 0);
}


Create_func_weekofyear Create_func_weekofyear::s_singleton;

Item*
Create_func_weekofyear::create(THD *thd, Item *arg1)
{
  Item *i1= new (thd->mem_root) Item_int(POS(), NAME_STRING("0"), 3, 1);
  return new (thd->mem_root) Item_func_week(POS(), arg1, i1);
}


Create_func_mbr_within Create_func_mbr_within::s_singleton;

Item*
Create_func_mbr_within::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_mbr_rel(POS(), arg1, arg2,
                               Item_func::SP_WITHIN_FUNC);
}


Create_func_within Create_func_within::s_singleton;

Item*
Create_func_within::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_spatial_rel(POS(), arg1, arg2,
                                                   Item_func::SP_WITHIN_FUNC);
}


Create_func_x Create_func_x::s_singleton;

Item*
Create_func_x::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_x(POS(), arg1);
}


Create_func_xml_extractvalue Create_func_xml_extractvalue::s_singleton;

Item*
Create_func_xml_extractvalue::create(THD *thd, Item *arg1, Item *arg2)
{
  return new (thd->mem_root) Item_func_xml_extractvalue(POS(), arg1,
                                                        arg2);
}


Create_func_xml_update Create_func_xml_update::s_singleton;

Item*
Create_func_xml_update::create(THD *thd, Item *arg1, Item *arg2, Item *arg3)
{
  return new (thd->mem_root) Item_func_xml_update(POS(), arg1, arg2,
                                                  arg3);
}


Create_func_y Create_func_y::s_singleton;

Item*
Create_func_y::create(THD *thd, Item *arg1)
{
  return new (thd->mem_root) Item_func_y(POS(), arg1);
}


Create_func_year_week Create_func_year_week::s_singleton;

Item*
Create_func_year_week::create_native(THD *thd, LEX_STRING name,
                                     PT_item_list *item_list)
{
  Item *func= NULL;
  int arg_count= 0;

  if (item_list != NULL)
    arg_count= item_list->elements();

  switch (arg_count) {
  case 1:
  {
    Item *param_1= item_list->pop_front();
    Item *i0= new (thd->mem_root) Item_int_0(POS());
    func= new (thd->mem_root) Item_func_yearweek(POS(), param_1, i0);
    break;
  }
  case 2:
  {
    Item *param_1= item_list->pop_front();
    Item *param_2= item_list->pop_front();
    func= new (thd->mem_root) Item_func_yearweek(POS(), param_1, param_2);
    break;
  }
  default:
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    break;
  }
  }

  return func;
}


struct Native_func_registry
{
  LEX_STRING name;
  Create_func *builder;
};

#define BUILDER(F) & F::s_singleton

#define GEOM_BUILDER(F) & F::s_singleton

/*
  MySQL native functions.
  MAINTAINER:
  - Keep sorted for human lookup. At runtime, a hash table is used.
  - do **NOT** conditionally (#ifdef, #ifndef) define a function *NAME*:
    doing so will cause user code that works against a --without-XYZ binary
    to fail with name collisions against a --with-XYZ binary.
    Use something similar to GEOM_BUILDER instead.
  - keep 1 line per entry, it makes grep | sort easier
*/

static Native_func_registry func_array[] =
{
  { { C_STRING_WITH_LEN("ABS") }, BUILDER(Create_func_abs)},
  { { C_STRING_WITH_LEN("ACOS") }, BUILDER(Create_func_acos)},
  { { C_STRING_WITH_LEN("ADDTIME") }, BUILDER(Create_func_addtime)},
  { { C_STRING_WITH_LEN("AES_DECRYPT") }, BUILDER(Create_func_aes_decrypt)},
  { { C_STRING_WITH_LEN("AES_ENCRYPT") }, BUILDER(Create_func_aes_encrypt)},
  { { C_STRING_WITH_LEN("ANY_VALUE") }, BUILDER(Create_func_any_value)},
  { { C_STRING_WITH_LEN("AREA") }, GEOM_BUILDER(Create_func_area_deprecated)},
  { { C_STRING_WITH_LEN("ASBINARY") }, GEOM_BUILDER(Create_func_as_binary_deprecated)},
  { { C_STRING_WITH_LEN("ASIN") }, BUILDER(Create_func_asin)},
  { { C_STRING_WITH_LEN("ASTEXT") }, GEOM_BUILDER(Create_func_as_text_deprecated)},
  { { C_STRING_WITH_LEN("ASWKB") }, GEOM_BUILDER(Create_func_as_wkb_deprecated)},
  { { C_STRING_WITH_LEN("ASWKT") }, GEOM_BUILDER(Create_func_as_wkt_deprecated)},
  { { C_STRING_WITH_LEN("ATAN") }, BUILDER(Create_func_atan)},
  { { C_STRING_WITH_LEN("ATAN2") }, BUILDER(Create_func_atan)},
  { { C_STRING_WITH_LEN("BENCHMARK") }, BUILDER(Create_func_benchmark)},
  { { C_STRING_WITH_LEN("BIN") }, BUILDER(Create_func_bin)},
  { { C_STRING_WITH_LEN("BIT_COUNT") }, BUILDER(Create_func_bit_count)},
  { { C_STRING_WITH_LEN("BUFFER") }, GEOM_BUILDER(Create_func_buffer_deprecated)},
  { { C_STRING_WITH_LEN("BIT_LENGTH") }, BUILDER(Create_func_bit_length)},
  { { C_STRING_WITH_LEN("CEIL") }, BUILDER(Create_func_ceiling)},
  { { C_STRING_WITH_LEN("CEILING") }, BUILDER(Create_func_ceiling)},
  { { C_STRING_WITH_LEN("CENTROID") }, GEOM_BUILDER(Create_func_centroid_deprecated)},
  { { C_STRING_WITH_LEN("CHARACTER_LENGTH") }, BUILDER(Create_func_char_length)},
  { { C_STRING_WITH_LEN("CHAR_LENGTH") }, BUILDER(Create_func_char_length)},
  { { C_STRING_WITH_LEN("COERCIBILITY") }, BUILDER(Create_func_coercibility)},
  { { C_STRING_WITH_LEN("COMPRESS") }, BUILDER(Create_func_compress)},
  { { C_STRING_WITH_LEN("CONCAT") }, BUILDER(Create_func_concat)},
  { { C_STRING_WITH_LEN("CONCAT_WS") }, BUILDER(Create_func_concat_ws)},
  { { C_STRING_WITH_LEN("CONNECTION_ID") }, BUILDER(Create_func_connection_id)},
  { { C_STRING_WITH_LEN("CONV") }, BUILDER(Create_func_conv)},
  { { C_STRING_WITH_LEN("CONVERT_TZ") }, BUILDER(Create_func_convert_tz)},
  { { C_STRING_WITH_LEN("CONVEXHULL") }, GEOM_BUILDER(Create_func_convex_hull_deprecated)},
  { { C_STRING_WITH_LEN("COS") }, BUILDER(Create_func_cos)},
  { { C_STRING_WITH_LEN("COT") }, BUILDER(Create_func_cot)},
  { { C_STRING_WITH_LEN("CRC32") }, BUILDER(Create_func_crc32)},
  { { C_STRING_WITH_LEN("CROSSES") }, GEOM_BUILDER(Create_func_crosses_deprecated)},
  { { C_STRING_WITH_LEN("DATEDIFF") }, BUILDER(Create_func_datediff)},
  { { C_STRING_WITH_LEN("DATE_FORMAT") }, BUILDER(Create_func_date_format)},
  { { C_STRING_WITH_LEN("DAYNAME") }, BUILDER(Create_func_dayname)},
  { { C_STRING_WITH_LEN("DAYOFMONTH") }, BUILDER(Create_func_dayofmonth)},
  { { C_STRING_WITH_LEN("DAYOFWEEK") }, BUILDER(Create_func_dayofweek)},
  { { C_STRING_WITH_LEN("DAYOFYEAR") }, BUILDER(Create_func_dayofyear)},
  { { C_STRING_WITH_LEN("DECODE") }, BUILDER(Create_func_decode)},
  { { C_STRING_WITH_LEN("DEGREES") }, BUILDER(Create_func_degrees)},
  { { C_STRING_WITH_LEN("DES_DECRYPT") }, BUILDER(Create_func_des_decrypt)},
  { { C_STRING_WITH_LEN("DES_ENCRYPT") }, BUILDER(Create_func_des_encrypt)},
  { { C_STRING_WITH_LEN("DIMENSION") }, GEOM_BUILDER(Create_func_dimension_deprecated)},
  { { C_STRING_WITH_LEN("DISJOINT") }, GEOM_BUILDER(Create_func_disjoint_deprecated)},
  { { C_STRING_WITH_LEN("DISTANCE") }, GEOM_BUILDER(Create_func_distance_deprecated)},
  { { C_STRING_WITH_LEN("ELT") }, BUILDER(Create_func_elt)},
  { { C_STRING_WITH_LEN("ENCODE") }, BUILDER(Create_func_encode)},
  { { C_STRING_WITH_LEN("ENCRYPT") }, BUILDER(Create_func_encrypt)},
  { { C_STRING_WITH_LEN("ENDPOINT") }, GEOM_BUILDER(Create_func_endpoint_deprecated)},
  { { C_STRING_WITH_LEN("ENVELOPE") }, GEOM_BUILDER(Create_func_envelope_deprecated)},
  { { C_STRING_WITH_LEN("EQUALS") }, GEOM_BUILDER(Create_func_equals_deprecated)},
  { { C_STRING_WITH_LEN("EXP") }, BUILDER(Create_func_exp)},
  { { C_STRING_WITH_LEN("EXPORT_SET") }, BUILDER(Create_func_export_set)},
  { { C_STRING_WITH_LEN("EXTERIORRING") }, GEOM_BUILDER(Create_func_exteriorring_deprecated)},
  { { C_STRING_WITH_LEN("EXTRACTVALUE") }, BUILDER(Create_func_xml_extractvalue)},
  { { C_STRING_WITH_LEN("FIELD") }, BUILDER(Create_func_field)},
  { { C_STRING_WITH_LEN("FIND_IN_SET") }, BUILDER(Create_func_find_in_set)},
  { { C_STRING_WITH_LEN("FLOOR") }, BUILDER(Create_func_floor)},
  { { C_STRING_WITH_LEN("FOUND_ROWS") }, BUILDER(Create_func_found_rows)},
  { { C_STRING_WITH_LEN("FROM_BASE64") }, BUILDER(Create_func_from_base64)},
  { { C_STRING_WITH_LEN("FROM_DAYS") }, BUILDER(Create_func_from_days)},
  { { C_STRING_WITH_LEN("FROM_UNIXTIME") }, BUILDER(Create_func_from_unixtime)},
  { { C_STRING_WITH_LEN("GEOMCOLLFROMTEXT") }, GEOM_BUILDER(Create_func_geomcollfromtext_deprecated)},
  { { C_STRING_WITH_LEN("GEOMCOLLFROMWKB") }, GEOM_BUILDER(Create_func_geomcollfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYCOLLECTIONFROMTEXT") }, GEOM_BUILDER(Create_func_geometrycollectionfromtext_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYCOLLECTIONFROMWKB") }, GEOM_BUILDER(Create_func_geometrycollectionfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYFROMTEXT") }, GEOM_BUILDER(Create_func_geometryfromtext_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYFROMWKB") }, GEOM_BUILDER(Create_func_geometryfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYN") }, GEOM_BUILDER(Create_func_geometryn_deprecated)},
  { { C_STRING_WITH_LEN("GEOMETRYTYPE") }, GEOM_BUILDER(Create_func_geometry_type_deprecated)},
  { { C_STRING_WITH_LEN("GEOMFROMTEXT") }, GEOM_BUILDER(Create_func_geomfromtext_deprecated)},
  { { C_STRING_WITH_LEN("GEOMFROMWKB") }, GEOM_BUILDER(Create_func_geomfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("GET_LOCK") }, BUILDER(Create_func_get_lock)},
  { { C_STRING_WITH_LEN("GLENGTH") }, GEOM_BUILDER(Create_func_glength_deprecated)},
  { { C_STRING_WITH_LEN("GREATEST") }, BUILDER(Create_func_greatest)},
  { { C_STRING_WITH_LEN("GTID_SUBTRACT") }, BUILDER(Create_func_gtid_subtract) },
  { { C_STRING_WITH_LEN("GTID_SUBSET") }, BUILDER(Create_func_gtid_subset) },
  { { C_STRING_WITH_LEN("HEX") }, BUILDER(Create_func_hex)},
  { { C_STRING_WITH_LEN("IFNULL") }, BUILDER(Create_func_ifnull)},
  { { C_STRING_WITH_LEN("INET_ATON") }, BUILDER(Create_func_inet_aton)},
  { { C_STRING_WITH_LEN("INET_NTOA") }, BUILDER(Create_func_inet_ntoa)},
  { { C_STRING_WITH_LEN("INET6_ATON") }, BUILDER(Create_func_inet6_aton)},
  { { C_STRING_WITH_LEN("INET6_NTOA") }, BUILDER(Create_func_inet6_ntoa)},
  { { C_STRING_WITH_LEN("IS_IPV4") }, BUILDER(Create_func_is_ipv4)},
  { { C_STRING_WITH_LEN("IS_IPV6") }, BUILDER(Create_func_is_ipv6)},
  { { C_STRING_WITH_LEN("IS_IPV4_COMPAT") }, BUILDER(Create_func_is_ipv4_compat)},
  { { C_STRING_WITH_LEN("IS_IPV4_MAPPED") }, BUILDER(Create_func_is_ipv4_mapped)},
  { { C_STRING_WITH_LEN("INSTR") }, BUILDER(Create_func_instr)},
  { { C_STRING_WITH_LEN("INTERIORRINGN") }, GEOM_BUILDER(Create_func_interiorringn_deprecated)},
  { { C_STRING_WITH_LEN("INTERSECTS") }, GEOM_BUILDER(Create_func_intersects_deprecated)},
  { { C_STRING_WITH_LEN("ISCLOSED") }, GEOM_BUILDER(Create_func_isclosed_deprecated)},
  { { C_STRING_WITH_LEN("ISEMPTY") }, GEOM_BUILDER(Create_func_isempty_deprecated)},
  { { C_STRING_WITH_LEN("ISNULL") }, BUILDER(Create_func_isnull)},
  { { C_STRING_WITH_LEN("ISSIMPLE") }, GEOM_BUILDER(Create_func_issimple_deprecated)},
  { { C_STRING_WITH_LEN("JSON_VALID") }, BUILDER(Create_func_json_valid)},
  { { C_STRING_WITH_LEN("JSON_CONTAINS") }, BUILDER(Create_func_json_contains)},
  { { C_STRING_WITH_LEN("JSON_CONTAINS_PATH") }, BUILDER(Create_func_json_contains_path)},
  { { C_STRING_WITH_LEN("JSON_LENGTH") }, BUILDER(Create_func_json_length)},
  { { C_STRING_WITH_LEN("JSON_DEPTH") }, BUILDER(Create_func_json_depth)},
  { { C_STRING_WITH_LEN("JSON_PRETTY") }, BUILDER(Create_func_json_pretty)},
  { { C_STRING_WITH_LEN("JSON_TYPE") }, BUILDER(Create_func_json_type)},
  { { C_STRING_WITH_LEN("JSON_KEYS") }, BUILDER(Create_func_json_keys)},
  { { C_STRING_WITH_LEN("JSON_EXTRACT") }, BUILDER(Create_func_json_extract)},
  { { C_STRING_WITH_LEN("JSON_ARRAY_APPEND") }, BUILDER(Create_func_json_array_append)},
  { { C_STRING_WITH_LEN("JSON_INSERT") }, BUILDER(Create_func_json_insert)},
  { { C_STRING_WITH_LEN("JSON_ARRAY_INSERT") }, BUILDER(Create_func_json_array_insert)},
  { { C_STRING_WITH_LEN("JSON_OBJECT") }, BUILDER(Create_func_json_row_object)},
  { { C_STRING_WITH_LEN("JSON_SEARCH") }, BUILDER(Create_func_json_search)},
  { { C_STRING_WITH_LEN("JSON_SET") }, BUILDER(Create_func_json_set)},
  { { C_STRING_WITH_LEN("JSON_REPLACE") }, BUILDER(Create_func_json_replace)},
  { { C_STRING_WITH_LEN("JSON_ARRAY") }, BUILDER(Create_func_json_array)},
  { { C_STRING_WITH_LEN("JSON_REMOVE") }, BUILDER(Create_func_json_remove)},
  { { C_STRING_WITH_LEN("JSON_MERGE") }, BUILDER(Create_func_json_merge)},
  { { C_STRING_WITH_LEN("JSON_MERGE_PATCH") }, BUILDER(Create_func_json_merge_patch)},
  { { C_STRING_WITH_LEN("JSON_MERGE_PRESERVE") }, BUILDER(Create_func_json_merge_preserve)},
  { { C_STRING_WITH_LEN("JSON_QUOTE") }, BUILDER(Create_func_json_quote)},
  { { C_STRING_WITH_LEN("JSON_STORAGE_SIZE") }, BUILDER(Create_func_json_storage_size)},
  { { C_STRING_WITH_LEN("JSON_UNQUOTE") }, BUILDER(Create_func_json_unquote)},
  { { C_STRING_WITH_LEN("IS_FREE_LOCK") }, BUILDER(Create_func_is_free_lock)},
  { { C_STRING_WITH_LEN("IS_USED_LOCK") }, BUILDER(Create_func_is_used_lock)},
  { { C_STRING_WITH_LEN("LAST_DAY") }, BUILDER(Create_func_last_day)},
  { { C_STRING_WITH_LEN("LAST_INSERT_ID") }, BUILDER(Create_func_last_insert_id)},
  { { C_STRING_WITH_LEN("LCASE") }, BUILDER(Create_func_lower)},
  { { C_STRING_WITH_LEN("LEAST") }, BUILDER(Create_func_least)},
  { { C_STRING_WITH_LEN("LENGTH") }, BUILDER(Create_func_length)},
#ifndef DBUG_OFF
  { { C_STRING_WITH_LEN("LIKE_RANGE_MIN") }, BUILDER(Create_func_like_range_min)},
  { { C_STRING_WITH_LEN("LIKE_RANGE_MAX") }, BUILDER(Create_func_like_range_max)},
#endif
  { { C_STRING_WITH_LEN("LINEFROMTEXT") }, GEOM_BUILDER(Create_func_linefromtext_deprecated)},
  { { C_STRING_WITH_LEN("LINEFROMWKB") }, GEOM_BUILDER(Create_func_linefromwkb_deprecated)},
  { { C_STRING_WITH_LEN("LINESTRINGFROMTEXT") }, GEOM_BUILDER(Create_func_linestringfromtext_deprecated)},
  { { C_STRING_WITH_LEN("LINESTRINGFROMWKB") }, GEOM_BUILDER(Create_func_linestringfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("LN") }, BUILDER(Create_func_ln)},
  { { C_STRING_WITH_LEN("LOAD_FILE") }, BUILDER(Create_func_load_file)},
  { { C_STRING_WITH_LEN("LOCATE") }, BUILDER(Create_func_locate)},
  { { C_STRING_WITH_LEN("LOG") }, BUILDER(Create_func_log)},
  { { C_STRING_WITH_LEN("LOG10") }, BUILDER(Create_func_log10)},
  { { C_STRING_WITH_LEN("LOG2") }, BUILDER(Create_func_log2)},
  { { C_STRING_WITH_LEN("LOWER") }, BUILDER(Create_func_lower)},
  { { C_STRING_WITH_LEN("LPAD") }, BUILDER(Create_func_lpad)},
  { { C_STRING_WITH_LEN("LTRIM") }, BUILDER(Create_func_ltrim)},
  { { C_STRING_WITH_LEN("MAKEDATE") }, BUILDER(Create_func_makedate)},
  { { C_STRING_WITH_LEN("MAKETIME") }, BUILDER(Create_func_maketime)},
  { { C_STRING_WITH_LEN("MAKE_SET") }, BUILDER(Create_func_make_set)},
  { { C_STRING_WITH_LEN("MASTER_POS_WAIT") }, BUILDER(Create_func_master_pos_wait)},
  { { C_STRING_WITH_LEN("MBRCONTAINS") }, GEOM_BUILDER(Create_func_mbr_contains)},
  { { C_STRING_WITH_LEN("MBRCOVEREDBY") }, GEOM_BUILDER(Create_func_mbr_covered_by)},
  { { C_STRING_WITH_LEN("MBRCOVERS") }, GEOM_BUILDER(Create_func_mbr_covers)},
  { { C_STRING_WITH_LEN("MBRDISJOINT") }, GEOM_BUILDER(Create_func_mbr_disjoint)},
  { { C_STRING_WITH_LEN("MBREQUAL") }, GEOM_BUILDER(Create_func_mbr_equal_deprecated)},
  { { C_STRING_WITH_LEN("MBREQUALS") }, GEOM_BUILDER(Create_func_mbr_equals)},
  { { C_STRING_WITH_LEN("MBRINTERSECTS") }, GEOM_BUILDER(Create_func_mbr_intersects)},
  { { C_STRING_WITH_LEN("MBROVERLAPS") }, GEOM_BUILDER(Create_func_mbr_overlaps)},
  { { C_STRING_WITH_LEN("MBRTOUCHES") }, GEOM_BUILDER(Create_func_mbr_touches)},
  { { C_STRING_WITH_LEN("MBRWITHIN") }, GEOM_BUILDER(Create_func_mbr_within)},
  { { C_STRING_WITH_LEN("MD5") }, BUILDER(Create_func_md5)},
  { { C_STRING_WITH_LEN("MLINEFROMTEXT") }, GEOM_BUILDER(Create_func_mlinefromtext_deprecated)},
  { { C_STRING_WITH_LEN("MLINEFROMWKB") }, GEOM_BUILDER(Create_func_mlinefromwkb_deprecated)},
  { { C_STRING_WITH_LEN("MONTHNAME") }, BUILDER(Create_func_monthname)},
  { { C_STRING_WITH_LEN("MPOINTFROMTEXT") }, GEOM_BUILDER(Create_func_mpointfromtext_deprecated)},
  { { C_STRING_WITH_LEN("MPOINTFROMWKB") }, GEOM_BUILDER(Create_func_mpointfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("MPOLYFROMTEXT") }, GEOM_BUILDER(Create_func_mpolyfromtext_deprecated)},
  { { C_STRING_WITH_LEN("MPOLYFROMWKB") }, GEOM_BUILDER(Create_func_mpolyfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("MULTILINESTRINGFROMTEXT") }, GEOM_BUILDER(Create_func_multilinestringfromtext_deprecated)},
  { { C_STRING_WITH_LEN("MULTILINESTRINGFROMWKB") }, GEOM_BUILDER(Create_func_multilinestringfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("MULTIPOINTFROMTEXT") }, GEOM_BUILDER(Create_func_multipointfromtext_deprecated)},
  { { C_STRING_WITH_LEN("MULTIPOINTFROMWKB") }, GEOM_BUILDER(Create_func_multipointfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("MULTIPOLYGONFROMTEXT") }, GEOM_BUILDER(Create_func_multipolygonfromtext_deprecated)},
  { { C_STRING_WITH_LEN("MULTIPOLYGONFROMWKB") }, GEOM_BUILDER(Create_func_multipolygonfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("NAME_CONST") }, BUILDER(Create_func_name_const)},
  { { C_STRING_WITH_LEN("NULLIF") }, BUILDER(Create_func_nullif)},
  { { C_STRING_WITH_LEN("NUMGEOMETRIES") }, GEOM_BUILDER(Create_func_numgeometries_deprecated)},
  { { C_STRING_WITH_LEN("NUMINTERIORRINGS") }, GEOM_BUILDER(Create_func_numinteriorring_deprecated)},
  { { C_STRING_WITH_LEN("NUMPOINTS") }, GEOM_BUILDER(Create_func_numpoints_deprecated)},
  { { C_STRING_WITH_LEN("OCT") }, BUILDER(Create_func_oct)},
  { { C_STRING_WITH_LEN("OCTET_LENGTH") }, BUILDER(Create_func_length)},
  { { C_STRING_WITH_LEN("ORD") }, BUILDER(Create_func_ord)},
  { { C_STRING_WITH_LEN("OVERLAPS") }, GEOM_BUILDER(Create_func_mbr_overlaps_deprecated)},
  { { C_STRING_WITH_LEN("PERIOD_ADD") }, BUILDER(Create_func_period_add)},
  { { C_STRING_WITH_LEN("PERIOD_DIFF") }, BUILDER(Create_func_period_diff)},
  { { C_STRING_WITH_LEN("PI") }, BUILDER(Create_func_pi)},
  { { C_STRING_WITH_LEN("POINTFROMTEXT") }, GEOM_BUILDER(Create_func_pointfromtext_deprecated)},
  { { C_STRING_WITH_LEN("POINTFROMWKB") }, GEOM_BUILDER(Create_func_pointfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("POINTN") }, GEOM_BUILDER(Create_func_pointn_deprecated)},
  { { C_STRING_WITH_LEN("POLYFROMTEXT") }, GEOM_BUILDER(Create_func_polyfromtext_deprecated)},
  { { C_STRING_WITH_LEN("POLYFROMWKB") }, GEOM_BUILDER(Create_func_polyfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("POLYGONFROMTEXT") }, GEOM_BUILDER(Create_func_polygonfromtext_deprecated)},
  { { C_STRING_WITH_LEN("POLYGONFROMWKB") }, GEOM_BUILDER(Create_func_polygonfromwkb_deprecated)},
  { { C_STRING_WITH_LEN("POW") }, BUILDER(Create_func_pow)},
  { { C_STRING_WITH_LEN("POWER") }, BUILDER(Create_func_pow)},
  { { C_STRING_WITH_LEN("QUOTE") }, BUILDER(Create_func_quote)},
  { { C_STRING_WITH_LEN("RADIANS") }, BUILDER(Create_func_radians)},
  { { C_STRING_WITH_LEN("RAND") }, BUILDER(Create_func_rand)},
  { { C_STRING_WITH_LEN("RANDOM_BYTES") }, BUILDER(Create_func_random_bytes) },
  { { C_STRING_WITH_LEN("RELEASE_ALL_LOCKS") }, BUILDER(Create_func_release_all_locks) },
  { { C_STRING_WITH_LEN("RELEASE_LOCK") }, BUILDER(Create_func_release_lock) },
  { { C_STRING_WITH_LEN("REVERSE") }, BUILDER(Create_func_reverse)},
  { { C_STRING_WITH_LEN("ROUND") }, BUILDER(Create_func_round)},
  { { C_STRING_WITH_LEN("RPAD") }, BUILDER(Create_func_rpad)},
  { { C_STRING_WITH_LEN("RTRIM") }, BUILDER(Create_func_rtrim)},
  { { C_STRING_WITH_LEN("SEC_TO_TIME") }, BUILDER(Create_func_sec_to_time)},
  { { C_STRING_WITH_LEN("SHA") }, BUILDER(Create_func_sha)},
  { { C_STRING_WITH_LEN("SHA1") }, BUILDER(Create_func_sha)},
  { { C_STRING_WITH_LEN("SHA2") }, BUILDER(Create_func_sha2)},
  { { C_STRING_WITH_LEN("SIGN") }, BUILDER(Create_func_sign)},
  { { C_STRING_WITH_LEN("SIN") }, BUILDER(Create_func_sin)},
  { { C_STRING_WITH_LEN("SLEEP") }, BUILDER(Create_func_sleep)},
  { { C_STRING_WITH_LEN("SOUNDEX") }, BUILDER(Create_func_soundex)},
  { { C_STRING_WITH_LEN("SPACE") }, BUILDER(Create_func_space)},
  { { C_STRING_WITH_LEN("WAIT_FOR_EXECUTED_GTID_SET") }, BUILDER(Create_func_executed_gtid_set_wait)},
  { { C_STRING_WITH_LEN("WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS") }, BUILDER(Create_func_master_gtid_set_wait)},
  { { C_STRING_WITH_LEN("SQRT") }, BUILDER(Create_func_sqrt)},
  { { C_STRING_WITH_LEN("SRID") }, GEOM_BUILDER(Create_func_srid_deprecated)},
  { { C_STRING_WITH_LEN("STARTPOINT") }, GEOM_BUILDER(Create_func_startpoint_deprecated)},
  { { C_STRING_WITH_LEN("STRCMP") }, BUILDER(Create_func_strcmp)},
  { { C_STRING_WITH_LEN("STR_TO_DATE") }, BUILDER(Create_func_str_to_date)},
  { { C_STRING_WITH_LEN("ST_AREA") }, GEOM_BUILDER(Create_func_area)},
  { { C_STRING_WITH_LEN("ST_ASBINARY") }, GEOM_BUILDER(Create_func_as_wkb)},
  { { C_STRING_WITH_LEN("ST_ASGEOJSON") }, GEOM_BUILDER(Create_func_as_geojson)},
  { { C_STRING_WITH_LEN("ST_ASTEXT") }, GEOM_BUILDER(Create_func_as_wkt)},
  { { C_STRING_WITH_LEN("ST_ASWKB") }, GEOM_BUILDER(Create_func_as_wkb)},
  { { C_STRING_WITH_LEN("ST_ASWKT") }, GEOM_BUILDER(Create_func_as_wkt)},
  { { C_STRING_WITH_LEN("ST_BUFFER") }, GEOM_BUILDER(Create_func_buffer)},
  { { C_STRING_WITH_LEN("ST_BUFFER_STRATEGY") }, GEOM_BUILDER(Create_func_buffer_strategy)},
  { { C_STRING_WITH_LEN("ST_CENTROID") }, GEOM_BUILDER(Create_func_centroid)},
  { { C_STRING_WITH_LEN("ST_CONTAINS") }, GEOM_BUILDER(Create_func_contains)},
  { { C_STRING_WITH_LEN("ST_CONVEXHULL") }, GEOM_BUILDER(Create_func_convex_hull)},
  { { C_STRING_WITH_LEN("ST_CROSSES") }, GEOM_BUILDER(Create_func_crosses)},
  { { C_STRING_WITH_LEN("ST_DIFFERENCE") }, GEOM_BUILDER(Create_func_difference)},
  { { C_STRING_WITH_LEN("ST_DIMENSION") }, GEOM_BUILDER(Create_func_dimension)},
  { { C_STRING_WITH_LEN("ST_DISJOINT") }, GEOM_BUILDER(Create_func_disjoint)},
  { { C_STRING_WITH_LEN("ST_DISTANCE") }, GEOM_BUILDER(Create_func_distance)},
  { { C_STRING_WITH_LEN("ST_DISTANCE_SPHERE") }, GEOM_BUILDER(Create_func_distance_sphere)},
  { { C_STRING_WITH_LEN("ST_ENDPOINT") }, GEOM_BUILDER(Create_func_endpoint)},
  { { C_STRING_WITH_LEN("ST_ENVELOPE") }, GEOM_BUILDER(Create_func_envelope)},
  { { C_STRING_WITH_LEN("ST_EQUALS") }, GEOM_BUILDER(Create_func_equals)},
  { { C_STRING_WITH_LEN("ST_EXTERIORRING") }, GEOM_BUILDER(Create_func_exteriorring)},
  { { C_STRING_WITH_LEN("ST_GEOHASH") }, GEOM_BUILDER(Create_func_geohash)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYN") }, GEOM_BUILDER(Create_func_geometryn)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYTYPE") }, GEOM_BUILDER(Create_func_geometry_type)},
  { { C_STRING_WITH_LEN("ST_GEOMFROMGEOJSON") }, GEOM_BUILDER(Create_func_geomfromgeojson)},
  { { C_STRING_WITH_LEN("ST_GEOMFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_INTERIORRINGN") }, GEOM_BUILDER(Create_func_interiorringn)},
  { { C_STRING_WITH_LEN("ST_INTERSECTS") }, GEOM_BUILDER(Create_func_intersects)},
  { { C_STRING_WITH_LEN("ST_INTERSECTION") }, GEOM_BUILDER(Create_func_intersection)},
  { { C_STRING_WITH_LEN("ST_ISCLOSED") }, GEOM_BUILDER(Create_func_isclosed)},
  { { C_STRING_WITH_LEN("ST_ISEMPTY") }, GEOM_BUILDER(Create_func_isempty)},
  { { C_STRING_WITH_LEN("ST_ISSIMPLE") }, GEOM_BUILDER(Create_func_issimple)},
  { { C_STRING_WITH_LEN("ST_ISVALID") }, GEOM_BUILDER(Create_func_isvalid)},
  { { C_STRING_WITH_LEN("ST_LATFROMGEOHASH") }, GEOM_BUILDER(Create_func_latfromgeohash)},
  { { C_STRING_WITH_LEN("ST_LENGTH") }, GEOM_BUILDER(Create_func_glength)},
  { { C_STRING_WITH_LEN("ST_LINEFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_LINEFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_LONGFROMGEOHASH") }, GEOM_BUILDER(Create_func_longfromgeohash)},
  { { C_STRING_WITH_LEN("ST_MAKEENVELOPE") }, GEOM_BUILDER(Create_func_make_envelope)},
  { { C_STRING_WITH_LEN("ST_MLINEFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MLINEFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_MPOINTFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MPOINTFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_MPOLYFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MPOLYFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_MULTILINESTRINGFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MULTILINESTRINGFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_MULTIPOINTFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MULTIPOINTFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_MULTIPOLYGONFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_MULTIPOLYGONFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_NUMGEOMETRIES") }, GEOM_BUILDER(Create_func_numgeometries)},
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRING") }, GEOM_BUILDER(Create_func_numinteriorring)},
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRINGS") }, GEOM_BUILDER(Create_func_numinteriorring)},
  { { C_STRING_WITH_LEN("ST_NUMPOINTS") }, GEOM_BUILDER(Create_func_numpoints)},
  { { C_STRING_WITH_LEN("ST_OVERLAPS") }, GEOM_BUILDER(Create_func_overlaps)},
  { { C_STRING_WITH_LEN("ST_POINTFROMGEOHASH") }, GEOM_BUILDER(Create_func_pointfromgeohash)},
  { { C_STRING_WITH_LEN("ST_POINTFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POINTFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_POINTN") }, GEOM_BUILDER(Create_func_pointn)},
  { { C_STRING_WITH_LEN("ST_POLYFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POLYFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_POLYGONFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POLYGONFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_SIMPLIFY") }, GEOM_BUILDER(Create_func_simplify)},
  { { C_STRING_WITH_LEN("ST_SRID") }, GEOM_BUILDER(Create_func_srid)},
  { { C_STRING_WITH_LEN("ST_STARTPOINT") }, GEOM_BUILDER(Create_func_startpoint)},
  { { C_STRING_WITH_LEN("ST_SYMDIFFERENCE") }, GEOM_BUILDER(Create_func_symdifference)},
  { { C_STRING_WITH_LEN("ST_TOUCHES") }, GEOM_BUILDER(Create_func_touches)},
  { { C_STRING_WITH_LEN("ST_UNION") }, GEOM_BUILDER(Create_func_union)},
  { { C_STRING_WITH_LEN("ST_VALIDATE") }, GEOM_BUILDER(Create_func_validate)},
  { { C_STRING_WITH_LEN("ST_WITHIN") }, GEOM_BUILDER(Create_func_within)},
  { { C_STRING_WITH_LEN("ST_X") }, GEOM_BUILDER(Create_func_x)},
  { { C_STRING_WITH_LEN("ST_Y") }, GEOM_BUILDER(Create_func_y)},
  { { C_STRING_WITH_LEN("SUBSTRING_INDEX") }, BUILDER(Create_func_substr_index)},
  { { C_STRING_WITH_LEN("SUBTIME") }, BUILDER(Create_func_subtime)},
  { { C_STRING_WITH_LEN("TAN") }, BUILDER(Create_func_tan)},
  { { C_STRING_WITH_LEN("TIMEDIFF") }, BUILDER(Create_func_timediff)},
  { { C_STRING_WITH_LEN("TIME_FORMAT") }, BUILDER(Create_func_time_format)},
  { { C_STRING_WITH_LEN("TIME_TO_SEC") }, BUILDER(Create_func_time_to_sec)},
  { { C_STRING_WITH_LEN("TOUCHES") }, GEOM_BUILDER(Create_func_touches_deprecated)},
  { { C_STRING_WITH_LEN("TO_BASE64") }, BUILDER(Create_func_to_base64)},
  { { C_STRING_WITH_LEN("TO_DAYS") }, BUILDER(Create_func_to_days)},
  { { C_STRING_WITH_LEN("TO_SECONDS") }, BUILDER(Create_func_to_seconds)},
  { { C_STRING_WITH_LEN("UCASE") }, BUILDER(Create_func_upper)},
  { { C_STRING_WITH_LEN("UNCOMPRESS") }, BUILDER(Create_func_uncompress)},
  { { C_STRING_WITH_LEN("UNCOMPRESSED_LENGTH") }, BUILDER(Create_func_uncompressed_length)},
  { { C_STRING_WITH_LEN("UNHEX") }, BUILDER(Create_func_unhex)},
  { { C_STRING_WITH_LEN("UNIX_TIMESTAMP") }, BUILDER(Create_func_unix_timestamp)},
  { { C_STRING_WITH_LEN("UPDATEXML") }, BUILDER(Create_func_xml_update)},
  { { C_STRING_WITH_LEN("UPPER") }, BUILDER(Create_func_upper)},
  { { C_STRING_WITH_LEN("UUID") }, BUILDER(Create_func_uuid)},
  { { C_STRING_WITH_LEN("UUID_SHORT") }, BUILDER(Create_func_uuid_short)},
  { { C_STRING_WITH_LEN("VALIDATE_PASSWORD_STRENGTH") }, BUILDER(Create_func_validate_password_strength)},
  { { C_STRING_WITH_LEN("VERSION") }, BUILDER(Create_func_version)},
  { { C_STRING_WITH_LEN("WEEKDAY") }, BUILDER(Create_func_weekday)},
  { { C_STRING_WITH_LEN("WEEKOFYEAR") }, BUILDER(Create_func_weekofyear)},
  { { C_STRING_WITH_LEN("WITHIN") }, GEOM_BUILDER(Create_func_within_deprecated)},
  { { C_STRING_WITH_LEN("X") }, GEOM_BUILDER(Create_func_x_deprecated)},
  { { C_STRING_WITH_LEN("Y") }, GEOM_BUILDER(Create_func_y_deprecated)},
  { { C_STRING_WITH_LEN("YEARWEEK") }, BUILDER(Create_func_year_week)},

  { {0, 0}, NULL}
};

static HASH native_functions_hash;

extern "C" uchar*
get_native_fct_hash_key(const uchar *buff, size_t *length,
                        my_bool /* unused */)
{
  Native_func_registry *func= (Native_func_registry*) buff;
  *length= func->name.length;
  return (uchar*) func->name.str;
}

/*
  Load the hash table for native functions.
  Note: this code is not thread safe, and is intended to be used at server
  startup only (before going multi-threaded)
*/

int item_create_init()
{
  Native_func_registry *func;

  DBUG_ENTER("item_create_init");

  if (my_hash_init(& native_functions_hash,
                   system_charset_info,
                   array_elements(func_array),
                   0,
                   0,
                   (my_hash_get_key) get_native_fct_hash_key,
                   NULL,                          /* Nothing to free */
                   MYF(0),
                   key_memory_native_functions))
    DBUG_RETURN(1);

  for (func= func_array; func->builder != NULL; func++)
  {
    if (my_hash_insert(& native_functions_hash, (uchar*) func))
      DBUG_RETURN(1);
  }

#ifndef DBUG_OFF
  for (uint i=0 ; i < native_functions_hash.records ; i++)
  {
    func= (Native_func_registry*) my_hash_element(& native_functions_hash, i);
    DBUG_PRINT("info", ("native function: %s  length: %u",
                        func->name.str, (uint) func->name.length));
  }
#endif

  DBUG_RETURN(0);
}

/*
  Empty the hash table for native functions.
  Note: this code is not thread safe, and is intended to be used at server
  shutdown only (after thread requests have been executed).
*/

void item_create_cleanup()
{
  DBUG_ENTER("item_create_cleanup");
  my_hash_free(& native_functions_hash);
  DBUG_VOID_RETURN;
}

Create_func *
find_native_function_builder(THD *thd, LEX_STRING name)
{
  Native_func_registry *func;
  Create_func *builder= NULL;

  /* Thread safe */
  func= (Native_func_registry*) my_hash_search(& native_functions_hash,
                                               (uchar*) name.str,
                                               name.length);

  if (func)
  {
    builder= func->builder;
  }

  return builder;
}

Create_qfunc *
find_qualified_function_builder(THD *thd)
{
  return & Create_sp_func::s_singleton;
}


Item *
create_func_cast(THD *thd, const POS &pos, Item *a, Cast_target cast_target,
                 const CHARSET_INFO *cs)
{
  Cast_type type;
  type.target= cast_target;
  type.charset= cs;
  type.type_flags= 0;
  type.length= NULL;
  type.dec= NULL;
  return create_func_cast(thd, pos, a, &type);
}


Item *
create_func_cast(THD *thd, const POS &pos, Item *a, const Cast_type *type)
{
  if (a == NULL)
    return NULL; // earlier syntax error detected

  const Cast_target cast_type= type->target;
  const char *c_len= type->length;
  const char *c_dec= type->dec;

  Item *res= NULL;

  switch (cast_type) {
  case ITEM_CAST_BINARY:
    res= new (thd->mem_root) Item_func_binary(pos, a);
    break;
  case ITEM_CAST_SIGNED_INT:
    res= new (thd->mem_root) Item_func_signed(pos, a);
    break;
  case ITEM_CAST_UNSIGNED_INT:
    res= new (thd->mem_root) Item_func_unsigned(pos, a);
    break;
  case ITEM_CAST_DATE:
    res= new (thd->mem_root) Item_date_typecast(pos, a);
    break;
  case ITEM_CAST_TIME:
  case ITEM_CAST_DATETIME:
  {
    uint dec= c_dec ? strtoul(c_dec, NULL, 10) : 0;
    if (dec > DATETIME_MAX_DECIMALS)
    {
      my_error(ER_TOO_BIG_PRECISION, MYF(0),
               (int) dec, "CAST", DATETIME_MAX_DECIMALS);
      return 0;
    }
    res= (cast_type == ITEM_CAST_TIME) ?
         (Item*) new (thd->mem_root) Item_time_typecast(pos, a, dec) :
         (Item*) new (thd->mem_root) Item_datetime_typecast(pos, a, dec);
    break;
  }
  case ITEM_CAST_DECIMAL:
  {
    ulong len= 0;
    uint dec= 0;

    if (c_len)
    {
      ulong decoded_size;
      errno= 0;
      decoded_size= strtoul(c_len, NULL, 10);
      if (errno != 0)
      {
        StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                               system_charset_info);
        my_error(ER_TOO_BIG_PRECISION, MYF(0), INT_MAX, buff.c_ptr_safe(),
                 static_cast<ulong>(DECIMAL_MAX_PRECISION));
        return NULL;
      }
      len= decoded_size;
    }

    if (c_dec)
    {
      ulong decoded_size;
      errno= 0;
      decoded_size= strtoul(c_dec, NULL, 10);
      if ((errno != 0) || (decoded_size > UINT_MAX))
      {
        StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                               system_charset_info);
        my_error(ER_TOO_BIG_SCALE, MYF(0), INT_MAX, buff.c_ptr_safe(),
                 static_cast<ulong>(DECIMAL_MAX_SCALE));
        return NULL;
      }
      dec= decoded_size;
    }
    my_decimal_trim(&len, &dec);
    if (len < dec)
    {
      my_error(ER_M_BIGGER_THAN_D, MYF(0), "");
      return 0;
    }
    if (len > DECIMAL_MAX_PRECISION)
    {
      StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                             system_charset_info);
      my_error(ER_TOO_BIG_PRECISION, MYF(0), static_cast<int>(len),
               buff.c_ptr_safe(), static_cast<ulong>(DECIMAL_MAX_PRECISION));
      return 0;
    }
    if (dec > DECIMAL_MAX_SCALE)
    {
      StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                             system_charset_info);
      my_error(ER_TOO_BIG_SCALE, MYF(0), dec, buff.c_ptr_safe(),
               static_cast<ulong>(DECIMAL_MAX_SCALE));
      return 0;
    }
    res= new (thd->mem_root) Item_decimal_typecast(pos, a, len, dec);
    break;
  }
  case ITEM_CAST_CHAR:
  {
    int len= -1;
    const CHARSET_INFO *cs= type->charset;
    const CHARSET_INFO *real_cs=
      (cs ? cs : thd->variables.collation_connection);
    if (c_len)
    {
      ulong decoded_size;
      errno= 0;
      decoded_size= strtoul(c_len, NULL, 10);
      if ((errno != 0) || (decoded_size > MAX_FIELD_BLOBLENGTH))
      {
        my_error(ER_TOO_BIG_DISPLAYWIDTH, MYF(0), "cast as char", MAX_FIELD_BLOBLENGTH);
        return NULL;
      }
      len= (int) decoded_size;
    }
    res= new (thd->mem_root) Item_char_typecast(POS(), a, len, real_cs);
    break;
  }
  case ITEM_CAST_JSON:
  {
    res= new (thd->mem_root) Item_json_typecast(thd, pos, a);

    break;
  }
  default:
  {
    DBUG_ASSERT(0);
    res= 0;
    break;
  }
  }
  return res;
}


/**
  Builder for datetime literals:
    TIME'00:00:00', DATE'2001-01-01', TIMESTAMP'2001-01-01 00:00:00'.
  @param thd          The current thread
  @param str          Character literal
  @param length       Length of str
  @param type         Type of literal (TIME, DATE or DATETIME)
  @param send_error   Whether to generate an error on failure
*/

Item *create_temporal_literal(THD *thd,
                              const char *str, size_t length,
                              const CHARSET_INFO *cs,
                              enum_field_types type, bool send_error)
{
  MYSQL_TIME_STATUS status;
  MYSQL_TIME ltime;
  Item *item= NULL;
  my_time_flags_t flags= TIME_FUZZY_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_IN_DATE)
    flags|= TIME_NO_ZERO_IN_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE)
    flags|= TIME_NO_ZERO_DATE;

  if (thd->variables.sql_mode & MODE_INVALID_DATES)
    flags|= TIME_INVALID_DATES;

  switch(type)
  {
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
    if (!str_to_datetime(cs, str, length, &ltime, flags, &status) &&
        ltime.time_type == MYSQL_TIMESTAMP_DATE && !status.warnings)
      item= new (thd->mem_root) Item_date_literal(&ltime);
    break;
  case MYSQL_TYPE_DATETIME:
    if (!str_to_datetime(cs, str, length, &ltime, flags, &status) &&
        ltime.time_type == MYSQL_TIMESTAMP_DATETIME && !status.warnings)
      item= new (thd->mem_root) Item_datetime_literal(&ltime,
                                                      status.fractional_digits);
    break;
  case MYSQL_TYPE_TIME:
    if (!str_to_time(cs, str, length, &ltime, 0, &status) &&
        ltime.time_type == MYSQL_TIMESTAMP_TIME && !status.warnings)
      item= new (thd->mem_root) Item_time_literal(&ltime,
                                                  status.fractional_digits);
    break;
  default:
    DBUG_ASSERT(0);
  }

  if (item)
    return item;

  if (send_error)
  {
    const char *typestr=
      (type == MYSQL_TYPE_DATE) ? "DATE" :
      (type == MYSQL_TYPE_TIME) ? "TIME" : "DATETIME";
    ErrConvString err(str, length, thd->variables.character_set_client);
    my_error(ER_WRONG_VALUE, MYF(0), typestr, err.ptr());
  }
  return NULL;
}

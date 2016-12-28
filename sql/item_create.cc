/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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
  @file sql/item_create.cc

  Functions to create an item. Used by sql_yacc.yy
*/

#include "item_create.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <sys/types.h>

#include "handler.h"
#include "hash.h"
#include "item.h"
#include "item_cmpfunc.h"        // Item_func_any_value
#include "item_func.h"           // Item_func_udf_str
#include "item_geofunc.h"        // Item_func_area
#include "item_inetfunc.h"       // Item_func_inet_ntoa
#include "item_json_func.h"      // Item_func_json
#include "item_strfunc.h"        // Item_func_aes_encrypt
#include "item_sum.h"            // Item_sum_udf_str
#include "item_timefunc.h"       // Item_func_add_time
#include "item_xmlfunc.h"        // Item_func_xml_extractvalue
#include "m_string.h"
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_global.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "parse_location.h"
#include "parse_tree_helpers.h"  // PT_item_list
#include "psi_memory_key.h"
#include "sql_class.h"           // THD
#include "sql_const.h"
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_security_ctx.h"
#include "sql_string.h"
#include "sql_time.h"            // str_to_datetime
#include "sql_udf.h"
#include "system_variables.h"

/**
  @addtogroup GROUP_PARSER
  @{
*/

/*
=============================================================================
  LOCAL DECLARATIONS
=============================================================================
*/

/**
  Adapter for native functions with a variable number of arguments.
  The main use of this class is to discard the following calls:

  `foo(expr1 AS name1, expr2 AS name2, ...)`

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
  @defgroup Instantiators Instantiator functions

  The Instantiator functions are used to call constructors and `operator new`
  on classes that implement SQL functions, basically, even though they don't
  have to be functions. This pattern has to be used because of the
  following reasons:

  - The parser produces PT_item_list objects of all argument lists, while the
    Item_func subclasses use overloaded constructors,
    e.g. Item_xxx_func(Item*), Item_xxx_func(Item*, Item*), etc.

  - We need to map parser tokens to classes and we don't have reflection.

  Because partial template specialization is used, the functions are
  implemented as class templates rather that functions templates.

  Functions objects that can be created simply by calling the constructor of
  their respective Item_func class need only instantiate the first template
  below. Some functions do some special tricks before creating the function
  object, and in that case they need their own Instantiator. See for instance
  Bin_instantiator or Oct_instantiator here below for how to do that.

  Keeping the templates in anonymous namespaces enables the compiler to inline
  more and hence keeps the generated code leaner.

  @{
*/

/**
  Instantiates a function class with the list of arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.

  @tparam Min_argcount The minimum number of arguments. Not used in this
  general case.

  @tparam Max_argcount The maximum number of arguments. Not used in this
  general case.
*/
namespace {
template<typename Function_class,
         int Min_argcount,
         int Max_argcount= Min_argcount>
class Instantiator
{
public:
  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(POS(), args);
  }
};


/**
  Instantiates a function class with no arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 0>
{
public:
  static const uint Min_argcount= 0;
  static const uint Max_argcount= 0;
  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(POS());
  }
};


/**
  Instantiates a function class with one parameter.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 1>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(POS(), (*args)[0]);
  }
};


/**
  Instantiates a function class with two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 2>
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
  }
};


/**
  Instantiates a function class with three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 3>
{
public:
  static const uint Min_argcount= 3;
  static const uint Max_argcount= 3;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
  }
};


} // namespace


class Bin_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    POS pos;
    Item *i10= new (thd->mem_root) Item_int(pos, 10, 2);
    Item *i2= new (thd->mem_root) Item_int(pos, 2, 1);
    return new (thd->mem_root) Item_func_conv(pos, (*args)[0], i10, i2);
  }
};


class Degrees_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_units(POS(), "degrees", (*args)[0], 180.0 / M_PI, 0.0);
  }
};


class Radians_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_units(POS(), "radians", (*args)[0], M_PI / 180.0, 0.0);
  }
};


class Oct_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    Item *i10= new (thd->mem_root) Item_int(POS(), 10, 2);
    Item *i8= new (thd->mem_root) Item_int(POS(), 8, 1);
    return new (thd->mem_root) Item_func_conv(POS(), (*args)[0], i10, i8);
  }
};


class Weekday_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Item_func_weekday(POS(), (*args)[0], 0);
  }
};


class Dayofweek_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Item_func_weekday(POS(), (*args)[0], 1);
  }
};


class Weekofyear_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    Item *i1= new (thd->mem_root) Item_int(POS(), NAME_STRING("0"), 3, 1);
    return new (thd->mem_root) Item_func_week(POS(), (*args)[0], i1);
  }
};


class Datediff_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    Item *i1= new (thd->mem_root) Item_func_to_days(POS(), (*args)[0]);
    Item *i2= new (thd->mem_root) Item_func_to_days(POS(), (*args)[1]);

    return new (thd->mem_root) Item_func_minus(POS(), i1, i2);
  }
};


class Subtime_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_add_time(POS(), (*args)[0], (*args)[1], false, true);
  }
};


class Time_format_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Item_func_date_format(POS(), (*args)[0], (*args)[1], true);
  }
};


template<Item_func::Functype Function_type>
class Mbr_rel_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_spatial_mbr_rel(POS(), (*args)[0], (*args)[1], Function_type);
  }
};

typedef Mbr_rel_instantiator<Item_func::SP_COVEREDBY_FUNC>
Mbr_covered_by_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_COVERS_FUNC> Mbr_covers_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_CONTAINS_FUNC> Mbr_contains_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_DISJOINT_FUNC> Mbr_disjoint_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_EQUALS_FUNC> Mbr_equals_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_INTERSECTS_FUNC>
Mbr_intersects_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_OVERLAPS_FUNC> Mbr_overlaps_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_TOUCHES_FUNC> Mbr_touches_instantiator;
typedef Mbr_rel_instantiator<Item_func::SP_WITHIN_FUNC> Mbr_within_instantiator;

typedef Mbr_rel_instantiator<Item_func::SP_CROSSES_FUNC> Mbr_crosses_instantiator;

template<Item_func_spatial_operation::op_type Op_type>
class Spatial_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_spatial_operation(POS(), (*args)[0], (*args)[1], Op_type);
  }
};

typedef Spatial_instantiator<Item_func_spatial_operation::op_intersection>
Intersection_instantiator;
typedef Spatial_instantiator<Item_func_spatial_operation::op_difference>
Difference_instantiator;
typedef Spatial_instantiator<Item_func_spatial_operation::op_union>
Union_instantiator;
typedef Spatial_instantiator<Item_func_spatial_operation::op_symdifference>
Symdifference_instantiator;


template<Item_func::Functype Function_type>
class Spatial_rel_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_spatial_rel(POS(), (*args)[0], (*args)[1], Function_type);
  }
};

typedef Spatial_rel_instantiator<Item_func::SP_CONTAINS_FUNC>
St_contains_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_CROSSES_FUNC>
St_crosses_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_DISJOINT_FUNC>
St_disjoint_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_EQUALS_FUNC>
St_equals_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_INTERSECTS_FUNC>
St_intersects_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_OVERLAPS_FUNC>
St_overlaps_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_TOUCHES_FUNC>
St_touches_instantiator;
typedef Spatial_rel_instantiator<Item_func::SP_WITHIN_FUNC>
St_within_instantiator;


template<Item_func::Functype Function_type>
class Spatial_decomp_n_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Item_func_spatial_decomp_n(POS(), (*args)[0], (*args)[1], Function_type);
  }
};

typedef Spatial_decomp_n_instantiator<Item_func::SP_GEOMETRYN>
Sp_geometryn_instantiator;
typedef Spatial_decomp_n_instantiator<Item_func::SP_INTERIORRINGN>
Sp_interiorringn_instantiator;
typedef Spatial_decomp_n_instantiator<Item_func::SP_POINTN>
Sp_pointn_instantiator;


/// @} (end of group Instantiators)

/**
  Factory for creating function objects. Performs validation check that the
  number of arguments is correct, then calls upon the instantiator function to
  instantiate the function object.

  @tparam Instantiator A class that is expected to contain the following:

  - Min_argcount: The minimal number of arguments required to call the
  function. If the parameter count is less, an SQL error is raised and nullptr
  is returned.

  - Max_argcount: The maximum number of arguments required to call the
  function. If the parameter count is greater, an SQL error is raised and
  nullptr is returned.

  - Item *instantiate(THD *, PT_item_list *): Should construct an Item.
*/
namespace {
template<typename Instantiator_fn>
class Function_factory : public Create_func
{
public:
  static Function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name, PT_item_list *item_list)
    override
  {
    if (m_instantiator.Min_argcount == 0 && m_instantiator.Max_argcount == 0)
    {
      if (item_list != nullptr)
      {
        my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
        return nullptr;
      }
    }
    else if (item_list == nullptr ||
             item_list->elements() < m_instantiator.Min_argcount ||
             item_list->elements() > m_instantiator.Max_argcount)
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }
    return m_instantiator.instantiate(thd, item_list);
  }

private:
  Function_factory() {}
  Instantiator_fn m_instantiator;
};

template<typename Instantiator_fn>
Function_factory<Instantiator_fn>
Function_factory<Instantiator_fn>::s_singleton;
}

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


class Create_func_as_wkb : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_as_wkb s_singleton;

protected:
  Create_func_as_wkb() {}
  virtual ~Create_func_as_wkb() {}
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


class Create_func_json_merge : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_json_merge s_singleton;

protected:
  Create_func_json_merge() {}
  virtual ~Create_func_json_merge() {}
};

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


class Create_func_uuid_to_bin : public Create_native_func
{
public:
  virtual Item* create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);
  static Create_func_uuid_to_bin s_singleton;

protected:
  Create_func_uuid_to_bin() {}
  virtual ~Create_func_uuid_to_bin() {}
};


class Create_func_bin_to_uuid : public Create_native_func
{
public:
  virtual Item* create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);
  static Create_func_bin_to_uuid s_singleton;

protected:
  Create_func_bin_to_uuid() {}
  virtual ~Create_func_bin_to_uuid() {}
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


class Create_func_srid : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_srid s_singleton;

protected:
  Create_func_srid() {}
  virtual ~Create_func_srid() {}
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


class Create_func_x : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_x s_singleton;

protected:
  Create_func_x() {}
  virtual ~Create_func_x() {}
};


class Create_func_y : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_y s_singleton;

protected:
  Create_func_y() {}
  virtual ~Create_func_y() {}
};


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


class Create_func_get_dd_column_privileges : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_get_dd_column_privileges s_singleton;

protected:
  Create_func_get_dd_column_privileges() {}
  virtual ~Create_func_get_dd_column_privileges() {}
};


class Create_func_get_dd_index_sub_part_length : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_get_dd_index_sub_part_length s_singleton;

protected:
  Create_func_get_dd_index_sub_part_length() {}
  virtual ~Create_func_get_dd_index_sub_part_length() {}
};


class Create_func_get_dd_create_options : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_get_dd_create_options s_singleton;

protected:
  Create_func_get_dd_create_options() {}
  virtual ~Create_func_get_dd_create_options() {}
};


class Create_func_internal_dd_char_length : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_internal_dd_char_length s_singleton;

protected:
  Create_func_internal_dd_char_length() {}
  virtual ~Create_func_internal_dd_char_length() {}
};


class Create_func_can_access_database : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_can_access_database s_singleton;

protected:
  Create_func_can_access_database() {}
  virtual ~Create_func_can_access_database() {}
};


class Create_func_can_access_table : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_can_access_table s_singleton;

protected:
  Create_func_can_access_table() {}
  virtual ~Create_func_can_access_table() {}
};


class Create_func_can_access_view : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_can_access_view s_singleton;

protected:
  Create_func_can_access_view() {}
  virtual ~Create_func_can_access_view() {}
};


class Create_func_can_access_column : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_can_access_column s_singleton;

protected:
  Create_func_can_access_column() {}
  virtual ~Create_func_can_access_column() {}
};


class Create_func_internal_table_rows: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_table_rows s_singleton;

protected:
 Create_func_internal_table_rows() {}
 virtual ~Create_func_internal_table_rows() {}
};


class Create_func_internal_avg_row_length: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_avg_row_length s_singleton;

protected:
 Create_func_internal_avg_row_length() {}
 virtual ~Create_func_internal_avg_row_length() {}
};


class Create_func_internal_data_length: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_data_length s_singleton;

protected:
 Create_func_internal_data_length() {}
 virtual ~Create_func_internal_data_length() {}
};


class Create_func_internal_max_data_length: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_max_data_length s_singleton;

protected:
 Create_func_internal_max_data_length() {}
 virtual ~Create_func_internal_max_data_length() {}
};


class Create_func_internal_index_length: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_index_length s_singleton;

protected:
 Create_func_internal_index_length() {}
 virtual ~Create_func_internal_index_length() {}
};


class Create_func_internal_data_free: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_data_free s_singleton;

protected:
 Create_func_internal_data_free() {}
 virtual ~Create_func_internal_data_free() {}
};


class Create_func_internal_auto_increment: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_auto_increment s_singleton;

protected:
 Create_func_internal_auto_increment() {}
 virtual ~Create_func_internal_auto_increment() {}
};


class Create_func_internal_checksum: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_checksum s_singleton;

protected:
 Create_func_internal_checksum() {}
 virtual ~Create_func_internal_checksum() {}
};


class Create_func_internal_update_time: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_update_time s_singleton;

protected:
 Create_func_internal_update_time() {}
 virtual ~Create_func_internal_update_time() {}
};


class Create_func_internal_check_time: public Create_native_func
{
public:
  virtual Item *create_native(THD *thd,
                              LEX_STRING name,
                              PT_item_list *item_list);

  static  Create_func_internal_check_time s_singleton;

protected:
 Create_func_internal_check_time() {}
 virtual ~Create_func_internal_check_time() {}
};


class Create_func_internal_keys_disabled : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_internal_keys_disabled s_singleton;

protected:
  Create_func_internal_keys_disabled() {}
  virtual ~Create_func_internal_keys_disabled() {}
};


class Create_func_internal_index_column_cardinality : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_internal_index_column_cardinality s_singleton;

protected:
  Create_func_internal_index_column_cardinality() {}
  virtual ~Create_func_internal_index_column_cardinality() {}
};


class Create_func_internal_get_comment_or_error : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_internal_get_comment_or_error s_singleton;

protected:
  Create_func_internal_get_comment_or_error() {}
  virtual ~Create_func_internal_get_comment_or_error() {}
};

class Create_func_internal_get_view_warning_or_error : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_internal_get_view_warning_or_error s_singleton;

protected:
  Create_func_internal_get_view_warning_or_error() {}
  virtual ~Create_func_internal_get_view_warning_or_error() {}
};

class Create_func_get_dd_table_private_data : public Create_native_func
{
public:
  virtual Item *create_native(THD *thd, LEX_STRING name,
                              PT_item_list *item_list);

  static Create_func_get_dd_table_private_data s_singleton;

protected:
  Create_func_get_dd_table_private_data () {}
  virtual ~Create_func_get_dd_table_private_data () {}
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


Create_func_aes_encrypt Create_func_aes_encrypt::s_singleton;


Create_func_aes_decrypt Create_func_aes_decrypt::s_singleton;


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
Create_func_as_wkb::create_native(THD *thd, LEX_STRING name,
                                  PT_item_list *item_list)
{
  Item* func= nullptr;
  int arg_count= 0;

  if (item_list != nullptr)
    arg_count= item_list->elements();

  switch (arg_count)
  {
  case 1:
    {
      Item *param_1= item_list->pop_front();
      func= new (thd->mem_root) Item_func_as_wkb(POS(), param_1);
      break;
    }
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_as_wkb(POS(), param_1, param_2);
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
  Item_func_geometry_from_text::Functype functype=
    Item_func_geometry_from_text::Functype::GEOMFROMTEXT;

  if (!native_strncasecmp("st_geomcollfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::GEOMCOLLFROMTEXT;
  else if (!native_strncasecmp("st_geomcollfromtxt", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::GEOMCOLLFROMTXT;
  else if (!native_strncasecmp("st_geometrycollectionfromtext", name.str,
                              name.length))
    functype=
      Item_func_geometry_from_text::Functype::GEOMETRYCOLLECTIONFROMTEXT;
  else if (!native_strncasecmp("st_geometryfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::GEOMETRYFROMTEXT;
  else if (!native_strncasecmp("st_geomfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::GEOMFROMTEXT;
  else if (!native_strncasecmp("st_linefromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::LINEFROMTEXT;
  else if (!native_strncasecmp("st_linestringfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::LINESTRINGFROMTEXT;
  else if (!native_strncasecmp("st_mlinefromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::MLINEFROMTEXT;
  else if (!native_strncasecmp("st_mpointfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::MPOINTFROMTEXT;
  else if (!native_strncasecmp("st_mpolyfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::MPOLYFROMTEXT;
  else if (!native_strncasecmp("st_multilinestringfromtext", name.str,
                              name.length))
    functype= Item_func_geometry_from_text::Functype::MULTILINESTRINGFROMTEXT;
  else if (!native_strncasecmp("st_multipointfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::MULTIPOINTFROMTEXT;
  else if (!native_strncasecmp("st_multipolygonfromtext", name.str,
                               name.length))
    functype= Item_func_geometry_from_text::Functype::MULTIPOLYGONFROMTEXT;
  else if (!native_strncasecmp("st_pointfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::POINTFROMTEXT;
  else if (!native_strncasecmp("st_polyfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::POLYFROMTEXT;
  else if (!native_strncasecmp("st_polygonfromtext", name.str, name.length))
    functype= Item_func_geometry_from_text::Functype::POLYGONFROMTEXT;
  else
    DBUG_ASSERT(false);

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
    {
      Item *param_1= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geometry_from_text(pos, param_1,
                                                             functype);
      break;
    }
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geometry_from_text(pos, param_1,
                                                             param_2,
                                                             functype);
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
  Item_func_geometry_from_wkb::Functype functype=
    Item_func_geometry_from_wkb::Functype::GEOMFROMWKB;

  if (!native_strncasecmp("st_geomcollfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::GEOMCOLLFROMWKB;
  else if (!native_strncasecmp("st_geometrycollectionfromwkb", name.str,
                               name.length))
    functype= Item_func_geometry_from_wkb::Functype::GEOMETRYCOLLECTIONFROMWKB;
  else if (!native_strncasecmp("st_geometryfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::GEOMETRYFROMWKB;
  else if (!native_strncasecmp("st_geomfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::GEOMFROMWKB;
  else if (!native_strncasecmp("st_linefromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::LINEFROMWKB;
  else if (!native_strncasecmp("st_linestringfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::LINESTRINGFROMWKB;
  else if (!native_strncasecmp("st_mlinefromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::MLINEFROMWKB;
  else if (!native_strncasecmp("st_mpointfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::MPOINTFROMWKB;
  else if (!native_strncasecmp("st_mpolyfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::MPOLYFROMWKB;
  else if (!native_strncasecmp("st_multilinestringfromwkb", name.str,
                               name.length))
    functype= Item_func_geometry_from_wkb::Functype::MULTILINESTRINGFROMWKB;
  else if (!native_strncasecmp("st_multipointfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::MULTIPOINTFROMWKB;
  else if (!native_strncasecmp("st_multipolygonfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::MULTIPOLYGONFROMWKB;
  else if (!native_strncasecmp("st_pointfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::POINTFROMWKB;
  else if (!native_strncasecmp("st_polyfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::POLYFROMWKB;
  else if (!native_strncasecmp("st_polygonfromwkb", name.str, name.length))
    functype= Item_func_geometry_from_wkb::Functype::POLYGONFROMWKB;
  else
    DBUG_ASSERT(false);

  if (item_list != NULL)
    arg_count= item_list->elements();

  POS pos;
  switch (arg_count) {
  case 1:
    {
      Item *param_1= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geometry_from_wkb(pos, param_1,
                                                            functype);
      break;
    }
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_geometry_from_wkb(pos, param_1,
                                                            param_2,
                                                            functype);
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

Create_func_json_merge Create_func_json_merge::s_singleton;

Item*
Create_func_json_merge::create_native(THD *thd, LEX_STRING name,
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
    func= new (thd->mem_root) Item_func_json_merge(thd, POS(), item_list);
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


Create_func_uuid_to_bin Create_func_uuid_to_bin::s_singleton;

Item*
Create_func_uuid_to_bin::create_native(THD *thd, LEX_STRING name,
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
      func= new (thd->mem_root) Item_func_uuid_to_bin(pos, param_1);
      break;
    }
    case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_uuid_to_bin(pos, param_1, param_2);
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


Create_func_bin_to_uuid Create_func_bin_to_uuid::s_singleton;

Item*
Create_func_bin_to_uuid::create_native(THD *thd, LEX_STRING name,
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
      func= new (thd->mem_root) Item_func_bin_to_uuid(pos, param_1);
      break;
    }
    case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_bin_to_uuid(pos, param_1, param_2);
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


Create_func_srid Create_func_srid::s_singleton;

Item*
Create_func_srid::create_native(THD *thd, LEX_STRING name,
                                PT_item_list *item_list)
{
  Item *func= nullptr;
  int arg_count= 0;

  if (item_list != nullptr)
  {
    arg_count= item_list->elements();
  }

  switch (arg_count)
  {
  case 1:
    {
      Item *param_1= item_list->pop_front();
      func= new (thd->mem_root) Item_func_get_srid(POS(), param_1);
      break;
    }
  case 2:
    {
      Item *param_1= item_list->pop_front();
      Item *param_2= item_list->pop_front();
      func= new (thd->mem_root) Item_func_set_srid(POS(), param_1, param_2);
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

Create_func_x Create_func_x::s_singleton;

Item*
Create_func_x::create_native(THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  Item* func= nullptr;
  int arg_count= 0;

  if (item_list != nullptr)
  {
    arg_count= item_list->elements();
  }
  switch (arg_count)
  {
  case 1:
    {
      Item *arg1= item_list->pop_front();
      return new (thd->mem_root) Item_func_get_x(POS(), arg1);
    }
  case 2:
    {
      Item *arg1= item_list->pop_front();
      Item *arg2= item_list->pop_front();
      return new (thd->mem_root) Item_func_set_x(POS(), arg1, arg2);
    }
  default:
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
      break;
    }
  }

  return func;
}


Create_func_y Create_func_y::s_singleton;

Item*
Create_func_y::create_native(THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  Item* func= nullptr;
  int arg_count= 0;

  if (item_list != nullptr)
  {
    arg_count= item_list->elements();
  }
  switch (arg_count)
  {
  case 1:
    {
      Item *arg1= item_list->pop_front();
      return new (thd->mem_root) Item_func_get_y(POS(), arg1);
    }
  case 2:
    {
      Item *arg1= item_list->pop_front();
      Item *arg2= item_list->pop_front();
      return new (thd->mem_root) Item_func_set_y(POS(), arg1, arg2);
    }
  default:
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
      break;
    }
  }

  return func;
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


Create_func_get_dd_column_privileges Create_func_get_dd_column_privileges::s_singleton;

Item*
Create_func_get_dd_column_privileges::create_native(THD *thd, LEX_STRING name,
                                          PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();

  return new (thd->mem_root) Item_func_get_dd_column_privileges(
                               POS(), param_1, param_2, param_3);
}


Create_func_get_dd_index_sub_part_length
  Create_func_get_dd_index_sub_part_length::s_singleton;

Item*
Create_func_get_dd_index_sub_part_length::create_native(
  THD *thd,
  LEX_STRING name,
  PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 5)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();
  Item *param_5= item_list->pop_front();

  return new (thd->mem_root) Item_func_get_dd_index_sub_part_length(POS(),
                               param_1, param_2, param_3, param_4, param_5);
}


Create_func_get_dd_create_options
  Create_func_get_dd_create_options::s_singleton;

Item*
Create_func_get_dd_create_options::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();

  return new (thd->mem_root) Item_func_get_dd_create_options(
                               POS(), param_1, param_2);
}


Create_func_can_access_database Create_func_can_access_database::s_singleton;

Item*
Create_func_can_access_database::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();

  return new (thd->mem_root) Item_func_can_access_database(
                               POS(), param_1);
}


Create_func_can_access_table Create_func_can_access_table::s_singleton;

Item*
Create_func_can_access_table::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 3)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();

  return new (thd->mem_root) Item_func_can_access_table(
                               POS(), param_1, param_2, param_3);
}


Create_func_can_access_view Create_func_can_access_view::s_singleton;

Item*
Create_func_can_access_view::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_can_access_view(
                               POS(), param_1, param_2, param_3, param_4);
}


Create_func_can_access_column Create_func_can_access_column::s_singleton;

Item*
Create_func_can_access_column::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_can_access_column(
                               POS(), param_1, param_2, param_3, param_4);
}


Create_func_internal_table_rows
  Create_func_internal_table_rows::s_singleton;

Item*
Create_func_internal_table_rows::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_table_rows(POS(),
                                                           param_1,
                                                           param_2,
                                                           param_3,
                                                           param_4);
}


Create_func_internal_avg_row_length
  Create_func_internal_avg_row_length::s_singleton;

Item*
Create_func_internal_avg_row_length::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_avg_row_length(POS(),
                                                               param_1,
                                                               param_2,
                                                               param_3,
                                                               param_4);
}


Create_func_internal_data_length Create_func_internal_data_length::s_singleton;

Item*
Create_func_internal_data_length::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_data_length(POS(),
                                                            param_1,
                                                            param_2,
                                                            param_3,
                                                            param_4);
}


Create_func_internal_max_data_length
  Create_func_internal_max_data_length::s_singleton;

Item*
Create_func_internal_max_data_length::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_max_data_length(POS(),
                                                                param_1,
                                                                param_2,
                                                                param_3,
                                                                param_4);
}


Create_func_internal_index_length
  Create_func_internal_index_length::s_singleton;

Item*
Create_func_internal_index_length::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_index_length(POS(),
                                                             param_1,
                                                             param_2,
                                                             param_3,
                                                             param_4);
}


Create_func_internal_data_free Create_func_internal_data_free::s_singleton;

Item*
Create_func_internal_data_free::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_data_free(POS(),
                                                          param_1,
                                                          param_2,
                                                          param_3,
                                                          param_4);
}


Create_func_internal_auto_increment
  Create_func_internal_auto_increment::s_singleton;

Item*
Create_func_internal_auto_increment::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_auto_increment(POS(),
                                                               param_1,
                                                               param_2,
                                                               param_3,
                                                               param_4);
}


Create_func_internal_checksum
  Create_func_internal_checksum::s_singleton;

Item*
Create_func_internal_checksum::create_native(THD *thd, LEX_STRING name,
                                             PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_checksum(POS(),
                                                         param_1,
                                                         param_2,
                                                         param_3,
                                                         param_4);
}


Create_func_internal_update_time
  Create_func_internal_update_time::s_singleton;

Item*
Create_func_internal_update_time::create_native(THD *thd, LEX_STRING name,
                                                PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_update_time(POS(),
                                                            param_1,
                                                            param_2,
                                                            param_3,
                                                            param_4);
}


Create_func_internal_check_time
  Create_func_internal_check_time::s_singleton;

Item*
Create_func_internal_check_time::create_native(THD *thd, LEX_STRING name,
                                               PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_check_time(POS(),
                                                           param_1,
                                                           param_2,
                                                           param_3,
                                                           param_4);
}


Create_func_internal_keys_disabled
  Create_func_internal_keys_disabled::s_singleton;

Item*
Create_func_internal_keys_disabled::create_native(THD *thd, LEX_STRING name,
                                                  PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 1)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_keys_disabled(
                               POS(), param_1);
}


Create_func_internal_index_column_cardinality
  Create_func_internal_index_column_cardinality::s_singleton;

Item*
Create_func_internal_index_column_cardinality::create_native(
  THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 7)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  return new (thd->mem_root) Item_func_internal_index_column_cardinality(
                               POS(), item_list);
}


Create_func_internal_get_comment_or_error
Create_func_internal_get_comment_or_error::s_singleton;

Item*
Create_func_internal_get_comment_or_error::create_native(
  THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 5)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  return new (thd->mem_root) Item_func_internal_get_comment_or_error(
                               POS(), item_list);
}

Create_func_internal_get_view_warning_or_error
  Create_func_internal_get_view_warning_or_error::s_singleton;

Item*
Create_func_internal_get_view_warning_or_error::create_native(
  THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return NULL;
  }

  return
    new (thd->mem_root)Item_func_internal_get_view_warning_or_error(POS(),
                                                                    item_list);
}

struct Native_func_registry
{
  LEX_STRING name;
  Create_func *builder;
};

#define BUILDER(F) & F::s_singleton

/**
  Shorthand macro to reference the singleton instance. This also instantiates
  the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN(F, N) &Function_factory<Instantiator<F, N>>::s_singleton

/**
  Shorthand macro to reference the singleton instance when there is a
  specialized intantiatior.

  @param INSTANTIATOR The instantiator class.
*/
#define SQL_FACTORY(INSTANTIATOR) &Function_factory<INSTANTIATOR>::s_singleton

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
  { { C_STRING_WITH_LEN("ABS") }, SQL_FN(Item_func_abs, 1) },
  { { C_STRING_WITH_LEN("ACOS") }, SQL_FN(Item_func_acos, 1) },
  { { C_STRING_WITH_LEN("ADDTIME") }, SQL_FN(Item_func_add_time, 2) },
  { { C_STRING_WITH_LEN("AES_DECRYPT") }, BUILDER(Create_func_aes_decrypt)},
  { { C_STRING_WITH_LEN("AES_ENCRYPT") }, BUILDER(Create_func_aes_encrypt)},
  { { C_STRING_WITH_LEN("ANY_VALUE") }, SQL_FN(Item_func_any_value, 1) },
  { { C_STRING_WITH_LEN("ASIN") }, SQL_FN(Item_func_asin, 1) },
  { { C_STRING_WITH_LEN("ATAN") }, BUILDER(Create_func_atan)},
  { { C_STRING_WITH_LEN("ATAN2") }, BUILDER(Create_func_atan)},
  { { C_STRING_WITH_LEN("BENCHMARK") }, SQL_FN(Item_func_benchmark, 2) },
  { { C_STRING_WITH_LEN("BIN") }, SQL_FACTORY(Bin_instantiator) },
  { { C_STRING_WITH_LEN("BIN_TO_UUID") }, BUILDER(Create_func_bin_to_uuid)},
  { { C_STRING_WITH_LEN("BIT_COUNT") }, SQL_FN(Item_func_bit_count, 1) },
  { { C_STRING_WITH_LEN("BIT_LENGTH") }, SQL_FN(Item_func_bit_length, 1) },
  { { C_STRING_WITH_LEN("CEIL") }, SQL_FN(Item_func_ceiling, 1) },
  { { C_STRING_WITH_LEN("CEILING") }, SQL_FN(Item_func_ceiling, 1) },
  { { C_STRING_WITH_LEN("CHARACTER_LENGTH") }, SQL_FN(Item_func_char_length, 1) },
  { { C_STRING_WITH_LEN("CHAR_LENGTH") }, SQL_FN(Item_func_char_length, 1) },
  { { C_STRING_WITH_LEN("COERCIBILITY") }, SQL_FN(Item_func_coercibility, 1) },
  { { C_STRING_WITH_LEN("COMPRESS") }, SQL_FN(Item_func_compress, 1) },
  { { C_STRING_WITH_LEN("CONCAT") }, BUILDER(Create_func_concat)},
  { { C_STRING_WITH_LEN("CONCAT_WS") }, BUILDER(Create_func_concat_ws)},
  { { C_STRING_WITH_LEN("CONNECTION_ID") }, SQL_FN(Item_func_connection_id, 0) },
  { { C_STRING_WITH_LEN("CONV") }, SQL_FN(Item_func_conv, 3) },
  { { C_STRING_WITH_LEN("CONVERT_TZ") }, SQL_FN(Item_func_convert_tz, 3) },
  { { C_STRING_WITH_LEN("COS") }, SQL_FN(Item_func_cos, 1) },
  { { C_STRING_WITH_LEN("COT") }, SQL_FN(Item_func_cot, 1) },
  { { C_STRING_WITH_LEN("CRC32") }, SQL_FN(Item_func_crc32, 1) },
  { { C_STRING_WITH_LEN("CURRENT_ROLE") }, SQL_FN(Item_func_current_role, 0) },
  { { C_STRING_WITH_LEN("DATEDIFF") }, SQL_FACTORY(Datediff_instantiator) },
  { { C_STRING_WITH_LEN("DATE_FORMAT") }, SQL_FN(Item_func_date_format, 2) },
  { { C_STRING_WITH_LEN("DAYNAME") }, SQL_FN(Item_func_dayname, 1) },
  { { C_STRING_WITH_LEN("DAYOFMONTH") }, SQL_FN(Item_func_dayofmonth, 1) },
  { { C_STRING_WITH_LEN("DAYOFWEEK") }, SQL_FACTORY(Dayofweek_instantiator) },
  { { C_STRING_WITH_LEN("DAYOFYEAR") }, SQL_FN(Item_func_dayofyear, 1) },
  { { C_STRING_WITH_LEN("DECODE") }, SQL_FN(Item_func_decode, 2) },
  { { C_STRING_WITH_LEN("DEGREES") }, SQL_FACTORY(Degrees_instantiator) },
  { { C_STRING_WITH_LEN("DES_DECRYPT") }, BUILDER(Create_func_des_decrypt)},
  { { C_STRING_WITH_LEN("DES_ENCRYPT") }, BUILDER(Create_func_des_encrypt)},
  { { C_STRING_WITH_LEN("ELT") }, BUILDER(Create_func_elt)},
  { { C_STRING_WITH_LEN("ENCODE") }, SQL_FN(Item_func_encode, 2) },
  { { C_STRING_WITH_LEN("ENCRYPT") }, BUILDER(Create_func_encrypt)},
  { { C_STRING_WITH_LEN("EXP") }, SQL_FN(Item_func_exp, 1) },
  { { C_STRING_WITH_LEN("EXPORT_SET") }, BUILDER(Create_func_export_set)},
  { { C_STRING_WITH_LEN("EXTRACTVALUE") }, SQL_FN(Item_func_xml_extractvalue, 2) },
  { { C_STRING_WITH_LEN("FIELD") }, BUILDER(Create_func_field)},
  { { C_STRING_WITH_LEN("FIND_IN_SET") }, SQL_FN(Item_func_find_in_set, 2) },
  { { C_STRING_WITH_LEN("FLOOR") }, SQL_FN(Item_func_floor, 1) },
  { { C_STRING_WITH_LEN("FOUND_ROWS") }, SQL_FN(Item_func_found_rows, 0) },
  { { C_STRING_WITH_LEN("FROM_BASE64") }, SQL_FN(Item_func_from_base64, 1) },
  { { C_STRING_WITH_LEN("FROM_DAYS") }, SQL_FN(Item_func_from_days, 1) },
  { { C_STRING_WITH_LEN("FROM_UNIXTIME") }, BUILDER(Create_func_from_unixtime)},
  { { C_STRING_WITH_LEN("GET_LOCK") }, SQL_FN(Item_func_get_lock, 2) },
  { { C_STRING_WITH_LEN("GREATEST") }, BUILDER(Create_func_greatest)},
  { { C_STRING_WITH_LEN("GTID_SUBTRACT") }, SQL_FN(Item_func_gtid_subtract, 2) },
  { { C_STRING_WITH_LEN("GTID_SUBSET") }, SQL_FN(Item_func_gtid_subset, 2) },
  { { C_STRING_WITH_LEN("HEX") }, SQL_FN(Item_func_hex, 1) },
  { { C_STRING_WITH_LEN("IFNULL") }, SQL_FN(Item_func_ifnull, 2) },
  { { C_STRING_WITH_LEN("INET_ATON") }, SQL_FN(Item_func_inet_aton, 1) },
  { { C_STRING_WITH_LEN("INET_NTOA") }, SQL_FN(Item_func_inet_ntoa, 1) },
  { { C_STRING_WITH_LEN("INET6_ATON") }, SQL_FN(Item_func_inet6_aton, 1) },
  { { C_STRING_WITH_LEN("INET6_NTOA") }, SQL_FN(Item_func_inet6_ntoa, 1)},
  { { C_STRING_WITH_LEN("IS_IPV4") }, SQL_FN(Item_func_is_ipv4, 1)},
  { { C_STRING_WITH_LEN("IS_IPV6") }, SQL_FN(Item_func_is_ipv6, 1)},
  { { C_STRING_WITH_LEN("IS_IPV4_COMPAT") }, SQL_FN(Item_func_is_ipv4_compat, 1)},
  { { C_STRING_WITH_LEN("IS_IPV4_MAPPED") }, SQL_FN(Item_func_is_ipv4_mapped, 1)},
  { { C_STRING_WITH_LEN("IS_UUID") }, SQL_FN(Item_func_is_uuid, 1) },
  { { C_STRING_WITH_LEN("INSTR") }, SQL_FN(Item_func_instr, 2) },
  { { C_STRING_WITH_LEN("ISNULL") }, SQL_FN(Item_func_isnull, 1) },
  { { C_STRING_WITH_LEN("JSON_VALID") }, SQL_FN(Item_func_json_valid, 1) },
  { { C_STRING_WITH_LEN("JSON_CONTAINS") }, BUILDER(Create_func_json_contains)},
  { { C_STRING_WITH_LEN("JSON_CONTAINS_PATH") }, BUILDER(Create_func_json_contains_path)},
  { { C_STRING_WITH_LEN("JSON_LENGTH") }, BUILDER(Create_func_json_length)},
  { { C_STRING_WITH_LEN("JSON_DEPTH") }, BUILDER(Create_func_json_depth)},
  { { C_STRING_WITH_LEN("JSON_TYPE") }, SQL_FN(Item_func_json_type, 1) },
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
  { { C_STRING_WITH_LEN("JSON_QUOTE") }, BUILDER(Create_func_json_quote)},
  { { C_STRING_WITH_LEN("JSON_UNQUOTE") }, BUILDER(Create_func_json_unquote)},
  { { C_STRING_WITH_LEN("IS_FREE_LOCK") }, SQL_FN(Item_func_is_free_lock, 1) },
  { { C_STRING_WITH_LEN("IS_USED_LOCK") }, SQL_FN(Item_func_is_used_lock, 1) },
  { { C_STRING_WITH_LEN("LAST_DAY") }, SQL_FN(Item_func_last_day, 1) },
  { { C_STRING_WITH_LEN("LAST_INSERT_ID") }, BUILDER(Create_func_last_insert_id)},
  { { C_STRING_WITH_LEN("LCASE") }, SQL_FN(Item_func_lower, 1) },
  { { C_STRING_WITH_LEN("LEAST") }, BUILDER(Create_func_least)},
  { { C_STRING_WITH_LEN("LENGTH") }, SQL_FN(Item_func_length, 1) },
#ifndef DBUG_OFF
  { { C_STRING_WITH_LEN("LIKE_RANGE_MIN") }, SQL_FN(Item_func_like_range_min, 2) },
  { { C_STRING_WITH_LEN("LIKE_RANGE_MAX") }, SQL_FN(Item_func_like_range_max, 2) },
#endif
  { { C_STRING_WITH_LEN("LN") }, SQL_FN(Item_func_ln, 1) },
  { { C_STRING_WITH_LEN("LOAD_FILE") }, SQL_FN(Item_load_file, 1) },
  { { C_STRING_WITH_LEN("LOCATE") }, BUILDER(Create_func_locate)},
  { { C_STRING_WITH_LEN("LOG") }, BUILDER(Create_func_log)},
  { { C_STRING_WITH_LEN("LOG10") }, SQL_FN(Item_func_log10, 1)},
  { { C_STRING_WITH_LEN("LOG2") }, SQL_FN(Item_func_log2, 1)},
  { { C_STRING_WITH_LEN("LOWER") }, SQL_FN(Item_func_lower, 1) },
  { { C_STRING_WITH_LEN("LPAD") }, SQL_FN(Item_func_lpad, 3) },
  { { C_STRING_WITH_LEN("LTRIM") }, SQL_FN(Item_func_ltrim, 1) },
  { { C_STRING_WITH_LEN("MAKEDATE") }, SQL_FN(Item_func_makedate, 2) },
  { { C_STRING_WITH_LEN("MAKETIME") }, SQL_FN(Item_func_maketime, 3) },
  { { C_STRING_WITH_LEN("MAKE_SET") }, BUILDER(Create_func_make_set)},
  { { C_STRING_WITH_LEN("MASTER_POS_WAIT") }, BUILDER(Create_func_master_pos_wait)},
  { { C_STRING_WITH_LEN("MBRCONTAINS") }, SQL_FACTORY(Mbr_contains_instantiator) },
  { { C_STRING_WITH_LEN("MBRCOVEREDBY") }, SQL_FACTORY(Mbr_covered_by_instantiator) },
  { { C_STRING_WITH_LEN("MBRCOVERS") }, SQL_FACTORY(Mbr_covers_instantiator) },
  { { C_STRING_WITH_LEN("MBRDISJOINT") }, SQL_FACTORY(Mbr_disjoint_instantiator) },
  { { C_STRING_WITH_LEN("MBREQUALS") }, SQL_FACTORY(Mbr_equals_instantiator) },
  { { C_STRING_WITH_LEN("MBRINTERSECTS") }, SQL_FACTORY(Mbr_intersects_instantiator) },
  { { C_STRING_WITH_LEN("MBROVERLAPS") }, SQL_FACTORY(Mbr_overlaps_instantiator) },
  { { C_STRING_WITH_LEN("MBRTOUCHES") }, SQL_FACTORY(Mbr_touches_instantiator) },
  { { C_STRING_WITH_LEN("MBRWITHIN") }, SQL_FACTORY(Mbr_within_instantiator) },
  { { C_STRING_WITH_LEN("MD5") }, SQL_FN(Item_func_md5, 1) },
  { { C_STRING_WITH_LEN("MONTHNAME") }, SQL_FN(Item_func_monthname, 1) },
  { { C_STRING_WITH_LEN("NAME_CONST") }, SQL_FN(Item_name_const, 2) },
  { { C_STRING_WITH_LEN("NULLIF") }, SQL_FN(Item_func_nullif, 2) },
  { { C_STRING_WITH_LEN("OCT") }, SQL_FACTORY(Oct_instantiator) },
  { { C_STRING_WITH_LEN("OCTET_LENGTH") }, SQL_FN(Item_func_length, 1) },
  { { C_STRING_WITH_LEN("ORD") }, SQL_FN(Item_func_ord, 1) },
  { { C_STRING_WITH_LEN("PERIOD_ADD") }, SQL_FN(Item_func_period_add, 2) },
  { { C_STRING_WITH_LEN("PERIOD_DIFF") }, SQL_FN(Item_func_period_diff, 2) },
  { { C_STRING_WITH_LEN("PI") }, SQL_FN(Item_func_pi, 0) },
  { { C_STRING_WITH_LEN("POW") }, SQL_FN(Item_func_pow, 2) },
  { { C_STRING_WITH_LEN("POWER") }, SQL_FN(Item_func_pow, 2) },
  { { C_STRING_WITH_LEN("QUOTE") }, SQL_FN(Item_func_quote, 1) },
  { { C_STRING_WITH_LEN("RADIANS") }, SQL_FACTORY(Radians_instantiator) },
  { { C_STRING_WITH_LEN("RAND") }, BUILDER(Create_func_rand)},
  { { C_STRING_WITH_LEN("RANDOM_BYTES") }, SQL_FN(Item_func_random_bytes, 1) },
  { { C_STRING_WITH_LEN("RELEASE_ALL_LOCKS") }, SQL_FN(Item_func_release_all_locks, 0) },
  { { C_STRING_WITH_LEN("RELEASE_LOCK") }, SQL_FN(Item_func_release_lock, 1) },
  { { C_STRING_WITH_LEN("REVERSE") }, SQL_FN(Item_func_reverse, 1) },
  { { C_STRING_WITH_LEN("ROLES_GRAPHML") }, SQL_FN(Item_func_roles_graphml, 0) },
  { { C_STRING_WITH_LEN("ROUND") }, BUILDER(Create_func_round)},
  { { C_STRING_WITH_LEN("RPAD") }, SQL_FN(Item_func_rpad, 3) },
  { { C_STRING_WITH_LEN("RTRIM") }, SQL_FN(Item_func_rtrim, 1) },
  { { C_STRING_WITH_LEN("SEC_TO_TIME") }, SQL_FN(Item_func_sec_to_time, 1) },
  { { C_STRING_WITH_LEN("SHA") }, SQL_FN(Item_func_sha, 1) },
  { { C_STRING_WITH_LEN("SHA1") }, SQL_FN(Item_func_sha, 1) },
  { { C_STRING_WITH_LEN("SHA2") }, SQL_FN(Item_func_sha2, 2) },
  { { C_STRING_WITH_LEN("SIGN") }, SQL_FN(Item_func_sign, 1) },
  { { C_STRING_WITH_LEN("SIN") }, SQL_FN(Item_func_sin, 1) },
  { { C_STRING_WITH_LEN("SLEEP") }, SQL_FN(Item_func_sleep, 1) },
  { { C_STRING_WITH_LEN("SOUNDEX") }, SQL_FN(Item_func_soundex, 1) },
  { { C_STRING_WITH_LEN("SPACE") }, SQL_FN(Item_func_space, 1) },
  { { C_STRING_WITH_LEN("WAIT_FOR_EXECUTED_GTID_SET") }, BUILDER(Create_func_executed_gtid_set_wait)},
  { { C_STRING_WITH_LEN("WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS") }, BUILDER(Create_func_master_gtid_set_wait)},
  { { C_STRING_WITH_LEN("SQRT") }, SQL_FN(Item_func_sqrt, 1) },
  { { C_STRING_WITH_LEN("STRCMP") }, SQL_FN(Item_func_strcmp, 2) },
  { { C_STRING_WITH_LEN("STR_TO_DATE") }, SQL_FN(Item_func_str_to_date, 2) },
  { { C_STRING_WITH_LEN("ST_AREA") }, SQL_FN(Item_func_area, 1) },
  { { C_STRING_WITH_LEN("ST_ASBINARY") }, GEOM_BUILDER(Create_func_as_wkb) },
  { { C_STRING_WITH_LEN("ST_ASGEOJSON") }, GEOM_BUILDER(Create_func_as_geojson)},
  { { C_STRING_WITH_LEN("ST_ASTEXT") }, SQL_FN(Item_func_as_wkt, 1) },
  { { C_STRING_WITH_LEN("ST_ASWKB") }, GEOM_BUILDER(Create_func_as_wkb) },
  { { C_STRING_WITH_LEN("ST_ASWKT") }, SQL_FN(Item_func_as_wkt, 1) },
  { { C_STRING_WITH_LEN("ST_BUFFER") }, GEOM_BUILDER(Create_func_buffer)},
  { { C_STRING_WITH_LEN("ST_BUFFER_STRATEGY") }, GEOM_BUILDER(Create_func_buffer_strategy)},
  { { C_STRING_WITH_LEN("ST_CENTROID") }, SQL_FN(Item_func_centroid, 1) },
  { { C_STRING_WITH_LEN("ST_CONTAINS") }, SQL_FACTORY(St_contains_instantiator) }, // !
  { { C_STRING_WITH_LEN("ST_CONVEXHULL") }, SQL_FN(Item_func_convex_hull, 1) },
  { { C_STRING_WITH_LEN("ST_CROSSES") }, SQL_FACTORY(St_crosses_instantiator) }, // !
  { { C_STRING_WITH_LEN("ST_DIFFERENCE") }, SQL_FACTORY(Difference_instantiator) },
  { { C_STRING_WITH_LEN("ST_DIMENSION") }, SQL_FN(Item_func_dimension, 1) },
  { { C_STRING_WITH_LEN("ST_DISJOINT") }, SQL_FACTORY(St_disjoint_instantiator) },
  { { C_STRING_WITH_LEN("ST_DISTANCE") }, GEOM_BUILDER(Create_func_distance)},
  { { C_STRING_WITH_LEN("ST_DISTANCE_SPHERE") }, GEOM_BUILDER(Create_func_distance_sphere)},
  { { C_STRING_WITH_LEN("ST_ENDPOINT") }, SQL_FN(Item_func_endpoint, 1) },
  { { C_STRING_WITH_LEN("ST_ENVELOPE") }, SQL_FN(Item_func_envelope, 1) },
  { { C_STRING_WITH_LEN("ST_EQUALS") }, SQL_FACTORY(St_equals_instantiator) },
  { { C_STRING_WITH_LEN("ST_EXTERIORRING") }, SQL_FN(Item_func_exteriorring, 1) },
  { { C_STRING_WITH_LEN("ST_GEOHASH") }, GEOM_BUILDER(Create_func_geohash)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_GEOMETRYN") }, SQL_FACTORY(Sp_geometryn_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYTYPE") }, SQL_FN(Item_func_geometry_type, 1) },
  { { C_STRING_WITH_LEN("ST_GEOMFROMGEOJSON") }, GEOM_BUILDER(Create_func_geomfromgeojson)},
  { { C_STRING_WITH_LEN("ST_GEOMFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_GEOMFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_INTERIORRINGN") }, SQL_FACTORY(Sp_interiorringn_instantiator) },
  { { C_STRING_WITH_LEN("ST_INTERSECTS") }, SQL_FACTORY(St_intersects_instantiator) },
  { { C_STRING_WITH_LEN("ST_INTERSECTION") }, SQL_FACTORY(Intersection_instantiator) },
  { { C_STRING_WITH_LEN("ST_ISCLOSED") }, SQL_FN(Item_func_isclosed, 1) },
  { { C_STRING_WITH_LEN("ST_ISEMPTY") }, SQL_FN(Item_func_isempty, 1) },
  { { C_STRING_WITH_LEN("ST_ISSIMPLE") }, SQL_FN(Item_func_issimple, 1) },
  { { C_STRING_WITH_LEN("ST_ISVALID") }, SQL_FN(Item_func_isvalid, 1) },
  { { C_STRING_WITH_LEN("ST_LATFROMGEOHASH") }, SQL_FN(Item_func_latfromgeohash, 1) },
  { { C_STRING_WITH_LEN("ST_LENGTH") }, SQL_FN(Item_func_glength, 1) },
  { { C_STRING_WITH_LEN("ST_LINEFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_LINEFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_LONGFROMGEOHASH") }, SQL_FN(Item_func_longfromgeohash, 1) },
  { { C_STRING_WITH_LEN("ST_MAKEENVELOPE") }, SQL_FN(Item_func_make_envelope, 2) },
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
  { { C_STRING_WITH_LEN("ST_NUMGEOMETRIES") }, SQL_FN(Item_func_numgeometries, 1) },
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRING") }, SQL_FN(Item_func_numinteriorring, 1) },
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRINGS") }, SQL_FN(Item_func_numinteriorring, 1) },
  { { C_STRING_WITH_LEN("ST_NUMPOINTS") }, SQL_FN(Item_func_numpoints, 1) },
  { { C_STRING_WITH_LEN("ST_OVERLAPS") }, SQL_FACTORY(St_overlaps_instantiator) },
  { { C_STRING_WITH_LEN("ST_POINTFROMGEOHASH") }, SQL_FN(Item_func_pointfromgeohash, 2) },
  { { C_STRING_WITH_LEN("ST_POINTFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POINTFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_POINTN") }, SQL_FACTORY(Sp_pointn_instantiator) },
  { { C_STRING_WITH_LEN("ST_POLYFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POLYFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_POLYGONFROMTEXT") }, GEOM_BUILDER(Create_func_geometry_from_text)},
  { { C_STRING_WITH_LEN("ST_POLYGONFROMWKB") }, GEOM_BUILDER(Create_func_geometry_from_wkb)},
  { { C_STRING_WITH_LEN("ST_SIMPLIFY") }, SQL_FN(Item_func_simplify, 2) },
  { { C_STRING_WITH_LEN("ST_SRID") }, GEOM_BUILDER(Create_func_srid)},
  { { C_STRING_WITH_LEN("ST_STARTPOINT") }, SQL_FN(Item_func_startpoint, 1) },
  { { C_STRING_WITH_LEN("ST_SYMDIFFERENCE") }, SQL_FACTORY(Symdifference_instantiator) },
  { { C_STRING_WITH_LEN("ST_TOUCHES") }, SQL_FACTORY(St_touches_instantiator) },
  { { C_STRING_WITH_LEN("ST_UNION") }, SQL_FACTORY(Union_instantiator) },
  { { C_STRING_WITH_LEN("ST_VALIDATE") }, SQL_FN(Item_func_validate, 1) },
  { { C_STRING_WITH_LEN("ST_WITHIN") }, SQL_FACTORY(St_within_instantiator) },
  { { C_STRING_WITH_LEN("ST_X") }, GEOM_BUILDER(Create_func_x)},
  { { C_STRING_WITH_LEN("ST_Y") }, GEOM_BUILDER(Create_func_y)},
  { { C_STRING_WITH_LEN("SUBSTRING_INDEX") }, SQL_FN(Item_func_substr_index, 3) },
  { { C_STRING_WITH_LEN("SUBTIME") }, SQL_FACTORY(Subtime_instantiator) },
  { { C_STRING_WITH_LEN("TAN") }, SQL_FN(Item_func_tan, 1) },
  { { C_STRING_WITH_LEN("TIMEDIFF") }, SQL_FN(Item_func_timediff, 2) },
  { { C_STRING_WITH_LEN("TIME_FORMAT") }, SQL_FACTORY(Time_format_instantiator) },
  { { C_STRING_WITH_LEN("TIME_TO_SEC") }, SQL_FN(Item_func_time_to_sec, 1) },
  { { C_STRING_WITH_LEN("TO_BASE64") }, SQL_FN(Item_func_to_base64, 1) },
  { { C_STRING_WITH_LEN("TO_DAYS") }, SQL_FN(Item_func_to_days, 1) },
  { { C_STRING_WITH_LEN("TO_SECONDS") }, SQL_FN(Item_func_to_seconds, 1) },
  { { C_STRING_WITH_LEN("UCASE") }, SQL_FN(Item_func_upper, 1) },
  { { C_STRING_WITH_LEN("UNCOMPRESS") }, SQL_FN(Item_func_uncompress, 1) },
  { { C_STRING_WITH_LEN("UNCOMPRESSED_LENGTH") }, SQL_FN(Item_func_uncompressed_length, 1) },
  { { C_STRING_WITH_LEN("UNHEX") }, SQL_FN(Item_func_unhex, 1) },
  { { C_STRING_WITH_LEN("UNIX_TIMESTAMP") }, BUILDER(Create_func_unix_timestamp)},
  { { C_STRING_WITH_LEN("UPDATEXML") }, SQL_FN(Item_func_xml_update, 3) },
  { { C_STRING_WITH_LEN("UPPER") }, SQL_FN(Item_func_upper, 1) },
  { { C_STRING_WITH_LEN("UUID") }, SQL_FN(Item_func_uuid, 0) },
  { { C_STRING_WITH_LEN("UUID_SHORT") }, SQL_FN(Item_func_uuid_short, 0) },
  { { C_STRING_WITH_LEN("UUID_TO_BIN") }, BUILDER(Create_func_uuid_to_bin)},
  { { C_STRING_WITH_LEN("VALIDATE_PASSWORD_STRENGTH") }, SQL_FN(Item_func_validate_password_strength, 1) },
  { { C_STRING_WITH_LEN("VERSION") }, SQL_FN(Item_func_version, 0) },
  { { C_STRING_WITH_LEN("WEEKDAY") }, SQL_FACTORY(Weekday_instantiator) },
  { { C_STRING_WITH_LEN("WEEKOFYEAR") }, SQL_FACTORY(Weekofyear_instantiator) },
  { { C_STRING_WITH_LEN("YEARWEEK") }, BUILDER(Create_func_year_week)},
  { { C_STRING_WITH_LEN("GET_DD_COLUMN_PRIVILEGES") }, BUILDER(Create_func_get_dd_column_privileges)},
  { { C_STRING_WITH_LEN("GET_DD_INDEX_SUB_PART_LENGTH") },
                BUILDER(Create_func_get_dd_index_sub_part_length)},
  { { C_STRING_WITH_LEN("GET_DD_CREATE_OPTIONS") },
                BUILDER(Create_func_get_dd_create_options)},
  { { C_STRING_WITH_LEN("internal_dd_char_length") },
                BUILDER(Create_func_internal_dd_char_length)},
  { { C_STRING_WITH_LEN("can_access_database") },
                BUILDER(Create_func_can_access_database)},
  { { C_STRING_WITH_LEN("can_access_table") },
                BUILDER(Create_func_can_access_table)},
  { { C_STRING_WITH_LEN("can_access_column") },
                BUILDER(Create_func_can_access_column)},
  { { C_STRING_WITH_LEN("can_access_view") },
                BUILDER(Create_func_can_access_view)},
  { { C_STRING_WITH_LEN("internal_table_rows") },
                BUILDER(Create_func_internal_table_rows)},
  { { C_STRING_WITH_LEN("internal_avg_row_length") },
                BUILDER(Create_func_internal_avg_row_length)},
  { { C_STRING_WITH_LEN("internal_data_length") },
                BUILDER(Create_func_internal_data_length)},
  { { C_STRING_WITH_LEN("internal_max_data_length") },
                BUILDER(Create_func_internal_max_data_length)},
  { { C_STRING_WITH_LEN("internal_index_length") },
                BUILDER(Create_func_internal_index_length)},
  { { C_STRING_WITH_LEN("internal_data_free") },
                BUILDER(Create_func_internal_data_free)},
  { { C_STRING_WITH_LEN("internal_auto_increment") },
                BUILDER(Create_func_internal_auto_increment)},
  { { C_STRING_WITH_LEN("internal_checksum") },
                BUILDER(Create_func_internal_checksum)},
  { { C_STRING_WITH_LEN("internal_update_time") },
                BUILDER(Create_func_internal_update_time)},
  { { C_STRING_WITH_LEN("internal_check_time") },
                BUILDER(Create_func_internal_check_time)},
  { { C_STRING_WITH_LEN("internal_keys_disabled") },
                BUILDER(Create_func_internal_keys_disabled)},
  { { C_STRING_WITH_LEN("internal_index_column_cardinality") },
                BUILDER(Create_func_internal_index_column_cardinality)},
  { { C_STRING_WITH_LEN("internal_get_comment_or_error") },
                BUILDER(Create_func_internal_get_comment_or_error)},
  { { C_STRING_WITH_LEN("internal_get_view_warning_or_error") },
    BUILDER(Create_func_internal_get_view_warning_or_error)},
  { { C_STRING_WITH_LEN("GET_DD_TABLE_PRIVATE_DATA") },
                BUILDER(Create_func_get_dd_table_private_data)}
};

static HASH native_functions_hash;

static const uchar*
get_native_fct_hash_key(const uchar *buff, size_t *length)
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
  DBUG_ENTER("item_create_init");

  if (my_hash_init(& native_functions_hash,
                   system_charset_info,
                   array_elements(func_array),
                   0,
                   get_native_fct_hash_key,
                   nullptr,                          /* Nothing to free */
                   MYF(0),
                   key_memory_native_functions))
    DBUG_RETURN(1);

  for (const Native_func_registry &func : func_array)
  {
    if (my_hash_insert(& native_functions_hash,
                       reinterpret_cast<const uchar*>(&func)))
      DBUG_RETURN(1);
  }

#ifndef DBUG_OFF
  for (uint i=0 ; i < native_functions_hash.records ; i++)
  {
    Native_func_registry *func=
      (Native_func_registry*) my_hash_element(& native_functions_hash, i);
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
    longlong len= -1;
    const CHARSET_INFO *cs= type->charset;
    const CHARSET_INFO *real_cs=
      (cs ? cs : thd->variables.collation_connection);
    if (c_len)
    {
      int error;
      len= my_strtoll10(c_len, NULL, &error);
      if ((error != 0) || (len > MAX_FIELD_BLOBLENGTH))
      {
        my_error(ER_TOO_BIG_DISPLAYWIDTH, MYF(0), "cast as char", MAX_FIELD_BLOBLENGTH);
        return nullptr;
      }
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
  @param cs           Character set of str
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


Create_func_internal_dd_char_length
  Create_func_internal_dd_char_length::s_singleton;

Item*
Create_func_internal_dd_char_length::create_native(THD *thd, LEX_STRING name,
                                          PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 4)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();
  Item *param_3= item_list->pop_front();
  Item *param_4= item_list->pop_front();

  return new (thd->mem_root) Item_func_internal_dd_char_length(POS(),
                                                               param_1,
                                                               param_2,
                                                               param_3,
                                                               param_4);
}

Create_func_get_dd_table_private_data
  Create_func_get_dd_table_private_data::s_singleton;

Item*
Create_func_get_dd_table_private_data::create_native(THD *thd, LEX_STRING name,
                                                 PT_item_list *item_list)
{
  int arg_count= 0;

  if (item_list)
    arg_count= item_list->elements();

  // This native method should be invoked from the system views only.
  if (thd->parsing_system_view == false)
  {
    my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  if (arg_count != 2)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), name.str);
    return nullptr;
  }

  Item *param_1= item_list->pop_front();
  Item *param_2= item_list->pop_front();

  return new (thd->mem_root) Item_func_get_dd_table_private_data(
                               POS(), param_1, param_2);
}

/**
  @} (end of group GROUP_PARSER)
*/


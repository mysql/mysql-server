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
#include <functional>
#include <math.h>
#include <limits>
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
  We use this to declare that a function takes an infinite number of
  arguments. The cryptic construction below gives us the greatest number that
  the return type of PT_item_list::elements() can take.

  @see Function_factory::create_func()
*/
static const auto MAX_ARGLIST_SIZE=
  std::numeric_limits<decltype(PT_item_list().elements())>::max();


namespace {

/**
  Instantiates a function class with the list of arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.

  @tparam Min_argcount The minimum number of arguments. Not used in this
  general case.

  @tparam Max_argcount The maximum number of arguments. Not used in this
  general case.
*/

template<typename Function_class, uint Min_argc, uint Max_argc= Min_argc>
class Instantiator
{
public:
  static const uint Min_argcount= Min_argc;
  static const uint Max_argcount= Max_argc;

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


template<typename Function_class, uint Min_argc, uint Max_argc= Min_argc>
class Instantiator_with_thd
{
public:
  static const uint Min_argcount= Min_argc;
  static const uint Max_argcount= Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(thd, POS(), args);
  }
};


template<typename Function_class, Item_func::Functype Functype,
         uint Min_argc, uint Max_argc= Min_argc>
class Instantiator_with_functype
{
public:
  static const uint Min_argcount= Min_argc;
  static const uint Max_argcount= Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(thd, POS(), args, Functype);
  }
};


template<typename Function_class, Item_func::Functype Function_type>
class Instantiator_with_functype<Function_class, Function_type, 1, 1>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Function_class(POS(), (*args)[0], Function_type);
  }
};


template<typename Function_class, Item_func::Functype Function_type>
class Instantiator_with_functype<Function_class, Function_type, 2, 2>
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Function_class(POS(), (*args)[0], (*args)[1], Function_type);
  }
};


template<typename Function_class, uint Min_argc, uint Max_argc= Min_argc>
class List_instantiator
{
public:
  static const uint Min_argcount= Min_argc;
  static const uint Max_argcount= Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(POS(), args);
  }
};


template<typename Function_class, uint Min_argc, uint Max_argc= Min_argc>
class List_instantiator_with_thd
{
public:
  static const uint Min_argcount= Min_argc;
  static const uint Max_argcount= Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root) Function_class(thd, POS(), args);
  }
};


/**
  Instantiates a function class with one argument.

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


/**
  Instantiates a function class with four arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 4>
{
public:
  static const uint Min_argcount= 4;
  static const uint Max_argcount= 4;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Function_class(POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3]);
  }
};


/**
  Instantiates a function class with five arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 5>
{
public:
  static const uint Min_argcount= 5;
  static const uint Max_argcount= 5;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    return new (thd->mem_root)
      Function_class(POS(), (*args)[0], (*args)[1], (*args)[2],
                     (*args)[3], (*args)[4]);
  }
};


/**
  Instantiates a function class with zero or one arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 0, 1>
{
public:
  static const uint Min_argcount= 0;
  static const uint Max_argcount= 1;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    uint argcount= args == nullptr ? 0 : args->elements();
    switch (argcount)
    {
    case 0:
      return new (thd->mem_root) Function_class(POS());
    case 1:
      return new (thd->mem_root) Function_class(POS(), (*args)[0]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class with one or two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 1, 2>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Function_class(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class with between one and three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 1, 3>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 3;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Function_class(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
    case 3:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class taking between one and three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator_with_thd<Function_class, 1, 3>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 3;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Function_class(thd, POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root) Function_class(thd, POS(), (*args)[0], (*args)[1]);
    case 3:
      return new (thd->mem_root)
        Function_class(thd, POS(), (*args)[0], (*args)[1], (*args)[2]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class taking a thd and one or two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator_with_thd<Function_class, 1, 2>
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Function_class(thd, POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root) Function_class(thd, POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class with two or three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 2, 3>
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 3;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 2:
      return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
    case 3:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class with between two and four arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 2, 4>
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 4;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 2:
      return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
    case 3:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
    case 4:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


/**
  Instantiates a function class with two or three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template<typename Function_class>
class Instantiator<Function_class, 3, 5>
{
public:
  static const uint Min_argcount= 3;
  static const uint Max_argcount= 5;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 3:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
    case 4:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3]);
    case 5:
      return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3],
                       (*args)[4]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


template<Item_func::Functype Functype>
using Mbr_rel_instantiator=
  Instantiator_with_functype<Item_func_spatial_mbr_rel, Functype, 2>;

using Mbr_covered_by_instantiator=
  Mbr_rel_instantiator<Item_func::SP_COVEREDBY_FUNC>;
using Mbr_covers_instantiator=
  Mbr_rel_instantiator<Item_func::SP_COVERS_FUNC>;
using Mbr_contains_instantiator=
  Mbr_rel_instantiator<Item_func::SP_CONTAINS_FUNC>;
using Mbr_disjoint_instantiator=
  Mbr_rel_instantiator<Item_func::SP_DISJOINT_FUNC>;
using Mbr_equals_instantiator=
  Mbr_rel_instantiator<Item_func::SP_EQUALS_FUNC>;
using Mbr_intersects_instantiator=
  Mbr_rel_instantiator<Item_func::SP_INTERSECTS_FUNC>;
using Mbr_overlaps_instantiator=
  Mbr_rel_instantiator<Item_func::SP_OVERLAPS_FUNC>;
using Mbr_touches_instantiator=
  Mbr_rel_instantiator<Item_func::SP_TOUCHES_FUNC>;
using Mbr_within_instantiator=
  Mbr_rel_instantiator<Item_func::SP_WITHIN_FUNC>;
using Mbr_crosses_instantiator=
  Mbr_rel_instantiator<Item_func::SP_CROSSES_FUNC>;

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

using Intersection_instantiator=
  Spatial_instantiator<Item_func_spatial_operation::op_intersection>;
using Difference_instantiator=
  Spatial_instantiator<Item_func_spatial_operation::op_difference>;
using Union_instantiator=
  Spatial_instantiator<Item_func_spatial_operation::op_union>;
using Symdifference_instantiator=
  Spatial_instantiator<Item_func_spatial_operation::op_symdifference>;


template<Item_func::Functype Functype>
using Spatial_rel_instantiator=
  Instantiator_with_functype<Item_func_spatial_rel, Functype, 2>;

using St_contains_instantiator=
  Spatial_rel_instantiator<Item_func::SP_CONTAINS_FUNC>;
using St_crosses_instantiator=
  Spatial_rel_instantiator<Item_func::SP_CROSSES_FUNC>;
using St_disjoint_instantiator=
  Spatial_rel_instantiator<Item_func::SP_DISJOINT_FUNC>;
using St_equals_instantiator=
  Spatial_rel_instantiator<Item_func::SP_EQUALS_FUNC>;
using St_intersects_instantiator=
  Spatial_rel_instantiator<Item_func::SP_INTERSECTS_FUNC>;
using St_overlaps_instantiator=
  Spatial_rel_instantiator<Item_func::SP_OVERLAPS_FUNC>;
using St_touches_instantiator=
  Spatial_rel_instantiator<Item_func::SP_TOUCHES_FUNC>;
using St_within_instantiator=
  Spatial_rel_instantiator<Item_func::SP_WITHIN_FUNC>;


template<Item_func::Functype Functype>
using Spatial_decomp_instantiator=
  Instantiator_with_functype<Item_func_spatial_decomp, Functype, 1>;

using Startpoint_instantiator=
  Spatial_decomp_instantiator<Item_func::SP_STARTPOINT>;
using Endpoint_instantiator=
  Spatial_decomp_instantiator<Item_func::SP_ENDPOINT>;
using Exteriorring_instantiator=
  Spatial_decomp_instantiator<Item_func::SP_EXTERIORRING>;


template<Item_func::Functype Functype>
using Spatial_decomp_n_instantiator=
  Instantiator_with_functype<Item_func_spatial_decomp_n, Functype, 2>;

using Sp_geometryn_instantiator=
  Spatial_decomp_n_instantiator<Item_func::SP_GEOMETRYN>;

using Sp_interiorringn_instantiator=
  Spatial_decomp_n_instantiator<Item_func::SP_INTERIORRINGN>;

using Sp_pointn_instantiator=
  Spatial_decomp_n_instantiator<Item_func::SP_POINTN>;


template<typename Geometry_class, enum Geometry_class::Functype Functype>
class Geometry_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements()) {
    case 1:
      return new (thd->mem_root)
        Geometry_class(POS(), (*args)[0], Functype);
    case 2:
      return new (thd->mem_root)
        Geometry_class(POS(), (*args)[0], (*args)[1], Functype);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};

using txt_ft = Item_func_geometry_from_text::Functype;
using I_txt = Item_func_geometry_from_text;
template<typename Geometry_class, enum Geometry_class::Functype Functype>
using G_i = Geometry_instantiator<Geometry_class, Functype>;

using Geomcollfromtext_instantiator= G_i<I_txt, txt_ft::GEOMCOLLFROMTEXT>;
using Geomcollfromtxt_instantiator= G_i<I_txt, txt_ft::GEOMCOLLFROMTXT>;
using Geometrycollectionfromtext_instantiator=
  G_i<I_txt, txt_ft::GEOMETRYCOLLECTIONFROMTEXT>;
using Geometryfromtext_instantiator= G_i<I_txt, txt_ft::GEOMETRYFROMTEXT>;
using Geomfromtext_instantiator= G_i<I_txt, txt_ft::GEOMFROMTEXT>;
using Linefromtext_instantiator= G_i<I_txt, txt_ft::LINEFROMTEXT>;
using Linestringfromtext_instantiator= G_i<I_txt, txt_ft::LINESTRINGFROMTEXT>;
using Mlinefromtext_instantiator= G_i<I_txt, txt_ft::MLINEFROMTEXT>;
using Mpointfromtext_instantiator= G_i<I_txt, txt_ft::MPOINTFROMTEXT>;
using Mpolyfromtext_instantiator= G_i<I_txt, txt_ft::MPOLYFROMTEXT>;
using Multilinestringfromtext_instantiator=
  G_i<I_txt, txt_ft::MULTILINESTRINGFROMTEXT>;
using Multipointfromtext_instantiator= G_i<I_txt, txt_ft::MULTIPOINTFROMTEXT>;
using Multipolygonfromtext_instantiator=
  G_i<I_txt, txt_ft::MULTIPOLYGONFROMTEXT>;
using Pointfromtext_instantiator= G_i<I_txt, txt_ft::POINTFROMTEXT>;
using Polyfromtext_instantiator= G_i<I_txt, txt_ft::POLYFROMTEXT>;
using Polygonfromtext_instantiator= G_i<I_txt, txt_ft::POLYGONFROMTEXT>;

using wkb_ft = Item_func_geometry_from_wkb::Functype;
using I_wkb = Item_func_geometry_from_wkb;

using Geomcollfromwkb_instantiator= G_i<I_wkb, wkb_ft::GEOMCOLLFROMWKB>;
using Geometrycollectionfromwkb_instantiator=
  G_i<I_wkb, wkb_ft::GEOMETRYCOLLECTIONFROMWKB>;
using Geometryfromwkb_instantiator= G_i<I_wkb, wkb_ft::GEOMETRYFROMWKB>;
using Geomfromwkb_instantiator= G_i<I_wkb, wkb_ft::GEOMFROMWKB>;
using Linefromwkb_instantiator= G_i<I_wkb, wkb_ft::LINEFROMWKB>;
using Linestringfromwkb_instantiator= G_i<I_wkb, wkb_ft::LINESTRINGFROMWKB>;
using Mlinefromwkb_instantiator= G_i<I_wkb, wkb_ft::MLINEFROMWKB>;
using Mpointfromwkb_instantiator= G_i<I_wkb, wkb_ft::MPOINTFROMWKB>;
using Mpolyfromwkb_instantiator= G_i<I_wkb, wkb_ft::MPOLYFROMWKB>;
using Multilinestringfromwkb_instantiator=
  G_i<I_wkb, wkb_ft::MULTILINESTRINGFROMWKB>;
using Multipointfromwkb_instantiator= G_i<I_wkb, wkb_ft::MULTIPOINTFROMWKB>;
using Multipolygonfromwkb_instantiator= G_i<I_wkb, wkb_ft::MULTIPOLYGONFROMWKB>;
using Pointfromwkb_instantiator= G_i<I_wkb, wkb_ft::POINTFROMWKB>;
using Polyfromwkb_instantiator= G_i<I_wkb, wkb_ft::POLYFROMWKB>;
using Polygonfromwkb_instantiator= G_i<I_wkb, wkb_ft::POLYGONFROMWKB>;


class Encrypt_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    if (!thd->is_error())
      push_deprecated_warn(thd, "ENCRYPT", "AES_ENCRYPT");
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_encrypt(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_encrypt(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Des_encrypt_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    if (!thd->is_error())
      push_deprecated_warn(thd, "DES_ENCRYPT", "AES_ENCRYPT");
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_des_encrypt(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_des_encrypt(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Des_decrypt_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    if (!thd->is_error())
      push_deprecated_warn(thd, "DES_DECRYPT", "AES_DECRYPT");
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_des_decrypt(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_des_decrypt(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
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
    return new (thd->mem_root)
      Item_func_date_format(POS(), (*args)[0], (*args)[1], true);
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


class From_unixtime_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_from_unixtime(POS(), (*args)[0]);
    case 2:
      {
        Item *ut= new (thd->mem_root)
          Item_func_from_unixtime(POS(), (*args)[0]);
        return new (thd->mem_root)
          Item_func_date_format(POS(), ut, (*args)[1], 0);
      }
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Round_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      {
        Item *i0 = new (thd->mem_root) Item_int_0(POS());
        return new (thd->mem_root) Item_func_round(POS(), (*args)[0], i0, 0);
      }
    case 2:
      return new (thd->mem_root)
        Item_func_round(POS(), (*args)[0], (*args)[1], 0);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Locate_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= 3;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 2:
      /* Yes, parameters in that order : 2, 1 */
      return new (thd->mem_root)
        Item_func_locate(POS(), (*args)[1], (*args)[0]);
    case 3:
      /* Yes, parameters in that order : 2, 1, 3 */
      return new (thd->mem_root)
        Item_func_locate(POS(), (*args)[1], (*args)[0], (*args)[2]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Srid_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_get_srid(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_set_srid(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class X_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_get_x(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_set_x(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Y_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      return new (thd->mem_root) Item_func_get_y(POS(), (*args)[0]);
    case 2:
      return new (thd->mem_root)
        Item_func_set_y(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Yearweek_instantiator
{
public:
  static const uint Min_argcount= 1;
  static const uint Max_argcount= 2;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    switch (args->elements())
    {
    case 1:
      {
        Item *i0= new (thd->mem_root) Item_int_0(POS());
        return new (thd->mem_root) Item_func_yearweek(POS(), (*args)[0], i0);
      }
    case 2:
      return new (thd->mem_root)
        Item_func_yearweek(POS(), (*args)[0], (*args)[1]);
    default:
      DBUG_ASSERT(false);
      return nullptr;
    }
  }
};


class Make_set_instantiator
{
public:
  static const uint Min_argcount= 2;
  static const uint Max_argcount= MAX_ARGLIST_SIZE;

  Item *instantiate(THD *thd, PT_item_list *args)
  {
    Item *param_1= args->pop_front();
    return new (thd->mem_root) Item_func_make_set(POS(), param_1, args);
  }
};


/// @} (end of group Instantiators)


uint arglist_length(const PT_item_list *args)
{
  if (args == nullptr)
    return 0;
  return args->elements();
}

bool check_argcount_bounds(THD *thd, LEX_STRING function_name,
                           PT_item_list *item_list,
                           uint min_argcount,
                           uint max_argcount)
{
  uint argcount= arglist_length(item_list);
  if (argcount < min_argcount || argcount > max_argcount)
  {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
    return true;
  }
  return false;
}


namespace {

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
template<typename Instantiator_fn>
class Function_factory : public Create_func
{
public:
  static Function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name, PT_item_list *item_list)
    override
  {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    return m_instantiator.instantiate(thd, item_list);
  }

private:
  Function_factory() {}
  Instantiator_fn m_instantiator;
};

template<typename Instantiator_fn>
Function_factory<Instantiator_fn>
Function_factory<Instantiator_fn>::s_singleton;


template<typename Instantiator_fn>
class Odd_argcount_function_factory : public Create_func
{
public:
  static Odd_argcount_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name, PT_item_list *item_list)
    override
  {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    if (arglist_length(item_list) % 2 == 0)
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }
    return m_instantiator.instantiate(thd, item_list);
  }

private:
  Odd_argcount_function_factory() {}
  Instantiator_fn m_instantiator;
};

template<typename Instantiator_fn>
Odd_argcount_function_factory<Instantiator_fn>
Odd_argcount_function_factory<Instantiator_fn>::s_singleton;


template<typename Instantiator_fn>
class Even_argcount_function_factory : public Create_func
{
public:
  static Even_argcount_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name, PT_item_list *item_list)
    override
  {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    if (arglist_length(item_list) % 2 != 0)
    {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }
    return m_instantiator.instantiate(thd, item_list);
  }

private:
  Even_argcount_function_factory() {}
  Instantiator_fn m_instantiator;
};

template<typename Instantiator_fn>
Even_argcount_function_factory<Instantiator_fn>
Even_argcount_function_factory<Instantiator_fn>::s_singleton;


/**
  Factory for internal functions that should be invoked from the system views
  only.

  @tparam Instantiator See Function_factory.
*/
template<typename Instantiator_fn>
class Internal_function_factory : public Create_func
{
public:
  static Internal_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name, PT_item_list *item_list)
    override
  {
    if (!thd->parsing_system_view)
    {
      my_error(ER_NO_ACCESS_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }

    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    return m_instantiator.instantiate(thd, item_list);
  }

private:
  Internal_function_factory() {}
  Instantiator_fn m_instantiator;
};

template<typename Instantiator_fn>
Internal_function_factory<Instantiator_fn>
Internal_function_factory<Instantiator_fn>::s_singleton;


} //namespace

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


Item *
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


struct Native_func_registry
{
  LEX_STRING name;
  Create_func *builder;
};


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

/**
  Use this macro if you want to instantiate the Item_func object like
  `Item_func_xxx::Item_func_xxx(pos, args[0], ..., args[MAX])`

  This also instantiates the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_V(F, MIN, MAX) \
  &Function_factory<Instantiator<F, MIN, MAX>>::s_singleton

/**
  Use this macro if you want to instantiate the Item_func object like
  `Item_func_xxx::Item_func_xxx(thd, pos, args[0], ..., args[MAX])`

  This also instantiates the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_V_THD(F, MIN, MAX) \
  &Function_factory<Instantiator_with_thd<F, MIN, MAX>>::s_singleton

/**
  Use this macro if you want to instantiate the Item_func object like
  `Item_func_xxx::Item_func_xxx(pos, item_list)`

  This also instantiates the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_V_LIST(F, MIN, MAX) \
  &Function_factory<List_instantiator<F, MIN, MAX>>::s_singleton

/**
  Use this macro if you want to instantiate the Item_func object like
  `Item_func_xxx::Item_func_xxx(pos, item_list)`

  This also instantiates the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN_LIST(F, N) \
  &Function_factory<List_instantiator<F, N>>::s_singleton

/**
  Use this macro if you want to instantiate the Item_func object like
  `Item_func_xxx::Item_func_xxx(thd, pos, item_list)`

  This also instantiates the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_V_LIST_THD(F, MIN, MAX) \
  &Function_factory<List_instantiator_with_thd<F, MIN, MAX>>::s_singleton

/**
  Just like SQL_FN_V_THD, but enforces a check that the argument count is odd.
*/
#define SQL_FN_ODD(F, MIN, MAX) \
  &Odd_argcount_function_factory<List_instantiator_with_thd<F, MIN, MAX>> \
    ::s_singleton

/**
  Just like SQL_FN_V_THD, but enforces a check that the argument count is even.
*/
#define SQL_FN_EVEN(F, MIN, MAX) \
  &Even_argcount_function_factory<List_instantiator_with_thd<F, MIN, MAX>> \
    ::s_singleton

/**
  Like SQL_FN, but for functions that may only be referenced from system views.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN_INTERNAL(F, N) \
  &Internal_function_factory<Instantiator<F, N>>::s_singleton

/**
  Like SQL_FN_LIST, but for functions that may only be referenced from system
  views.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN_LIST_INTERNAL(F, N) \
  &Internal_function_factory<List_instantiator<F, N>>::s_singleton


/*
  MySQL native functions.
  MAINTAINER:
  - Keep sorted for human lookup. At runtime, a hash table is used.
  - do **NOT** conditionally (#ifdef, #ifndef) define a function *NAME*:
    doing so will cause user code that works against a --without-XYZ binary
    to fail with name collisions against a --with-XYZ binary.
  - keep 1 line per entry, it makes grep | sort easier
*/

static Native_func_registry func_array[] =
{
  { { C_STRING_WITH_LEN("ABS") }, SQL_FN(Item_func_abs, 1) },
  { { C_STRING_WITH_LEN("ACOS") }, SQL_FN(Item_func_acos, 1) },
  { { C_STRING_WITH_LEN("ADDTIME") }, SQL_FN(Item_func_add_time, 2) },
  { { C_STRING_WITH_LEN("AES_DECRYPT") }, SQL_FN_V(Item_func_aes_decrypt, 2, 3)},
  { { C_STRING_WITH_LEN("AES_ENCRYPT") }, SQL_FN_V(Item_func_aes_encrypt, 2, 3)},
  { { C_STRING_WITH_LEN("ANY_VALUE") }, SQL_FN(Item_func_any_value, 1) },
  { { C_STRING_WITH_LEN("ASIN") }, SQL_FN(Item_func_asin, 1) },
  { { C_STRING_WITH_LEN("ATAN") }, SQL_FN_V(Item_func_atan, 1, 2)},
  { { C_STRING_WITH_LEN("ATAN2") }, SQL_FN_V(Item_func_atan, 1, 2) },
  { { C_STRING_WITH_LEN("BENCHMARK") }, SQL_FN(Item_func_benchmark, 2) },
  { { C_STRING_WITH_LEN("BIN") }, SQL_FACTORY(Bin_instantiator) },
  { { C_STRING_WITH_LEN("BIN_TO_UUID") }, SQL_FN_V(Item_func_bin_to_uuid, 1, 2) },
  { { C_STRING_WITH_LEN("BIT_COUNT") }, SQL_FN(Item_func_bit_count, 1) },
  { { C_STRING_WITH_LEN("BIT_LENGTH") }, SQL_FN(Item_func_bit_length, 1) },
  { { C_STRING_WITH_LEN("CEIL") }, SQL_FN(Item_func_ceiling, 1) },
  { { C_STRING_WITH_LEN("CEILING") }, SQL_FN(Item_func_ceiling, 1) },
  { { C_STRING_WITH_LEN("CHARACTER_LENGTH") }, SQL_FN(Item_func_char_length, 1) },
  { { C_STRING_WITH_LEN("CHAR_LENGTH") }, SQL_FN(Item_func_char_length, 1) },
  { { C_STRING_WITH_LEN("COERCIBILITY") }, SQL_FN(Item_func_coercibility, 1) },
  { { C_STRING_WITH_LEN("COMPRESS") }, SQL_FN(Item_func_compress, 1) },
  { { C_STRING_WITH_LEN("CONCAT") }, SQL_FN_V(Item_func_concat, 1, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("CONCAT_WS") }, SQL_FN_V(Item_func_concat_ws, 2, MAX_ARGLIST_SIZE) },
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
  { { C_STRING_WITH_LEN("DES_DECRYPT") }, SQL_FACTORY(Des_decrypt_instantiator) },
  { { C_STRING_WITH_LEN("DES_ENCRYPT") }, SQL_FACTORY(Des_encrypt_instantiator) },
  { { C_STRING_WITH_LEN("ELT") }, SQL_FN_V(Item_func_elt, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("ENCODE") }, SQL_FN(Item_func_encode, 2) },
  { { C_STRING_WITH_LEN("ENCRYPT") }, SQL_FACTORY(Encrypt_instantiator) },
  { { C_STRING_WITH_LEN("EXP") }, SQL_FN(Item_func_exp, 1) },
  { { C_STRING_WITH_LEN("EXPORT_SET") }, SQL_FN_V(Item_func_export_set, 3, 5) },
  { { C_STRING_WITH_LEN("EXTRACTVALUE") }, SQL_FN(Item_func_xml_extractvalue, 2) },
  { { C_STRING_WITH_LEN("FIELD") }, SQL_FN_V(Item_func_field, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("FIND_IN_SET") }, SQL_FN(Item_func_find_in_set, 2) },
  { { C_STRING_WITH_LEN("FLOOR") }, SQL_FN(Item_func_floor, 1) },
  { { C_STRING_WITH_LEN("FOUND_ROWS") }, SQL_FN(Item_func_found_rows, 0) },
  { { C_STRING_WITH_LEN("FROM_BASE64") }, SQL_FN(Item_func_from_base64, 1) },
  { { C_STRING_WITH_LEN("FROM_DAYS") }, SQL_FN(Item_func_from_days, 1) },
  { { C_STRING_WITH_LEN("FROM_UNIXTIME") }, SQL_FACTORY(From_unixtime_instantiator) },
  { { C_STRING_WITH_LEN("GET_LOCK") }, SQL_FN(Item_func_get_lock, 2) },
  { { C_STRING_WITH_LEN("GREATEST") }, SQL_FN_V(Item_func_max, 2, MAX_ARGLIST_SIZE) },
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
  { { C_STRING_WITH_LEN("JSON_CONTAINS") }, SQL_FN_V_LIST_THD(Item_func_json_contains, 2, 3) },
  { { C_STRING_WITH_LEN("JSON_CONTAINS_PATH") }, SQL_FN_V_THD(Item_func_json_contains_path, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_LENGTH") }, SQL_FN_V_THD(Item_func_json_length, 1, 2) },
  { { C_STRING_WITH_LEN("JSON_DEPTH") }, SQL_FN(Item_func_json_depth, 1) },
  { { C_STRING_WITH_LEN("JSON_TYPE") }, SQL_FN(Item_func_json_type, 1) },
  { { C_STRING_WITH_LEN("JSON_KEYS") }, SQL_FN_V_THD(Item_func_json_keys, 1, 2) },
  { { C_STRING_WITH_LEN("JSON_EXTRACT") }, SQL_FN_V_THD(Item_func_json_extract, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_ARRAY_APPEND") }, SQL_FN_ODD(Item_func_json_array_append, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_INSERT") }, SQL_FN_ODD(Item_func_json_insert, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_ARRAY_INSERT") }, SQL_FN_ODD(Item_func_json_array_insert, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_OBJECT") }, SQL_FN_EVEN(Item_func_json_row_object, 0, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_SEARCH") }, SQL_FN_V_THD(Item_func_json_search, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_SET") }, SQL_FN_ODD(Item_func_json_set, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_REPLACE") }, SQL_FN_ODD(Item_func_json_replace, 3, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_ARRAY") }, SQL_FN_V_LIST_THD(Item_func_json_array, 0, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_REMOVE") }, SQL_FN_V_LIST_THD(Item_func_json_remove, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_MERGE") }, SQL_FN_V_LIST_THD(Item_func_json_merge, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("JSON_QUOTE") }, SQL_FN_LIST(Item_func_json_quote, 1) },
  { { C_STRING_WITH_LEN("JSON_UNQUOTE") }, SQL_FN_LIST(Item_func_json_unquote, 1) },
  { { C_STRING_WITH_LEN("IS_FREE_LOCK") }, SQL_FN(Item_func_is_free_lock, 1) },
  { { C_STRING_WITH_LEN("IS_USED_LOCK") }, SQL_FN(Item_func_is_used_lock, 1) },
  { { C_STRING_WITH_LEN("LAST_DAY") }, SQL_FN(Item_func_last_day, 1) },
  { { C_STRING_WITH_LEN("LAST_INSERT_ID") }, SQL_FN_V(Item_func_last_insert_id, 0, 1) },
  { { C_STRING_WITH_LEN("LCASE") }, SQL_FN(Item_func_lower, 1) },
  { { C_STRING_WITH_LEN("LEAST") }, SQL_FN_V_LIST(Item_func_min, 2, MAX_ARGLIST_SIZE) },
  { { C_STRING_WITH_LEN("LENGTH") }, SQL_FN(Item_func_length, 1) },
#ifndef DBUG_OFF
  { { C_STRING_WITH_LEN("LIKE_RANGE_MIN") }, SQL_FN(Item_func_like_range_min, 2) },
  { { C_STRING_WITH_LEN("LIKE_RANGE_MAX") }, SQL_FN(Item_func_like_range_max, 2) },
#endif
  { { C_STRING_WITH_LEN("LN") }, SQL_FN(Item_func_ln, 1) },
  { { C_STRING_WITH_LEN("LOAD_FILE") }, SQL_FN(Item_load_file, 1) },
  { { C_STRING_WITH_LEN("LOCATE") }, SQL_FACTORY(Locate_instantiator) },
  { { C_STRING_WITH_LEN("LOG") }, SQL_FN_V(Item_func_log, 1, 2) },
  { { C_STRING_WITH_LEN("LOG10") }, SQL_FN(Item_func_log10, 1)},
  { { C_STRING_WITH_LEN("LOG2") }, SQL_FN(Item_func_log2, 1)},
  { { C_STRING_WITH_LEN("LOWER") }, SQL_FN(Item_func_lower, 1) },
  { { C_STRING_WITH_LEN("LPAD") }, SQL_FN(Item_func_lpad, 3) },
  { { C_STRING_WITH_LEN("LTRIM") }, SQL_FN(Item_func_ltrim, 1) },
  { { C_STRING_WITH_LEN("MAKEDATE") }, SQL_FN(Item_func_makedate, 2) },
  { { C_STRING_WITH_LEN("MAKETIME") }, SQL_FN(Item_func_maketime, 3) },
  { { C_STRING_WITH_LEN("MAKE_SET") }, SQL_FACTORY(Make_set_instantiator)},
  { { C_STRING_WITH_LEN("MASTER_POS_WAIT") }, SQL_FN_V(Item_master_pos_wait, 2, 4) },
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
  { { C_STRING_WITH_LEN("RAND") }, SQL_FN_V(Item_func_rand, 0, 1) },
  { { C_STRING_WITH_LEN("RANDOM_BYTES") }, SQL_FN(Item_func_random_bytes, 1) },
  { { C_STRING_WITH_LEN("RELEASE_ALL_LOCKS") }, SQL_FN(Item_func_release_all_locks, 0) },
  { { C_STRING_WITH_LEN("RELEASE_LOCK") }, SQL_FN(Item_func_release_lock, 1) },
  { { C_STRING_WITH_LEN("REVERSE") }, SQL_FN(Item_func_reverse, 1) },
  { { C_STRING_WITH_LEN("ROLES_GRAPHML") }, SQL_FN(Item_func_roles_graphml, 0) },
  { { C_STRING_WITH_LEN("ROUND") }, SQL_FACTORY(Round_instantiator) },
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
  { { C_STRING_WITH_LEN("WAIT_FOR_EXECUTED_GTID_SET") }, SQL_FN_V(Item_wait_for_executed_gtid_set, 1, 2) },
  { { C_STRING_WITH_LEN("WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS") }, SQL_FN_V(Item_master_gtid_set_wait, 1, 3) },
  { { C_STRING_WITH_LEN("SQRT") }, SQL_FN(Item_func_sqrt, 1) },
  { { C_STRING_WITH_LEN("STRCMP") }, SQL_FN(Item_func_strcmp, 2) },
  { { C_STRING_WITH_LEN("STR_TO_DATE") }, SQL_FN(Item_func_str_to_date, 2) },
  { { C_STRING_WITH_LEN("ST_AREA") }, SQL_FN(Item_func_area, 1) },
  { { C_STRING_WITH_LEN("ST_ASBINARY") }, SQL_FN(Item_func_as_wkb, 1) },
  { { C_STRING_WITH_LEN("ST_ASGEOJSON") }, SQL_FN_V_THD(Item_func_as_geojson, 1, 3) },
  { { C_STRING_WITH_LEN("ST_ASTEXT") }, SQL_FN(Item_func_as_wkt, 1) },
  { { C_STRING_WITH_LEN("ST_ASWKB") }, SQL_FN_V(Item_func_as_wkb, 1, 2) },
  { { C_STRING_WITH_LEN("ST_ASWKT") }, SQL_FN(Item_func_as_wkt, 1) },
  { { C_STRING_WITH_LEN("ST_BUFFER") }, SQL_FN_V_LIST(Item_func_buffer, 2, 5) },
  { { C_STRING_WITH_LEN("ST_BUFFER_STRATEGY") }, SQL_FN_V_LIST(Item_func_buffer_strategy, 1, 2) },
  { { C_STRING_WITH_LEN("ST_CENTROID") }, SQL_FN(Item_func_centroid, 1) },
  { { C_STRING_WITH_LEN("ST_CONTAINS") }, SQL_FACTORY(St_contains_instantiator) },
  { { C_STRING_WITH_LEN("ST_CONVEXHULL") }, SQL_FN(Item_func_convex_hull, 1) },
  { { C_STRING_WITH_LEN("ST_CROSSES") }, SQL_FACTORY(St_crosses_instantiator) },
  { { C_STRING_WITH_LEN("ST_DIFFERENCE") }, SQL_FACTORY(Difference_instantiator) },
  { { C_STRING_WITH_LEN("ST_DIMENSION") }, SQL_FN(Item_func_dimension, 1) },
  { { C_STRING_WITH_LEN("ST_DISJOINT") }, SQL_FACTORY(St_disjoint_instantiator) },
  { { C_STRING_WITH_LEN("ST_DISTANCE") }, SQL_FN_LIST(Item_func_distance, 2) },
  { { C_STRING_WITH_LEN("ST_DISTANCE_SPHERE") }, SQL_FN_V_LIST(Item_func_distance_sphere, 2, 3) },
  { { C_STRING_WITH_LEN("ST_ENDPOINT") }, SQL_FACTORY(Endpoint_instantiator) },
  { { C_STRING_WITH_LEN("ST_ENVELOPE") }, SQL_FN(Item_func_envelope, 1) },
  { { C_STRING_WITH_LEN("ST_EQUALS") }, SQL_FACTORY(St_equals_instantiator) },
  { { C_STRING_WITH_LEN("ST_EXTERIORRING") }, SQL_FACTORY(Exteriorring_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOHASH") }, SQL_FN_V(Item_func_geohash, 2, 3) },
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTEXT") }, SQL_FACTORY(Geomcollfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMTXT") }, SQL_FACTORY(Geomcollfromtxt_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMCOLLFROMWKB") }, SQL_FACTORY(Geomcollfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMTEXT") }, SQL_FACTORY(Geometrycollectionfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYCOLLECTIONFROMWKB") }, SQL_FACTORY(Geometrycollectionfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMTEXT") }, SQL_FACTORY(Geometryfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYFROMWKB") }, SQL_FACTORY(Geometryfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYN") }, SQL_FACTORY(Sp_geometryn_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMETRYTYPE") }, SQL_FN(Item_func_geometry_type, 1) },
  { { C_STRING_WITH_LEN("ST_GEOMFROMGEOJSON") }, SQL_FN_V(Item_func_geomfromgeojson, 1, 3) },
  { { C_STRING_WITH_LEN("ST_GEOMFROMTEXT") }, SQL_FACTORY(Geomfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_GEOMFROMWKB") }, SQL_FACTORY(Geomfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_INTERIORRINGN") }, SQL_FACTORY(Sp_interiorringn_instantiator) },
  { { C_STRING_WITH_LEN("ST_INTERSECTS") }, SQL_FACTORY(St_intersects_instantiator) },
  { { C_STRING_WITH_LEN("ST_INTERSECTION") }, SQL_FACTORY(Intersection_instantiator) },
  { { C_STRING_WITH_LEN("ST_ISCLOSED") }, SQL_FN(Item_func_isclosed, 1) },
  { { C_STRING_WITH_LEN("ST_ISEMPTY") }, SQL_FN(Item_func_isempty, 1) },
  { { C_STRING_WITH_LEN("ST_ISSIMPLE") }, SQL_FN(Item_func_issimple, 1) },
  { { C_STRING_WITH_LEN("ST_ISVALID") }, SQL_FN(Item_func_isvalid, 1) },
  { { C_STRING_WITH_LEN("ST_LATFROMGEOHASH") }, SQL_FN(Item_func_latfromgeohash, 1) },
  { { C_STRING_WITH_LEN("ST_LENGTH") }, SQL_FN(Item_func_glength, 1) },
  { { C_STRING_WITH_LEN("ST_LINEFROMTEXT") }, SQL_FACTORY(Linefromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_LINEFROMWKB") }, SQL_FACTORY(Linefromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMTEXT") }, SQL_FACTORY(Linestringfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_LINESTRINGFROMWKB") }, SQL_FACTORY(Linestringfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_LONGFROMGEOHASH") }, SQL_FN(Item_func_longfromgeohash, 1) },
  { { C_STRING_WITH_LEN("ST_MAKEENVELOPE") }, SQL_FN(Item_func_make_envelope, 2) },
  { { C_STRING_WITH_LEN("ST_MLINEFROMTEXT") }, SQL_FACTORY(Mlinefromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MLINEFROMWKB") }, SQL_FACTORY(Mlinefromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_MPOINTFROMTEXT") }, SQL_FACTORY(Mpointfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MPOINTFROMWKB") }, SQL_FACTORY(Mpointfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_MPOLYFROMTEXT") }, SQL_FACTORY(Mpolyfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MPOLYFROMWKB") }, SQL_FACTORY(Mpolyfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTILINESTRINGFROMTEXT") }, SQL_FACTORY(Multilinestringfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTILINESTRINGFROMWKB") }, SQL_FACTORY(Multilinestringfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTIPOINTFROMTEXT") }, SQL_FACTORY(Multipointfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTIPOINTFROMWKB") }, SQL_FACTORY(Multipointfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTIPOLYGONFROMTEXT") }, SQL_FACTORY(Multipolygonfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_MULTIPOLYGONFROMWKB") }, SQL_FACTORY(Multipolygonfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_NUMGEOMETRIES") }, SQL_FN(Item_func_numgeometries, 1) },
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRING") }, SQL_FN(Item_func_numinteriorring, 1) },
  { { C_STRING_WITH_LEN("ST_NUMINTERIORRINGS") }, SQL_FN(Item_func_numinteriorring, 1) },
  { { C_STRING_WITH_LEN("ST_NUMPOINTS") }, SQL_FN(Item_func_numpoints, 1) },
  { { C_STRING_WITH_LEN("ST_OVERLAPS") }, SQL_FACTORY(St_overlaps_instantiator) },
  { { C_STRING_WITH_LEN("ST_POINTFROMGEOHASH") }, SQL_FN(Item_func_pointfromgeohash, 2) },
  { { C_STRING_WITH_LEN("ST_POINTFROMTEXT") }, SQL_FACTORY(Pointfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_POINTFROMWKB") }, SQL_FACTORY(Pointfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_POINTN") }, SQL_FACTORY(Sp_pointn_instantiator) },
  { { C_STRING_WITH_LEN("ST_POLYFROMTEXT") }, SQL_FACTORY(Polyfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_POLYFROMWKB") }, SQL_FACTORY(Polyfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_POLYGONFROMTEXT") }, SQL_FACTORY(Polygonfromtext_instantiator) },
  { { C_STRING_WITH_LEN("ST_POLYGONFROMWKB") }, SQL_FACTORY(Polygonfromwkb_instantiator) },
  { { C_STRING_WITH_LEN("ST_SIMPLIFY") }, SQL_FN(Item_func_simplify, 2) },
  { { C_STRING_WITH_LEN("ST_SRID") }, SQL_FACTORY(Srid_instantiator) },
  { { C_STRING_WITH_LEN("ST_STARTPOINT") }, SQL_FACTORY(Startpoint_instantiator) },
  { { C_STRING_WITH_LEN("ST_SYMDIFFERENCE") }, SQL_FACTORY(Symdifference_instantiator) },
  { { C_STRING_WITH_LEN("ST_SWAPXY") }, SQL_FN(Item_func_swap_xy, 1) },
  { { C_STRING_WITH_LEN("ST_TOUCHES") }, SQL_FACTORY(St_touches_instantiator) },
  { { C_STRING_WITH_LEN("ST_UNION") }, SQL_FACTORY(Union_instantiator) },
  { { C_STRING_WITH_LEN("ST_VALIDATE") }, SQL_FN(Item_func_validate, 1) },
  { { C_STRING_WITH_LEN("ST_WITHIN") }, SQL_FACTORY(St_within_instantiator) },
  { { C_STRING_WITH_LEN("ST_X") }, SQL_FACTORY(X_instantiator) },
  { { C_STRING_WITH_LEN("ST_Y") }, SQL_FACTORY(Y_instantiator) },
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
  { { C_STRING_WITH_LEN("UNIX_TIMESTAMP") }, SQL_FN_V(Item_func_unix_timestamp, 0, 1) },
  { { C_STRING_WITH_LEN("UPDATEXML") }, SQL_FN(Item_func_xml_update, 3) },
  { { C_STRING_WITH_LEN("UPPER") }, SQL_FN(Item_func_upper, 1) },
  { { C_STRING_WITH_LEN("UUID") }, SQL_FN(Item_func_uuid, 0) },
  { { C_STRING_WITH_LEN("UUID_SHORT") }, SQL_FN(Item_func_uuid_short, 0) },
  { { C_STRING_WITH_LEN("UUID_TO_BIN") }, SQL_FN_V(Item_func_uuid_to_bin, 1, 2) },
  { { C_STRING_WITH_LEN("VALIDATE_PASSWORD_STRENGTH") }, SQL_FN(Item_func_validate_password_strength, 1) },
  { { C_STRING_WITH_LEN("VERSION") }, SQL_FN(Item_func_version, 0) },
  { { C_STRING_WITH_LEN("WEEKDAY") }, SQL_FACTORY(Weekday_instantiator) },
  { { C_STRING_WITH_LEN("WEEKOFYEAR") }, SQL_FACTORY(Weekofyear_instantiator) },
  { { C_STRING_WITH_LEN("YEARWEEK") }, SQL_FACTORY(Yearweek_instantiator) },
  { { C_STRING_WITH_LEN("GET_DD_COLUMN_PRIVILEGES") }, SQL_FN_INTERNAL(Item_func_get_dd_column_privileges, 3) },
  { { C_STRING_WITH_LEN("GET_DD_INDEX_SUB_PART_LENGTH") }, SQL_FN_INTERNAL(Item_func_get_dd_index_sub_part_length, 5) },
  { { C_STRING_WITH_LEN("GET_DD_CREATE_OPTIONS") }, SQL_FN_INTERNAL(Item_func_get_dd_create_options, 2) },
  { { C_STRING_WITH_LEN("internal_dd_char_length") }, SQL_FN_INTERNAL(Item_func_internal_dd_char_length, 4) },
  { { C_STRING_WITH_LEN("can_access_database") }, SQL_FN_INTERNAL(Item_func_can_access_database, 1) },
  { { C_STRING_WITH_LEN("can_access_table") }, SQL_FN_INTERNAL(Item_func_can_access_table, 2) },
  { { C_STRING_WITH_LEN("can_access_column") }, SQL_FN_INTERNAL(Item_func_can_access_column, 3) },
  { { C_STRING_WITH_LEN("can_access_view") }, SQL_FN_INTERNAL(Item_func_can_access_view, 4) },
  { { C_STRING_WITH_LEN("internal_table_rows") }, SQL_FN_INTERNAL(Item_func_internal_table_rows, 4) },
  { { C_STRING_WITH_LEN("internal_avg_row_length") }, SQL_FN_INTERNAL(Item_func_internal_avg_row_length, 4) },
  { { C_STRING_WITH_LEN("internal_data_length") }, SQL_FN_INTERNAL(Item_func_internal_data_length, 4) },
  { { C_STRING_WITH_LEN("internal_max_data_length") }, SQL_FN_INTERNAL(Item_func_internal_max_data_length, 4) },
  { { C_STRING_WITH_LEN("internal_index_length") }, SQL_FN_INTERNAL(Item_func_internal_index_length, 4) },
  { { C_STRING_WITH_LEN("internal_data_free") }, SQL_FN_INTERNAL(Item_func_internal_data_free, 4) },
  { { C_STRING_WITH_LEN("internal_auto_increment") }, SQL_FN_INTERNAL(Item_func_internal_auto_increment, 4) },
  { { C_STRING_WITH_LEN("internal_checksum") }, SQL_FN_INTERNAL(Item_func_internal_checksum, 4) },
  { { C_STRING_WITH_LEN("internal_update_time") }, SQL_FN_INTERNAL(Item_func_internal_update_time, 4) },
  { { C_STRING_WITH_LEN("internal_check_time") }, SQL_FN_INTERNAL(Item_func_internal_check_time, 4) },
  { { C_STRING_WITH_LEN("internal_keys_disabled") }, SQL_FN_INTERNAL(Item_func_internal_keys_disabled, 1) },
  { { C_STRING_WITH_LEN("internal_index_column_cardinality") }, SQL_FN_LIST_INTERNAL(Item_func_internal_index_column_cardinality, 7) },
  { { C_STRING_WITH_LEN("internal_get_comment_or_error") }, SQL_FN_LIST_INTERNAL(Item_func_internal_get_comment_or_error, 5) },
  { { C_STRING_WITH_LEN("internal_get_view_warning_or_error") }, SQL_FN_LIST_INTERNAL(Item_func_internal_get_view_warning_or_error, 4) }
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


/**
  @} (end of group GROUP_PARSER)
*/


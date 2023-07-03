/*
<<<<<<< HEAD
   Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.
=======
   Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file sql/item_create.cc

  Functions to create an item. Used by sql_yacc.yy
*/

#include "sql/item_create.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

#include "decimal.h"
#include "field_types.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"    // Item_func_any_value
#include "sql/item_func.h"       // Item_func_udf_str
#include "sql/item_geofunc.h"    // Item_func_st_area
#include "sql/item_gtid_func.h"  // Item_wait_for_executed_gtid_set Item_master_gtid_set_wait Item_func_gtid_subset
#include "sql/item_inetfunc.h"   // Item_func_inet_ntoa
#include "sql/item_json_func.h"  // Item_func_json
#include "sql/item_pfs_func.h"   // Item_pfs_func_thread_id
#include "sql/item_regexp_func.h"  // Item_func_regexp_xxx
#include "sql/item_strfunc.h"      // Item_func_aes_encrypt
#include "sql/item_sum.h"          // Item_sum_udf_str
#include "sql/item_timefunc.h"     // Item_func_add_time
#include "sql/item_xmlfunc.h"      // Item_func_xml_extractvalue
#include "sql/my_decimal.h"
#include "sql/parse_location.h"
#include "sql/parse_tree_helpers.h"  // PT_item_list
#include "sql/parser_yystype.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"  // handle_std_exception
#include "sql/sql_lex.h"
#include "sql/sql_time.h"  // str_to_datetime
#include "sql/sql_udf.h"
#include "sql/system_variables.h"
#include "sql_string.h"
#include "tztime.h"  // convert_time_zone_displacement

/**
  @addtogroup GROUP_PARSER
  @{
*/

namespace {

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
constexpr auto MAX_ARGLIST_SIZE =
    std::numeric_limits<decltype(PT_item_list().elements())>::max();

/**
  Instantiates a function class with the list of arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.

  @tparam Min_argc The minimum number of arguments. Not used in this
  general case.

  @tparam Max_argc The maximum number of arguments. Not used in this
  general case.
*/

template <typename Function_class, uint Min_argc, uint Max_argc = Min_argc>
class Instantiator {
 public:
  static const uint Min_argcount = Min_argc;
  static const uint Max_argcount = Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(POS(), args);
  }
};

/**
  Instantiates a function class with no arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 0> {
 public:
  static const uint Min_argcount = 0;
  static const uint Max_argcount = 0;
  Item *instantiate(THD *thd, PT_item_list *) {
    return new (thd->mem_root) Function_class(POS());
  }
};

template <typename Function_class, uint Min_argc, uint Max_argc = Min_argc>
class Instantiator_with_thd {
 public:
  static const uint Min_argcount = Min_argc;
  static const uint Max_argcount = Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(thd, POS(), args);
  }
};

template <typename Function_class, Item_func::Functype Functype, uint Min_argc,
          uint Max_argc = Min_argc>
class Instantiator_with_functype {
 public:
  static const uint Min_argcount = Min_argc;
  static const uint Max_argcount = Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(thd, POS(), args, Functype);
  }
};

template <typename Function_class, Item_func::Functype Function_type>
class Instantiator_with_functype<Function_class, Function_type, 1, 1> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(POS(), (*args)[0], Function_type);
  }
};

template <typename Function_class, Item_func::Functype Function_type>
class Instantiator_with_functype<Function_class, Function_type, 2, 2> {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], Function_type);
  }
};

template <typename Function_class, uint Min_argc, uint Max_argc = Min_argc>
class List_instantiator {
 public:
  static const uint Min_argcount = Min_argc;
  static const uint Max_argcount = Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(POS(), args);
  }
};

template <typename Function_class, uint Min_argc, uint Max_argc = Min_argc>
class List_instantiator_with_thd {
 public:
  static const uint Min_argcount = Min_argc;
  static const uint Max_argcount = Max_argc;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(thd, POS(), args);
  }
};

/**
  Instantiates a function class with one argument.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 1> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(POS(), (*args)[0]);
  }
};

/**
  Instantiates a function class with two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 2> {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1]);
  }
};

/**
  Instantiates a function class with three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 3> {
 public:
  static const uint Min_argcount = 3;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
  }
};

/**
  Instantiates a function class with four arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 4> {
 public:
  static const uint Min_argcount = 4;
  static const uint Max_argcount = 4;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root)
        Function_class(POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3]);
  }
};

/**
  Instantiates a function class with five arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 5> {
 public:
  static const uint Min_argcount = 5;
  static const uint Max_argcount = 5;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Function_class(
        POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3], (*args)[4]);
  }
};

/**
  Instantiates a function class with zero or one arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 0, 1> {
 public:
  static const uint Min_argcount = 0;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    uint argcount = args == nullptr ? 0 : args->elements();
    switch (argcount) {
      case 0:
        return new (thd->mem_root) Function_class(POS());
      case 1:
        return new (thd->mem_root) Function_class(POS(), (*args)[0]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with one or two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 1, 2> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Function_class(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with between one and three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 1, 3> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Function_class(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1]);
      case 3:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class taking between one and three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator_with_thd<Function_class, 1, 3> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Function_class(thd, POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Function_class(thd, POS(), (*args)[0], (*args)[1]);
      case 3:
        return new (thd->mem_root)
            Function_class(thd, POS(), (*args)[0], (*args)[1], (*args)[2]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class taking a thd and one or two arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator_with_thd<Function_class, 1, 2> {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Function_class(thd, POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Function_class(thd, POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with two or three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 2, 3> {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 2:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1]);
      case 3:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with between two and four arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 2, 4> {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 4;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 2:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1]);
      case 3:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
      case 4:
        return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1],
                                                  (*args)[2], (*args)[3]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with between two and six arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 2, 6> {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 6;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 2:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1]);
      case 3:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
      case 4:
        return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1],
                                                  (*args)[2], (*args)[3]);
      case 5:
        return new (thd->mem_root) Function_class(
            POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3], (*args)[4]);
      case 6:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2],
                           (*args)[3], (*args)[4], (*args)[5]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

/**
  Instantiates a function class with two or three arguments.

  @tparam Function_class The class that implements the function. Does not need
  to inherit Item_func.
*/
template <typename Function_class>
class Instantiator<Function_class, 3, 5> {
 public:
  static const uint Min_argcount = 3;
  static const uint Max_argcount = 5;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 3:
        return new (thd->mem_root)
            Function_class(POS(), (*args)[0], (*args)[1], (*args)[2]);
      case 4:
        return new (thd->mem_root) Function_class(POS(), (*args)[0], (*args)[1],
                                                  (*args)[2], (*args)[3]);
      case 5:
        return new (thd->mem_root) Function_class(
            POS(), (*args)[0], (*args)[1], (*args)[2], (*args)[3], (*args)[4]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

template <Item_func::Functype Functype>
using Spatial_decomp_instantiator =
    Instantiator_with_functype<Item_func_spatial_decomp, Functype, 1>;

using Startpoint_instantiator =
    Spatial_decomp_instantiator<Item_func::SP_STARTPOINT>;
using Endpoint_instantiator =
    Spatial_decomp_instantiator<Item_func::SP_ENDPOINT>;
using Exteriorring_instantiator =
    Spatial_decomp_instantiator<Item_func::SP_EXTERIORRING>;

template <Item_func::Functype Functype>
using Spatial_decomp_n_instantiator =
    Instantiator_with_functype<Item_func_spatial_decomp_n, Functype, 2>;

using Sp_geometryn_instantiator =
    Spatial_decomp_n_instantiator<Item_func::SP_GEOMETRYN>;

using Sp_interiorringn_instantiator =
    Spatial_decomp_n_instantiator<Item_func::SP_INTERIORRINGN>;

using Sp_pointn_instantiator =
    Spatial_decomp_n_instantiator<Item_func::SP_POINTN>;

template <typename Geometry_class, enum Geometry_class::Functype Functype>
class Geometry_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Geometry_class(POS(), (*args)[0], Functype);
      case 2:
        return new (thd->mem_root)
            Geometry_class(POS(), (*args)[0], (*args)[1], Functype);
      case 3:
        return new (thd->mem_root)
            Geometry_class(POS(), (*args)[0], (*args)[1], (*args)[2], Functype);
      default:
        assert(false);
        return nullptr;
    }
  }
};

using txt_ft = Item_func_geometry_from_text::Functype;
using I_txt = Item_func_geometry_from_text;
template <typename Geometry_class, enum Geometry_class::Functype Functype>
using G_i = Geometry_instantiator<Geometry_class, Functype>;

using Geomcollfromtext_instantiator = G_i<I_txt, txt_ft::GEOMCOLLFROMTEXT>;
using Geomcollfromtxt_instantiator = G_i<I_txt, txt_ft::GEOMCOLLFROMTXT>;
using Geometrycollectionfromtext_instantiator =
    G_i<I_txt, txt_ft::GEOMETRYCOLLECTIONFROMTEXT>;
using Geometryfromtext_instantiator = G_i<I_txt, txt_ft::GEOMETRYFROMTEXT>;
using Geomfromtext_instantiator = G_i<I_txt, txt_ft::GEOMFROMTEXT>;
using Linefromtext_instantiator = G_i<I_txt, txt_ft::LINEFROMTEXT>;
using Linestringfromtext_instantiator = G_i<I_txt, txt_ft::LINESTRINGFROMTEXT>;
using Mlinefromtext_instantiator = G_i<I_txt, txt_ft::MLINEFROMTEXT>;
using Mpointfromtext_instantiator = G_i<I_txt, txt_ft::MPOINTFROMTEXT>;
using Mpolyfromtext_instantiator = G_i<I_txt, txt_ft::MPOLYFROMTEXT>;
using Multilinestringfromtext_instantiator =
    G_i<I_txt, txt_ft::MULTILINESTRINGFROMTEXT>;
using Multipointfromtext_instantiator = G_i<I_txt, txt_ft::MULTIPOINTFROMTEXT>;
using Multipolygonfromtext_instantiator =
    G_i<I_txt, txt_ft::MULTIPOLYGONFROMTEXT>;
using Pointfromtext_instantiator = G_i<I_txt, txt_ft::POINTFROMTEXT>;
using Polyfromtext_instantiator = G_i<I_txt, txt_ft::POLYFROMTEXT>;
using Polygonfromtext_instantiator = G_i<I_txt, txt_ft::POLYGONFROMTEXT>;

using wkb_ft = Item_func_geometry_from_wkb::Functype;
using I_wkb = Item_func_geometry_from_wkb;

using Geomcollfromwkb_instantiator = G_i<I_wkb, wkb_ft::GEOMCOLLFROMWKB>;
using Geometrycollectionfromwkb_instantiator =
    G_i<I_wkb, wkb_ft::GEOMETRYCOLLECTIONFROMWKB>;
using Geometryfromwkb_instantiator = G_i<I_wkb, wkb_ft::GEOMETRYFROMWKB>;
using Geomfromwkb_instantiator = G_i<I_wkb, wkb_ft::GEOMFROMWKB>;
using Linefromwkb_instantiator = G_i<I_wkb, wkb_ft::LINEFROMWKB>;
using Linestringfromwkb_instantiator = G_i<I_wkb, wkb_ft::LINESTRINGFROMWKB>;
using Mlinefromwkb_instantiator = G_i<I_wkb, wkb_ft::MLINEFROMWKB>;
using Mpointfromwkb_instantiator = G_i<I_wkb, wkb_ft::MPOINTFROMWKB>;
using Mpolyfromwkb_instantiator = G_i<I_wkb, wkb_ft::MPOLYFROMWKB>;
using Multilinestringfromwkb_instantiator =
    G_i<I_wkb, wkb_ft::MULTILINESTRINGFROMWKB>;
using Multipointfromwkb_instantiator = G_i<I_wkb, wkb_ft::MULTIPOINTFROMWKB>;
using Multipolygonfromwkb_instantiator =
    G_i<I_wkb, wkb_ft::MULTIPOLYGONFROMWKB>;
using Pointfromwkb_instantiator = G_i<I_wkb, wkb_ft::POINTFROMWKB>;
using Polyfromwkb_instantiator = G_i<I_wkb, wkb_ft::POLYFROMWKB>;
using Polygonfromwkb_instantiator = G_i<I_wkb, wkb_ft::POLYGONFROMWKB>;

class Bin_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    POS pos;
    Item *i10 = new (thd->mem_root) Item_int(pos, 10, 2);
    Item *i2 = new (thd->mem_root) Item_int(pos, 2, 1);
    return new (thd->mem_root) Item_func_conv(pos, (*args)[0], i10, i2);
  }
};

class Oct_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    Item *i10 = new (thd->mem_root) Item_int(POS(), 10, 2);
    Item *i8 = new (thd->mem_root) Item_int(POS(), 8, 1);
    return new (thd->mem_root) Item_func_conv(POS(), (*args)[0], i10, i8);
  }
};

class Weekday_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Item_func_weekday(POS(), (*args)[0], false);
  }
};

class Weekofyear_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    Item *i1 = new (thd->mem_root) Item_int(POS(), NAME_STRING("0"), 3, 1);
    return new (thd->mem_root) Item_func_week(POS(), (*args)[0], i1);
  }
};

class Datediff_instantiator {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    Item *i1 = new (thd->mem_root) Item_func_to_days(POS(), (*args)[0]);
    Item *i2 = new (thd->mem_root) Item_func_to_days(POS(), (*args)[1]);

    return new (thd->mem_root) Item_func_minus(POS(), i1, i2);
  }
};

class Subtime_instantiator {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root)
        Item_func_add_time(POS(), (*args)[0], (*args)[1], false, true);
  }
};

class Time_format_instantiator {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root)
        Item_func_date_format(POS(), (*args)[0], (*args)[1], true);
  }
};

class Dayofweek_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 1;

  Item *instantiate(THD *thd, PT_item_list *args) {
    return new (thd->mem_root) Item_func_weekday(POS(), (*args)[0], true);
  }
};

class From_unixtime_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Item_func_from_unixtime(POS(), (*args)[0]);
      case 2: {
        Item *ut =
            new (thd->mem_root) Item_func_from_unixtime(POS(), (*args)[0]);
        return new (thd->mem_root)
            Item_func_date_format(POS(), ut, (*args)[1], false);
      }
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Round_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1: {
        Item *i0 = new (thd->mem_root) Item_int_0(POS());
        return new (thd->mem_root)
            Item_func_round(POS(), (*args)[0], i0, false);
      }
      case 2:
        return new (thd->mem_root)
            Item_func_round(POS(), (*args)[0], (*args)[1], false);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Locate_instantiator {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = 3;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 2:
        /* Yes, parameters in that order : 2, 1 */
        return new (thd->mem_root)
            Item_func_locate(POS(), (*args)[1], (*args)[0]);
      case 3:
        /* Yes, parameters in that order : 2, 1, 3 */
        return new (thd->mem_root)
            Item_func_locate(POS(), (*args)[1], (*args)[0], (*args)[2]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Srid_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root)
            Item_func_st_srid_observer(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Item_func_st_srid_mutator(POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Latitude_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root)
            Item_func_st_latitude_observer(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Item_func_st_latitude_mutator(POS(), (*args)[0], (*args)[1]);
      default:
        /* purecov: begin deadcode */
        assert(false);
        return nullptr;
        /* purecov: end */
    }
  }
};

class Longitude_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root)
            Item_func_st_longitude_observer(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Item_func_st_longitude_mutator(POS(), (*args)[0], (*args)[1]);
      default:
        /* purecov: begin deadcode */
        assert(false);
        return nullptr;
        /* purecov: end */
    }
  }
};

class X_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Item_func_st_x_observer(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Item_func_st_x_mutator(POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Y_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1:
        return new (thd->mem_root) Item_func_st_y_observer(POS(), (*args)[0]);
      case 2:
        return new (thd->mem_root)
            Item_func_st_y_mutator(POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Yearweek_instantiator {
 public:
  static const uint Min_argcount = 1;
  static const uint Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    switch (args->elements()) {
      case 1: {
        Item *i0 = new (thd->mem_root) Item_int_0(POS());
        return new (thd->mem_root) Item_func_yearweek(POS(), (*args)[0], i0);
      }
      case 2:
        return new (thd->mem_root)
            Item_func_yearweek(POS(), (*args)[0], (*args)[1]);
      default:
        assert(false);
        return nullptr;
    }
  }
};

class Make_set_instantiator {
 public:
  static const uint Min_argcount = 2;
  static const uint Max_argcount = MAX_ARGLIST_SIZE;

  Item *instantiate(THD *thd, PT_item_list *args) {
    Item *param_1 = args->pop_front();
    return new (thd->mem_root) Item_func_make_set(POS(), param_1, args);
  }
};

/// Instantiates a call to JSON_LENGTH, which may take either one or
/// two arguments. The two-argument variant is rewritten from
/// JSON_LENGTH(doc, path) to JSON_LENGTH(JSON_EXTRACT(doc, path)).
class Json_length_instantiator {
 public:
  static constexpr int Min_argcount = 1;
  static constexpr int Max_argcount = 2;

  Item *instantiate(THD *thd, PT_item_list *args) {
    if (args->elements() == 1) {
      return new (thd->mem_root) Item_func_json_length(POS(), (*args)[0]);
    } else {
      assert(args->elements() == 2);
      auto arg = new (thd->mem_root)
          Item_func_json_extract(thd, POS(), (*args)[0], (*args)[1]);
      if (arg == nullptr) return nullptr;
      return new (thd->mem_root) Item_func_json_length(POS(), arg);
    }
  }
};

/// @} (end of group Instantiators)

uint arglist_length(const PT_item_list *args) {
  if (args == nullptr) return 0;
  return args->elements();
}

bool check_argcount_bounds(THD *, LEX_STRING function_name,
                           PT_item_list *item_list, uint min_argcount,
                           uint max_argcount) {
  uint argcount = arglist_length(item_list);
  if (argcount < min_argcount || argcount > max_argcount) {
    my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
    return true;
  }
  return false;
}

/**
  Factory for creating function objects. Performs validation check that the
  number of arguments is correct, then calls upon the instantiator function to
  instantiate the function object.

  @tparam Instantiator_fn A class that is expected to contain the following:

  - Min_argcount: The minimal number of arguments required to call the
  function. If the parameter count is less, an SQL error is raised and nullptr
  is returned.

  - Max_argcount: The maximum number of arguments required to call the
  function. If the parameter count is greater, an SQL error is raised and
  nullptr is returned.

  - Item *instantiate(THD *, PT_item_list *): Should construct an Item.
*/
template <typename Instantiator_fn>
class Function_factory : public Create_func {
 public:
  static Function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name,
                    PT_item_list *item_list) override {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    return m_instantiator.instantiate(thd, item_list);
  }

 private:
  Function_factory() = default;
  Instantiator_fn m_instantiator;
};

template <typename Instantiator_fn>
Function_factory<Instantiator_fn>
    Function_factory<Instantiator_fn>::s_singleton;

template <typename Instantiator_fn>
class Odd_argcount_function_factory : public Create_func {
 public:
  static Odd_argcount_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name,
                    PT_item_list *item_list) override {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    if (arglist_length(item_list) % 2 == 0) {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }
    return m_instantiator.instantiate(thd, item_list);
  }

 private:
  Odd_argcount_function_factory() = default;
  Instantiator_fn m_instantiator;
};

template <typename Instantiator_fn>
Odd_argcount_function_factory<Instantiator_fn>
    Odd_argcount_function_factory<Instantiator_fn>::s_singleton;

template <typename Instantiator_fn>
class Even_argcount_function_factory : public Create_func {
 public:
  static Even_argcount_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name,
                    PT_item_list *item_list) override {
    if (check_argcount_bounds(thd, function_name, item_list,
                              m_instantiator.Min_argcount,
                              m_instantiator.Max_argcount))
      return nullptr;
    if (arglist_length(item_list) % 2 != 0) {
      my_error(ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT, MYF(0), function_name.str);
      return nullptr;
    }
    return m_instantiator.instantiate(thd, item_list);
  }

 private:
  Even_argcount_function_factory() = default;
  Instantiator_fn m_instantiator;
};

template <typename Instantiator_fn>
Even_argcount_function_factory<Instantiator_fn>
    Even_argcount_function_factory<Instantiator_fn>::s_singleton;

/**
  Factory for internal functions that should be invoked from the system views
  only.

  @tparam Instantiator_fn See Function_factory.
*/
template <typename Instantiator_fn>
class Internal_function_factory : public Create_func {
 public:
  static Internal_function_factory<Instantiator_fn> s_singleton;

  Item *create_func(THD *thd, LEX_STRING function_name,
                    PT_item_list *item_list) override {
    if (!thd->parsing_system_view && !thd->is_dd_system_thread() &&
        DBUG_EVALUATE_IF("skip_dd_table_access_check", false, true)) {
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
  Internal_function_factory() = default;
  Instantiator_fn m_instantiator;
};

template <typename Instantiator_fn>
Internal_function_factory<Instantiator_fn>
    Internal_function_factory<Instantiator_fn>::s_singleton;

}  // namespace

/**
  Function builder for stored functions.
*/
class Create_sp_func : public Create_qfunc {
 public:
  Item *create(THD *thd, LEX_STRING db, LEX_STRING name, bool use_explicit_name,
               PT_item_list *item_list) override;

  static Create_sp_func s_singleton;

 protected:
  /** Constructor. */
  Create_sp_func() = default;
  /** Destructor. */
  ~Create_sp_func() override = default;
};

<<<<<<< HEAD
Item *Create_qfunc::create_func(THD *thd, LEX_STRING name,
                                PT_item_list *item_list) {
  return create(thd, NULL_STR, name, false, item_list);
=======



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
    Item *func= NULL, *p1= NULL, *p2= NULL, *p3= NULL, *p4= NULL, *p5= NULL, *p6= NULL;
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
    case 4:
      {
        p1= item_list->pop_front();
        p2= item_list->pop_front();
        p3= item_list->pop_front();
        p4= item_list->pop_front();
        func= create_aes(thd, p1, p2, p3, p4);
        break;
      }
    case 5:
      {
        p1= item_list->pop_front();
        p2= item_list->pop_front();
        p3= item_list->pop_front();
        p4= item_list->pop_front();
        p5= item_list->pop_front();
        func= create_aes(thd, p1, p2, p3, p4, p5);
        break;
      }
    case 6:
      {
        p1= item_list->pop_front();
        p2= item_list->pop_front();
        p3= item_list->pop_front();
        p4= item_list->pop_front();
        p5= item_list->pop_front();
        p6= item_list->pop_front();
        func= create_aes(thd, p1, p2, p3, p4, p5, p6);
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
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4)= 0;
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5)= 0;
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5, Item *arg6)= 0;
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
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4)
  {
    return new (thd->mem_root) Item_func_aes_encrypt(POS(), arg1, arg2, arg3, arg4);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5)
  {
    return new (thd->mem_root) Item_func_aes_encrypt(POS(), arg1, arg2, arg3, arg4, arg5);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5, Item *arg6)
  {
    return new (thd->mem_root) Item_func_aes_encrypt(POS(), arg1, arg2, arg3, arg4, arg5, arg6 );
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
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4)
  {
    return new (thd->mem_root) Item_func_aes_decrypt(POS(), arg1, arg2, arg3, arg4);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5)
  {
    return new (thd->mem_root) Item_func_aes_decrypt(POS(), arg1, arg2, arg3, arg4, arg5);
  }
  virtual Item *create_aes(THD *thd, Item *arg1, Item *arg2, Item *arg3, Item *arg4, Item *arg5, Item *arg6)
  {
    return new (thd->mem_root) Item_func_aes_decrypt(POS(), arg1, arg2, arg3, arg4, arg5, arg6 );
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


#ifndef NDEBUG
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
  LEX_STRING db= NULL_STR;
  if (thd->lex->copy_db_to(&db.str, &db.length))
    return NULL;

  return create(thd, db, name, false, item_list);
>>>>>>> upstream/cluster-7.6
}

Create_udf_func Create_udf_func::s_singleton;

<<<<<<< HEAD
Item *Create_udf_func::create_func(THD *thd, LEX_STRING name,
                                   PT_item_list *item_list) {
  udf_func *udf = find_udf(name.str, name.length);
<<<<<<< HEAD
  assert(udf);
=======
  DBUG_ASSERT(udf);
=======
Item*
Create_udf_func::create_func(THD *thd, LEX_STRING name, PT_item_list *item_list)
{
  udf_func *udf= find_udf(name.str, name.length);
  assert(udf);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  return create(thd, udf, item_list);
}

Item *Create_udf_func::create(THD *thd, udf_func *udf,
                              PT_item_list *item_list) {
  DBUG_TRACE;

<<<<<<< HEAD
  assert((udf->type == UDFTYPE_FUNCTION) || (udf->type == UDFTYPE_AGGREGATE));
=======
<<<<<<< HEAD
  DBUG_ASSERT((udf->type == UDFTYPE_FUNCTION) ||
              (udf->type == UDFTYPE_AGGREGATE));
=======
  assert(   (udf->type == UDFTYPE_FUNCTION)
            || (udf->type == UDFTYPE_AGGREGATE));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  Item *func = nullptr;
  POS pos{};

  switch (udf->returns) {
    case STRING_RESULT:
      if (udf->type == UDFTYPE_FUNCTION)
        func = new (thd->mem_root) Item_func_udf_str(pos, udf, item_list);
      else
        func = new (thd->mem_root) Item_sum_udf_str(pos, udf, item_list);
      break;
    case REAL_RESULT:
      if (udf->type == UDFTYPE_FUNCTION)
        func = new (thd->mem_root) Item_func_udf_float(pos, udf, item_list);
      else
        func = new (thd->mem_root) Item_sum_udf_float(pos, udf, item_list);
      break;
    case INT_RESULT:
      if (udf->type == UDFTYPE_FUNCTION)
        func = new (thd->mem_root) Item_func_udf_int(pos, udf, item_list);
      else
        func = new (thd->mem_root) Item_sum_udf_int(pos, udf, item_list);
      break;
    case DECIMAL_RESULT:
      if (udf->type == UDFTYPE_FUNCTION)
        func = new (thd->mem_root) Item_func_udf_decimal(pos, udf, item_list);
      else
        func = new (thd->mem_root) Item_sum_udf_decimal(pos, udf, item_list);
      break;
    default:
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "UDF return type");
  }
  return func;
}

Create_sp_func Create_sp_func::s_singleton;

Item *Create_sp_func::create(THD *thd, LEX_STRING db, LEX_STRING name,
                             bool use_explicit_name, PT_item_list *item_list) {
  return new (thd->mem_root)
<<<<<<< HEAD
      Item_func_sp(POS(), db, name, use_explicit_name, item_list);
}

/**
  Shorthand macro to reference the singleton instance. This also instantiates
  the Function_factory and Instantiator templates.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN(F, N) &Function_factory<Instantiator<F, N>>::s_singleton

/**
  Shorthand macro to reference the singleton instance when there is a
  specialized instantiator.

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
#define SQL_FN_ODD(F, MIN, MAX)   \
  &Odd_argcount_function_factory< \
      List_instantiator_with_thd<F, MIN, MAX>>::s_singleton

/**
  Just like SQL_FN_V_THD, but enforces a check that the argument count is even.
*/
#define SQL_FN_EVEN(F, MIN, MAX)   \
  &Even_argcount_function_factory< \
      List_instantiator_with_thd<F, MIN, MAX>>::s_singleton

/**
  Like SQL_FN, but for functions that may only be referenced from system views.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN_INTERNAL(F, N) \
  &Internal_function_factory<Instantiator<F, N>>::s_singleton

/**
  Just like SQL_FN_INTERNAL, but enforces a check that the argument count
  is even.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_INTERNAL_V(F, MIN, MAX) \
  &Internal_function_factory<Instantiator<F, MIN, MAX>>::s_singleton

/**
  Like SQL_FN_LIST, but for functions that may only be referenced from system
  views.

  @param F The Item_func that the factory should make.
  @param N Number of arguments that the function accepts.
*/
#define SQL_FN_LIST_INTERNAL(F, N) \
  &Internal_function_factory<List_instantiator<F, N>>::s_singleton

/**
  Like SQL_FN_LIST, but enforces a check that the argument count
  is within the range specified.

  @param F The Item_func that the factory should make.
  @param MIN Number of arguments that the function accepts.
  @param MAX Number of arguments that the function accepts.
*/
#define SQL_FN_LIST_INTERNAL_V(F, MIN, MAX) \
  &Internal_function_factory<List_instantiator<F, MIN, MAX>>::s_singleton

/**
=======
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


#ifndef NDEBUG
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
>>>>>>> upstream/cluster-7.6
  MySQL native functions.
  MAINTAINER:
  - Keep sorted for human lookup. At runtime, a hash table is used.
  - do **NOT** conditionally (\#ifdef, \#ifndef) define a function *NAME*:
    doing so will cause user code that works against a `--without-XYZ` binary
    to fail with name collisions against a `--with-XYZ` binary.
  - keep 1 line per entry, it makes `grep | sort` easier
  - Use uppercase (tokens are converted to uppercase before lookup.)

<<<<<<< HEAD
  This can't be constexpr because
  - Sun Studio does not allow the Create_func pointer to be constexpr.
*/
static const std::pair<const char *, Create_func *> func_array[] = {
    {"ABS", SQL_FN(Item_func_abs, 1)},
    {"ACOS", SQL_FN(Item_func_acos, 1)},
    {"ADDTIME", SQL_FN(Item_func_add_time, 2)},
    {"AES_DECRYPT", SQL_FN_V(Item_func_aes_decrypt, 2, 6)},
    {"AES_ENCRYPT", SQL_FN_V(Item_func_aes_encrypt, 2, 6)},
    {"ANY_VALUE", SQL_FN(Item_func_any_value, 1)},
    {"ASIN", SQL_FN(Item_func_asin, 1)},
    {"ATAN", SQL_FN_V(Item_func_atan, 1, 2)},
    {"ATAN2", SQL_FN_V(Item_func_atan, 1, 2)},
    {"BENCHMARK", SQL_FN(Item_func_benchmark, 2)},
    {"BIN", SQL_FACTORY(Bin_instantiator)},
    {"BIN_TO_UUID", SQL_FN_V(Item_func_bin_to_uuid, 1, 2)},
    {"BIT_COUNT", SQL_FN(Item_func_bit_count, 1)},
    {"BIT_LENGTH", SQL_FN(Item_func_bit_length, 1)},
    {"CEIL", SQL_FN(Item_func_ceiling, 1)},
    {"CEILING", SQL_FN(Item_func_ceiling, 1)},
    {"CHARACTER_LENGTH", SQL_FN(Item_func_char_length, 1)},
    {"CHAR_LENGTH", SQL_FN(Item_func_char_length, 1)},
    {"COERCIBILITY", SQL_FN(Item_func_coercibility, 1)},
    {"COMPRESS", SQL_FN(Item_func_compress, 1)},
    {"CONCAT", SQL_FN_V(Item_func_concat, 1, MAX_ARGLIST_SIZE)},
    {"CONCAT_WS", SQL_FN_V(Item_func_concat_ws, 2, MAX_ARGLIST_SIZE)},
    {"CONNECTION_ID", SQL_FN(Item_func_connection_id, 0)},
    {"CONV", SQL_FN(Item_func_conv, 3)},
    {"CONVERT_TZ", SQL_FN(Item_func_convert_tz, 3)},
    {"COS", SQL_FN(Item_func_cos, 1)},
    {"COT", SQL_FN(Item_func_cot, 1)},
    {"CRC32", SQL_FN(Item_func_crc32, 1)},
    {"CURRENT_ROLE", SQL_FN(Item_func_current_role, 0)},
    {"DATEDIFF", SQL_FACTORY(Datediff_instantiator)},
    {"DATE_FORMAT", SQL_FN(Item_func_date_format, 2)},
    {"DAYNAME", SQL_FN(Item_func_dayname, 1)},
    {"DAYOFMONTH", SQL_FN(Item_func_dayofmonth, 1)},
    {"DAYOFWEEK", SQL_FACTORY(Dayofweek_instantiator)},
    {"DAYOFYEAR", SQL_FN(Item_func_dayofyear, 1)},
    {"DEGREES", SQL_FN(Item_func_degrees, 1)},
    {"ELT", SQL_FN_V(Item_func_elt, 2, MAX_ARGLIST_SIZE)},
    {"EXP", SQL_FN(Item_func_exp, 1)},
    {"EXPORT_SET", SQL_FN_V(Item_func_export_set, 3, 5)},
    {"EXTRACTVALUE", SQL_FN(Item_func_xml_extractvalue, 2)},
    {"FIELD", SQL_FN_V(Item_func_field, 2, MAX_ARGLIST_SIZE)},
    {"FIND_IN_SET", SQL_FN(Item_func_find_in_set, 2)},
    {"FLOOR", SQL_FN(Item_func_floor, 1)},
    {"FORMAT_BYTES", SQL_FN(Item_func_pfs_format_bytes, 1)},
    {"FORMAT_PICO_TIME", SQL_FN(Item_func_pfs_format_pico_time, 1)},
    {"FOUND_ROWS", SQL_FN(Item_func_found_rows, 0)},
    {"FROM_BASE64", SQL_FN(Item_func_from_base64, 1)},
    {"FROM_DAYS", SQL_FN(Item_func_from_days, 1)},
    {"FROM_UNIXTIME", SQL_FACTORY(From_unixtime_instantiator)},
    {"GET_LOCK", SQL_FN(Item_func_get_lock, 2)},
    {"GREATEST", SQL_FN_V(Item_func_max, 2, MAX_ARGLIST_SIZE)},
    {"GTID_SUBTRACT", SQL_FN(Item_func_gtid_subtract, 2)},
    {"GTID_SUBSET", SQL_FN(Item_func_gtid_subset, 2)},
    {"HEX", SQL_FN(Item_func_hex, 1)},
    {"IFNULL", SQL_FN(Item_func_ifnull, 2)},
    {"INET_ATON", SQL_FN(Item_func_inet_aton, 1)},
    {"INET_NTOA", SQL_FN(Item_func_inet_ntoa, 1)},
    {"INET6_ATON", SQL_FN(Item_func_inet6_aton, 1)},
    {"INET6_NTOA", SQL_FN(Item_func_inet6_ntoa, 1)},
    {"IS_IPV4", SQL_FN(Item_func_is_ipv4, 1)},
    {"IS_IPV6", SQL_FN(Item_func_is_ipv6, 1)},
    {"IS_IPV4_COMPAT", SQL_FN(Item_func_is_ipv4_compat, 1)},
    {"IS_IPV4_MAPPED", SQL_FN(Item_func_is_ipv4_mapped, 1)},
    {"IS_UUID", SQL_FN(Item_func_is_uuid, 1)},
    {"INSTR", SQL_FN(Item_func_instr, 2)},
    {"ISNULL", SQL_FN(Item_func_isnull, 1)},
    {"JSON_VALID", SQL_FN(Item_func_json_valid, 1)},
    {"JSON_CONTAINS", SQL_FN_V_LIST_THD(Item_func_json_contains, 2, 3)},
    {"JSON_CONTAINS_PATH",
     SQL_FN_V_THD(Item_func_json_contains_path, 3, MAX_ARGLIST_SIZE)},
    {"JSON_LENGTH", SQL_FACTORY(Json_length_instantiator)},
    {"JSON_DEPTH", SQL_FN(Item_func_json_depth, 1)},
    {"JSON_PRETTY", SQL_FN(Item_func_json_pretty, 1)},
    {"JSON_TYPE", SQL_FN(Item_func_json_type, 1)},
    {"JSON_KEYS", SQL_FN_V_THD(Item_func_json_keys, 1, 2)},
    {"JSON_EXTRACT", SQL_FN_V_THD(Item_func_json_extract, 2, MAX_ARGLIST_SIZE)},
    {"JSON_ARRAY_APPEND",
     SQL_FN_ODD(Item_func_json_array_append, 3, MAX_ARGLIST_SIZE)},
    {"JSON_INSERT", SQL_FN_ODD(Item_func_json_insert, 3, MAX_ARGLIST_SIZE)},
    {"JSON_ARRAY_INSERT",
     SQL_FN_ODD(Item_func_json_array_insert, 3, MAX_ARGLIST_SIZE)},
    {"JSON_OBJECT",
     SQL_FN_EVEN(Item_func_json_row_object, 0, MAX_ARGLIST_SIZE)},
    {"JSON_OVERLAPS", SQL_FN(Item_func_json_overlaps, 2)},
    {"JSON_SEARCH", SQL_FN_V_THD(Item_func_json_search, 3, MAX_ARGLIST_SIZE)},
    {"JSON_SET", SQL_FN_ODD(Item_func_json_set, 3, MAX_ARGLIST_SIZE)},
    {"JSON_REPLACE", SQL_FN_ODD(Item_func_json_replace, 3, MAX_ARGLIST_SIZE)},
    {"JSON_ARRAY",
     SQL_FN_V_LIST_THD(Item_func_json_array, 0, MAX_ARGLIST_SIZE)},
    {"JSON_REMOVE",
     SQL_FN_V_LIST_THD(Item_func_json_remove, 2, MAX_ARGLIST_SIZE)},
    {"JSON_MERGE",
     SQL_FN_V_LIST_THD(Item_func_json_merge, 2, MAX_ARGLIST_SIZE)},
    {"JSON_MERGE_PATCH",
     SQL_FN_V_LIST_THD(Item_func_json_merge_patch, 2, MAX_ARGLIST_SIZE)},
    {"JSON_MERGE_PRESERVE",
     SQL_FN_V_LIST_THD(Item_func_json_merge_preserve, 2, MAX_ARGLIST_SIZE)},
    {"JSON_QUOTE", SQL_FN_LIST(Item_func_json_quote, 1)},
    {"JSON_SCHEMA_VALID", SQL_FN(Item_func_json_schema_valid, 2)},
    {"JSON_SCHEMA_VALIDATION_REPORT",
     SQL_FN_V_THD(Item_func_json_schema_validation_report, 2, 2)},
    {"JSON_STORAGE_FREE", SQL_FN(Item_func_json_storage_free, 1)},
    {"JSON_STORAGE_SIZE", SQL_FN(Item_func_json_storage_size, 1)},
    {"JSON_UNQUOTE", SQL_FN_LIST(Item_func_json_unquote, 1)},
    {"IS_FREE_LOCK", SQL_FN(Item_func_is_free_lock, 1)},
    {"IS_USED_LOCK", SQL_FN(Item_func_is_used_lock, 1)},
    {"LAST_DAY", SQL_FN(Item_func_last_day, 1)},
    {"LAST_INSERT_ID", SQL_FN_V(Item_func_last_insert_id, 0, 1)},
    {"LCASE", SQL_FN(Item_func_lower, 1)},
    {"LEAST", SQL_FN_V_LIST(Item_func_min, 2, MAX_ARGLIST_SIZE)},
    {"LENGTH", SQL_FN(Item_func_length, 1)},
#ifndef NDEBUG
    {"LIKE_RANGE_MIN", SQL_FN(Item_func_like_range_min, 2)},
    {"LIKE_RANGE_MAX", SQL_FN(Item_func_like_range_max, 2)},
=======
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
#ifndef NDEBUG
  { { C_STRING_WITH_LEN("LIKE_RANGE_MIN") }, BUILDER(Create_func_like_range_min)},
  { { C_STRING_WITH_LEN("LIKE_RANGE_MAX") }, BUILDER(Create_func_like_range_max)},
>>>>>>> upstream/cluster-7.6
#endif
    {"LN", SQL_FN(Item_func_ln, 1)},
    {"LOAD_FILE", SQL_FN(Item_load_file, 1)},
    {"LOCATE", SQL_FACTORY(Locate_instantiator)},
    {"LOG", SQL_FN_V(Item_func_log, 1, 2)},
    {"LOG10", SQL_FN(Item_func_log10, 1)},
    {"LOG2", SQL_FN(Item_func_log2, 1)},
    {"LOWER", SQL_FN(Item_func_lower, 1)},
    {"LPAD", SQL_FN(Item_func_lpad, 3)},
    {"LTRIM", SQL_FN(Item_func_ltrim, 1)},
    {"MAKEDATE", SQL_FN(Item_func_makedate, 2)},
    {"MAKETIME", SQL_FN(Item_func_maketime, 3)},
    {"MAKE_SET", SQL_FACTORY(Make_set_instantiator)},
    {"MASTER_POS_WAIT", SQL_FN_V(Item_master_pos_wait, 2, 4)},
    {"MBRCONTAINS", SQL_FN(Item_func_mbrcontains, 2)},
    {"MBRCOVEREDBY", SQL_FN(Item_func_mbrcoveredby, 2)},
    {"MBRCOVERS", SQL_FN(Item_func_mbrcovers, 2)},
    {"MBRDISJOINT", SQL_FN(Item_func_mbrdisjoint, 2)},
    {"MBREQUALS", SQL_FN(Item_func_mbrequals, 2)},
    {"MBRINTERSECTS", SQL_FN(Item_func_mbrintersects, 2)},
    {"MBROVERLAPS", SQL_FN(Item_func_mbroverlaps, 2)},
    {"MBRTOUCHES", SQL_FN(Item_func_mbrtouches, 2)},
    {"MBRWITHIN", SQL_FN(Item_func_mbrwithin, 2)},
    {"MD5", SQL_FN(Item_func_md5, 1)},
    {"MONTHNAME", SQL_FN(Item_func_monthname, 1)},
    {"NAME_CONST", SQL_FN(Item_name_const, 2)},
    {"NULLIF", SQL_FN(Item_func_nullif, 2)},
    {"OCT", SQL_FACTORY(Oct_instantiator)},
    {"OCTET_LENGTH", SQL_FN(Item_func_length, 1)},
    {"ORD", SQL_FN(Item_func_ord, 1)},
    {"PERIOD_ADD", SQL_FN(Item_func_period_add, 2)},
    {"PERIOD_DIFF", SQL_FN(Item_func_period_diff, 2)},
    {"PI", SQL_FN(Item_func_pi, 0)},
    {"POW", SQL_FN(Item_func_pow, 2)},
    {"POWER", SQL_FN(Item_func_pow, 2)},
    {"PS_CURRENT_THREAD_ID", SQL_FN(Item_func_pfs_current_thread_id, 0)},
    {"PS_THREAD_ID", SQL_FN(Item_func_pfs_thread_id, 1)},
    {"QUOTE", SQL_FN(Item_func_quote, 1)},
    {"RADIANS", SQL_FN(Item_func_radians, 1)},
    {"RAND", SQL_FN_V(Item_func_rand, 0, 1)},
    {"RANDOM_BYTES", SQL_FN(Item_func_random_bytes, 1)},
    {"REGEXP_INSTR", SQL_FN_V_LIST(Item_func_regexp_instr, 2, 6)},
    {"REGEXP_LIKE", SQL_FN_V_LIST(Item_func_regexp_like, 2, 3)},
    {"REGEXP_REPLACE", SQL_FN_V_LIST(Item_func_regexp_replace, 3, 6)},
    {"REGEXP_SUBSTR", SQL_FN_V_LIST(Item_func_regexp_substr, 2, 5)},
    {"RELEASE_ALL_LOCKS", SQL_FN(Item_func_release_all_locks, 0)},
    {"RELEASE_LOCK", SQL_FN(Item_func_release_lock, 1)},
    {"REVERSE", SQL_FN(Item_func_reverse, 1)},
    {"ROLES_GRAPHML", SQL_FN(Item_func_roles_graphml, 0)},
    {"ROUND", SQL_FACTORY(Round_instantiator)},
    {"RPAD", SQL_FN(Item_func_rpad, 3)},
    {"RTRIM", SQL_FN(Item_func_rtrim, 1)},
    {"SEC_TO_TIME", SQL_FN(Item_func_sec_to_time, 1)},
    {"SHA", SQL_FN(Item_func_sha, 1)},
    {"SHA1", SQL_FN(Item_func_sha, 1)},
    {"SHA2", SQL_FN(Item_func_sha2, 2)},
    {"SIGN", SQL_FN(Item_func_sign, 1)},
    {"SIN", SQL_FN(Item_func_sin, 1)},
    {"SLEEP", SQL_FN(Item_func_sleep, 1)},
    {"SOUNDEX", SQL_FN(Item_func_soundex, 1)},
    {"SOURCE_POS_WAIT", SQL_FN_V(Item_source_pos_wait, 2, 4)},
    {"SPACE", SQL_FN(Item_func_space, 1)},
    {"STATEMENT_DIGEST", SQL_FN(Item_func_statement_digest, 1)},
    {"STATEMENT_DIGEST_TEXT", SQL_FN(Item_func_statement_digest_text, 1)},
    {"WAIT_FOR_EXECUTED_GTID_SET",
     SQL_FN_V(Item_wait_for_executed_gtid_set, 1, 2)},
    {"WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS",
     SQL_FN_V(Item_master_gtid_set_wait, 1, 3)},
    {"SQRT", SQL_FN(Item_func_sqrt, 1)},
    {"STRCMP", SQL_FN(Item_func_strcmp, 2)},
    {"STR_TO_DATE", SQL_FN(Item_func_str_to_date, 2)},
    {"ST_AREA", SQL_FN(Item_func_st_area, 1)},
    {"ST_ASBINARY", SQL_FN_V(Item_func_as_wkb, 1, 2)},
    {"ST_ASGEOJSON", SQL_FN_V_THD(Item_func_as_geojson, 1, 3)},
    {"ST_ASTEXT", SQL_FN_V(Item_func_as_wkt, 1, 2)},
    {"ST_ASWKB", SQL_FN_V(Item_func_as_wkb, 1, 2)},
    {"ST_ASWKT", SQL_FN_V(Item_func_as_wkt, 1, 2)},
    {"ST_BUFFER", SQL_FN_V_LIST(Item_func_st_buffer, 2, 5)},
    {"ST_BUFFER_STRATEGY", SQL_FN_V_LIST(Item_func_buffer_strategy, 1, 2)},
    {"ST_CENTROID", SQL_FN(Item_func_centroid, 1)},
    {"ST_CONTAINS", SQL_FN(Item_func_st_contains, 2)},
    {"ST_CONVEXHULL", SQL_FN(Item_func_convex_hull, 1)},
    {"ST_CROSSES", SQL_FN(Item_func_st_crosses, 2)},
    {"ST_DIFFERENCE", SQL_FN(Item_func_st_difference, 2)},
    {"ST_DIMENSION", SQL_FN(Item_func_dimension, 1)},
    {"ST_DISJOINT", SQL_FN(Item_func_st_disjoint, 2)},
    {"ST_DISTANCE", SQL_FN_V_LIST(Item_func_distance, 2, 3)},
    {"ST_DISTANCE_SPHERE", SQL_FN_V_LIST(Item_func_st_distance_sphere, 2, 3)},
    {"ST_ENDPOINT", SQL_FACTORY(Endpoint_instantiator)},
    {"ST_ENVELOPE", SQL_FN(Item_func_envelope, 1)},
    {"ST_EQUALS", SQL_FN(Item_func_st_equals, 2)},
    {"ST_EXTERIORRING", SQL_FACTORY(Exteriorring_instantiator)},
    {"ST_FRECHETDISTANCE", SQL_FN_V_LIST(Item_func_st_frechet_distance, 2, 3)},
    {"ST_GEOHASH", SQL_FN_V(Item_func_geohash, 2, 3)},
    {"ST_GEOMCOLLFROMTEXT", SQL_FACTORY(Geomcollfromtext_instantiator)},
    {"ST_GEOMCOLLFROMTXT", SQL_FACTORY(Geomcollfromtxt_instantiator)},
    {"ST_GEOMCOLLFROMWKB", SQL_FACTORY(Geomcollfromwkb_instantiator)},
    {"ST_GEOMETRYCOLLECTIONFROMTEXT",
     SQL_FACTORY(Geometrycollectionfromtext_instantiator)},
    {"ST_GEOMETRYCOLLECTIONFROMWKB",
     SQL_FACTORY(Geometrycollectionfromwkb_instantiator)},
    {"ST_GEOMETRYFROMTEXT", SQL_FACTORY(Geometryfromtext_instantiator)},
    {"ST_GEOMETRYFROMWKB", SQL_FACTORY(Geometryfromwkb_instantiator)},
    {"ST_GEOMETRYN", SQL_FACTORY(Sp_geometryn_instantiator)},
    {"ST_GEOMETRYTYPE", SQL_FN(Item_func_geometry_type, 1)},
    {"ST_GEOMFROMGEOJSON", SQL_FN_V(Item_func_geomfromgeojson, 1, 3)},
    {"ST_GEOMFROMTEXT", SQL_FACTORY(Geomfromtext_instantiator)},
    {"ST_GEOMFROMWKB", SQL_FACTORY(Geomfromwkb_instantiator)},
    {"ST_HAUSDORFFDISTANCE",
     SQL_FN_V_LIST(Item_func_st_hausdorff_distance, 2, 3)},
    {"ST_INTERIORRINGN", SQL_FACTORY(Sp_interiorringn_instantiator)},
    {"ST_INTERSECTS", SQL_FN(Item_func_st_intersects, 2)},
    {"ST_INTERSECTION", SQL_FN(Item_func_st_intersection, 2)},
    {"ST_ISCLOSED", SQL_FN(Item_func_isclosed, 1)},
    {"ST_ISEMPTY", SQL_FN(Item_func_isempty, 1)},
    {"ST_ISSIMPLE", SQL_FN(Item_func_st_issimple, 1)},
    {"ST_ISVALID", SQL_FN(Item_func_isvalid, 1)},
    {"ST_LATFROMGEOHASH", SQL_FN(Item_func_latfromgeohash, 1)},
    {"ST_LATITUDE", SQL_FACTORY(Latitude_instantiator)},
    {"ST_LENGTH", SQL_FN_V_LIST(Item_func_st_length, 1, 2)},
    {"ST_LINEFROMTEXT", SQL_FACTORY(Linefromtext_instantiator)},
    {"ST_LINEFROMWKB", SQL_FACTORY(Linefromwkb_instantiator)},
    {"ST_LINEINTERPOLATEPOINT", SQL_FN(Item_func_lineinterpolatepoint, 2)},
    {"ST_LINEINTERPOLATEPOINTS", SQL_FN(Item_func_lineinterpolatepoints, 2)},
    {"ST_LINESTRINGFROMTEXT", SQL_FACTORY(Linestringfromtext_instantiator)},
    {"ST_LINESTRINGFROMWKB", SQL_FACTORY(Linestringfromwkb_instantiator)},
    {"ST_LONGFROMGEOHASH", SQL_FN(Item_func_longfromgeohash, 1)},
    {"ST_LONGITUDE", SQL_FACTORY(Longitude_instantiator)},
    {"ST_MAKEENVELOPE", SQL_FN(Item_func_make_envelope, 2)},
    {"ST_MLINEFROMTEXT", SQL_FACTORY(Mlinefromtext_instantiator)},
    {"ST_MLINEFROMWKB", SQL_FACTORY(Mlinefromwkb_instantiator)},
    {"ST_MPOINTFROMTEXT", SQL_FACTORY(Mpointfromtext_instantiator)},
    {"ST_MPOINTFROMWKB", SQL_FACTORY(Mpointfromwkb_instantiator)},
    {"ST_MPOLYFROMTEXT", SQL_FACTORY(Mpolyfromtext_instantiator)},
    {"ST_MPOLYFROMWKB", SQL_FACTORY(Mpolyfromwkb_instantiator)},
    {"ST_MULTILINESTRINGFROMTEXT",
     SQL_FACTORY(Multilinestringfromtext_instantiator)},
    {"ST_MULTILINESTRINGFROMWKB",
     SQL_FACTORY(Multilinestringfromwkb_instantiator)},
    {"ST_MULTIPOINTFROMTEXT", SQL_FACTORY(Multipointfromtext_instantiator)},
    {"ST_MULTIPOINTFROMWKB", SQL_FACTORY(Multipointfromwkb_instantiator)},
    {"ST_MULTIPOLYGONFROMTEXT", SQL_FACTORY(Multipolygonfromtext_instantiator)},
    {"ST_MULTIPOLYGONFROMWKB", SQL_FACTORY(Multipolygonfromwkb_instantiator)},
    {"ST_NUMGEOMETRIES", SQL_FN(Item_func_numgeometries, 1)},
    {"ST_NUMINTERIORRING", SQL_FN(Item_func_numinteriorring, 1)},
    {"ST_NUMINTERIORRINGS", SQL_FN(Item_func_numinteriorring, 1)},
    {"ST_NUMPOINTS", SQL_FN(Item_func_numpoints, 1)},
    {"ST_OVERLAPS", SQL_FN(Item_func_st_overlaps, 2)},
    {"ST_POINTATDISTANCE", SQL_FN(Item_func_st_pointatdistance, 2)},
    {"ST_POINTFROMGEOHASH", SQL_FN(Item_func_pointfromgeohash, 2)},
    {"ST_POINTFROMTEXT", SQL_FACTORY(Pointfromtext_instantiator)},
    {"ST_POINTFROMWKB", SQL_FACTORY(Pointfromwkb_instantiator)},
    {"ST_POINTN", SQL_FACTORY(Sp_pointn_instantiator)},
    {"ST_POLYFROMTEXT", SQL_FACTORY(Polyfromtext_instantiator)},
    {"ST_POLYFROMWKB", SQL_FACTORY(Polyfromwkb_instantiator)},
    {"ST_POLYGONFROMTEXT", SQL_FACTORY(Polygonfromtext_instantiator)},
    {"ST_POLYGONFROMWKB", SQL_FACTORY(Polygonfromwkb_instantiator)},
    {"ST_SIMPLIFY", SQL_FN(Item_func_st_simplify, 2)},
    {"ST_SRID", SQL_FACTORY(Srid_instantiator)},
    {"ST_STARTPOINT", SQL_FACTORY(Startpoint_instantiator)},
    {"ST_SYMDIFFERENCE", SQL_FN(Item_func_st_symdifference, 2)},
    {"ST_SWAPXY", SQL_FN(Item_func_swap_xy, 1)},
    {"ST_TOUCHES", SQL_FN(Item_func_st_touches, 2)},
    {"ST_TRANSFORM", SQL_FN(Item_func_st_transform, 2)},
    {"ST_UNION", SQL_FN(Item_func_st_union, 2)},
    {"ST_VALIDATE", SQL_FN(Item_func_validate, 1)},
    {"ST_WITHIN", SQL_FN(Item_func_st_within, 2)},
    {"ST_X", SQL_FACTORY(X_instantiator)},
    {"ST_Y", SQL_FACTORY(Y_instantiator)},
    {"SUBSTRING_INDEX", SQL_FN(Item_func_substr_index, 3)},
    {"SUBTIME", SQL_FACTORY(Subtime_instantiator)},
    {"TAN", SQL_FN(Item_func_tan, 1)},
    {"TIMEDIFF", SQL_FN(Item_func_timediff, 2)},
    {"TIME_FORMAT", SQL_FACTORY(Time_format_instantiator)},
    {"TIME_TO_SEC", SQL_FN(Item_func_time_to_sec, 1)},
    {"TO_BASE64", SQL_FN(Item_func_to_base64, 1)},
    {"TO_DAYS", SQL_FN(Item_func_to_days, 1)},
    {"TO_SECONDS", SQL_FN(Item_func_to_seconds, 1)},
    {"UCASE", SQL_FN(Item_func_upper, 1)},
    {"UNCOMPRESS", SQL_FN(Item_func_uncompress, 1)},
    {"UNCOMPRESSED_LENGTH", SQL_FN(Item_func_uncompressed_length, 1)},
    {"UNHEX", SQL_FN(Item_func_unhex, 1)},
    {"UNIX_TIMESTAMP", SQL_FN_V(Item_func_unix_timestamp, 0, 1)},
    {"UPDATEXML", SQL_FN(Item_func_xml_update, 3)},
    {"UPPER", SQL_FN(Item_func_upper, 1)},
    {"UUID", SQL_FN(Item_func_uuid, 0)},
    {"UUID_SHORT", SQL_FN(Item_func_uuid_short, 0)},
    {"UUID_TO_BIN", SQL_FN_V(Item_func_uuid_to_bin, 1, 2)},
    {"VALIDATE_PASSWORD_STRENGTH",
     SQL_FN(Item_func_validate_password_strength, 1)},
    {"VERSION", SQL_FN(Item_func_version, 0)},
    {"WEEKDAY", SQL_FACTORY(Weekday_instantiator)},
    {"WEEKOFYEAR", SQL_FACTORY(Weekofyear_instantiator)},
    {"YEARWEEK", SQL_FACTORY(Yearweek_instantiator)},
    {"GET_DD_COLUMN_PRIVILEGES",
     SQL_FN_INTERNAL(Item_func_get_dd_column_privileges, 3)},
    {"GET_DD_INDEX_SUB_PART_LENGTH",
     SQL_FN_LIST_INTERNAL(Item_func_get_dd_index_sub_part_length, 5)},
    {"GET_DD_CREATE_OPTIONS",
     SQL_FN_INTERNAL(Item_func_get_dd_create_options, 3)},
    {"GET_DD_SCHEMA_OPTIONS",
     SQL_FN_INTERNAL(Item_func_get_dd_schema_options, 1)},
    {"GET_DD_TABLESPACE_PRIVATE_DATA",
     SQL_FN_INTERNAL(Item_func_get_dd_tablespace_private_data, 2)},
    {"GET_DD_INDEX_PRIVATE_DATA",
     SQL_FN_INTERNAL(Item_func_get_dd_index_private_data, 2)},
    {"INTERNAL_DD_CHAR_LENGTH",
     SQL_FN_INTERNAL(Item_func_internal_dd_char_length, 4)},
    {"CAN_ACCESS_DATABASE", SQL_FN_INTERNAL(Item_func_can_access_database, 1)},
    {"CAN_ACCESS_TABLE", SQL_FN_INTERNAL(Item_func_can_access_table, 2)},
    {"CAN_ACCESS_COLUMN", SQL_FN_INTERNAL(Item_func_can_access_column, 3)},
    {"CAN_ACCESS_VIEW", SQL_FN_INTERNAL(Item_func_can_access_view, 4)},
    {"CAN_ACCESS_TRIGGER", SQL_FN_INTERNAL(Item_func_can_access_trigger, 2)},
    {"CAN_ACCESS_ROUTINE",
     SQL_FN_LIST_INTERNAL(Item_func_can_access_routine, 5)},
    {"CAN_ACCESS_EVENT", SQL_FN_INTERNAL(Item_func_can_access_event, 1)},
    {"CAN_ACCESS_USER", SQL_FN_INTERNAL(Item_func_can_access_user, 2)},
    {"ICU_VERSION", SQL_FN(Item_func_icu_version, 0)},
    {"CAN_ACCESS_RESOURCE_GROUP",
     SQL_FN_INTERNAL(Item_func_can_access_resource_group, 1)},
    {"CONVERT_CPU_ID_MASK", SQL_FN_INTERNAL(Item_func_convert_cpu_id_mask, 1)},
    {"IS_VISIBLE_DD_OBJECT",
     SQL_FN_INTERNAL_V(Item_func_is_visible_dd_object, 1, 3)},
    {"INTERNAL_TABLE_ROWS",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_table_rows, 8, 9)},
    {"INTERNAL_AVG_ROW_LENGTH",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_avg_row_length, 8, 9)},
    {"INTERNAL_DATA_LENGTH",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_data_length, 8, 9)},
    {"INTERNAL_MAX_DATA_LENGTH",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_max_data_length, 8, 9)},
    {"INTERNAL_INDEX_LENGTH",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_index_length, 8, 9)},
    {"INTERNAL_DATA_FREE",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_data_free, 8, 9)},
    {"INTERNAL_AUTO_INCREMENT",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_auto_increment, 9, 10)},
    {"INTERNAL_CHECKSUM",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_checksum, 8, 9)},
    {"INTERNAL_UPDATE_TIME",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_update_time, 8, 9)},
    {"INTERNAL_CHECK_TIME",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_check_time, 8, 9)},
    {"INTERNAL_KEYS_DISABLED",
     SQL_FN_INTERNAL(Item_func_internal_keys_disabled, 1)},
    {"INTERNAL_INDEX_COLUMN_CARDINALITY",
     SQL_FN_LIST_INTERNAL(Item_func_internal_index_column_cardinality, 11)},
    {"INTERNAL_GET_COMMENT_OR_ERROR",
     SQL_FN_LIST_INTERNAL(Item_func_internal_get_comment_or_error, 5)},
    {"INTERNAL_GET_VIEW_WARNING_OR_ERROR",
     SQL_FN_LIST_INTERNAL(Item_func_internal_get_view_warning_or_error, 4)},
    {"INTERNAL_GET_PARTITION_NODEGROUP",
     SQL_FN_INTERNAL(Item_func_get_partition_nodegroup, 1)},
    {"INTERNAL_TABLESPACE_ID",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_id, 4)},
    {"INTERNAL_TABLESPACE_TYPE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_type, 4)},
    {"INTERNAL_TABLESPACE_LOGFILE_GROUP_NAME",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_logfile_group_name, 4)},
    {"INTERNAL_TABLESPACE_LOGFILE_GROUP_NUMBER",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_logfile_group_number, 4)},
    {"INTERNAL_TABLESPACE_FREE_EXTENTS",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_free_extents, 4)},
    {"INTERNAL_TABLESPACE_TOTAL_EXTENTS",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_total_extents, 4)},
    {"INTERNAL_TABLESPACE_EXTENT_SIZE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_extent_size, 4)},
    {"INTERNAL_TABLESPACE_INITIAL_SIZE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_initial_size, 4)},
    {"INTERNAL_TABLESPACE_MAXIMUM_SIZE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_maximum_size, 4)},
    {"INTERNAL_TABLESPACE_AUTOEXTEND_SIZE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_autoextend_size, 4)},
    {"INTERNAL_TABLESPACE_VERSION",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_version, 4)},
    {"INTERNAL_TABLESPACE_ROW_FORMAT",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_row_format, 4)},
    {"INTERNAL_TABLESPACE_DATA_FREE",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_data_free, 4)},
    {"INTERNAL_TABLESPACE_STATUS",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_status, 4)},
    {"INTERNAL_TABLESPACE_EXTRA",
     SQL_FN_INTERNAL(Item_func_internal_tablespace_extra, 4)},
    {"GET_DD_PROPERTY_KEY_VALUE",
     SQL_FN_INTERNAL(Item_func_get_dd_property_key_value, 2)},
    {"REMOVE_DD_PROPERTY_KEY",
     SQL_FN_INTERNAL(Item_func_remove_dd_property_key, 2)},
    {"CONVERT_INTERVAL_TO_USER_INTERVAL",
     SQL_FN_INTERNAL(Item_func_convert_interval_to_user_interval, 2)},
    {"INTERNAL_GET_DD_COLUMN_EXTRA",
     SQL_FN_LIST_INTERNAL(Item_func_internal_get_dd_column_extra, 8)},
    {"INTERNAL_GET_USERNAME",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_get_username, 0, 1)},
    {"INTERNAL_GET_HOSTNAME",
     SQL_FN_LIST_INTERNAL_V(Item_func_internal_get_hostname, 0, 1)},
    {"INTERNAL_GET_ENABLED_ROLE_JSON",
     SQL_FN_INTERNAL(Item_func_internal_get_enabled_role_json, 0)},
    {"INTERNAL_GET_MANDATORY_ROLES_JSON",
     SQL_FN_INTERNAL(Item_func_internal_get_mandatory_roles_json, 0)},
    {"INTERNAL_IS_MANDATORY_ROLE",
     SQL_FN_INTERNAL(Item_func_internal_is_mandatory_role, 2)},
    {"INTERNAL_IS_ENABLED_ROLE",
     SQL_FN_INTERNAL(Item_func_internal_is_enabled_role, 2)}};

using Native_functions_hash = std::unordered_map<std::string, Create_func *>;
static const Native_functions_hash *native_functions_hash;

bool item_create_init() {
  try {
    native_functions_hash =
        new Native_functions_hash(std::begin(func_array), std::end(func_array));
  } catch (...) {
    handle_std_exception("item_create_init");
    return true;
  }
  return false;
}

<<<<<<< HEAD
void item_create_cleanup() { delete native_functions_hash; }

Create_func *find_native_function_builder(const LEX_STRING &lex_name) {
  try {
    std::string name(lex_name.str, lex_name.length);
    for (auto &it : name) it = std::toupper(it);

    auto entry = native_functions_hash->find(name);
    if (entry == native_functions_hash->end()) return nullptr;
    return entry->second;
  } catch (...) {
    handle_std_exception("find_native_function_builder");
    return nullptr;
=======
#ifndef NDEBUG
  for (uint i=0 ; i < native_functions_hash.records ; i++)
  {
    func= (Native_func_registry*) my_hash_element(& native_functions_hash, i);
    DBUG_PRINT("info", ("native function: %s  length: %u",
                        func->name.str, (uint) func->name.length));
>>>>>>> upstream/cluster-7.6
  }
}

Create_qfunc *find_qualified_function_builder(THD *) {
  return &Create_sp_func::s_singleton;
}

Item *create_func_cast(THD *thd, const POS &pos, Item *a,
                       Cast_target cast_target, const CHARSET_INFO *cs) {
  Cast_type type;
  type.target = cast_target;
  type.charset = cs;
  type.length = nullptr;
  type.dec = nullptr;
  return create_func_cast(thd, pos, a, type, false);
}

/**
  Validates a cast target type and extracts the specified length and precision
  of the target type. Helper function for creating Items representing CAST
  expressions, and Items performing CAST-like tasks, such as JSON_VALUE.

  @param thd        thread handler
  @param pos        the location of the expression
  @param arg        the value to cast
  @param cast_type  the target type of the cast
  @param as_array   true if the target type is an array type
  @param[out] length     gets set to the maximum length of the target type
  @param[out] precision  gets set to the precision of the target type
  @return true on error, false on success
*/
static bool validate_cast_type_and_extract_length(
    const THD *thd, const POS &pos, Item *arg, const Cast_type &cast_type,
    bool as_array, int64_t *length, uint *precision) {
  // earlier syntax error detected
  if (arg == nullptr) return true;

  if (as_array) {
    // Disallow arrays in stored routines.
    if (thd->lex->get_sp_current_parsing_ctx()) {
      my_error(ER_WRONG_USAGE, MYF(0), "CAST( .. AS .. ARRAY)",
               "stored routines");
      return true;
    }

    /*
      Multi-valued index currently only supports two character sets: binary for
      BINARY(x) keys and my_charset_utf8mb4_0900_bin for CHAR(x) keys. The
      latter one is because it's closest to binary in terms of sort order and
      doesn't pad spaces. This is important because JSON treats e.g. "abc" and
      "abc " as different values and a space padding charset will cause
      inconsistent key handling.
    */
    if (cast_type.charset != nullptr && cast_type.charset != &my_charset_bin) {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0),
               "specifying charset for multi-valued index");
      return true;
    }
  }

  *length = 0;
  *precision = 0;

  const char *const c_len = cast_type.length;
  const char *const c_dec = cast_type.dec;

  switch (cast_type.target) {
    case ITEM_CAST_SIGNED_INT:
    case ITEM_CAST_UNSIGNED_INT:
    case ITEM_CAST_DATE:
      return false;
    case ITEM_CAST_YEAR:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of YEAR");
        return true;
      }
      return false;
    case ITEM_CAST_TIME:
    case ITEM_CAST_DATETIME: {
      uint dec = c_dec ? strtoul(c_dec, nullptr, 10) : 0;
      if (dec > DATETIME_MAX_DECIMALS) {
        my_error(ER_TOO_BIG_PRECISION, MYF(0), dec, "CAST",
                 DATETIME_MAX_DECIMALS);
        return true;
      }
      *precision = dec;
      return false;
    }
    case ITEM_CAST_DECIMAL: {
      ulong len = 0;
      uint dec = 0;

      if (c_len) {
        ulong decoded_size;
        errno = 0;
        decoded_size = strtoul(c_len, nullptr, 10);
        if (errno != 0) {
          StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                                 system_charset_info);
          my_error(ER_TOO_BIG_PRECISION, MYF(0), INT_MAX, buff.c_ptr_safe(),
                   static_cast<ulong>(DECIMAL_MAX_PRECISION));
          return true;
        }
        len = decoded_size;
      }

      if (c_dec) {
        ulong decoded_size;
        errno = 0;
        decoded_size = strtoul(c_dec, nullptr, 10);
        if ((errno != 0) || (decoded_size > UINT_MAX)) {
          // The parser rejects scale values above INT32_MAX, so this error path
          // is never taken.
          /* purecov: begin inspected */
          StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                                 system_charset_info);
          my_error(ER_TOO_BIG_SCALE, MYF(0), INT_MAX, buff.c_ptr_safe(),
                   static_cast<ulong>(DECIMAL_MAX_SCALE));
          return true;
          /* purecov: end */
        }
        dec = decoded_size;
      }
      my_decimal_trim(&len, &dec);
      if (len < dec) {
        my_error(ER_M_BIGGER_THAN_D, MYF(0), "");
        return true;
      }
      if (len > DECIMAL_MAX_PRECISION) {
        StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                               system_charset_info);
        my_error(ER_TOO_BIG_PRECISION, MYF(0), static_cast<int>(len),
                 buff.c_ptr_safe(), static_cast<ulong>(DECIMAL_MAX_PRECISION));
        return true;
      }
      if (dec > DECIMAL_MAX_SCALE) {
        StringBuffer<192> buff(pos.cpp.start, pos.cpp.length(),
                               system_charset_info);
        my_error(ER_TOO_BIG_SCALE, MYF(0), dec, buff.c_ptr_safe(),
                 static_cast<ulong>(DECIMAL_MAX_SCALE));
        return true;
      }
      *length = len;
      *precision = dec;
      return false;
    }
    case ITEM_CAST_CHAR: {
      longlong len = -1;
      if (c_len) {
        int error;
        len = my_strtoll10(c_len, nullptr, &error);
        if ((error != 0) || (len > MAX_FIELD_BLOBLENGTH)) {
          my_error(ER_TOO_BIG_DISPLAYWIDTH, MYF(0), "cast as char",
                   static_cast<unsigned long>(MAX_FIELD_BLOBLENGTH));
          return true;
        }
      }
      if (as_array && (len == -1 || len > CONVERT_IF_BIGGER_TO_BLOB)) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of char/binary BLOBs");
        return true;
      }
      *length = len;
      return false;
    }
    case ITEM_CAST_DOUBLE:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of DOUBLE");
        return true;
      }
      return false;
    case ITEM_CAST_FLOAT: {
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of FLOAT");
        return true;
      }

<<<<<<< HEAD
      ulong decoded_size = 0;

      // Check if binary precision is specified
      if (c_len != nullptr) {
        errno = 0;
        decoded_size = strtoul(c_len, nullptr, 10);
        if (errno != 0 || decoded_size > PRECISION_FOR_DOUBLE) {
          my_error(ER_TOO_BIG_PRECISION, MYF(0), decoded_size, "CAST",
                   PRECISION_FOR_DOUBLE);
          return true;
        }
      }
      *length = decoded_size;
      return false;
    }
    case ITEM_CAST_JSON:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of JSON");
        return true;
      }
      return false;
    case ITEM_CAST_POINT:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of POINT");
        return true;
      }
      return false;
    case ITEM_CAST_LINESTRING:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of LINESTRING");
        return true;
      }
      return false;
    case ITEM_CAST_POLYGON:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of POLYGON");
        return true;
      }
      return false;
    case ITEM_CAST_MULTIPOINT:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of MULTIPOINT");
        return true;
      }
      return false;
    case ITEM_CAST_MULTILINESTRING:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of MULTILINESTRING>");
        return true;
      }
      return false;
    case ITEM_CAST_MULTIPOLYGON:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of MULTIPOLYGON");
        return true;
      }
      return false;
    case ITEM_CAST_GEOMETRYCOLLECTION:
      if (as_array) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                 "CAST-ing data to array of GEOMETRYCOLLECTION");
        return true;
      }
      return false;
=======
<<<<<<< HEAD
      break;
    }
    default: {
      DBUG_ASSERT(0);
      res = 0;
      break;
    }
=======
    break;
  }
  default:
  {
    assert(0);
    res= 0;
    break;
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  /* purecov: begin deadcode */
  assert(false);
  return true;
  /* purecov: end */
}

/// This function does not store the reference to `type`.
Item *create_func_cast(THD *thd, const POS &pos, Item *arg,
                       const Cast_type &type, bool as_array) {
  int64_t length = 0;
  unsigned precision = 0;
  if (validate_cast_type_and_extract_length(thd, pos, arg, type, as_array,
                                            &length, &precision))
    return nullptr;

  if (as_array) {
    return new (thd->mem_root) Item_func_array_cast(
        pos, arg, type.target, length, precision, type.charset);
  }

  switch (type.target) {
    case ITEM_CAST_SIGNED_INT:
      return new (thd->mem_root) Item_typecast_signed(pos, arg);
    case ITEM_CAST_UNSIGNED_INT:
      return new (thd->mem_root) Item_typecast_unsigned(pos, arg);
    case ITEM_CAST_DATE:
      return new (thd->mem_root) Item_typecast_date(pos, arg);
    case ITEM_CAST_TIME:
      return new (thd->mem_root) Item_typecast_time(pos, arg, precision);
    case ITEM_CAST_DATETIME:
      return new (thd->mem_root) Item_typecast_datetime(pos, arg, precision);
    case ITEM_CAST_YEAR:
      return new (thd->mem_root) Item_typecast_year(pos, arg);
    case ITEM_CAST_DECIMAL:
      return new (thd->mem_root)
          Item_typecast_decimal(pos, arg, length, precision);
    case ITEM_CAST_CHAR: {
      const CHARSET_INFO *cs = type.charset;
      if (cs == nullptr) cs = thd->variables.collation_connection;
      return new (thd->mem_root) Item_typecast_char(pos, arg, length, cs);
    }
    case ITEM_CAST_JSON:
      return new (thd->mem_root) Item_typecast_json(thd, pos, arg);
    case ITEM_CAST_FLOAT:
      return new (thd->mem_root) Item_typecast_real(
          pos, arg, /*as_double=*/(length > PRECISION_FOR_FLOAT));
    case ITEM_CAST_DOUBLE:
      return new (thd->mem_root)
          Item_typecast_real(pos, arg, /*as_double=*/true);
    case ITEM_CAST_POINT:
      return new (thd->mem_root) Item_typecast_point(pos, arg);
    case ITEM_CAST_LINESTRING:
      return new (thd->mem_root) Item_typecast_linestring(pos, arg);
    case ITEM_CAST_POLYGON:
      return new (thd->mem_root) Item_typecast_polygon(pos, arg);
    case ITEM_CAST_MULTIPOINT:
      return new (thd->mem_root) Item_typecast_multipoint(pos, arg);
    case ITEM_CAST_MULTILINESTRING:
      return new (thd->mem_root) Item_typecast_multilinestring(pos, arg);
    case ITEM_CAST_MULTIPOLYGON:
      return new (thd->mem_root) Item_typecast_multipolygon(pos, arg);
    case ITEM_CAST_GEOMETRYCOLLECTION:
      return new (thd->mem_root) Item_typecast_geometrycollection(pos, arg);
  }

  /* purecov: begin deadcode */
  assert(false);
  return nullptr;
  /* purecov: end */
}

Item *create_func_json_value(THD *thd, const POS &pos, Item *arg, Item *path,
                             const Cast_type &cast_type,
                             Json_on_response_type on_empty_type,
                             Item *on_empty_default,
                             Json_on_response_type on_error_type,
                             Item *on_error_default) {
  int64_t length = 0;
  unsigned precision = 0;
  if (validate_cast_type_and_extract_length(thd, pos, arg, cast_type, false,
                                            &length, &precision))
    return nullptr;

  // Create dummy items for the default values, if they haven't been specified.
  if (on_empty_default == nullptr)
    on_empty_default = new (thd->mem_root) Item_null;
  if (on_error_default == nullptr)
    on_error_default = new (thd->mem_root) Item_null;

  return new (thd->mem_root) Item_func_json_value(
      pos, arg, path, cast_type, length, precision, on_empty_type,
      on_empty_default, on_error_type, on_error_default);
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

Item *create_temporal_literal(THD *thd, const char *str, size_t length,
                              const CHARSET_INFO *cs, enum_field_types type,
                              bool send_error) {
  MYSQL_TIME_STATUS status;
  MYSQL_TIME ltime;
  Item *item = nullptr;
  my_time_flags_t flags = TIME_FUZZY_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_IN_DATE)
    flags |= TIME_NO_ZERO_IN_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE) flags |= TIME_NO_ZERO_DATE;

  if (thd->variables.sql_mode & MODE_INVALID_DATES) flags |= TIME_INVALID_DATES;

<<<<<<< HEAD
  switch (type) {
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      if (!propagate_datetime_overflow(
              thd, &status.warnings,
              str_to_datetime(cs, str, length, &ltime, flags, &status)) &&
          ltime.time_type == MYSQL_TIMESTAMP_DATE && !status.warnings) {
        check_deprecated_datetime_format(thd, cs, status);
        item = new (thd->mem_root) Item_date_literal(&ltime);
      }
      break;
    case MYSQL_TYPE_DATETIME:
      if (!propagate_datetime_overflow(
              thd, &status.warnings,
              str_to_datetime(cs, str, length, &ltime, flags, &status)) &&
          (ltime.time_type == MYSQL_TIMESTAMP_DATETIME ||
           ltime.time_type == MYSQL_TIMESTAMP_DATETIME_TZ) &&
          !status.warnings) {
        check_deprecated_datetime_format(thd, cs, status);
        if (convert_time_zone_displacement(thd->time_zone(), &ltime))
          return nullptr;
        item = new (thd->mem_root) Item_datetime_literal(
            &ltime, status.fractional_digits, thd->time_zone());
      }
      break;
    case MYSQL_TYPE_TIME:
      if (!str_to_time(cs, str, length, &ltime, 0, &status) &&
          ltime.time_type == MYSQL_TIMESTAMP_TIME && !status.warnings) {
        check_deprecated_datetime_format(thd, cs, status);
        item = new (thd->mem_root)
            Item_time_literal(&ltime, status.fractional_digits);
      }
      break;
    default:
<<<<<<< HEAD
      assert(0);
=======
      DBUG_ASSERT(0);
=======
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
    assert(0);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }

  if (item) return item;

  if (send_error) {
    const char *typestr = (type == MYSQL_TYPE_DATE)
                              ? "DATE"
                              : (type == MYSQL_TYPE_TIME) ? "TIME" : "DATETIME";
    ErrConvString err(str, length, thd->variables.character_set_client);
    my_error(ER_WRONG_VALUE, MYF(0), typestr, err.ptr());
  }
  return nullptr;
}

/**
  @} (end of group GROUP_PARSER)
*/

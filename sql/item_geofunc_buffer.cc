/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
  @file
  @brief
  This file contains the implementation for the Item that implements
  ST_Buffer().
*/

#include <sys/types.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>  // std::unique_ptr
#include <vector>

#include <boost/concept/usage.hpp>
#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/strategies/agnostic/buffer_distance_symmetric.hpp>
#include <boost/geometry/strategies/buffer.hpp>
#include <boost/geometry/strategies/cartesian/buffer_end_flat.hpp>
#include <boost/geometry/strategies/cartesian/buffer_end_round.hpp>
#include <boost/geometry/strategies/cartesian/buffer_join_miter.hpp>
#include <boost/geometry/strategies/cartesian/buffer_join_round.hpp>
#include <boost/geometry/strategies/cartesian/buffer_point_circle.hpp>
#include <boost/geometry/strategies/cartesian/buffer_point_square.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side_straight.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include "m_ctype.h"
#include "m_string.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/derror.h"  // ER_THD
#include "sql/item.h"
#include "sql/item_geofunc.h"
#include "sql/item_geofunc_internal.h"
#include "sql/item_strfunc.h"
#include "sql/parse_location.h"  // POS
#include "sql/spatial.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"
#include "sql/srs_fetcher.h"
#include "sql/system_variables.h"
#include "sql_string.h"
#include "template_utils.h"

class PT_item_list;

namespace boost {
namespace geometry {
namespace cs {
struct cartesian;
}  // namespace cs
}  // namespace geometry
}  // namespace boost

static const char *const buffer_strategy_names[] = {
    "invalid_strategy", "end_round",    "end_flat",    "join_round",
    "join_miter",       "point_circle", "point_square"};

template <typename Char_type>
inline int char_icmp(const Char_type a, const Char_type b) {
  const int a1 = std::tolower(a);
  const int b1 = std::tolower(b);
  return a1 > b1 ? 1 : (a1 < b1 ? -1 : 0);
}

/**
  Case insensitive comparison of two ascii strings.
  @param a '\0' ended string.
  @param b '\0' ended string.
 */
template <typename Char_type>
int str_icmp(const Char_type *a, const Char_type *b) {
  int ret = 0, i;

  for (i = 0; a[i] != 0 && b[i] != 0; i++)
    if ((ret = char_icmp(a[i], b[i]))) return ret;
  if (a[i] == 0 && b[i] != 0) return -1;
  if (a[i] != 0 && b[i] == 0) return 1;
  return 0;
}

Item_func_buffer_strategy::Item_func_buffer_strategy(const POS &pos,
                                                     PT_item_list *ilist)
    : Item_str_func(pos, ilist) {
  // Here we want to use the String::set(const char*, ..) version.
  const char *pbuf = tmp_buffer;
  tmp_value.set(pbuf, 0, nullptr);
}

bool Item_func_buffer_strategy::resolve_type(THD *thd) {
  if (param_type_is_default(thd, 0, 1)) return true;
  if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
  set_data_type_string(16, &my_charset_bin);
  set_nullable(true);
  return false;
}

String *Item_func_buffer_strategy::val_str(String * /* str_arg */) {
  String str;
  String *strat_name = args[0]->val_str_ascii(&str);
  if ((null_value = args[0]->null_value)) {
    assert(is_nullable());
    return nullptr;
  }

  // Get the NULL-terminated ascii string.
  const char *pstrat_name = strat_name->c_ptr_safe();

  bool found = false;

  tmp_value.set_charset(&my_charset_bin);
  // The tmp_value is supposed to always stores a {uint32,double} pair,
  // and it uses a char tmp_buffer[16] array data member.
  uchar *result_buf = pointer_cast<uchar *>(tmp_value.ptr());

  // Although the result of this item node is never persisted, we still have to
  // use portable endianness access otherwise unaligned access will crash
  // on sparc CPUs.
  for (uint32 i = 0; i <= max_strategy; i++) {
    // The above var_str_ascii() call makes the strat_name an ascii string so
    // we can do below comparison.
    if (str_icmp(pstrat_name, buffer_strategy_names[i]) != 0) continue;

    int4store(result_buf, i);
    result_buf += 4;
    enum_buffer_strategies istrat = static_cast<enum_buffer_strategies>(i);

    /*
      The end_flat and point_square strategies must have no more arguments;
      The rest strategies must have 2nd parameter which must be a positive
      numeric value, and we will store it as a double.
      We use float8store to ensure that the value is independent of endianness.
    */
    if (istrat != end_flat && istrat != point_square) {
      if (arg_count != 2) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
        return error_str();
      }

      double val = args[1]->val_real();
      if ((null_value = args[1]->null_value)) {
        assert(is_nullable());
        return nullptr;
      }
      if (val <= 0) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
        return error_str();
      }

      if (istrat != join_miter &&
          val > current_thd->variables.max_points_in_geometry) {
        my_error(ER_GIS_MAX_POINTS_IN_GEOMETRY_OVERFLOWED, MYF(0),
                 "points_per_circle",
                 current_thd->variables.max_points_in_geometry, func_name());
        return error_str();
      }

      float8store(result_buf, val);
    } else if (arg_count != 1) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
      return error_str();
    } else
      float8store(result_buf, 0.0);

    found = true;

    break;
  }

  // Unrecognized strategy names, report error.
  if (!found) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return error_str();
  }
  tmp_value.length(12);

  return &tmp_value;
}

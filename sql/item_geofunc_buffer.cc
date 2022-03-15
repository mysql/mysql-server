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

/*
  Convert strategies stored in String objects into Strategy_setting objects.
*/
void Item_func_buffer::set_strategies() {
  for (int i = 0; i < num_strats; i++) {
    String *pstr = strategies[i];
    const uchar *pstrat = pointer_cast<const uchar *>(pstr->ptr());

    uint32 snum = 0;

    if (pstr->length() != 12 ||
        !((snum = uint4korr(pstrat)) > invalid_strategy &&
          snum <= max_strategy)) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "st_buffer");
      null_value = true;
      return;
    }

    const enum_buffer_strategies strat = (enum_buffer_strategies)snum;
    double value = float8get(pstrat + 4);
    enum_buffer_strategy_types strategy_type = invalid_strategy_type;

    switch (strat) {
      case end_round:
      case end_flat:
        strategy_type = end_strategy;
        break;
      case join_round:
      case join_miter:
        strategy_type = join_strategy;
        break;
      case point_circle:
      case point_square:
        strategy_type = point_strategy;
        break;
      default:
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "st_buffer");
        null_value = true;
        return;
        break;
    }

    // Each strategy option can be set no more than once for every ST_Buffer()
    // call.
    if (settings[strategy_type].strategy != invalid_strategy) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "st_buffer");
      null_value = true;
      return;
    } else {
      settings[strategy_type].strategy = (enum_buffer_strategies)snum;
      settings[strategy_type].value = value;
    }
  }
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
  for (uint32 i = 0; i <= Item_func_buffer::max_strategy; i++) {
    // The above var_str_ascii() call makes the strat_name an ascii string so
    // we can do below comparison.
    if (str_icmp(pstrat_name, buffer_strategy_names[i]) != 0) continue;

    int4store(result_buf, i);
    result_buf += 4;
    Item_func_buffer::enum_buffer_strategies istrat =
        static_cast<Item_func_buffer::enum_buffer_strategies>(i);

    /*
      The end_flat and point_square strategies must have no more arguments;
      The rest strategies must have 2nd parameter which must be a positive
      numeric value, and we will store it as a double.
      We use float8store to ensure that the value is independent of endianness.
    */
    if (istrat != Item_func_buffer::end_flat &&
        istrat != Item_func_buffer::point_square) {
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

      if (istrat != Item_func_buffer::join_miter &&
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

#define CALL_BG_BUFFER(result, geom, geom_out, dist_strategy, side_strategy, \
                       join_strategy, end_strategy, point_strategy)          \
  do {                                                                       \
    (result) = false;                                                        \
    switch ((geom)->get_type()) {                                            \
      case Geometry::wkb_point: {                                            \
        BG_models<bgcs::cartesian>::Point bg(                                \
            (geom)->get_data_ptr(), (geom)->get_data_size(),                 \
            (geom)->get_flags(), (geom)->get_srid());                        \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      case Geometry::wkb_multipoint: {                                       \
        BG_models<bgcs::cartesian>::Multipoint bg(                           \
            (geom)->get_data_ptr(), (geom)->get_data_size(),                 \
            (geom)->get_flags(), (geom)->get_srid());                        \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      case Geometry::wkb_linestring: {                                       \
        BG_models<bgcs::cartesian>::Linestring bg(                           \
            (geom)->get_data_ptr(), (geom)->get_data_size(),                 \
            (geom)->get_flags(), (geom)->get_srid());                        \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      case Geometry::wkb_multilinestring: {                                  \
        BG_models<bgcs::cartesian>::Multilinestring bg(                      \
            (geom)->get_data_ptr(), (geom)->get_data_size(),                 \
            (geom)->get_flags(), (geom)->get_srid());                        \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      case Geometry::wkb_polygon: {                                          \
        const void *data_ptr = (geom)->normalize_ring_order();               \
        if (data_ptr == NULL) {                                              \
          my_error(ER_GIS_INVALID_DATA, MYF(0), "st_buffer");                \
          (result) = true;                                                   \
          break;                                                             \
        }                                                                    \
        BG_models<bgcs::cartesian>::Polygon bg(                              \
            data_ptr, (geom)->get_data_size(), (geom)->get_flags(),          \
            (geom)->get_srid());                                             \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      case Geometry::wkb_multipolygon: {                                     \
        const void *data_ptr = (geom)->normalize_ring_order();               \
        if (data_ptr == NULL) {                                              \
          my_error(ER_GIS_INVALID_DATA, MYF(0), "st_buffer");                \
          (result) = true;                                                   \
          break;                                                             \
        }                                                                    \
        BG_models<bgcs::cartesian>::Multipolygon bg(                         \
            data_ptr, (geom)->get_data_size(), (geom)->get_flags(),          \
            (geom)->get_srid());                                             \
        bg::buffer(bg, (geom_out), (dist_strategy), (side_strategy),         \
                   (join_strategy), (end_strategy), (point_strategy));       \
        break;                                                               \
      }                                                                      \
      default:                                                               \
        assert(false);                                                       \
        break;                                                               \
    }                                                                        \
  } while (0)

Item_func_buffer::Item_func_buffer(const POS &pos, PT_item_list *ilist)
    : Item_geometry_func(pos, ilist) {
  num_strats = 0;
  memset(settings, 0, sizeof(settings));
  memset(strategies, 0, sizeof(strategies));
}

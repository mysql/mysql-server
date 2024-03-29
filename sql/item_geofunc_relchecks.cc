/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#include <stddef.h>
#include <boost/concept/usage.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/types/spatial_reference_system.h"
#include "sql/derror.h"  // ER_THD
#include "sql/gis/geometries.h"
#include "sql/gis/relops.h"
#include "sql/gis/srid.h"
#include "sql/gis/wkb.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_geofunc.h"
#include "sql/item_geofunc_internal.h"
#include "sql/spatial.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"
#include "sql/srs_fetcher.h"
#include "sql_string.h"

namespace boost {
namespace geometry {
namespace cs {
struct cartesian;
}  // namespace cs
}  // namespace geometry
}  // namespace boost

longlong Item_func_spatial_relation::val_int() {
  DBUG_TRACE;
  assert(fixed);

  String tmp_value1;
  String *res1 = args[0]->val_str(&tmp_value1);
  if (current_thd->is_error()) return error_int();
  if ((null_value = (res1 == nullptr || args[0]->null_value))) {
    assert(is_nullable());
    return 0;
  }
  String tmp_value2;
  String *res2 = args[1]->val_str(&tmp_value2);
  if (current_thd->is_error()) return error_int();
  if ((null_value = (res2 == nullptr || args[1]->null_value))) {
    assert(is_nullable());
    return 0;
  }

  if (res1 == nullptr || res2 == nullptr) {
    assert(false);
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }

  const dd::Spatial_reference_system *srs1 = nullptr;
  const dd::Spatial_reference_system *srs2 = nullptr;
  std::unique_ptr<gis::Geometry> g1;
  std::unique_ptr<gis::Geometry> g2;
  std::unique_ptr<dd::cache::Dictionary_client::Auto_releaser> releaser(
      new dd::cache::Dictionary_client::Auto_releaser(
          current_thd->dd_client()));
  if (gis::parse_geometry(current_thd, func_name(), res1, &srs1, &g1) ||
      gis::parse_geometry(current_thd, func_name(), res2, &srs2, &g2)) {
    return error_int();
  }

  gis::srid_t srid1 = srs1 == nullptr ? 0 : srs1->id();
  gis::srid_t srid2 = srs2 == nullptr ? 0 : srs2->id();
  if (srid1 != srid2) {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(), srid1, srid2);
    return error_int();
  }

  bool result;
  bool error = eval(srs1, g1.get(), g2.get(), &result, &null_value);

  if (error) return error_int();

  if (null_value) {
    assert(is_nullable());
    return 0;
  }

  return result;
}

bool Item_func_st_contains::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::within(srs, g2, g1, func_name(), result, null);
}

bool Item_func_st_crosses::eval(const dd::Spatial_reference_system *srs,
                                const gis::Geometry *g1,
                                const gis::Geometry *g2, bool *result,
                                bool *null) {
  return gis::crosses(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_disjoint::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::disjoint(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_equals::eval(const dd::Spatial_reference_system *srs,
                               const gis::Geometry *g1, const gis::Geometry *g2,
                               bool *result, bool *null) {
  return gis::equals(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_intersects::eval(const dd::Spatial_reference_system *srs,
                                   const gis::Geometry *g1,
                                   const gis::Geometry *g2, bool *result,
                                   bool *null) {
  return gis::intersects(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrcontains::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::mbr_within(srs, g2, g1, func_name(), result, null);
}

bool Item_func_mbrcoveredby::eval(const dd::Spatial_reference_system *srs,
                                  const gis::Geometry *g1,
                                  const gis::Geometry *g2, bool *result,
                                  bool *null) {
  return gis::mbr_covered_by(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrcovers::eval(const dd::Spatial_reference_system *srs,
                               const gis::Geometry *g1, const gis::Geometry *g2,
                               bool *result, bool *null) {
  return gis::mbr_covered_by(srs, g2, g1, func_name(), result, null);
}

bool Item_func_mbrdisjoint::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::mbr_disjoint(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrequals::eval(const dd::Spatial_reference_system *srs,
                               const gis::Geometry *g1, const gis::Geometry *g2,
                               bool *result, bool *null) {
  return gis::mbr_equals(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrintersects::eval(const dd::Spatial_reference_system *srs,
                                   const gis::Geometry *g1,
                                   const gis::Geometry *g2, bool *result,
                                   bool *null) {
  return gis::mbr_intersects(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbroverlaps::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::mbr_overlaps(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrtouches::eval(const dd::Spatial_reference_system *srs,
                                const gis::Geometry *g1,
                                const gis::Geometry *g2, bool *result,
                                bool *null) {
  return gis::mbr_touches(srs, g1, g2, func_name(), result, null);
}

bool Item_func_mbrwithin::eval(const dd::Spatial_reference_system *srs,
                               const gis::Geometry *g1, const gis::Geometry *g2,
                               bool *result, bool *null) {
  return gis::mbr_within(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_overlaps::eval(const dd::Spatial_reference_system *srs,
                                 const gis::Geometry *g1,
                                 const gis::Geometry *g2, bool *result,
                                 bool *null) {
  return gis::overlaps(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_touches::eval(const dd::Spatial_reference_system *srs,
                                const gis::Geometry *g1,
                                const gis::Geometry *g2, bool *result,
                                bool *null) {
  return gis::touches(srs, g1, g2, func_name(), result, null);
}

bool Item_func_st_within::eval(const dd::Spatial_reference_system *srs,
                               const gis::Geometry *g1, const gis::Geometry *g2,
                               bool *result, bool *null) {
  return gis::within(srs, g1, g2, func_name(), result, null);
}

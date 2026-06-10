// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_SETS_INTERVAL_CONST_VIEWS_H
#define MYSQL_SETS_INTERVAL_CONST_VIEWS_H

/// @file
/// Experimental API header

#include "mysql/sets/base_const_views.h"        // Empty_set_view
#include "mysql/sets/boundary_set_category.h"   // Boundary_set_category_tag
#include "mysql/sets/interval_set_category.h"   // Interval_set_category_tag
#include "mysql/sets/interval_set_interface.h"  // Interval_set_interface

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// View over the empty Interval set for the given Set traits.
template <Is_bounded_set_traits Set_traits_tp>
class Empty_set_view<Interval_set_category_tag, Set_traits_tp>
    : public Interval_set_interface<
          Empty_set_view<Interval_set_category_tag, Set_traits_tp>,
          Empty_set_view<Boundary_set_category_tag, Set_traits_tp>> {
 public:
  [[nodiscard]] const auto &boundaries() const {
    return make_empty_set_view<Boundary_set_category_tag, Set_traits_tp>();
  }
};

/// View over the Interval set containing the full range of values for
/// the given Set traits.
template <Is_bounded_set_traits Set_traits_tp>
class Full_set_view<Interval_set_category_tag, Set_traits_tp>
    : public Interval_set_interface<
          Full_set_view<Interval_set_category_tag, Set_traits_tp>,
          Full_set_view<Boundary_set_category_tag, Set_traits_tp>> {
 public:
  [[nodiscard]] const auto &boundaries() const {
    return make_full_set_view<Boundary_set_category_tag, Set_traits_tp>();
  }
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_CONST_VIEWS_H

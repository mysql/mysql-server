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

#ifndef MYSQL_SETS_INTERVAL_SET_UNION_VIEW_H
#define MYSQL_SETS_INTERVAL_SET_UNION_VIEW_H

/// @file
/// Experimental API header

#include "mysql/sets/base_binary_operation_views.h"  // Union_view
#include "mysql/sets/interval_set_binary_operation_view_base.h"  // Interval_set_binary_operation_view_base
#include "mysql/sets/interval_set_meta.h"          // Is_interval_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Specialization of Union_view for interval sets.
///
/// This provides forward iterators.
///
/// @tparam Source1_tp Type of first interval set.
///
/// @tparam Source2_tp Type of second interval set.
template <Is_interval_set Source1_tp, Is_interval_set Source2_tp>
class Union_view<Source1_tp, Source2_tp>
    : public detail::Interval_set_binary_operation_view_base<
          Source1_tp, Source2_tp, Binary_operation::op_union> {
  using Base_t = detail::Interval_set_binary_operation_view_base<
      Source1_tp, Source2_tp, Binary_operation::op_union>;

 public:
  /// Construct a new, empty view.
  Union_view() noexcept = default;

  /// Construct a new view over the two given interval set sources.
  ///
  /// @param source1 The first source.
  ///
  /// @param source2 The second source.
  Union_view(const Source1_tp &source1, const Source2_tp &source2) noexcept
      : Base_t(source1, source2) {}

  /// Construct a new view over the two given interval set sources.
  ///
  /// Use this constructor if one of the sources may be empty and you do not
  /// have an object representing it; then pass nullptr for that source.
  ///
  /// @param source1 The first source, or nullptr for empty set.
  ///
  /// @param source2 The second source, or nullptr for empty set.
  Union_view(const Source1_tp *source1, const Source2_tp *source2) noexcept
      : Base_t(source1, source2) {}
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_UNION_VIEW_H

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

#ifndef MYSQL_SETS_NESTED_SET_VOLUME_H
#define MYSQL_SETS_NESTED_SET_VOLUME_H

/// @file
/// Experimental API header

#include <numeric>                                 // accumulate
#include <ranges>                                  // range
#include "mysql/math/summation.h"                  // sequence_sum_difference
#include "mysql/ranges/flat_view.h"                // make_flat_view
#include "mysql/ranges/projection_views.h"         // make_mapped_view
#include "mysql/ranges/transform_view.h"           // Transform_view
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Given a set, returns its volume.
struct Volume_transform {
  static auto transform(const auto &set) { return volume(set); }
};

/// Given a nested set, returns a range over its inner sets.
struct Unfold_set {
  [[nodiscard]] static auto unfold(const Is_nested_set auto &nested_set) {
    return mysql::ranges::make_mapped_view(nested_set);
  }
};

/// Given a Nested set, returns a range over the volumes of the inner sets.
template <Is_nested_set Nested_set_t>
[[nodiscard]] auto make_volume_view(const Nested_set_t &nested_set) {
  auto flat_view =
      mysql::ranges::make_flat_view<detail::Unfold_set>(nested_set);
  return mysql::ranges::make_transform_view<Volume_transform>(flat_view);
}

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Return the volume of a nested set.
///
/// @tparam Result_t Type of return value. Default is double. The result will
/// be exact if it is at most `mysql::math::max_exact_int<double>`.
///
/// @tparam Nested_set_t Type of nested set set.
///
/// @param set Set.
template <class Result_t = double, Is_nested_set Nested_set_t>
[[nodiscard]] Result_t volume(const Nested_set_t &set) {
  auto volume_view = make_volume_view(set);
  auto ret =
      std::accumulate(volume_view.begin(), volume_view.end(), Result_t{});
  return ret;
}

/// Return the volume of the first nested set minus the volume of the second
/// nested set.
///
/// @tparam Result_t Type of return value. Default is double. The result will
/// be exact if it is at most `mysql::math::max_exact_int<double>`.
///
/// @tparam Set1_t Type of first set.
///
/// @tparam Set2_t Type of second set.
///
/// param set1 First set.
///
/// param set2 Second set.
//
// Doxygen complains that the parameters are duplicated (maybe mixing up
// overloads with different constraints). We work around that by using param
// instead of @param.
template <class Result_t = double, Is_nested_set Set1_t, Is_nested_set Set2_t>
  requires Is_compatible_set<Set1_t, Set2_t>
[[nodiscard]] Result_t volume_difference(const Set1_t &set1,
                                         const Set2_t &set2) {
  auto volume_view1 = make_volume_view(set1);
  auto volume_view2 = make_volume_view(set2);
  return mysql::math::sequence_sum_difference<Result_t>(
      volume_view1.begin(), volume_view1.end(), volume_view2.begin(),
      volume_view2.end());
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_VOLUME_H

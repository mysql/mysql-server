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

#ifndef MYSQL_SETS_BASE_VOLUME_H
#define MYSQL_SETS_BASE_VOLUME_H

/// @file
/// Experimental API header

#include "mysql/sets/set_categories_and_traits.h"  // Is_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Return the volume of the first set minus the volume of the second set.
///
/// This is the default implementation. The result is accurate for set types
/// whose volumes can be represented without loss of precision in the return
/// type from `volume`, i.e., typically set types whose volume does not exceed
/// std::numeric_limits<uint64_t>::max(). Other set types should override this
/// function to avoid precision loss for large sets of almost the same size.
///
/// @tparam Result_t Type of return value. Default is double. The result will be
/// exact if it is at most `mysql::math::max_exact_int<double>`.
///
/// @tparam Set1_t Type of first set.
///
/// @tparam Set2_t Type of second set.
///
/// @param set1 First set.
///
/// @param set2 Second set.
template <class Result_t = double, Is_set Set1_t, Is_set Set2_t>
  requires Is_compatible_set<Set1_t, Set2_t>
[[nodiscard]] Result_t volume_difference(const Set1_t &set1,
                                         const Set2_t &set2) {
  auto c1 = volume(set1);
  auto c2 = volume(set2);
  if (c1 < c2) return -Result_t(c2 - c1);
  return Result_t(c1 - c2);
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BASE_VOLUME_H

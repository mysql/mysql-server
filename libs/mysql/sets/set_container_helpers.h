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

#ifndef MYSQL_SETS_SET_CONTAINER_HELPERS_H
#define MYSQL_SETS_SET_CONTAINER_HELPERS_H

/// @file
/// Experimental API header

#include <utility>                        // forward
#include "mysql/sets/binary_operation.h"  // Binary_operation
#include "mysql/sets/meta.h"              // Has_fast_size
#include "mysql/utils/is_same_object.h"   // is_same_object

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Handle the trivial cases of inplace union/intersection/subtraction where
/// either both operands refer to the same set or one is empty.
///
/// @return true if a trivial case applied and was executed, false otherwise.
template <Binary_operation operation, class Target_t, class Source_t>
bool handle_inplace_op_trivial_cases(Target_t &target, Source_t &&source) {
  if (mysql::utils::is_same_object(source, target)) {
    if constexpr (operation == Binary_operation::op_subtraction) target.clear();
    // self-union and self-intersection are no-ops.
    return true;
  }
  if (Has_fast_size<Source_t> && source.empty()) {
    if constexpr (operation == Binary_operation::op_intersection)
      target.clear();
    // union and subtraction with an empty set RHS are no-ops.
    return true;
  }
  if (Has_fast_size<Target_t> && target.empty()) {
    if constexpr (operation == Binary_operation::op_union) {
      // Overwrite target by source, if we can do it without having to propagate
      // a returned error status. (Which typically is the case if target is
      // throwing).
      if constexpr (std::same_as<decltype(target.assign(
                                     std::forward<Source_t>(source))),
                                 void>) {
        target.assign(std::forward<Source_t>(source));
        return true;
      }
    } else {
      // intersection and subtraction on an empty set LHS are no-ops.
      return true;
    }
  }
  return false;
}

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_SET_CONTAINER_HELPERS_H

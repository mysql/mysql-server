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

#ifndef MYSQL_UTILS_IS_SAME_OBJECT_H
#define MYSQL_UTILS_IS_SAME_OBJECT_H

/// @file
/// Experimental API header

#include <concepts>  // derived_from

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// Return true if the types of the two objects are either equal or one derived
/// from the other, and in addition, refer to the same object.
template <class Obj1_t, class Obj2_t>
bool is_same_object(const Obj1_t &obj1, const Obj2_t &obj2) {
  if constexpr (std::derived_from<Obj1_t, Obj2_t> ||
                std::derived_from<Obj2_t, Obj1_t>) {
    return &obj1 == &obj2;
  } else {
    return false;
  }
}

}  // namespace mysql::utils

// addtogroup GroupLibsMysqlUtils
/// @}

#endif  // ifndef MYSQL_UTILS_IS_SAME_OBJECT_H

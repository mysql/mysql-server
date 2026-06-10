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

#ifndef MYSQL_SETS_INTERVAL_SET_CATEGORY_H
#define MYSQL_SETS_INTERVAL_SET_CATEGORY_H

/// @file
/// Experimental API header

#include "mysql/sets/set_categories.h"  // Base_set_category_tag

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Tag to identify a class as an Interval set.
///
/// Interval set classes should have the member type Set_category_t defined to
/// this class, and meet the requirements for Is_interval_set.
struct Interval_set_category_tag : public Base_set_category_tag {};

/// Declare that Interval sets are iterator-defined. See Is_iterator_defined_set
/// for details.
template <>
inline constexpr bool
    is_iterator_defined_set_category<Interval_set_category_tag> = true;

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_CATEGORY_H

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

#ifndef MYSQL_SETS_NESTED_SET_TRAITS_H
#define MYSQL_SETS_NESTED_SET_TRAITS_H

/// @file
/// Experimental API header

#include "mysql/sets/set_categories.h"  // Is_set_category
#include "mysql/sets/set_traits.h"      // Is_ordered_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Set traits for Nested sets
///
/// @tparam Key_traits_tp Traits for the key type. This must satisfy
/// Ordered_set_traits.
///
/// @tparam Mapped_traits_tp Traits for the mapped type.
///
/// @tparam Mapped_category_tp Category for the mapped type.
template <Is_ordered_set_traits Key_traits_tp, Is_set_traits Mapped_traits_tp,
          Is_set_category Mapped_category_tp>
struct Nested_set_traits : public Base_set_traits {
  using Key_traits_t = Key_traits_tp;
  using Mapped_category_t = Mapped_category_tp;
  using Mapped_traits_t = Mapped_traits_tp;
  using Key_t = Key_traits_tp::Element_t;
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_TRAITS_H

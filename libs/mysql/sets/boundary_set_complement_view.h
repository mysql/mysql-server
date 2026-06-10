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

#ifndef MYSQL_SETS_BOUNDARY_SET_COMPLEMENT_VIEW_H
#define MYSQL_SETS_BOUNDARY_SET_COMPLEMENT_VIEW_H

/// @file
/// Experimental API header

#include "mysql/sets/base_binary_operation_views.h"  // Subtraction_view
#include "mysql/sets/base_complement_view.h"         // Complement_view
#include "mysql/sets/boundary_set_category.h"     // Boundary_set_category_tag
#include "mysql/sets/boundary_set_const_views.h"  // Full_set_view
#include "mysql/sets/boundary_set_meta.h"         // Is_boundary_set
#include "mysql/sets/common_predicates.h"  // operator==(Boundary_set,Boundary_set)

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Specialization of Complement_view for boundary sets, providing a
/// view over the complement of another boundary set.
///
/// This provides forward iterators.
///
/// @tparam Source_tp Type the source boundary set.
template <Is_boundary_set Source_tp>
class Complement_view<Source_tp>
    : public Subtraction_view<Full_set_view<Boundary_set_category_tag,
                                            typename Source_tp::Set_traits_t>,
                              Source_tp> {
 public:
  using Source_t = Source_tp;
  using Set_traits_t = typename Source_t::Set_traits_t;
  using Full_set_view_t =
      Full_set_view<Boundary_set_category_tag, Set_traits_t>;
  using Base_t = Subtraction_view<Full_set_view_t, Source_t>;

  Complement_view() = default;

  /// Construct the complement view over the given source.
  explicit Complement_view(const Source_t &source)
      : Base_t(make_full_set_view<Boundary_set_category_tag, Set_traits_t>(),
               source) {}

  // This can be faster than the implementation inherited from Subtraction_view.
  [[nodiscard]] bool empty() const {
    return source() ==
           make_full_set_view<Boundary_set_category_tag, Set_traits_t>();
  }

  /// Return the source boundary set, that this is the complement of.
  [[nodiscard]] const Source_t &source() const { return *this->source2(); }
};  // class Complement_view over boundary set

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_COMPLEMENT_VIEW_H

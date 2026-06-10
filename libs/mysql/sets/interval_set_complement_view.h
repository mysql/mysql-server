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

#ifndef MYSQL_SETS_INTERVAL_SET_COMPLEMENT_VIEW_H
#define MYSQL_SETS_INTERVAL_SET_COMPLEMENT_VIEW_H

/// @file
/// Experimental API header

#include "mysql/sets/base_complement_view.h"      // Complement_view
#include "mysql/sets/interval_set_interface.h"    // Interval_set_interface
#include "mysql/sets/interval_set_meta.h"         // Is_interval_set
#include "mysql/sets/optional_view_source_set.h"  // Optional_view_source_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Specialization of Complement_view for interval sets, providing a view over
/// the complement of another interval set.
///
/// This provides forward iterators.
///
/// @tparam Source_tp Type of interval set.
template <Is_interval_set Source_tp>
class Complement_view<Source_tp>
    : public Interval_set_interface<
          Complement_view<Source_tp>,
          Complement_view<typename Source_tp::Boundary_set_t>> {
 public:
  using Source_t = Source_tp;
  using Opt_source_t = Optional_view_source_set<Source_t>;
  using Set_traits_t = typename Source_t::Set_traits_t;
  using Source_boundary_set_t = typename Source_tp::Boundary_set_t;
  using Complement_boundary_set_t = Complement_view<Source_boundary_set_t>;

  /// Construct a new interval complement view over the given interval
  /// set.
  explicit Complement_view(const Source_t &source)
      : m_boundaries(source.boundaries()), m_source(source) {}

  /// Return the boundary set.
  [[nodiscard]] const Complement_boundary_set_t &boundaries() const {
    return m_boundaries;
  }

  /// Return reference to the source interval set that this is the complement
  /// of. The return type is a reference if the source is a container, and a
  /// value if it is a view.
  [[nodiscard]] decltype(auto) source() const { return m_source.get(); }

 private:
  /// Boundary complement view over the boundary set for the given
  /// interval set.
  const Complement_boundary_set_t m_boundaries;

  /// The source interval set, that this is the complement of.
  Opt_source_t m_source;
};  // class Complement_view over interval set

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_COMPLEMENT_VIEW_H

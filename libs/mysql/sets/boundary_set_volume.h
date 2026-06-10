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

#ifndef MYSQL_SETS_BOUNDARY_SET_VOLUME_H
#define MYSQL_SETS_BOUNDARY_SET_VOLUME_H

/// @file
/// Experimental API header

#include "mysql/sets/boundary_set_meta.h"       // Is_boundary_set
#include "mysql/sets/interval_set_interface.h"  // make_interval_set_view
#include "mysql/sets/interval_set_volume.h"     // volume
#include "mysql/sets/set_traits.h"              // Is_metric_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Return the sum of the lengths of all intervals in the given Boundary set.
template <Is_boundary_set Boundary_set_t>
  requires Is_metric_set_traits<typename Boundary_set_t::Set_traits_t>
[[nodiscard]] constexpr auto volume(const Boundary_set_t &set) {
  return volume(make_interval_set_view(set));
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_VOLUME_H

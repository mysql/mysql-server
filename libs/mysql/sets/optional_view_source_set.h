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

#ifndef MYSQL_SETS_OPTIONAL_VIEW_SOURCE_SET_H
#define MYSQL_SETS_OPTIONAL_VIEW_SOURCE_SET_H

/// @file
/// Experimental API header

#include <utility>                                 // forward
#include "mysql/meta/not_decayed.h"                // Not_decayed
#include "mysql/ranges/view_sources.h"             // Optional_view_source
#include "mysql/sets/set_categories_and_traits.h"  // Is_set

/// @addtogroup GroupLibsMysqlSets
/// @{
///
/// This file defines helper functions used by views whose source(s) are
/// optional. When an optional source is absent, it is treated as an empty set.

namespace mysql::sets {

/// Used to represent an optional source of a view, when that source is a set.
/// Provides the functions find, lower_bound, and upper_bound which work even if
/// the optional source is absent, in which case they return the end iterator.
template <Is_set Source_tp>
class Optional_view_source_set
    : public mysql::ranges::Optional_view_source<Source_tp> {
  using Base_t = mysql::ranges::Optional_view_source<Source_tp>;
  using This_t = Optional_view_source_set<Source_tp>;

 public:
  /// Delegate construction to Optional_view_source
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Optional_view_source_set(Args_t &&...args)
      : Base_t(std::forward<Args_t>(args)...) {}

  /// Default rule-of-5
  Optional_view_source_set(const Optional_view_source_set &) = default;
  Optional_view_source_set(Optional_view_source_set &&) = default;
  Optional_view_source_set &operator=(const Optional_view_source_set &) =
      default;
  Optional_view_source_set &operator=(Optional_view_source_set &&) = default;
  ~Optional_view_source_set() = default;

  /// If the source is present, invoke find on it. Otherwise, return an end
  /// iterator.
  template <class... Args_t>
  [[nodiscard]] auto find(Args_t &&...args) const {
    if (!this->has_object()) return Base_t::null_iterator();
    return (*this)->find(std::forward<Args_t>(args)...);
  }

  /// If the source is present, invoke lower_bound on it. Otherwise, return an
  /// end iterator.
  template <class... Args_t>
  [[nodiscard]] auto lower_bound(Args_t &&...args) const {
    if (!this->has_object()) return Base_t::null_iterator();
    return (*this)->lower_bound(std::forward<Args_t>(args)...);
  }

  /// If the source is present, invoke upper_bound on it. Otherwise, return an
  /// end iterator.
  template <class... Args_t>
  [[nodiscard]] auto upper_bound(Args_t &&...args) const {
    if (!this->has_object()) return Base_t::null_iterator();
    return (*this)->upper_bound(std::forward<Args_t>(args)...);
  }
};  // class Optional_view_source_set

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_OPTIONAL_VIEW_SOURCE_SET_H

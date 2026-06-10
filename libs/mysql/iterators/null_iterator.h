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

#ifndef MYSQL_ITERATORS_NULL_ITERATOR_H
#define MYSQL_ITERATORS_NULL_ITERATOR_H

/// @file
/// Experimental API header

#include <ranges>  // range

/// @addtogroup GroupLibsMysqlIterators
/// @{

namespace mysql::iterators {

/// Returns an iterator object for a given range type, created without passing a
/// range object, while guaranteeing that two such iterators for the same type
/// compare as equal.
///
/// The use case is a view which does not have a range: then the view can return
/// null iterators from its begin and end member functions, making it behave as
/// a view over an empty range.
///
/// Note that default-constructed iterators do not generally work in this case,
/// because comparison for default-constructed standard library iterators is
/// undefined behavior.
///
/// This is implemented by returning the end iterator for a (singleton)
/// default-constructed range object.
template <std::ranges::range Range_t>
[[nodiscard]] auto null_iterator() {
  static const Range_t default_instance{};
  return default_instance.end();
}

}  // namespace mysql::iterators

// addtogroup GroupLibsMysqlIterators
/// @}

#endif  // ifndef MYSQL_ITERATORS_NULL_ITERATOR_H

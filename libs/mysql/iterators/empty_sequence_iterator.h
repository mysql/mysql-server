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

#ifndef MYSQL_ITERATORS_EMPTY_SEQUENCE_ITERATOR_H
#define MYSQL_ITERATORS_EMPTY_SEQUENCE_ITERATOR_H

/// @file
/// Experimental API header

#include <cassert>                               // assert
#include <iterator>                              // random_access_iterator_tag
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface

/// @addtogroup GroupLibsMysqlIterators
/// @{

namespace mysql::iterators {

/// Iterator over an empty sequence.
///
/// @tparam Value_tp Type of values returned from the iterator. Although no
/// values are returned, this is required in order to define the return type for
/// the dereference iterator.
template <class Value_tp>
class Empty_sequence_iterator
    : public Iterator_interface<Empty_sequence_iterator<Value_tp>> {
 public:
  using Value_t = Value_tp;

  [[nodiscard]] constexpr Value_t get() const {
    Value_t *tmp{};  // default-constructed = null
    assert(false);
    return *tmp;
  }

  constexpr void advance(std::ptrdiff_t) {}

  [[nodiscard]] constexpr std::ptrdiff_t distance_from(
      const Empty_sequence_iterator &) const {
    return 0;
  }

  [[nodiscard]] constexpr std::ptrdiff_t distance_from_sentinel() const {
    return 0;
  }
};

}  // namespace mysql::iterators

// addtogroup GroupLibsMysqlIterators
/// @}

#endif  // ifndef MYSQL_ITERATORS_EMPTY_SEQUENCE_ITERATOR_H

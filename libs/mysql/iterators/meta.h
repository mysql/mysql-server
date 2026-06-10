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

#ifndef MYSQL_ITERATORS_META_H
#define MYSQL_ITERATORS_META_H

/// @file
/// Experimental API header

#include <iterator>  // iterator_traits

/// @addtogroup GroupLibsMysqlIterators
/// @{

namespace mysql::iterators {

// The type tag for the strongest of the standard iterator categories that
// Iterator_t satisfies.
template <class Iterator_t>
using Iterator_concept_tag = std::conditional_t<
    std::contiguous_iterator<Iterator_t>, std::contiguous_iterator_tag,
    std::conditional_t<
        std::random_access_iterator<Iterator_t>,
        std::random_access_iterator_tag,
        std::conditional_t<
            std::bidirectional_iterator<Iterator_t>,
            std::bidirectional_iterator_tag,
            std::conditional_t<
                std::forward_iterator<Iterator_t>, std::forward_iterator_tag,
                std::conditional_t<std::input_iterator<Iterator_t>,
                                   std::input_iterator_tag, void>>>>>;

/// True if the iterator is declared to meet LegacyInputIterator requirements.
///
/// Note that this does not check the requirements (not even syntactic ones): an
/// iterator that lies about its category will fool this function. However, an
/// iterator defined using Iterator_interface is guaranteed to satisfy the
/// syntactic requirements.
template <class Test>
concept Is_declared_legacy_input_iterator =
    std::derived_from<typename std::iterator_traits<Test>::iterator_category,
                      std::input_iterator_tag>;

/// True if the iterator is declared to meet LegacyForwardIterator requirements.
///
/// Note that this does not check the requirements (not even syntactic ones): an
/// iterator that lies about its category will fool this function. However, an
/// iterator defined using Iterator_interface is guaranteed to satisfy the
/// syntactic requirements.
template <class Test>
concept Is_declared_legacy_forward_iterator =
    std::derived_from<typename std::iterator_traits<Test>::iterator_category,
                      std::forward_iterator_tag>;

/// True if the iterator is declared to meet LegacyBidirectionalIterator
/// requirements.
///
/// Note that this does not check the requirements (not even syntactic ones): an
/// iterator that lies about its category will fool this function. However, an
/// iterator defined using Iterator_interface is guaranteed to satisfy the
/// syntactic requirements.
template <class Test>
concept Is_declared_legacy_bidirectional_iterator =
    std::derived_from<typename std::iterator_traits<Test>::iterator_category,
                      std::bidirectional_iterator_tag>;

/// True if the iterator is declared to meet LegacyRandomAccessIterator
/// requirements.
///
/// Note that this does not check the requirements (not even syntactic ones): an
/// iterator that lies about its category will fool this function. However, an
/// iterator defined using Iterator_interface is guaranteed to satisfy the
/// syntactic requirements.
template <class Test>
concept Is_declared_legacy_random_access_iterator =
    std::derived_from<typename std::iterator_traits<Test>::iterator_category,
                      std::random_access_iterator_tag>;

}  // namespace mysql::iterators

// addtogroup GroupLibsMysqlIterators
/// @}

#endif  // ifndef MYSQL_ITERATORS_META_H

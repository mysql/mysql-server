/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/containers.h
TempTable index containers declarations. */

#ifndef TEMPTABLE_CONTAINERS_H
#define TEMPTABLE_CONTAINERS_H

#include <set>           /* std::multiset */
#include <type_traits>   /* std::is_same */
#include <unordered_set> /* std::unordered_set, std::unordered_multiset */

#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */
#include "storage/temptable/include/temptable/indexed_cells.h" /* temptable::Indexed_cells */

namespace temptable {

/** The container used by tree unique and non-unique indexes. */
typedef std::multiset<Indexed_cells, Indexed_cells_less,
                      Allocator<Indexed_cells>>
    Tree_container;

/** The container used by hash non-unique indexes. */
typedef std::unordered_multiset<Indexed_cells, Indexed_cells_hash,
                                Indexed_cells_equal_to,
                                Allocator<Indexed_cells>>
    Hash_duplicates_container;

/** The container used by hash unique indexes. */
typedef std::unordered_set<Indexed_cells, Indexed_cells_hash,
                           Indexed_cells_equal_to, Allocator<Indexed_cells>>
    Hash_unique_container;

static_assert(
    std::is_same<Hash_duplicates_container::const_iterator,
                 Hash_unique_container::const_iterator>::value,
    "Duplicates and unique hash tables must have the same iterator type.");

} /* namespace temptable */

#endif /* TEMPTABLE_CONTAINERS_H */

/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/index.cc
TempTable Index implementation. */

#include <utility>

#include "my_base.h"
#include "sql/key.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/containers.h"
#include "storage/temptable/include/temptable/cursor.h"
#include "storage/temptable/include/temptable/index.h"
#include "storage/temptable/include/temptable/indexed_cells.h"
#include "storage/temptable/include/temptable/result.h"

namespace temptable {

Index::Index(const Table &table, const KEY &mysql_index)
    : m_number_of_indexed_columns(mysql_index.user_defined_key_parts),
      m_table(table),
      m_mysql_index(&mysql_index) {
  assert(m_number_of_indexed_columns <= m_indexed_columns.size());

  /* Re-construction of the array with proper initialization. */
  for (size_t i = 0; i < m_number_of_indexed_columns; ++i) {
    /* Call destructor as the default constructor was already called. */
    m_indexed_columns[i].~Indexed_column();
    /* Call constructor with arguments. */
    new (&m_indexed_columns[i]) Indexed_column{mysql_index.key_part[i]};
  }
}

Index::~Index() {
  assert(m_number_of_indexed_columns <= m_indexed_columns.size());

  /* No need to call m_indexed_columns destructors manually,
   * the std::array will do that. */
}

Tree::Tree(const Table &table, const KEY &mysql_index,
           const Allocator<Indexed_cells> &allocator)
    : Index(table, mysql_index),
      m_tree(Indexed_cells_less(*this), allocator),
      m_allow_duplicates(!(mysql_index.flags & HA_NOSAME)) {}

Result Tree::insert(const Indexed_cells &indexed_cells,
                    Cursor *insert_position) {
  bool ok_to_insert;

  if (m_allow_duplicates) {
    ok_to_insert = true;
  } else {
    /* Duplicates not allowed. See if `indexed_cells` is already in the tree. */
    auto pos = m_tree.lower_bound(indexed_cells);

    /* `pos` points to the first element (counting from smallest to
     * largest) that is greater or equal to `indexed_cells`. */

    if (pos == m_tree.end()) {
      /* All elements in the tree are smaller than `indexed_cells`. */
      ok_to_insert = true;
    } else {
      /* indexed_cells <= *pos */
      if (m_tree.key_comp()(indexed_cells, *pos)) {
        /* indexed_cells < *pos */
        ok_to_insert = true;
      } else {
        /* indexed_cells == *pos */
        ok_to_insert = false;
      }
    }
  }

  if (ok_to_insert) {
    Container::iterator it;

    try {
      it = m_tree.emplace(indexed_cells);
    } catch (Result ex) {
      return ex;
    }

    *insert_position = Cursor(it);

    return Result::OK;
  }

  return Result::FOUND_DUPP_KEY;
}

Index::Lookup Tree::lookup(const Indexed_cells &search_cells,
                           Cursor *first) const {
  return lookup(search_cells, first, nullptr);
}

Index::Lookup Tree::lookup(const Indexed_cells &search_cells, Cursor *first,
                           Cursor *after_last) const {
  Container::const_iterator tree_iterator = m_tree.lower_bound(search_cells);

  if (tree_iterator == m_tree.end()) {
    return Lookup::NOT_FOUND_CURSOR_UNDEFINED;
  }

  const Indexed_cells &cells_in_tree = *tree_iterator;

  *first = Cursor(tree_iterator);

  /* search_cells < cells_in_tree */
  if (m_tree.key_comp()(search_cells, cells_in_tree)) {
    if (after_last != nullptr) {
      *after_last = *first;
    }
    return Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT;
  }

  if (!m_allow_duplicates) {
    if (after_last != nullptr) {
      ++tree_iterator;
      *after_last = Cursor(tree_iterator);
    }
    return Lookup::FOUND;
  }

  if (after_last != nullptr) {
    tree_iterator = m_tree.upper_bound(search_cells);
    *after_last = Cursor(tree_iterator);
  }

  return Lookup::FOUND;
}

void Tree::erase(const Cursor &target) { m_tree.erase(target.tree_iterator()); }

void Tree::truncate() { m_tree.clear(); }

Cursor Tree::begin() const { return Cursor(m_tree.begin()); }

Cursor Tree::end() const { return Cursor(m_tree.end()); }

Hash_duplicates::Hash_duplicates(const Table &table, const KEY &mysql_index,
                                 const Allocator<Indexed_cells> &allocator)
    : Index(table, mysql_index),
      m_hash_table(INDEX_DEFAULT_HASH_TABLE_BUCKETS, Indexed_cells_hash(*this),
                   Indexed_cells_equal_to(*this), allocator) {}

Result Hash_duplicates::insert(const Indexed_cells &indexed_cells,
                               Cursor *insert_position) {
  Container::iterator it;

  try {
    it = m_hash_table.emplace(indexed_cells);
  } catch (Result ex) {
    return ex;
  }

  *insert_position = Cursor(it);

  return Result::OK;
}

Index::Lookup Hash_duplicates::lookup(const Indexed_cells &search_cells,
                                      Cursor *first) const {
  return lookup(search_cells, first, nullptr);
}

Index::Lookup Hash_duplicates::lookup(const Indexed_cells &search_cells,
                                      Cursor *first, Cursor *after_last) const {
  auto range = m_hash_table.equal_range(search_cells);

  if (range.first == m_hash_table.end()) {
    return Lookup::NOT_FOUND_CURSOR_UNDEFINED;
  }

  *first = Cursor(range.first);
  if (after_last != nullptr) {
    *after_last = Cursor(range.second);
  }

  return Lookup::FOUND;
}

void Hash_duplicates::erase(const Cursor &target) {
  m_hash_table.erase(target.hash_iterator());
}

void Hash_duplicates::truncate() { m_hash_table.clear(); }

Cursor Hash_duplicates::begin() const { return Cursor(m_hash_table.begin()); }

Cursor Hash_duplicates::end() const { return Cursor(m_hash_table.end()); }

Hash_unique::Hash_unique(const Table &table, const KEY &mysql_index,
                         const Allocator<Indexed_cells> &allocator)
    : Index(table, mysql_index),
      m_hash_table(INDEX_DEFAULT_HASH_TABLE_BUCKETS, Indexed_cells_hash(*this),
                   Indexed_cells_equal_to(*this), allocator) {}

Result Hash_unique::insert(const Indexed_cells &indexed_cells,
                           Cursor *insert_position) {
  std::pair<Container::iterator, bool> r;

  try {
    r = m_hash_table.emplace(indexed_cells);
  } catch (Result ex) {
    return ex;
  }

  auto &pos = r.first;
  const bool new_element_inserted = r.second;

  if (!new_element_inserted) {
    return Result::FOUND_DUPP_KEY;
  }

  *insert_position = Cursor(pos);

  return Result::OK;
}

Index::Lookup Hash_unique::lookup(const Indexed_cells &search_cells,
                                  Cursor *first) const {
  return lookup(search_cells, first, nullptr);
}

Index::Lookup Hash_unique::lookup(const Indexed_cells &search_cells,
                                  Cursor *first, Cursor *after_last) const {
  auto range = m_hash_table.equal_range(search_cells);

  if (range.first == m_hash_table.end()) {
    return Lookup::NOT_FOUND_CURSOR_UNDEFINED;
  }

  *first = Cursor(range.first);
  if (after_last != nullptr) {
    *after_last = Cursor(range.second);
  }

  return Lookup::FOUND;
}

void Hash_unique::erase(const Cursor &target) {
  m_hash_table.erase(target.hash_iterator());
}

void Hash_unique::truncate() { m_hash_table.clear(); }

Cursor Hash_unique::begin() const { return Cursor(m_hash_table.begin()); }

Cursor Hash_unique::end() const { return Cursor(m_hash_table.end()); }

} /* namespace temptable */

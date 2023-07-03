/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

/** @file storage/temptable/include/temptable/index.h
TempTable Index declarations. */

#ifndef TEMPTABLE_INDEX_H
#define TEMPTABLE_INDEX_H

#include <assert.h>

#include "sql/key.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/containers.h"
#include "storage/temptable/include/temptable/cursor.h"
#include "storage/temptable/include/temptable/indexed_cells.h"
#include "storage/temptable/include/temptable/indexed_column.h"
#include "storage/temptable/include/temptable/result.h"

namespace temptable {

class Table;

/** Index interface. */
class Index {
 public:
  /** Index lookup (search) result. */
  enum class Lookup {
    /** The searched for indexed cells were found and the cursor is
     * positioned
     * on them. */
    FOUND,

    /** The searched for indexed cells were not found and the cursor is
     * positioned on the next indexed cells in index order. */
    NOT_FOUND_CURSOR_POSITIONED_ON_NEXT,

    /** The searched for indexed cells were not found and the cursor
     * position is undefined. */
    NOT_FOUND_CURSOR_UNDEFINED,
  };

  /** Constructor. */
  Index(
      /** [in] Table where this index belongs, used only for fetching metadata
       * of its columns. */
      const Table &table,
      /** [in] MySQL index, for querying metadata. */
      const KEY &mysql_index);

  /** Destructor. */
  virtual ~Index();

  /** Insert a new entry into the index.
   * @return Result::OK or another Result::* error code */
  virtual Result insert(
      /** [in] Indexed cells to insert. */
      const Indexed_cells &indexed_cells,
      /** [out] If insert succeeds (Result::OK) this will be set to the position
       * inside the index where the new indexed cells where inserted. */
      Cursor *insert_position) = 0;

  /** Lookup (search) an indexed cells.
   * @retval Lookup::FOUND the provided `search_cells` were found and `first`
   * was positioned on them (the first entry, if there are duplicates).
   * @retval Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT the provided
   * `search_cells` were not found and `first` was positioned on the next
   * indexed cells in index order.
   * @retval Lookup::NOT_FOUND_CURSOR_UNDEFINED the provided `search_cells` were
   * not found and `first` is undefined. */
  virtual Lookup lookup(
      /** [in] Indexed cells to search for. */
      const Indexed_cells &search_cells,
      /** [out] First indexed cells that were found, see above. */
      Cursor *first) const = 0;

  /** Lookup (search) an indexed cells.
   * @retval Lookup::FOUND the provided `search_cells` were found, `first`
   * was positioned on them (the first entry, if there are duplicates) and
   * `after_last` was positioned after the last matching entry.
   * @retval Lookup::NOT_FOUND_CURSOR_POSITIONED_ON_NEXT the provided
   * `search_cells` were not found and `first` and `after_last` were
   * positioned on the next indexed cells in index order.
   * @retval Lookup::NOT_FOUND_CURSOR_UNDEFINED the provided `search_cells` were
   * not found and `first` and `after_last` are undefined. */
  virtual Lookup lookup(const Indexed_cells &search_cells, Cursor *first,
                        Cursor *after_last) const = 0;

  /** Erase the indexed cells pointer to by a cursor. */
  virtual void erase(
      /** [in] Position to erase. */
      const Cursor &target) = 0;

  /** Truncate the index, deleting all of its entries. */
  virtual void truncate() = 0;

  /** Get a cursor to the first entry.
   * @return cursor to the first indexed cells in index order */
  virtual Cursor begin() const = 0;

  /** Get a cursor after the last entry.
   * @return a cursor after the last indexed cells in index order */
  virtual Cursor end() const = 0;

  /** Get the number of indexed columns by this index.
   * @return number of indexed columns */
  size_t number_of_indexed_columns() const;

  /** Get the Nth indexed column.
   * @return the Nth indexed column */
  const Indexed_column &indexed_column(
      /** [in] Index of the column to fetch,
       * must be < `number_of_indexed_columns()`. */
      size_t i) const;

  /** Get the table of the index.
   * @return table */
  const Table &table() const;

  /** Get the MySQL index structure which corresponds to this index.
   * @return mysql index */
  const KEY &mysql_index() const;

 private:
  /** Number of indexed columns. */
  size_t m_number_of_indexed_columns;

  /** Table of the index. */
  const Table &m_table;

  /** Indexed columns metadata, from [0, m_number_of_indexed_columns). */
  std::array<Indexed_column, MAX_REF_PARTS> m_indexed_columns;

  /** MySQL index. */
  const KEY *m_mysql_index;
};

class Tree : public Index {
 public:
  typedef Tree_container Container;

  Tree(const Table &table, const KEY &mysql_index,
       const Allocator<Indexed_cells> &allocator);

  Result insert(const Indexed_cells &indexed_cells,
                Cursor *insert_position) override;

  Lookup lookup(const Indexed_cells &search_cells,
                Cursor *first) const override;

  Lookup lookup(const Indexed_cells &search_cells, Cursor *first,
                Cursor *after_last) const override;

  void erase(const Cursor &target) override;

  void truncate() override;

  Cursor begin() const override;

  Cursor end() const override;

  Container m_tree;
  bool m_allow_duplicates;
};

class Hash_duplicates : public Index {
 public:
  typedef Hash_duplicates_container Container;

  Hash_duplicates(const Table &table, const KEY &mysql_index,
                  const Allocator<Indexed_cells> &allocator);

  Result insert(const Indexed_cells &indexed_cells,
                Cursor *insert_position) override;

  Lookup lookup(const Indexed_cells &search_cells,
                Cursor *first) const override;

  Lookup lookup(const Indexed_cells &search_cells, Cursor *first,
                Cursor *after_last) const override;

  void erase(const Cursor &target) override;

  void truncate() override;

  Cursor begin() const override;

  Cursor end() const override;

  Container m_hash_table;
};

class Hash_unique : public Index {
 public:
  typedef Hash_unique_container Container;

  Hash_unique(const Table &table, const KEY &mysql_index,
              const Allocator<Indexed_cells> &allocator);

  Result insert(const Indexed_cells &indexed_cells,
                Cursor *insert_position) override;

  Lookup lookup(const Indexed_cells &search_cells,
                Cursor *first) const override;

  Lookup lookup(const Indexed_cells &search_cells, Cursor *first,
                Cursor *after_last) const override;

  void erase(const Cursor &target) override;

  void truncate() override;

  Cursor begin() const override;

  Cursor end() const override;

  Container m_hash_table;
};

/* Implementation of inlined methods. */

inline size_t Index::number_of_indexed_columns() const {
  return m_number_of_indexed_columns;
}

inline const Indexed_column &Index::indexed_column(size_t i) const {
  assert(i < m_number_of_indexed_columns);
  return m_indexed_columns[i];
}

inline const Table &Index::table() const { return m_table; }

inline const KEY &Index::mysql_index() const { return *m_mysql_index; }

} /* namespace temptable */

#endif /* TEMPTABLE_INDEX_H */

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

/** @file storage/temptable/include/temptable/cursor.h
TempTable index cursor. */

#ifndef TEMPTABLE_CURSOR_H
#define TEMPTABLE_CURSOR_H

#include <assert.h>

#include "storage/temptable/include/temptable/column.h"
#include "storage/temptable/include/temptable/containers.h"
#include "storage/temptable/include/temptable/indexed_cells.h"
#include "storage/temptable/include/temptable/row.h"
#include "storage/temptable/include/temptable/storage.h"

namespace temptable {

/** A cursor for iterating over an `Index`. */
class Cursor {
 public:
  /** Constructor. */
  Cursor();

  /** Constructor from `Hash_duplicates` iterator. */
  explicit Cursor(
      /** [in] Iterator for cursor initial position. */
      const Hash_duplicates_container::const_iterator &iterator);

  /** Constructor from `Tree` iterator. */
  explicit Cursor(
      /** [in] Iterator for cursor initial position. */
      const Tree_container::const_iterator &iterator);

  Cursor(const Cursor &) = default;
  Cursor(Cursor &&) noexcept = default;

  /** Check if the cursor is positioned.
   * @return true if positioned */
  bool is_positioned() const;

  /** Unposition the cursor. */
  void unposition();

  /** Get the indexed cells of the current cursor position.
   * @return indexed cells */
  const Indexed_cells &indexed_cells() const;

  /** Get a pointer to the row of the current cursor position.
   * @return a pointer to a row */
  Storage::Element *row() const;

  /** Export the row that is pointed to by this cursor in mysql write_row()
   * format. */
  void export_row_to_mysql(
      /** [in] Metadata for the columns that constitute the row. */
      const Columns &columns,
      /** [out] Destination buffer to write the row to. */
      unsigned char *mysql_row,
      /** [in] Presumed length of the mysql row in bytes. */
      size_t mysql_row_length) const;

  /** Get the underlying hash iterator. The cursor must be on a hash index.
   * @return iterator */
  const Hash_duplicates_container::const_iterator &hash_iterator() const;

  /** Get the underlying tree iterator. The cursor must be on a tree index.
   * @return iterator */
  const Tree_container::const_iterator &tree_iterator() const;

  /** Copy-assign from another cursor.
   * @return *this */
  Cursor &operator=(
      /** [in] Source cursor to assign from. */
      const Cursor &rhs);

  /** Advance the cursor forward.
   * @return *this */
  Cursor &operator++();

  /** Recede the cursor backwards.
   * @return *this */
  Cursor &operator--();

  /** Check if equal to another cursor.
   * @return true if equal */
  bool operator==(
      /** [in] Cursor to compare with. */
      const Cursor &other) const;

  /** Check if not equal to another cursor.
   * @return true if not equal */
  bool operator!=(
      /** [in] Cursor to compare with. */
      const Cursor &other) const;

 private:
  /** Type of the index the cursor iterates over. */
  enum class Type : uint8_t {
    /** Hash index. */
    HASH,
    /** Tree index. */
    TREE,
  };

  /** Type of the index the cursor iterates over. */
  Type m_type;

  /** Indicate whether the cursor is positioned. */
  bool m_is_positioned;

  /** Iterator that is used if `m_type == Type::HASH`. */
  Hash_duplicates_container::const_iterator m_hash_iterator;

  /** Iterator that is used if `m_type == Type::TREE`. */
  Tree_container::const_iterator m_tree_iterator;
};

/* Implementation of inlined methods. */

inline Cursor::Cursor() : m_is_positioned(false) {}

inline Cursor::Cursor(const Hash_duplicates_container::const_iterator &iterator)
    : m_type(Type::HASH), m_is_positioned(true), m_hash_iterator(iterator) {}

inline Cursor::Cursor(const Tree_container::const_iterator &iterator)
    : m_type(Type::TREE), m_is_positioned(true), m_tree_iterator(iterator) {}

inline bool Cursor::is_positioned() const { return m_is_positioned; }

inline void Cursor::unposition() { m_is_positioned = false; }

inline const Indexed_cells &Cursor::indexed_cells() const {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    return *m_hash_iterator;
  }

  assert(m_type == Type::TREE);
  return *m_tree_iterator;
}

inline Storage::Element *Cursor::row() const {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    return m_hash_iterator->row();
  }

  assert(m_type == Type::TREE);
  return m_tree_iterator->row();
}

inline void Cursor::export_row_to_mysql(const Columns &columns,
                                        unsigned char *mysql_row,
                                        size_t mysql_row_length) const {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    return m_hash_iterator->export_row_to_mysql(columns, mysql_row,
                                                mysql_row_length);
  }

  assert(m_type == Type::TREE);
  return m_tree_iterator->export_row_to_mysql(columns, mysql_row,
                                              mysql_row_length);
}

inline const Hash_duplicates_container::const_iterator &Cursor::hash_iterator()
    const {
  assert(m_type == Type::HASH);
  return m_hash_iterator;
}

inline const Tree_container::const_iterator &Cursor::tree_iterator() const {
  assert(m_type == Type::TREE);
  return m_tree_iterator;
}

inline Cursor &Cursor::operator=(const Cursor &rhs) {
  m_is_positioned = rhs.m_is_positioned;

  m_type = rhs.m_type;

  if (m_is_positioned) {
    if (m_type == Type::HASH) {
      m_hash_iterator = rhs.m_hash_iterator;
    } else {
      assert(m_type == Type::TREE);
      m_tree_iterator = rhs.m_tree_iterator;
    }
  }

  return *this;
}

inline Cursor &Cursor::operator++() {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    ++m_hash_iterator;
  } else {
    assert(m_type == Type::TREE);
    ++m_tree_iterator;
  }

  return *this;
}

inline Cursor &Cursor::operator--() {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    /* We don't support decrement on a hash and it shouldn't be called. */
    my_abort();
  } else {
    assert(m_type == Type::TREE);
    --m_tree_iterator;
  }

  return *this;
}

inline bool Cursor::operator==(const Cursor &other) const {
  assert(m_is_positioned);

  if (m_type == Type::HASH) {
    return m_hash_iterator == other.m_hash_iterator;
  }

  assert(m_type == Type::TREE);
  return m_tree_iterator == other.m_tree_iterator;
}

inline bool Cursor::operator!=(const Cursor &other) const {
  return !(*this == other);
}

} /* namespace temptable */

#endif /* TEMPTABLE_CURSOR_H */

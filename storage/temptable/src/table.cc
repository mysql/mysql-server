/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file storage/temptable/src/table.cc
TempTable Table implementation. */

#include <cstddef>       /* size_t */
#include <functional>    /* std::hash, std::equal_to */
#include <new>           /* new */
#include <string>        /* std::string */
#include <unordered_map> /* std::unordered_map */
#include <utility>       /* std::pair */
#include <vector>        /* std::vector */

#include "my_base.h"   /* HA_KEY_ALG_* */
#include "my_dbug.h"   /* DBUG_ASSERT(), DBUG_ABORT() */
#include "sql/field.h" /* Field */
#include "sql/key.h"   /* KEY */
#include "sql/table.h" /* TABLE, TABLE_SHARE */
#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */
#include "storage/temptable/include/temptable/cursor.h" /* temptable::Cursor */
#include "storage/temptable/include/temptable/index.h"  /* temptable::Index */
#include "storage/temptable/include/temptable/indexed_cells.h" /* temptable::Indexed_cells */
#include "storage/temptable/include/temptable/result.h" /* temptable::Result */
#include "storage/temptable/include/temptable/row.h"    /* temptable::Row */
#include "storage/temptable/include/temptable/table.h"  /* temptable::Table */

namespace temptable {

Table::Table(TABLE *mysql_table, bool all_columns_are_fixed_size)
    : m_rows(&m_allocator),
      m_all_columns_are_fixed_size(all_columns_are_fixed_size),
      m_indexes_are_enabled(true),
      m_mysql_row_length(mysql_table->s->rec_buff_length),
      m_index_entries(m_allocator),
      m_insert_undo(m_allocator),
      m_columns(m_allocator),
      m_mysql_table_share(mysql_table->s) {
  const size_t number_of_indexes = mysql_table->s->keys;
  const size_t number_of_columns = mysql_table->s->fields;

  m_columns.reserve(number_of_columns);
  for (size_t i = 0; i < number_of_columns; ++i) {
    m_columns.emplace_back(*mysql_table, *mysql_table->field[i]);
  }

  if (m_all_columns_are_fixed_size) {
    m_rows.element_size(m_mysql_row_length);
  } else {
    m_rows.element_size(sizeof(Row));
  }

  indexes_create();

  m_insert_undo.reserve(number_of_indexes);
}

Table::~Table() {
  indexes_destroy();

  if (!m_all_columns_are_fixed_size) {
    for (auto element : m_rows) {
      Row *row = static_cast<Row *>(element);
      row->~Row();
    }
  }
}

Result Table::insert(const unsigned char *mysql_row) {
  Storage::Element *row;

  try {
    row = m_rows.allocate_back();
  } catch (Result ex) {
    return ex;
  }

  Result ret;

  if (m_all_columns_are_fixed_size) {
    DBUG_ASSERT(m_rows.element_size() == m_mysql_table_share->rec_buff_length);
    DBUG_ASSERT(m_rows.element_size() == m_mysql_row_length);

    memcpy(row, mysql_row, m_mysql_row_length);
  } else {
    DBUG_ASSERT(m_rows.element_size() == sizeof(Row));

    new (row) Row(mysql_row, &m_allocator);

    ret = static_cast<Row *>(row)->copy_to_own_memory(m_columns,
                                                      m_mysql_row_length);

    if (ret != Result::OK) {
      m_rows.deallocate_back();
      return ret;
    }
  }

  DBUG_ASSERT(m_insert_undo.empty());

  if (!m_indexes_are_enabled) {
    return Result::OK;
  }

  ret = Result::OK;

  for (auto &entry : m_index_entries) {
    Index *index = entry.m_index;
    Cursor insert_position;

    Indexed_cells indexed_cells =
        m_all_columns_are_fixed_size
            ? Indexed_cells{static_cast<unsigned char *>(row), *index}
            : Indexed_cells{*static_cast<Row *>(row), *index};

    ret = index->insert(indexed_cells, &insert_position);

    if (ret != Result::OK) {
      break;
    }

    /* Only bother with postponing undo operations if we have more than one
     * index. If we are here and have just one index, then we know that the
     * operation succeeded and this loop is not going to iterate anymore. */
    if (m_index_entries.size() > 1) {
      m_insert_undo.emplace_back(insert_position);
    }
  }

  if (ret != Result::OK) {
    /* Undo the above insertions. */
    for (size_t i = 0; i < m_insert_undo.size(); ++i) {
      Index *index = m_index_entries[i].m_index;
      const Cursor &target = m_insert_undo[i];
      index->erase(target);
    }
    if (!m_all_columns_are_fixed_size) {
      static_cast<Row *>(row)->~Row();
    }
    m_rows.deallocate_back();
  }

  m_insert_undo.clear();

  return ret;
}

Result Table::update(const unsigned char *mysql_row_old,
                     const unsigned char *mysql_row_new,
                     Storage::Element *target_row, bool reversal,
                     size_t max_index) {
#ifndef DBUG_OFF
  if (m_all_columns_are_fixed_size) {
    DBUG_ASSERT(m_rows.element_size() == m_mysql_row_length);
  } else {
    DBUG_ASSERT(m_rows.element_size() == sizeof(Row));
    Row *row_in_m_rows = reinterpret_cast<Row *>(target_row);
    const Row row_old(mysql_row_old, nullptr);
    DBUG_ASSERT(Row::compare(*row_in_m_rows, row_old, m_columns,
                             m_mysql_table_share->field) == 0);
  }
#endif /* DBUG_OFF */

  Result ret;

  /* We update `target_row` to `mysql_row_new` inplace in `m_rows` and in each
   * index by delete & insert. */

  /* Update inplace in the list of rows. */
  if (m_all_columns_are_fixed_size) {
    memcpy(target_row, mysql_row_new, m_mysql_row_length);
  } else {
    Row *row = reinterpret_cast<Row *>(target_row);

    *row = Row(mysql_row_new, &m_allocator);

    ret = row->copy_to_own_memory(m_columns, m_mysql_row_length);

    if (ret != Result::OK) {
      *row = Row(mysql_row_old, &m_allocator);
      return ret;
    }
  }

  if (!m_indexes_are_enabled || m_index_entries.empty()) {
    return Result::OK;
  }

  /* Update in each index:
   * - find the index entry (Indexed_cells) that points to target_row
   * - delete that index entry
   * - insert new Indexed_cells, created from mysql_row_new. */

  if (!reversal) {
    max_index = m_index_entries.size() - 1;
  }

  ret = Result::OK;

  size_t i;
  for (i = 0; i <= max_index; ++i) {
    Index &index = *m_index_entries[i].m_index;

    const Indexed_cells indexed_cells_old(mysql_row_old, index);
    const Indexed_cells indexed_cells_new(mysql_row_new, index);

    if (Indexed_cells_equal_to(index)(indexed_cells_old, indexed_cells_new)) {
      DBUG_ASSERT(Indexed_cells_hash(index)(indexed_cells_old) ==
                  Indexed_cells_hash(index)(indexed_cells_new));
      /* No need to update this index because its columns are not affected by
       * the update. If the cell is case insensitive and the update is changing
       * it from 'a' to 'A' for example, then we will enter here and not update
       * the index, which is fine. The index only contains a pointer to the row
       * inside Table::m_rows and as long as the comparisons (== < >) and hash
       * return the same results (and they do for 'a' and 'A') there is no need
       * to delete & reinsert into the index. */
      continue;
    }

    /* Find the index entry (Indexed_cells) to erase. */

    /* We don't erase from the last index in case of reversal (because the
     * insertion in the forward operation failed, so there is nothing to
     * reverse). */
    if (!(reversal && i == max_index)) {
      Cursor first;
      Cursor after_last;

      if (index.lookup(indexed_cells_old, &first, &after_last) !=
          Index::Lookup::FOUND) {
        ret = Result::TABLE_CORRUPT;
        break;
      }

      Cursor c = first;
      bool found = false;

      for (; c != after_last; ++c) {
        if (c.row() == target_row) {
          found = true;
          break;
        }
      }

      if (!found) {
        ret = Result::TABLE_CORRUPT;
        break;
      }

      index.erase(c);
    }

    /* Insert the new indexed cells into the index. */

    const Indexed_cells indexed_cells =
        m_all_columns_are_fixed_size
            ? Indexed_cells{reinterpret_cast<unsigned char *>(target_row),
                            index}
            : Indexed_cells{*reinterpret_cast<Row *>(target_row), index};
    Cursor inserted_pos;

    ret = index.insert(indexed_cells, &inserted_pos);

    if (ret != Result::OK) {
      break;
    }
  }

  if (ret == Result::OK) {
    return Result::OK;
  }

  if (reversal) {
    ret = Result::TABLE_CORRUPT;
  } else if (update(mysql_row_new, mysql_row_old, target_row, true, i) !=
             Result::OK) {
    /* If we cannot successfully revert a partically complete update (in order
     * to restore the table to its state as it were before update() was called),
     * then we declare the table corrupted. */
    ret = Result::TABLE_CORRUPT;
  }

  return ret;
}

Result Table::remove(const unsigned char *mysql_row_must_be,
                     const Storage::Iterator &victim_position) {
  Row row(mysql_row_must_be, &m_allocator);

#ifndef DBUG_OFF
  /* Check that `mysql_row_must_be` equals the row pointed to by
   * `victim_position`. */
  if (m_all_columns_are_fixed_size) {
  } else {
    /* *victim_position is a pointer to an `temptable::Row` object. */
    Row *row_our = reinterpret_cast<Row *>(*victim_position);
    DBUG_ASSERT(Row::compare(*row_our, row, m_columns,
                             m_mysql_table_share->field) == 0);
  }
#endif /* DBUG_OFF */

  if (m_indexes_are_enabled) {
    const auto index_count = m_index_entries.size();
    for (size_t i = 0; i < index_count; ++i) {
      Index *index = m_index_entries[i].m_index;
      const Indexed_cells cells(row, *index);
      Cursor first;
      Cursor after_last;

      if (index->lookup(cells, &first, &after_last) != Index::Lookup::FOUND) {
        return i == 0 ? Result::KEY_NOT_FOUND : Result::TABLE_CORRUPT;
      }

      /* We have one or more matching entries in this index. We have to delete
       * the one that points `*victim_position` inside m_rows. */

      bool found_in_m_rows = false;

      for (Cursor c = first; c != after_last; ++c) {
        if (c.row() == *victim_position) {
          found_in_m_rows = true;
          index->erase(c);
          break;
        }
      }

      if (!found_in_m_rows) {
        return Result::TABLE_CORRUPT;
      }
    }
  }

  if (!m_all_columns_are_fixed_size) {
    Row *row = reinterpret_cast<Row *>(*victim_position);
    row->~Row();
  }

  m_rows.erase(victim_position);

  return Result::OK;
}

void Table::indexes_create() {
  DBUG_ASSERT(m_index_entries.empty());

  const size_t number_of_indexes = m_mysql_table_share->keys;

  m_index_entries.reserve(number_of_indexes);

  for (size_t i = 0; i < number_of_indexes; ++i) {
    const KEY &mysql_index = m_mysql_table_share->key_info[i];

    switch (mysql_index.algorithm) {
      case HA_KEY_ALG_BTREE:
        append_new_index<Tree>(mysql_index);
        break;
      case HA_KEY_ALG_HASH:
        if (mysql_index.flags & HA_NOSAME) {
          append_new_index<Hash_unique>(mysql_index);
        } else {
          append_new_index<Hash_duplicates>(mysql_index);
        }
        break;
      case HA_KEY_ALG_SE_SPECIFIC:
      case HA_KEY_ALG_RTREE:
      case HA_KEY_ALG_FULLTEXT:
        DBUG_ABORT();
    }
  }
}

void Table::indexes_destroy() {
  for (auto &entry : m_index_entries) {
    Index *index = entry.m_index;

    index->~Index();

    m_allocator.deallocate(reinterpret_cast<uint8_t *>(index),
                           entry.m_alloc_size);
  }

  m_index_entries.clear();
}

thread_local Tables tables;

} /* namespace temptable */

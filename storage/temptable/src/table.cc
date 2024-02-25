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

/** @file storage/temptable/src/table.cc
TempTable Table implementation. */

#include <cstddef>
#include <functional>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "my_base.h"
#include "my_dbug.h"
#include "sql/field.h"
#include "sql/key.h"
#include "sql/table.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/cursor.h"
#include "storage/temptable/include/temptable/index.h"
#include "storage/temptable/include/temptable/indexed_cells.h"
#include "storage/temptable/include/temptable/result.h"
#include "storage/temptable/include/temptable/row.h"
#include "storage/temptable/include/temptable/table.h"

namespace temptable {

Table::Table(TABLE *mysql_table, Block *shared_block,
             bool all_columns_are_fixed_size, size_t tmp_table_size_limit)
    : m_resource_monitor(tmp_table_size_limit),
      m_allocator(shared_block, m_resource_monitor),
      m_rows(&m_allocator),
      m_all_columns_are_fixed_size(all_columns_are_fixed_size),
      m_indexes_are_enabled(true),
      m_mysql_row_length(mysql_table->s->rec_buff_length),
      /* We use `explicit vector(size_type count, const Allocator& alloc)`
       * constructor as the one we would like to use, the
       * `explicit vector(const Allocator& alloc) noexcept` is noexcept, while
       * the MS VC++ with non-zero ITERATOR_DEBUG_LEVEL macro value will perform
       * an allocation using the supplied allocator and cause std::terminate to
       * be called in case the exception is thrown, as it is not allowed with
       * `noexcept`. There is a related bug reported to VC++:
       * https://developercommunity.visualstudio.com/t/debug-version-of-stl-is-not-excepion-safe-and-caus/77779
       */
      m_index_entries(0, m_allocator),
      m_insert_undo(m_allocator),
      m_columns(m_allocator),
      m_mysql_table_share(mysql_table->s) {
  const size_t number_of_indexes = mysql_table->s->keys;
  const size_t number_of_columns = mysql_table->s->fields;
  const unsigned char *mysql_row = nullptr;

  if (number_of_columns > 0) {
    auto const field_ptr = mysql_table->field[0]->field_ptr();
    auto const mysql_row_length = mysql_table->s->rec_buff_length;

    mysql_row = mysql_table->record[0];
    if ((field_ptr >= mysql_row) &&
        (field_ptr < (mysql_row + mysql_row_length))) {
      /* field_ptr is inside record[0]. */
    } else {
      mysql_row = mysql_table->record[1];
      if ((field_ptr >= mysql_row) &&
          (field_ptr < (mysql_row + mysql_row_length))) {
        /* field_ptr is inside record[1]. */
      } else {
        /* ptr does not point inside neither record[0] nor record[1]. */
        my_abort();
      }
    }
  }

  m_columns.reserve(number_of_columns);
  for (size_t i = 0; i < number_of_columns; ++i) {
    m_columns.emplace_back(mysql_row, *mysql_table, *mysql_table->field[i]);
  }

  if (m_all_columns_are_fixed_size) {
    m_rows.element_size(m_mysql_row_length);
    assert(m_rows.number_of_elements_per_page() > 0);
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
    assert(m_rows.element_size() == m_mysql_table_share->rec_buff_length);
    assert(m_rows.element_size() == m_mysql_row_length);

    memcpy(row, mysql_row, m_mysql_row_length);
  } else {
    assert(m_rows.element_size() == sizeof(Row));

    new (row) Row(mysql_row, &m_allocator);

    ret = static_cast<Row *>(row)->copy_to_own_memory(m_columns,
                                                      m_mysql_row_length);

    if (ret != Result::OK) {
      static_cast<Row *>(row)->~Row();
      m_rows.deallocate_back();
      return ret;
    }
  }

  if (!indexed()) {
    return Result::OK;
  }

  ret = indexes_insert(row);

  if (ret != Result::OK) {
    if (!m_all_columns_are_fixed_size) {
      static_cast<Row *>(row)->~Row();
    }
    m_rows.deallocate_back();
  }

  return ret;
}

Result Table::update(const unsigned char *mysql_row_old,
                     const unsigned char *mysql_row_new,
                     Storage::Element *target_row) {
#ifndef NDEBUG
  if (m_all_columns_are_fixed_size) {
    assert(m_rows.element_size() == m_mysql_row_length);
  } else {
    assert(m_rows.element_size() == sizeof(Row));
    Row *row_in_m_rows = reinterpret_cast<Row *>(target_row);
    const Row row_old(mysql_row_old, nullptr);
    assert(Row::compare(*row_in_m_rows, row_old, m_columns,
                        m_mysql_table_share->field) == 0);
  }
#endif /* NDEBUG */

  /* Index update is unsupported.
  See bug #27978968: TEMPTABLE::TABLE::UPDATE MAY CORRUPT THE TABLE
  for a commit that removed the support (as at that moment it was
  a dead code). */
  if (indexed()) {
    if (is_index_update_needed(mysql_row_old, mysql_row_new)) {
      /* Assert to make it easier to catch potential problem during tests */
      assert(false);
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "update of indexes");
      return Result::UNSUPPORTED;
    }
  }

  /* For rows that does not have fixed size the contents are swapped
   * between this row and the target_row. That guaranteed the buffer
   * memory will be released at the end of function (if needed).
   *
   * The old contents must not be lost (deallocated) until update is done
   * as e.g. the pointers to BLOBs data may still be used in row data
   * and also we may need to restore the original contents.*/
  Row tmp_row(mysql_row_new, &m_allocator);

  if (!m_all_columns_are_fixed_size) {
    Result ret = tmp_row.copy_to_own_memory(m_columns, m_mysql_row_length);
    if (ret != Result::OK) {
      return ret;
    }
  }

  /* Update the target row contents. */
  if (m_all_columns_are_fixed_size) {
    memcpy(target_row, mysql_row_new, m_mysql_row_length);
  } else {
    Row *row = reinterpret_cast<Row *>(target_row);
    Row::swap(*row, tmp_row);
  }

  return Result::OK;
}

Result Table::remove(const unsigned char *mysql_row_must_be,
                     const Storage::Iterator &victim_position) {
  Row row(mysql_row_must_be, &m_allocator);

#ifndef NDEBUG
  /* Check that `mysql_row_must_be` equals the row pointed to by
   * `victim_position`. */
  if (m_all_columns_are_fixed_size) {
    /* nop */
  } else {
    /* *victim_position is a pointer to an `temptable::Row` object. */
    Row *row_our = reinterpret_cast<Row *>(*victim_position);
    assert(Row::compare(*row_our, row, m_columns, m_mysql_table_share->field) ==
           0);
  }
#endif /* NDEBUG */

  if (indexed()) {
    Result ret = indexes_remove(*victim_position);
    if (ret != Result::OK) {
      return ret;
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
  assert(m_index_entries.empty());

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

bool Table::is_index_update_needed(const unsigned char *mysql_row_old,
                                   const unsigned char *mysql_row_new) const {
  for (auto &entry : m_index_entries) {
    Index *index = entry.m_index;

    const Indexed_cells indexed_cells_old(mysql_row_old, *index);
    const Indexed_cells indexed_cells_new(mysql_row_new, *index);

    if (!Indexed_cells_equal_to(*index)(indexed_cells_old, indexed_cells_new)) {
      return true;
    }
  }
  return false;
}

Result Table::indexes_insert(Storage::Element *row) {
  Result ret = Result::OK;

  assert(m_insert_undo.empty());

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
  }

  m_insert_undo.clear();

  return ret;
}

Result Table::indexes_remove(Storage::Element *row) {
  Result result = Result::OK;

  for (auto &entry : m_index_entries) {
    Index *index = entry.m_index;

    Indexed_cells cells =
        m_all_columns_are_fixed_size
            ? Indexed_cells{static_cast<unsigned char *>(row), *index}
            : Indexed_cells{*static_cast<Row *>(row), *index};

    Cursor first;
    Cursor after_last;

    if (index->lookup(cells, &first, &after_last) != Index::Lookup::FOUND) {
      result = Result::TABLE_CORRUPT;
      break;
    }

    /* We have one or more matching entries in this index. We have to delete
     * the one that points `*victim_position` inside m_rows. */

    bool found = false;

    for (Cursor c = first; c != after_last; ++c) {
      if (c.row() == row) {
        found = true;
        index->erase(c);
        break;
      }
    }

    if (!found) {
      result = Result::TABLE_CORRUPT;
      break;
    }
  }

  return result;
}

} /* namespace temptable */

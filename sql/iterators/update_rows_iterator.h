#ifndef SQL_ITERATORS_UPDATE_ROWS_ITERATOR_H_
#define SQL_ITERATORS_UPDATE_ROWS_ITERATOR_H_

/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cassert>
#include <memory>

#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "sql/iterators/row_iterator.h"
#include "sql/sql_list.h"

class COPY_INFO;
class Copy_field;
class Item;
class THD;
struct TABLE;
class Table_ref;
template <class T>
class mem_root_deque;

/// An iterator that performs updates to rows returned by its child iterator.
class UpdateRowsIterator final : public RowIterator {
 public:
  UpdateRowsIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                     TABLE *outermost_table, TABLE *immediate_table,
                     Table_ref *update_tables, TABLE **tmp_tables,
                     Copy_field *copy_fields,
                     List<TABLE> unupdated_check_opt_tables,
                     COPY_INFO **update_operations,
                     mem_root_deque<Item *> **fields_for_table,
                     mem_root_deque<Item *> **values_for_table,
                     table_map tables_with_rowid_in_buffer);
  ~UpdateRowsIterator() override;
  bool Init() override;
  int Read() override;
  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void SetNullRowFlag([[maybe_unused]] bool is_null_row) override {
    assert(false);
  }
  void UnlockRow() override { assert(false); }
  ha_rows found_rows() const { return m_found_rows; }
  ha_rows updated_rows() const { return m_updated_rows; }

 private:
  /// The iterator producing the rows to update.
  unique_ptr_destroy_only<RowIterator> m_source;
  /// The outermost table of the join. It may or may not be one of the tables
  /// being updated.
  TABLE *m_outermost_table;
  /// The table to perform immediate update on, or nullptr if immediate update
  /// is not possible.
  TABLE *m_immediate_table;
  /// Pointer to list of updated tables, linked via 'next_local'.
  Table_ref *m_update_tables;
  /// Temporary tables used to store cached updates.
  TABLE **m_tmp_tables;
  /// Objects that copy the updated values from a temporary table to the update
  /// target table, and perform conversions if the types differ.
  Copy_field *m_copy_fields;
  /// Tables referenced in the CHECK OPTION condition of the updated view
  /// excluding the updated table.
  List<TABLE> m_unupdated_check_opt_tables;
  /// The update operations of each table in m_update_tables (indexed in the
  /// same order as m_update_tables).
  COPY_INFO **m_update_operations;
  /// The fields list decomposed into separate lists per table.
  mem_root_deque<Item *> **m_fields_for_table;
  /// The values list decomposed into separate lists per table.
  mem_root_deque<Item *> **m_values_for_table;
  /// The number of rows matching the WHERE and join conditions.
  ha_rows m_found_rows{0};
  /// The number of rows actually updated.
  ha_rows m_updated_rows{0};
  /// All the tables that are part of a hash join. We use this map to find out
  /// how to get the row ID from a table when buffering row IDs for delayed
  /// update. For those tables that are part of a hash join, the row ID will
  /// already be available in handler::ref, and calling handler::position() will
  /// overwrite it with an incorrect row ID (most likely the last row read from
  /// the table). For those that are not part of a hash join,
  /// handler::position() must be called to get the current row ID from the
  /// underlying scan.
  table_map m_hash_join_tables;

  /// Perform all the immediate updates for the current row returned by the
  /// join, and buffer row IDs for the non-immediate tables.
  ///
  /// @param[out] trans_safe Gets set to false if a non-transactional table
  ///                        is updated.
  /// @param[out] transactional_tables Gets set to true if a transactional
  ///                                  table is updated.
  /// @return True on error.
  bool DoImmediateUpdatesAndBufferRowIds(bool *trans_safe,
                                         bool *transactional_tables);

  /// Perform all the delayed updates.
  ///
  /// @param[in,out] trans_safe Gets set to false if a non-transactional table
  ///                           is updated.
  /// @param[out] transactional_tables Gets set to true if a transactional
  ///                                  table is updated.
  /// @return True on error.
  bool DoDelayedUpdates(bool *trans_safe, bool *transactional_tables);
};

#endif  // SQL_ITERATORS_UPDATE_ROWS_ITERATOR_H_

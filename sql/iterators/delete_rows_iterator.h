#ifndef SQL_ITERATORS_DELETE_ROWS_ITERATOR_H_
#define SQL_ITERATORS_DELETE_ROWS_ITERATOR_H_

/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>

#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "sql/iterators/row_iterator.h"
#include "sql/mem_root_array.h"
#include "sql/uniques.h"

class JOIN;
class THD;
struct TABLE;

/// An iterator that deletes all rows returned by its child iterator.
class DeleteRowsIterator final : public RowIterator {
 public:
  DeleteRowsIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                     JOIN *join, table_map tables_to_delete_from,
                     table_map immediate_tables);
  bool Init() override;
  int Read() override;
  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void SetNullRowFlag(bool /*is_null_row*/) override { assert(false); }
  void UnlockRow() override { assert(false); }

 private:
  /// The iterator producing the rows to delete.
  unique_ptr_destroy_only<RowIterator> m_source;
  /// The join producing the rows to delete.
  JOIN *m_join;
  /// The tables to delete from.
  table_map m_tables_to_delete_from;
  /// The tables to delete from immediately while scanning the join result.
  table_map m_immediate_tables;
  /// All the tables that are part of a hash join. We use this map to find out
  /// how to get the row ID from a table when buffering row IDs for delayed
  /// delete. For those tables that are part of a hash join, the row ID will
  /// already be available in handler::ref, and calling handler::position() will
  /// overwrite it with an incorrect row ID (most likely the last row read from
  /// the table). For those that are not part of a hash join,
  /// handler::position() must be called to get the current row ID from the
  /// underlying scan.
  table_map m_hash_join_tables;
  /// The target tables that live in transactional storage engines.
  table_map m_transactional_tables{0};
  /// The target tables that have before delete triggers.
  table_map m_tables_with_before_triggers{0};
  /// The target tables that have after delete triggers.
  table_map m_tables_with_after_triggers{0};
  /// Temporary files holding row IDs to delete after the scan of
  /// the join result is complete.
  Mem_root_array<unique_ptr_destroy_only<Unique>> m_tempfiles;
  /// The tables to delete from after the scan of the join result is
  /// complete.
  Mem_root_array<TABLE *> m_delayed_tables;
  /// The number of rows that have been deleted.
  ha_rows m_deleted_rows{0};
  /// True if any row ID has been stored in one of the m_tempfiles.
  bool m_has_delayed_deletes{false};

  /// Perform all the immediate deletes for the current row returned by the
  /// join, and buffer row IDs for the non-immediate tables.
  bool DoImmediateDeletesAndBufferRowIds();
  /// Perform all the delayed deletes.
  bool DoDelayedDeletes();
  /// Perform all the delayed deletes for the given table.
  bool DoDelayedDeletesFromTable(TABLE *table);
};

/// Sets various flags in the TABLE and handler objects associated with the
/// target tables of a DELETE statement, in order to make them ready to be
/// deleted from.
///
/// @param thd   The session object.
/// @param join  The top-level JOIN object of the DELETE operation.
void SetUpTablesForDelete(THD *thd, JOIN *join);

#endif  // SQL_ITERATORS_DELETE_ROWS_ITERATOR_H_

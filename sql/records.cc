/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Functions for easy reading of records, possible through a cache
*/

#include "sql/records.h"

#include <string.h>
#include <algorithm>
#include <atomic>
#include <new>

#include "my_base.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/psi_memory_key.h"
#include "sql/sort_param.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_sort.h"
#include "sql/table.h"

/**
  Initialize READ_RECORD structure to perform full index scan in desired
  direction using the RowIterator interface

  This function has been added at late stage and is used only by
  UPDATE/DELETE. Other statements perform index scans using IndexScanIterator.

  @param info         READ_RECORD structure to initialize.
  @param thd          Thread handle
  @param table        Table to be accessed
  @param idx          index to scan
  @param reverse      Scan in the reverse direction
  @param qep_tab      If not NULL, used for record buffer and pushed condition

  @retval true   error
  @retval false  success
*/

void setup_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table, uint idx,
                           bool reverse, QEP_TAB *qep_tab) {
  empty_record(table);
  new (info) READ_RECORD;

  ha_rows *examined_rows = nullptr;
  if (qep_tab != nullptr && qep_tab->join() != nullptr) {
    examined_rows = &qep_tab->join()->examined_rows;
  }

  unique_ptr_destroy_only<RowIterator> iterator;

  if (reverse) {
    iterator.reset(
        new (&info->iterator_holder.index_scan_reverse) IndexScanIterator<true>(
            thd, table, idx,
            /*use_order=*/true, qep_tab,
            qep_tab ? qep_tab->condition() : nullptr, examined_rows));
  } else {
    iterator.reset(
        new (&info->iterator_holder.index_scan) IndexScanIterator<false>(
            thd, table, idx,
            /*use_order=*/true, qep_tab,
            qep_tab ? qep_tab->condition() : nullptr, examined_rows));
  }
  info->iterator = std::move(iterator);
}

template <bool Reverse>
IndexScanIterator<Reverse>::IndexScanIterator(THD *thd, TABLE *table, int idx,
                                              bool use_order, QEP_TAB *qep_tab,
                                              Item *pushed_condition,
                                              ha_rows *examined_rows)
    : RowIterator(thd, table),
      m_record(table->record[0]),
      m_idx(idx),
      m_use_order(use_order),
      m_qep_tab(qep_tab),
      m_pushed_condition(pushed_condition),
      m_examined_rows(examined_rows) {}

template <bool Reverse>
IndexScanIterator<Reverse>::~IndexScanIterator() {
  if (table() && table()->key_read) {
    table()->set_keyread(false);
  }
}

template <bool Reverse>
bool IndexScanIterator<Reverse>::Init() {
  if (!table()->file->inited) {
    if (table()->covering_keys.is_set(m_idx) && !table()->no_keyread) {
      table()->set_keyread(true);
    }

    int error = table()->file->ha_index_init(m_idx, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }

    if (set_record_buffer(m_qep_tab)) {
      return true;
    }
  }
  PushDownCondition(m_pushed_condition);
  m_first = true;
  return false;
}

// Doxygen gets confused by the explicit specializations.

//! @cond
template <>
int IndexScanIterator<false>::Read() {  // Forward read.
  int error;
  if (m_first) {
    error = table()->file->ha_index_first(m_record);
    m_first = false;
  } else {
    error = table()->file->ha_index_next(m_record);
  }
  if (error) return HandleError(error);
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

template <>
int IndexScanIterator<true>::Read() {  // Backward read.
  int error;
  if (m_first) {
    error = table()->file->ha_index_last(m_record);
    m_first = false;
  } else {
    error = table()->file->ha_index_prev(m_record);
  }
  if (error) return HandleError(error);
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}
//! @endcond

template class IndexScanIterator<true>;
template class IndexScanIterator<false>;

/**
  setup_read_record is used to scan by using a number of different methods.
  Which method to use is set-up in this call so that you can fetch rows
  through the resulting row iterator afterwards.

  @param info     OUT read structure
  @param thd      Thread handle
  @param table    Table the data [originally] comes from; if NULL,
    'table' is inferred from 'qep_tab'; if non-NULL, 'qep_tab' must be NULL.
  @param qep_tab  QEP_TAB for 'table', if there is one; we may use
    qep_tab->quick() as data source
  @param disable_rr_cache
    Don't use caching in SortBufferIndirectIterator (used by sort-union
    index-merge which produces rowid sequences that are already ordered)
  @param ignore_not_found_rows
    Ignore any rows not found in reference tables, as they may already have
    been deleted by foreign key handling. Only relevant for methods that need
    to look up rows in tables (those marked “Indirect”).
  @param examined_rows
    If not-nullptr, the iterator will increase this variable by the number of
    examined rows. If nullptr, will use qep_tab->join()->examined_rows
    if possible.
 */
void setup_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                       QEP_TAB *qep_tab, bool disable_rr_cache,
                       bool ignore_not_found_rows, ha_rows *examined_rows) {
  // If only 'table' is given, assume no quick, no condition.
  DBUG_ASSERT(!(table && qep_tab));
  if (!table) table = qep_tab->table();
  empty_record(table);

  new (info) READ_RECORD;

  if (examined_rows == nullptr && qep_tab != nullptr &&
      qep_tab->join() != nullptr) {
    examined_rows = &qep_tab->join()->examined_rows;
  }

  unique_ptr_destroy_only<RowIterator> iterator;

  QUICK_SELECT_I *quick = qep_tab ? qep_tab->quick() : NULL;
  if (table->unique_result.io_cache &&
      my_b_inited(table->unique_result.io_cache)) {
    DBUG_PRINT("info", ("using SortFileIndirectIterator"));
    iterator.reset(
        new (&info->iterator_holder.sort_file_indirect)
            SortFileIndirectIterator(thd, table, table->unique_result.io_cache,
                                     !disable_rr_cache, ignore_not_found_rows,
                                     qep_tab ? qep_tab->condition() : nullptr,
                                     examined_rows));
    table->unique_result.io_cache =
        nullptr;  // Now owned by SortFileIndirectIterator.
  } else if (quick) {
    DBUG_PRINT("info", ("using IndexRangeScanIterator"));
    iterator.reset(
        new (&info->iterator_holder.index_range_scan) IndexRangeScanIterator(
            thd, table, quick, qep_tab,
            qep_tab ? qep_tab->condition() : nullptr, examined_rows));
  } else if (table->unique_result.has_result_in_memory()) {
    /*
      The Unique class never puts its results into table->sort's
      Filesort_buffer.
    */
    DBUG_ASSERT(!table->unique_result.sorted_result_in_fsbuf);
    DBUG_PRINT("info", ("using SortBufferIndirectIterator (unique)"));
    iterator.reset(
        new (&info->iterator_holder.sort_buffer_indirect)
            SortBufferIndirectIterator(
                thd, table, &table->unique_result, ignore_not_found_rows,
                qep_tab ? qep_tab->condition() : nullptr, examined_rows));
  } else {
    DBUG_PRINT("info", ("using TableScanIterator"));
    iterator.reset(new (&info->iterator_holder.table_scan) TableScanIterator(
        thd, table, qep_tab, qep_tab ? qep_tab->condition() : nullptr,
        examined_rows));
  }
  info->iterator = std::move(iterator);
}

bool init_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                      QEP_TAB *qep_tab, bool disable_rr_cache,
                      bool ignore_not_found_rows) {
  setup_read_record(info, thd, table, qep_tab, disable_rr_cache,
                    ignore_not_found_rows, /*examined_rows=*/nullptr);
  if (info->iterator->Init()) {
    info->iterator.reset();
    return true;
  }
  return false;
}

/**
  The default implementation of unlock-row method of READ_RECORD,
  used in all access methods except EQRefIterator.
*/
void RowIterator::UnlockRow() { m_table->file->unlock_row(); }

int RowIterator::HandleError(int error) {
  if (thd()->killed) {
    m_thd->send_kill_message();
    return 1;
  }

  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
    m_table->set_no_row();
    return -1;
  } else {
    PrintError(error);
    return 1;
  }
}

void RowIterator::PrintError(int error) {
  m_table->file->print_error(error, MYF(0));
}

/*
  Do condition pushdown for UPDATE/DELETE.
  TODO: Remove this from here as it causes two condition pushdown calls
  when we're running a SELECT and the condition cannot be pushed down.
  Some temporary tables do not have a TABLE_LIST object, and it is never
  needed to push down conditions (ECP) for such tables.
*/
void RowIterator::PushDownCondition(Item *condition) {
  if (m_thd->optimizer_switch_flag(
          OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
      condition && m_table->pos_in_table_list &&
      (condition->used_tables() & m_table->pos_in_table_list->map()) &&
      !m_table->file->pushed_cond) {
    m_table->file->cond_push(condition);
  }
}

IndexRangeScanIterator::IndexRangeScanIterator(THD *thd, TABLE *table,
                                               QUICK_SELECT_I *quick,
                                               QEP_TAB *qep_tab,
                                               Item *pushed_condition,
                                               ha_rows *examined_rows)
    : RowIterator(thd, table),
      m_quick(quick),
      m_qep_tab(qep_tab),
      m_pushed_condition(pushed_condition),
      m_examined_rows(examined_rows) {}

bool IndexRangeScanIterator::Init() {
  PushDownCondition(m_pushed_condition);

  /*
    Only attempt to allocate a record buffer the first time the handler is
    initialized.
  */
  const bool first_init = !table()->file->inited;

  int error = m_quick->reset();
  if (error) {
    // Ensures error status is propagated back to client.
    (void)report_handler_error(table(), error);
    return true;
  }

  if (first_init && table()->file->inited && set_record_buffer(m_qep_tab))
    return 1; /* purecov: inspected */

  return false;
}

int IndexRangeScanIterator::Read() {
  int tmp;
  while ((tmp = m_quick->get_next())) {
    if (thd()->killed || (tmp != HA_ERR_RECORD_DELETED)) {
      return HandleError(tmp);
    }
  }

  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

TableScanIterator::TableScanIterator(THD *thd, TABLE *table, QEP_TAB *qep_tab,
                                     Item *pushed_condition,
                                     ha_rows *examined_rows)
    : RowIterator(thd, table),
      m_record(table->record[0]),
      m_qep_tab(qep_tab),
      m_pushed_condition(pushed_condition),
      m_examined_rows(examined_rows) {}

TableScanIterator::~TableScanIterator() {
  table()->file->ha_index_or_rnd_end();
}

bool TableScanIterator::Init() {
  /*
    Only attempt to allocate a record buffer the first time the handler is
    initialized.
  */
  const bool first_init = !table()->file->inited;

  int error = table()->file->ha_rnd_init(1);
  if (error) {
    PrintError(error);
    return true;
  }

  if (first_init && set_record_buffer(m_qep_tab))
    return true; /* purecov: inspected */

  PushDownCondition(m_pushed_condition);

  return false;
}

int TableScanIterator::Read() {
  int tmp;
  while ((tmp = table()->file->ha_rnd_next(m_record))) {
    /*
      ha_rnd_next can return RECORD_DELETED for MyISAM when one thread is
      reading and another deleting without locks.
    */
    if (tmp == HA_ERR_RECORD_DELETED && !thd()->killed) continue;
    return HandleError(tmp);
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ft_global.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/ref_row_iterators.h"
#include "sql/iterators/row_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/key.h"  // key_cmp
#include "sql/key_spec.h"
#include "sql/mysqld.h"     // stage_executing
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_join_buffer.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/visible_fields.h"
#include "template_utils.h"

using std::make_pair;
using std::pair;

static inline pair<uchar *, key_part_map> FindKeyBufferAndMap(
    const Index_lookup *ref);

ConstIterator::ConstIterator(THD *thd, TABLE *table, Index_lookup *table_ref,
                             ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(table_ref),
      m_examined_rows(examined_rows) {}

bool ConstIterator::Init() {
  m_first_record_since_init = true;
  return false;
}

/**
  Read a constant table when there is at most one matching row, using an
  index lookup.

  @retval 0  Row was found
  @retval -1 Row was not found
  @retval 1  Got an error (other than row not found) during read
*/

int ConstIterator::Read() {
  if (!m_first_record_since_init) {
    return -1;
  }
  m_first_record_since_init = false;
  int err = read_const(table(), m_ref);
  if (err == 0 && m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  table()->const_table = true;
  return err;
}

EQRefIterator::EQRefIterator(THD *thd, TABLE *table, Index_lookup *ref,
                             ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_examined_rows(examined_rows) {}

/**
  Read row using unique key: eq_ref access method implementation

  @details
    This is the "read_first" function for the eq_ref access method.
    The difference from ref access function is that it has a one-element
    lookup cache, maintained in record[0]. Since the eq_ref access method
    will always return the same row, it is not necessary to read the row
    more than once, regardless of how many times it is needed in execution.
    This cache element is used when a row is needed after it has been read once,
    unless a key conversion error has occurred, or the cache has been disabled.

  @retval  0 - Ok
  @retval -1 - Row not found
  @retval  1 - Error
*/

bool EQRefIterator::Init() {
  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_ref->key, /*sorted=*/false);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  m_first_record_since_init = true;

  return false;
}

/**
  Read row using unique key: eq_ref access method implementation

  @details
    The difference from RefIterator is that it has a one-element
    lookup cache, maintained in record[0]. Since the eq_ref access method
    will always return the same row, it is not necessary to read the row
    more than once, regardless of how many times it is needed in execution.
    This cache element is used when a row is needed after it has been read once,
    unless a key conversion error has occurred, or the cache has been disabled.

  @retval  0 - Ok
  @retval -1 - Row not found
  @retval  1 - Error
*/

int EQRefIterator::Read() {
  if (!m_first_record_since_init) {
    return -1;
  }
  m_first_record_since_init = false;

  /*
    Calculate if needed to read row. Always needed if
    - no rows read yet, or
    - table has a pushed condition, or
    - cache is disabled, or
    - previous lookup caused error when calculating key.
  */
  bool read_row = !table()->is_started() || table()->file->pushed_cond ||
                  m_ref->disable_cache || m_ref->key_err;
  if (!read_row)
    // Last lookup found a row, copy its key to secondary buffer
    memcpy(m_ref->key_buff2, m_ref->key_buff, m_ref->key_length);

  // Create new key for lookup
  m_ref->key_err = construct_lookup(thd(), table(), m_ref);
  if (m_ref->key_err) {
    table()->set_no_row();
    return -1;
  }

  // Re-use current row if keys are equal
  if (!read_row &&
      memcmp(m_ref->key_buff2, m_ref->key_buff, m_ref->key_length) != 0)
    read_row = true;

  if (read_row) {
    /*
       Moving away from the current record. Unlock the row
       in the handler if it did not match the partial WHERE.
     */
    if (table()->has_row() && m_ref->use_count == 0)
      table()->file->unlock_row();

    /*
      Perform "Late NULLs Filtering" (see internals manual for explanations)

      As EQRefIterator effectively implements a one row cache of last
      fetched row, the NULLs filtering can't be done until after the cache
      key has been checked and updated, and row locks maintained.
    */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("EQRefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }

    pair<uchar *, key_part_map> key_buff_and_map = FindKeyBufferAndMap(m_ref);
    int error = table()->file->ha_index_read_map(
        table()->record[0], key_buff_and_map.first, key_buff_and_map.second,
        HA_READ_KEY_EXACT);
    if (error) {
      return HandleError(error);
    }

    m_ref->use_count = 1;
    table()->save_null_flags();
  } else if (table()->has_row()) {
    assert(!table()->has_null_row());
    table()->restore_null_flags();
    m_ref->use_count++;
  }

  if (table()->has_row() && m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return table()->has_row() ? 0 : -1;
}

/**
  Since EQRefIterator may buffer a record, do not unlock
  it if it was not used in this invocation of EQRefIterator::Read().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  Index_lookup buffer.
  @sa EQRefIterator::Read()
*/

void EQRefIterator::UnlockRow() {
  assert(m_ref->use_count);
  if (m_ref->use_count) m_ref->use_count--;
}

PushedJoinRefIterator::PushedJoinRefIterator(THD *thd, TABLE *table,
                                             Index_lookup *ref, bool use_order,
                                             bool is_unique,
                                             ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_is_unique(is_unique),
      m_examined_rows(examined_rows) {}

bool PushedJoinRefIterator::Init() {
  assert(!m_use_order);  // Pushed child can't be sorted

  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_ref->key, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  m_first_record_since_init = true;
  return false;
}

int PushedJoinRefIterator::Read() {
  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
    if (m_ref->impossible_null_ref()) {
      table()->set_no_row();
      DBUG_PRINT("info", ("PushedJoinRefIterator::Read() null_rejected"));
      return -1;
    }

    if (construct_lookup(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }

    // 'read' itself is a NOOP:
    //  handler::ha_index_read_pushed() only unpack the prefetched row and
    //  set 'status'
    int error = table()->file->ha_index_read_pushed(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts));
    if (error) {
      return HandleError(error);
    }
  } else if (not m_is_unique) {
    int error = table()->file->ha_index_next_pushed(table()->record[0]);
    if (error) {
      return HandleError(error);
    }
  } else {
    // 'm_is_unique' can at most return a single row, which we had
    table()->set_no_row();
    return -1;
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

/**
  Initialize an index scan.

  @param table   the table to read
  @param file    the handler to initialize
  @param idx     the index to use
  @param sorted  use the sorted order of the index
  @retval true   if an error occurred
  @retval false  on success
*/
static bool init_index(TABLE *table, handler *file, uint idx, bool sorted) {
  int error = file->ha_index_init(idx, sorted);
  if (error != 0) {
    (void)report_handler_error(table, error);
    return true;
  }

  return false;
}

template <bool Reverse>
bool RefIterator<Reverse>::Init() {
  m_first_record_since_init = true;
  m_is_mvi_unique_filter_enabled = false;
  if (table()->file->inited) return false;
  if (init_index(table(), table()->file, m_ref->key, m_use_order)) {
    return true;
  }
  // Enable & reset unique record filter for multi-valued index
  if (table()->key_info[m_ref->key].flags & HA_MULTI_VALUED_KEY) {
    table()->file->ha_extra(HA_EXTRA_ENABLE_UNIQUE_RECORD_FILTER);
    table()->prepare_for_position();
    m_is_mvi_unique_filter_enabled = true;
  }
  return set_record_buffer(table(), m_expected_rows);
}

// Doxygen gets confused by the explicit specializations.

//! @cond
template <>
int RefIterator<false>::Read() {  // Forward read.
  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /*
      a = b can never return true if a or b is NULL, so if we're asked
      to do such a lookup, we can say there won't be a match without even
      checking the index. This is “late NULLs filtering” (as opposed to
      “early NULLs filtering”, which propagates the IS NOT NULL constraint
      further back to the other table so we don't even get the request).
      See the internals manual for more details.
     */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("RefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }
    if (construct_lookup(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }

    pair<uchar *, key_part_map> key_buff_and_map = FindKeyBufferAndMap(m_ref);
    int error = table()->file->ha_index_read_map(
        table()->record[0], key_buff_and_map.first, key_buff_and_map.second,
        HA_READ_KEY_EXACT);
    if (error) {
      return HandleError(error);
    }
  } else {
    int error = 0;
    // Fetch unique rows matching the Ref Key in case of multi-value index
    do {
      error = table()->file->ha_index_next_same(
          table()->record[0], m_ref->key_buff, m_ref->key_length);
    } while (error == HA_ERR_KEY_NOT_FOUND && m_is_mvi_unique_filter_enabled);
    if (error) {
      return HandleError(error);
    }
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

/**
  This function is used when optimizing away ORDER BY in
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/
template <>
int RefIterator<true>::Read() {  // Reverse read.
  assert(m_ref->keypart_hash == nullptr);

  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /*
      a = b can never return true if a or b is NULL, so if we're asked
      to do such a lookup, we can say there won't be a match without even
      checking the index. This is “late NULLs filtering” (as opposed to
      “early NULLs filtering”, which propagates the IS NOT NULL constraint
      further back to the other table so we don't even get the request).
      See the internals manual for more details.
     */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("RefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }
    if (construct_lookup(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }
    int error = table()->file->ha_index_read_last_map(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts));
    if (error) {
      return HandleError(error);
    }
  } else {
    /*
      Using ha_index_prev() for reading records from the table can cause
      performance issues if used in combination with ICP. The ICP code
      in the storage engine does not know when to stop reading from the
      index and a call to ha_index_prev() might cause the storage engine
      to read to the beginning of the index if no qualifying record is
      found.
     */
    assert(table()->file->pushed_idx_cond == nullptr);
    int error = table()->file->ha_index_prev(table()->record[0]);
    if (error) {
      return HandleError(error);
    }
    if (key_cmp_if_same(table(), m_ref->key_buff, m_ref->key,
                        m_ref->key_length)) {
      table()->set_no_row();
      return -1;
    }
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

template <bool Reverse>
RefIterator<Reverse>::~RefIterator() {
  if (table()->key_info[m_ref->key].flags & HA_MULTI_VALUED_KEY &&
      table()->file) {
    table()->file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);
  }
}

template class RefIterator<true>;
template class RefIterator<false>;
//! @endcond

DynamicRangeIterator::DynamicRangeIterator(THD *thd, TABLE *table,
                                           QEP_TAB *qep_tab,
                                           ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_qep_tab(qep_tab),
      m_mem_root(key_memory_test_quick_select_exec,
                 thd->variables.range_alloc_block_size),
      m_examined_rows(examined_rows),
      m_read_set_without_base_columns(table->read_set) {
  add_virtual_gcol_base_cols(table, thd->mem_root,
                             &m_read_set_with_base_columns);
}

DynamicRangeIterator::~DynamicRangeIterator() {
  // This is owned by our MEM_ROOT.
  destroy(m_qep_tab->range_scan());
  m_qep_tab->set_range_scan(nullptr);
}

bool DynamicRangeIterator::Init() {
  Opt_trace_context *const trace = &thd()->opt_trace;
  const bool disable_trace =
      m_quick_traced_before &&
      !trace->feature_enabled(Opt_trace_context::DYNAMIC_RANGE);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);

  m_quick_traced_before = true;

  Opt_trace_object wrapper(trace);
  Opt_trace_object trace_table(trace, "rows_estimation_per_outer_row");
  trace_table.add_utf8_table(m_qep_tab->table_ref);

  Key_map needed_reg_dummy;
  // In execution, range estimation is done for each row,
  // so we can access previous tables.
  table_map const_tables = m_qep_tab->join()->found_const_table_map;
  table_map read_tables =
      m_qep_tab->prefix_tables() & ~m_qep_tab->added_tables();
  DEBUG_SYNC(thd(), "quick_not_created");

  /*
    EXPLAIN CONNECTION is used to understand why a query is currently taking
    so much time. So it makes sense to show what the execution is doing now:
    is it a table scan or a range scan? A range scan on which index.
    So: below we want to change the type and quick visible in EXPLAIN, and for
    that, we need to take mutex and change type and quick_optim.
  */

  DEBUG_SYNC(thd(), "quick_created_before_mutex");

  // We're about to destroy the MEM_ROOT containing the old quick, below.
  // But we cannot run test_quick_select() under the plan lock, since it might
  // want to evaluate a subquery that in itself has a DynamicRangeIterator(),
  // and the plan lock is not recursive. So we set a different plan temporarily
  // while we are calculating the new one, so that EXPLAIN FOR CONNECTION
  // does not read bad data.
  thd()->lock_query_plan();
  m_qep_tab->set_type(JT_UNKNOWN);
  thd()->unlock_query_plan();

  unique_ptr_destroy_only<RowIterator> qck;

  // Clear out and destroy any old iterators before we start constructing
  // new ones, since they may share the same memory in the union.
  m_iterator.reset();
  m_qep_tab->set_range_scan(nullptr);
  m_mem_root.ClearForReuse();

  AccessPath *range_scan;

  int rc = test_quick_select(thd(), &m_mem_root, &m_mem_root, m_qep_tab->keys(),
                             const_tables, read_tables, HA_POS_ERROR,
                             false,  // don't force quick range
                             ORDER_NOT_RELEVANT, m_qep_tab->table(),
                             m_qep_tab->skip_records_in_range(),
                             m_qep_tab->condition(), &needed_reg_dummy,
                             m_qep_tab->table()->force_index,
                             m_qep_tab->join()->query_block, &range_scan);
  if (thd()->is_error())  // @todo consolidate error reporting
                          // of test_quick_select
  {
    return true;
  }
  m_qep_tab->set_range_scan(range_scan);
  if (range_scan == nullptr) {
    m_qep_tab->set_type(JT_ALL);
  } else {
    qck = CreateIteratorFromAccessPath(thd(), &m_mem_root, range_scan,
                                       /*join=*/nullptr,
                                       /*eligible_for_batch_mode=*/false);
    if (qck == nullptr || thd()->is_error()) {
      return true;
    }
    m_qep_tab->set_type(calc_join_type(range_scan));
  }

  DEBUG_SYNC(thd(), "quick_droped_after_mutex");

  if (rc == -1) {
    return false;
  }

  // Create the required Iterator based on the strategy chosen. Also set the
  // read set to be used while accessing the table. Unlike a regular range
  // scan, as the access strategy keeps changing for a dynamic range scan,
  // optimizer cannot know if the read set should include base columns of
  // virtually generated columns or not. As a result, this Iterator maintains
  // two different read sets, to be used once the access strategy is chosen
  // here.
  if (qck) {
    m_iterator = std::move(qck);
    // If the range optimizer chose index merge scan or a range scan with
    // covering index, use the read set without base columns. Otherwise we use
    // the read set with base columns included.
    if (used_index(range_scan) == MAX_KEY ||
        table()->covering_keys.is_set(used_index(range_scan)))
      table()->read_set = m_read_set_without_base_columns;
    else
      table()->read_set = &m_read_set_with_base_columns;
  } else {
    m_iterator = NewIterator<TableScanIterator>(
        thd(), &m_mem_root, table(), m_qep_tab->position()->rows_fetched,
        m_examined_rows);
    // For a table scan, include base columns in read set.
    table()->read_set = &m_read_set_with_base_columns;
  }
  return m_iterator->Init();
}

int DynamicRangeIterator::Read() {
  if (m_iterator == nullptr) {
    return -1;
  } else {
    return m_iterator->Read();
  }
}

FullTextSearchIterator::FullTextSearchIterator(THD *thd, TABLE *table,
                                               Index_lookup *ref,
                                               Item_func_match *ft_func,
                                               bool use_order, bool use_limit,
                                               ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_ft_func(ft_func),
      m_use_order(use_order),
      m_use_limit(use_limit),
      m_examined_rows(examined_rows) {
  // Mark the full-text search function as used for index scan, if using the
  // hypergraph optimizer. The old optimizer uses heuristics to determine if a
  // full-text index scan should be used, and can set this flag the moment it
  // decides it should use an index scan. The hypergraph optimizer, on the other
  // hand, maintains alternative plans with and without index scans throughout
  // the planning, and doesn't determine whether it should use the indexed or
  // non-indexed plan until the full query plan has been constructed.
  if (thd->lex->using_hypergraph_optimizer) {
    // Should not already be enabled.
    assert(!ft_func->score_from_index_scan);
    // Should operate on the main object.
    assert(ft_func->get_master() == ft_func);

    // Mark the MATCH function as a source for a full-text index scan.
    ft_func->score_from_index_scan = true;

    if (table->covering_keys.is_set(ft_func->key) && !table->no_keyread) {
      // The index is covering. Tell the storage engine that it can do an
      // index-only scan.
      table->set_keyread(true);
    }

    // Enable ordering of the results on relevance, if requested.
    if (use_order) {
      ft_func->get_hints()->set_hint_flag(FT_SORTED);
    }

    // Propagate the limit to the storage engine, if requested.
    if (use_limit) {
      ft_func->get_hints()->set_hint_limit(
          ft_func->table_ref->query_block->join->m_select_limit);
    }
  }

  assert(ft_func->score_from_index_scan);
}

FullTextSearchIterator::~FullTextSearchIterator() {
  table()->file->ha_index_or_rnd_end();
  if (table()->key_read) {
    table()->set_keyread(false);
  }
}

bool FullTextSearchIterator::Init() {
  assert(m_ft_func->ft_handler != nullptr);
  assert(table()->file->ft_handler == m_ft_func->ft_handler);

  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_ref->key, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  // Mark the full-text function as reading from an index scan, and initialize
  // the full-text index scan.
  m_ft_func->score_from_index_scan = true;
  if (int error = table()->file->ft_init(); error != 0) {
    PrintError(error);
    return true;
  }

  return false;
}

int FullTextSearchIterator::Read() {
  int error = table()->file->ha_ft_read(table()->record[0]);
  if (error) {
    return HandleError(error);
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

/**
  Reading of key with key reference and one part that may be NULL.
*/

RefOrNullIterator::RefOrNullIterator(THD *thd, TABLE *table, Index_lookup *ref,
                                     bool use_order, double expected_rows,
                                     ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_expected_rows(expected_rows),
      m_examined_rows(examined_rows) {}

bool RefOrNullIterator::Init() {
  m_reading_first_row = true;
  m_is_mvi_unique_filter_enabled = false;
  *m_ref->null_ref_key = false;
  if (table()->file->inited) return false;
  if (init_index(table(), table()->file, m_ref->key, m_use_order)) {
    return true;
  }
  // Enable & reset unique record filter for multi-valued index
  if (table()->key_info[m_ref->key].flags & HA_MULTI_VALUED_KEY) {
    table()->file->ha_extra(HA_EXTRA_ENABLE_UNIQUE_RECORD_FILTER);
    table()->prepare_for_position();
    m_is_mvi_unique_filter_enabled = true;
  }
  return set_record_buffer(table(), m_expected_rows);
}

int RefOrNullIterator::Read() {
  if (m_reading_first_row && !*m_ref->null_ref_key) {
    /* Perform "Late NULLs Filtering" (see internals manual for explanations)
     */
    if (m_ref->impossible_null_ref() ||
        construct_lookup(thd(), table(), m_ref)) {
      // Skip searching for non-NULL rows; go straight to NULL rows.
      *m_ref->null_ref_key = true;
    }
  }

  pair<uchar *, key_part_map> key_buff_and_map = FindKeyBufferAndMap(m_ref);

  int error;
  if (m_reading_first_row) {
    m_reading_first_row = false;
    error = table()->file->ha_index_read_map(
        table()->record[0], key_buff_and_map.first, key_buff_and_map.second,
        HA_READ_KEY_EXACT);
  } else {
    // Fetch unique rows matching the Ref Key in case of multi-value index
    do {
      error = table()->file->ha_index_next_same(
          table()->record[0], key_buff_and_map.first, m_ref->key_length);
    } while (error == HA_ERR_KEY_NOT_FOUND && m_is_mvi_unique_filter_enabled);
  }

  if (error == 0) {
    if (m_examined_rows != nullptr) {
      ++*m_examined_rows;
    }
    return 0;
  } else if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
    if (!*m_ref->null_ref_key) {
      // No more non-NULL rows; try again with NULL rows.
      *m_ref->null_ref_key = true;
      m_reading_first_row = true;
      return Read();
    } else {
      // Real EOF.
      table()->set_no_row();
      return -1;
    }
  } else {
    return HandleError(error);
  }
}

RefOrNullIterator::~RefOrNullIterator() {
  if (table()->key_info[m_ref->key].flags & HA_MULTI_VALUED_KEY &&
      table()->file) {
    table()->file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);
  }
}

AlternativeIterator::AlternativeIterator(
    THD *thd, TABLE *table, unique_ptr_destroy_only<RowIterator> source,
    unique_ptr_destroy_only<RowIterator> table_scan_iterator, Index_lookup *ref)
    : RowIterator(thd),
      m_source_iterator(std::move(source)),
      m_table_scan_iterator(std::move(table_scan_iterator)),
      m_table(table),
      m_original_read_set(table->read_set) {
  for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
       ++key_part_idx) {
    bool *cond_guard = ref->cond_guards[key_part_idx];
    if (cond_guard != nullptr) {
      m_applicable_cond_guards.push_back(cond_guard);
    }
  }
  assert(!m_applicable_cond_guards.empty());

  add_virtual_gcol_base_cols(table, thd->mem_root, &m_table_scan_read_set);
}

bool AlternativeIterator::Init() {
  m_iterator = m_source_iterator.get();
  m_table->read_set = m_original_read_set;
  for (bool *cond_guard : m_applicable_cond_guards) {
    if (!*cond_guard) {
      m_iterator = m_table_scan_iterator.get();
      m_table->read_set = &m_table_scan_read_set;
      break;
    }
  }

  if (m_iterator != m_last_iterator_inited) {
    m_table->file->ha_index_or_rnd_end();
    m_last_iterator_inited = m_iterator;
  }

  return m_iterator->Init();
}

/**
  Get exact count of rows in all tables. When this is called, at least one
  table's SE doesn't include HA_COUNT_ROWS_INSTANT.

    @param qep_tab      List of qep_tab in this JOIN.
    @param table_count  Count of qep_tab in the JOIN.
    @param error [out]  Return any possible error. Else return 0

    @returns
      Cartesian product of count of the rows in all tables if success
      0 if error.

  @note The "error" parameter is required for the sake of testcases like the
        one in innodb-wl6742.test:272. Earlier if an error was raised by
        ha_records, it wasn't handled by get_exact_record_count. Instead it was
        just allowed to go to the execution phase, where end_send_group would
        see the same error and raise it.

        But with the new function 'end_send_count' in the execution phase,
        such an error should be properly returned so that it can be raised.
*/
static ulonglong get_exact_record_count(QEP_TAB *qep_tab, uint table_count,
                                        int *error) {
  ulonglong count = 1;
  QEP_TAB *qt;

  for (uint i = 0; i < table_count; i++) {
    ha_rows tmp = 0;
    qt = qep_tab + i;

    if (qt->type() == JT_ALL || (qt->index() == qt->table()->s->primary_key &&
                                 qt->table()->file->primary_key_is_clustered()))
      *error = qt->table()->file->ha_records(&tmp);
    else
      *error = qt->table()->file->ha_records(&tmp, qt->index());
    if (*error != 0) {
      (void)report_handler_error(qt->table(), *error);
      return 0;
    }
    count *= tmp;
  }
  *error = 0;
  return count;
}

int UnqualifiedCountIterator::Read() {
  if (!m_has_row) {
    return -1;
  }

  for (Item *item : *m_join->fields) {
    if (item->type() == Item::SUM_FUNC_ITEM &&
        down_cast<Item_sum *>(item)->sum_func() == Item_sum::COUNT_FUNC) {
      int error;
      ulonglong count = get_exact_record_count(m_join->qep_tab,
                                               m_join->primary_tables, &error);
      if (error) return 1;

      down_cast<Item_sum_count *>(item)->make_const(
          static_cast<longlong>(count));
    }
  }

  // If we are outputting to a temporary table, we need to copy the results
  // into it here. It is also used for nonaggregated items, even when there are
  // no temporary tables involved.
  if (copy_funcs(&m_join->tmp_table_param, m_join->thd)) {
    return 1;
  }

  m_has_row = false;
  return 0;
}

int ZeroRowsAggregatedIterator::Read() {
  if (!m_has_row) {
    return -1;
  }

  // Mark tables as containing only NULL values
  for (Table_ref *table = m_join->query_block->leaf_tables; table;
       table = table->next_leaf) {
    table->table->set_null_row();
  }

  // Calculate aggregate functions for no rows

  /*
    Must notify all fields that there are no rows (not only those
    that will be returned) because join->having may refer to
    fields that are not part of the result columns.
   */
  for (Item *item : *m_join->fields) {
    item->no_rows_in_result();
  }

  m_has_row = false;
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

TableValueConstructorIterator::TableValueConstructorIterator(
    THD *thd, ha_rows *examined_rows,
    const mem_root_deque<mem_root_deque<Item *> *> &row_value_list,
    mem_root_deque<Item *> *join_fields)
    : RowIterator(thd),
      m_examined_rows(examined_rows),
      m_row_value_list(row_value_list),
      m_output_refs(join_fields) {
  assert(examined_rows != nullptr);
}

bool TableValueConstructorIterator::Init() {
  m_row_it = m_row_value_list.begin();
  return false;
}

int TableValueConstructorIterator::Read() {
  if (*m_examined_rows == m_row_value_list.size()) return -1;

  // If the TVC has a single row, we don't create Item_values_column reference
  // objects during resolving. We will instead use the single row directly from
  // Query_block::item_list, such that we don't have to change references here.
  if (m_row_value_list.size() != 1) {
    auto output_refs_it = VisibleFields(*m_output_refs).begin();
    for (const Item *value : **m_row_it) {
      Item_values_column *ref =
          down_cast<Item_values_column *>(*output_refs_it);
      ++output_refs_it;

      // Ideally we would not be casting away constness here. However, as the
      // evaluation of Item objects during execution is not const (i.e. none of
      // the val methods are const), the reference contained in a
      // Item_values_column object cannot be const.
      ref->set_value(const_cast<Item *>(value));
    }
    ++m_row_it;
  }

  ++*m_examined_rows;
  return 0;
}

static inline pair<uchar *, key_part_map> FindKeyBufferAndMap(
    const Index_lookup *ref) {
  if (ref->keypart_hash != nullptr) {
    return make_pair(pointer_cast<uchar *>(ref->keypart_hash), key_part_map{1});
  } else {
    return make_pair(ref->key_buff, make_prev_keypart_map(ref->key_parts));
  }
}

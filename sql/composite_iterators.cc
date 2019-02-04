/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/composite_iterators.h"

#include <string.h>
#include <atomic>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "scope_guard.h"
#include "sql/debug_sync.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_sum.h"
#include "sql/key.h"
#include "sql/opt_explain.h"
#include "sql/opt_trace.h"
#include "sql/pfs_batch_mode.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_tmp_table.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"

class Opt_trace_context;
template <class T>
class List;

using std::string;
using std::vector;

namespace {

void SwitchSlice(JOIN *join, int slice_num) {
  if (!join->ref_items[slice_num].is_null()) {
    join->set_ref_item_slice(slice_num);
  }
}

}  // namespace

int FilterIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) return err;

    bool matched = m_condition->val_int();

    if (thd()->killed) {
      thd()->send_kill_message();
      return 1;
    }

    /* check for errors evaluating the condition */
    if (thd()->is_error()) return 1;

    if (!matched) {
      m_source->UnlockRow();
      continue;
    }

    // Successful row.
    return 0;
  }
}

bool LimitOffsetIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  for (ha_rows row_idx = 0; row_idx < m_offset; ++row_idx) {
    int err = m_source->Read();
    if (err == 1) {
      return true;  // Note that this will propagate Read() errors to Init().
    } else if (err == -1) {
      m_seen_rows = m_limit;  // So that Read() will return -1.
      return false;           // EOF is not an error.
    }
    if (m_skipped_rows != nullptr) {
      ++*m_skipped_rows;
    }
    m_source->UnlockRow();
  }
  m_seen_rows = m_offset;
  return false;
}

vector<RowIterator::Child> FilterIterator::children() const {
  // Return the source iterator, and also iterators for any subqueries in the
  // condition.
  vector<Child> ret{{m_source.get(), ""}};

  ForEachSubselect(m_condition, [&ret](int select_number, bool is_dependent,
                                       bool is_cacheable,
                                       RowIterator *iterator) {
    char description[256];
    if (is_dependent) {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in condition; dependent)", select_number);
    } else if (!is_cacheable) {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in condition; uncacheable)",
               select_number);
    } else {
      snprintf(description, sizeof(description),
               "Select #%d (subquery in condition; run only once)",
               select_number);
    }
    ret.push_back(Child{iterator, description});
  });

  return ret;
}

int LimitOffsetIterator::Read() {
  if (m_seen_rows++ >= m_limit) {
    return -1;
  } else {
    return m_source->Read();
  }
}

bool AggregateIterator::Init() {
  DBUG_ASSERT(!m_join->tmp_table_param.precomputed_group_by);
  if (m_source->Init()) {
    return true;
  }

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();

  m_first_row = true;
  m_eof = false;
  m_save_nullinfo = 0;
  return false;
}

int AggregateIterator::Read() {
  if (m_eof) {
    // We've seen the last row earlier.
    if (m_save_nullinfo != 0) {
      m_join->restore_fields(m_save_nullinfo);
      m_save_nullinfo = 0;
    }
    return -1;
  }

  // Switch to the input slice before we call Read(), so that any processing
  // that happens in sub-iterators is on the right slice.
  SwitchSlice(m_join, m_input_slice);

  if (m_first_row) {
    // Start the first group, if possible. (If we're not at the first row,
    // we already saw the first row in the new group at the previous Read().)
    m_first_row = false;
    int err = m_source->Read();
    if (err == -1) {
      m_eof = true;
      if (m_join->grouped || m_join->group_optimized_away) {
        return -1;
      } else {
        // If there's no GROUP BY, we need to output a row even if there are no
        // input rows.

        // Calculate aggregate functions for no rows
        for (Item &item : *m_join->get_current_fields()) {
          item.no_rows_in_result();
        }

        /*
          Mark tables as containing only NULL values for ha_write_row().
          Calculate a set of tables for which NULL values need to
          be restored after sending data.
        */
        if (m_join->clear_fields(&m_save_nullinfo)) {
          return 1;
        }
        return 0;
      }
    }
    if (err != 0) return err;
  }

  // This is the start of a new group. Make a copy of the group expressions,
  // because they risk being overwritten on the next call to m_source->Read().
  // We cannot reuse the Item_cached_* fields in m_join->group_fields for this
  // (even though also need to be initialized as part of the start of the
  // group), because they are overwritten by the testing at each row, just like
  // the data from Read() will be.
  //
  // If we are outputting to a temporary table (ie., there's a
  // MaterializeIterator after us), this copy of the group expressions actually
  // goes directly into the output row, since there's room there. In this case,
  // MaterializeIterator does not try to do the copying itself; it would only
  // get the wrong version.
  {
    Switch_ref_item_slice slice_switch(m_join, m_output_slice);

    // m_temp_table_param->items_to_copy, copied through copy_funcs(),
    // can contain two distinct kinds of Items:
    //
    //  - Group expressions, similar to the ones we are copying in copy_fields()
    //    (by way of copy_fields_and_funcs()), e.g. GROUP BY f1 + 1. If we are
    //    materializing, and setup_copy_fields() was never called (which happens
    //    when we materialize due to ORDER BY and set up copy_funcs() via
    //    ConvertItemsToCopy -- the difference is largely due to historical
    //    accident), these expressions will point to the input fields, whose
    //    values are lost when we start the next group. If, on the other hand,
    //    setup_copy_fields() _was_ called, we can copy them later, and due to
    //    the slice system, they'll refer to the Item_fields we just copied
    //    _to_, but we can't rely on that.
    //  - When outputting to a materialized table only: Non-group expressions.
    //    When we copy them here, they can refer to aggregates that
    //    are not ready before output time (e.g., SUM(f1) + 1), and will thus
    //    get the wrong value.
    //
    // We solve the case of #1 by calling copy_funcs() here (through
    // copy_fields_and_funcs()), and then the case of #2 by calling copy_funcs()
    // again later for only those expressions containing aggregates, once those
    // aggregates have their final value. This works even for cases that
    // reference group expressions (e.g. SELECT f1 + SUM(f2) GROUP BY f1),
    // because setup_fields() has done special splitting of such expressions and
    // replaced the group fields by Item_refs pointing to saved copies of them.
    // It's complicated, and it's really a problem we brought on ourselves.
    if (copy_fields_and_funcs(m_temp_table_param, m_join->thd)) {
      return 1;
    }

    (void)update_item_cache_if_changed(m_join->group_fields);
    // TODO: Implement rollup.
    if (init_sum_functions(m_join->sum_funcs, m_join->sum_funcs_end[0])) {
      return 1;
    }
  }

  // Keep reading rows as long as they are part of the existing group.
  for (;;) {
    int err = m_source->Read();
    if (err == 1) return 1;  // Error.

    if (err == -1) {
      // End of input rows; return the last group.
      SwitchSlice(m_join, m_output_slice);

      // Store the result in the temporary table, if we are outputting to that.
      // Also see the comment after create_field(), above.
      copy_sum_funcs(m_join->sum_funcs,
                     m_join->sum_funcs_end[m_join->send_group_parts]);
      if (m_temp_table_param->items_to_copy != nullptr) {
        if (copy_funcs(m_temp_table_param, m_join->thd,
                       CFT_DEPENDING_ON_AGGREGATE)) {
          return 1;
        }
      }

      m_eof = true;
      return 0;
    }

    int idx = update_item_cache_if_changed(m_join->group_fields);
    if (idx >= 0) {
      // The group changed. Return the current row; the next Read() will deal
      // with the new group.
      SwitchSlice(m_join, m_output_slice);

      // Store the result in the temporary table, if we are outputting to that.
      // Also see the comment after create_field(), above.
      copy_sum_funcs(m_join->sum_funcs,
                     m_join->sum_funcs_end[m_join->send_group_parts]);
      if (m_temp_table_param->items_to_copy != nullptr) {
        if (copy_funcs(m_temp_table_param, m_join->thd,
                       CFT_DEPENDING_ON_AGGREGATE)) {
          return 1;
        }
      }

      return 0;
    } else {
      // We're still in the same group.
      if (update_sum_func(m_join->sum_funcs)) {
        return 1;
      }
    }
  }
}

void AggregateIterator::UnlockRow() {
  // Most likely, HAVING failed. Ideally, we'd like to backtrack and unlock
  // all rows that went into this aggregate, but we can't do that, and we also
  // can't unlock the _current_ row, since that belongs to a different group.
  // Thus, do nothing.
}

vector<string> AggregateIterator::DebugString() const {
  string ret;
  if (m_join->grouped || m_join->group_optimized_away) {
    if (m_join->sum_funcs == m_join->sum_funcs_end[0]) {
      ret = "Group (no aggregates)";
    } else {
      ret = "Group aggregate: ";
    }
  } else {
    ret = "Aggregate: ";
  }

  bool first = true;
  for (Item_sum **item = m_join->sum_funcs; item != m_join->sum_funcs_end[0];
       ++item) {
    if (first) {
      first = false;
    } else {
      ret += ", ";
    }
    ret += ItemToString(*item);
  }
  return {ret};
}

bool PrecomputedAggregateIterator::Init() {
  DBUG_ASSERT(m_join->tmp_table_param.precomputed_group_by);
  DBUG_ASSERT(m_join->grouped || m_join->group_optimized_away);
  return m_source->Init();
}

int PrecomputedAggregateIterator::Read() {
  int err = m_source->Read();
  if (err != 0) {
    return err;
  }

  // Even if the aggregates have been precomputed (typically by
  // QUICK_RANGE_MIN_MAX), we need to copy over the non-aggregated
  // fields here.
  if (copy_fields_and_funcs(m_temp_table_param, m_join->thd)) {
    return 1;
  }
  SwitchSlice(m_join, m_output_slice);
  return 0;
}

void PrecomputedAggregateIterator::UnlockRow() {
  // See AggregateIterator::UnlockRow().
}

vector<string> PrecomputedAggregateIterator::DebugString() const {
  string ret;

  // If precomputed_group_by is set, there's always grouping; thus, our
  // EXPLAIN output should always say “group”, unlike AggregateIterator.
  // Do note that neither m_join->grouped nor m_join->group_optimized_away
  // need to be set (in particular, this seems to be the case for
  // skip index scan).
  if (m_join->sum_funcs == m_join->sum_funcs_end[0]) {
    ret = "Group (computed in earlier step, no aggregates)";
  } else {
    ret = "Group aggregate (computed in earlier step): ";
  }

  bool first = true;
  for (Item_sum **item = m_join->sum_funcs; item != m_join->sum_funcs_end[0];
       ++item) {
    if (first) {
      first = false;
    } else {
      ret += ", ";
    }
    ret += ItemToString(*item);
  }
  return {ret};
}

bool NestedLoopIterator::Init() {
  if (m_source_outer->Init()) {
    return true;
  }
  m_state = NEEDS_OUTER_ROW;
  m_source_inner->EndPSIBatchModeIfStarted();
  return false;
}

int NestedLoopIterator::Read() {
  if (m_state == END_OF_ROWS) {
    return -1;
  }

  for (;;) {  // Termination condition within loop.
    if (m_state == NEEDS_OUTER_ROW) {
      int err = m_source_outer->Read();
      if (err == 1) {
        return 1;  // Error.
      }
      if (err == -1) {
        m_state = END_OF_ROWS;
        return -1;
      }
      if (m_pfs_batch_mode) {
        m_source_inner->StartPSIBatchMode();
      }
      if (m_source_inner->Init()) {
        return 1;
      }
      m_source_inner->SetNullRowFlag(false);
      m_state = READING_FIRST_INNER_ROW;
    }
    DBUG_ASSERT(m_state == READING_INNER_ROWS ||
                m_state == READING_FIRST_INNER_ROW);

    int err = m_source_inner->Read();
    if (err != 0) {
      m_source_inner->EndPSIBatchModeIfStarted();
    }
    if (err == 1) {
      return 1;  // Error.
    }
    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }
    if (err == -1) {
      // Out of inner rows for this outer row. If we are an outer join
      // and never found any inner rows, return a null-complemented row.
      // If not, skip that and go straight to reading a new outer row.
      if ((m_join_type == JoinType::OUTER &&
           m_state == READING_FIRST_INNER_ROW) ||
          m_join_type == JoinType::ANTI) {
        m_source_inner->SetNullRowFlag(true);
        m_state = NEEDS_OUTER_ROW;
        return 0;
      } else {
        m_state = NEEDS_OUTER_ROW;
        continue;
      }
    }

    // An inner row has been found.

    if (m_join_type == JoinType::ANTI) {
      // Anti-joins should stop scanning the inner side as soon as we see a row.
      m_state = NEEDS_OUTER_ROW;
      continue;
    }

    // We have a new row.
    m_state = READING_INNER_ROWS;
    return 0;
  }
}

vector<string> NestedLoopIterator::DebugString() const {
  switch (m_join_type) {
    case JoinType::INNER:
      return {"Nested loop inner join"};
    case JoinType::OUTER:
      return {"Nested loop left join"};
    case JoinType::ANTI:
      return {"Nested loop anti-join"};
    default:
      DBUG_ASSERT(false);
      return {"Nested loop <error>"};
  }
}

vector<string> CacheInvalidatorIterator::DebugString() const {
  string ret =
      string("Invalidate materialized tables (row from ") + m_name + ")";
  return {ret};
}

MaterializeIterator::MaterializeIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *tmp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator,
    const Common_table_expr *cte, SELECT_LEX *select_lex, JOIN *join,
    int ref_slice, bool copy_fields_and_items, bool rematerialize,
    ha_rows limit_rows)
    : TableRowIterator(thd, table),
      m_subquery_iterator(move(subquery_iterator)),
      m_table_iterator(move(table_iterator)),
      m_cte(cte),
      m_tmp_table_param(tmp_table_param),
      m_select_lex(select_lex),
      m_join(join),
      m_ref_slice(ref_slice),
      m_copy_fields_and_items(copy_fields_and_items),
      m_rematerialize(rematerialize),
      m_limit_rows(limit_rows),
      m_invalidators(thd->mem_root) {}

bool MaterializeIterator::Init() {
  // If this is a CTE, it could be referred to multiple times in the same query.
  // If so, check if we have already been materialized through any of our alias
  // tables.
  if (!table()->materialized && m_cte != nullptr) {
    for (TABLE_LIST *table_ref : m_cte->tmp_tables) {
      if (table_ref->table->materialized) {
        table()->materialized = true;
        break;
      }
    }
  }

  if (table()->materialized) {
    bool rematerialize = m_rematerialize;

    if (!rematerialize) {
      // See if any lateral tables that we depend on have changed since last
      // time (which would force a rematerialization).
      //
      // TODO: It would be better, although probably much harder, to check the
      // actual column values instead of just whether we've seen any new rows.
      for (const Invalidator &invalidator : m_invalidators) {
        if (invalidator.iterator->generation() !=
            invalidator.generation_at_last_materialize) {
          rematerialize = true;
          break;
        }
      }
    }

    if (!rematerialize) {
      // Just a rescan of the same table.
      return m_table_iterator->Init();
    }
  }
  table()->set_not_started();

  {
    Opt_trace_context *const trace = &thd()->opt_trace;
    Opt_trace_object trace_wrapper(trace);
    Opt_trace_object trace_exec(trace, "materialize");
    trace_exec.add_select_number(m_select_lex->select_number);
    Opt_trace_array trace_steps(trace, "steps");

    if (m_subquery_iterator->Init()) {
      return true;
    }

    if (!table()->is_created()) {
      if (instantiate_tmp_table(thd(), table())) {
        return true;
      }
      empty_record(table());
    }

    table()->file->ha_delete_all_rows();
    m_join->unit->clear_corr_ctes();

    // If we are removing duplicates by way of a hash field
    // (see doing_hash_deduplication() for an explanation), we need to
    // initialize scanning of the index over that hash field. (This is entirely
    // separate from any index usage when reading back the materialized table;
    // m_table_iterator will do that for us.)
    auto end_unique_index =
        create_scope_guard([&] { table()->file->ha_index_end(); });
    if (!table()->file->inited && doing_hash_deduplication()) {
      if (table()->file->ha_index_init(0, 0)) {
        return true;
      }
    } else {
      // We didn't open the index, so we don't need to close it.
      end_unique_index.commit();
    }

    PFSBatchMode pfs_batch_mode(&m_join->qep_tab[m_join->const_tables], m_join);
    ha_rows stored_rows = 0;
    while (stored_rows < m_limit_rows) {
      int error = m_subquery_iterator->Read();
      if (error > 0 || thd()->is_error())
        return true;
      else if (error < 0)
        break;
      else if (thd()->killed) {
        thd()->send_kill_message();
        return true;
      }

      // Materialize items for this row.
      if (m_copy_fields_and_items) {
        if (copy_fields_and_funcs(m_tmp_table_param, thd())) return true;
      }

      if (!check_unique_constraint(table())) continue;

      error = table()->file->ha_write_row(table()->record[0]);
      if (error == 0) {
        ++stored_rows;
        continue;
      }
      // create_ondisk_from_heap will generate error if needed.
      if (!table()->file->is_ignorable_error(error)) {
        bool is_duplicate;
        if (create_ondisk_from_heap(thd(), table(), error, true, &is_duplicate))
          return true; /* purecov: inspected */
        // Table's engine changed; index is not initialized anymore.
        if (table()->hash_field) table()->file->ha_index_init(0, false);
        ++stored_rows;
      } else {
        // An ignorable error means duplicate key, ie. we deduplicated away
        // the row. This is seemingly separate from check_unique_constraint(),
        // which only checks hash indexes.
      }
    }

    end_unique_index.rollback();

    table()->materialized = true;
  }

  if (!m_rematerialize) {
    DEBUG_SYNC(thd(), "after_materialize_derived");
  }

  for (Invalidator &invalidator : m_invalidators) {
    invalidator.generation_at_last_materialize =
        invalidator.iterator->generation();
  }

  return m_table_iterator->Init();
}

int MaterializeIterator::Read() {
  /*
    Enable the items which one should use if one wants to evaluate anything
    (e.g. functions in WHERE, HAVING) involving columns of this table.
  */
  if (m_join != nullptr && m_ref_slice != -1) {
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }
  return m_table_iterator->Read();
}

vector<string> MaterializeIterator::DebugString() const {
  // The table iterator could be a whole string of iterators
  // (sort, filter, etc.) due to add_sorting_to_table(), so show them all.
  //
  // TODO: Make the optimizer put these on top of the MaterializeIterator
  // instead (or perhaps better yet, on the subquery iterator), so that
  // table_iterator is always just a single basic iterator.
  vector<string> ret;
  RowIterator *sub_iterator = m_table_iterator.get();
  for (;;) {
    for (string str : sub_iterator->DebugString()) {
      if (sub_iterator->children().size() > 1) {
        // This can happen if e.g. a filter has subqueries in it.
        // TODO: Consider having a RowIterator::parent(), so that we can show
        // the entire tree.
        str += " [other sub-iterators not shown]";
      }
      ret.push_back(str);
    }
    if (sub_iterator->children().empty()) break;
    sub_iterator = sub_iterator->children()[0].iterator;
  }

  string str;
  if (m_cte != nullptr) {
    if (m_cte->tmp_tables.size() == 1) {
      str = "Materialize CTE " + to_string(m_cte->name);
    } else {
      str = "Materialize CTE " + to_string(m_cte->name) + " if needed";
      if (m_cte->tmp_tables[0]->table != table()) {
        // See children().
        str += " (query plan printed elsewhere)";
      }
    }
  } else if (m_rematerialize) {
    str = "Temporary table";
  } else {
    str = "Materialize";
  }

  // We assume that if there's an unique index, it has to be used for
  // deduplication.
  bool any_unique_index = false;
  if (table()->key_info != nullptr) {
    for (size_t i = 0; i < table()->s->keys; ++i) {
      if ((table()->key_info[i].flags & HA_NOSAME) != 0) {
        any_unique_index = true;
        break;
      }
    }
  }

  if (doing_hash_deduplication() || any_unique_index) {
    str += " with deduplication";
  }

  if (!m_invalidators.empty()) {
    bool first = true;
    str += " (invalidate on row from ";
    for (const Invalidator &invalidator : m_invalidators) {
      if (!first) {
        str += "; ";
      }
      first = false;
      str += invalidator.iterator->name();
    }
    str += ")";
  }

  ret.push_back(str);
  return ret;
}

vector<RowIterator::Child> MaterializeIterator::children() const {
  // If a CTE is references multiple times, only bother printing its query plan
  // once, instead of repeating it over and over again.
  //
  // TODO: Consider printing CTE query plans on the top level of the query block
  // instead?
  if (m_cte != nullptr && m_cte->tmp_tables[0]->table != table()) {
    return {};
  }

  char heading[256] = "";
  if (m_limit_rows != HA_POS_ERROR) {
    // We call this “Limit table size” as opposed to “Limit”, to be able to
    // distinguish between the two in EXPLAIN when debugging.
    if (doing_hash_deduplication() || table()->key_info != nullptr) {
      snprintf(heading, sizeof(heading), "Limit table size: %llu unique row(s)",
               m_limit_rows);
    } else {
      snprintf(heading, sizeof(heading), "Limit table size: %llu row(s)",
               m_limit_rows);
    }
  }

  // We don't list the table iterator as an explicit child; we mark it in our
  // DebugString() instead. (Anything else would look confusingly much like a
  // join.)
  return vector<Child>{{m_subquery_iterator.get(), heading}};
}

void MaterializeIterator::AddInvalidator(
    const CacheInvalidatorIterator *invalidator) {
  m_invalidators.push_back(
      Invalidator{invalidator, /*generation_at_last_materialize=*/-1});

  // If we're invalidated, the join also needs to invalidate all of its
  // own materialization operations, but it will automatically do so by
  // virtue of the SELECT_LEX being marked as uncachable
  // (create_iterators() always sets rematerialize=true for such cases).
}

TemptableAggregateIterator::TemptableAggregateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *tmp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, SELECT_LEX *select_lex,
    JOIN *join, int ref_slice)
    : TableRowIterator(thd, table),
      m_subquery_iterator(move(subquery_iterator)),
      m_table_iterator(move(table_iterator)),
      m_tmp_table_param(tmp_table_param),
      m_select_lex(select_lex),
      m_join(join),
      m_ref_slice(ref_slice) {}

bool TemptableAggregateIterator::Init() {
  // NOTE: We never scan these tables more than once, so we don't need to
  // check whether we have already materialized.

  Opt_trace_context *const trace = &thd()->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "temp_table_aggregate");
  trace_exec.add_select_number(m_select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");

  if (m_subquery_iterator->Init()) {
    return true;
  }

  if (!table()->is_created()) {
    if (instantiate_tmp_table(thd(), table())) {
      return true;
    }
    empty_record(table());
  }

  table()->file->ha_delete_all_rows();

  // Initialize the index used for finding the groups.
  if (table()->file->ha_index_init(0, 0)) {
    return true;
  }
  auto end_unique_index =
      create_scope_guard([&] { table()->file->ha_index_end(); });

  PFSBatchMode pfs_batch_mode(&m_join->qep_tab[m_join->const_tables], m_join);
  for (;;) {
    int error = m_subquery_iterator->Read();
    if (error > 0 || thd()->is_error())  // Fatal error
      return true;
    else if (error < 0)
      break;
    else if (thd()->killed)  // Aborted by user
    {
      thd()->send_kill_message();
      return true;
    }

    // See comment below.
    DBUG_ASSERT(m_tmp_table_param->grouped_expressions.size() == 0);

    // Materialize items for this row. Note that groups are copied twice.
    // (FIXME: Is this comment really still current? It seems to date back
    // to pre-2000, but I can't see that it's really true.)
    if (copy_fields(m_tmp_table_param, thd()))
      return 1; /* purecov: inspected */

    // See if we have seen this row already; if so, we want to update it,
    // not insert a new one.
    bool group_found;
    if (using_hash_key()) {
      /*
        We need to call copy_funcs here in order to get correct value for
        hash_field. However, this call isn't needed so early when hash_field
        isn't used as it would cause unnecessary additional evaluation of
        functions to be copied when 2nd and further records in group are
        found.
      */
      if (copy_funcs(m_tmp_table_param, thd()))
        return 1; /* purecov: inspected */
      group_found = !check_unique_constraint(table());
    } else {
      for (ORDER *group = table()->group; group; group = group->next) {
        Item *item = *group->item;
        item->save_org_in_field(group->field_in_tmp_table);
        /* Store in the used key if the field was 0 */
        if (item->maybe_null)
          group->buff[-1] = (char)group->field_in_tmp_table->is_null();
      }
      const uchar *key = m_tmp_table_param->group_buff;
      group_found = !table()->file->ha_index_read_map(
          table()->record[1], key, HA_WHOLE_KEY, HA_READ_KEY_EXACT);
    }
    if (group_found) {
      // Update the existing record. (If it's unchanged, that's a
      // nonfatal error.)
      restore_record(table(), record[1]);
      update_tmptable_sum_func(m_join->sum_funcs, table());
      int error =
          table()->file->ha_update_row(table()->record[1], table()->record[0]);
      if (error != 0 && error != HA_ERR_RECORD_IS_THE_SAME) {
        PrintError(error);
        return 1;
      }
      continue;
    }

    // OK, we need to insert a new row; we need to materialize any items that we
    // are doing GROUP BY on.

    /*
      Why do we advance the slice here and not before copy_fields()?
      Because of the evaluation of *group->item above: if we do it with this tmp
      table's slice, *group->item points to the field materializing the
      expression, which hasn't been calculated yet. We could force the missing
      calculation by doing copy_funcs() before evaluating *group->item; but
      then, for a group made of N rows, we might be doing N evaluations of
      another function when only one would suffice (like the '*' in
      "SELECT a, a*a ... GROUP BY a": only the first/last row of the group,
      needs to evaluate a*a).

      The assertion on tmp_tbl->grouped_expressions.size() is to make sure
      copy_fields() doesn't suffer from the late switching.
    */
    Switch_ref_item_slice slice_switch(m_join, m_ref_slice);

    /*
      Copy null bits from group key to table
      We can't copy all data as the key may have different format
      as the row data (for example as with VARCHAR keys)
    */
    if (!using_hash_key()) {
      ORDER *group;
      KEY_PART_INFO *key_part;
      for (group = table()->group, key_part = table()->key_info[0].key_part;
           group; group = group->next, key_part++) {
        // Field null indicator is located one byte ahead of field value.
        // @todo - check if this NULL byte is really necessary for grouping
        if (key_part->null_bit)
          memcpy(table()->record[0] + key_part->offset - 1, group->buff - 1, 1);
      }
      /* See comment on copy_funcs above. */
      if (copy_funcs(m_tmp_table_param, thd())) return 1;
    }
    init_tmptable_sum_functions(m_join->sum_funcs);
    error = table()->file->ha_write_row(table()->record[0]);
    if (error != 0) {
      if (create_ondisk_from_heap(thd(), table(), error, false, NULL)) {
        end_unique_index.commit();
        return 1;  // Not a table_is_full error.
      }
      // Table's engine changed, index is not initialized anymore
      error = table()->file->ha_index_init(0, false);
      if (error != 0) {
        end_unique_index.commit();
        PrintError(error);
        return 1;
      }
    }
  }

  table()->file->ha_index_end();
  end_unique_index.commit();

  table()->materialized = true;

  return m_table_iterator->Init();
}

int TemptableAggregateIterator::Read() {
  /*
    Enable the items which one should use if one wants to evaluate anything
    (e.g. functions in WHERE, HAVING) involving columns of this table.
  */
  if (m_join != nullptr && m_ref_slice != -1) {
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }
  return m_table_iterator->Read();
}

vector<string> TemptableAggregateIterator::DebugString() const {
  vector<string> ret = m_table_iterator->DebugString();
  ret.push_back("Aggregate using temporary table");
  return ret;
}

vector<RowIterator::Child> TemptableAggregateIterator::children() const {
  // We don't list the table iterator as an explicit child; we mark it in our
  // DebugString() instead. (Anything else would look confusingly much like a
  // join.)
  return vector<Child>{{m_subquery_iterator.get(), ""}};
}

MaterializedTableFunctionIterator::MaterializedTableFunctionIterator(
    THD *thd, Table_function *table_function, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator)
    : TableRowIterator(thd, table),
      m_table_iterator(move(table_iterator)),
      m_table_function(table_function) {}

bool MaterializedTableFunctionIterator::Init() {
  (void)m_table_function->fill_result_table();
  if (table()->in_use->is_error()) {
    return true;
  }
  return m_table_iterator->Init();
}

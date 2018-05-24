/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <atomic>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_sum.h"
#include "sql/opt_explain.h"
#include "sql/opt_trace.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
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
      m_seen_rows = m_offset;  // So that Read() will return -1.
      return false;            // EOF is not an error.
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
  {
    Switch_ref_item_slice slice_switch(m_join, REF_SLICE_ORDERED_GROUP_BY);
    if (copy_fields(&m_join->tmp_table_param, m_join->thd)) {
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
      SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
      m_eof = true;
      return 0;
    }

    int idx = update_item_cache_if_changed(m_join->group_fields);
    if (idx >= 0) {
      // The group changed. Return the current row; the next Read() will deal
      // with the new group.
      SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
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
  if (copy_fields(&m_join->tmp_table_param, m_join->thd)) {
    return 1;
  }
  SwitchSlice(m_join, REF_SLICE_ORDERED_GROUP_BY);
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

MaterializeIterator::MaterializeIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    List<Item> *fields, Temp_table_param *tmp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, SELECT_LEX *select_lex)
    : TableRowIterator(thd, table),
      m_subquery_iterator(move(subquery_iterator)),
      m_table_iterator(move(table_iterator)),
      m_fields(fields),
      m_tmp_table_param(tmp_table_param),
      m_select_lex(select_lex) {}

bool MaterializeIterator::Init() {
  if (table()->materialized) {
    // Just a rescan of the same table.
    return m_table_iterator->Init();
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

    for (;;) {
      // TODO: Activate performance schema batch mode.
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
      if (fill_record(thd(), table(), table()->visible_field_ptr(), *m_fields,
                      NULL, NULL))
        return true; /* purecov: inspected */

      if (!check_unique_constraint(table())) continue;

      error = table()->file->ha_write_row(table()->record[0]);
      if (error == 0) {
        continue;
      }
      // create_ondisk_from_heap will generate error if needed.
      if (!table()->file->is_ignorable_error(error)) {
        bool is_duplicate;
        if (create_ondisk_from_heap(thd(), table(), error, true, &is_duplicate))
          return true; /* purecov: inspected */
        // Table's engine changed; index is not initialized anymore.
        if (table()->hash_field) table()->file->ha_index_init(0, false);
      } else {
        // An ignorable error means duplicate key, ie. we deduplicated away
        // the row.
      }
    }

    table()->materialized = true;
  }

  return m_table_iterator->Init();
}

int MaterializeIterator::Read() { return m_table_iterator->Read(); }

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

  ret.push_back("Materialize");
  return ret;
}

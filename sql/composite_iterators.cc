/* Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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
#include "sql/basic_row_iterators.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/error_handler.h"
#include "sql/field.h"
#include "sql/filesort.h"
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
#include "sql/sql_join_buffer.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_show.h"
#include "sql/sql_tmp_table.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"  // Table_function
#include "sql/temp_table_param.h"
#include "sql/timing_iterator.h"

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
  if (m_offset > 0) {
    m_seen_rows = m_limit;
    m_needs_offset = true;
  } else {
    m_seen_rows = 0;
    m_needs_offset = false;
  }
  return false;
}

int LimitOffsetIterator::Read() {
  if (m_seen_rows >= m_limit) {
    // We either have hit our LIMIT, or we need to skip OFFSET rows.
    // Check which one.
    if (m_needs_offset) {
      // We skip OFFSET rows here and not in Init(), since performance schema
      // batch mode may not be set up by the executor before the first Read().
      // This makes sure that
      //
      //   a) we get the performance benefits of batch mode even when reading
      //      OFFSET rows, and
      //   b) we don't inadvertedly enable batch mode (e.g. through the
      //      NestedLoopIterator) during Init(), since the executor may not
      //      be ready to _disable_ it if it gets an error before first Read().
      for (ha_rows row_idx = 0; row_idx < m_offset; ++row_idx) {
        int err = m_source->Read();
        if (err != 0) {
          // Note that we'll go back into this loop if Init() is called again,
          // and return the same error/EOF status.
          return err;
        }
        if (m_skipped_rows != nullptr) {
          ++*m_skipped_rows;
        }
        m_source->UnlockRow();
      }
      m_seen_rows = m_offset;
      m_needs_offset = false;

      // Fall through to LIMIT testing.
    }

    if (m_seen_rows >= m_limit) {
      // We really hit LIMIT (or hit LIMIT immediately after OFFSET finished),
      // so EOF.
      if (m_count_all_rows) {
        // Count rows until the end or error (ignore the error if any).
        while (m_source->Read() == 0) {
          ++*m_skipped_rows;
        }
      }
      return -1;
    }
  }

  const int result = m_source->Read();
  if (m_reject_multiple_rows) {
    if (result != 0) {
      ++m_seen_rows;
      return result;
    }
    // We read a row. Check for scalar subquery cardinality violation
    if (m_seen_rows - m_offset > 0) {
      my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
      return 1;
    }
  }

  ++m_seen_rows;
  return result;
}

AggregateIterator::AggregateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, JOIN *join,
    Temp_table_param *temp_table_param, int output_slice, bool rollup)
    : RowIterator(thd),
      m_source(move(source)),
      m_join(join),
      m_output_slice(output_slice),
      m_temp_table_param(temp_table_param),
      m_rollup(rollup) {}

bool AggregateIterator::Init() {
  DBUG_ASSERT(!m_join->tmp_table_param.precomputed_group_by);

  // Disable any leftover rollup items used in children.
  m_current_rollup_position = -1;
  SetRollupLevel(INT_MAX);

  if (m_source->Init()) {
    return true;
  }

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();

  m_seen_eof = false;
  m_save_nullinfo = 0;

  // Not really used, just to be sure.
  m_last_unchanged_group_item_idx = 0;

  m_state = READING_FIRST_ROW;

  return false;
}

void AggregateIterator::copy_sum_funcs() {
  for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
    Field *f = (*item)->get_result_field();
    if (f != nullptr) {
      (*item)->save_in_field(f, true);
    }
  }
}

int AggregateIterator::Read() {
  switch (m_state) {
    case READING_FIRST_ROW: {
      // Switch to the input slice before we call Read(), so that any processing
      // that happens in sub-iterators is on the right slice.
      SwitchSlice(m_join, m_input_slice);

      // Start the first group, if possible. (If we're not at the first row,
      // we already saw the first row in the new group at the previous Read().)
      int err = m_source->Read();
      if (err == -1) {
        m_seen_eof = true;
        m_state = DONE_OUTPUTTING_ROWS;
        if (m_join->grouped || m_join->group_optimized_away) {
          SetRollupLevel(m_join->send_group_parts);
          return -1;
        } else {
          // If there's no GROUP BY, we need to output a row even if there are
          // no input rows.

          // Calculate aggregate functions for no rows
          for (Item *item : *m_join->get_current_fields()) {
            item->no_rows_in_result();
          }

          /*
            Mark tables as containing only NULL values for ha_write_row().
            Calculate a set of tables for which NULL values need to
            be restored after sending data.
          */
          if (m_join->clear_fields(&m_save_nullinfo)) {
            return 1;
          }
          // If we are outputting to a materialized table, copy the output of
          // the aggregate functions into it.
          if (copy_fields_and_funcs(m_temp_table_param, m_join->thd)) {
            return 1;
          }
          return 0;
        }
      }
      if (err != 0) return err;

      // Set the initial value of the group fields.
      (void)update_item_cache_if_changed(m_join->group_fields);

      m_state = LAST_ROW_STARTED_NEW_GROUP;
      m_last_unchanged_group_item_idx = 0;
    }
      // Fall through.

    case LAST_ROW_STARTED_NEW_GROUP: {
      SetRollupLevel(m_join->send_group_parts);

      // This is the start of a new group. Make a copy of the group expressions,
      // because they risk being overwritten on the next call to
      // m_source->Read(). We cannot reuse the Item_cached_* fields in
      // m_join->group_fields for this (even though also need to be initialized
      // as part of the start of the group), because they are overwritten by the
      // testing at each row, just like the data from Read() will be.
      //
      // If we are outputting to a temporary table (ie., there's a
      // MaterializeIterator after us), this copy of the group expressions
      // actually goes directly into the output row, since there's room there.
      // In this case, MaterializeIterator does not try to do the copying
      // itself; it would only get the wrong version.
      SwitchSlice(m_join, m_output_slice);

      // m_temp_table_param->items_to_copy, copied through copy_funcs(),
      // can contain two distinct kinds of Items:
      //
      //  - Group expressions, similar to the ones we are copying in
      //    copy_fields() (by way of copy_fields_and_funcs()), e.g.
      //    GROUP BY f1 + 1. If we are materializing, and setup_copy_fields()
      //    was never called (which happens when we materialize due to ORDER BY
      //    and set up copy_funcs() via ConvertItemsToCopy -- the difference is
      //    largely due to historical accident), these expressions will point to
      //    the input fields, whose values are lost when we start the next
      //    group. If, on the other hand, setup_copy_fields() _was_ called, we
      //    can copy them later, and due to the slice system, they'll refer to
      //    the Item_fields we just copied _to_, but we can't rely on that.
      //  - When outputting to a materialized table only: Non-group expressions.
      //    When we copy them here, they can refer to aggregates that
      //    are not ready before output time (e.g., SUM(f1) + 1), and will thus
      //    get the wrong value.
      //
      // We solve the case of #1 by calling copy_funcs() here (through
      // copy_fields_and_funcs()), and then the case of #2 by calling
      // copy_funcs() again later for only those expressions containing
      // aggregates, once those aggregates have their final value. This works
      // even for cases that reference group expressions (e.g. SELECT f1 +
      // SUM(f2) GROUP BY f1), because setup_fields() has done special splitting
      // of such expressions and replaced the group fields by Item_refs pointing
      // to saved copies of them. It's complicated, and it's really a problem we
      // brought on ourselves.
      if (copy_fields_and_funcs(m_temp_table_param, m_join->thd)) {
        return 1;
      }

      for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
        if (m_rollup) {
          if (down_cast<Item_rollup_sum_switcher *>(*item)
                  ->reset_and_add_for_rollup(m_last_unchanged_group_item_idx))
            return true;
        } else {
          if ((*item)->reset_and_add()) return true;
        }
      }

      m_state = READING_ROWS;
    }
      // Fall through.

    case READING_ROWS:
      // Switch to the input slice before we call Read(), so that any
      // processing that happens in sub-iterators is on the right slice.
      SwitchSlice(m_join, m_input_slice);

      // Keep reading rows as long as they are part of the existing group.
      for (;;) {
        int err = m_source->Read();
        if (err == 1) return 1;  // Error.

        if (err == -1) {
          m_seen_eof = true;

          // End of input rows; return the last group.
          SwitchSlice(m_join, m_output_slice);

          // Store the result in the temporary table, if we are outputting
          // to that.
          copy_sum_funcs();
          if (m_temp_table_param->items_to_copy != nullptr) {
            if (copy_funcs(m_temp_table_param, m_join->thd,
                           CFT_DEPENDING_ON_AGGREGATE)) {
              return 1;
            }
          }

          if (m_rollup && m_join->send_group_parts > 0) {
            // Also output the final groups, including the total row
            // (with NULLs in all fields).
            SetRollupLevel(m_join->send_group_parts);
            m_last_unchanged_group_item_idx = 0;
            m_state = OUTPUTTING_ROLLUP_ROWS;
          } else {
            SetRollupLevel(m_join->send_group_parts);
            m_state = DONE_OUTPUTTING_ROWS;
          }
          return 0;
        }

        int first_changed_idx =
            update_item_cache_if_changed(m_join->group_fields);
        if (first_changed_idx >= 0) {
          // The group changed. Return the current row and mark so that next
          // Read() will deal with the new group.
          SwitchSlice(m_join, m_output_slice);

          // Store the result in the temporary table, if we are outputting
          // to that.
          copy_sum_funcs();
          if (m_temp_table_param->items_to_copy != nullptr) {
            if (copy_funcs(m_temp_table_param, m_join->thd,
                           CFT_DEPENDING_ON_AGGREGATE)) {
              return 1;
            }
          }

          // If we have rollup, we may need to output more than one row.
          // Mark so that the next calls to Read() will return those rows.
          //
          // NOTE: first_changed_idx is the first group value that _changed_,
          // while what we store is the last item that did _not_ change.
          if (m_rollup) {
            m_last_unchanged_group_item_idx = first_changed_idx + 1;
            if (static_cast<unsigned>(first_changed_idx) <
                m_join->send_group_parts - 1) {
              SetRollupLevel(m_join->send_group_parts);
              m_state = OUTPUTTING_ROLLUP_ROWS;
            } else {
              SetRollupLevel(m_join->send_group_parts);
              m_state = LAST_ROW_STARTED_NEW_GROUP;
            }
          } else {
            m_last_unchanged_group_item_idx = 0;
            m_state = LAST_ROW_STARTED_NEW_GROUP;
          }
          return 0;
        }

        // Give the new values to all the new aggregate functions.
        for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
          if (m_rollup) {
            if (down_cast<Item_rollup_sum_switcher *>(*item)
                    ->aggregator_add_all()) {
              return 1;
            }
          } else {
            if ((*item)->aggregator_add()) {
              return 1;
            }
          }
        }

        // We're still in the same group, so just loop back.
      }

    case OUTPUTTING_ROLLUP_ROWS: {
      m_join->current_ref_item_slice = -1;

      SetRollupLevel(m_current_rollup_position - 1);

      // Save fields that are now NULL (we can't call copy_fields_and_funcs(),
      // or the next group would bleed over).
      for (Item_copy *item : m_temp_table_param->grouped_expressions) {
        if (has_rollup_result(item->get_item())) {
          if (item->copy(thd())) return true;
        }
      }

      // Store the result in the temporary table, if we are outputting to that.
      copy_sum_funcs();
      if (m_temp_table_param->items_to_copy != nullptr) {
        if (copy_funcs(m_temp_table_param, m_join->thd,
                       CFT_DEPENDING_ON_AGGREGATE)) {
          return 1;
        }
        if (copy_funcs(m_temp_table_param, m_join->thd, CFT_ROLLUP_NULLS)) {
          return 1;
        }
      }

      if (m_current_rollup_position <= m_last_unchanged_group_item_idx) {
        // Done outputting rollup rows; on next Read() call, deal with the new
        // group instead.
        if (m_seen_eof) {
          m_state = DONE_OUTPUTTING_ROWS;
        } else {
          m_state = LAST_ROW_STARTED_NEW_GROUP;
        }
      }

      return 0;
    }

    case DONE_OUTPUTTING_ROWS:
      SwitchSlice(m_join,
                  m_output_slice);  // We could have set it to -1 earlier.
      if (m_save_nullinfo != 0) {
        m_join->restore_fields(m_save_nullinfo);
        m_save_nullinfo = 0;
      }
      SetRollupLevel(INT_MAX);  // Higher-level iterators up above should not
                                // activate any rollup.
      return -1;
  }

  DBUG_ASSERT(false);
  return 1;
}

void AggregateIterator::SetRollupLevel(int level) {
  if (m_rollup && m_current_rollup_position != level) {
    m_current_rollup_position = level;
    for (Item_rollup_group_item *item : m_join->rollup_group_items) {
      item->set_current_rollup_level(level);
    }
    for (Item_rollup_sum_switcher *item : m_join->rollup_sums) {
      item->set_current_rollup_level(level);
    }
  }
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

bool NestedLoopIterator::Init() {
  if (m_source_outer->Init()) {
    return true;
  }
  m_state = NEEDS_OUTER_ROW;
  if (m_pfs_batch_mode) {
    m_source_inner->EndPSIBatchModeIfStarted();
  }
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

      // Init() could read the NULL row flags (e.g., when building a hash
      // table), so unset them before instead of after.
      m_source_inner->SetNullRowFlag(false);

      if (m_source_inner->Init()) {
        return 1;
      }
      m_state = READING_FIRST_INNER_ROW;
    }
    DBUG_ASSERT(m_state == READING_INNER_ROWS ||
                m_state == READING_FIRST_INNER_ROW);

    int err = m_source_inner->Read();
    if (err != 0 && m_pfs_batch_mode) {
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
      // Anti-joins should stop scanning the inner side as soon as we see
      // a row, without returning that row.
      m_state = NEEDS_OUTER_ROW;
      continue;
    }

    // We have a new row. Semijoins should stop after the first row;
    // regular joins (inner and outer) should go on to scan the rest.
    if (m_join_type == JoinType::SEMI) {
      m_state = NEEDS_OUTER_ROW;
    } else {
      m_state = READING_INNER_ROWS;
    }
    return 0;
  }
}

MaterializeIterator::MaterializeIterator(
    THD *thd, Mem_root_array<QueryBlock> query_blocks_to_materialize,
    TABLE *table, unique_ptr_destroy_only<RowIterator> table_iterator,
    Common_table_expr *cte, SELECT_LEX_UNIT *unit, JOIN *join, int ref_slice,
    bool rematerialize, ha_rows limit_rows, bool reject_multiple_rows)
    : TableRowIterator(thd, table),
      m_query_blocks_to_materialize(std::move(query_blocks_to_materialize)),
      m_table_iterator(move(table_iterator)),
      m_cte(cte),
      m_unit(unit),
      m_join(join),
      m_ref_slice(ref_slice),
      m_rematerialize(rematerialize),
      m_reject_multiple_rows(reject_multiple_rows),
      m_limit_rows(limit_rows),
      m_invalidators(thd->mem_root) {
  if (ref_slice != -1) {
    DBUG_ASSERT(m_join != nullptr);
  }
  if (m_join != nullptr) {
    DBUG_ASSERT(m_query_blocks_to_materialize.size() == 1);
    DBUG_ASSERT(m_query_blocks_to_materialize[0].join == m_join);
  }
}

bool MaterializeIterator::Init() {
  if (!table()->materialized && table()->pos_in_table_list != nullptr &&
      table()->pos_in_table_list->is_view_or_derived()) {
    // Create the table if it's the very first time.
    //
    // TODO(sgunders): create_materialized_table() calls
    // instantiate_tmp_table(), and then has some logic to deal with more
    // complicated cases like multiple reference to the same CTE.
    // Consider unifying this with the instantiate_tmp_table() case below
    // (which is used for e.g. materialization for sorting).
    if (table()->pos_in_table_list->create_materialized_table(thd())) {
      return true;
    }
  }

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
      // See if any lateral tables that we depend on have changed since
      // last time (which would force a rematerialization).
      //
      // TODO: It would be better, although probably much harder, to check
      // the actual column values instead of just whether we've seen any
      // new rows.
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

  if (!table()->is_created()) {
    if (instantiate_tmp_table(thd(), table())) {
      return true;
    }
    empty_record(table());
  } else {
    table()->file->ha_index_or_rnd_end();  // @todo likely unneeded => remove
    table()->file->ha_delete_all_rows();
  }

  if (m_unit != nullptr)
    if (m_unit->clear_correlated_query_blocks()) return true;

  if (m_cte != nullptr) {
    // This is needed in a special case. Consider:
    // SELECT FROM ot WHERE EXISTS(WITH RECURSIVE cte (...)
    //                             SELECT * FROM cte)
    // and assume that the CTE is outer-correlated. When EXISTS is
    // evaluated, SELECT_LEX_UNIT::ClearForExecution() calls
    // clear_correlated_query_blocks(), which scans the WITH clause and clears
    // the CTE, including its references to itself in its recursive definition.
    // But, if the query expression owning WITH is merged up, e.g. like this:
    // FROM ot SEMIJOIN cte ON TRUE,
    // then there is no SELECT_LEX_UNIT anymore, so its WITH clause is
    // not reached. But this "lateral CTE" still needs comprehensive resetting.
    // That's done here.
    if (m_cte->clear_all_references()) return true;
  }

  // If we are removing duplicates by way of a hash field
  // (see doing_hash_deduplication() for an explanation), we need to
  // initialize scanning of the index over that hash field. (This is entirely
  // separate from any index usage when reading back the materialized table;
  // m_table_iterator will do that for us.)
  auto end_unique_index =
      create_scope_guard([&] { table()->file->ha_index_end(); });
  if (doing_hash_deduplication()) {
    if (table()->file->ha_index_init(0, /*sorted=*/false)) {
      return true;
    }
  } else {
    // We didn't open the index, so we don't need to close it.
    end_unique_index.commit();
  }

  if (m_unit != nullptr && m_unit->is_recursive()) {
    if (MaterializeRecursive()) return true;
  } else {
    ha_rows stored_rows = 0;
    for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
      if (MaterializeQueryBlock(query_block, &stored_rows)) return true;
      if (m_reject_multiple_rows && stored_rows > 1) {
        my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
        return true;
      } else if (stored_rows >= m_limit_rows) {
        break;
      }
    }
  }

  end_unique_index.rollback();
  table()->materialized = true;

  if (!m_rematerialize) {
    DEBUG_SYNC(thd(), "after_materialize_derived");
  }

  for (Invalidator &invalidator : m_invalidators) {
    invalidator.generation_at_last_materialize =
        invalidator.iterator->generation();
  }

  return m_table_iterator->Init();
}

/**
  Recursive materialization happens much like regular materialization,
  but some steps are repeated multiple times. Our general strategy is:

    1. Materialize all non-recursive query blocks, once.

    2. Materialize all recursive query blocks in turn.

    3. Repeat #2 until no query block writes any more rows (ie., we have
       converged) -- for UNION DISTINCT queries, rows removed by deduplication
       do not count. Each materialization sees only rows that were newly added
       since the previous iteration; see FollowTailIterator for more details
       on the implementation.

  Note that the result table is written to while other iterators are still
  reading from it; again, see FollowTailIterator. This means that each run
  of #2 can potentially run many actual CTE iterations -- possibly the entire
  query to completion if we have only one query block.

  This is not how the SQL standard specifies recursive CTE execution
  (it assumes building up the new result set from scratch for each iteration,
  using the previous iteration's results), but it is equivalent, and more
  efficient for the class of queries we support, since we don't need to
  re-create the same rows over and over again.
 */
bool MaterializeIterator::MaterializeRecursive() {
  /*
    For RECURSIVE, beginners will forget that:
    - the CTE's column types are defined by the non-recursive member
    - which implies that recursive member's selected expressions are cast to
    the non-recursive member's type.
    That will cause silent truncation and possibly an infinite recursion due
    to a condition like: 'LENGTH(growing_col) < const', or,
    'growing_col < const',
    which is always satisfied due to truncation.

    This situation is similar to
    create table t select "x" as a;
    insert into t select concat("x",a) from t;
    which sends ER_DATA_TOO_LONG in strict mode.

    So we should inform the user.

    If we only raised warnings: it will not interrupt an infinite recursion,
    a MAX_RECURSION hint (if we featured one) may interrupt; but then the
    warnings won't be seen, as the interruption will raise an error. So
    warnings are useless.
    Instead, we send a truncation error: it is visible, indicates the
    source of the problem, and is consistent with the INSERT case above.

    Usually, truncation in SELECT triggers an error only in
    strict mode; but if we don't send an error we get a runaway query;
    and as WITH RECURSIVE is a new feature we don't have to carry the
    permissiveness of the past, so we send an error even if in non-strict
    mode.

    For a non-recursive UNION, truncation shouldn't happen as all UNION
    members participated in type calculation.
  */
  Strict_error_handler strict_handler(
      Strict_error_handler::ENABLE_SET_SELECT_STRICT_ERROR_HANDLER);
  enum_check_fields save_check_for_truncated_fields{};
  bool set_error_handler = thd()->is_strict_mode();
  if (set_error_handler) {
    save_check_for_truncated_fields = thd()->check_for_truncated_fields;
    thd()->check_for_truncated_fields = CHECK_FIELD_WARN;
    thd()->push_internal_handler(&strict_handler);
  }
  auto cleanup_handler = create_scope_guard(
      [this, set_error_handler, save_check_for_truncated_fields] {
        if (set_error_handler) {
          thd()->pop_internal_handler();
          thd()->check_for_truncated_fields = save_check_for_truncated_fields;
        }
      });

  ha_rows stored_rows = 0;

  // Give each recursive iterator access to the stored number of rows
  // (see FollowTailIterator::Read() for details).
  for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
    if (query_block.is_recursive_reference) {
      query_block.recursive_reader->set_stored_rows_pointer(&stored_rows);
    }
  }

#ifndef DBUG_OFF
  // Trash the pointers on exit, to ease debugging of dangling ones to the
  // stack.
  auto pointer_cleanup = create_scope_guard([this] {
    for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
      if (query_block.is_recursive_reference) {
        query_block.recursive_reader->set_stored_rows_pointer(nullptr);
      }
    }
  });
#endif

  // First, materialize all non-recursive query blocks.
  for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
    if (!query_block.is_recursive_reference) {
      if (MaterializeQueryBlock(query_block, &stored_rows)) return true;
    }
  }

  // Then, materialize all recursive query blocks until we converge.
  Opt_trace_context &trace = thd()->opt_trace;
  bool disabled_trace = false;
  ha_rows last_stored_rows;
  do {
    last_stored_rows = stored_rows;
    for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
      if (query_block.is_recursive_reference) {
        if (MaterializeQueryBlock(query_block, &stored_rows)) return true;
      }
    }

    /*
      If recursive query blocks have been executed at least once, and repeated
      executions should not be traced, disable tracing, unless it already is
      disabled.
    */
    if (!disabled_trace &&
        !trace.feature_enabled(Opt_trace_context::REPEATED_SUBSELECT)) {
      trace.disable_I_S_for_this_and_children();
      disabled_trace = true;
    }
  } while (stored_rows > last_stored_rows);

  if (disabled_trace) {
    trace.restore_I_S();
  }
  return false;
}

bool MaterializeIterator::MaterializeQueryBlock(const QueryBlock &query_block,
                                                ha_rows *stored_rows) {
  Opt_trace_context *const trace = &thd()->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "materialize");
  trace_exec.add_select_number(query_block.select_number);
  Opt_trace_array trace_steps(trace, "steps");

  JOIN *join = query_block.join;
  if (join != nullptr) {
    join->set_executed();  // The dynamic range optimizer expects this.

    // TODO(sgunders): Consider doing this in some iterator instead.
    if (join->m_windows.elements > 0 && !join->m_windowing_steps) {
      // Initialize state of window functions as end_write_wf() will be shortcut
      for (Window &w : join->m_windows) {
        w.reset_all_wf_state();
      }
    }
  }

  if (query_block.subquery_iterator->Init()) {
    return true;
  }

  PFSBatchMode pfs_batch_mode(query_block.subquery_iterator.get());
  while (*stored_rows < m_limit_rows) {
    int error = query_block.subquery_iterator->Read();
    if (error > 0 || thd()->is_error())
      return true;
    else if (error < 0)
      break;
    else if (thd()->killed) {
      thd()->send_kill_message();
      return true;
    }

    // Materialize items for this row.
    if (query_block.copy_fields_and_items) {
      if (copy_fields_and_funcs(query_block.temp_table_param, thd()))
        return true;
    }

    if (query_block.disable_deduplication_by_hash_field) {
      DBUG_ASSERT(doing_hash_deduplication());
    } else if (!check_unique_constraint(table())) {
      continue;
    }

    error = table()->file->ha_write_row(table()->record[0]);
    if (error == 0) {
      ++*stored_rows;
      continue;
    }
    // create_ondisk_from_heap will generate error if needed.
    if (!table()->file->is_ignorable_error(error)) {
      bool is_duplicate;
      if (create_ondisk_from_heap(thd(), table(), error, true, &is_duplicate))
        return true; /* purecov: inspected */
      // Table's engine changed; index is not initialized anymore.
      if (table()->hash_field) table()->file->ha_index_init(0, false);
      if (!is_duplicate) ++*stored_rows;

      // Inform each reader that the table has changed under their feet,
      // so they'll need to reposition themselves.
      for (const QueryBlock &query_b : m_query_blocks_to_materialize) {
        if (query_b.is_recursive_reference) {
          query_b.recursive_reader->RepositionCursorAfterSpillToDisk();
        }
      }
    } else {
      // An ignorable error means duplicate key, ie. we deduplicated
      // away the row. This is seemingly separate from
      // check_unique_constraint(), which only checks hash indexes.
    }
  }

  return false;
}

int MaterializeIterator::Read() {
  /*
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table.
  */
  if (m_ref_slice != -1) {
    DBUG_ASSERT(m_join != nullptr);
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }
  return m_table_iterator->Read();
}

void MaterializeIterator::EndPSIBatchModeIfStarted() {
  for (const QueryBlock &query_block : m_query_blocks_to_materialize) {
    query_block.subquery_iterator->EndPSIBatchModeIfStarted();
  }
  m_table_iterator->EndPSIBatchModeIfStarted();
}

bool MaterializeIterator::doing_deduplication() const {
  if (doing_hash_deduplication()) {
    return true;
  }

  // We assume that if there's an unique index, it has to be used for
  // deduplication.
  if (table()->key_info != nullptr) {
    for (size_t i = 0; i < table()->s->keys; ++i) {
      if ((table()->key_info[i].flags & HA_NOSAME) != 0) {
        return true;
      }
    }
  }
  return false;
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

StreamingIterator::StreamingIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table,
    bool copy_fields_and_items, bool provide_rowid, JOIN *join, int ref_slice)
    : TableRowIterator(thd, table),
      m_subquery_iterator(move(subquery_iterator)),
      m_temp_table_param(temp_table_param),
      m_copy_fields_and_items(copy_fields_and_items),
      m_join(join),
      m_output_slice(ref_slice),
      m_provide_rowid(provide_rowid) {
  DBUG_ASSERT(m_subquery_iterator != nullptr);

  // If we have weedout in this query, it will expect to have row IDs that
  // uniquely identify each row, so calling position() will fail (since we
  // do not actually write these rows to anywhere). Use the row number as a
  // fake ID; since the real handler on this temporary table is never called,
  // it is safe to replace it with something of the same length.
  if (m_provide_rowid) {
    if (table->file->ref_length < sizeof(m_row_number)) {
      table->file->ref_length = sizeof(m_row_number);
      table->file->ref = nullptr;
    }
    if (table->file->ref == nullptr) {
      table->file->ref =
          pointer_cast<uchar *>(thd->mem_calloc(table->file->ref_length));
    }
  }
}

bool StreamingIterator::Init() {
  if (m_provide_rowid) {
    memset(table()->file->ref, 0, table()->file->ref_length);
  }

  m_input_slice = m_join->get_ref_item_slice();

  m_row_number = 0;
  return m_subquery_iterator->Init();
}

int StreamingIterator::Read() {
  /*
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table. Make sure to switch to the right output slice before we
    exit the function.
  */
  m_join->set_ref_item_slice(m_input_slice);
  auto switch_to_output_slice = create_scope_guard([&] {
    if (m_output_slice != -1 && !m_join->ref_items[m_output_slice].is_null()) {
      m_join->set_ref_item_slice(m_output_slice);
    }
  });

  int error = m_subquery_iterator->Read();
  if (error != 0) return error;

  // Materialize items for this row.
  if (m_copy_fields_and_items) {
    if (copy_fields_and_funcs(m_temp_table_param, thd())) return 1;
  }

  if (m_provide_rowid) {
    memcpy(table()->file->ref, &m_row_number, sizeof(m_row_number));
    ++m_row_number;
  }

  return 0;
}

TemptableAggregateIterator::TemptableAggregateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join,
    int ref_slice)
    : TableRowIterator(thd, table),
      m_subquery_iterator(move(subquery_iterator)),
      m_table_iterator(move(table_iterator)),
      m_temp_table_param(temp_table_param),
      m_join(join),
      m_ref_slice(ref_slice) {}

bool TemptableAggregateIterator::Init() {
  // NOTE: We never scan these tables more than once, so we don't need to
  // check whether we have already materialized.

  Opt_trace_context *const trace = &thd()->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "temp_table_aggregate");
  trace_exec.add_select_number(m_join->select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");

  if (m_subquery_iterator->Init()) {
    return true;
  }

  if (!table()->is_created()) {
    if (instantiate_tmp_table(thd(), table())) {
      return true;
    }
    empty_record(table());
  } else {
    if (table()->file->inited) {
      // If we're being called several times (in particular, as part of a
      // LATERAL join), the table iterator may have started a scan, so end it
      // before we start our own.
      table()->file->ha_index_or_rnd_end();
    }
    table()->file->ha_delete_all_rows();
  }

  // Initialize the index used for finding the groups.
  if (table()->file->ha_index_init(0, false)) {
    return true;
  }
  auto end_unique_index =
      create_scope_guard([&] { table()->file->ha_index_end(); });

  PFSBatchMode pfs_batch_mode(m_subquery_iterator.get());
  for (;;) {
    int read_error = m_subquery_iterator->Read();
    if (read_error > 0 || thd()->is_error())  // Fatal error
      return true;
    else if (read_error < 0)
      break;
    else if (thd()->killed)  // Aborted by user
    {
      thd()->send_kill_message();
      return true;
    }

    // See comment below.
    DBUG_ASSERT(m_temp_table_param->grouped_expressions.size() == 0);

    // Materialize items for this row. Note that groups are copied twice.
    // (FIXME: Is this comment really still current? It seems to date back
    // to pre-2000, but I can't see that it's really true.)
    if (copy_fields(m_temp_table_param, thd()))
      return true; /* purecov: inspected */

    // See if we have seen this row already; if so, we want to update it,
    // not insert a new one.
    bool group_found;
    if (using_hash_key()) {
      /*
        We need to call copy_funcs here in order to get correct value for
        hash_field. However, this call isn't needed so early when
        hash_field isn't used as it would cause unnecessary additional
        evaluation of functions to be copied when 2nd and further records
        in group are found.
      */
      if (copy_funcs(m_temp_table_param, thd()))
        return true; /* purecov: inspected */
      group_found = !check_unique_constraint(table());
    } else {
      for (ORDER *group = table()->group; group; group = group->next) {
        Item *item = *group->item;
        item->save_org_in_field(group->field_in_tmp_table);
        /* Store in the used key if the field was 0 */
        if (item->maybe_null)
          group->buff[-1] = (char)group->field_in_tmp_table->is_null();
      }
      const uchar *key = m_temp_table_param->group_buff;
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
        return true;
      }
      continue;
    }

    // OK, we need to insert a new row; we need to materialize any items
    // that we are doing GROUP BY on.

    /*
      Why do we advance the slice here and not before copy_fields()?
      Because of the evaluation of *group->item above: if we do it with
      this tmp table's slice, *group->item points to the field
      materializing the expression, which hasn't been calculated yet. We
      could force the missing calculation by doing copy_funcs() before
      evaluating *group->item; but then, for a group made of N rows, we
      might be doing N evaluations of another function when only one would
      suffice (like the '*' in "SELECT a, a*a ... GROUP BY a": only the
      first/last row of the group, needs to evaluate a*a).

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
        // @todo - check if this NULL byte is really necessary for
        // grouping
        if (key_part->null_bit)
          memcpy(table()->record[0] + key_part->offset - 1, group->buff - 1, 1);
      }
      /* See comment on copy_funcs above. */
      if (copy_funcs(m_temp_table_param, thd())) return true;
    }
    init_tmptable_sum_functions(m_join->sum_funcs);
    int error = table()->file->ha_write_row(table()->record[0]);
    if (error != 0) {
      /*
         If the error is HA_ERR_FOUND_DUPP_KEY and the grouping involves a
         TIMESTAMP field, throw a meaningfull error to user with the actual
         reason and the workaround. I.e, "Grouping on temporal is
         non-deterministic for timezones having DST. Please consider switching
         to UTC for this query". This is a temporary measure until we implement
         WL#13148 (Do all internal handling TIMESTAMP in UTC timezone), which
         will make such problem impossible.
       */
      if (error == HA_ERR_FOUND_DUPP_KEY) {
        for (ORDER *group = table()->group; group; group = group->next) {
          if (group->field_in_tmp_table->type() == MYSQL_TYPE_TIMESTAMP) {
            my_error(ER_GROUPING_ON_TIMESTAMP_IN_DST, MYF(0));
            return true;
          }
        }
      }
      if (create_ondisk_from_heap(thd(), table(), error, false, nullptr)) {
        end_unique_index.commit();
        return true;  // Not a table_is_full error.
      }
      // Table's engine changed, index is not initialized anymore
      error = table()->file->ha_index_init(0, false);
      if (error != 0) {
        end_unique_index.commit();
        PrintError(error);
        return true;
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
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table.
  */
  if (m_join != nullptr && m_ref_slice != -1) {
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }
  return m_table_iterator->Read();
}

MaterializedTableFunctionIterator::MaterializedTableFunctionIterator(
    THD *thd, Table_function *table_function, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator)
    : TableRowIterator(thd, table),
      m_table_iterator(move(table_iterator)),
      m_table_function(table_function) {}

bool MaterializedTableFunctionIterator::Init() {
  if (!table()->materialized) {
    // Create the table if it's the very first time.
    if (table()->pos_in_table_list->create_materialized_table(thd())) {
      return true;
    }
  }
  (void)m_table_function->fill_result_table();
  if (table()->in_use->is_error()) {
    return true;
  }
  return m_table_iterator->Init();
}

WeedoutIterator::WeedoutIterator(THD *thd,
                                 unique_ptr_destroy_only<RowIterator> source,
                                 SJ_TMP_TABLE *sj,
                                 table_map tables_to_get_rowid_for)
    : RowIterator(thd),
      m_source(move(source)),
      m_sj(sj),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for) {
  // Confluent weedouts should have been rewritten to LIMIT 1 earlier.
  DBUG_ASSERT(!m_sj->is_confluent);
  DBUG_ASSERT(m_sj->tmp_table != nullptr);
}

bool WeedoutIterator::Init() {
  if (m_sj->tmp_table->file->ha_delete_all_rows()) {
    return true;
  }
  if (m_sj->tmp_table->hash_field != nullptr &&
      !m_sj->tmp_table->file->inited) {
    m_sj->tmp_table->file->ha_index_init(0, false);
  }
  for (SJ_TMP_TABLE_TAB *tab = m_sj->tabs; tab != m_sj->tabs_end; ++tab) {
    TABLE *table = tab->qep_tab->table();
    if (m_tables_to_get_rowid_for & table->pos_in_table_list->map()) {
      table->prepare_for_position();
    }
  }
  return m_source->Init();
}

int WeedoutIterator::Read() {
  for (;;) {
    int ret = m_source->Read();
    if (ret != 0) {
      // Error, or EOF.
      return ret;
    }

    for (SJ_TMP_TABLE_TAB *tab = m_sj->tabs; tab != m_sj->tabs_end; ++tab) {
      TABLE *table = tab->qep_tab->table();
      if ((m_tables_to_get_rowid_for & table->pos_in_table_list->map()) &&
          can_call_position(table)) {
        table->file->position(table->record[0]);
      }
    }

    ret = do_sj_dups_weedout(thd(), m_sj);
    if (ret == -1) {
      // Error.
      return 1;
    }

    if (ret == 0) {
      // Not a duplicate, so return the row.
      return 0;
    }

    // Duplicate, so read the next row instead.
  }
}

RemoveDuplicatesIterator::RemoveDuplicatesIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, const TABLE *table,
    KEY *key, size_t key_len)
    : RowIterator(thd),
      m_source(move(source)),
      m_table(table),
      m_key(key),
      m_key_buf(new (thd->mem_root) uchar[key_len]),
      m_key_len(key_len) {}

bool RemoveDuplicatesIterator::Init() {
  m_first_row = true;
  return m_source->Init();
}

int RemoveDuplicatesIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) {
      return err;
    }

    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    if (!m_first_row && key_cmp(m_key->key_part, m_key_buf, m_key_len) == 0) {
      // Same as previous row, so keep scanning.
      continue;
    }

    m_first_row = false;
    key_copy(m_key_buf, m_table->record[0], m_key, m_key_len);
    return 0;
  }
}

NestedLoopSemiJoinWithDuplicateRemovalIterator::
    NestedLoopSemiJoinWithDuplicateRemovalIterator(
        THD *thd, unique_ptr_destroy_only<RowIterator> source_outer,
        unique_ptr_destroy_only<RowIterator> source_inner, const TABLE *table,
        KEY *key, size_t key_len)
    : RowIterator(thd),
      m_source_outer(move(source_outer)),
      m_source_inner(move(source_inner)),
      m_table_outer(table),
      m_key(key),
      m_key_buf(new (thd->mem_root) uchar[key_len]),
      m_key_len(key_len) {
  DBUG_ASSERT(m_source_outer != nullptr);
  DBUG_ASSERT(m_source_inner != nullptr);
}

bool NestedLoopSemiJoinWithDuplicateRemovalIterator::Init() {
  if (m_source_outer->Init()) {
    return true;
  }
  m_deduplicate_against_previous_row = false;
  return false;
}

int NestedLoopSemiJoinWithDuplicateRemovalIterator::Read() {
  m_source_inner->SetNullRowFlag(false);

  for (;;) {  // Termination condition within loop.
    // Find an outer row that is different (key-wise) from the previous
    // one we returned.
    for (;;) {
      int err = m_source_outer->Read();
      if (err != 0) {
        return err;
      }
      if (thd()->killed) {  // Aborted by user.
        thd()->send_kill_message();
        return 1;
      }

      if (m_deduplicate_against_previous_row &&
          key_cmp(m_key->key_part, m_key_buf, m_key_len) == 0) {
        // Same as previous row, so keep scanning.
        continue;
      }

      break;
    }

    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    // Now find a single (matching) inner row.
    if (m_source_inner->Init()) {
      return 1;
    }

    int err = m_source_inner->Read();
    if (err == 1) {
      return 1;  // Error.
    }
    if (err == -1) {
      // No inner row found for this outer row, so search for a new outer
      // row, potentially with the same key.
      m_deduplicate_against_previous_row = false;
      continue;
    }

    // We found an inner row for this outer row, so we do not want more
    // with the same key.
    m_deduplicate_against_previous_row = true;
    key_copy(m_key_buf, m_table_outer->record[0], m_key, m_key_len);

    return 0;
  }
}

WindowingIterator::WindowingIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source,
    Temp_table_param *temp_table_param, JOIN *join, int output_slice)
    : RowIterator(thd),
      m_source(move(source)),
      m_temp_table_param(temp_table_param),
      m_window(temp_table_param->m_window),
      m_join(join),
      m_output_slice(output_slice) {
  DBUG_ASSERT(!m_window->needs_buffering());
}

bool WindowingIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  m_window->reset_round();

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();

  return false;
}

int WindowingIterator::Read() {
  SwitchSlice(m_join, m_input_slice);

  int err = m_source->Read();
  if (err != 0) {
    return err;
  }

  SwitchSlice(m_join, m_output_slice);

  if (copy_fields_and_funcs(m_temp_table_param, thd(), CFT_HAS_NO_WF)) return 1;

  m_window->check_partition_boundary();

  if (copy_funcs(m_temp_table_param, thd(), CFT_WF)) return 1;

  if (m_window->is_last() && copy_funcs(m_temp_table_param, thd(), CFT_HAS_WF))
    return 1;

  return 0;
}

BufferingWindowingIterator::BufferingWindowingIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source,
    Temp_table_param *temp_table_param, JOIN *join, int output_slice)
    : RowIterator(thd),
      m_source(move(source)),
      m_temp_table_param(temp_table_param),
      m_window(temp_table_param->m_window),
      m_join(join),
      m_output_slice(output_slice) {
  DBUG_ASSERT(m_window->needs_buffering());
}

bool BufferingWindowingIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  m_window->reset_round();
  m_possibly_buffered_rows = false;
  m_last_input_row_started_new_partition = false;
  m_eof = false;

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();
  DBUG_ASSERT(m_input_slice >= 0);

  return false;
}

int BufferingWindowingIterator::Read() {
  SwitchSlice(m_join, m_output_slice);

  if (m_eof) {
    return ReadBufferedRow(/*new_partition_or_eof=*/true);
  }

  // The previous call to Read() may have caused multiple rows to be ready
  // for output, but could only return one of them. See if there are more
  // to be output.
  if (m_possibly_buffered_rows) {
    int err = ReadBufferedRow(m_last_input_row_started_new_partition);
    if (err != -1) {
      return err;
    }
  }

  for (;;) {
    if (m_last_input_row_started_new_partition) {
      /*
        We didn't really buffer this row yet since, we found a partition
        change so we had to finalize the previous partition first.
        Bring back saved row for next partition.
      */
      if (bring_back_frame_row(
              thd(), m_window, m_temp_table_param,
              Window::FBC_FIRST_IN_NEXT_PARTITION,
              Window_retrieve_cached_row_reason::WONT_UPDATE_HINT)) {
        return 1;
      }

      /*
        copy_funcs(CFT_HAS_NO_WF) is not necessary: a non-WF function was
        calculated and saved in OUT, then this OUT column was copied to
        special record, then restored to OUT column.
      */

      m_window->reset_partition_state();
      if (buffer_windowing_record(thd(), m_temp_table_param,
                                  nullptr /* first in new partition */)) {
        return 1;
      }

      m_last_input_row_started_new_partition = false;
    } else {
      // Read a new input row, if it exists. This needs to be done under
      // the input slice, so that any expressions in sub-iterators are
      // evaluated correctly.
      int err;
      {
        Switch_ref_item_slice slice_switch(m_join, m_input_slice);
        err = m_source->Read();
      }
      if (err == 1) {
        return 1;  // Error.
      }
      if (err == -1) {
        // EOF. Read any pending buffered rows, and then that's it.
        m_eof = true;
        return ReadBufferedRow(/*new_partition_or_eof=*/true);
      }

      /*
        This saves the values of non-WF functions for the row. For
        example, 1+t.a. But also 1+LEAD. Even though at this point we lack
        data to compute LEAD; the saved value is thus incorrect; later,
        when the row is fully computable, we will re-evaluate the
        CFT_NON_WF to get a correct value for 1+LEAD.
      */
      if (copy_fields_and_funcs(m_temp_table_param, thd(), CFT_HAS_NO_WF)) {
        return 1;
      }

      bool new_partition = false;
      if (buffer_windowing_record(thd(), m_temp_table_param, &new_partition)) {
        return 1;
      }
      m_last_input_row_started_new_partition = new_partition;
    }

    int err = ReadBufferedRow(m_last_input_row_started_new_partition);
    if (err == 1) {
      return 1;
    }

    if (m_window->needs_restore_input_row()) {
      /*
        Reestablish last row read from input table in case it is needed
        again before reading a new row. May be necessary if this is the
        first window following after a join, cf. the caching presumption
        in EQRefIterator. This logic can be removed if we move to copying
        between out tmp record and frame buffer record, instead of
        involving the in record. FIXME.
      */
      if (bring_back_frame_row(
              thd(), m_window, nullptr /* no copy to OUT */,
              Window::FBC_LAST_BUFFERED_ROW,
              Window_retrieve_cached_row_reason::WONT_UPDATE_HINT)) {
        return 1;
      }
    }

    if (err == 0) {
      return 0;
    }

    // This input row didn't generate an output row right now, so we'll just
    // continue the loop.
  }
}

int BufferingWindowingIterator::ReadBufferedRow(bool new_partition_or_eof) {
  bool output_row_ready;
  if (process_buffered_windowing_record(
          thd(), m_temp_table_param, new_partition_or_eof, &output_row_ready)) {
    return 1;
  }
  if (thd()->killed) {
    thd()->send_kill_message();
    return 1;
  }
  if (output_row_ready) {
    // Return the buffered row, and there are possibly more.
    // These will be checked on the next call to Read().
    m_possibly_buffered_rows = true;
    return 0;
  } else {
    // No more buffered rows.
    m_possibly_buffered_rows = false;
    return -1;
  }
}

MaterializeInformationSchemaTableIterator::
    MaterializeInformationSchemaTableIterator(
        THD *thd, unique_ptr_destroy_only<RowIterator> table_iterator,
        TABLE_LIST *table_list, Item *condition)
    : RowIterator(thd),
      m_table_iterator(move(table_iterator)),
      m_table_list(table_list),
      m_condition(condition) {}

bool MaterializeInformationSchemaTableIterator::Init() {
  m_table_list->table->file->ha_extra(HA_EXTRA_RESET_STATE);
  m_table_list->table->file->ha_delete_all_rows();
  free_io_cache(m_table_list->table);
  m_table_list->table->set_not_started();

  if (do_fill_information_schema_table(thd(), m_table_list, m_condition)) {
    return true;
  }

  m_table_list->schema_table_state = PROCESSED_BY_JOIN_EXEC;

  return m_table_iterator->Init();
}

AppendIterator::AppendIterator(
    THD *thd, std::vector<unique_ptr_destroy_only<RowIterator>> &&sub_iterators)
    : RowIterator(thd), m_sub_iterators(move(sub_iterators)) {
  DBUG_ASSERT(!m_sub_iterators.empty());
}

bool AppendIterator::Init() {
  m_current_iterator_index = 0;
  m_pfs_batch_mode_enabled = false;
  return m_sub_iterators[0]->Init();
}

int AppendIterator::Read() {
  if (m_current_iterator_index >= m_sub_iterators.size()) {
    // Already exhausted all iterators.
    return -1;
  }
  int err = m_sub_iterators[m_current_iterator_index]->Read();
  if (err != -1) {
    // A row, or error.
    return err;
  }

  // EOF. Go to the next iterator.
  m_sub_iterators[m_current_iterator_index]->EndPSIBatchModeIfStarted();
  if (++m_current_iterator_index >= m_sub_iterators.size()) {
    return -1;
  }
  if (m_sub_iterators[m_current_iterator_index]->Init()) {
    return 1;
  }
  if (m_pfs_batch_mode_enabled) {
    m_sub_iterators[m_current_iterator_index]->StartPSIBatchMode();
  }
  return Read();  // Try again, with the new iterator as current.
}

void AppendIterator::SetNullRowFlag(bool is_null_row) {
  DBUG_ASSERT(m_current_iterator_index < m_sub_iterators.size());
  m_sub_iterators[m_current_iterator_index]->SetNullRowFlag(is_null_row);
}

void AppendIterator::StartPSIBatchMode() {
  m_pfs_batch_mode_enabled = true;
  m_sub_iterators[m_current_iterator_index]->StartPSIBatchMode();
}

void AppendIterator::EndPSIBatchModeIfStarted() {
  for (const unique_ptr_destroy_only<RowIterator> &sub_iterator :
       m_sub_iterators) {
    sub_iterator->EndPSIBatchModeIfStarted();
  }
  m_pfs_batch_mode_enabled = false;
}

void AppendIterator::UnlockRow() {
  DBUG_ASSERT(m_current_iterator_index < m_sub_iterators.size());
  m_sub_iterators[m_current_iterator_index]->UnlockRow();
}

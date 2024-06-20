/* Copyright (c) 2001, 2024, Oracle and/or its affiliates.

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

/*
  Process query expressions that are composed of

  1. UNION of query blocks, and/or

  2. have ORDER BY / LIMIT clauses in more than one level.

  An example of 2) is:

    (SELECT * FROM t1 ORDER BY a LIMIT 10) ORDER BY b LIMIT 5
*/

#include "sql/sql_union.h"

#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "prealloced_array.h"  // Prealloced_array
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_subselect.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"
#include "sql/opt_explain.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"
#include "sql/parse_tree_node_base.h"
#include "sql/parse_tree_nodes.h"  // PT_with_clause
#include "sql/parser_yystype.h"
#include "sql/pfs_batch_mode.h"
#include "sql/protocol.h"
#include "sql/query_options.h"
#include "sql/query_term.h"
#include "sql/sql_base.h"  // fill_record
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"   // tmp tables
#include "sql/table_function.h"  // Table_function
#include "sql/thd_raii.h"
#include "sql/visible_fields.h"
#include "sql/window.h"  // Window
#include "template_utils.h"

using std::vector;

class Item_rollup_group_item;
class Item_rollup_sum_switcher;
class Opt_trace_context;

bool Query_result_union::prepare(THD *, const mem_root_deque<Item *> &,
                                 Query_expression *u) {
  unit = u;
  return false;
}

bool Query_result_union::send_data(THD *thd,
                                   const mem_root_deque<Item *> &values) {
  if (fill_record(thd, table, table->visible_field_ptr(), values, nullptr,
                  nullptr, false))
    return true; /* purecov: inspected */

  if (table->is_union_or_table() && !check_unique_fields(table)) return false;

  const int error = table->file->ha_write_row(table->record[0]);
  if (!error) {
    return false;
  }
  // create_ondisk_from_heap will generate error if needed
  if (!table->file->is_ignorable_error(error)) {
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, table, error, /*insert_last_record=*/true,
                                /*ignore_last_dup=*/true, &is_duplicate))
      return true; /* purecov: inspected */
    // Table's engine changed, index is not initialized anymore
    if (table->hash_field) table->file->ha_index_init(0, false);
  }
  return false;
}

bool Query_result_union::send_eof(THD *) { return false; }

bool Query_result_union::flush() { return false; }

/**
  Create a temporary table to store the result of a query expression
  (used, among others, when materializing a UNION DISTINCT).

  @param thd_arg            thread handle
  @param column_types       a list of items used to define columns of the
                            temporary table
  @param is_union_distinct  if set, the temporary table will eliminate
                            duplicates on insert
  @param options            create options
  @param table_alias        name of the temporary table
  @param bit_fields_as_long convert bit fields to ulonglong
  @param create_table If false, a table handler will not be created when
                      creating the result table.
  @param op                 If we are creating a result table for a set
                            operation, op should contain the relevant set
                            operation's query term. In other cases, op should
                            be nullptr.

  @details
    Create a temporary table that is used to store the result of a UNION,
    derived table, or a materialized cursor.

  @returns false if table created, true if error
*/

bool Query_result_union::create_result_table(
    THD *thd_arg, const mem_root_deque<Item *> &column_types,
    bool is_union_distinct, ulonglong options, const char *table_alias,
    bool bit_fields_as_long, bool create_table, Query_term_set_op *op) {
  mem_root_deque<Item *> visible_fields(thd_arg->mem_root);
  for (Item *item : VisibleFields(column_types)) {
    visible_fields.push_back(item);
  }

  assert(table == nullptr);
  tmp_table_param = Temp_table_param();
  count_field_types(thd_arg->lex->current_query_block(), &tmp_table_param,
                    visible_fields, false, true);
  tmp_table_param.skip_create_table = !create_table;
  tmp_table_param.bit_fields_as_long = bit_fields_as_long;
  if (unit != nullptr && op == nullptr) {
    if (unit->is_recursive()) {
      /*
        If the UNIQUE key specified for UNION DISTINCT were a primary key in
        InnoDB, rows would be returned by the scan in an order depending on
        their columns' values, not in insertion order.
      */
      tmp_table_param.can_use_pk_for_unique = false;
    }
    if (!unit->is_simple() && unit->can_materialize_directly_into_result()) {
      op = unit->set_operation();
    }
  }

  if (op == nullptr) {
    ;
  } else if (op->term_type() == QT_INTERSECT || op->term_type() == QT_EXCEPT) {
    tmp_table_param.m_operation = op->term_type() == QT_INTERSECT
                                      ? Temp_table_param::TTP_INTERSECT
                                      : Temp_table_param::TTP_EXCEPT;
    // No duplicate rows will exist after the last operation
    tmp_table_param.m_last_operation_is_distinct = op->last_distinct() > 0;
    assert((op->first_distinct() < std::numeric_limits<int64_t>::max() ||
            !tmp_table_param.m_last_operation_is_distinct) &&
           (op->first_distinct() <= op->last_distinct() ||
            op->first_distinct() == std::numeric_limits<int64_t>::max()));
    tmp_table_param.force_hash_field_for_unique = true;
  } else if (op->has_mixed_distinct_operators()) {
    // If we have mixed UNION DISTINCT / UNION ALL, we can't use an unique
    // index to deduplicate, as we need to be able to turn off deduplication
    // checking when we get to the UNION ALL part. The handler supports
    // turning off indexes (and the pre-iterator executor used this to
    // implement mixed DISTINCT/ALL), but not selectively, and we might very
    // well need the other indexes when querying against the table.
    // (Also, it would be nice to be able to remove this functionality
    // altogether from the handler.) Thus, we do it manually instead.
    tmp_table_param.force_hash_field_for_unique = true;
  }

  const bool use_setop_hashing =
      op != nullptr &&
      (op->term_type() == QT_EXCEPT || op->term_type() == QT_INTERSECT) &&
      thd_arg->optimizer_switch_flag(OPTIMIZER_SWITCH_HASH_SET_OPERATIONS);

  if (!(table = create_tmp_table(thd_arg, &tmp_table_param, visible_fields,
                                 nullptr, is_union_distinct, true, options,
                                 HA_POS_ERROR, table_alias)))
    return true;
  if (create_table) {
    table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table->hash_field != nullptr && !use_setop_hashing)
      table->file->ha_index_init(0, false);
  }
  return false;
}

/**
  Reset and empty the temporary table that stores the materialized query result.

  @note The cleanup performed here is exactly the same as for the two temp
  tables of JOIN - exec_tmp_table_[1 | 2].
*/

bool Query_result_union::reset() {
  return table ? table->empty_result_table() : false;
}

void Query_result_union::set_limit(ha_rows limit_rows) {
  table->m_limit_rows = limit_rows;
}
/**
  This class is effectively dead. It was used for non-DISTINCT UNIONs
  in the pre-iterator executor. Now it exists only as a shell for certain
  setup tasks, and should be removed.
*/
class Query_result_union_direct final : public Query_result_union {
 private:
  /// Result object that receives all rows
  Query_result *result;

  /// Wrapped result is optimized
  bool optimized;
  /// Wrapped result has started execution
  bool execution_started;

 public:
  Query_result_union_direct(Query_result *result, Query_block *last_query_block)
      : Query_result_union(),
        result(result),
        optimized(false),
        execution_started(false) {
    unit = last_query_block->master_query_expression();
  }
  bool change_query_result(THD *thd, Query_result *new_result) override;
  uint field_count(const mem_root_deque<Item *> &) const override {
    // Only called for top-level Query_results, usually Query_result_send
    assert(false); /* purecov: inspected */
    return 0;      /* purecov: inspected */
  }
  bool postponed_prepare(THD *thd,
                         const mem_root_deque<Item *> &types) override;
  bool send_result_set_metadata(THD *, const mem_root_deque<Item *> &,
                                uint) override {
    // Should never be called.
    my_abort();
  }
  bool send_data(THD *, const mem_root_deque<Item *> &) override {
    // Should never be called.
    my_abort();
  }
  bool start_execution(THD *thd) override {
    if (execution_started) return false;
    execution_started = true;
    return result->start_execution(thd);
  }
  bool send_eof(THD *) override {
    // Should never be called.
    my_abort();
  }
  bool flush() override { return false; }
  bool check_supports_cursor() const override {
    // Only called for top-level Query_results, usually Query_result_send
    assert(false); /* purecov: inspected */
    return false;  /* purecov: inspected */
  }
  void abort_result_set(THD *thd) override {
    result->abort_result_set(thd); /* purecov: inspected */
  }
  void cleanup() override {}
};

/**
  Replace the current query result with new_result and prepare it.

  @param thd        Thread handle
  @param new_result New query result

  @returns false if success, true if error
*/
bool Query_result_union_direct::change_query_result(THD *thd,
                                                    Query_result *new_result) {
  result = new_result;
  return result->prepare(thd, *unit->get_unit_column_types(), unit);
}

bool Query_result_union_direct::postponed_prepare(
    THD *thd, const mem_root_deque<Item *> &types) {
  if (result == nullptr) return false;

  return result->prepare(thd, types, unit);
}

bool Query_expression::can_materialize_directly_into_result() const {
  // There's no point in doing this if we're not already trying to materialize.
  if (!is_set_operation()) {
    return false;
  }

  // We can't materialize directly into the result if we have sorting.
  // Otherwise, we're fine.
  return global_parameters()->order_list.elements == 0;
}

/**
  Prepares all query blocks of a query expression.
  If a recursive query expression, this also creates the materialized temporary
  table.

  @param thd           Thread handler
  @param sel_result    Result object where the unit's output should go.
  @param insert_field_list Pointer to field list if INSERT op, NULL otherwise.
  @param added_options These options will be added to the query blocks.
  @param removed_options Options that cannot be used for this query

  @returns false if success, true if error
 */

bool Query_expression::prepare(THD *thd, Query_result *sel_result,
                               mem_root_deque<Item *> *insert_field_list,
                               ulonglong added_options,
                               ulonglong removed_options) {
  DBUG_TRACE;

  assert(!is_prepared());
  Change_current_query_block save_query_block(thd);

  set_query_result(sel_result);

  thd->lex->set_current_query_block(first_query_block());

  if (is_simple()) {
    // Only one query block. No extra result needed:
    query_term()->query_block()->set_query_result(sel_result);
  } else {
    set_operation()->set_is_materialized(
        query_term()->term_type() != QT_UNION ||
        set_operation()->last_distinct() > 0 ||
        global_parameters()->order_list.elements > 0 ||
        ((thd->lex->sql_command == SQLCOM_INSERT_SELECT ||
          thd->lex->sql_command == SQLCOM_REPLACE_SELECT) &&
         thd->lex->unit == this));
    /*
      There exists a query block that consolidates the result, so
      no need to buffer bottom level query block's result.
    */
    added_options &= ~OPTION_BUFFER_RESULT;
  }

  first_query_block()->context.resolve_in_select_list = true;

  ulonglong create_options =
      first_query_block()->active_options() | TMP_TABLE_ALL_COLUMNS;

  if (query_term()->prepare_query_term(thd, this, &save_query_block,
                                       insert_field_list,
                                       /*common_result*/ nullptr, added_options,
                                       removed_options, create_options))
    return true;

  if (is_recursive()) {
    // This had to wait until all query blocks are prepared:
    if (check_materialized_derived_query_blocks(thd))
      return true; /* purecov: inspected */
  }

  // Query blocks are prepared, update the state
  set_prepared();

  return false;
}

/// Finalizes the initialization of all the full-text functions used in the
/// given query expression, and recursively in every query expression inner to
/// the given one. We do this fairly late, since we need to know whether or not
/// the full-text function is to be used for a full-text index scan, and whether
/// or not that scan is sorted. When the iterators have been created, we know
/// that the final decision has been made, so we do it right after the iterators
/// have been created.
static bool finalize_full_text_functions(THD *thd,
                                         Query_expression *query_expression) {
  assert(thd->lex->using_hypergraph_optimizer());
  for (Query_expression *qe = query_expression; qe != nullptr;
       qe = qe->next_query_expression()) {
    for (Query_block *qb = qe->first_query_block(); qb != nullptr;
         qb = qb->next_query_block()) {
      if (qb->has_ft_funcs()) {
        if (init_ftfuncs(thd, qb)) {
          return true;
        }
      }
      if (finalize_full_text_functions(thd,
                                       qb->first_inner_query_expression())) {
        return true;
      }
    }
  }
  return false;
}

/**
  Determine if we should set or add the contribution of the given query block to
  the total row count estimate for the query expression.
  If we have INTERSECT or EXCEPT, only set row estimate for left side since
  the total number of rows in the result set can only decrease as a result
  of the set operation.
  @param qb query block
  @return true if the estimate should be added
*/
static bool contributes_to_rowcount_estimate(Query_block *qb) {
  if (qb->parent() == nullptr) return true;

  // When parent isn't nullptr, we know this is a leaf block.
  Query_term *query_term = qb;

  // See if this query block is contained in a right side of an INTERSECT
  // or EXCEPT operation anywhere in tree. If so, we can ignore its count.
  Query_term_set_op *parent = query_term->parent();
  while (parent != nullptr) {
    if (parent->in_right_side_in_except_or_intersect(query_term)) return false;
    // Even if we are on the left side of an INTERSECT, EXCEPT, the set
    // operation itself could still be in a non-contributing side higher up, so
    // continue checking.
    query_term = parent;
    parent = parent->parent();
  }
  return true;
}

static bool use_iterator(TABLE *materialize_destination,
                         Query_term *query_term) {
  if (materialize_destination == nullptr) return false;

  switch (query_term->term_type()) {
    case QT_INTERSECT:
    case QT_EXCEPT:
      // In corner cases for transform of scalar subquery, it can happen
      // that the destination table isn't ready for INTERSECT or EXCEPT, so
      // force double materialization.
      // FIXME: find out if we can remove this exception.
      return materialize_destination->is_union_or_table();
    default:;
  }
  return false;
}

bool Query_expression::optimize(THD *thd, TABLE *materialize_destination,
                                bool finalize_access_paths) {
  DBUG_TRACE;
  assert(is_prepared() && !is_optimized());

  Change_current_query_block save_query_block(thd);

  ha_rows estimated_rowcount = 0;
  double estimated_cost = 0.0;

  if (query_result() != nullptr) query_result()->estimated_rowcount = 0;

  for (Query_block *query_block = first_query_block(); query_block != nullptr;
       query_block = query_block->next_query_block()) {
    thd->lex->set_current_query_block(query_block);

    // LIMIT is required for optimization
    if (set_limit(thd, query_block)) return true; /* purecov: inspected */

    if (query_block->optimize(thd, finalize_access_paths)) return true;

    /*
      Accumulate estimated number of rows.
      1. Implicitly grouped query has one row (with HAVING it has zero or one
         rows).
      2. If GROUP BY clause is optimized away because it was a constant then
         query produces at most one row.
     */
    if (contributes_to_rowcount_estimate(query_block))
      estimated_rowcount += (query_block->is_implicitly_grouped() ||
                             query_block->join->group_optimized_away)
                                ? 1
                                : query_block->join->best_rowcount;

    estimated_cost += query_block->join->best_read;

    // Table_ref::fetch_number_of_rows() expects to get the number of rows
    // from all earlier query blocks from the query result, so we need to update
    // it as we go. In particular, this is used when optimizing a recursive
    // SELECT in a CTE, so that it knows how many rows the non-recursive query
    // blocks will produce.
    //
    // TODO(sgunders): Communicate this in a different way when the query result
    // goes away.
    if (query_result() != nullptr) {
      query_result()->estimated_rowcount = estimated_rowcount;
      query_result()->estimated_cost = estimated_cost;
    }
  }

  if (!is_simple() && query_term()->open_result_tables(thd, 0)) return true;

  if ((uncacheable & UNCACHEABLE_DEPENDENT) && estimated_rowcount <= 1) {
    /*
      This depends on outer references, so optimization cannot assume that all
      executions will always produce the same row. So, increase the counter to
      prevent that this table is replaced with a constant.
      Not testing all bits of "uncacheable", as if derived table sets user
      vars (UNCACHEABLE_SIDEEFFECT) the logic above doesn't apply.
    */
    estimated_rowcount = PLACEHOLDER_TABLE_ROW_ESTIMATE;
  }

  if (!is_simple()) {
    if (query_term()->optimize_query_term(thd, this)) return true;
    if (set_limit(thd, query_term()->query_block())) return true;
    if (!is_union()) query_result()->set_limit(select_limit_cnt);
  }

  query_result()->estimated_rowcount = estimated_rowcount;
  query_result()->estimated_cost = estimated_cost;

  // If the caller has asked for materialization directly into a table of its
  // own, and we can do so, do an unfinished materialization (see the comment
  // on this function for more details).
  if (thd->lex->m_sql_cmd != nullptr &&
      thd->lex->m_sql_cmd->using_secondary_storage_engine()) {
    // Not supported when using secondary storage engine.
    create_access_paths(thd);
  } else if (estimated_rowcount <= 1 ||
             use_iterator(materialize_destination, query_term())) {
    // Don't do it for const tables, as for those, optimize_derived() wants to
    // run the query during optimization, and thus needs an iterator.
    //
    // Do note that JOIN::extract_func_dependent_tables() can want to read from
    // the derived table during the optimization phase even if it has
    // estimated_rowcount larger than one (e.g., because it understands it can
    // get only one row due to a unique index), but will detect that the table
    // has not been created, and treat the the lookup as non-const.
    create_access_paths(thd);
  } else if (materialize_destination != nullptr &&
             can_materialize_directly_into_result()) {
    assert(!is_simple());
    const bool calc_found_rows =
        (first_query_block()->active_options() & OPTION_FOUND_ROWS);
    m_operands = set_operation()->setup_materialize_set_op(
        thd, materialize_destination,
        /*union_distinct_only=*/false, calc_found_rows);
  } else {
    // Recursive CTEs expect to see the rows in the result table immediately
    // after writing them.
    assert(!is_recursive());
    create_access_paths(thd);
  }

  set_optimized();  // All query blocks optimized, update the state

  if (item != nullptr) {
    // If we're part of an IN subquery, the containing engine may want to
    // add its own iterators on top, e.g. to materialize us.
    //
    // TODO(sgunders): See if we can do away with the engine concept
    // altogether, now that there's much less execution logic in them.
    assert(!unfinished_materialization());
    item->create_iterators(thd);
    if (m_root_access_path == nullptr) {
      return false;
    }
  }

  // When done with the outermost query expression, and if max_join_size is in
  // effect, estimate the total number of row accesses in the query, and error
  // out if it exceeds max_join_size.
  if (outer_query_block() == nullptr &&
      !Overlaps(thd->variables.option_bits, OPTION_BIG_SELECTS) &&
      !thd->lex->is_explain() &&
      EstimateRowAccesses(m_root_access_path, /*num_evaluations=*/1.0,
                          std::numeric_limits<double>::infinity()) >
          static_cast<double>(thd->variables.max_join_size)) {
    my_error(ER_TOO_BIG_SELECT, MYF(0));
    return true;
  }

  return false;
}

bool Query_expression::finalize(THD *thd) {
  for (Query_block *query_block = first_query_block(); query_block != nullptr;
       query_block = query_block->next_query_block()) {
    if (query_block->join != nullptr && query_block->join->needs_finalize) {
      if (FinalizePlanForQueryBlock(thd, query_block)) {
        return true;
      }
    }
  }
  return false;
}

bool Query_expression::force_create_iterators(THD *thd) {
  assert(IteratorsAreNeeded(thd, m_root_access_path));
  if (m_root_iterator == nullptr) {
    return create_iterators(thd);
  }
  return false;
}

bool Query_expression::create_iterators(THD *thd) {
  assert(m_root_iterator == nullptr);

  if (!IteratorsAreNeeded(thd, m_root_access_path)) {
    return false;
  }

  JOIN *const top_join = query_term()->query_block()->join;
  m_root_iterator =
      CreateIteratorFromAccessPath(thd, m_root_access_path, top_join,
                                   /*eligible_for_batch_mode=*/true);

  if (m_root_iterator == nullptr) return true;

  if (thd->lex->using_hypergraph_optimizer()) {
    if (finalize_full_text_functions(thd, this)) {
      return true;
    }
  }

  return false;
}

#ifndef NDEBUG
void Query_expression::DebugPrintQueryPlan(THD *thd,
                                           const char *keyword) const {
  DBUG_PRINT(keyword, ("\n%s", thd->query().str));
  if (m_query_term != nullptr) {
    std::ostringstream buf;
    m_query_term->debugPrint(0, buf);
    DBUG_PRINT(keyword, ("\n%s", buf.str().c_str()));
  }

  JOIN *const join = query_term()->query_block()->join;
  const bool is_root_of_join = join != nullptr;
  DBUG_PRINT(
      keyword,
      ("Query plan:\n%s\n",
       PrintQueryPlan(0, m_root_access_path, join, is_root_of_join).c_str()));
}
#endif

/**
  Make materialization parameters for a query block given its input path
  and destination table,
  @param child_path the input access path
  @param dst_table  the table to materialize into

  @returns materialization parameter
*/
MaterializePathParameters::Operand Query_block::setup_materialize_query_block(
    AccessPath *child_path, TABLE *dst_table) const {
  ConvertItemsToCopy(*join->fields, dst_table->visible_field_ptr(),
                     &join->tmp_table_param);

  MaterializePathParameters::Operand operand;
  operand.subquery_path = child_path;
  operand.select_number = select_number;
  operand.join = join;
  operand.disable_deduplication_by_hash_field = false;
  operand.copy_items = true;
  operand.temp_table_param = &join->tmp_table_param;
  operand.is_recursive_reference = recursive_reference != nullptr;
  return operand;
}

/**
   Sets up each(*) query block in this query expression for materialization
   into the given table by making a materialization parameter for each block
   (*) modulo union_distinct_only.

   @param thd       session context
   @param dst_table the table to materialize into
   @param union_distinct_only
                    if true, materialize only UNION DISTINCT query blocks
     (any UNION ALL blocks are presumed handled higher up, by AppendIterator)
   @param calc_found_rows
                    if true, calculate rows found
   @returns array of materialization parameters
*/
Mem_root_array<MaterializePathParameters::Operand>
Query_term_set_op::setup_materialize_set_op(THD *thd, TABLE *dst_table,
                                            bool union_distinct_only,
                                            bool calc_found_rows) {
  Mem_root_array<MaterializePathParameters::Operand> operands(thd->mem_root);

  int64 idx = -1;
  for (Query_term *term : m_children) {
    ++idx;
    bool activate_deduplication =
        idx <= m_last_distinct ||
        term_type() != QT_UNION; /* always for INTERSECT and EXCEPT */
    JOIN *join = term->query_block()->join;
    AccessPath *child_path = join->root_access_path();
    assert(join->is_optimized() && child_path != nullptr);

    if (term->term_type() != QT_QUERY_BLOCK)
      child_path =
          term->make_set_op_access_path(thd, nullptr, nullptr, calc_found_rows);

    MaterializePathParameters::Operand param =
        term->query_block()->setup_materialize_query_block(child_path,
                                                           dst_table);
    param.m_first_distinct = m_first_distinct;
    param.m_operand_idx = idx;
    param.m_total_operands = m_children.size();
    param.disable_deduplication_by_hash_field =
        (has_mixed_distinct_operators() && !activate_deduplication);
    operands.push_back(param);

    if (idx == m_last_distinct && idx > 0 && union_distinct_only)
      // The rest will be done by appending.
      break;
  }
  return operands;
}

void Query_expression::create_access_paths(THD *thd) {
  if (is_simple()) {
    JOIN *join = first_query_block()->join;
    assert(join && join->is_optimized());
    m_root_access_path = join->root_access_path();
    return;
  }

  // Decide whether we can stream rows, ie., never actually put them into the
  // temporary table. If we can, we materialize the UNION DISTINCT blocks first,
  // and then stream the remaining UNION ALL blocks (if any) by means of
  // AppendIterator.
  //
  // If we cannot stream (ie., everything has to go into the temporary table),
  // our strategy for mixed UNION ALL/DISTINCT becomes a bit different;
  // see MaterializeIterator for details.
  bool streaming_allowed = true;
  if (global_parameters()->order_list.size() != 0 ||
      (!is_simple() && set_operation()->is_materialized())) {
    // If we're sorting, we currently put it in a real table no matter what.
    // This is a legacy decision, because we used to not know whether filesort
    // would want to refer to rows in the table after the sort (sort by row ID).
    // We could probably be more intelligent here now.
    streaming_allowed = false;
  } else if ((thd->lex->sql_command == SQLCOM_INSERT_SELECT ||
              thd->lex->sql_command == SQLCOM_REPLACE_SELECT) &&
             thd->lex->unit == this) {
    // If we're doing an INSERT or REPLACE, and we're not outputting to
    // a temporary table already (ie., we are the topmost unit), then we
    // don't want to insert any records before we're done scanning. Otherwise,
    // we would risk incorrect results and/or infinite loops, as we'd be seeing
    // our own records as they get inserted.
    //
    // @todo Figure out if we can check for OPTION_BUFFER_RESULT instead;
    //       see bug #23022426.
    streaming_allowed = false;
  }

  ha_rows offset = global_parameters()->get_offset(thd);
  ha_rows limit = global_parameters()->get_limit(thd);
  if (limit + offset >= limit)
    limit += offset;
  else
    limit = HA_POS_ERROR; /* purecov: inspected */
  const bool calc_found_rows =
      (first_query_block()->active_options() & OPTION_FOUND_ROWS);

  Mem_root_array<AppendPathParameters> *union_all_sub_paths =
      new (thd->mem_root) Mem_root_array<AppendPathParameters>(thd->mem_root);

  // If streaming is allowed, we can do all the parts that are UNION ALL by
  // streaming; the rest have to go to the table.
  //
  // Handle the query blocks that we need to materialize. This may be
  // UNION DISTINCT query blocks only, or all blocks.
  if (!streaming_allowed || !is_simple()) {
    AppendPathParameters param;
    param.path = m_query_term->make_set_op_access_path(
        thd, /*parent*/ nullptr,
        streaming_allowed ? union_all_sub_paths : nullptr, calc_found_rows);
    param.join = nullptr;
    if (!streaming_allowed) union_all_sub_paths->push_back(param);
    // else filled in by make_set_op_access_path
  }

  assert(!union_all_sub_paths->empty());
  if (union_all_sub_paths->size() == 1) {
    m_root_access_path = (*union_all_sub_paths)[0].path;
  } else {
    // Just append all the UNION ALL sub-blocks.
    assert(streaming_allowed);
    m_root_access_path = NewAppendAccessPath(thd, union_all_sub_paths);
  }

  // NOTE: If there's a fake_query_block, its JOIN's iterator already handles
  // LIMIT/OFFSET, so we don't do it again here.
  if (streaming_allowed && (limit != HA_POS_ERROR || offset != 0) &&
      (is_simple() || set_operation()->last_distinct() == 0)) {
    m_root_access_path = NewLimitOffsetAccessPath(
        thd, m_root_access_path, limit, offset, calc_found_rows,
        /*reject_multiple_rows=*/false, &send_records);
  }
}

bool Query_expression::explain_query_term(THD *explain_thd,
                                          const THD *query_thd,
                                          Query_term *qt) {
  Explain_format *fmt = explain_thd->lex->explain_format;
  switch (qt->term_type()) {
    case QT_QUERY_BLOCK:
      if (fmt->begin_context(CTX_QUERY_SPEC)) return true;
      if (explain_query_specification(explain_thd, query_thd, qt, CTX_JOIN))
        return true;
      if (fmt->end_context(CTX_QUERY_SPEC)) return true;
      break;
    case QT_UNION: {
      if (fmt->begin_context(CTX_UNION)) return true;
      Query_term_set_op *const qts = down_cast<Query_term_set_op *>(qt);
      for (size_t idx = 0; idx < qts->child_count(); ++idx) {
        if (explain_query_term(explain_thd, query_thd, qts->child(idx)))
          return true;
      }
      if (down_cast<Query_term_union *>(qt)->is_materialized() &&
          explain_query_specification(explain_thd, query_thd, qt,
                                      CTX_UNION_RESULT))
        return true;
      if (fmt->end_context(CTX_UNION)) return true;
    } break;
    case QT_INTERSECT: {
      if (fmt->begin_context(CTX_INTERSECT)) return true;
      Query_term_set_op *const qts = down_cast<Query_term_set_op *>(qt);
      for (size_t idx = 0; idx < qts->child_count(); ++idx) {
        if (explain_query_term(explain_thd, query_thd, qts->child(idx)))
          return true;
      }
      if (explain_query_specification(explain_thd, query_thd, qt,
                                      CTX_INTERSECT_RESULT))
        return true;
      if (fmt->end_context(CTX_INTERSECT)) return true;
    } break;
    case QT_EXCEPT: {
      if (fmt->begin_context(CTX_EXCEPT)) return true;
      Query_term_set_op *const qts = down_cast<Query_term_set_op *>(qt);
      for (size_t idx = 0; idx < qts->child_count(); ++idx) {
        if (explain_query_term(explain_thd, query_thd, qts->child(idx)))
          return true;
      }
      if (explain_query_specification(explain_thd, query_thd, qt,
                                      CTX_EXCEPT_RESULT))
        return true;
      if (fmt->end_context(CTX_EXCEPT)) return true;
    } break;
    case QT_UNARY: {
      if (fmt->begin_context(CTX_UNARY)) return true;
      Query_term_set_op *const qts = down_cast<Query_term_set_op *>(qt);
      if (explain_query_term(explain_thd, query_thd, qts->child(0)))
        return true;
      if (explain_query_specification(explain_thd, query_thd, qt,
                                      CTX_UNARY_RESULT))
        return true;
      if (fmt->end_context(CTX_UNARY)) return true;

    } break;
  }

  return false;
}
/**
  Explain query starting from this unit.

  @param explain_thd thread handle for the connection doing explain
  @param query_thd   thread handle for the connection being explained

  @return false if success, true if error
*/

bool Query_expression::explain(THD *explain_thd, const THD *query_thd) {
  DBUG_TRACE;

#ifndef NDEBUG
  Query_block *lex_select_save = query_thd->lex->current_query_block();
#endif
  const bool other = (query_thd != explain_thd);

  assert(other || is_optimized() || outer_query_block()->is_empty_query() ||
         // @todo why is this necessary?
         outer_query_block()->join == nullptr ||
         outer_query_block()->join->zero_result_cause);

  if (explain_query_term(explain_thd, query_thd, query_term())) return true;
  if (!other)
    assert(current_thd->lex->current_query_block() == lex_select_save);
  return false;
}

bool Common_table_expr::clear_all_references() {
  bool reset_tables = false;
  for (Table_ref *tl : references) {
    if (tl->table &&
        tl->derived_query_expression()->uncacheable & UNCACHEABLE_DEPENDENT) {
      reset_tables = true;
      if (tl->derived_query_expression()->query_result()->reset()) return true;
    }
    /*
      This loop has found all non-recursive clones; one writer and N
      readers.
    */
  }
  if (!reset_tables) return false;
  for (Table_ref *tl : tmp_tables) {
    if (tl->is_derived()) continue;  // handled above
    if (tl->table->empty_result_table()) return true;
    // This loop has found all recursive clones (only readers).
  }
  /*
    Above, emptying all clones is necessary, to rewind every handler (cursor)
    to the table's start. Setting materialized=false on all is also important
    or the writer would skip materialization, see loop at start of
    Table_ref::materialize_derived()). There is one "recursive table"
    which we don't find here: it's the UNION DISTINCT tmp table. It's reset in
    unit::execute() of the unit which is the body of the CTE.
  */
  return false;
}

/**
  Empties all correlated query blocks defined within the query expression;
  that is, correlated CTEs defined in the expression's WITH clause, and
  correlated derived tables.
 */
bool Query_expression::clear_correlated_query_blocks() {
  for (Query_block *sl = first_query_block(); sl; sl = sl->next_query_block()) {
    sl->join->clear_corr_derived_tmp_tables();
    sl->join->clear_sj_tmp_tables();
    sl->join->clear_hash_tables();
  }
  if (!m_with_clause) return false;
  for (auto el : m_with_clause->m_list->elements()) {
    Common_table_expr &cte = el->m_postparse;
    if (cte.clear_all_references()) return true;
  }
  return false;
}

bool Query_expression::ClearForExecution() {
  if (is_executed()) {
    if (clear_correlated_query_blocks()) return true;

    // TODO(sgunders): Most of JOIN::reset() should be done in iterators.
    for (auto qt : query_terms<QTC_POST_ORDER>()) {
      if (qt->term_type() == QT_QUERY_BLOCK ||
          down_cast<Query_term_set_op *>(qt)->is_materialized()) {
        Query_block *sl = qt->query_block();
        if (sl->join->is_executed()) {
          sl->join->reset();
        }
      }
    }
  }

  for (Query_block *query_block = first_query_block(); query_block;
       query_block = query_block->next_query_block()) {
    JOIN *join = query_block->join;
    query_block->join->examined_rows = 0;
    query_block->join
        ->set_executed();  // The dynamic range optimizer expects this.

    // TODO(sgunders): Consider doing this in some iterator instead.
    if (join->m_windows.elements > 0 && !join->m_windowing_steps) {
      // Initialize state of window functions as end_write_wf() will be
      // shortcut
      for (Window &w : query_block->join->m_windows) {
        w.reset_all_wf_state();
      }
    }
  }
  return false;
}

bool Query_expression::ExecuteIteratorQuery(THD *thd) {
  THD_STAGE_INFO(thd, stage_executing);
  DEBUG_SYNC(thd, "before_join_exec");

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "join_execution");
  if (is_simple()) {
    trace_exec.add_select_number(first_query_block()->select_number);
  }
  Opt_trace_array trace_steps(trace, "steps");

  if (ClearForExecution()) {
    return true;
  }

  mem_root_deque<Item *> *fields = get_field_list();
  Query_result *query_result = this->query_result();
  assert(query_result != nullptr);

  if (query_result->start_execution(thd)) return true;

  if (query_result->send_result_set_metadata(
          thd, *fields, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    return true;
  }

  set_executed();

  // Hand over the query to the secondary engine if needed.
  if (first_query_block()->join->override_executor_func != nullptr) {
    thd->current_found_rows = 0;
    for (Query_block *select = first_query_block(); select != nullptr;
         select = select->next_query_block()) {
      if (select->join->override_executor_func(select->join, query_result)) {
        return true;
      }
      thd->current_found_rows += select->join->send_records;
    }
    const bool calc_found_rows =
        (first_query_block()->active_options() & OPTION_FOUND_ROWS);
    if (!calc_found_rows) {
      // This is for backwards compatibility reasons only;
      // we have documented that without SQL_CALC_FOUND_ROWS,
      // we return the actual number of rows returned.
      thd->current_found_rows =
          std::min(thd->current_found_rows, select_limit_cnt);
    }
    return query_result->send_eof(thd);
  }

  if (item != nullptr) {
    item->reset_has_values();

    if (item->is_value_assigned()) {
      item->reset_value_assigned();  // Prepare for re-execution of this unit
      item->reset();
    }
  }

  // We need to accumulate in the first join's send_records as long as
  // we support SQL_CALC_FOUND_ROWS, since LimitOffsetIterator will use it
  // for reporting rows skipped by OFFSET or LIMIT. When we get rid of
  // SQL_CALC_FOUND_ROWS, we can use a local variable here instead.
  ha_rows *send_records_ptr;
  if (is_simple()) {
    // Not a UNION: found_rows() applies to the join.
    // LimitOffsetIterator will write skipped OFFSET rows into the JOIN's
    // send_records, so use that.
    send_records_ptr = &first_query_block()->join->send_records;
  } else if (set_operation()->is_materialized()) {
    send_records_ptr = &query_term()->query_block()->join->send_records;
  } else {
    // UNION, but without a fake_query_block (may or may not have a
    // LIMIT): found_rows() applies to the outermost block. See
    // Query_expression::send_records for more information.
    send_records_ptr = &send_records;
  }
  *send_records_ptr = 0;

  thd->get_stmt_da()->reset_current_row_for_condition();

  {
    auto join_cleanup = create_scope_guard([this, thd] {
      for (Query_block *sl = first_query_block(); sl;
           sl = sl->next_query_block()) {
        JOIN *join = sl->join;
        join->join_free();
        thd->inc_examined_row_count(join->examined_rows);
      }
      if (!is_simple() && set_operation()->is_materialized())
        thd->inc_examined_row_count(
            query_term()->query_block()->join->examined_rows);
    });

    if (m_root_iterator->Init()) {
      return true;
    }

    PFSBatchMode pfs_batch_mode(m_root_iterator.get());

    for (;;) {
      int error = m_root_iterator->Read();
      DBUG_EXECUTE_IF("bug13822652_1", thd->killed = THD::KILL_QUERY;);

      if (error > 0 || thd->is_error())  // Fatal error
        return true;
      else if (error < 0)
        break;
      else if (thd->killed)  // Aborted by user
      {
        thd->send_kill_message();
        return true;
      }

      ++*send_records_ptr;

      if (query_result->send_data(thd, *fields)) {
        return true;
      }
      thd->get_stmt_da()->inc_current_row_for_condition();

      DBUG_EXECUTE_IF("simulate_partial_result_set_scenario",
                      my_error(ER_UNKNOWN_ERROR, MYF(0)););
    }

    // NOTE: join_cleanup must be done before we send EOF, so that we get the
    // row counts right.
  }

  thd->current_found_rows = *send_records_ptr;

  return query_result->send_eof(thd);
}

/**
  Execute a query expression that may be a UNION and/or have an ordered result.

  @param thd          thread handle

  @returns false if success, true if error
*/

bool Query_expression::execute(THD *thd) {
  DBUG_TRACE;
  assert(is_optimized());

  if (is_executed() && !uncacheable) return false;

  assert(!unfinished_materialization());

  // Collect information about the chosen query plan.
  if (!is_executed()) {
    CollectStatusVariables(thd, query_term()->query_block()->join,
                           *root_access_path());
  }

  /*
    Even if we return "true" the statement might continue
    (e.g. ER_SUBQUERY_1_ROW in stmt with IGNORE), so we want to restore
    current_query_block():
  */
  Change_current_query_block save_query_block(thd);

  return ExecuteIteratorQuery(thd);
}

/**
  Cleanup this query expression object after preparation or one round
  of execution. After the cleanup, the object can be reused for a
  new round of execution, but a new optimization will be needed before
  the execution.
*/

void Query_expression::cleanup(bool full) {
  DBUG_TRACE;

  if (cleaned >= (full ? UC_CLEAN : UC_PART_CLEAN)) return;

  cleaned = (full ? UC_CLEAN : UC_PART_CLEAN);

  if (full) {
    clear_root_access_path();
  }

  m_operands.clear();

  for (auto qt : query_terms<QTC_PRE_ORDER>()) {
    if (qt->term_type() == QT_QUERY_BLOCK && slave == nullptr)
      continue;  // already invalidated
    // post order fails here for corner case SELECT 1 UNION SELECT 1 LIMIT 0
    qt->cleanup(full);
  }
  // subselect_hash_sj_engine may hold iterators that need to be cleaned up
  // before the MEM_ROOT goes away.
  if (item != nullptr) {
    item->cleanup();
  }

  /*
    explain_marker is (mostly) a property determined at prepare time and must
    thus be preserved for the next execution, if this is a prepared statement.
  */
}

void Query_expression::destroy() {
  /*
    @todo WL#6570 This is incomplete:
    - It does not handle the case where a UNIT is prepared (success or error)
      and not cleaned.
    - It does not handle the case where a UNIT is optimized with error
      and not cleaned.
  */
  assert(!is_optimized() || cleaned == UC_CLEAN);

  for (auto qt : query_terms<QTC_PRE_ORDER>()) {
    if (qt->owning_operand() && qt->setop_query_result() != nullptr &&
        qt->setop_query_result_union()->table != nullptr) {
      // Destroy materialized result set for a set operation
      free_tmp_table(qt->setop_query_result_union()->table);
      qt->result_table().table = nullptr;
    }
    qt->query_block()->destroy();
  }
  m_query_term->destroy_tree();
  m_query_term = nullptr;
  invalidate();
}

#ifndef NDEBUG
void Query_expression::assert_not_fully_clean() {
  assert(cleaned < UC_CLEAN);
  Query_block *sl = first_query_block();
  for (;;) {
    if (!sl) {
      if (is_simple())
        break;
      else
        sl = query_term()->query_block();
    }
    for (Query_expression *lex_query_expression =
             sl->first_inner_query_expression();
         lex_query_expression;
         lex_query_expression = lex_query_expression->next_query_expression())
      lex_query_expression->assert_not_fully_clean();
    if (!is_simple() && sl == query_term()->query_block())
      break;
    else
      sl = sl->next_query_block();
  }
}
#endif

/**
  Change the query result object used to return the final result of
  the unit, replacing occurrences of old_result with new_result.

  @param thd        Thread handle
  @param new_result New query result object
  @param old_result Old query result object

  @retval false Success
  @retval true  Error
*/

bool Query_expression::change_query_result(
    THD *thd, Query_result_interceptor *new_result,
    Query_result_interceptor *old_result) {
  for (Query_block *sl = first_query_block(); sl; sl = sl->next_query_block()) {
    if (sl->query_result() &&
        sl->change_query_result(thd, new_result, old_result))
      return true; /* purecov: inspected */
  }
  set_query_result(new_result);
  return false;
}

/**
  Get column type information for this query expression.

  For a single query block the column types are taken from the list
  of selected items of this block.

  For a (binary) set operations, this function assumes that the type holders
  were created for unioned column types of all query blocks, by
  \c Query_expression::prepare().

  For unary query terms (\c QT_UNARY), we use return child terms's type
  information.

  @returns List of items as specified in function description.
    May contain hidden fields (item->hidden = true), so the caller
    needs to be able to filter them out.
*/

mem_root_deque<Item *> *Query_expression::get_unit_column_types() {
  return query_term()->types_array();
}

size_t Query_expression::num_visible_fields() const {
  return query_term()->visible_column_count();
}

/**
  Get field list for this query expression.

  For a UNION of query blocks, return the field list generated during prepare.
  For a single query block, return the field list after all possible
  intermediate query processing steps are done (optimization is complete).

  @returns List containing fields of the query expression.
*/

mem_root_deque<Item *> *Query_expression::get_field_list() {
  assert(is_optimized());

  if (is_simple()) {
    return down_cast<Query_block *>(query_term())->join->fields;
  }
  if (set_operation()->is_materialized()) {
    return query_term()->query_block()->join->fields;
  }
  return query_term()->fields();
}

bool Query_expression::walk(Item_processor processor, enum_walk walk,
                            uchar *arg) {
  for (auto qt : query_terms<>())
    if (qt->query_block()->walk(processor, walk, arg)) return true;

  return false;
}

void Query_expression::change_to_access_path_without_in2exists(THD *thd) {
  for (Query_block *select = first_query_block(); select != nullptr;
       select = select->next_query_block()) {
    select->join->change_to_access_path_without_in2exists();
  }
  create_access_paths(thd);
}

/**
  Closes (and, if last reference, drops) temporary tables created to
  materialize derived tables, schema tables and CTEs.

  @param list List of tables to search in
*/
static void cleanup_tmp_tables(Table_ref *list) {
  for (auto tl = list; tl; tl = tl->next_local) {
    if (tl->merge_underlying_list) {
      // Find a materialized view inside another view.
      cleanup_tmp_tables(tl->merge_underlying_list);
    } else if (tl->is_table_function()) {
      tl->table_function->cleanup();
    }
    if (tl->table == nullptr) continue;  // Not materialized
    if ((tl->is_view_or_derived() || tl->is_recursive_reference() ||
         tl->schema_table || tl->is_table_function())) {
      close_tmp_table(tl->table);
      if (tl->schema_table) {
        free_tmp_table(tl->table);  // Schema tables are per execution
        tl->table = nullptr;
      } else {
        // Clear indexes added during optimization, keep possible unique index
        TABLE *t = tl->table;
        t->s->keys = t->s->is_distinct ? 1 : 0;
        t->s->first_unused_tmp_key = 0;
        t->keys_in_use_for_query.clear_all();
        t->keys_in_use_for_group_by.clear_all();
        t->keys_in_use_for_order_by.clear_all();
      }
    }
  }
}

/**
   Destroy temporary tables created to materialize derived tables,
   schema tables and CTEs.

   @param list List of tables to search in
*/
static void destroy_tmp_tables(Table_ref *list) {
  for (auto tl = list; tl; tl = tl->next_local) {
    if (tl->merge_underlying_list) {
      // Find a materialized view inside another view.
      destroy_tmp_tables(tl->merge_underlying_list);
    } else if (tl->is_table_function()) {
      tl->table_function->destroy();
    }
    // If this table has a reference to CTE, we need to remove it.
    if (tl->common_table_expr() != nullptr) {
      tl->common_table_expr()->references.erase_value(tl);
    }
    if (tl->table == nullptr) continue;  // Not materialized
    assert(tl->schema_table == nullptr);
    if (tl->is_view_or_derived() || tl->is_recursive_reference() ||
        tl->schema_table || tl->is_table_function()) {
      free_tmp_table(tl->table);
      tl->table = nullptr;
    }
  }
}

/**
  Cleanup after preparation of one round of execution.
*/
void Query_block::cleanup(bool full) {
  cleanup_query_result(full);

  if (join) {
    if (full) {
      assert(join->query_block == this);
      join->destroy();
      ::destroy_at(join);
      join = nullptr;
    } else
      join->cleanup();
  }

  if (full) {
    cleanup_tmp_tables(get_table_list());
    if (hidden_items_from_optimization > 0) remove_hidden_items();
    if (m_windows.elements > 0) {
      List_iterator<Window> li(m_windows);
      Window *w;
      while ((w = li++)) w->cleanup();
    }
  }

  for (Query_expression *qe = first_inner_query_expression(); qe != nullptr;
       qe = qe->next_query_expression()) {
    qe->cleanup(full);
  }
}

void Query_block::cleanup_all_joins() {
  if (join) join->cleanup();

  for (Query_expression *unit = first_inner_query_expression(); unit;
       unit = unit->next_query_expression()) {
    for (Query_block *sl = unit->first_query_block(); sl;
         sl = sl->next_query_block())
      sl->cleanup_all_joins();
  }
}

void Query_block::destroy() {
  Query_expression *unit = first_inner_query_expression();
  while (unit != nullptr) {
    Query_expression *next = unit->next_query_expression();
    unit->destroy();
    unit = next;
  }

  List_iterator<Window> li(m_windows);
  Window *w;
  while ((w = li++)) w->destroy();

  // Destroy allocated derived tables
  destroy_tmp_tables(get_table_list());

  // Our destructor is not called, so we need to make sure
  // all the memory for these arrays is freed.
  if (olap == ROLLUP_TYPE) {
    rollup_group_items.clear();
    rollup_group_items.shrink_to_fit();
    rollup_sums.clear();
    rollup_sums.shrink_to_fit();
  }
  invalidate();
}

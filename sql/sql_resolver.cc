/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

/**
  @file

  @brief
  Implementation of name resolution stage


  @defgroup Query_Resolver  Query Resolver
  @{
*/

#include "sql/sql_resolver.h"

#include <sys/types.h>

#include <algorithm>
#include <cassert>
#include <cstddef>  // size_t
#include <cstdio>   // snprintf
#include <cstring>  // strcmp
#include <deque>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

#include "field_types.h"
#include "lex_string.h"
#include "map_helpers.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql_com.h"  // NAME_LEN
#include "mysqld_error.h"
#include "prealloced_array.h"     // Prealloced_array
#include "sql/aggregate_check.h"  // Group_check
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_single_table_access
#include "sql/check_stack.h"       // check_stack_overrun
#include "sql/current_thd.h"       // current_thd
#include "sql/derror.h"            // ER_THD
#include "sql/enum_query_type.h"
#include "sql/error_handler.h"  // View_error_handler
#include "sql/field.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_row.h"
#include "sql/item_subselect.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/mdl.h"  // MDL_SHARED_READ
#include "sql/mem_root_array.h"
#include "sql/nested_join.h"
#include "sql/opt_hints.h"
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_nodes.h"  // PT_order_expr
#include "sql/parser_yystype.h"
#include "sql/query_options.h"
#include "sql/query_result.h"  // Query_result
#include "sql/range_optimizer/partition_pruning.h"
#include "sql/range_optimizer/range_optimizer.h"  // prune_partitions
#include "sql/sql_base.h"                         // setup_fields
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"  // Sql_cmd
#include "sql/sql_const.h"
#include "sql/sql_derived.h"  //Condition_pushdown
#include "sql/sql_error.h"
#include "sql/sql_executor.h"  // is_rollup_sum_wrapper, is_rollup_group_wrapper
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // build_bitmap_for_nested_joins
#include "sql/sql_select.h"
#include "sql/sql_test.h"   // print_where
#include "sql/sql_union.h"  // Query_result_union
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/thr_malloc.h"
#include "sql/visible_fields.h"
#include "sql/window.h"
#include "template_utils.h"
#include "thr_lock.h"  // TL_READ

using std::find;
using std::function;

static const enum_walk walk_options =
    enum_walk::PREFIX | enum_walk::POSTFIX | enum_walk::SUBQUERY;

static bool simplify_const_condition(THD *thd, Item **cond,
                                     bool remove_cond = true,
                                     bool *ret_cond_value = nullptr);
static Item *create_rollup_switcher(THD *thd, Query_block *query_block,
                                    Item_sum *item, int send_group_parts);
static bool fulltext_uses_rollup_column(const Query_block *query_block);

/**
  Prepare query block for optimization.

  Resolve table and column information.
  Resolve all expressions (item trees), ie WHERE clause, join conditions,
  GROUP BY clause, HAVING clause, ORDER BY clause, LIMIT clause.
  Prepare all subqueries recursively as part of resolving the expressions.
  Apply permanent transformations to the abstract syntax tree, such as
  semi-join transformation, derived table transformation, elimination of
  constant values and redundant clauses (e.g ORDER BY, GROUP BY).

  @param thd    thread handler
  @param insert_field_list List of fields when used in INSERT, otherwise NULL

  @returns false if success, true if error

  @note on privilege checking for SELECT query that possibly contains view
        or derived table references:

   - When this function is called, it is assumed that the precheck() function
     has been called. precheck() ensures that the user has some SELECT
     privileges to the tables involved in the query. When resolving views
     it has also been established that the user has some privileges for them.
     To prepare a view for privilege checking, it is also needed to call
     check_view_privileges() after views have been merged into the query.
     This is not necessary for unnamed derived tables since it has already
     been established that we have SELECT privileges for the underlying tables
     by the precheck functions. (precheck() checks a query without resolved
     views, ie. before tables are opened, so underlying tables of views
     are not yet available).

   - When a query block is resolved, always ensure that the user has SELECT
     privileges to the columns referenced in the WHERE clause, the join
     conditions, the GROUP BY clause, the HAVING clause and the ORDER BY clause.

   - When resolving the outer-most query block, ensure that the user also has
     SELECT privileges to the columns in the selected expressions.

   - When setting up a derived table or view for materialization, ensure that
     the user has SELECT privileges to the columns in the selected expressions

   - Column privileges are normally checked by Item_field::fix_fields().
     Exceptions are select list of derived tables/views which are checked
     in Table_ref::setup_materialized_derived(), and natural/using join
     conditions that are checked in mark_common_columns().

   - As far as INSERT, UPDATE and DELETE statements have the same expressions
     as a SELECT statement, this note applies to those statements as well.
*/
bool Query_block::prepare(THD *thd, mem_root_deque<Item *> *insert_field_list) {
  DBUG_TRACE;

  assert(this == thd->lex->current_query_block());
  assert(join == nullptr);
  assert(!thd->is_error());

  // If this query block is a table value constructor, a lot of the preparation
  // done in Query_block::prepare becomes irrelevant. Thus we call our own
  // Query_block::prepare_values in this case.
  if (is_table_value_constructor) return prepare_values(thd);

  Query_expression *const unit = master_query_expression();

  if (!m_table_nest.empty()) propagate_nullability(&m_table_nest, false);

  /*
    Determine whether it is suggested to merge immediate derived tables, based
    on the placement of the query block:
      - DTs belonging to outermost query block: always
      - DTs belonging to first level subqueries: Yes if inside SELECT statement,
        no otherwise (including UPDATE and DELETE).
        This is required to support a workaround for allowing subqueries
        containing the same table as is target for delete or update,
        by forcing a materialization of the subquery.
      - All other cases inherit status of parent query block.
  */
  allow_merge_derived = outer_query_block() == nullptr ||
                        master_query_expression()->item == nullptr ||
                        (outer_query_block()->outer_query_block() == nullptr
                             ? parent_lex->sql_command == SQLCOM_SELECT ||
                                   parent_lex->sql_command == SQLCOM_SET_OPTION
                             : outer_query_block()->allow_merge_derived);

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper_prepare(trace);
  Opt_trace_object trace_prepare(trace, "join_preparation");
  trace_prepare.add_select_number(select_number);
  Opt_trace_array trace_steps(trace, "steps");

  /*
    Setup the expressions in the SELECT list.
    For derived tables/views, wait with privilege checking of columns and
    marking in read/write sets until we know how they are used (may be used in
    UPDATE and INSERT). Exceptions:
     - Always assume columns referenced in subqueries are selected.
     - Always assume outer references are selected (marking is then done in
       Item_outer_ref::fix_fields).

    Expressions must be resolved here, before tables are set up, otherwise table
    function's arguments are not resolved properly.
  */
  const bool check_privs = !thd->derived_tables_processing ||
                           master_query_expression()->item != nullptr;
  thd->mark_used_columns = check_privs ? MARK_COLUMNS_READ : MARK_COLUMNS_NONE;
  ulonglong want_privilege_saved = thd->want_privilege;
  thd->want_privilege = check_privs ? SELECT_ACL : 0;

  /*
    Expressions in lateral join can't refer to item list, thus item list lookup
    shouldn't be allowed during table/table function setup.
  */
  is_item_list_lookup = false;

  /* Check that all tables, fields, conds and order are ok */

  if (setup_tables(thd, get_table_list(), false)) return true;

  if ((derived_table_count || table_func_count) &&
      resolve_placeholder_tables(thd, true))
    return true;

  // Wait with privilege checking until all derived tables are resolved.
  if (derived_table_count && !thd->derived_tables_processing &&
      check_view_privileges(thd, SELECT_ACL, SELECT_ACL))
    return true;

  is_item_list_lookup = true;

  // Precompute and store the row types of NATURAL/USING joins.
  if (leaf_table_count >= 2 &&
      setup_natural_join_row_types(thd, m_current_table_nest, &context))
    return true;

  Mem_root_array<Item_exists_subselect *> sj_candidates_local(thd->mem_root);
  set_sj_candidates(&sj_candidates_local);

  /*
    Item and Item_field CTORs will both increment some counters
    in current_query_block(), based on the current parsing context.
    We are not parsing anymore: any new Items created now are due to
    query rewriting, so stop incrementing counters.
   */
  assert(parsing_place == CTX_NONE);
  parsing_place = CTX_NONE;

  resolve_place = RESOLVE_SELECT_LIST;

  if (with_wild && setup_wild(thd)) return true;
  if (setup_base_ref_items(thd)) return true; /* purecov: inspected */

  if (setup_fields(thd, thd->want_privilege, /*allow_sum_func=*/true,
                   /*split_sum_funcs=*/true, /*column_update=*/false,
                   insert_field_list, &fields, base_ref_items))
    return true;

  resolve_place = RESOLVE_NONE;

  const nesting_map save_allow_sum_func = thd->lex->allow_sum_func;
  const nesting_map save_deny_window_func = thd->lex->m_deny_window_func;

  // Do not allow local set functions for join conditions, WHERE and GROUP BY
  thd->lex->allow_sum_func &= ~((nesting_map)1 << nest_level);

  thd->mark_used_columns = MARK_COLUMNS_READ;
  thd->want_privilege = SELECT_ACL;

  // Set up join conditions and WHERE clause
  if (setup_conds(thd)) return true;

  // Set up the GROUP BY clause
  int all_fields_count = fields.size();
  if (group_list.elements && setup_group(thd)) return true;
  hidden_group_field_count = fields.size() - all_fields_count;

  // Allow local set functions in HAVING and ORDER BY
  thd->lex->allow_sum_func |= (nesting_map)1 << nest_level;

  // Windowing is not allowed with HAVING
  thd->lex->m_deny_window_func |= (nesting_map)1 << nest_level;

  if (is_non_primitive_grouped()) {
    for (Item *item : fields) {
      mark_item_as_maybe_null_if_non_primitive_grouped(item);
      item->update_used_tables();
    }
    if (populate_grouping_sets(thd)) {
      return true;
    }
  }

  // Setup the HAVING clause
  if (m_having_cond) {
    assert(m_having_cond->is_bool_func());
    thd->where = "having clause";
    having_fix_field = true;
    resolve_place = RESOLVE_HAVING;
    if (!m_having_cond->fixed &&
        (m_having_cond->fix_fields(thd, &m_having_cond) ||
         m_having_cond->check_cols(1)))
      return true;

    assert(m_having_cond->data_type() != MYSQL_TYPE_INVALID);

    /*
      Rollup may alter nullability of HAVING condition, so wait with
      simplification of this condition until after rollup is resolved.
    */

    having_fix_field = false;
    resolve_place = RESOLVE_NONE;
  }

  if (olap == ROLLUP_TYPE && resolve_rollup(thd))
    return true; /* purecov: inspected */

  thd->lex->m_deny_window_func = save_deny_window_func;

  if (m_having_cond != nullptr) {
    if (olap == ROLLUP_TYPE) {
      m_having_cond = resolve_rollup_item(thd, m_having_cond);
      if (m_having_cond == nullptr) {
        return true;
      }
    }
    /*
      Simplify the having condition if it is a const item.
      Leave a TRUE condition if HAVING is always true, so that query block
      is still marked as having a HAVING condition.
    */
    if (m_having_cond->const_item() && !thd->lex->is_view_context_analysis() &&
        !m_having_cond->walk(&Item::is_non_const_over_literals,
                             enum_walk::POSTFIX, nullptr) &&
        simplify_const_condition(thd, &m_having_cond, false))
      return true;
  }

  if (m_qualify_cond != nullptr) {
    assert(m_qualify_cond->is_bool_func());
    thd->where = "qualify clause";
    resolve_place = RESOLVE_QUALIFY;
    if (!m_qualify_cond->fixed &&
        (m_qualify_cond->fix_fields(thd, &m_qualify_cond) ||
         m_qualify_cond->check_cols(1)))
      return true;

    assert(m_qualify_cond->data_type() != MYSQL_TYPE_INVALID);
    resolve_place = RESOLVE_NONE;

    /*
      Simplify the QUALIFY condition if it is a const item.
      Leave a TRUE condition if QUALIFY is always true, so that query block
      is still marked as having a QUALIFY condition.
    */
    if (m_qualify_cond->const_item() && !thd->lex->is_view_context_analysis() &&
        !m_qualify_cond->walk(&Item::is_non_const_over_literals,
                              enum_walk::POSTFIX, nullptr) &&
        simplify_const_condition(thd, &m_qualify_cond, false))
      return true;

    /*
      The QUALIFY clause requires the inclusion of at least one window function
      in the query block. The window function can be part of any one of the
      following: a) SELECT column list. b) Filter predicate of the QUALIFY
      clause.

      Rejects the query if the QUALIFY clause is present but neither of the
      above conditions are satisfied.
    */
    if (!has_windows() && !m_qualify_cond->has_wf()) {
      my_error(ER_QUALIFY_WITHOUT_WINDOW_FUNCTION, MYF(0));
      return true;
    }
  }

  // Set up the ORDER BY clause
  all_fields_count = fields.size();
  if (order_list.elements) {
    if (setup_order(thd, base_ref_items, get_table_list(), &fields,
                    order_list.first))
      return true;
  }

  if (fulltext_uses_rollup_column(this)) {
    my_error(ER_FULLTEXT_WITH_ROLLUP, MYF(0));
    return true;
  }

  hidden_order_field_count = fields.size() - all_fields_count;

  // Resolve OFFSET and LIMIT clauses
  if (resolve_limits(thd)) return true;

  /*
    Query block is completely resolved, except for windows (see below) which
    handles its own, so restore set function allowance.
  */
  thd->lex->allow_sum_func = save_allow_sum_func;

  /*
    Permanently remove redundant parts from the query if
      1) This is a subquery
      2) Not normalizing a view. Removal should take place when a
         query involving a view is optimized, not when the view
         is created
  */
  if (unit->item &&                           // 1)
      !thd->lex->is_view_context_analysis())  // 2)
  {
    if (remove_redundant_subquery_clauses(thd)) return true;
  }

  /*
    Set up windows after setup_order() (as the query's ORDER BY may contain
    window functions), and before setup_order_final() (as such function needs
    to know about implicit grouping which may be induced by an aggregate
    function in the window's PARTITION or ORDER clause).
  */
  const size_t fields_cnt = fields.size();
  if (m_windows.elements != 0 &&
      Window::setup_windows1(thd, this, base_ref_items, get_table_list(),
                             &fields, &m_windows))
    return true;

  bool added_new_sum_funcs = fields.size() > fields_cnt;

  if (order_list.elements) {
    if (setup_order_final(thd)) return true; /* purecov: inspected */
    added_new_sum_funcs = true;
  }

  thd->want_privilege = want_privilege_saved;

  if (is_distinct() && can_skip_distinct())
    remove_base_options(SELECT_DISTINCT);

  /*
    Printing the expanded query should happen here and not elsewhere, because
    when a view is merged (when the view is opened in open_tables()), the
    parent query's query_block does not yet contain a correct WHERE clause (it
    misses the view's merged WHERE clause). This is corrected only just above,
    in Table_ref::prep_where(), called by
    setup_without_group()->setup_conds().
    We also have to wait for fix_fields() on HAVING, above.
    At this stage, we also have properly set up Item_ref-s.
  */
  {
    Opt_trace_object trace_wrapper(trace);
    opt_trace_print_expanded_query(thd, this, &trace_wrapper);
  }

  // Transform eligible scalar subqueries to derived tables.
  //
  // Don't transform if analyzing a view: the resulting query may not be
  // compilable from sqldump, (due to group by check/visibility in HAVING).
  //
  // Don't transform if the switch subquery_to_derived is false.
  //
  // Note that the transformation must precede m_having_cond->split_sum_func2
  // below since substitutions may be made in the HAVING clause which would not
  // otherwise get done.

  if (!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SUBQUERY_TO_DERIVED) ||
       (parent_lex->m_sql_cmd != nullptr &&
        thd->secondary_engine_optimization() ==
            Secondary_engine_optimization::SECONDARY)) &&
      transform_scalar_subqueries_to_join_with_derived(thd))
    return true; /* purecov: inspected */

  /*
    If GROUPING function is present in having condition -
    1. Set that the evaluation of this condition depends on rollup
    result.
    2. Add a reference to the condition so that result is stored
    after evaluation.
  */
  if (m_having_cond && (m_having_cond->has_aggregation() ||
                        m_having_cond->has_grouping_func())) {
    if (m_having_cond->split_sum_func2(thd, base_ref_items, &fields,
                                       &m_having_cond, true)) {
      return true;
    }
    added_new_sum_funcs = true;
  }
  // Move aggregation functions and window functions present in
  // QUALIFY clause to the field list and replace them with references.
  if (m_qualify_cond != nullptr &&
      (m_qualify_cond->has_aggregation() || m_qualify_cond->has_wf())) {
    if (m_qualify_cond->split_sum_func2(thd, base_ref_items, &fields,
                                        &m_qualify_cond, true)) {
      return true;
    }
    added_new_sum_funcs = true;
  }
  if (inner_sum_func_list) {
    Item_sum *end = inner_sum_func_list;
    Item_sum *item_sum = end;
    do {
      item_sum = item_sum->next_sum;
      if (item_sum->split_sum_func2(thd, base_ref_items, &fields, nullptr,
                                    false)) {
        return true;
      }
      added_new_sum_funcs = true;
    } while (item_sum != end);
  }

  if (added_new_sum_funcs && olap == ROLLUP_TYPE) {
    uint send_group_parts = group_list_size();
    for (auto it = fields.begin(); it != fields.end(); ++it) {
      Item *item = *it;
      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item()) {
        Item_sum *item_sum = down_cast<Item_sum *>(item);
        if (item_sum->aggr_query_block == this &&
            !item_sum->is_rollup_sum_wrapper()) {
          // split_sum_func2 created a new aggregate function item,
          // so we need to update it for rollup.
          Item *new_item =
              create_rollup_switcher(thd, this, item_sum, send_group_parts);
          if (new_item == nullptr) return true;
          *it = new_item;
        }
      }
    }
  }

  if (group_list.elements) {
    /*
      Because HEAP tables can't index BIT fields we need to use an
      additional hidden field for grouping because later it will be
      converted to a LONG field. Original field will remain of the
      BIT type and will be returned to a client.
    */
    for (ORDER *ord = group_list.first; ord; ord = ord->next) {
      if ((*ord->item)->type() == Item::FIELD_ITEM &&
          (*ord->item)->data_type() == MYSQL_TYPE_BIT) {
        Item_field *field = new Item_field(thd, *(Item_field **)ord->item);
        ord->item = add_hidden_item(field);
      }
    }
  }

  // Setup full-text functions after resolving HAVING
  if (has_ft_funcs()) {
    // The full-text search function cannot be called after aggregation, as it
    // needs the underlying scan to be positioned on the correct row. Therefore,
    // lift calls to the full-text search MATCH function to the SELECT list (as
    // hidden items), so the results can be materialized before or during
    // aggregation.
    if (lift_fulltext_from_having_to_select_list(thd)) {
      return true;
    }

    if (setup_ftfuncs(thd, this)) return true;
  }

  if (query_result() && query_result()->prepare(thd, fields, unit)) return true;

  if (has_sj_candidates() && flatten_subqueries(thd)) return true;

  set_sj_candidates(nullptr);

  /*
    When reaching the top-most query block, or the next-to-top query block for
    the SQL command SET and for SP instructions (indicated with SQLCOM_END),
    apply local transformations to this query block and all underlying query
    blocks.
  */
  if (!thd->lex->is_view_context_analysis() &&
      (outer_query_block() == nullptr ||
       ((parent_lex->sql_command == SQLCOM_SET_OPTION ||
         parent_lex->sql_command == SQLCOM_END ||
         parent_lex->sql_command == SQLCOM_LOAD) &&
        outer_query_block()->outer_query_block() == nullptr)) &&
      !skip_local_transforms) {
    /*
      This code is invoked in the following cases:
      - if this is not a create view statement as transformations are
      not required when creating a view.
      - if this is an outer-most query block of a SELECT or multi-table
      UPDATE/DELETE statement. Notice that for a UNION, this applies to
      all query blocks. It also applies to a fake_query_block object.
      - if this is one of highest-level subqueries, if the statement is
      something else; like subq-i in:
      UPDATE t1 SET col1=(subq-1), col2=(subq-2);
      - If this is a subquery in a SET command,
      or scalar subqueries used in SP expressions like sp_instr_freturn
      (undicated by SQLCOM_END).
      @todo: Refactor SET so that this is not needed.
      - If this is a subquery in a LOAD command.
      - INSERT may in some cases alter the sequence of preparation calls, by
      setting the skip_local_transforms flag before calling prepare().

      Local transforms are applied after query block merging.
      This means that we avoid unnecessary invocations, as local transforms
      would otherwise have been performed first before query block merging and
      then another time after query block merging.
      Thus, apply_local_transforms() may run only after the top query
      is finished with query block merging. That's why
      apply_local_transforms() is initiated only by the top query, and then
      recurses into subqueries.
     */
    if (apply_local_transforms(thd, true)) return true;
  }

  // Eliminate unused window definitions, redundant sorts etc.
  if (!m_windows.is_empty()) Window::eliminate_unused_objects(&m_windows);

  // Replace group by field references inside window functions with references
  // in the presence of ROLLUP.
  if (olap == ROLLUP_TYPE && resolve_rollup_wfs(thd))
    return true; /* purecov: inspected */

  // If CUBE is present in the query, all expressions that include
  // any GROUP BY expression need to be marked as dependent on
  // grouping set.
  if (olap == CUBE_TYPE) {
    for (Item *item : fields) {
      bool is_updated = false;
      WalkItem(item, enum_walk::POSTFIX, [this, &is_updated](Item *inner_item) {
        if (find_in_group_list(inner_item, /*rollup_level=*/nullptr) !=
            nullptr) {
          inner_item->set_group_by_modifier();
          is_updated = true;
        }
        return false;
      });
      if (is_updated) item->update_used_tables();
    }
  }

  assert(!thd->is_error());
  return false;
}

/*
  Push conditions if possible to all the materialized derived tables.
  Keep pushing as far down as possible making the call to this function
  recursively.

  @param thd      thread handler

  @returns false if success, true if error

  Since this is called at the end after applying local transformations,
  call this function while traversing the query block hierarchy top-down.
*/
bool Query_block::push_conditions_to_derived_tables(THD *thd) {
  if (materialized_derived_table_count > 0)
    for (Table_ref *tl = leaf_tables; tl; tl = tl->next_leaf) {
      if (tl->is_view_or_derived() && tl->uses_materialization() &&
          where_cond() && tl->can_push_condition_to_derived(thd)) {
        Item **where = where_cond_ref();
        Opt_trace_context *const trace = &thd->opt_trace;
        Condition_pushdown cp(*where, tl, thd, trace);
        // Make condition for the derived table
        if (cp.make_cond_for_derived()) return true;
        // The remaining condition that could not be pushed stays in this
        // WHERE clause.
        *where = cp.get_remainder_cond();
      }
    }
  /*
    Push conditions if possible to derived tables which were not merged. By
    running top-down, the resulting pushed down condition can be pushed down
    even more, in the case where a derived table contains an inner derived
    table.
   */
  for (Query_expression *unit = first_inner_query_expression(); unit;
       unit = unit->next_query_expression()) {
    for (Query_block *sl = unit->first_query_block(); sl;
         sl = sl->next_query_block()) {
      if (sl->push_conditions_to_derived_tables(thd)) return true;
    }
  }
  return false;
}

/**
  Prepare a table value constructor query block for optimization.

  In the case of a table value constructor Query_block, we return the result of
  this function from Query_block::prepare, instead of doing the standard prepare
  routine.

  For a table value constructor block, most preparation of a standard
  Query_block becomes irrelevant (in particular INTO, FROM, WHERE, GROUP, HAVING
  and WINDOW). We therefore substitute the standard resolving routine with this
  one, which is simply responsible for resolving the expressions contained in
  VALUES, as well as the query result.

  @param thd    thread handler

  @returns false if success, true if error
 */

bool Query_block::prepare_values(THD *thd) {
  Query_expression *const unit = master_query_expression();

  if (resolve_table_value_constructor_values(thd)) return true;

  if (setup_tables(thd, get_table_list(), /*select_insert=*/false)) {
    return true;
  }

  // Setup the HAVING clause, duplicating code from Query_block::prepare. This
  // is strictly necessary in the case of PREPARE statements, where
  // subquery transformations may rewrite its Query_block to use m_having_cond.
  //
  // For example, a query like `SELECT * FROM t WHERE (a, b) IN (VALUES ROW(1,
  // 10))` may be rewritten such that the Query_block within the IN subquery has
  // a HAVING clause with an Item_cond_and. This must be taken into account
  // during the second preparation that is done when the prepared statement is
  // _executed_; we now have to resolve m_having_cond properly.
  //
  // Note that this duplicated code should be removed in the future. TODO: for
  // wl#9384, which refactors DML statement preparation to be done only once.
  if (m_having_cond) {
    assert(m_having_cond->is_bool_func());
    thd->where = "having clause";
    having_fix_field = true;
    resolve_place = RESOLVE_HAVING;
    if (!m_having_cond->fixed &&
        (m_having_cond->fix_fields(thd, &m_having_cond) ||
         m_having_cond->check_cols(1)))
      return true; /* purecov: inspected */

    assert(!m_having_cond->const_item());

    having_fix_field = false;
    resolve_place = RESOLVE_NONE;
  }

  assert(qualify_cond() == nullptr);

  /*
    A table value constructor may have a defined ordering, thus calling
    setup_order() is needed, however calling setup_order_final() is
    not necessary since this construct cannot be aggregated.
  */
  if (is_ordered() && setup_order(thd, base_ref_items, get_table_list(),
                                  &fields, order_list.first)) {
    return true;
  }

  if (query_result() && query_result()->prepare(thd, fields, unit))
    return true; /* purecov: inspected */

  if (resolve_limits(thd)) return true;

  // If this is a subquery, remove redundant clauses (ORDER BY in particular).
  if (unit->item != nullptr && !thd->lex->is_view_context_analysis()) {
    if (remove_redundant_subquery_clauses(thd)) return true;
  }

  return false;
}

/**
  Apply local transformations, such as join nest simplification. 'Local' means
  that each transformation happens on one single query block.
  Also perform partition pruning, which is most effective after transformations
  have been done.
  This function also does condition pushdown to derived tables after all
  the local transformations are applied although condition pushdown is
  strictly not a local transform.

  @param thd      thread handler
  @param prune    if true, then prune partitions based on const conditions

  @returns false if success, true if error

  Since this is called after flattening of query blocks, call this function
  while traversing the query block hierarchy top-down.
*/

bool Query_block::apply_local_transforms(THD *thd, bool prune) {
  DBUG_TRACE;

  assert(first_execution);

  /*
    If query block contains one or more merged derived tables/views,
    walk through lists of columns in select lists and remove unused columns.
  */
  if (derived_table_count) delete_unused_merged_columns(&m_table_nest);

  for (Query_expression *unit = first_inner_query_expression(); unit;
       unit = unit->next_query_expression())
    for (auto qt : unit->query_terms<>())
      if (qt->query_block()->apply_local_transforms(thd, true)) return true;

  // Convert all outer joins to inner joins if possible
  if (simplify_joins(thd, &m_table_nest, true, false, &m_where_cond))
    return true;
  if (record_join_nest_info(&m_table_nest)) return true;
  build_bitmap_for_nested_joins(&m_table_nest, 0);

  /*
    Here are the reasons why we do the following check here (i.e. late).
    * setup_fields () may have done split_sum_func () on aggregate items of
    the SELECT list, so for reliable comparison of the ORDER BY list with
    the SELECT list, we need to wait until split_sum_func() is done with
    the ORDER BY list.
    * we get resolved expressions "most of the time", which is always a good
    thing. Some outer references may not be resolved, though.
    * we need nested_join::used_tables, and this member is set in
    simplify_joins()
    * simplify_joins() does outer-join-to-inner conversion, which increases
    opportunities for functional dependencies (weak-to-strong, which is
    unusable, becomes strong-to-strong).
    * check_only_full_group_by() is dependent on processing done by
    simplify_joins() (for example it uses the value of
    Query_block::outer_join).

    The drawback is that the checks are after subquery transformations, so can
    meet strange "internally added" items.

    Note that when we are creating a view, simplify_joins() doesn't run so
    check_only_full_group_by() cannot run, any error will be raised only
    when the view is later used (SELECTed...)
  */
  if ((is_distinct() || is_grouped()) &&
      (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY) &&
      check_only_full_group_by(thd))
    return true;

  /*
    Prune partitions for all query blocks after query block merging, if
    pruning is wanted.
  */
  if (partitioned_table_count && prune) {
    for (Table_ref *tbl = leaf_tables; tbl; tbl = tbl->next_leaf) {
      /*
        This will only prune constant conditions, which will be used for
        lock pruning.
      */
      if (prune_partitions(thd, tbl->table, this,
                           tbl->join_cond() ? tbl->join_cond() : m_where_cond))
        return true; /* purecov: inspected */

      if (tbl->table->all_partitions_pruned_away &&
          !tbl->is_inner_table_of_outer_join())
        set_empty_query();
    }
  }
  /*
     Pushing conditions down to derived tables must be done after validity
     checks of grouped queries done above; indeed, by replacing columns
     with expressions, inside equalities of WHERE, pushdown makes the checks
     impossible.
     The said validity checks must be done after simplify_joins() has been
     done on all query blocks. While pushdown must be done on the outer
     most query block first, then on subqueries.
     These circular dependencies explain why:
     - pushdown is done after all local transformations have been applied.
     - a pushed-down condition cannot help to convert LEFT JOIN to inner join
     inside a derived table's definition.
   */
  if (outer_query_block() == nullptr && push_conditions_to_derived_tables(thd))
    return true;

  return false;
}

/**
  Update used tables information for a JOIN expression
*/
static void update_used_tables_for_join(mem_root_deque<Table_ref *> *tables) {
  for (Table_ref *table_ref : *tables) {
    if (table_ref->join_cond() != nullptr)
      table_ref->join_cond()->update_used_tables();

    if (table_ref->nested_join != nullptr)
      update_used_tables_for_join(&table_ref->nested_join->m_tables);
  }
}

/**
  Update used tables information for all local expressions.
*/
void Query_block::update_used_tables() {
  for (Item *item : visible_fields()) {
    item->update_used_tables();
  }
  if (m_current_table_nest != nullptr)
    update_used_tables_for_join(m_current_table_nest);
  if (where_cond() != nullptr) where_cond()->update_used_tables();
  for (ORDER *group = group_list.first; group; group = group->next)
    (*group->item)->update_used_tables();
  if (having_cond() != nullptr) having_cond()->update_used_tables();
  for (ORDER *order = order_list.first; order; order = order->next)
    (*order->item)->update_used_tables();
  List_iterator<Window> wi(m_windows);
  Window *w;
  while ((w = wi++)) {
    for (ORDER *wp = w->first_partition_by(); wp != nullptr; wp = wp->next)
      (*wp->item)->update_used_tables();
    for (ORDER *wo = w->first_order_by(); wo != nullptr; wo = wo->next)
      (*wo->item)->update_used_tables();
  }
}

/**
  Resolve OFFSET and LIMIT clauses for a query block.

  @param thd     Thread handler

  @returns false if success, true if error

  OFFSET and LIMIT clauses may be attached to query blocks that make up
  a query expression. OFFSET and LIMIT clauses that apply to a whole
  query expression are attached to the fake_query_block, hence we can use
  this interface to resolve them as well.

  OFFSET and LIMIT may be unsigned integer literal values or parameters.
  If parameters, ensure that the type is unsigned integer.
*/

bool Query_block::resolve_limits(THD *thd) {
  if (offset_limit != nullptr) {
    if (offset_limit->fix_fields(thd, nullptr))
      return true; /* purecov: inspected */
    if (offset_limit->data_type() == MYSQL_TYPE_INVALID) {
      if (offset_limit->propagate_type(
              thd, Type_properties(MYSQL_TYPE_LONGLONG, true)))
        return true;
      offset_limit->pin_data_type();
    }
  }

  if (select_limit != nullptr) {
    if (select_limit->fix_fields(thd, nullptr))
      return true; /* purecov: inspected */
    if (select_limit->data_type() == MYSQL_TYPE_INVALID) {
      if (select_limit->propagate_type(
              thd, Type_properties(MYSQL_TYPE_LONGLONG, true)))
        return true;
      select_limit->pin_data_type();
    }
  }
  return false;
}

/**
  Try to replace a const condition with a simple constant.
  A true condition is replaced with an empty item pointer if remove_cond
  is true. Else it is replaced with the constant TRUE.
  A false condition is replaced with the constant FALSE.

  @param thd            Thread handler
  @param[in,out]  cond  Address of condition, may be substituted with a literal
  @param remove_cond    If true removes a "true" condition. Else replaces
                        it with a constant TRUE.
  @param ret_cond_value Store the result of the evaluated const condition

  @returns false if success, true if error
*/

static bool simplify_const_condition(THD *thd, Item **cond, bool remove_cond,
                                     bool *ret_cond_value) {
  assert((*cond)->const_item());

  bool cond_value;

  /* Push ignore / strict error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  bool err = eval_const_cond(thd, *cond, &cond_value);
  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  if (err) return true;

  DBUG_EXECUTE("where",
               print_where(thd, *cond, "simplify_const_cond", QT_ORDINARY););
  if (cond_value) {
    if (remove_cond)
      *cond = nullptr;
    else {
      Prepared_stmt_arena_holder ps_arena_holder(thd);
      *cond = new (thd->mem_root) Item_func_true();
      if (*cond == nullptr) return true;
    }
  } else if ((*cond)->type() != Item::INT_ITEM) {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    *cond = new (thd->mem_root) Item_func_false();
    if (*cond == nullptr) return true;
  }
  if (ret_cond_value) *ret_cond_value = cond_value;
  return false;
}

/**
  Check if the subquery predicate can be executed via materialization.

  @param thd       THD
  @param query_block Query_block of the subquery
  @param outer      Parent Query_block (outer to subquery)

  @return true if subquery allows materialization, false otherwise.
*/

bool Item_in_subselect::subquery_allows_materialization(
    THD *thd, Query_block *query_block, const Query_block *outer) {
  const uint elements = query_expr()->first_query_block()->num_visible_fields();
  DBUG_TRACE;
  assert(elements >= 1);
  assert(left_expr->cols() == elements);

  OPT_TRACE_TRANSFORM(&thd->opt_trace, trace_wrapper, trace_mat,
                      query_block->select_number, "IN (SELECT)",
                      "materialization");

  const char *cause = nullptr;
  if (subquery_type() != Item_subselect::IN_SUBQUERY) {
    // Subq-mat cannot handle 'outer_expr > {ANY|ALL}(subq)'...
    cause = "not an IN predicate";
  } else if (m_subquery_used_tables & RAND_TABLE_BIT) {
    // Subquery with a random function cannot be materalized.
    // But random function in left expression is OK
    cause = "non-deterministic";
  } else if (!query_block->is_simple_query_block()) {
    // Subquery must be a simple query specification clause (not a set operation
    // or a parenthesized query expression).
    cause = "in set operation or a parenthesized query expression";
  } else if (!query_block->master_query_expression()
                  ->first_query_block()
                  ->leaf_tables) {
    // Subquery has no tables, hence no point in materializing.
    cause = "no inner tables";
  } else if (!outer->join) {
    /*
      Maybe this is a subquery of a single table UPDATE/DELETE (TODO:
      handle this by switching to multi-table UPDATE/DELETE).
    */
    cause = "parent query has no JOIN";
  } else if (!outer->leaf_tables) {
    // The upper query is SELECT ... FROM DUAL. No gain in materializing.
    cause = "no tables in outer query";
  } else if (dependent_before_in2exists()) {
    /*
      Subquery should not be correlated; the correlation due to predicates
      injected by IN->EXISTS does not count as we will remove them if we
      choose materialization.

      TODO:
      This is an overly restrictive condition. It can be extended to:
         (Subquery is non-correlated ||
          Subquery is correlated to any query outer to IN predicate ||
          (Subquery is correlated to the immediate outer query &&
           Subquery !contains {GROUP BY, ORDER BY [LIMIT],
           aggregate functions}) && subquery predicate is not under "NOT IN"))
    */
    cause = "correlated";
  } else {
    /*
      Check that involved expression types allow materialization.
      This is a temporary fix for BUG#36752; see bug report for
      description of restrictions we need to put on the compared expressions.
    */
    assert(left_expr->fixed);
    // @see comment in Item_subselect::element_index()
    bool has_nullables = left_expr->is_nullable();

    uint i = 0;
    for (Item *const inner_item :
         query_expr()->first_query_block()->visible_fields()) {
      Item *const outer_item = left_expr->element_index(i++);
      if (!types_allow_materialization(outer_item, inner_item)) {
        cause = "type mismatch";
        break;
      }
      if (inner_item->is_blob_field())  // 6
      {
        cause = "inner blob";
        break;
      }
      has_nullables |= inner_item->is_nullable();
    }

    if (!cause) {
      trace_mat.add("has_nullable_expressions", has_nullables);
      /*
        Subquery materialization cannot handle NULLs partial matching
        properly, yet. If the outer or inner values are NULL, the
        subselect_hash_sj_engine may reply FALSE when it should reply UNKNOWN.
        So, we must limit it to those three cases:
        - when FALSE and UNKNOWN are equivalent answers. I.e. this is a a
        top-level predicate (this implies it is not negated).
        - when outer and inner values cannot be NULL.
        - when there is a single inner column (because for this we have a
        limited implementation of NULLs partial matching).
      */
      trace_mat.add("treat_UNKNOWN_as_FALSE", abort_on_null);

      if (!abort_on_null && has_nullables && (elements > 1))
        cause = "cannot_handle_partial_matches";
      else {
        trace_mat.add("possible", true);
        return true;
      }
    }
  }
  assert(cause != nullptr);
  trace_mat.add("possible", false).add_alnum("cause", cause);
  return false;
}

/**
  Make list of leaf tables of join table tree

  @param list    pointer to pointer on list first element
                 Must be set to NULL before first (recursive) call
  @param tables  table list

  @returns pointer on pointer to next_leaf of last element
*/

static Table_ref **make_leaf_tables(Table_ref **list, Table_ref *tables) {
  for (Table_ref *table = tables; table; table = table->next_local) {
    // A mergeable view is not allowed to have a table pointer.
    assert(!(table->is_view() && table->is_merged() && table->table));
    if (table->merge_underlying_list) {
      assert(table->is_merged());

      list = make_leaf_tables(list, table->merge_underlying_list);
    } else {
      *list = table;
      list = &table->next_leaf;
    }
  }
  return list;
}

/**
  Check privileges for the view tables merged into a query block.

  @param thd                   Thread context.
  @param want_privilege_first  Privileges requested for the first leaf.
  @param want_privilege_next   Privileges requested for the remaining leaves.

  @note Beware that it can't properly check privileges in cases when
        table being changed is not the first table in the list of leaf
        tables (for example, for multi-UPDATE).

  @note The inner loop is slightly inefficient. A view will have its privileges
        checked once for every base table that it refers to.

  @returns false if success, true if error.
*/

bool Query_block::check_view_privileges(THD *thd, ulong want_privilege_first,
                                        ulong want_privilege_next) {
  ulong want_privilege = want_privilege_first;
  Internal_error_handler_holder<View_error_handler, Table_ref> view_handler(
      thd, true, leaf_tables);

  for (Table_ref *tl = leaf_tables; tl; tl = tl->next_leaf) {
    for (Table_ref *ref = tl; ref->referencing_view;
         ref = ref->referencing_view) {
      if (check_single_table_access(thd, want_privilege, ref, false))
        return true;
    }
    want_privilege = want_privilege_next;
  }
  return false;
}

/**
  Set up table leaves in the query block based on list of tables.

  @param thd           Thread handler
  @param tables        List of tables to handle
  @param select_insert It is SELECT ... INSERT command

  @note
    Check also that the 'used keys' and 'ignored keys' exists and set up the
    table structure accordingly.
    Create a list of leaf tables.

    This function has to be called for all tables that are used by items,
    as otherwise table->map is not set and all Item_field will be regarded
    as const items.

  @returns False on success, true on error
*/

bool Query_block::setup_tables(THD *thd, Table_ref *tables,
                               bool select_insert) {
  DBUG_TRACE;

  assert((select_insert && !tables->next_name_resolution_table) || !tables ||
         (context.table_list && context.first_name_resolution_table));

  leaf_tables = nullptr;
  (void)make_leaf_tables(&leaf_tables, tables);

  Table_ref *first_query_block_table = nullptr;
  if (select_insert) {
    // "insert_table" is needed for remap_tables().
    thd->lex->insert_table = leaf_tables->top_table();

    // Get first table in SELECT part
    first_query_block_table = thd->lex->insert_table->next_local;

    // Then, find the first leaf table
    if (first_query_block_table)
      first_query_block_table = first_query_block_table->first_leaf_table();
  }
  uint tableno = 0;
  leaf_table_count = 0;
  partitioned_table_count = 0;

  for (Table_ref *tr = leaf_tables; tr; tr = tr->next_leaf, tableno++) {
    TABLE *const table = tr->table;
    if (tr == first_query_block_table) {
      /*
        For INSERT ... SELECT command, restart numbering from zero for first
        leaf table from SELECT part of query.
      */
      first_query_block_table = nullptr;
      tableno = 0;
    }
    if (tableno >= MAX_TABLES) {
      my_error(ER_TOO_MANY_TABLES, MYF(0), static_cast<int>(MAX_TABLES));
      return true;
    }
    tr->set_tableno(tableno);
    leaf_table_count++;  // Count the input tables of the query

    if (opt_hints_qb &&        // QB hints initialized
        !tr->opt_hints_table)  // Table hints are not adjusted yet
    {
      tr->opt_hints_table = opt_hints_qb->adjust_table_hints(tr);
    }

    if (tr->has_tablesample() && tr->validate_tablesample_clause(thd)) {
      return true;
    }

    if (table == nullptr) continue;
    assert(table->pos_in_table_list == tr);
    if (!tr->opt_hints_table ||
        // Ignore old index hint processing if new style hints are specified.
        !tr->opt_hints_table->update_index_hint_maps(thd, tr->table)) {
      if (tr->process_index_hints(thd, table)) return true;
    }

    if (table->part_info)  // Count number of partitioned tables
      partitioned_table_count++;
  }

  /*
    @todo - consider calling this from SELECT::prepare() instead.
    It might save the test on select_insert to prevent check_unresolved()
    from being called twice for INSERT ... SELECT.
  */
  if (opt_hints_qb && !select_insert) opt_hints_qb->check_unresolved(thd);

  return false;
}

/**
  Re-map table numbers for all tables in a query block.

  @param thd           Thread handler

  @note
    This function needs to be called after setup_tables() has been called,
    and after a query block for a subquery has been merged into a parent
    quary block.
*/

void Query_block::remap_tables(THD *thd) {
  LEX *const lex = thd->lex;
  Table_ref *first_query_block_table = nullptr;
  if (lex->insert_table && lex->insert_table == leaf_tables->top_table()) {
    /*
      For INSERT ... SELECT command, restart numbering from zero for first
      leaf table from SELECT part of query.
    */
    // Get first table in SELECT part
    first_query_block_table = lex->insert_table->next_local;

    // Then, recurse down to get first leaf table
    if (first_query_block_table)
      first_query_block_table = first_query_block_table->first_leaf_table();
  }

  uint tableno = 0;
  for (Table_ref *tl = leaf_tables; tl; tl = tl->next_leaf) {
    // Reset table number after having reached first table after insert table
    if (first_query_block_table == tl) tableno = 0;
    tl->set_tableno(tableno++);
  }
}

/**
  @brief Resolve derived table, view or table function references in query block

  @param thd            Pointer to THD.
  @param apply_semijoin if true, apply semi-join transform when possible

  @return false if success, true if error
*/

bool Query_block::resolve_placeholder_tables(THD *thd, bool apply_semijoin) {
  DBUG_TRACE;

  assert(derived_table_count > 0 || table_func_count > 0);

  // Prepare derived tables and views that belong to this query block.
  for (Table_ref *tl = get_table_list(); tl; tl = tl->next_local) {
    if (!tl->is_view_or_derived() && !tl->is_table_function()) continue;

    // scalar to derived: derived tables may have been merged already:
    // WL#6570 transform_grouped_to_derived() calls setup_tables() and
    // resolve_placeholder_tables().
    if (tl->is_merged() || tl->uses_materialization()) {
      continue;
    }

    assert(!tl->is_merged() && !tl->uses_materialization());

    if (tl->resolve_derived(thd, apply_semijoin)) return true;
    /*
      Merge the derived tables that do not require materialization into
      the current query block, if possible.
      Merging is only done once and must not be repeated for prepared execs.
    */
    if (!thd->lex->is_view_context_analysis()) {
      if (tl->is_mergeable() && merge_derived(thd, tl))
        return true; /* purecov: inspected */
    }
    if (tl->is_merged()) continue;
    // Prepare remaining derived tables for materialization
    if (tl->is_table_function()) {
      if (tl->setup_table_function(thd)) {
        return true;
      }
    } else if (tl->table == nullptr && tl->setup_materialized_derived(thd)) {
      return true;
    }
    materialized_derived_table_count++;
  }

  return false;
}

/**

  Check if the offset and limit are valid for a semijoin. A semijoin
  can be used only if OFFSET is 0 and select LIMIT is not 0.

  @retval false  if OFFSET and LIMIT does not permit a semijoin,
  @retval true   otherwise.
*/

bool Query_block::is_row_count_valid_for_semi_join() {
  if (offset_limit != nullptr &&
      (!offset_limit->const_item() || offset_limit->val_int() != 0))
    return false;

  if (select_limit != nullptr &&
      (!select_limit->const_item() || select_limit->val_int() == 0))
    return false;

  return true;
}

/**
  Expand all '*' in list of expressions with the matching column references

  Function should not be called with no wild cards in select list

  @param  thd     thread handler

  @returns false if OK, true if error
*/

bool Query_block::setup_wild(THD *thd) {
  DBUG_TRACE;

  assert(with_wild > 0);

  // PS/SP uses arena so that changes are made permanently.
  Prepared_stmt_arena_holder ps_arena_holder(thd);

  for (auto it = fields.begin(); with_wild > 0 && it != fields.end(); ++it) {
    Item *item = *it;
    if (item->hidden) continue;
    Item_field *item_field;
    if (item->type() == Item::FIELD_ITEM &&
        (item_field = down_cast<Item_field *>(item)) &&
        item_field->is_asterisk()) {
      assert(item_field->field == nullptr);
      const bool any_privileges = item_field->any_privileges;
      Item_subselect *subsel = master_query_expression()->item;

      /*
        In case of EXISTS(SELECT * ... HAVING ...), don't use this
        transformation. The columns in HAVING will need to resolve to the
        select list. Replacing * with 1 effectively eliminates this
        possibility.
      */
      if (subsel != nullptr &&
          subsel->subquery_type() == Item_subselect::EXISTS_SUBQUERY &&
          !having_cond()) {
        /*
          It is EXISTS(SELECT * ...) and we can replace * by any constant.

          Item_int do not need fix_fields() because it is basic constant.
        */
        *it = new Item_int(NAME_STRING("Not_used"), 1,
                           MY_INT64_NUM_DECIMAL_DIGITS);
      } else {
        assert(item_field->context == &this->context);
        if (insert_fields(thd, this, item_field->db_name,
                          item_field->table_name, &fields, &it, any_privileges))
          return true;
      }

      with_wild--;
    }
  }

  return false;
}

/**
  Resolve WHERE condition and join conditions

  @param  thd     thread handler

  @returns false if success, true if error
*/

bool Query_block::setup_conds(THD *thd) {
  DBUG_TRACE;

  /*
    it_is_update set to true when tables of primary Query_block (Query_block
    which belong to LEX, i.e. most up SELECT) will be updated by
    INSERT/UPDATE/LOAD
    NOTE: using this condition helps to prevent call of prepare_check_option()
    from subquery of VIEW, because tables of subquery belongs to VIEW
    (see condition before prepare_check_option() call)
  */
  const bool it_is_update = (this == thd->lex->query_block) &&
                            thd->lex->which_check_option_applicable();
  const bool save_is_item_list_lookup = is_item_list_lookup;
  is_item_list_lookup = false;

  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));

  if (m_where_cond) {
    assert(m_where_cond->is_bool_func());
    resolve_place = Query_block::RESOLVE_CONDITION;
    thd->where = "where clause";
    if ((!m_where_cond->fixed &&
         m_where_cond->fix_fields(thd, &m_where_cond)) ||
        m_where_cond->check_cols(1))
      return true;

    assert(m_where_cond->data_type() != MYSQL_TYPE_INVALID);

    // Simplify the where condition if it's a const item
    if (m_where_cond->const_item() && !thd->lex->is_view_context_analysis() &&
        !m_where_cond->walk(&Item::is_non_const_over_literals,
                            enum_walk::POSTFIX, nullptr) &&
        simplify_const_condition(thd, &m_where_cond))
      return true;

    resolve_place = Query_block::RESOLVE_NONE;
  }

  // Resolve all join condition clauses
  if (!m_table_nest.empty() &&
      setup_join_cond(thd, &m_table_nest, it_is_update))
    return true;

  is_item_list_lookup = save_is_item_list_lookup;

  assert(thd->lex->current_query_block() == this);
  assert(!thd->is_error());
  return false;
}

/**
  Resolve join conditions for a join nest

  @param thd    thread handler
  @param tables List of tables with join conditions
  @param in_update True if used in update command that may have CHECK OPTION

  @returns false if success, true if error
*/

bool Query_block::setup_join_cond(THD *thd, mem_root_deque<Table_ref *> *tables,
                                  bool in_update) {
  DBUG_TRACE;

  for (Table_ref *tr : *tables) {
    // Traverse join conditions recursively
    if (tr->nested_join != nullptr &&
        setup_join_cond(thd, &tr->nested_join->m_tables, in_update))
      return true;

    Item **ref = tr->join_cond_ref();
    Item *join_cond = tr->join_cond();
    bool remove_cond = false;
    if (join_cond) {
      assert(join_cond->is_bool_func());
      resolve_place = Query_block::RESOLVE_JOIN_NEST;
      resolve_nest = tr;
      thd->where = "on clause";
      if ((!join_cond->fixed && join_cond->fix_fields(thd, ref)) ||
          join_cond->check_cols(1))
        return true;
      cond_count++;

      assert(tr->join_cond()->data_type() != MYSQL_TYPE_INVALID);

      if ((*ref)->const_item() && !thd->lex->is_view_context_analysis() &&
          !(*ref)->walk(&Item::is_non_const_over_literals, enum_walk::POSTFIX,
                        nullptr) &&
          simplify_const_condition(thd, ref, remove_cond))
        return true;

      resolve_place = Query_block::RESOLVE_NONE;
      resolve_nest = nullptr;
    }
    if (in_update) {
      // Process CHECK OPTION
      Table_ref *view = tr->top_table();
      if (view->is_view() && view->is_merged()) {
        if (view->prepare_check_option(thd))
          return true; /* purecov: inspected */
        tr->check_option = view->check_option;
      }
    }
  }

  return false;
}

/**
  Set NESTED_JOIN::counter=0 in all nested joins in passed list.

  @param join_list  Pass NULL. Non-NULL is reserved for recursive inner calls,
  then it is a list of nested joins to process, and may also contain base
  tables which will be ignored.
*/

void Query_block::reset_nj_counters(mem_root_deque<Table_ref *> *join_list) {
  DBUG_TRACE;
  if (join_list == nullptr) join_list = &m_table_nest;
  for (Table_ref *table : *join_list) {
    NESTED_JOIN *nested_join;
    if ((nested_join = table->nested_join)) {
      nested_join->nj_counter = 0;
      reset_nj_counters(&nested_join->m_tables);
    }
  }
}

/**
  Simplify joins replacing outer joins by inner joins whenever it's
  possible.

    The function, during a retrieval of join_list,  eliminates those
    outer joins that can be converted into inner join, possibly nested.
    It also moves the join conditions for the converted outer joins
    and from inner joins to conds.
    The function also calculates some attributes for nested joins:

    -# used_tables
    -# not_null_tables
    -# dep_tables.
    -# join_cond_dep_tables

    The first two attributes are used to test whether an outer join can
    be substituted by an inner join. The third attribute represents the
    relation 'to be dependent on' for tables. If table t2 is dependent
    on table t1, then in any evaluated execution plan table access to
    table t2 must precede access to table t2. This relation is used also
    to check whether the query contains  invalid cross-references.
    The fourth attribute is an auxiliary one and is used to calculate
    dep_tables.
    As the attribute dep_tables qualifies possibles orders of tables in the
    execution plan, the dependencies required by the straight join
    modifiers are reflected in this attribute as well.
    The function also removes all parentheses that can be removed from the join
    expression without changing its meaning.

  @note
    An outer join can be replaced by an inner join if the where condition
    or the join condition for an embedding nested join contains a conjunctive
    predicate rejecting null values for some attribute of the inner tables.

    E.g. in the query:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    the predicate t2.b < 5 rejects nulls.
    The query is converted first to:
    @code
      SELECT * FROM t1 INNER JOIN t2 ON t2.a=t1.a WHERE t2.b < 5
    @endcode
    then to the equivalent form:
    @code
      SELECT * FROM t1, t2 ON t2.a=t1.a WHERE t2.b < 5 AND t2.a=t1.a
    @endcode

    Similarly the following query:
    @code
      SELECT * from t1 LEFT JOIN (t2, t3) ON t2.a=t1.a t3.b=t1.b
        WHERE t2.c < 5
    @endcode
    is converted to:
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a t3.b=t1.b
    @endcode

    One conversion might trigger another:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a
                       LEFT JOIN t3 ON t3.b=t2.b
        WHERE t3 IS NOT NULL =>
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t1.a, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b =>
      SELECT * FROM t1, t2, t3
        WHERE t3 IS NOT NULL AND t3.b=t2.b AND t2.a=t1.a
    @endcode

    The function removes all unnecessary parentheses from the expression
    produced by the conversions.
    E.g.
    @code
      SELECT * FROM t1, (t2, t3) WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b
    @endcode
    finally is converted to:
    @code
      SELECT * FROM t1, t2, t3 WHERE t2.c < 5 AND t2.a=t1.a AND t3.b=t1.b
    @endcode

    It also will remove parentheses from the following queries:
    @code
      SELECT * from (t1 LEFT JOIN t2 ON t2.a=t1.a) LEFT JOIN t3 ON t3.b=t2.b
      SELECT * from (t1, (t2,t3)) WHERE t1.a=t2.a AND t2.b=t3.b.
    @endcode

    The benefit of this simplification procedure is that it might return
    a query for which the optimizer can evaluate execution plans with more
    join orders. With a left join operation the optimizer does not
    consider any plan where one of the inner tables is before some of outer
    tables.

  IMPLEMENTATION
    The function is implemented by a recursive procedure.  On the recursive
    ascent all attributes are calculated, all outer joins that can be
    converted are replaced and then all unnecessary parentheses are removed.
    As join list contains join tables in the reverse order sequential
    elimination of outer joins does not require extra recursive calls.

  SEMI-JOIN NOTES
    Remove all semi-joins that have are within another semi-join (i.e. have
    an "ancestor" semi-join nest)

  EXAMPLES
    Here is an example of a join query with invalid cross references:
    @code
      SELECT * FROM t1 LEFT JOIN t2 ON t2.a=t3.a LEFT JOIN t3 ON t3.b=t1.b
    @endcode

  @param thd         thread handler
  @param join_list   list representation of the join to be converted
  @param top         true <=> cond is the where condition
  @param in_sj       true <=> processing semi-join nest's children
  @param[in,out] cond In: condition to which the join condition for converted
                          outer joins is to be added;
                      Out: new condition
  @param changelog   Don't specify this parameter, it is reserved for
                     recursive calls inside this function

  @returns true for error, false for success
*/
bool Query_block::simplify_joins(THD *thd,
                                 mem_root_deque<Table_ref *> *join_list,
                                 bool top, bool in_sj, Item **cond,
                                 uint *changelog) {
  /*
    Each type of change done by this function, or its recursive calls, is
    tracked in a bitmap:
  */
  enum change {
    NONE = 0,
    OUTER_JOIN_TO_INNER = 1 << 0,
    JOIN_COND_TO_WHERE = 1 << 1,
    PAREN_REMOVAL = 1 << 2,
    SEMIJOIN = 1 << 3
  };
  uint changes = 0;          // To keep track of changes.
  if (changelog == nullptr)  // This is the top call.
    changelog = &changes;

  Table_ref *prev_table = nullptr;
  const bool straight_join = active_options() & SELECT_STRAIGHT_JOIN;
  DBUG_TRACE;

  /*
    Try to simplify join operations from join_list.
    The most outer join operation is checked for conversion first.
    join_list is a join nest, and 'cond' is a condition which acts as a filter
    applied to the nest's operation (post-filter).
    Thus, considering this example:
    (A LEFT JOIN B ON JC) WHERE W ,
    we'll "confront W with A LEFT JOIN B": this will, recursively,
    - confront W with B,
    - confront W with A.
    Because W is external to the nest, if W would be false when B is
    NULL-complemented we know we can change LEFT JOIN to JOIN.
    We will not confront JC with B or A, it wouldn't make sense, as JC isn't a
    post-filter for their join operation.
    Another example:
    (A LEFT JOIN (B LEFT JOIN C ON JC2) ON JC1) WHERE W ,
    while confronting W with (B LEFT JOIN C), we will also, as first step,
    confront JC1 with (B LEFT JOIN C), and thus recursively confront JC1
    with C and then with B.
    Another example:
    (A LEFT JOIN (B SEMI JOIN C ON JC2) ON JC1) WHERE W ,
    while confronting W with (B SEMI JOIN C), if W is known false we will
  */
  for (Table_ref *table : *join_list) {
    table_map used_tables;
    table_map not_null_tables = table_map(0);

    NESTED_JOIN *nested_join = table->nested_join;
    if (nested_join != nullptr) {
      /*
         If the element of join_list is a nested join apply
         the procedure to its nested join list first.
         This confronts the join nest's condition with each member of the
         nest.
      */
      if (table->join_cond() != nullptr) {
        Item *join_cond = table->join_cond();
        /*
           If a join condition JC is attached to the table,
           check all null rejected predicates in this condition.
           If such a predicate over an attribute belonging to
           an inner table of an embedded outer join is found,
           the outer join is converted to an inner join and
           the corresponding join condition is added to JC.
        */
        if (simplify_joins(
                thd, &nested_join->m_tables,
                false,  // not 'top' as it's not WHERE.
                // SJ nests can dissolve into upper SJ or anti SJ nests:
                in_sj || table->is_sj_or_aj_nest(), &join_cond, changelog))
          return true;

        if (join_cond != table->join_cond()) {
          assert(join_cond != nullptr);
          table->set_join_cond(join_cond);
          /*
            For a semi-join or anti-join table nest, if the join condition
            has been reduced to a constant value, it means that factored out
            join condition operands can be removed.
          */
          if (table->is_sj_or_aj_nest() && join_cond->const_item()) {
            clear_sj_expressions(nested_join);
          }
        }
      }
      nested_join->used_tables = table_map(0);
      nested_join->not_null_tables = table_map(0);
      // This recursively confronts "cond" with each member of the nest
      if (simplify_joins(thd, &nested_join->m_tables,
                         top,  // if it was WHERE it still is
                         in_sj || table->is_sj_or_aj_nest(), cond, changelog))
        return true;
      used_tables = nested_join->used_tables;
      not_null_tables = nested_join->not_null_tables;
    } else {
      used_tables = table->map();
      if (*cond != nullptr) not_null_tables = (*cond)->not_null_tables();
    }

    if (table->embedding != nullptr) {
      table->embedding->nested_join->used_tables |= used_tables;
      table->embedding->nested_join->not_null_tables |= not_null_tables;
    }

    if (!table->outer_join || (used_tables & not_null_tables)) {
      /*
        For some of the inner tables there are conjunctive predicates
        that reject nulls => the outer join can be replaced by an inner join.
      */
      if (table->outer_join) {
        *changelog |= OUTER_JOIN_TO_INNER;
        table->outer_join = false;
      }
      if (table->join_cond() != nullptr) {
        *changelog |= JOIN_COND_TO_WHERE;
        /* Add join condition to the WHERE or upper-level join condition. */
        if (*cond != nullptr) {
          Item *i1 = *cond;
          Item *i2 = table->join_cond();
          /*
            User supplied stored procedures in the query can violate row-level
            filter enforced by a view. So make sure view's filter conditions
            precede any other conditions.
          */
          if (table->is_view() && i1->has_stored_program()) {
            std::swap(i1, i2);
          }

          Item_cond_and *new_cond =
              down_cast<Item_cond_and *>(and_conds(i1, i2));
          if (new_cond == nullptr) return true;
          new_cond->apply_is_true();
          /*
            It is always a new item as both the upper-level condition and a
            join condition existed
          */
          assert(!new_cond->fixed);
          Item *cond_after_fix = new_cond;
          if (new_cond->fix_fields(thd, &cond_after_fix)) return true;

          if (new_cond == cond_after_fix) {
          }
          *cond = cond_after_fix;
        } else {
          *cond = table->join_cond();
        }
        table->set_join_cond(nullptr);
      }
    }

    // A table is traversed when 'cond' is WHERE, and when 'cond' is the join
    // condition of any nest containing the table. Some bitmaps can be set
    // only after all traversals of this table i.e. when 'cond' is WHERE.
    if (!top) continue;

    /*
      Only inner tables of non-convertible outer joins remain with
      the join condition.
    */
    if (table->join_cond() != nullptr) {
      table->dep_tables |= table->join_cond()->used_tables();
      // At this point the joined tables always have an embedding join nest:
      assert(table->embedding != nullptr);
      table->dep_tables &= ~table->embedding->nested_join->used_tables;

      // Embedding table depends on tables used in embedded join conditions.
      table->embedding->join_cond_dep_tables |=
          table->join_cond()->used_tables();
    }

    if (prev_table != nullptr) {
      /* The order of tables is reverse: prev_table follows table */
      if (prev_table->straight || straight_join)
        prev_table->dep_tables |= used_tables;
      if (prev_table->join_cond() != nullptr) {
        prev_table->dep_tables |= table->join_cond_dep_tables;
        table_map prev_used_tables = prev_table->nested_join != nullptr
                                         ? prev_table->nested_join->used_tables
                                         : prev_table->map();
        /*
          If join condition contains no reference to outer tables
          we still make the inner tables dependent on the outer tables,
          as the outer must go before the inner since the executor requires
          that at least one outer table is before the inner tables.
          It would be enough to set dependency only on one outer table
          for them. Yet this is really a rare case.
          Note:
          PSEUDO_TABLE_BITS mask should not be counted as it
          prevents update of inner table dependencies.
          For example it might happen if RAND()/COUNT(*) function
          is used in JOIN ON clause.
        */
        if ((((prev_table->join_cond()->used_tables() & ~PSEUDO_TABLE_BITS) &
              ~prev_used_tables) &
             used_tables) == 0) {
          prev_table->dep_tables |= used_tables;
        }
      }
    }
    prev_table = table;
  }

  /*
    Flatten nested joins that can be flattened.
    no join condition and not a semi-join => can be flattened.
  */
  for (auto li = join_list->begin(); li != join_list->end();) {
    Table_ref *table = *li;
    NESTED_JOIN *nested_join = table->nested_join;
    if (table->is_sj_nest() && !in_sj) {
      /*
        If this is a semi-join that is not contained within another semi-join,
        leave it intact.
        Otherwise it is flattened, for example
        A SJ (B SJ (C)) becomes the equivalent A SJ (B JOIN C),
        A AJ (B SJ (C)) becomes the equivalent A AJ (B JOIN C),
        While dissolving a SJ nest into an AJ nest is ok (for the AJ
        this may lead to duplicates but AJ only cares for "at least
        one match"), dissolving an AJ nest into a SJ is not ok:
        A SJ (B AJ (C)) is not equivalent to A SJ (B JOIN C);
        that is why the next if() block is guarded by !join_cond() which takes
        care of that.
        Note that when dissolving the SJ nest, its condition isn't lost as it
        has previously been added to WHERE or outer nest's condition in
        convert_subquery_to_semijoin().
      */
      *changelog |= SEMIJOIN;
    } else if (nested_join != nullptr && table->join_cond() == nullptr) {
      *changelog |= PAREN_REMOVAL;
      for (Table_ref *tbl : nested_join->m_tables) {
        tbl->embedding = table->embedding;
        tbl->join_list = table->join_list;
        tbl->dep_tables |= table->dep_tables;
      }
      li = join_list->erase(li);
      li = join_list->insert(li, nested_join->m_tables.begin(),
                             nested_join->m_tables.end());

      // Don't advance li; we want to process the newly added tables.
      continue;
    }
    ++li;
  }

  if (changes) {
    Opt_trace_context *trace = &thd->opt_trace;
    if (unlikely(trace->is_started())) {
      Opt_trace_object trace_wrapper(trace);
      Opt_trace_object trace_object(trace, "transformations_to_nested_joins");
      {
        Opt_trace_array trace_changes(trace, "transformations");
        if (changes & SEMIJOIN) trace_changes.add_alnum("semijoin");
        if (changes & OUTER_JOIN_TO_INNER)
          trace_changes.add_alnum("outer_join_to_inner_join");
        if (changes & JOIN_COND_TO_WHERE)
          trace_changes.add_alnum("JOIN_condition_to_WHERE");
        if (changes & PAREN_REMOVAL)
          trace_changes.add_alnum("parenthesis_removal");
      }
      // the newly transformed query is worth printing
      opt_trace_print_expanded_query(thd, this, &trace_object);
    }
  }
  return false;
}

/**
  Record join nest info in the select block.

  After simplification of inner join, outer join and semi-join structures:
   - record the remaining semi-join structures in the enclosing query block.
   - record transformed join conditions in Table_ref objects.

  This function is called recursively for each join nest and/or table
  in the query block.

  @param tables List of tables and join nests

  @return False if successful, True if failure
*/
bool Query_block::record_join_nest_info(mem_root_deque<Table_ref *> *tables) {
  for (Table_ref *table : *tables) {
    if (table->nested_join == nullptr) {
      if (table->join_cond()) outer_join |= table->map();
      continue;
    }

    if (record_join_nest_info(&table->nested_join->m_tables)) return true;
    /*
      sj_inner_tables is set properly later in pull_out_semijoin_tables().
      This assignment is required in case pull_out_semijoin_tables()
      is not called.
    */
    if (table->is_sj_or_aj_nest())
      table->sj_inner_tables = table->nested_join->used_tables;

    if (table->is_sj_or_aj_nest()) {
      sj_nests.push_back(table);
    }

    if (table->join_cond()) outer_join |= table->nested_join->used_tables;
  }
  return false;
}

/**
  Update table reference information for conditions and expressions due to
  query blocks having been merged in from derived tables/views and due to
  semi-join transformation.

  This is needed for two reasons:

  1. Since table numbers are changed, we need to update used_tables
     information for all conditions and expressions that are possibly touched.

  2. For semi-join, some column references are changed from outer references
     to local references.

  The function needs to recursively walk down into join nests,
  in order to cover all conditions and expressions.

  For a semi-join, tables from the subquery are added last in the query block.
  This means that conditions and expressions from the outer query block
  are unaffected. But all conditions inside the semi-join nest, including
  join conditions, must have their table numbers changed.

  For a derived table/view, tables from the subquery are merged into the
  outer query, and this function is called for every derived table that is
  merged in. This algorithm only works when derived tables are merged in
  the order of their original table numbers.

  A hypothetical example with a triple self-join over a mergeable view:

    CREATE VIEW v AS SELECT t1.a, t2.b FROM t1 JOIN t2 USING (a);
    SELECT v1.a, v1.b, v2.b, v3.b
    FROM v AS v1 JOIN v AS v2 ON ... JOIN v AS v3 ON ...;

  The analysis starts with three tables v1, v2 and v3 having numbers 0, 1, 2.
  First we merge in v1, so we get (t1, t2, v2, v3). v2 and v3 are shifted up.
  Tables from v1 need to have their table numbers altered (actually they do not
  since both old and new numbers are 0 and 1, but this is a special case).
  v2 and v3 are not merged in yet, so we delay pullout on them until they
  are merged. Conditions and expressions from the outer query are not resolved
  yet, so regular resolving will take of them later.
  Then we merge in v2, so we get (t1, t2, t1, t2, v3). The tables from this
  view gets numbers 2 and 3, and v3 gets number 4.
  Because v2 had a higher number than the tables from v1, the join nest
  representing v1 is unaffected. And v3 is still not merged, so the only
  join nest we need to consider is v2.
  Finally we merge in v3, and then we have tables (t1, t2, t1, t2, t1, t2),
  with numbers 0 through 5.
  Again, since v3 has higher number than any of the already merged in views,
  only this join nest needs the pullout.

  @param parent_query_block  Query block being merged into
  @param removed_query_block Query block that is removed (subquery)
  @param tr             Table object this pullout is applied to
  @param table_adjust   Number of positions that a derived table nest is
                        adjusted, used to fix up semi-join related fields.
                        Tables are adjusted from position N to N+table_adjust
  @param lateral_deps   Lateral dependencies of the unit owning
  removed_query_block
*/

static void fix_tables_after_pullout(Query_block *parent_query_block,
                                     Query_block *removed_query_block,
                                     Table_ref *tr, uint table_adjust,
                                     table_map lateral_deps) {
  if (tr->is_merged()) {
    // Update select list of merged derived tables:
    for (Field_translator *transl = tr->field_translation;
         transl < tr->field_translation_end; transl++) {
      assert(transl->item->fixed);
      transl->item->fix_after_pullout(parent_query_block, removed_query_block);
    }
    // Update used table info for the WHERE clause of the derived table
    assert(!tr->derived_where_cond || tr->derived_where_cond->fixed);
    if (tr->derived_where_cond)
      tr->derived_where_cond->fix_after_pullout(parent_query_block,
                                                removed_query_block);
  }

  /*
    If join_cond() is fixed, it contains a join condition from a subquery
    that has already been resolved. Call fix_after_pullout() to update
    used table information since table numbers may have changed.
    If join_cond() is not fixed, it contains a condition that was generated
    in the derived table merge operation, which will be fixed later.
    This condition may also contain a fixed part, but this is saved as
    derived_where_cond and is pulled out explicitly.
  */
  if (tr->join_cond() && tr->join_cond()->fixed)
    tr->join_cond()->fix_after_pullout(parent_query_block, removed_query_block);

  if (tr->nested_join) {
    // In case a derived table is merged-in, these fields need adjustment:
    tr->nested_join->sj_corr_tables <<= table_adjust;
    tr->nested_join->sj_depends_on <<= table_adjust;

    // If the removed query block is from a LATERAL derived table, and
    // contains a semi-join nest, this nest may depend on the lateral
    // dependencies, and if then, these should now be recorded as
    // local dependencies of the nest. But it's impossible to know if this is
    // the case, as the members below don't mention outer references. Be
    // conservative and add dependencies unconditionally. At least this will
    // prevent materialization.
    tr->nested_join->sj_corr_tables |= lateral_deps;
    tr->nested_join->sj_depends_on |= lateral_deps;

    for (Table_ref *child : tr->nested_join->m_tables) {
      fix_tables_after_pullout(parent_query_block, removed_query_block, child,
                               table_adjust, lateral_deps);
    }
  }
  if (tr->is_derived() && tr->table &&
      tr->derived_query_expression()->uncacheable & UNCACHEABLE_DEPENDENT) {
    /*
      It's a materialized derived table which is being pulled up.
      If it has an outer reference, and this ref belongs to parent_query_block,
      then the derived table will need re-materialization as if it were
      LATERAL, not just once per execution of parent_query_block.
      We thus compute its used_tables in the new context, to decide.
    */
    Query_expression *unit = tr->derived_query_expression();
    unit->m_lateral_deps = OUTER_REF_TABLE_BIT;
    unit->fix_after_pullout(parent_query_block, removed_query_block);
    unit->m_lateral_deps &= ~PSEUDO_TABLE_BITS;
    tr->dep_tables |= unit->m_lateral_deps;
    /*
      If m_lateral_deps!=0, some outer ref is now a neighbour in FROM: we have
      made 'tr' LATERAL.
      Note that 'tr' might be a common table expression: it means we now have a
      "lateral CTE".
    */
  }
}

/**
  Fix used tables information for a subquery after query transformations.
  This is for transformations where the subquery remains a subquery - it is
  not merged, it merely moves up by effect of a transformation on a containing
  query block.
  Most actions here involve re-resolving information for conditions
  and items belonging to the subquery.
  If the subquery contains an outer reference into removed_query_block or
  parent_query_block, the relevant information is updated by
  Item_ident::fix_after_pullout().
*/
void Query_expression::fix_after_pullout(Query_block *parent_query_block,
                                         Query_block *removed_query_block)

{
  // Go through all query specification objects of the subquery and re-resolve
  // all relevant expressions belonging to them.
  for (Query_block *sel = first_query_block(); sel;
       sel = sel->next_query_block()) {
    sel->fix_after_pullout(parent_query_block, removed_query_block);
  }
  // @todo figure out if we need to do it for fake_query_block too.
}

/// @see Query_expression::fix_after_pullout
void Query_block::fix_after_pullout(Query_block *parent_query_block,
                                    Query_block *removed_query_block) {
  if (where_cond())
    where_cond()->fix_after_pullout(parent_query_block, removed_query_block);

  /*
    Join conditions can contain an outer reference; and
    derived table merging changes WHERE to a join condition, which thus can
    have an outer reference. So we have to call fix_after_pullout() on join
    conditions. The reference may also be located in a derived table used by
    this subquery. fix_tables_after_pullout() will handle the two cases.
    table_adjust and lateral_deps are 0 because we're not merging these tables
    up.
  */
  for (Table_ref *tr : m_table_nest) {
    fix_tables_after_pullout(parent_query_block, removed_query_block, tr,
                             /*table_adjust=*/0, /*lateral_deps=*/0);
  }

  if (having_cond())
    having_cond()->fix_after_pullout(parent_query_block, removed_query_block);

  for (Item *item : visible_fields()) {
    item->fix_after_pullout(parent_query_block, removed_query_block);
  }

  /* Re-resolve ORDER BY and GROUP BY fields */

  for (ORDER *order = order_list.first; order; order = order->next)
    (*order->item)->fix_after_pullout(parent_query_block, removed_query_block);

  for (ORDER *group = group_list.first; group; group = group->next)
    (*group->item)->fix_after_pullout(parent_query_block, removed_query_block);
}

/**
 Remove SJ outer/inner expressions.

 @param nested_join         join nest
*/

void Query_block::clear_sj_expressions(NESTED_JOIN *nested_join) {
  nested_join->sj_outer_exprs.clear();
  nested_join->sj_inner_exprs.clear();
  assert(sj_nests.empty());
}

/**
  Build equality conditions using outer expressions and inner
  expressions. If the equality condition is not constant, add
  it to the semi-join condition. Otherwise, evaluate it and
  remove the constant expressions from the
  outer/inner expressions list if the result is true. If the
  result is false, remove all the expressions in outer/inner
  expression list and attach an always false condition
  to semijoin condition.

  @param thd            Thread context
  @param nested_join    Join nest
  @param subq_query_block    Query block for the subquery
  @param outer_tables_map Map of tables from original outer query block
  @param[in,out] sj_cond   Semi-join condition to be constructed
                           Contains non-equalities on input.
  @param[out]    simple_const true if the returned semi-join condition is
                              a simple true or false predicate, false otherwise.

  @return false if success, true if error
*/
bool Query_block::build_sj_cond(THD *thd, NESTED_JOIN *nested_join,
                                Query_block *subq_query_block,
                                table_map outer_tables_map, Item **sj_cond,
                                bool *simple_const) {
  *simple_const = false;

  Item *new_cond = nullptr;

  auto ii = nested_join->sj_inner_exprs.begin();
  auto oi = nested_join->sj_outer_exprs.begin();
  while (ii != nested_join->sj_inner_exprs.end() &&
         oi != nested_join->sj_outer_exprs.end()) {
    bool should_remove = false;
    Item *inner = *ii;
    Item *outer = *oi;
    /*
      Ensure that all involved expressions are pulled out after transformation.
      (If they are already out, this is a no-op).
    */
    outer->fix_after_pullout(this, subq_query_block);
    inner->fix_after_pullout(this, subq_query_block);

    Item_func_eq *item_eq = new Item_func_eq(outer, inner);
    if (item_eq == nullptr) return true; /* purecov: inspected */
    Item *predicate = item_eq;
    if (!item_eq->fixed && item_eq->fix_fields(thd, &predicate)) return true;

    // Evaluate if the condition is on const expressions
    if (predicate->const_item() &&
        !(predicate)->walk(&Item::is_non_const_over_literals,
                           enum_walk::POSTFIX, nullptr)) {
      bool cond_value = true;

      /* Push ignore / strict error handler */
      Ignore_error_handler ignore_handler;
      Strict_error_handler strict_handler;
      if (thd->lex->is_ignore())
        thd->push_internal_handler(&ignore_handler);
      else if (thd->is_strict_mode())
        thd->push_internal_handler(&strict_handler);

      bool err = eval_const_cond(thd, predicate, &cond_value);
      /* Pop ignore / strict error handler */
      if (thd->lex->is_ignore() || thd->is_strict_mode())
        thd->pop_internal_handler();

      if (err) return true;

      if (cond_value) {
        /*
          Remove the expression from inner/outer expression list if the
          const condition evaluates to true as Item_cond::fix_fields will
          remove the condition later.
        */
        should_remove = true;
      } else {
        /*
          Remove all the expressions in inner/outer expression list if
          one of condition evaluates to always false. Add an always false
          condition to semi-join condition.
        */
        nested_join->sj_inner_exprs.clear();
        nested_join->sj_outer_exprs.clear();
        Item *new_item = new Item_func_false();
        if (new_item == nullptr) return true;
        (*sj_cond) = new_item;
        *simple_const = true;
        return false;
      }
    }
    /*
      If the selected expression has a reference to our query block, add it as
      a non-trivially correlated reference (to avoid materialization).
      The case of yet-more-outer references is handled like this:
      - if this nest is part of a LATERAL derived table, which is later
        merged, fix_tables_after_pullout will update sj_corr_tables (with its
        lateral_deps argument).
      - if this nest is part of a subquery which later becomes a
        semi/anti-join nest, it will be dissolved into the new parent nest, so
        the inner nest's sj_corr_tables will be unused, while the parent's
        will be correct as it will be computed from the concatenated new WHERE
        condition.
    */
    nested_join->sj_corr_tables |= inner->used_tables() & outer_tables_map;

    if (should_remove) {
      ii = nested_join->sj_inner_exprs.erase(ii);
      oi = nested_join->sj_outer_exprs.erase(oi);
    } else {
      new_cond = and_items(new_cond, predicate);
      if (new_cond == nullptr) return true; /* purecov: inspected */

      ++ii, ++oi;
    }
  }
  /*
    Semijoin processing expects at least one inner/outer expression
    in the list if there is a sj_nest present. This is required for semi-join
    materialization and loose scan.
  */
  if (nested_join->sj_inner_exprs.empty()) {
    Item *const_item = new Item_int(1);
    if (const_item == nullptr) return true;
    nested_join->sj_inner_exprs.push_back(const_item);
    nested_join->sj_outer_exprs.push_back(const_item);
    new_cond = new Item_func_true();
    if (new_cond == nullptr) return true;
    *simple_const = true;
  }
  (*sj_cond) = and_items(*sj_cond, new_cond);
  if (*sj_cond == nullptr) return true; /* purecov: inspected */

  return false;
}

/// Context object used by semijoin equality decorrelation code.
class Semijoin_decorrelation {
  mem_root_deque<Item *> *sj_outer_exprs, *sj_inner_exprs;
  /// If nullptr: only a=b is decorrelated.
  /// Otherwise, a OP b is decorrelated for OP in <>, >=, >, <=, <, and
  /// for each decorrelated SJ outer/inner pair, located at position N
  /// in sj_outer_exprs and sj_inner_exprs, we store, at the
  /// same position in op_types, the operator's type code representing "outer OP
  /// inner" (for example, LE_FUNC for outer<=inner as well as inner>=outer).
  Mem_root_array<Item_func::Functype> *op_types;

 public:
  Semijoin_decorrelation(mem_root_deque<Item *> *sj_outer_exprs_arg,
                         mem_root_deque<Item *> *sj_inner_exprs_arg,
                         Mem_root_array<Item_func::Functype> *op_types_arg)
      : sj_outer_exprs(sj_outer_exprs_arg),
        sj_inner_exprs(sj_inner_exprs_arg),
        op_types(op_types_arg) {}
  void add_outer(Item *i) { sj_outer_exprs->push_back(i); }
  void add_inner(Item *i) { sj_inner_exprs->push_back(i); }
  bool decorrelate_only_eq() const { return op_types == nullptr; }
  bool add_op_type(Item_func::Functype op_type) {
    return (op_types != nullptr) ? op_types->push_back(op_type) : false;
  }
  Item_func::Functype op_type_at(int j) const {
    return (op_types != nullptr) ? op_types->at(j) : Item_func::EQ_FUNC;
  }
};

/**
  Try to decorrelate an (in)equality node. The node can be decorrelated if one
  argument contains only outer references and the other argument contains
  references only to local tables.
  Both arguments should be deterministic.
  const-for-execution values are accepted in both arguments.

  @note that a predicate like '(a,b) IN ((c,d))' is changed to two equalities
  only during optimization, so at the present stage it isn't decorrelate-able.

  @param sj_decor Object for recording the decorrelated expressions
  @param func    The query function node
  @param[out] was_correlated = true if comparison is correlated and the
                 the expressions are added to sj_nest.

  @returns false if success, true if error
*/

static bool decorrelate_equality(Semijoin_decorrelation &sj_decor,
                                 Item_func *func, bool *was_correlated) {
  *was_correlated = false;
  Item_bool_func2 *bool_func = down_cast<Item_bool_func2 *>(func);
  Item *const left = bool_func->arguments()[0];
  Item *const right = bool_func->arguments()[1];
  Item *inner = nullptr;
  Item *outer = nullptr;
  table_map left_used_tables = left->used_tables() & ~INNER_TABLE_BIT;
  table_map right_used_tables = right->used_tables() & ~INNER_TABLE_BIT;

  /*
    Predicates that have non-deterministic elements are not decorrelated,
    see explanation for Query_block::decorrelate_condition().
  */
  if ((left_used_tables & RAND_TABLE_BIT) ||
      (right_used_tables & RAND_TABLE_BIT))
    return false;

  if (left_used_tables == OUTER_REF_TABLE_BIT) {
    outer = left;
  } else if (!(left_used_tables & OUTER_REF_TABLE_BIT)) {
    inner = left;
  }
  if (right_used_tables == OUTER_REF_TABLE_BIT) {
    outer = right;
  } else if (!(right_used_tables & OUTER_REF_TABLE_BIT)) {
    inner = right;
  }
  if (inner == nullptr || outer == nullptr) return false;

  // Equalities over row items cannot be decorrelated
  if (outer->type() == Item::ROW_ITEM) return false;

  sj_decor.add_outer(outer);
  sj_decor.add_inner(inner);
  if (sj_decor.add_op_type(
          // use canonical form "outer OP inner":
          (outer == left) ? bool_func->functype() : bool_func->rev_functype()))
    return true;

  *was_correlated = true;

  return false;
}

static inline bool can_decorrelate_operator(Item_func *func, bool only_eq) {
  auto op_type = func->functype();
  switch (op_type) {
    case Item_func::EQ_FUNC:
      return true;
    case Item_func::NE_FUNC:
    case Item_func::LT_FUNC:
    case Item_func::LE_FUNC:
    case Item_func::GT_FUNC:
    case Item_func::GE_FUNC:
      return !only_eq;
    default:
      return false;
  }
}

/**
  Decorrelate the WHERE clause or a join condition of a subquery used in
  an IN or EXISTS predicate.
  Correlated predicates are removed from the condition and added to the
  supplied semi-join nest.
  The predicate must be either a simple (in)equality, or an AND condition that
  contains one or more simple equalities, in order for decorrelation to be
  possible.

  @param sj_decor  Object for recording the decorrelated expressions
  @param join_nest Nest containing join condition to be decorrelated
                   =NULL: decorrelate the WHERE condition

  @returns false if success, true if error

  Decorrelation for subqueries containing non-deterministic components:
  --------------------------------------------------------------------

  There are two types of IN and EXISTS queries with non-deterministic
  functions that may be meaningful (the EXISTS queries below are correlated
  equivalents of the respective IN queries):

  1. Non-deterministic function as substitute for expression from outer
     query block:

  A SELECT * FROM t1
    WHERE RAND() IN (SELECT t2.x FROM t2)

  B SELECT * FROM t1
    WHERE EXISTS (SELECT * FROM t2 WHERE RAND() = t2.x);

  Pick a set of random rows that matches against a fixed set (the subquery).

  The intuitive interpretation of the IN subquery is that the random function
  is evaluated per row of the outer query block, whereas in the EXISTS subquery,
  it should be evaluated per row of the inner query block, and the subquery
  is evaluated once per row of the outer query block.

  2. Non-deterministic function as substitute for expression from inner
     query block:

  A SELECT * FROM t1
    WHERE t1.x IN (SELECT RAND() FROM t2)

  B SELECT * FROM t1
    WHERE EXISTS (SELECT * FROM t2 WHERE RAND() = t1.x);

  This is another way of picking a random row, but now the non-determinism
  occurs in the inner query block.

  The user will expect that only query 1A has the evaluation of
  non-deterministic functions being performed in the outer query block.
  Using decorrelation for query 1B would change the apparent semantics of
  the query.

  The purpose of decorrelation is to be able to use more execution strategies.
  Without decorrelation, EXISTS is limited to FirstMatch and DupsWeedout
  strategies. Decorrelation enables LooseScan and Materialization.
  We can rule out LooseScan for case 2B, since it requires an indexed column
  from the subquery, and for case 1B, since it requires that the outer table
  is partitioned according to the distinct values of the index, and random
  values do not fulfill that partitioning requirement.

  The only strategy left is Materialization. With decorrelation, 1B would be
  evaluated like 1A, which is not the intuitive way. 2B would also be
  implemented like 2A, meaning that evaluation of non-deterministic functions
  would move to the materialization function.

  Thus, the intuitive interpretation is to avoid materialization for subqueries
  with non-deterministic components in the inner query block, and hence
  such predicates will not be decorrelated.
*/

bool Query_block::decorrelate_condition(Semijoin_decorrelation &sj_decor,
                                        Table_ref *join_nest) {
  Item *base_cond =
      join_nest == nullptr ? where_cond() : join_nest->join_cond();
  Item_cond *cond;
  Item_func *func;

  assert(base_cond != nullptr);

  if (base_cond->type() == Item::FUNC_ITEM &&
      (func = down_cast<Item_func *>(base_cond)) &&
      can_decorrelate_operator(func, sj_decor.decorrelate_only_eq())) {
    bool was_correlated;
    if (decorrelate_equality(sj_decor, func, &was_correlated)) return true;
    if (was_correlated) {  // The simple equality has been decorrelated
      if (join_nest == nullptr)
        set_where_cond(nullptr);
      else  // Join conditions cannot be empty so install a TRUE value
        join_nest->set_join_cond(new Item_func_true());
    }
  } else if (base_cond->type() == Item::COND_ITEM &&
             (cond = down_cast<Item_cond *>(base_cond)) &&
             cond->functype() == Item_func::COND_AND_FUNC) {
    List<Item> *args = cond->argument_list();
    List_iterator<Item> li(*args);
    Item *item;
    while ((item = li++)) {
      if (item->type() == Item::FUNC_ITEM &&
          (func = down_cast<Item_func *>(item)) &&
          can_decorrelate_operator(func, sj_decor.decorrelate_only_eq())) {
        bool was_correlated;
        if (decorrelate_equality(sj_decor, func, &was_correlated)) return true;
        if (was_correlated) li.remove();
      }
    }
    if (args->is_empty()) {  // All predicates have been decorrelated
      if (join_nest == nullptr)
        set_where_cond(nullptr);
      else  // Join conditions cannot be empty so install a TRUE value
        join_nest->set_join_cond(new Item_func_true());
    }
  }
  return false;
}

bool Query_block::allocate_grouping_sets(THD *thd) {
  auto max_group_by_elements = GetMaximumNumGrpByColsSupported(olap);

  if (group_list.elements > static_cast<uint>(max_group_by_elements)) {
    /* The number of Grouping sets cannot be greater than INT_MAX as IsBitSet
     * take integer as the input bit*/
    my_error(ER_TOO_MANY_GROUP_BY_MODIFIER_BRANCHES, MYF(0),
             GroupByModifierString(olap), max_group_by_elements);
    return true;
  }
  m_num_grouping_sets = (olap == ROLLUP_TYPE)
                            ? group_list.elements + 1
                            : pow(static_cast<double>(2),
                                  static_cast<double>(group_list.elements));

  assert(m_num_grouping_sets != 0);

  /*  Allocate bitmap for grouping sets. */
  for (ORDER *grp = group_list.first; grp != nullptr; grp = grp->next) {
    grp->grouping_set_info =
        pointer_cast<MY_BITMAP *>(thd->alloc(sizeof(MY_BITMAP)));
    if (grp->grouping_set_info == nullptr) {
      return true;
    }
    my_bitmap_map *bitbuf = pointer_cast<my_bitmap_map *>(
        thd->alloc(bitmap_buffer_size(m_num_grouping_sets)));
    bitmap_init(grp->grouping_set_info, bitbuf, m_num_grouping_sets);
  }
  return false;
}

/**
  Populate the grouping set bitvector if the query block has non-primitive
  grouping. If the non-primitive grouping is ROLLUP or CUBE, the grouping sets
  have to be computed. The representation of the grouping set is done using a
  bitfield in the ORDER object.
  case ROLLUP : Say the query has GROUP BY ROLLUP (a,b) then the grouping sets
  will be (a,b) (a) () where () represents single group aggregate without any
  grouping. Here there are 3 grouping sets ranging from 0 to 2 and 0 is the
  single group aggregate. The bitfield associated with GROUP BY element 'a'
  will be 3 (i,e. 2+1) The bitfield associated with Group by element 'b' will
  be 2 as it is part of only set number 2.
  case CUBE: Say the query has GROUP BY CUBE (a,b) then the grouping sets
  will be (a,b) (a) (b) (). The number of grouping sets will be (2^n)
  where n is the number of elements in the GROUP BY list. The bitfield
  associated with Group by element 'a' will be 6 (i.e. 4+2).
  The bitfield associated with Group by element 'b' will be 1.
*/
bool Query_block::populate_grouping_sets(THD *thd) {
  assert(group_list.elements != 0 && olap != UNSPECIFIED_OLAP_TYPE);

  if (allocate_grouping_sets(thd)) {
    return true;
  }

  bool rollup = (olap == ROLLUP_TYPE);
  int gby_idx = 0;
  for (ORDER *grp = group_list.first; grp != nullptr;
       grp = grp->next, gby_idx++) {
    for (int gs = 1; gs < m_num_grouping_sets; gs++) {
      if ((rollup && gby_idx < gs) || (!rollup && IsBitSet(gby_idx, gs))) {
        bitmap_set_bit(grp->grouping_set_info, gs);
      }
    }
  }

  return false;
}

bool walk_join_list(mem_root_deque<Table_ref *> &list,
                    std::function<bool(Table_ref *)> action) {
  for (Table_ref *tl : list) {
    if (action(tl)) return true;
    if (tl->nested_join != nullptr &&
        walk_join_list(tl->nested_join->m_tables, action))
      return true;
  }
  return false;
}

/**
  Builds the list of SJ outer/inner expressions
  @param      thd            Connection handle
  @param[out] sj_outer_exprs Will add outer expressions here
  @param[out] sj_inner_exprs Will add inner expressions here
  @param      subq_pred      Item for the subquery
  @param      subq_query_block    Single query block for the subquery

  @returns true if error
 */
static bool build_sj_exprs(THD *thd, mem_root_deque<Item *> *sj_outer_exprs,
                           mem_root_deque<Item *> *sj_inner_exprs,
                           Item_exists_subselect *subq_pred,
                           Query_block *subq_query_block) {
  Item_in_subselect *in_subq_pred = down_cast<Item_in_subselect *>(subq_pred);

  assert(in_subq_pred->left_expr->fixed);

  /*
    We have a special case for IN predicates with a scalar subquery or a
    row subquery in the predicand (left operand), such as this:
     (SELECT 1,2 FROM t1) IN (SELECT x,y FROM t2)
    We cannot make the join condition 1=x AND 2=y, since that might evaluate
    to true even if t1 is empty. Instead make the join condition
    (SELECT 1,2 FROM t1) = (x,y) in this case.
  */
  Item_subselect *left_subquery =
      (in_subq_pred->left_expr->type() == Item::SUBQUERY_ITEM)
          ? static_cast<Item_subselect *>(in_subq_pred->left_expr)
          : nullptr;

  if (left_subquery &&
      (left_subquery->subquery_type() == Item_subselect::SCALAR_SUBQUERY)) {
    mem_root_deque<Item *> ref_list(thd->mem_root);
    Item *header = subq_query_block->base_ref_items[0];
    for (uint i = 1; i < in_subq_pred->left_expr->cols(); i++) {
      ref_list.push_back(subq_query_block->base_ref_items[i]);
    }

    Item_row *right_expr = new Item_row(header, ref_list);
    if (!right_expr) return true; /* purecov: inspected */

    sj_outer_exprs->push_back(in_subq_pred->left_expr);
    sj_inner_exprs->push_back(right_expr);
  } else {
    for (uint i = 0; i < in_subq_pred->left_expr->cols(); i++) {
      Item *const li = in_subq_pred->left_expr->element_index(i);
      sj_outer_exprs->push_back(li);
      sj_inner_exprs->push_back(subq_query_block->base_ref_items[i]);
    }
  }
  return false;
}

/**
  Convert a subquery predicate of this query block into a Table_ref
  semi-join nest.

  @param thd         Thread handle
  @param subq_pred   Subquery predicate to be converted.
                     This is either an IN, =ANY or EXISTS predicate, possibly
                     negated.

  @returns false if success, true if error

  The following transformations are performed:

  1. IN/=ANY predicates on the form:

  @code
  SELECT ...
  FROM ot1 ... otN
  WHERE (oe1, ... oeM) IN (SELECT ie1, ..., ieM
                           FROM it1 ... itK
                          [WHERE inner-cond])
   [AND outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  are transformed into:

  @code
  SELECT ...
  FROM (ot1 ... otN) SJ (it1 ... itK)
                     ON (oe1, ... oeM) = (ie1, ..., ieM)
                        [AND inner-cond]
  [WHERE outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  Notice that the inner-cond may contain correlated and non-correlated
  expressions. Further transformations will analyze and break up such
  expressions.

  2. EXISTS predicates on the form:

  @code
  SELECT ...
  FROM ot1 ... otN
  WHERE EXISTS (SELECT expressions
                FROM it1 ... itK
                [WHERE inner-cond])
   [AND outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  are transformed into:

  @code
  SELECT ...
  FROM (ot1 ... otN) SJ (it1 ... itK)
                     [ON inner-cond]
  [WHERE outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  3. Negated EXISTS predicates on the form:

  @code
  SELECT ...
  FROM ot1 ... otN
  WHERE NOT EXISTS (SELECT expressions
                FROM it1 ... itK
                [WHERE inner-cond])
   [AND outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  are transformed into:

  @code
  SELECT ...
  FROM (ot1 ... otN) AJ (it1 ... itK)
                     [ON inner-cond]
  [WHERE outer-cond AND is-null-cond(it1)]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  where AJ means "antijoin" and is like a LEFT JOIN; and is-null-cond is
  false if the row of it1 is "found" and "not_null_compl" (i.e. matches
  inner-cond).

  4. Negated IN predicates on the form:

  @code
  SELECT ...
  FROM ot1 ... otN
  WHERE (oe1, ... oeM) NOT IN (SELECT ie1, ..., ieM
                               FROM it1 ... itK
                               [WHERE inner-cond])
   [AND outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  are transformed into:

  @code
  SELECT ...
  FROM (ot1 ... otN) AJ (it1 ... itK)
                     ON (oe1, ... oeM) = (ie1, ..., ieM)
                        [AND inner-cond]
  [WHERE outer-cond]
  [GROUP BY ...] [HAVING ...] [ORDER BY ...]
  @endcode

  5. The cases 1/2 (respectively 3/4) above also apply when the predicate is
  decorated with IS TRUE or IS NOT FALSE (respectively IS NOT TRUE or IS
  FALSE).
*/
bool Query_block::convert_subquery_to_semijoin(
    THD *thd, Item_exists_subselect *subq_pred) {
  Table_ref *emb_tbl_nest = nullptr;
  mem_root_deque<Table_ref *> *emb_join_list = &m_table_nest;
  DBUG_TRACE;

  assert(subq_pred->subquery_type() == Item_subselect::IN_SUBQUERY ||
         subq_pred->subquery_type() == Item_subselect::EXISTS_SUBQUERY);

  Opt_trace_context *trace = &thd->opt_trace;
  Opt_trace_object trace_object(trace, "transformation_to_semi_join");
  if (unlikely(trace->is_started())) {
    trace_object.add("subquery_predicate", subq_pred);
  }

  bool outer_join = false;  // True if predicate is inner to an outer join

  // Save the set of tables in the outer query block:
  table_map outer_tables_map = all_tables_map();
  const bool do_aj = subq_pred->can_do_aj;

  /*
    Find out where to insert the semi-join nest and the generated condition.

    For t1 LEFT JOIN t2, embedding_join_nest will be t2.
    Note that t2 may be a simple table or may itself be a join nest
    (e.g. in the case t1 LEFT JOIN (t2 JOIN t3))
  */
  if (subq_pred->embedding_join_nest != nullptr) {
    // Is this on inner side of an outer join?
    outer_join = subq_pred->embedding_join_nest->is_inner_table_of_outer_join();

    if (subq_pred->embedding_join_nest->nested_join) {
      /*
        We're dealing with

          ... [LEFT] JOIN  ( ... ) ON (subquery AND condition) ...

        The sj-nest will be inserted into the brackets nest.
      */
      emb_tbl_nest = subq_pred->embedding_join_nest;
      emb_join_list = &emb_tbl_nest->nested_join->m_tables;
    } else if (!subq_pred->embedding_join_nest->outer_join) {
      /*
        We're dealing with

          ... INNER JOIN tblX ON (subquery AND condition) ...

        The sj-nest will be tblX's "sibling", i.e. another child of its
        parent. This is ok because tblX is joined as an inner join.
      */
      emb_tbl_nest = subq_pred->embedding_join_nest->embedding;
      if (emb_tbl_nest) emb_join_list = &emb_tbl_nest->nested_join->m_tables;
    } else {
      Table_ref *outer_tbl = subq_pred->embedding_join_nest;
      /*
        We're dealing with

          ... LEFT JOIN tbl ON (on_expr AND subq_pred) ...

        tbl will be replaced with:

          ( tbl SJ (subq_tables) )
          |                      |
          |<----- wrap_nest ---->|

        giving:
          ... LEFT JOIN ( tbl SJ (subq_tables) ) ON (on_expr AND subq_pred) ...

        Q:  other subqueries may be pointing to this element. What to do?
        A1: simple solution: copy *subq_pred->embedding_join_nest= *parent_nest.
            But we'll need to fix other pointers.
        A2: Another way: have Table_ref::next_ptr so the following
            subqueries know the table has been nested.
        A3: changes in the Table_ref::outer_join will make everything work
            automatically.
      */
      Table_ref *const wrap_nest = Table_ref::new_nested_join(
          thd->mem_root, "(sj-wrap)", outer_tbl->embedding,
          outer_tbl->join_list, this);
      if (wrap_nest == nullptr) return true;

      wrap_nest->nested_join->m_tables.push_back(outer_tbl);

      outer_tbl->embedding = wrap_nest;
      outer_tbl->join_list = &wrap_nest->nested_join->m_tables;

      /*
        wrap_nest will take place of outer_tbl, so move the outer join flag
        and join condition.
      */
      wrap_nest->outer_join = outer_tbl->outer_join;
      outer_tbl->outer_join = false;

      wrap_nest->set_join_cond(outer_tbl->join_cond());
      outer_tbl->set_join_cond(nullptr);

      for (auto li = wrap_nest->join_list->begin();
           li != wrap_nest->join_list->end(); ++li) {
        Table_ref *tbl = *li;
        if (tbl == outer_tbl) {
          *li = wrap_nest;
          break;
        }
      }

      /*
        outer_tbl is replaced by wrap_nest. Any subquery which was attached to
        outer_tbl must be attached to embedding_join_nest instead.
      */
      for (Item_exists_subselect *subquery : (*sj_candidates)) {
        if (subquery->embedding_join_nest == outer_tbl)
          subquery->embedding_join_nest = wrap_nest;
      }

      /*
        Ok now wrap_nest 'contains' outer_tbl and we're ready to add the
        semi-join nest into it
      */
      emb_join_list = &wrap_nest->nested_join->m_tables;
      emb_tbl_nest = wrap_nest;
    }
  }
  // else subquery is in WHERE.

  if (do_aj) {
    /*
      A negated IN/EXISTS like:
      NOT EXISTS(... FROM subq_tables WHERE subq_cond)
      The above code has ensured that we have one of these 3 situations:

      (a) FROM ... WHERE (subquery AND condition)
      (emb_tbl_nest == nullptr, emb_join_list == FROM clause)

      which has to be changed to
          FROM (...)            LEFT JOIN (subq_tables) ON subq_cond
               ^ aj-left-nest             ^aj-nest
          WHERE x IS NULL AND condition

      or:
      (b) ... [LEFT] JOIN ( ...          ) ON (subquery AND condition) ...
                          ^ emb_tbl_nest, emb_join_list

      which has to be changed to
          ... [LEFT] JOIN ( (...)          LEFT JOIN (subq_tables) ON subq_cond)
                            ^aj-left-nest            ^aj-nest
                          ^ emb_tbl_nest, emb_join_list
              ON x IS NULL AND condition ...

      or:
      (c) ... INNER JOIN tblX ON (subquery AND condition) ...
          ^ emb_tbl_nest, emb_join_list
            (if no '()' above this INNER JOIN up to the root, emb_tbl_nest ==
             nullptr and emb_join_list == FROM clause)

      which has to be changed to
       ( ... INNER JOIN tblX ON condition) LEFT JOIN (subq_tables) ON subq_cond
       ^aj-left-nest                                 ^aj-nest

      so:
      - move all tables of emb_join_list into a new aj-left-nest
      - emb_join_list is now empty
      - put subq_tables in a new aj-nest
      - add the subq's subq_cond to aj-nest's ON
      - add a LEFT JOIN operator between the aj-left-nest and aj-nest, with
      ON condition subq_cond.
      - insert aj-nest and aj-left-nest into emb_join_list
      - for some reason, a LEFT JOIN must always be wrapped into a nest (call
      nest_last_join() then)
      - do not yet add 'x IS NULL to WHERE' (add it in optimization phase when
      we have the QEP_TABs so we can set up the 'found'/'not_null_compl'
      pointers in trig conds).
    */
    Table_ref *const wrap_nest = Table_ref::new_nested_join(
        thd->mem_root, "(aj-left-nest)", emb_tbl_nest, emb_join_list, this);
    if (wrap_nest == nullptr) return true;

    // Go through tables of emb_join_list, insert them in wrap_nest
    for (Table_ref *outer_tbl : *emb_join_list) {
      wrap_nest->nested_join->m_tables.push_back(outer_tbl);
      outer_tbl->embedding = wrap_nest;
      outer_tbl->join_list = &wrap_nest->nested_join->m_tables;
    }
    // FROM clause is now only the new left nest
    emb_join_list->clear();
    emb_join_list->push_back(wrap_nest);
    outer_join = true;
  }

  if (unlikely(trace->is_started()))
    trace_object.add_alnum("embedded in", emb_tbl_nest ? "JOIN" : "WHERE");

  Table_ref *const sj_nest = Table_ref::new_nested_join(
      thd->mem_root, do_aj ? "(aj-nest)" : "(sj-nest)", emb_tbl_nest,
      emb_join_list, this);
  if (sj_nest == nullptr) return true; /* purecov: inspected */

  NESTED_JOIN *const nested_join = sj_nest->nested_join;

  /* Nests do not participate in those 'chains', so: */
  /* sj_nest->next_leaf= sj_nest->next_local= sj_nest->next_global == NULL*/
  /*
    Using push_front, as sj_nest may be right arg of LEFT JOIN if
    antijoin, and right args of LEFT JOIN go before left arg.
  */
  emb_join_list->push_front(sj_nest);

  /*
    Natural joins inside a semi-join nest were already processed when the
    subquery went through initial preparation.
  */
  sj_nest->nested_join->natural_join_processed = true;
  /*
    nested_join->used_tables and nested_join->not_null_tables are
    initialized in simplify_joins().
  */
  Query_block *const subq_query_block =
      subq_pred->query_expr()->first_query_block();

  nested_join->query_block_id = subq_query_block->select_number;

  // Merge tables from underlying query block into this join nest
  if (sj_nest->merge_underlying_tables(subq_query_block))
    return true; /* purecov: inspected */

  /*
    Add tables from subquery at end of leaf table chain.
    (This also means that table map for parent query block tables are unchanged)
  */
  Table_ref *tl;
  for (tl = leaf_tables; tl->next_leaf; tl = tl->next_leaf) {
  }
  tl->next_leaf = subq_query_block->leaf_tables;

  // Add tables from subquery at end of next_local chain.
  m_table_list.push_back(&subq_query_block->m_table_list);

  // Note that subquery's tables are already in the next_global chain

  // Remove the original subquery predicate from the WHERE/ON
  // The subqueries were replaced with TRUE value earlier
  // @todo also reset the 'with_subselect' there.

  // Walk through child's tables and adjust table map
  uint table_no = leaf_table_count;
  for (tl = subq_query_block->leaf_tables; tl; tl = tl->next_leaf, table_no++) {
    tl->dep_tables <<= leaf_table_count;
    tl->set_tableno(table_no);
  }

  /*
    If we leave this function in an error path before subq_query_block is
    unlinked, make sure tables are not duplicated, or cleanup code could be
    confused:
  */
  subq_query_block->m_table_list.clear();
  subq_query_block->leaf_tables = nullptr;

  // Adjust table and expression counts in parent query block:
  derived_table_count += subq_query_block->derived_table_count;
  materialized_derived_table_count +=
      subq_query_block->materialized_derived_table_count;
  table_func_count += subq_query_block->table_func_count;
  has_sj_nests |= subq_query_block->has_sj_nests;
  has_aj_nests |= subq_query_block->has_aj_nests;
  partitioned_table_count += subq_query_block->partitioned_table_count;
  leaf_table_count += subq_query_block->leaf_table_count;
  cond_count += subq_query_block->cond_count;
  between_count += subq_query_block->between_count;

  if (subq_query_block->active_options() & OPTION_SCHEMA_TABLE)
    add_base_options(OPTION_SCHEMA_TABLE);

  if (outer_join) propagate_nullability(&sj_nest->nested_join->m_tables, true);

  nested_join->sj_outer_exprs.clear();
  nested_join->sj_inner_exprs.clear();

  if (subq_pred->subquery_type() == Item_subselect::IN_SUBQUERY) {
    build_sj_exprs(thd, &nested_join->sj_outer_exprs,
                   &nested_join->sj_inner_exprs, subq_pred, subq_query_block);
  } else {  // this is EXISTS
    // Expressions from the SELECT list will not be used; unlike in the case of
    // IN, they are not part of sj_inner_exprs.
    // @todo in WL#6570, move this to resolve_subquery().
    for (Item *item : subq_query_block->visible_fields()) {
      Item::Cleanup_after_removal_context ctx(this);
      item->walk(&Item::clean_up_after_removal, walk_options,
                 pointer_cast<uchar *>(&ctx));
    }
  }

  {
    /*
      The WHERE clause and the join conditions may contain equalities that may
      be leveraged by semi-join strategies (e.g to set up key lookups in
      semi-join materialization), decorrelate them (ie. add respective fields
      and expressions to sj_inner_exprs and sj_outer_exprs).
    */
    Semijoin_decorrelation sj_decor(&sj_nest->nested_join->sj_outer_exprs,
                                    &sj_nest->nested_join->sj_inner_exprs,
                                    // decorrelate only equalities
                                    /*op_types=*/nullptr);

    if (subq_query_block->where_cond() &&
        subq_query_block->decorrelate_condition(sj_decor, nullptr))
      return true;

    if (walk_join_list(
            subq_query_block->m_table_nest, [&](Table_ref *tr) -> bool {
              return !tr->is_inner_table_of_outer_join() && tr->join_cond() &&
                     subq_query_block->decorrelate_condition(sj_decor, tr);
            }))
      return true;
  }

  // Unlink the subquery's query expression:
  subq_query_block->master_query_expression()->exclude_level();

  // Merge subquery's name resolution contexts into parent's
  merge_contexts(subq_query_block);

  repoint_contexts_of_join_nests(subq_query_block->m_table_nest);

  // Update table map for semi-join nest's WHERE condition and join conditions
  fix_tables_after_pullout(this, subq_query_block, sj_nest, 0, 0);

  Item *sj_cond = subq_query_block->where_cond();
  if (sj_cond != nullptr) sj_cond->fix_after_pullout(this, subq_query_block);

  // Assign the set of non-trivially tables after decorrelation
  nested_join->sj_corr_tables =
      (sj_cond != nullptr ? sj_cond->used_tables() & outer_tables_map : 0);

  walk_join_list(subq_query_block->m_table_nest, [&](Table_ref *tr) -> bool {
    if (tr->join_cond())
      nested_join->sj_corr_tables |=
          tr->join_cond()->used_tables() & outer_tables_map;
    if (tr->is_derived() && tr->uses_materialization())
      nested_join->sj_corr_tables |=
          tr->derived_query_expression()->m_lateral_deps;
    return false;
  });

  // Build semijoin condition using the inner/outer expression list
  bool simple_cond;
  if (build_sj_cond(thd, nested_join, subq_query_block, outer_tables_map,
                    &sj_cond, &simple_cond))
    return true;

  // Processing requires a non-empty semi-join condition:
  assert(sj_cond != nullptr);

  // Fix the created equality and AND
  if (!sj_cond->fixed) {
    Opt_trace_array sj_on_trace(&thd->opt_trace,
                                "evaluating_constant_semijoin_conditions");
    sj_cond->apply_is_true();
    if (sj_cond->fix_fields(thd, &sj_cond))
      return true; /* purecov: inspected */
  }

  sj_nest->set_sj_or_aj_nest();
  assert(sj_nest->join_cond() == nullptr);

  if (do_aj) {
    sj_nest->outer_join = true;
    sj_nest->set_join_cond(sj_cond);
    this->outer_join |= sj_nest->nested_join->used_tables;
    if (emb_tbl_nest == nullptr)
      nest_last_join(thd);  // as is done for a true LEFT JOIN
  }

  if (unlikely(trace->is_started())) {
    trace_object.add("semi-join condition", sj_cond);
    Opt_trace_array trace_dep(trace, "decorrelated_predicates");
    auto ii = nested_join->sj_inner_exprs.begin();
    auto oi = nested_join->sj_outer_exprs.begin();
    while (ii != nested_join->sj_inner_exprs.end() &&
           oi != nested_join->sj_outer_exprs.end()) {
      Item *inner = *ii++, *outer = *oi++;
      Opt_trace_object trace_predicate(trace);
      trace_predicate.add("outer", outer);
      trace_predicate.add("inner", inner);
    }
  }

  /*
    sj_depends_on contains the set of outer tables referred in the
    subquery's WHERE clause as well as tables referred in the IN predicate's
    left-hand side, and lateral dependencies from materialized derived tables
    contained in the original subquery.
  */
  nested_join->sj_depends_on =
      nested_join->sj_corr_tables | (sj_cond->used_tables() & outer_tables_map);

  assert((nested_join->sj_corr_tables & OUTER_REF_TABLE_BIT) == 0);
  assert((nested_join->sj_depends_on & OUTER_REF_TABLE_BIT) == 0);

  // TODO fix QT_
  DBUG_EXECUTE("where", print_where(thd, sj_cond, "SJ-COND", QT_ORDINARY););

  Item *cond = nullptr;
  if (do_aj) {
    // Condition remains attached to inner table, as for LEFT JOIN
    cond = sj_cond;
  } else if (emb_tbl_nest != nullptr) {
    // Inject semi-join condition into parent's join condition
    emb_tbl_nest->set_join_cond(and_items(emb_tbl_nest->join_cond(), sj_cond));
    if (emb_tbl_nest->join_cond() == nullptr) return true;
    emb_tbl_nest->join_cond()->apply_is_true();
    if (!emb_tbl_nest->join_cond()->fixed &&
        emb_tbl_nest->join_cond()->fix_fields(thd,
                                              emb_tbl_nest->join_cond_ref()))
      return true;
    cond = emb_tbl_nest->join_cond();
  } else {
    // Inject semi-join condition into parent's WHERE condition
    m_where_cond = and_items(m_where_cond, sj_cond);
    if (m_where_cond == nullptr) return true;
    m_where_cond->apply_is_true();
    if (m_where_cond->fix_fields(thd, &m_where_cond)) return true;
    cond = m_where_cond;
  }
  /*
    If the current semi-join or anti-join condition is always TRUE or
    always FALSE:
    (a) there is no need to set up lookups (for loosescan or materialization).
    (b) if some predicates were eliminated as part of const value optimization,
        their expressions are still in the inner/outer expression list
        and must be removed.
    (If a "simple condition" was added in build_sj_cond(), this is not necessary
     since the expressions were constant values and are safe to keep.)
  */
  if (cond != nullptr && cond->const_item() && !simple_cond) {
    clear_sj_expressions(nested_join);
  }

  if (subq_query_block->ftfunc_list->elements &&
      add_ftfunc_list(subq_query_block->ftfunc_list))
    return true; /* purecov: inspected */

  if (do_aj)
    has_aj_nests = true;
  else
    has_sj_nests = true;  // This query block has semi-join nests

  return false;
}

/**
  Merge a derived table or view into a query block.
  If some constraint prevents the derived table from being merged then do
  nothing, which means the table will be prepared for materialization later.

  After this call, check is_merged() to see if the table was really merged.

  @param thd           Thread handler
  @param derived_table Derived table which is to be merged.

  @return false if successful, true if error
*/

bool Query_block::merge_derived(THD *thd, Table_ref *derived_table) {
  DBUG_TRACE;

  if (!derived_table->is_view_or_derived() || derived_table->is_merged())
    return false;

  Query_expression *const derived_query_expression =
      derived_table->derived_query_expression();

  // A derived table must be prepared before we can merge it
  assert(derived_query_expression->is_prepared());

  LEX *const lex = parent_lex;

  // Check whether the outer query allows merged views
  if ((master_query_expression() == lex->unit && !lex->can_use_merged()) ||
      lex->can_not_use_merged())
    return false;

  /*
    @todo: The implementation of LEX::can_use_merged() currently avoids
           merging of views that are contained in other views if
           can_use_merged() returns false.
  */
  /*
    Check whether derived table is mergeable, and directives allow merging;
    priority order is:
    - ALGORITHM says MERGE or TEMPTABLE
    - hint specifies MERGE or NO_MERGE (=materialization)
    - optimizer_switch's derived_merge is ON and heuristic suggests merge
  */
  if (derived_table->algorithm == VIEW_ALGORITHM_TEMPTABLE ||
      !derived_query_expression->is_mergeable())
    return false;

  if (derived_table->algorithm == VIEW_ALGORITHM_UNDEFINED) {
    const bool merge_heuristic =
        (derived_table->is_view() || allow_merge_derived) &&
        derived_query_expression->merge_heuristic(thd->lex);
    if (!hint_table_state(thd, derived_table, DERIVED_MERGE_HINT_ENUM,
                          merge_heuristic ? OPTIMIZER_SWITCH_DERIVED_MERGE : 0))
      return false;
  }

  Query_block *const derived_query_block =
      derived_query_expression->first_query_block();
  /*
    If STRAIGHT_JOIN is specified, it is not valid to merge in a query block
    that contains semi-join nests
  */
  if ((active_options() & SELECT_STRAIGHT_JOIN) &&
      (derived_query_block->has_sj_nests || derived_query_block->has_aj_nests))
    return false;

  // Check that we have room for the merged tables in the table map:
  if (leaf_table_count + derived_query_block->leaf_table_count - 1 > MAX_TABLES)
    return false;

  derived_table->set_merged();

  DBUG_PRINT("info", ("algorithm: MERGE"));

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_derived(trace,
                                 derived_table->is_view() ? "view" : "derived");
  trace_derived.add_utf8_table(derived_table)
      .add("select#", derived_query_block->select_number)
      .add("merged", true);

  Prepared_stmt_arena_holder ps_arena_holder(thd);

  // Save offset for table number adjustment
  uint table_adjust = derived_table->tableno();

  // Set up permanent list of underlying tables of a merged view
  derived_table->merge_underlying_list = derived_query_block->get_table_list();

  /**
    A view is updatable if any underlying table is updatable.
    A view is insertable-into if all underlying tables are insertable.
    A view is not updatable nor insertable if it contains an outer join
    @see mysql_register_view()
  */
  if (derived_table->is_view()) {
    bool updatable = false;
    bool insertable = true;
    bool outer_joined = false;
    for (Table_ref *tr = derived_table->merge_underlying_list; tr;
         tr = tr->next_local) {
      updatable |= tr->is_updatable();
      insertable &= tr->is_insertable();
      outer_joined |= tr->is_inner_table_of_outer_join();
    }
    updatable &= !outer_joined;
    insertable &= !outer_joined;
    if (updatable) derived_table->set_updatable();
    if (insertable) derived_table->set_insertable();
  }

  // Add a nested join object to the derived table object
  if (!(derived_table->nested_join = new (thd->mem_root) NESTED_JOIN))
    return true;

  // Merge tables from underlying query block into this join nest
  if (derived_table->merge_underlying_tables(derived_query_block))
    return true; /* purecov: inspected */

  // Replace derived table in leaf table list with underlying tables:
  for (Table_ref **tl = &leaf_tables; *tl; tl = &(*tl)->next_leaf) {
    if (*tl == derived_table) {
      for (Table_ref *leaf = derived_query_block->leaf_tables; leaf;
           leaf = leaf->next_leaf) {
        leaf->dep_tables <<= table_adjust;
        if (leaf->next_leaf == nullptr) {
          leaf->next_leaf = (*tl)->next_leaf;
          break;
        }
      }
      *tl = derived_query_block->leaf_tables;
      break;
    }
  }

  leaf_table_count += (derived_query_block->leaf_table_count - 1);
  derived_table_count += derived_query_block->derived_table_count;
  table_func_count += derived_query_block->table_func_count;
  materialized_derived_table_count +=
      derived_query_block->materialized_derived_table_count;
  has_sj_nests |= derived_query_block->has_sj_nests;
  has_aj_nests |= derived_query_block->has_aj_nests;
  partitioned_table_count += derived_query_block->partitioned_table_count;
  cond_count += derived_query_block->cond_count;
  between_count += derived_query_block->between_count;

  // Remove tables from old query block:
  derived_query_block->leaf_tables = nullptr;
  derived_query_block->leaf_table_count = 0;
  derived_query_block->m_table_list.clear();

  // Propagate schema table indication:
  // @todo: Add to BASE options instead
  if (derived_query_block->active_options() & OPTION_SCHEMA_TABLE)
    add_base_options(OPTION_SCHEMA_TABLE);

  // Propagate nullability for derived tables within outer joins:
  if (derived_table->is_inner_table_of_outer_join())
    propagate_nullability(&derived_table->nested_join->m_tables, true);

  select_n_having_items += derived_query_block->select_n_having_items;

  // Merge the WHERE clause into the outer query block
  if (derived_table->merge_where(thd)) return true; /* purecov: inspected */

  if (derived_table->create_field_translation(thd))
    return true; /* purecov: inspected */

  // Exclude the derived table query expression from query graph.
  derived_query_expression->exclude_level();

  // Don't try to access it:
  derived_table->set_derived_query_expression((Query_expression *)1);

  // Merge subquery's name resolution contexts into parent's
  merge_contexts(derived_query_block);

  repoint_contexts_of_join_nests(derived_query_block->m_table_nest);

  // Leaf tables have been shuffled, so update table numbers for them
  remap_tables(thd);

  // Update table info of referenced expressions after query block is merged
  fix_tables_after_pullout(this, derived_query_block, derived_table,
                           table_adjust,
                           derived_query_expression->m_lateral_deps);

  if (derived_query_block->is_ordered()) {
    /*
      An ORDER BY clause is moved to an outer query block
      - if the outer query block allows ordering, and
      - that refers to this view/derived table only, and
      - is not part of a set operation (UNION, EXCEPT, INTERSECT), and
      - may have a WHERE clause but is not grouped or aggregated and is not
        itself ordered.
     Otherwise the ORDER BY clause is ignored.

     Only SELECT statements and single-table UPDATE and DELETE statements
     allow ordering.

     Up to version 5.6 included, ORDER BY was unconditionally merged.
     Currently we only merge in the simple case above, which ensures
     backward compatibility for most reasonable use cases.

     Note that table numbers in order_list do not need updating, since
     the outer query contains only one table reference.
    */
    // LIMIT currently blocks derived table merge
    assert(!derived_query_block->has_limit());

    if ((lex->sql_command == SQLCOM_SELECT ||
         lex->sql_command == SQLCOM_UPDATE ||
         lex->sql_command == SQLCOM_DELETE) &&
        !(master_query_expression()->is_set_operation() || is_grouped() ||
          is_distinct() || is_ordered() ||
          get_table_list()->next_local != nullptr)) {
      order_list.push_back(&derived_query_block->order_list);
      for (ORDER *o = derived_query_block->order_list.first; o != nullptr;
           o = o->next) {
        /*
          ORDER BY clause may contain expressions with outer references that
          must be adjusted:
        */
        o->item[0]->fix_after_pullout(this, derived_query_block);
        /*
          If at outer-most level (not within another derived table), ensure
          the ordering columns are marked in read_set, since columns selected
          from derived tables are not marked in initial resolving.
        */
        if (!thd->derived_tables_processing) {
          Mark_field mf(thd->mark_used_columns);
          o->item[0]->walk(&Item::mark_field_in_map, enum_walk::POSTFIX,
                           pointer_cast<uchar *>(&mf));
        }
      }
    } else {
      if (derived_query_block->empty_order_list(this)) return true;
      trace_derived.add_alnum("transformations_to_derived_table",
                              "removed_ordering");
    }
  }

  // Add any full-text functions from derived table into outer query
  if (derived_query_block->ftfunc_list->elements &&
      add_ftfunc_list(derived_query_block->ftfunc_list))
    return true; /* purecov: inspected */

  /*
    The "laterality" of this nest is not interesting anymore; it was
    transferred to underlying tables.
  */
  derived_query_expression->m_lateral_deps = 0;

  return false;
}

/**
   Destructively replaces a sub-condition inside a condition tree. The
   parse tree is also altered.

   @param thd  thread handler

   @param tree Must be the handle to the top level condition. This is needed
   when the top-level condition changes.

   @param old_cond The condition to be replaced.

   @param new_cond The condition to be substituted.

   @param do_fix_fields If true, Item::fix_fields(THD*, Item**) is called for
   the new condition.

   @param[out] found_ptr Pointer to boolean; used only in recursive sub-calls;
   top call must not specify this argument. Function deposits there if it
   found the searched Item or not.

   @return error status

   @retval true If there was an error.
   @retval false If successful.
*/
static bool replace_subcondition(THD *thd, Item **tree, Item *old_cond,
                                 Item *new_cond, bool do_fix_fields,
                                 bool *found_ptr = nullptr) {
  if (*tree == old_cond) {
    *tree = new_cond;
    if (do_fix_fields && new_cond->fix_fields(thd, tree)) return true;
    if (found_ptr != nullptr) *found_ptr = true;  // inform upper call
    return false;
  }
  if ((*tree)->type() == Item::COND_ITEM) {
    Item_cond *cond = down_cast<Item_cond *>(*tree);
    List_iterator<Item> li(*cond->argument_list());
    Item *item;
    bool found_local = false;
    while ((item = li++)) {
      if (replace_subcondition(thd, li.ref(), old_cond, new_cond, do_fix_fields,
                               &found_local))
        return true;
      if (found_local) {
        if (found_ptr != nullptr) *found_ptr = true;  // inform upper call
        return false;
      }
    }
  } else if ((*tree)->type() == Item::FUNC_ITEM) {
    Item_func *func = down_cast<Item_func *>(*tree);
    bool found_local = false;
    for (uint i = 0; i < func->arg_count; i++) {
      if (replace_subcondition(thd, func->arguments() + i, old_cond, new_cond,
                               do_fix_fields, &found_local))
        return true;
      if (found_local) {
        if (found_ptr != nullptr) *found_ptr = true;  // inform upper call
        return false;
      }
    }
  }
  // item not found
  // if it is the top call: error, else: no error.
  assert(found_ptr != nullptr);
  return (found_ptr == nullptr);
}

/**
  Convert semi-join subquery predicates into semi-join join nests.

  Convert candidate subquery predicates into semi-join join nests. This
  transformation is performed once in query lifetime and is irreversible.

  Conversion of one subquery predicate
  ------------------------------------

  We start with a query block that has a semi-join subquery predicate:

  @code
  SELECT ...
  FROM ot, ...
  WHERE oe IN (SELECT ie FROM it1 ... itN WHERE subq_where) AND outer_where
  @endcode

  and convert the predicate and subquery into a semi-join nest:

  @code
  SELECT ...
  FROM ot SEMI JOIN (it1 ... itN), ...
  WHERE outer_where AND subq_where AND oe=ie
  @endcode

  that is, in order to do the conversion, we need to

   * Create the "SEMI JOIN (it1 .. itN)" part and add it into the parent
     query block's FROM structure.
   * Add "AND subq_where AND oe=ie" into parent query block's WHERE (or ON if
     the subquery predicate was in an ON condition)
   * Remove the subquery predicate from the parent query block's WHERE

  Considerations when converting many predicates
  ----------------------------------------------

  A join may have at most MAX_TABLES tables. This may prevent us from
  flattening all subqueries when the total number of tables in parent and
  child selects exceeds MAX_TABLES. In addition, one slot is reserved per
  semi-join nest, in case the subquery needs to be materialized in a
  temporary table.
  We deal with this problem by flattening children's subqueries first and
  then using a heuristic rule to determine each subquery predicate's
  priority, which is calculated in this order:

  1. Prefer dependent subqueries over non-dependent ones
  2. Prefer subqueries with many tables over those with fewer tables
  3. Prefer early subqueries over later ones (to make sort deterministic)

  @returns false if success, true if error
*/
bool Query_block::flatten_subqueries(THD *thd) {
  DBUG_TRACE;

  assert(has_sj_candidates());

  Item_exists_subselect **subq, **subq_begin = sj_candidates->begin(),
                                **subq_end = sj_candidates->end();

  Opt_trace_context *const trace = &thd->opt_trace;

  /*
    Semijoin flattening is bottom-up. Indeed, we have this execution flow,
    for SELECT#1 WHERE X IN (SELECT #2 WHERE Y IN (SELECT#3)) :

    Query_block::prepare() (select#1)
       -> fix_fields() on IN condition
           -> Query_block::prepare() on subquery (select#2)
               -> fix_fields() on IN condition
                    -> Query_block::prepare() on subquery (select#3)
                    <- Query_block::prepare()
               <- fix_fields()
               -> flatten_subqueries: merge #3 in #2
               <- flatten_subqueries
           <- Query_block::prepare()
       <- fix_fields()
       -> flatten_subqueries: merge #2 in #1

    Note that flattening of #(N) is done by its parent JOIN#(N-1), because
    there are cases where flattening is not possible and only the parent can
    know.
   */
  uint subq_no;
  for (subq = subq_begin, subq_no = 0; subq < subq_end; subq++, subq_no++) {
    Item_exists_subselect *item = *subq;
    /*
      Some subqueries may have been deleted, remove them fully before sorting
      sj_candidates and subsequent processing:
    */
    if (item->strategy == Subquery_strategy::DELETED) {
      sj_candidates->erase_value(item);
      subq--;  // So that the next iteration will handle the next subquery.
      subq_end = sj_candidates->end();  // array's end moved.

      continue;
    }
    // Transformation of IN and EXISTS subqueries is supported
    assert(item->subquery_type() == Item_subselect::IN_SUBQUERY ||
           item->subquery_type() == Item_subselect::EXISTS_SUBQUERY);

    Query_block *child_query_block = item->query_expr()->first_query_block();

    // Check that we proceeded bottom-up
    assert(child_query_block->sj_candidates == nullptr);

    bool dependent = item->query_expr()->uncacheable & UNCACHEABLE_DEPENDENT;
    item->sj_convert_priority =
        (((dependent * MAX_TABLES_FOR_SIZE) +  // dependent subqueries first
          child_query_block->leaf_table_count) *
         65536) +           // then with many tables
        (65536 - subq_no);  // then based on position

    /*
      We may actually allocate more than 64k subqueries in a query block,
      but this is so unlikely that we ignore the impact it may have on sorting.
     */
  }

  /*
    Pick which subqueries to convert:
      sort the subquery array
      - prefer correlated subqueries over uncorrelated;
      - prefer subqueries that have greater number of outer tables;
  */
  std::sort(subq_begin, subq_begin + sj_candidates->size(),
            [](Item_exists_subselect *el1, Item_exists_subselect *el2) {
              return el1->sj_convert_priority > el2->sj_convert_priority;
            });

  // A permanent transformation is going to start, so:
  Prepared_stmt_arena_holder ps_arena_holder(thd);

  // Transform certain subquery predicates to derived tables
  for (subq = subq_begin; subq < subq_end; subq++) {
    Item_exists_subselect *item = *subq;
    if (item->strategy != Subquery_strategy::CANDIDATE_FOR_DERIVED_TABLE)
      continue;
    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        item->query_expr()->first_query_block()->select_number,
                        "IN (SELECT)", "joined derived table");
    oto1.add("chosen", true);
    if (transform_table_subquery_to_join_with_derived(thd, item)) return true;
  }
  /*
    Replace all subqueries to be flattened with a truth predicate.
    Generally, this predicate is TRUE, but if the subquery has a WHERE condition
    that is always false, replace with a FALSE predicate. In the latter case,
    also avoid converting the subquery to a semi-join.
  */

  uint table_count = leaf_table_count;
  for (subq = subq_begin; subq < subq_end; subq++) {
    Item_exists_subselect *item = *subq;
    if (item->strategy != Subquery_strategy::CANDIDATE_FOR_SEMIJOIN) continue;

    // Add the tables in the subquery nest plus one in case of materialization:
    const uint tables_added =
        item->query_expr()->first_query_block()->leaf_table_count + 1;

    // (1) Not too many tables in total.
    // (2) This subquery contains no antijoin nest (anti/semijoin nest cannot
    // include antijoin nest for implementation reasons, see
    // advance_sj_state()).
    if (table_count + tables_added <= MAX_TABLES &&              // (1)
        !item->query_expr()->first_query_block()->has_aj_nests)  // (2)
      item->strategy = Subquery_strategy::SEMIJOIN;

    Item *subq_where = item->query_expr()->first_query_block()->where_cond();
    /*
      A predicate can be evaluated to ALWAYS TRUE or ALWAYS FALSE when it
      has only const items. If found to be ALWAYS FALSE, do not include
      the subquery in transformations.
    */
    bool cond_value = true;
    if (subq_where && subq_where->const_item() &&
        !subq_where->walk(&Item::is_non_const_over_literals, enum_walk::POSTFIX,
                          nullptr) &&
        simplify_const_condition(thd, &subq_where, false, &cond_value))
      return true;

    if (!cond_value) {
      // Unlink and delete this subquery's query expression
      Item::Cleanup_after_removal_context ctx(this);
      item->walk(&Item::clean_up_after_removal, walk_options,
                 pointer_cast<uchar *>(&ctx));
    }

    if (item->strategy == Subquery_strategy::SEMIJOIN)
      table_count += tables_added;

    if (item->strategy != Subquery_strategy::SEMIJOIN &&
        item->strategy != Subquery_strategy::DELETED) {
      item->strategy = Subquery_strategy::UNSPECIFIED;
      continue;
    }
    /*
      In WHERE/ON of parent query, replace IN (subq) with truth value:
      - When subquery is converted to anti/semi-join: truth value true.
      - When subquery WHERE cond is false: IN returns FALSE, so truth value
      false if a semijoin (IN) and truth value true if an antijoin (NOT IN).
    */
    Item *truth_item =
        (cond_value || item->can_do_aj)
            ? implicit_cast<Item *>(new (thd->mem_root) Item_func_true())
            : implicit_cast<Item *>(new (thd->mem_root) Item_func_false());
    if (truth_item == nullptr) return true;
    Item **tree = (item->embedding_join_nest == nullptr)
                      ? &m_where_cond
                      : item->embedding_join_nest->join_cond_ref();
    if (replace_subcondition(thd, tree, item, truth_item, false))
      return true; /* purecov: inspected */
  }

  /* Transform the selected subqueries into semi-join */

  for (subq = subq_begin; subq < subq_end; subq++) {
    Item_exists_subselect *item = *subq;
    if (item->strategy != Subquery_strategy::SEMIJOIN) continue;

    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        item->query_expr()->first_query_block()->select_number,
                        "IN (SELECT)",
                        item->can_do_aj ? "antijoin" : "semijoin");
    oto1.add("chosen", true);
    if (convert_subquery_to_semijoin(thd, *subq)) return true;
  }
  /*
    Finalize the subqueries that we did not convert,
    ie. perform IN->EXISTS rewrite.
  */
  for (subq = subq_begin; subq < subq_end; subq++) {
    Item_exists_subselect *item = *subq;
    if (item->strategy != Subquery_strategy::UNSPECIFIED) continue;

    Query_block *save_query_block = thd->lex->current_query_block();
    thd->lex->set_current_query_block(item->query_expr()->first_query_block());

    Item *transformed = nullptr;
    if (item->transformer(thd, &transformed)) {
      return true;
    }
    thd->lex->set_current_query_block(save_query_block);
    /*
      If the Item has been substituted with another Item (e.g an
      Item_in_optimizer), resolve it and add it to proper WHERE or ON clause.
      If no substitute exists (e.g for EXISTS predicate), no action is required.
    */
    if (transformed == nullptr) continue;
    const bool do_fix_fields = !transformed->fixed;
    const bool subquery_in_join_clause = item->embedding_join_nest != nullptr;

    Item **tree = subquery_in_join_clause
                      ? (item->embedding_join_nest->join_cond_ref())
                      : &m_where_cond;
    if (replace_subcondition(thd, tree, *subq, transformed, do_fix_fields))
      return true;
  }

  sj_candidates->clear();
  return false;
}

/**
  Propagate nullability into inner tables of outer join operation

  @param tables  List of tables and join nests, start at m_table_nest
  @param nullable  true: Set all underlying tables as nullable
*/
void propagate_nullability(mem_root_deque<Table_ref *> *tables, bool nullable) {
  for (Table_ref *tr : *tables) {
    if (tr->table && !tr->table->is_nullable() && (nullable || tr->outer_join))
      tr->table->set_nullable();
    if (tr->nested_join == nullptr) continue;
    propagate_nullability(&tr->nested_join->m_tables,
                          nullable || tr->outer_join);
  }
}

/**
  Propagate exclusion from unique table check into all subqueries belonging
  to this query block.

  This function can be applied to all subqueries of a materialized derived
  table or view.
*/

void Query_block::propagate_unique_test_exclusion() {
  for (Query_expression *unit = first_inner_query_expression(); unit;
       unit = unit->next_query_expression())
    for (Query_block *sl = unit->first_query_block(); sl;
         sl = sl->next_query_block())
      sl->propagate_unique_test_exclusion();

  exclude_from_table_unique_test = true;
}

/**
  Add a list of full-text function elements into a query block.

  @param ftfuncs   List of full-text function elements to add.

  @returns false if success, true if error
*/

bool Query_block::add_ftfunc_list(List<Item_func_match> *ftfuncs) {
  Item_func_match *ifm;
  List_iterator_fast<Item_func_match> li(*ftfuncs);
  while ((ifm = li++)) {
    if (ftfunc_list->push_back(ifm)) return true; /* purecov: inspected */
  }
  return false;
}

/**
   Go through a list of tables and join nests, recursively, and repoint
   its query_block pointer.

   @param  join_list  List of tables and join nests
*/
void Query_block::repoint_contexts_of_join_nests(
    mem_root_deque<Table_ref *> join_list) {
  for (Table_ref *tbl : join_list) {
    tbl->query_block = this;
    if (tbl->nested_join)
      repoint_contexts_of_join_nests(tbl->nested_join->m_tables);
  }
}

/**
  Merge name resolution context objects belonging to an inner subquery
  to parent query block.
  Update all context objects to have this base query block.
  Used when a subquery's query block is merged into its parent.

  @param inner  Subquery for which context objects are to be merged.
*/
void Query_block::merge_contexts(Query_block *inner) {
  for (Name_resolution_context *ctx = inner->first_context; ctx != nullptr;
       ctx = ctx->next_context) {
    ctx->query_block = this;
    if (ctx->next_context == nullptr) {
      ctx->next_context = first_context;
      first_context = inner->first_context;
      inner->first_context = nullptr;
      break;
    }
  }
}

/**
   For a table subquery predicate (IN/ANY/ALL/EXISTS/etc):
   since it does not support LIMIT the following clauses are redundant:

   ORDER BY
   DISTINCT
   GROUP BY   if there are no aggregate functions and no HAVING clause

   For a scalar subquery without LIMIT:
   ORDER BY is redundant, as the number of rows to order must be 1.

   This removal is permanent. Thus, it only makes sense to call this function
   for regular queries and on first execution of SP/PS

   @param thd               thread handler
   @return true on error
*/

bool Query_block::remove_redundant_subquery_clauses(THD *thd) {
  Item_subselect *subq_predicate = master_query_expression()->item;
  enum change {
    REMOVE_NONE = 0,
    REMOVE_ORDER = 1 << 0,
    REMOVE_DISTINCT = 1 << 1,
    REMOVE_GROUP = 1 << 2
  };
  uint possible_changes;

  if (subq_predicate->subquery_type() == Item_subselect::SCALAR_SUBQUERY) {
    if (has_limit()) return false;
    possible_changes = REMOVE_ORDER;
  } else {
    assert(subq_predicate->subquery_type() == Item_subselect::EXISTS_SUBQUERY ||
           subq_predicate->subquery_type() == Item_subselect::IN_SUBQUERY ||
           subq_predicate->subquery_type() == Item_subselect::ALL_SUBQUERY ||
           subq_predicate->subquery_type() == Item_subselect::ANY_SUBQUERY);
    possible_changes = REMOVE_ORDER | REMOVE_DISTINCT | REMOVE_GROUP;
  }

  uint changelog = 0;

  if ((possible_changes & REMOVE_ORDER) && order_list.elements) {
    changelog |= REMOVE_ORDER;
    if (empty_order_list(this)) return true;
  }

  if ((possible_changes & REMOVE_DISTINCT) && is_distinct()) {
    changelog |= REMOVE_DISTINCT;
    remove_base_options(SELECT_DISTINCT);
  }

  /*
    Remove GROUP BY if there are no aggregate functions, no HAVING clause,
    no non-primitive grouping and no windowing functions.
  */

  if ((possible_changes & REMOVE_GROUP) && group_list.elements &&
      !agg_func_used() && !having_cond() && olap == UNSPECIFIED_OLAP_TYPE &&
      m_windows.elements == 0) {
    changelog |= REMOVE_GROUP;
    for (ORDER *g = group_list.first; g != nullptr; g = g->next) {
      if (g->is_item_original()) {
        Item::Cleanup_after_removal_context ctx(this);
        (*g->item)->walk(&Item::clean_up_after_removal, walk_options,
                         pointer_cast<uchar *>(&ctx));
      }
    }
    group_list.clear();
    while (hidden_group_field_count-- > 0) {
      fields.pop_front();
      base_ref_items[fields.size()] = nullptr;
    }
  }

  if (changelog) {
    Opt_trace_context *trace = &thd->opt_trace;
    if (unlikely(trace->is_started())) {
      Opt_trace_object trace_wrapper(trace);
      Opt_trace_array trace_changes(trace, "transformations_to_subquery");
      if (changelog & REMOVE_ORDER) trace_changes.add_alnum("removed_ordering");
      if (changelog & REMOVE_DISTINCT)
        trace_changes.add_alnum("removed_distinct");
      if (changelog & REMOVE_GROUP) trace_changes.add_alnum("removed_grouping");
    }
  }
  return false;
}

/**
  Empty the ORDER list.
  Delete corresponding elements from fields and base_ref_items too.
  If ORDER list contain any subqueries, delete them from the query block list.

  @param sl  Query block that possible subquery blocks in the ORDER BY clause
             are attached to (may be different from "this" when query block has
             been merged into an outer query block).
  @returns true on error
*/

bool Query_block::empty_order_list(Query_block *sl) {
  for (ORDER *o = order_list.first; o != nullptr; o = o->next) {
    if (o->is_item_original()) {
      Item *const order_item = o->item_initial;
      Item::Cleanup_after_removal_context ctx(sl);
      order_item->walk(&Item::clean_up_after_removal, walk_options,
                       pointer_cast<uchar *>(&ctx));
      if (order_item->hidden && m_windows.elements != 0) {
        // Below, when we pop off the unused expression from the select list,
        // we do it only if the query block has no windows. So, instead, we
        // replace the ordering expression in the select list and
        // base_ref_items with a hidden NULL which is harmless.
        Item *const replacement = new (parent_lex->thd->mem_root) Item_null;
        if (replacement == nullptr) return true;
        replacement->hidden = true;
        std::replace(fields.begin(), fields.end(), order_item, replacement);
        std::replace(base_ref_items.begin(),
                     base_ref_items.begin() + fields.size(), order_item,
                     replacement);
      }
    }
  }
  order_list.clear();
  if (m_windows.elements != 0) {
    /*
      The next lines doing cleanup of ORDER elements expect the
      query block's ORDER BY items to be the last part of fields and
      base_ref_items, as they just chop the lists' end. But if there is a
      window, that end is actually the PARTITION BY and ORDER BY clause of the
      window, so do not chop then: leave the items in place.
    */
    return false;
  }
  while (hidden_order_field_count-- > 0) {
    fields.pop_front();
    base_ref_items[fields.size()] = nullptr;
  }
  return false;
}

/*****************************************************************************
  Group and order functions
*****************************************************************************/

/**
  Resolve an ORDER BY or GROUP BY column reference.

  Given a column reference (represented by 'order') from a GROUP BY or ORDER
  BY clause, find the actual column it represents. If the column being
  resolved is from the GROUP BY clause, the procedure searches the SELECT
  list 'fields' and the columns in the FROM list 'tables'. If 'order' is from
  the ORDER BY clause, only the SELECT list is being searched.

  If 'order' is resolved to an Item, then order->item is set to the found
  Item. If there is no item for the found column (that is, it was resolved
  into a table field), order->item is 'fixed' and is added to fields and
  ref_item_array.

  ref_item_array and fields are updated.

  @param[in] thd                    Pointer to current thread structure
  @param[in,out] ref_item_array     All select, group and order by fields
  @param[in] tables                 List of tables to search in (usually
    FROM clause)
  @param[in] order                  Column reference to be resolved
  @param[in,out] fields             List of fields to search in (usually
    SELECT list; hidden items are ignored)
  @param[in] is_group_field         True if order is a GROUP field, false if
    ORDER by field
  @param[in] is_window_order        True if order is a Window function's
    PARTITION BY or ORDER BY field

  @retval
    false if OK
  @retval
    true  if error occurred
*/

bool find_order_in_list(THD *thd, Ref_item_array ref_item_array,
                        Table_ref *tables, ORDER *order,
                        mem_root_deque<Item *> *fields, bool is_group_field,
                        bool is_window_order) {
  Item *order_item = *order->item; /* The item from the GROUP/ORDER clause. */
  Item::Type order_item_type;
  Item **select_item; /* The corresponding item from the SELECT clause. */
  Field *from_field;  /* The corresponding field from the FROM clause. */
  uint counter;
  enum_resolution_type resolution;

  /*
    Local SP variables may be int but are expressions, not positions.
    (And they can't be used before fix_fields is called for them).
  */
  if (order_item->type() == Item::INT_ITEM &&
      order_item->basic_const_item()) { /* Order by position */
    uint count = (uint)order_item->val_int();
    if (!count || count > CountVisibleFields(*fields)) {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), order_item->full_name(), thd->where);
      return true;
    }
    order->item = &ref_item_array[count - 1];
    // Order by is now referencing select expression, so increment the reference
    // count for the select expression.
    (*order->item)->increment_ref_count();
    order->in_field_list = true;
    return false;
  }
  /* Lookup the current GROUP/ORDER field in the SELECT clause. */
  if (find_item_in_list(thd, order_item, fields, &select_item, &counter,
                        &resolution)) {
    return true;
  }

  /* Check whether the resolved field is unambiguous. */
  if (select_item != nullptr) {
    Item *view_ref = nullptr;
    /*
      If we have found field not by its alias in select list but by its
      original field name, we should additionally check if we have conflict
      for this name (in case if we would perform lookup in all tables).
    */
    if (resolution == RESOLVED_BEHIND_ALIAS && !order_item->fixed &&
        order_item->fix_fields(thd, order->item))
      return true;

    /*
      Lookup the current GROUP or WINDOW partition by or order by field in the
      FROM clause.
    */
    order_item_type = order_item->type();
    from_field = not_found_field;
    if (((is_group_field || is_window_order) &&
         order_item_type == Item::FIELD_ITEM) ||
        order_item_type == Item::REF_ITEM) {
      from_field = find_field_in_tables(thd, (Item_ident *)order_item, tables,
                                        nullptr, &view_ref, IGNORE_ERRORS, true,
                                        // view_ref is a local variable, so
                                        // don't record a change to roll back:
                                        false);
      if (thd->is_error()) return true;

      if (!from_field) from_field = not_found_field;
    }

    if (from_field == not_found_field ||
        (from_field != view_ref_found
             ?
             /* it is field of base table => check that fields are same */
             ((*select_item)->type() == Item::FIELD_ITEM &&
              ((Item_field *)(*select_item))->field->eq(from_field))
             :
             /*
               in is field of view table => check that references on translation
               table are same
             */
             ((*select_item)->type() == Item::REF_ITEM &&
              view_ref->type() == Item::REF_ITEM &&
              down_cast<Item_ref *>(*select_item)->ref_pointer() ==
                  down_cast<Item_ref *>(view_ref)->ref_pointer()))) {
      /*
        If there is no such field in the FROM clause, or it is the same field
        as the one found in the SELECT clause, then use the Item created for
        the SELECT field. As a result if there was a derived field that
        'shadowed' a table field with the same name, the table field will be
        chosen over the derived field.

        If we replace *order->item with one from the select list or
        from a table in the FROM list, we should clean up after
        removing the old *order->item from the query. The item has not
        been fixed (so there are no aggregation functions that need
        cleaning up), but it may contain subqueries that should be
        unlinked.
      */
      if ((*order->item)->real_item() != (*select_item)->real_item()) {
        Item::Cleanup_after_removal_context ctx(
            thd->lex->current_query_block());
        (*order->item)
            ->walk(&Item::clean_up_after_removal, walk_options,
                   pointer_cast<uchar *>(&ctx));
      }
      order->item = &ref_item_array[counter];
      // Order by is now referencing select expression, so increment the
      // reference count for the select expression.
      (*order->item)->increment_ref_count();
      order->in_field_list = true;
      if (resolution == RESOLVED_AGAINST_ALIAS && from_field == not_found_field)
        order->used_alias = (*order->item)->item_name.ptr();
      return false;
    }
    /*
      There is a field with the same name in the FROM clause. This
      is the field that will be chosen. In this case we issue a
      warning so the user knows that the field from the FROM clause
      overshadows the column reference from the SELECT list.
      For window functions we do not need to issue this warning
      (field should resolve to a unique column in the FROM derived
      table expression, cf. SQL 2016 section 7.15 SR 4)
    */
    if (!is_window_order) {
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_NON_UNIQ_ERROR,
                          ER_THD(thd, ER_NON_UNIQ_ERROR),
                          ((Item_ident *)order_item)->field_name, thd->where);
    }
  }

  // If we couldn't find the item, see if we can find it in a merged derived
  // table, hidden behind an Item_view_ref. This is a lowest-priority
  // fallback to make sure we don't add the field twice to the select list;
  // once as hidden (directly) and once as visible (through the view_ref).
  // Such double-adds would be a problem if we later create a temporary table
  // containing the item, which will call item->get_tmp_table_item() and
  // effectively peel away the ref -- an item cannot be both visible and
  // hidden at the same time.
  counter = 0;
  for (auto it = VisibleFields(*fields).begin();
       it != VisibleFields(*fields).end(); ++it, ++counter) {
    Item *item = *it;
    if (item->type() == Item::REF_ITEM &&
        ((Item_ref *)item)->ref_type() == Item_ref::VIEW_REF) {
      Item_view_ref *item_ref = down_cast<Item_view_ref *>(item);
      if (item_ref->cached_table->is_merged() &&
          order_item->eq(item_ref->ref_item(), false)) {
        order->item = &ref_item_array[counter];
        // Order by is now referencing select expression, so increment the
        // reference count for the select expression.
        (*order->item)->increment_ref_count();
        order->in_field_list = true;
        return false;
      }
    }
  }

  order->in_field_list = false;
  /*
    The call to order_item->fix_fields() means that here we resolve
    'order_item' to a column from a table in the list 'tables', or to
    a column in some outer query. Exactly because of the second case
    we come to this point even if (select_item == nullptr),
    in spite of that fix_fields() calls find_item_in_list() one more
    time.

    We check order_item->fixed because Item_func_group_concat can put
    arguments for which fix_fields already was called.

    group_fix_field = true is so that we properly reject GROUP BY on
    subqueries with references to group fields.
  */
  bool save_group_fix_field = thd->lex->current_query_block()->group_fix_field;
  if (is_group_field) thd->lex->current_query_block()->group_fix_field = true;
  bool ret =
      (!order_item->fixed && (order_item->fix_fields(thd, order->item) ||
                              (order_item = *order->item)->check_cols(1)));
  thd->lex->current_query_block()->group_fix_field = save_group_fix_field;
  if (ret) return true; /* Wrong field. */

  order_item->increment_ref_count();

  assert_consistent_hidden_flags(*fields, order_item, /*hidden=*/true);

  uint el = fields->size();
  order_item->hidden = true;
  fields->push_front(order_item); /* Add new field to field list. */
  ref_item_array[el] = order_item;
  /*
    If the order_item is a SUM_FUNC_ITEM, when fix_fields is called
    referenced_by is set to order->item which is the address of order_item.
    But this needs to be address of order_item in the fields list.
    As a result, when it gets replaced with Item_aggregate_ref
    object in Item::split_sum_func2, we will be able to retrieve the
    newly created object.
  */
  if (order_item->type() == Item::SUM_FUNC_ITEM)
    down_cast<Item_sum *>(order_item)->referenced_by[0] = &(*fields)[0];

  /*
    Currently, we assume that this assertion holds. If it turns out
    that it fails for some query, order->item has changed and the old
    item is removed from the query. In that case, we must call walk()
    with clean_up_after_removal() on the old order->item.
  */
  assert(order_item == *order->item);
  order->item = &ref_item_array[el];
  return false;
}

/**
  Resolve and setup list of expressions in ORDER BY clause.

  Change order to point at item in select list.
  If item isn't a number and doesn't exists in the select list, add it to the
  the field list.

  @param thd            Current session.
  @param ref_item_array The Ref_item_array for this query block.
  @param tables         From clause of the query.
  @param fields         All columns, including hidden ones.
  @param order          The query block's order clause.

  @returns false if success, true if error.
*/

bool setup_order(THD *thd, Ref_item_array ref_item_array, Table_ref *tables,
                 mem_root_deque<Item *> *fields, ORDER *order) {
  DBUG_TRACE;

  assert(order);

  Query_block *const select = thd->lex->current_query_block();

  thd->where = "order clause";

  const bool for_set_operation =
      select->master_query_expression()->is_set_operation() &&
      select == select->master_query_expression()->query_term()->query_block();
  const bool is_aggregated = select->is_grouped();

  for (uint number = 1; order; order = order->next, number++) {
    Item *order_item = *order->item;
    if (order_item->fixed && !order_item->const_item()) {
      // If a non constant expression in order by is already
      // resolved, it must have been merged from a derived table.
      // So, we do not need to re-resolve in this query block. Add
      // a hidden item if not present in the visible fields list.
      // Update with the correct ref item.
      uint counter = fields->size();
      for (uint i = 0; i < fields->size(); i++) {
        if (order_item->real_item()->eq(ref_item_array[i]->real_item(),
                                        false)) {
          order->item = &ref_item_array[i];
          // Order by is now referencing select expression, so increment the
          // reference count for the select expression.
          (*order->item)->increment_ref_count();
          order->in_field_list = true;
          counter = i;
          break;
        }
      }
      if (counter == fields->size()) {
        // Add as a hidden item.
        ref_item_array[counter] = order_item;
        fields->push_front(order_item);
        order_item->hidden = true;
        order->in_field_list = false;
        order->item = &ref_item_array[counter];
      }
      continue;
    }

    if (find_order_in_list(thd, ref_item_array, tables, order, fields, false,
                           false))
      return true;
    if ((*order->item)->has_aggregation()) {
      /*
        Aggregated expressions in ORDER BY are not supported by SQL standard,
        but MySQL has some limited support for them. The limitations are
        checked below:

        1. A set operation query is not aggregated, so ordering by a set
           function is always wrong.
      */
      if (for_set_operation) {
        my_error(ER_AGGREGATE_ORDER_FOR_UNION, MYF(0), number);
        return true;
      }

      /*
        2. A non-aggregated query combined with a set function in ORDER BY
           that does not contain an outer reference is illegal, because it
           would cause the query to become aggregated.
           (Since is_aggregated is false, this expression would cause
            agg_func_used() to become true).
      */
      if (!is_aggregated && select->agg_func_used()) {
        my_error(ER_AGGREGATE_ORDER_NON_AGG_QUERY, MYF(0), number);
        return true;
      }
    }
    if (for_set_operation && (*order->item)->has_wf()) {
      // Window function in ORDER BY of set operation not supported,
      // SQL2014 4.16.3
      my_error(ER_AGGREGATE_ORDER_FOR_UNION, MYF(0), number);
      return true;
    }
    if ((*order->item)->data_type() == MYSQL_TYPE_INVALID &&
        (*order->item)->propagate_type(thd, MYSQL_TYPE_VARCHAR))
      return true;
  }
  return false;
}

/**
   Runs checks mandated by ONLY_FULL_GROUP_BY

   @param  thd                     THD pointer

   @returns true if ONLY_FULL_GROUP_BY is violated.
*/

bool Query_block::check_only_full_group_by(THD *thd) {
  bool rc = false;

  if (is_grouped()) {
    /*
      "root" has very short lifetime, and should not consume much
      => not instrumented.
    */
    MEM_ROOT root(PSI_NOT_INSTRUMENTED, MEM_ROOT_BLOCK_SIZE);
    {
      Group_check gc(this, &root);
      rc = gc.check_query(thd);
      gc.to_opt_trace(thd);
    }  // Scope, to let any destructor run before the MEM_ROOT DTOR.
  }

  if (!rc && is_distinct()) {
    Distinct_check dc(this);
    rc = dc.check_query(thd);
  }

  return rc;
}

/**
  Do final setup of ORDER BY clause, after the query block is fully resolved.

  Check that ORDER BY clause is not redundant.
  Split any aggregate functions.

  @param thd                      Thread handler

  @returns false if success, true if error
*/
bool Query_block::setup_order_final(THD *thd) {
  DBUG_TRACE;
  if (is_implicitly_grouped()) {
    // Result will contain zero or one row - ordering is redundant
    return empty_order_list(this);
  }

  if (!master_query_expression()->is_simple()) {
    std::pair<bool, bool> result =
        master_query_expression()->query_term()->redundant_order_by(this, 0);
    assert(result.first);  // that we found the block
    if (result.second) {
      // Part of set operation which requires global ordering may skip local
      // order
      if (empty_order_list(this)) return true;
    }
  }

  for (ORDER *ord = order_list.first; ord; ord = ord->next) {
    Item *const item = *ord->item;

    const bool is_grouped_aggregate =
        (item->type() == Item::SUM_FUNC_ITEM && !item->m_is_window_function);
    if (is_grouped_aggregate) continue;

    if (item->has_aggregation() || item->has_wf()) {
      if (item->split_sum_func(thd, base_ref_items, &fields)) {
        return true; /* purecov: inspected */
      }
    }
  }
  return false;
}

/**
  Resolve and set up the GROUP BY list.

  @param thd			Thread handler

  @todo
    change ER_WRONG_FIELD_WITH_GROUP to more detailed
    ER_NON_GROUPING_FIELD_USED

  @returns false if success, true if error
*/

bool Query_block::setup_group(THD *thd) {
  DBUG_TRACE;
  assert(group_list.elements);

  thd->where = "group statement";

  for (ORDER *group = group_list.first; group; group = group->next) {
    if (find_order_in_list(thd, base_ref_items, get_table_list(), group,
                           &fields, true, false))
      return true;

    Item *item = *group->item;
    if (item->has_aggregation() || item->has_wf()) {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), (*group->item)->full_name());
      return true;
    }

    else if (item->has_grouping_func()) {
      my_error(ER_WRONG_GROUP_FIELD, MYF(0), "GROUPING function");
      return true;
    }
    if (item->data_type() == MYSQL_TYPE_INVALID &&
        item->propagate_type(thd, MYSQL_TYPE_VARCHAR))
      return true;
  }

  return false;
}

/****************************************************************************
 ROLLUP handling
 ****************************************************************************/

ORDER *Query_block::find_in_group_list(Item *item, int *rollup_level) const {
  Item *real_item = item->real_item();
  if (real_item->type() == Item::CACHE_ITEM) {
    // Unwrap the cache, if any. NOTE: There should never be any caches
    // in the GROUP BY list, so we don't need to unwrap any from there.
    real_item = down_cast<const Item_cache *>(real_item)->get_example();
  }

  ORDER *best_candidate = nullptr;
  int idx = 0;
  for (ORDER *group = group_list.first; group; group = group->next, ++idx) {
    Item *group_item = *group->item;
    assert(group_item->real_item()->type() != Item::CACHE_ITEM);
    if (real_item->eq(group_item->real_item(), /*binary_cmp=*/false)) {
      if (item->item_name.ptr() != nullptr &&
          group_item->item_name.ptr() != nullptr &&
          item->item_name.eq(group_item->item_name)) {
        // Match on group _and_ alias; return immediately.
        if (rollup_level != nullptr) {
          *rollup_level = idx;
        }
        return group;
      } else if (best_candidate == nullptr) {
        // Match on group but not alias; it's a good candidate,
        // but only if we don't find a better match. (If there
        // are multiple such candidates, we use the leftmost one.)
        if (rollup_level != nullptr) {
          *rollup_level = idx;
        }
        best_candidate = group;
      }
    }
  }
  return best_candidate;
}

int Query_block::group_list_size() const {
  int size = 0;
  for (ORDER *group = group_list.first; group; group = group->next) {
    ++size;
  }
  return size;
}

/**
  Checks whether an item matches a grouped expression, creates an
  Item_rollup_group_item around it and replaces the reference to it with that
  item.
 */
static ReplaceResult wrap_grouped_expressions_for_rollup(
    Query_block *select, Item *item, Item *parent, unsigned argument_idx) {
  if (is_rollup_group_wrapper(item->real_item())) {
    // This item must already be a group item, or we wouldn't have
    // wrapped it earlier. No need to do anything more about it,
    // since it's already wrapped (also, don't traverse further).
    return {ReplaceResult::REPLACE, item};
  }

  int rollup_level = 0;
  ORDER *group = select->find_in_group_list(item, &rollup_level);
  if (group != nullptr) {
    Item_rollup_group_item *new_item =
        new Item_rollup_group_item(rollup_level, item);
    if (new_item == nullptr || select->rollup_group_items.push_back(new_item)) {
      return {ReplaceResult::ERROR, nullptr};
    }
    new_item->quick_fix_field();
    if (group->rollup_item == nullptr) {
      group->rollup_item = new_item;
    }
    return {ReplaceResult::REPLACE, new_item};
  } else if (parent != nullptr && parent->type() == Item::FUNC_ITEM &&
             down_cast<Item_func *>(parent)->functype() ==
                 Item_func::GROUPING_FUNC) {
    my_error(ER_FIELD_IN_GROUPING_NOT_GROUP_BY, MYF(0), (argument_idx + 1));
    return {ReplaceResult::ERROR, nullptr};
  }

  return {ReplaceResult::KEEP_TRAVERSING, nullptr};
}

/**
   Helper function for WalkAndReplace() which replaces the Item referenced by
   "child_ref" if "get_new_item" returns a replacement, or visits the children
   of "child_ref" otherwise.
 */
static bool WalkAndReplaceInner(
    THD *thd, Item *parent, unsigned argument_idx,
    const function<ReplaceResult(Item *item, Item *parent,
                                 unsigned argument_idx)> &get_new_item,
    Item **child_ref) {
  ReplaceResult result = get_new_item(*child_ref, parent, argument_idx);
  if (result.action == ReplaceResult::ERROR) {
    return true;
  }

  if (result.action == ReplaceResult::REPLACE) {
    if (thd->lex->is_exec_started()) {
      thd->change_item_tree(child_ref, result.replacement);
    } else {
      *child_ref = result.replacement;
    }
    return false;
  }

  return WalkAndReplace(thd, *child_ref, get_new_item);
}

bool WalkAndReplace(
    THD *thd, Item *item,
    const function<ReplaceResult(Item *item, Item *parent,
                                 unsigned argument_idx)> &get_new_item) {
  if (item->type() == Item::FUNC_ITEM ||
      (item->type() == Item::SUM_FUNC_ITEM && item->m_is_window_function)) {
    Item **args = down_cast<Item_func *>(item)->arguments();
    const unsigned arg_count = down_cast<Item_func *>(item)->argument_count();
    for (unsigned argument_idx = 0; argument_idx < arg_count; argument_idx++) {
      if (WalkAndReplaceInner(thd, item, argument_idx, get_new_item,
                              &args[argument_idx])) {
        return true;
      }
    }

    if (item->m_is_window_function) {
      down_cast<Item_sum *>(item)->update_after_wf_arguments_changed(thd);
    }
  } else if (item->type() == Item::ROW_ITEM) {
    // Pretty much exactly the same logic as functions above.
    Item_row *row_item = down_cast<Item_row *>(item);
    for (unsigned argument_idx = 0; argument_idx < row_item->cols();
         argument_idx++) {
      if (WalkAndReplaceInner(thd, item, argument_idx, get_new_item,
                              row_item->addr(argument_idx))) {
        return true;
      }
    }
  } else if (item->type() == Item::COND_ITEM) {
    Item_cond *cond_item = down_cast<Item_cond *>(item);
    List_iterator<Item> li(*cond_item->argument_list());
    unsigned argument_idx = 0;
    for (Item *arg = li++; arg != nullptr; arg = li++) {
      if (WalkAndReplaceInner(thd, item, argument_idx++, get_new_item,
                              li.ref())) {
        return true;
      }
    }
  } else if (item->type() == Item::SUBQUERY_ITEM) {
    const Item_subselect::Subquery_type subquery_type =
        down_cast<Item_subselect *>(item)->subquery_type();
    if (subquery_type == Item_subselect::IN_SUBQUERY ||
        subquery_type == Item_subselect::ALL_SUBQUERY ||
        subquery_type == Item_subselect::ANY_SUBQUERY) {
      return WalkAndReplaceInner(
          thd, item, /*argument_idx=*/0, get_new_item,
          &down_cast<Item_in_subselect *>(item)->left_expr);
    }
  }
  return false;
}

/**
  Marks occurrences of group by fields in a function's arguments as nullable,
  so that we do not optimize them away before we get to add the rollup wrappers.

  @todo
    Some functions are not null-preserving. For those functions
    updating of the m_nullable attribute is an overkill.

*/

void Query_block::mark_item_as_maybe_null_if_non_primitive_grouped(
    Item *item) const {
  if (find_in_group_list(item, /*rollup_level=*/nullptr) != nullptr) {
    /*
      If this item is present in GROUP BY clause, set m_nullable
      to true, as ROLLUP will generate NULLs for this column.
      This prevents the optimizer from constant-folding away
      IS NULL expressions (e.g. in HAVING). This must be done
      before we start resolving subselects in m_having_cond.
    */
    item->set_nullable(true);
  }
}

Item *Query_block::single_visible_field() const {
  Item *ret = nullptr;
  for (Item *item : visible_fields()) {
    if (ret != nullptr) {
      // More than one.
      return nullptr;
    }
    ret = item;
  }
  return ret;
}

size_t Query_block::num_visible_fields() const {
  return CountVisibleFields(fields);
}

bool Query_block::field_list_is_empty() const {
  for (Item *item : fields) {
    if (!item->hidden) return false;
  }
  return true;
}

/**
  Refreshes the comparators after ROLLUP resolving.

  This is needed because ROLLUP resolving happens after the comparators have
  been set up. In ROLLUP resolving, it may turn out that something initially
  believed to be constant, is not constant after all (e.g., group items that may
  be NULL in some cases). So we call set_cmp_func() to make Arg_comparator
  adjust/remove its caches accordingly.
*/
static bool refresh_comparators_after_rollup(Item *item) {
  return WalkItem(item, enum_walk::POSTFIX, [](Item *inner_item) {
    if (inner_item->type() != Item::FUNC_ITEM) {
      return false;
    }
    switch (down_cast<Item_func *>(inner_item)->functype()) {
      case Item_func::GE_FUNC:
      case Item_func::GT_FUNC:
      case Item_func::LT_FUNC:
      case Item_func::LE_FUNC:
      case Item_func::EQ_FUNC:
      case Item_func::NE_FUNC:
      case Item_func::EQUAL_FUNC:
        return down_cast<Item_bool_func2 *>(inner_item)->set_cmp_func();
      default:
        return false;
    }
  });
}

/**
  Resolve an item (and its tree) for rollup processing by replacing items
  matching grouped expressions with Item_rollup_group_items and
  updating properties (m_nullable, PROP_ROLLUP_FIELD).
  Also check any GROUPING function for incorrect column.

  @param   thd      session context
  @param   item     the item to be processed
  @returns the new item, or nullptr on error
*/
Item *Query_block::resolve_rollup_item(THD *thd, Item *item) {
  ReplaceResult result =
      wrap_grouped_expressions_for_rollup(this, item, nullptr, 0);
  if (result.action == ReplaceResult::ERROR) {
    return nullptr;
  } else if (result.action == ReplaceResult::REPLACE) {
    item->set_nullable(true);
    return result.replacement;
  }
  bool changed = false;
  bool error = WalkAndReplace(
      thd, item,
      [this, &changed](Item *inner_item, Item *parent, unsigned argument_idx) {
        ReplaceResult inner_result = wrap_grouped_expressions_for_rollup(
            this, inner_item, parent, argument_idx);
        changed |= (inner_result.action == ReplaceResult::REPLACE);
        return inner_result;
      });
  if (error) return nullptr;
  if (changed) {
    if (refresh_comparators_after_rollup(item)) {
      return nullptr;
    }
    item->update_used_tables();
    // Since item is now nullable, mark every expression (except rollup sum
    // functions) depending on it as also potentially nullable. (This is a
    // conservative choice; in some cases, expressions can be proven
    // non-nullable even for NULL arguments.)
    class Update_nullability_for_rollup_items : public Item_tree_walker {
     public:
      using Item_tree_walker::is_stopped;
      using Item_tree_walker::stop_at;
    };
    Update_nullability_for_rollup_items info;
    if (WalkItem(
            item, enum_walk::PREFIX | enum_walk::POSTFIX,
            [&info](Item *inner_item) {
              if (info.is_stopped(inner_item)) {
                return false;
              } else if (inner_item->type() == Item::SUM_FUNC_ITEM &&
                         down_cast<Item_sum *>(inner_item)->real_sum_func() ==
                             Item_sum::ROLLUP_SUM_SWITCHER_FUNC) {
                info.stop_at(inner_item);
                return false;
              } else {
                inner_item->set_nullable(true);
                return false;
              }
            })) {
      return nullptr;
    }
  }
  return item;
}

Item *create_rollup_switcher(THD *thd, Query_block *query_block, Item_sum *item,
                             int send_group_parts) {
  assert(!item->m_is_window_function);
  assert(!item->is_rollup_sum_wrapper());

  List<Item> alternatives;
  alternatives.push_back(item);
  for (int level = 0; level < send_group_parts; ++level) {
    Item_sum *new_item = down_cast<Item_sum *>(item->copy_or_same(thd));
    if (new_item == nullptr) {
      return nullptr;
    }
    new_item->make_unique();
    if (alternatives.push_back(new_item)) {
      return nullptr;
    }
  }
  Item_rollup_sum_switcher *new_item =
      new Item_rollup_sum_switcher(&alternatives);
  if (new_item == nullptr || query_block->rollup_sums.push_back(new_item)) {
    return nullptr;
  }
  new_item->quick_fix_field();
  return new_item;
}

/**
  Resolve items in SELECT list and ORDER BY list for rollup processing

  @param   thd   session context

  @returns false if success, true if error
*/

bool Query_block::resolve_rollup(THD *thd) {
  DBUG_TRACE;

  uint send_group_parts = group_list_size();

  for (auto it = fields.begin(); it != fields.end(); ++it) {
    Item *item = *it;
    Item *new_item;
    if (Item_sum * item_sum; item->type() == Item::SUM_FUNC_ITEM &&
                             !item->const_item() &&
                             (item_sum = down_cast<Item_sum *>(item),
                              item_sum->aggr_query_block == this)) {
      // This is a top level aggregate, which must be replaced with
      // a different one for each rollup level.
      new_item = create_rollup_switcher(thd, this, item_sum, send_group_parts);
    } else {
      new_item = resolve_rollup_item(thd, item);
    }
    if (new_item == nullptr) {
      return true;
    }
    *it = new_item;
  }
  return false;
}

/**
  Checks if there are any calls to the MATCH function that take a ROLLUP column
  as argument in the SELECT list, GROUP BY clause, HAVING clause or ORDER BY
  clause. Such calls should be rejected, since MATCH only works on base columns.
*/
static bool fulltext_uses_rollup_column(const Query_block *query_block) {
  if (query_block->olap != ROLLUP_TYPE || !query_block->has_ft_funcs()) {
    return false;
  }

  // References to ROLLUP columns in SELECT and HAVING are represented
  // by Item_rollup_group_items. So we can just check if any of the MATCH
  // functions has such an argument.
  for (Item_func_match &match : *query_block->ftfunc_list) {
    if (match.has_grouping_set_dep()) {
      return true;
    }
  }

  // The references in ORDER BY and GROUP BY are not wrapped in
  // Item_rollup_group_item, so we need to search for them.
  for (ORDER *order = query_block->order_list.first; order != nullptr;
       order = order->next) {
    if (WalkItem(*order->item, enum_walk::PREFIX, [query_block](Item *item) {
          if (is_function_of_type(item, Item_func::FT_FUNC)) {
            Item_func_match *match = down_cast<Item_func_match *>(item);
            for (unsigned i = 0; i < match->arg_count; ++i) {
              if (query_block->find_in_group_list(match->get_arg(i),
                                                  /*rollup_level=*/nullptr) !=
                  nullptr) {
                return true;
              }
            }
          }
          return false;
        })) {
      return true;
    }
  }
  for (ORDER *group = query_block->group_list.first; group != nullptr;
       group = group->next) {
    if (WalkItem(*group->item, enum_walk::PREFIX, [query_block](Item *item) {
          if (is_function_of_type(item, Item_func::FT_FUNC)) {
            Item_func_match *match = down_cast<Item_func_match *>(item);
            for (unsigned i = 0; i < match->arg_count; ++i) {
              if (query_block->find_in_group_list(match->get_arg(i),
                                                  /*rollup_level=*/nullptr) !=
                  nullptr) {
                return true;
              }
            }
          }
          return false;
        })) {
      return true;
    }
  }

  return false;
}

/**
  Replace group by field references inside window functions with references
  in the presence of ROLLUP.

  @param   thd   session context
  @returns false if success, true if error
*/

bool Query_block::resolve_rollup_wfs(THD *thd) {
  DBUG_TRACE;
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    Item *new_item = resolve_rollup_item(thd, *it);
    if (new_item == nullptr) return true;
    *it = new_item;

    // Any expression having a window function which involves rollup
    // expressions should be set nullable.
    if (!new_item->is_nullable()) {
      bool any_nullable_wf = false;
      WalkItem(new_item, enum_walk::POSTFIX,
               [&any_nullable_wf](Item *inner_item) {
                 if (inner_item->real_item()->type() == Item::SUM_FUNC_ITEM &&
                     inner_item->real_item()->m_is_window_function &&
                     inner_item->has_grouping_set_dep()) {
                   inner_item->set_nullable(true);
                   any_nullable_wf = true;
                 }
                 return false;
               });
      if (any_nullable_wf) new_item->set_nullable(true);
    }
  }
  /*
    When this method is called, all ORDER BY items not already present in
    the SELECT list have been added to the select list as hidden items,
    so we do not need to traverse order_list to see all items.
    The companion method, resolve_rollup, needs to traverse order_list
    list, because at the the time that method is called, the ORDER BY
    items haven't been added yet. Cf second loop in resolve_rollup.
  */

  return false;
}
/**
  @brief  validate_gc_assignment
  Check whether the other values except DEFAULT are assigned
  for generated columns.

  @param fields                     Item_fields list to be filled
  @param values                     values to fill with
  @param table                      table to be checked
  @return Operation status
    @retval false   OK
    @retval true    Error occurred

  @note  This function must be called after table->write_set has been
         filled.
*/
bool validate_gc_assignment(const mem_root_deque<Item *> &fields,
                            const mem_root_deque<Item *> &values,
                            TABLE *table) {
  Field **fld = nullptr;
  MY_BITMAP *bitmap = table->write_set;
  bool use_table_field = false;
  DBUG_TRACE;

  if (values.empty()) return false;

  // If fields has no elements, we use all table fields
  if (fields.empty()) {
    use_table_field = true;
    fld = table->field;
  }

  auto field_it = VisibleFields(fields).begin();
  auto value_it = VisibleFields(values).begin();
  while (value_it != VisibleFields(values).end()) {
    Item *value = *value_it++;
    const Field *rfield;

    if (!use_table_field)
      rfield = (down_cast<Item_field *>((*field_it++)->real_item()))->field;
    else
      rfield = *(fld++);
    if (rfield->table != table) continue;

    // Skip hidden system fields.
    if (rfield->is_hidden_by_system()) continue;

    // If any of the explicit values is DEFAULT
    if (rfield->m_default_val_expr &&
        value->type() == Item::DEFAULT_VALUE_ITEM) {
      // Restore the statement safety flag to current lex
      current_thd->lex->set_stmt_unsafe_flags(
          rfield->m_default_val_expr->get_stmt_unsafe_flags());
      // Mark the columns that this expression reads to rthe ead_set
      for (uint j = 0; j < table->s->fields; j++) {
        if (bitmap_is_set(&rfield->m_default_val_expr->base_columns_map, j)) {
          bitmap_set_bit(table->read_set, j);
        }
      }
    }

    /* skip non marked fields */
    if (!bitmap_is_set(bitmap, rfield->field_index())) continue;
    if (rfield->gcol_info && value->type() != Item::DEFAULT_VALUE_ITEM) {
      my_error(ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN, MYF(0),
               rfield->field_name, rfield->table->s->table_name.str);
      return true;
    }
  }
  return false;
}

/**
  Delete unused columns from merged tables.

  This function is called recursively for each join nest and/or table
  in the query block. For each merged table that it finds, each column
  that contains a subquery and is not marked as used is removed and
  the translation item is set to NULL.

  @param tables List of tables and join nests
*/

void Query_block::delete_unused_merged_columns(
    mem_root_deque<Table_ref *> *tables) {
  DBUG_TRACE;

  for (Table_ref *tl : *tables) {
    if (tl->nested_join == nullptr) continue;
    if (tl->is_merged()) {
      for (Field_translator *transl = tl->field_translation;
           transl < tl->field_translation_end; transl++) {
        Item *const item = transl->item;
        // Decrement the ref count as its no more used in
        // select list.
        if (item->decrement_ref_count()) continue;

        // Cleanup the item since its not referenced from
        // anywhere.
        assert(item->fixed);
        Item::Cleanup_after_removal_context ctx(this);
        item->walk(&Item::clean_up_after_removal, walk_options,
                   pointer_cast<uchar *>(&ctx));
        transl->item = nullptr;
      }
    }
    delete_unused_merged_columns(&tl->nested_join->m_tables);
  }
}

/**
  Add item to the hidden part of select list.

  @param item  the item to add

  @return Pointer to reference to the added item
*/

Item **Query_block::add_hidden_item(Item *item) {
  const uint el = fields.size();
  base_ref_items[el] = item;
  assert_consistent_hidden_flags(fields, item, /*hidden=*/true);
  fields.push_front(item);
  item->hidden = true;
  return &base_ref_items[el];
}

void Query_block::remove_hidden_items() {
  for (uint i = 0; i < hidden_items_from_optimization; i++) {
    fields.pop_front();
  }
  hidden_items_from_optimization = 0;
}

/**
  Resolve the rows of a table value constructor and aggregate the type of each
  column across rows.

  @param thd    thread handler

  @returns false if success, true if error
*/

bool Query_block::resolve_table_value_constructor_values(THD *thd) {
  // Item_values_column objects may be allocated; they should be persistent for
  // PREPARE statements.
  Prepared_stmt_arena_holder ps_arena_holder(thd);

  size_t num_rows = row_value_list->size();
  size_t row_degree = row_value_list->front()->size();

  // All table row value expressions shall be of the same degree. Note that
  // non-scalar subqueries are not allowed; we can simply count the number of
  // elements.
  if (row_degree > MAX_FIELDS) {
    my_error(ER_TOO_MANY_FIELDS, MYF(0));
    return true;
  }

  size_t row_index = 0;
  for (mem_root_deque<Item *> *values_row : *row_value_list) {
    if (values_row->size() != row_degree) {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), row_index + 1);
      return true;
    } else if (values_row->empty()) {
      // A table value constructor with empty row objects is a syntax error,
      // except when used as the source for an INSERT statement.
      my_error(ER_TABLE_VALUE_CONSTRUCTOR_MUST_HAVE_COLUMNS, MYF(0));
      return true;
    }

    size_t item_index = 0;
    for (auto it = values_row->begin(); it != values_row->end(); ++it) {
      Item *item = *it;
      if ((!item->fixed && item->fix_fields(thd, &*it)) ||
          (item = *it)->check_cols(1))
        return true; /* purecov: inspected */

      if (item->type() == Item::DEFAULT_VALUE_ITEM) {
        my_error(ER_TABLE_VALUE_CONSTRUCTOR_CANNOT_HAVE_DEFAULT, MYF(0));
        return true;
      }

      /*
        In case this item is or contains a parameter, propagate a default
        data type for the expression. Note that there is no context available
        here that can give us a good default value (like what is done when
        a VALUES clause is used directly with an INSERT statement).
      */
      if (item->data_type() == MYSQL_TYPE_INVALID) {
        if (item->propagate_type(thd, item->default_data_type())) return true;
      }

      if (row_index == 0) {
        // If single row, we skip setting up indirections.
        if (num_rows != 1 && first_execution) {
          Item_values_column *column = new Item_values_column(thd, item);
          if (column == nullptr) return true;
          column->add_used_tables(item);
          item = column;
        }
        // Make sure to also replace the reference in item_list. In the case
        // where fix_fields transforms an item, it.ref() will only update the
        // reference of values_row.
        if (first_execution) fields[item_index] = item;
      } else {
        Item_values_column *column = down_cast<Item_values_column *>(
            GetNthVisibleField(fields, item_index));
        if (column->join_types(thd, item)) return true;
        column->add_used_tables(item);
        column->fixed = true;  // Does not have regular fix_fields()
      }

      ++item_index;
    }

    ++row_index;
  }

  // base_ref_items is used during row_value_in_to_exists_transformer to set up
  // equality checks when transforming IN subquery predicates.
  if (setup_base_ref_items(thd)) return true;

  size_t name_len;
  char buff[NAME_LEN + 1];
  if (check_stack_overrun(thd, STACK_MIN_SIZE, pointer_cast<uchar *>(buff)))
    return true; /* purecov: inspected */

  size_t item_index = 0;
  for (Item *column : visible_fields()) {
    base_ref_items[item_index] = column;

    // Name the columns column_0, column_1, ...
    name_len = snprintf(buff, NAME_LEN, "column_%zu", item_index);
    column->item_name.copy(buff, name_len);

    ++item_index;
  }

  return false;
}

static bool baptize_item(THD *thd, Item *item, int *field_no);
static bool update_context_to_derived(Item *expr, Query_block *new_derived);

/**
  Replace a table subquery ([NOT] {IN, EXISTS}) with a join to a derived table.

  The principle of this transformation is:
  FROM [tables] WHERE ... AND/OR oe IN (SELECT ie FROM it) ...
  becomes
  FROM (tables) LEFT JOIN (SELECT DISTINCT ie FROM it) AS derived
                ON oe = derived.ie WHERE ... AND/OR derived.ie IS NOT NULL ...
  If the subquery predicate is top-level in WHERE, and not negated, we use
  JOIN instead of LEFT JOIN, and use TRUE instead of IS NOT NULL. If the
  subquery predicate is negated, we use IS NULL instead of IS NOT NULL. If the
  subquery predicate is without aggregation(etc), we decorrelate any equality
  from it, and, if negated, we also decorrelate '<>,<,<=,>,>='; thus we handle
  EXISTS too. If the subquery cannot be decorrelated, the derived table could be
  made LATERAL, but as a certain secondary engine doesn't support that we just
  return an error.

  @param thd   Connection handle
  @param subq  Item for subquery
  @returns true if error
*/

bool Query_block::transform_table_subquery_to_join_with_derived(
    THD *thd, Item_exists_subselect *subq) {
  assert(first_execution);
  Query_expression *const subs_query_expression = subq->query_expr();
  Query_block *subs_query_block = subs_query_expression->first_query_block();
  assert(subs_query_block->first_execution);

  subq->strategy = Subquery_strategy::DERIVED_TABLE;

  const int hidden_fields = CountHiddenFields(subs_query_block->fields);
  const bool no_aggregates = !subs_query_block->is_grouped() &&
                             !subs_query_block->with_sum_func &&
                             subs_query_block->having_cond() == nullptr &&
                             !subs_query_block->has_windows();
  const bool decorrelate =
      no_aggregates &&
      (subs_query_expression->uncacheable & UNCACHEABLE_DEPENDENT) &&
      subs_query_block->where_cond() != nullptr &&
      subs_query_block->where_cond()->is_outer_reference() &&
      // decorrelation adds to the SELECT list, and hidden fields make it
      // impossible (search for "hidden" in this function). Hidden fields
      // usually come from aggregation, which we disallowed just above, but also
      // if a SELECT list element is a subquery which contains an outer
      // reference to subs_query_block.
      hidden_fields == 0;

  // Ensure that all lists are consistent. all_fields should have an optional
  // prefix and then be fields_list. If no aggregates, base_ref_items should
  // start with fields_list.
  assert(hidden_fields >= 0);

  // We're going to build the lists of outer and inner semijoin
  // expressions:
  // - they start empty
  // - first (build_sj_exprs()), if this is IN, we add the left and right
  // expressions of IN; if this is EXISTS, we do nothing
  // - second (decorrelate_condition()), we decorrelate comparison operators
  // in the subquery, and add the resulting left and right expressions.

  mem_root_deque<Item *> sj_outer_exprs(thd->mem_root);
  mem_root_deque<Item *> sj_inner_exprs(thd->mem_root);
  Mem_root_array<Item_func::Functype> op_types(thd->mem_root);

  if (subq->subquery_type() == Item_subselect::IN_SUBQUERY) {
    build_sj_exprs(thd, &sj_outer_exprs, &sj_inner_exprs, subq,
                   subs_query_block);
    // All these expressions are compared with '=':
    op_types.resize(sj_outer_exprs.size(), Item_func::EQ_FUNC);
  } else {
    assert(subq->subquery_type() == Item_subselect::EXISTS_SUBQUERY);

    if (subs_query_block->is_table_value_constructor) {
      if ((subs_query_block->select_limit != nullptr &&
           !subs_query_block->select_limit->const_item()) ||
          (subs_query_block->offset_limit != nullptr &&
           !subs_query_block->offset_limit->const_item())) {
        subq->strategy = Subquery_strategy::SUBQ_MATERIALIZATION;
        // We can't determine until materialization time whether we have
        // an empty or non-empty result set, skip transform
        return false;
      }
    }
    // We must replace of all EXISTS' initial SELECT list with
    // constants, otherwise they will interfere in DISTINCT, indeed if we didn't
    // replace,
    // SELECT ... FROM ot WHERE EXISTS(SELECT c1 FROM it)
    // would become
    // SELECT ... FROM ot JOIN (SELECT DISTINCT c1 FROM it) AS dt
    // and we may get duplicate copies of a row of 'ot', wrongly.

    // Note that in setup_wild() we already do that, but only for "SELECT *",
    // not for an explicit list "SELECT expr1, expr2", so we still have to do
    // that here.

    // We cannot do that if the query is aggregated, consider:
    // EXISTS(SELECT SUM(a) AS x, b as y FROM t GROUP BY y HAVING x>2)
    // if we replace we get
    // EXISTS(SELECT 1, 1 FROM t GROUP BY y HAVING x>2)
    // And as 'x' points to 1, HAVING is "always false".
    // Resolving ensures that this assertion holds.
    assert(no_aggregates);

    if (subs_query_block->is_table_value_constructor) {
      // This transformation effectively converts a table value constructor
      // query block to a scalar subquery with zero or one constant rows.
      subs_query_block->is_table_value_constructor = false;
      // We checked above that we can evaluate LIMIT/OFFSET, so use that to
      // compute here whether result set is empty or not
      const ulonglong limit = (subs_query_block->select_limit != nullptr)
                                  ? subs_query_block->select_limit->val_uint()
                                  : std::numeric_limits<ulonglong>::max();
      const ulonglong offset = (subs_query_block->offset_limit != nullptr)
                                   ? subs_query_block->offset_limit->val_uint()
                                   : 0;
      const ulonglong actual_rows = subs_query_block->row_value_list->size();
      const bool empty_rs = limit == 0 || offset >= actual_rows;
      auto limes = new (thd->mem_root) Item_int(empty_rs ? 0 : 1);
      if (limes == nullptr) return true;

      subs_query_block->select_limit = limes;
      subs_query_block->offset_limit = nullptr;
    }

    Item::Cleanup_after_removal_context ctx(this);
    int i = 0;
    for (auto it = subs_query_block->visible_fields().begin();
         it != subs_query_block->visible_fields().end(); ++it, ++i) {
      Item *inner = *it;
      if (inner->basic_const_item()) continue;  // no need to replace it
      auto constant = new (thd->mem_root) Item_int(
          NAME_STRING("Not_used"), (longlong)1, MY_INT64_NUM_DECIMAL_DIGITS);
      *it = constant;
      subs_query_block->base_ref_items[i] = constant;
      // Expressions from the SELECT list will not be used; unlike in the case
      // of IN, they are not part of sj_inner_exprs.
      inner->walk(&Item::clean_up_after_removal, walk_options,
                  pointer_cast<uchar *>(&ctx));
    }
    subs_query_block->select_list_tables = 0;
  }

  Semijoin_decorrelation sj_decor(
      &sj_outer_exprs, &sj_inner_exprs,
      // If antijoin, we can decorrelate '<>', '>=', etc, too (but not '<=>'):
      // multiple inner rows may match '<>', but they will fail the IS NULL
      // condition, and if this condition is top-level in WHERE it will
      // eliminate the rows.
      (subq->can_do_aj &&
       subq->outer_condition_context == enum_condition_context::ANDS)
          ? &op_types
          : nullptr);

  if (decorrelate) {
    // We try to decorrelate it, by looking at equalities in its WHERE.
    // This helps for this common pattern:
    // EXISTS(SELECT FROM it WHERE it.c=ot.c AND <condition on 'it' only>)
    const int initial_sj_inner_exprs_count = sj_inner_exprs.size();

    if (subs_query_block->decorrelate_condition(sj_decor, nullptr)) return true;

    // Append inner expressions of decorrelated equalities to the SELECT
    // list. Correct context info of outer expressions.
    auto it_outer = sj_outer_exprs.begin() + initial_sj_inner_exprs_count;
    auto it_inner = sj_inner_exprs.begin() + initial_sj_inner_exprs_count;
    for (int i = 0; it_outer != sj_outer_exprs.end();
         ++it_outer, ++it_inner, ++i) {
      Item *inner = *it_inner;
      Item *outer = *it_outer;
      // In setup_base_ref_items() we allocated space for appending this
      // element.
      // If there were a hidden element (there is none, see the setting of
      // 'decorrelate'), we would be appending a *non*-hidden element
      // (participating in DISTINCT) *after* the hidden element, which would
      // break the usual layout of base_ref_items which is: "non-hidden then
      // hidden" (see Query_block::add_hidden_item()). While this layout is not
      // documented (?), it is safer to not break it.
      subs_query_block->base_ref_items[subs_query_block->fields.size()] = inner;
      subs_query_block->fields.push_back(inner);

      // Needed for fix_after_pullout:
      update_context_to_derived(outer, this);
      // Decorrelated outer expression will move to ON, so fix it.
      outer->fix_after_pullout(this, subs_query_block);
    }

    // Decorrelation identified new outer/inner expression pairs.
    // Recalculate used_tables() after that (the subquery may have become
    // uncorrelated). Because there is no aggregation, window functions, ORDER
    // BY, we only have to collect used_tables bits from the SELECT list, FROM
    // clause (outer-correlated derived tables and join conditions) and WHERE
    // clause.
    for (Item *inner : subs_query_block->visible_fields()) {
      subs_query_block->select_list_tables |= inner->used_tables();
    }

    table_map new_used_tables = subs_query_block->select_list_tables;
    if (subs_query_block->where_cond()) {
      subs_query_block->where_cond()->update_used_tables();
      new_used_tables |= subs_query_block->where_cond()->used_tables();
    }
    // Walk the FROM clause to gather any outer-correlated derived table or join
    // condition.
    walk_join_list(subs_query_block->m_table_nest, [&](Table_ref *tr) -> bool {
      if (tr->join_cond()) new_used_tables |= tr->join_cond()->used_tables();
      if (tr->is_derived() && tr->uses_materialization())
        new_used_tables |= tr->derived_query_expression()->m_lateral_deps;
      return false;
    });

    if (!(new_used_tables & OUTER_REF_TABLE_BIT)) {
      // there is no outer reference anymore
      subs_query_block->uncacheable &= ~UNCACHEABLE_DEPENDENT;
      subs_query_expression->uncacheable &= ~UNCACHEABLE_DEPENDENT;
      // this must be called only after the change to 'uncacheable' above
      subq->update_used_tables();
    }
  }

  if (!subs_query_block->can_skip_distinct())
    subs_query_block->add_base_options(SELECT_DISTINCT);

  // As the synthesised ON and WHERE will reference columns of the derived
  // table, we must have unique names.
  // A derived table must have unique column names, while a quantified
  // subquery needn't; so names may not currently be unique and we have to
  // make them so.
  {
    int i = 1;
    for (Item *inner : subs_query_block->visible_fields()) {
      if (baptize_item(thd, inner, &i)) return true;
    }
  }

  // If the subquery is (still) correlated, we would need to create a LATERAL
  // derived table, but a certain secondary engine doesn't support it. Error:
  if ((subq->subquery_used_tables() & ~PSEUDO_TABLE_BITS) != 0) {
    my_error(ER_SUBQUERY_TRANSFORM_REJECTED, MYF(0));
    return true;
  }

  // We have added to subs_query_expression->fields;
  // subs_query_expression->types must always be equal to its visible fields.
  subs_query_expression->types.clear();
  for (Item *item : subq->query_expr()->first_query_block()->visible_fields()) {
    subs_query_expression->types.push_back(item);
  }

  Table_ref *tl;
  if (transform_subquery_to_derived(
          thd, &tl, subs_query_expression, subq,
          // If subquery is top-level in WHERE, and not negated, use INNER JOIN,
          // else use LEFT JOIN.
          // We could use LEFT JOIN unconditionally and let simplify_joins()
          // convert it to INNER JOIN, but the conversion is not perfect, as
          // not all effects of propagate_nullability() are undone.
          /*use_inner_join=*/
          subq->outer_condition_context == enum_condition_context::ANDS &&
              !subq->can_do_aj,
          /*reject_multiple_rows*/ false,
          /*join_condition=*/nullptr,
          /*lifted_where_cond*/ nullptr))
    return true;

  assert(CountVisibleFields(sj_inner_exprs) == sj_inner_exprs.size());
  const int first_sj_inner_expr_of_subquery =
      CountVisibleFields(subs_query_block->fields) - sj_inner_exprs.size();

  Item_field *derived_field;
  // Make the join condition for the derived table:
  Item *join_cond = nullptr;
  // Start at first SJ inner expression in SELECT list:
  int i = first_sj_inner_expr_of_subquery;
  int j = 0;  // counter of processed SJ inner expressions
  for (auto it_outer = sj_outer_exprs.begin(); it_outer != sj_outer_exprs.end();
       ++i, ++j, ++it_outer) {
    Item *outer = *it_outer;
    assert(i < (int)tl->table->s->fields);
    // Using this constructor, instead of the alternative which only takes a
    // Field pointer, gives a persistent name to the item (sets orig_table_name
    // etc) which is necessary for prepared statements.
    derived_field = new (thd->mem_root)
        Item_field(thd, &this->context, tl, tl->table->field[i]);
    if (derived_field == nullptr) return true;
    // The said constructor sets 'fixed' to true, so join_cond->fix_fields()
    // below ignores 'derived_field', so derived_field->cached_table isn't set,
    // making a prepared statement fail. Setting cached_table solves it, and
    // also helps during name resolution because the derived table isn't in the
    // context's name resolution chain.
    // derived_field->cached_table = tl;
    // derived_field->cached_field_index = i;
    Item_bool_func *comp_item;
    Item_func::Functype op_type = sj_decor.op_type_at(j);
    switch (op_type) {
      case Item_func::EQ_FUNC:
        comp_item = new (thd->mem_root) Item_func_eq(outer, derived_field);
        break;
      case Item_func::NE_FUNC:
        comp_item = new (thd->mem_root) Item_func_ne(outer, derived_field);
        break;
      case Item_func::LT_FUNC:
        comp_item = new (thd->mem_root) Item_func_lt(outer, derived_field);
        break;
      case Item_func::LE_FUNC:
        comp_item = new (thd->mem_root) Item_func_le(outer, derived_field);
        break;
      case Item_func::GT_FUNC:
        comp_item = new (thd->mem_root) Item_func_gt(outer, derived_field);
        break;
      case Item_func::GE_FUNC:
        comp_item = new (thd->mem_root) Item_func_ge(outer, derived_field);
        break;
      default:
        assert(false);
        comp_item = nullptr;
    }
    if (comp_item == nullptr) return true;
    // 'outer' moved from the left expression of IN (or from an operator in
    // WHERE, if decorrelated) to this new equality:
    // thd->replace_rollback_place(comp_item->arguments());
    join_cond = and_items(join_cond, comp_item);
  }

  if (join_cond == nullptr)  // it's EXISTS and we couldn't decorrelate anything
    join_cond = new (thd->mem_root) Item_func_true();

  join_cond->apply_is_true();
  if (!join_cond->fixed && join_cond->fix_fields(thd, &join_cond)) return true;
  tl->set_join_cond(join_cond);

  // Make the IS [NOT] NULL condition:
  derived_field = new (thd->mem_root)
      Item_field(thd, &this->context, tl, tl->table->field[0]);
  if (derived_field == nullptr) return true;
  // derived_field->cached_table = tl;
  // derived_field->cached_field_index = 0;

  Item *null_check;
  if (!tl->outer_join)
    null_check = new (thd->mem_root) Item_func_true();
  else if (subq->can_do_aj)
    null_check = new (thd->mem_root) Item_func_isnull(derived_field);
  else
    null_check = new (thd->mem_root) Item_func_isnotnull(derived_field);
  null_check->apply_is_true();
  if (null_check->fix_fields(thd, &null_check)) return true;

  // We only need to test the first column for null-ness:
  // if the NOT NULL test eliminates it, i.e. if it's NULL:
  // - if it's not NULL-complemented: it's a NULL in the right member of the
  // LEFT JOIN, thus in the subquery, thus it wouldn't pass the IN
  // condition,
  // - if it is NULL-complemented: then one IN sub-equality failed, thus it
  // wouldn't pass the IN condition.
  // Reciprocically: if the NOT NULL does not eliminate it: it's not
  // NULL-complemented, so all IN sub-equalities passed, it would pass the IN
  // condition.
  // If the subquery was rather with EXISTS, the SELECT list's first
  // expression is 1, so if it's NULL it's surely NULL-complemented; if there
  // were decorrelated equalities one of them failed, or the inner table
  // was empty.

  // Walk the parent query's WHERE, to find the subquery item, and replace it.
  if (replace_subcondition(thd, &m_where_cond, subq, null_check, false))
    return true; /* purecov: inspected */

  // WHERE now references the derived table's column, so used_tables needs an
  // update; so does not_null_tables (by making it up to date, we allow
  // simplify_joins() to optimize more).
  m_where_cond->update_used_tables();
  return false;
}

/**
  Create a new Table_ref object for this query block, for either:
  1) a derived table which will replace the subquery, or
  2) an extra derived table for handling grouping, if necessary,
     cf. transform_grouped_to_derived.

  The derived table is added to the list of used tables for the query block
  ("outer").

  @param     thd        the session context
  @param     unit       the query expression for subquery (case 1), or a new
                        query expression for (case 2)
  @param     join_cond  != nullptr: we are  synthesizing a derived table for a
                        subquery within this join condition
                        = nullptr: synthesizing a derived table for a subquery
                        where the subquery is not contained in a join condition
  @param     left_outer true for case (1), false for (2)
  @param     use_inner_join for case (1): if true/false use INNER/LEFT JOIN
  @returns the derived table object, or nullptr on error.
*/
Table_ref *Query_block::synthesize_derived(THD *thd, Query_expression *unit,
                                           Item *join_cond, bool left_outer,
                                           bool use_inner_join) {
  char name[STRING_BUFFER_USUAL_SIZE];
  const uint i = unit->first_query_block()->select_number;
  std::snprintf(name, sizeof(name), "derived_%d_%d", select_number, i);
  char *namep = thd->mem_strdup(name);
  if (namep == nullptr) return nullptr;

  auto *const ti = new (thd->mem_root) Table_ident(unit);
  if (ti == nullptr) return nullptr;

  Table_ref *derived_table =
      add_table_to_list(thd, ti, namep, 0, TL_READ, MDL_SHARED_READ);
  if (derived_table == nullptr) return nullptr;

  if (left_outer) {
    derived_table->outer_join = !use_inner_join;
    if (!unit->item->is_bool_func())
      derived_table->m_was_scalar_subquery = true;

    if (join_cond != nullptr) {
      // impossible if table subquery:
      assert(derived_table->m_was_scalar_subquery);
      if (nest_derived(thd, join_cond, m_current_table_nest, derived_table))
        return nullptr;
    } else {
      // The derived table is not for a subquery in a join condition
      if (add_joined_table(derived_table)) return nullptr;
      if (nest_last_join(thd) == nullptr) return nullptr;
    }
    if (derived_table->m_was_scalar_subquery) {
      auto *const join_cond_true = new (thd->mem_root) Item_func_true();
      if (join_cond_true == nullptr) return nullptr;
      derived_table->set_join_cond(join_cond_true);
    }  // else: table subquery, the join condition is complex, made by caller.
  }

  unit->derived_table = derived_table;
  return derived_table;
}

/**
  A minion of transform_grouped_to_derived.

  Replace occurrences of the aggregate function identified in info.m_target with
  the the field info.m_replacement in the expressions contained in list.
  Note that since this is part of a permanent transformation, we use the extra
  m_permanent_transform flag in the THD

  @param info  a tuple containing {aggregate, replacement field}
  @param was_hidden true if the aggregate was originally hidden
  @param list  the list of expressions
  @param ref_item_array to be kept in sync with any changes in 'list'

  @returns true on error (can not happen currently unless replacement field is
                          empty)
*/
static bool replace_aggregate_in_list(Item::Aggregate_replacement &info,
                                      bool was_hidden,
                                      mem_root_deque<Item *> *list,
                                      Ref_item_array *ref_item_array) {
  for (auto lii = list->begin(); lii != list->end(); ++lii) {
    Item *select_expr = *lii;
    Item *const new_item = select_expr->transform(&Item::replace_aggregate,
                                                  pointer_cast<uchar *>(&info));
    if (new_item == nullptr) return true;
    new_item->update_used_tables();
    if (new_item != select_expr) {
      new_item->hidden = was_hidden;
      new_item->increment_ref_count();
      *lii = new_item;
      for (size_t i = 0; i < list->size(); i++) {
        if ((*ref_item_array)[i] == select_expr)
          (*ref_item_array)[i] = new_item;
      }
    }
  }
  return false;
}

/**
  A minion of transform_grouped_to_derived.

  "Remove" any non-window aggregate functions from fields unconditionally.
  If such an aggregate is found, the query block should have a HAVING clause.
  This is asserted in debug mode. We "remove" them by replacing them with
  an Item_int, which should have no adverse effects. This avoids creating
  trouble for Query_block::add_hidden_item which would otherwise need to keep
  track of removed items.

  @param thd      session context
  @param select   the query block whose aggregates are being moved into a
                  derived table
  @returns true on error, else false
*/
bool Query_block::remove_aggregates(THD *thd,
                                    [[maybe_unused]] Query_block *select) {
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    Item *select_expr = *it;
    if (!select_expr->m_is_window_function &&
        select_expr->type() == Item::SUM_FUNC_ITEM) {
      // must be an aggregate induced from a HAVING clause, remove from
      // transformed query block since it is not needed on that
      // level any more
      assert(select->having_cond() != nullptr);
      Item *int_item = new (thd->mem_root) Item_int(0);
      int_item->hidden = select_expr->hidden;
      if (int_item == nullptr) return true;
      *it = int_item;
      for (size_t i = 0; i < fields.size(); i++) {
        if (base_ref_items[i] == select_expr) base_ref_items[i] = int_item;
      }
    }
  }
  return false;
}

/**
  A minion of transform_grouped_to_derived.

  This updates the name resolution contexts in expr to that of new_derived
  permanently.

  @param  expr        the expression to be updated
  @param  new_derived the query block of the new derived table which now holds
                      the expression after it has been moved down.

  @returns true on error
*/
static bool update_context_to_derived(Item *expr, Query_block *new_derived) {
  Item_ident::Change_context ctx(&new_derived->context);
  if (expr != nullptr && expr->walk(&Item::change_context_processor,
                                    enum_walk::POSTFIX, (uchar *)&ctx))
    return true; /* purecov: inspected */
  return false;
}

/**
  A minion of transform_grouped_to_derived.

  Collect a unique list of aggregate functions used in the transformed query
  block, which will need to be replaced with fields from the derived table
  containing the grouping during transform_grouped_to_derived.

  @param[in]       select     the query block
  @param[in, out]  aggregates the accumulator which will contain the aggregates
  @return true on error
*/
static bool collect_aggregates(
    Query_block *select, Item_sum::Collect_grouped_aggregate_info *aggregates) {
  for (Item *select_expr : select->visible_fields()) {
    if (select_expr->walk(&Item::collect_grouped_aggregates,
                          enum_walk::SUBQUERY_PREFIX,
                          pointer_cast<uchar *>(aggregates)))
      return true; /* purecov: inspected */
  }

  if (select->having_cond() != nullptr) {
    if (select->having_cond()->walk(&Item::collect_grouped_aggregates,
                                    enum_walk::SUBQUERY_PREFIX,
                                    pointer_cast<uchar *>(aggregates)))
      return true; /* purecov: inspected */
  }
  // We move the aggregate functions from an implicitly grouped query block to
  // a new derived table, effectively making the existing query block
  // non-grouped. When the grouping is implicit, the ORDER BY is eliminated
  // since the result set has only one row, so skip processing of the
  // order_list.
  assert(select->order_list.elements == 0);

  List_iterator<Window> li(select->m_windows);
  for (Window *w = li++; w != nullptr; w = li++) {
    for (ORDER *it : {w->first_order_by(), w->first_partition_by()}) {
      if (it != nullptr) {
        for (auto ord = it; ord != nullptr; ord = ord->next) {
          if ((*ord->item)
                  ->walk(&Item::collect_grouped_aggregates, enum_walk::PREFIX,
                         pointer_cast<uchar *>(aggregates)))
            return true; /* purecov: inspected */
        }
      }
    }
  }
  return false;
}

/**
  Helper function to make names for columns of a derived table replacing a
  scalar or table subquery.

  Fields from the query block containing the scalar subquery are moved
  to the new derived table. We give them synthetic unique names here.

  @param thd      current session context
  @param item     the item we want to name
  @param field_no the field number
  @returns true on error
*/
static bool baptize_item(THD *thd, Item *item, int *field_no) {
  char buff[100];
  std::snprintf(buff, sizeof(buff), SYNTHETIC_FIELD_NAME "%d", (*field_no)++);
  char *namep = thd->mem_strdup(buff);
  if (namep == nullptr) return true;
  item->orig_name.set(item->item_name.ptr());
  item->item_name.set(namep);
  return false;
}

/**
  Minion of \c transform_grouped_to_derived.  Do a replacement in \c expr
  using \c Item::transform as specified in \c info using \c transformer.
 */
bool Query_block::replace_item_in_expression(Item **expr, bool was_hidden,
                                             Item::Item_replacement *info,
                                             Item_transformer transformer) {
  Item *new_item = (*expr)->transform(transformer, pointer_cast<uchar *>(info));
  if (new_item == nullptr) return true;
  new_item->update_used_tables();
  if (new_item != *expr) {
    // Save our original item name at this level
    auto saved_item_name =
        (*expr)->orig_name.is_set() ? (*expr)->orig_name : (*expr)->item_name;
    replace_referenced_item(*expr, new_item);
    // Replace in fields
    const auto it = find(fields.begin(), fields.end(), new_item);
    if (it == fields.end()) {
      *expr = new_item;
    } else {
      // More than one occurrence of same replaced field, make another copy so
      // we do not clobber the item_name (alias) of another occurrence in select
      // list.
      Item_field *f = down_cast<Item_field *>(new_item);
      Item_field *cpy = new (parent_lex->thd->mem_root) Item_field(f->field);
      if (cpy == nullptr) return true;
      *expr = cpy;
    }

    // Mark this expression as hidden if it was hidden in this query
    // block.
    (*expr)->hidden = was_hidden;
    (*expr)->item_name = saved_item_name;
  }
  return false;
}

/**
  Minion of \c transform_scalar_subqueries_to_join_with_derived. Moves implicit
  grouping down into a derived table to prepare for
  \c transform_scalar_subqueries_to_join_with_derived.

  Example:

  @verbatim

    SELECT ( SELECT COUNT(*)
             FROM t1 ) AS tot,
           IFNULL(MAX(t2.b), 0) + 6 AS mx  # MAX(t2.b), FROM t2 and
    FROM t2                                # WHERE go to derived_1_3
    WHERE ANY_VALUE(t2.b) = 10;

  is transformed to

    SELECT ( SELECT COUNT(*)
             FROM t1 ) AS tot,
           IFNULL(derived_1_3.`MAX(t2.b)`, 0) + 6 AS mx
    FROM ( SELECT MAX(t2.b) AS `MAX(t2.b)`
           FROM t2
           WHERE (ANY_VALUE(t2.b) = 10)) derived_1_3

  @endverbatim

  Create a new query expression object and query block object to represent the
  contents of a derived table ("new_derived" in the code below, "derived_1_3"
  in the example above), with a select list which only contains the aggregate
  functions lifted out of the transformed query block ("MAX(t2.b) AS
  `MAX(t2.b)`" above) and any fields referenced.

  The transformed query block retains the original select list except aggregates
  and fields are replaced by fields ("derived_1_3.`MAX(t2.b)`") from the
  new subquery, but it loses its FROM list, replaced by the new derived table
  ("derived_1_3" above) and its WHERE and HAVING clauses which all go to
  the derived table's query block.

  Any DISTINCT, WINDOW clauses and LIMITs stay in place at the transformed
  query block.

  @param      thd        session context
  @param[out] break_off  set to true of transformation could not be performed
  @returns               true on error
*/
bool Query_block::transform_grouped_to_derived(THD *thd, bool *break_off) {
  // Collect all aggregates, and add them to our new select list
  Item_sum::Collect_grouped_aggregate_info aggregates(this);

  if (collect_aggregates(this, &aggregates)) return true;
  if (aggregates.m_break_off) {
    *break_off = true;  // some aggregates functions aggregate in an outer query
    return false;
  } else if (aggregates.list.size() == 0) {
    // No longer to be found, probably optimized away ORDER BY
    return false;
  }

  // Remember implicit grouping in case this query is also a scalar subquery
  // so we can still identify it after this transform.
  assert(is_implicitly_grouped());
  m_was_implicitly_grouped = true;

  Table_ref *tl = nullptr;
  Query_block *new_derived = nullptr;
  List<Item> item_fields_or_view_refs;
  Mem_root_array<Item_view_ref *> unique_view_refs(thd->mem_root);
  mem_root_unordered_map<Field *, Item_field *> unique_fields(thd->mem_root);
  mem_root_unordered_map<Field *, Item_field *> unique_default_values(
      thd->mem_root);
  mem_root_unordered_map<Field *, Item_field *> *field_classes[] = {
      &unique_default_values, &unique_fields};

  /*
    In addition to adding the aggregates to the derived table's SELECT list,
    we need to add all referenced fields that will be needed in this query
    block.
    They fall into three categories:

    1) fields referenced directly in the select list
    2) fields referenced by window functions as arguments, or in
       in a window definition's ORDER BY or PARTITION BY clauses
    3) fields referenced by the transformed query block's ORDER BY clause

    All of these can reference items from tables that are now moved inside the
    derived table.

    This query block will get its fields replaced by the corresponding ones in
    the derived table shortly, after we have resolved the derived table.  We
    need to give them unique names in the derived table, else we could have
    issues with resolution. Can probably be removed after WL#6570.

    Method: collect all unique fields referenced in categories 1-3 above.
    Add them with unique names to the SELECT list of the derived table,
    after the aggregates (e.g. inside the derived table one may see t1.i and
    t2.i, but at this level both fields are part of the same derived table,
    so they cannot both be known as i in this query block).

    When the fields in the derived table are known (after the call to
    resolve_placeholder_tables below, we can go back and modify the references
    at this level.
  */
  mem_root_unordered_map<Item **, bool> contrib_exprs(thd->mem_root);

  // We want permanent changes
  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    Query_expression *const old_slave = slave;
    slave = nullptr;
    // The new derived table takes over WHERE and HAVING from this query block
    Query_expression *new_slu = parent_lex->create_query_expr_and_block(
        thd, this, m_where_cond, m_having_cond, CTX_DERIVED);
    if (new_slu == nullptr) return true;
    new_derived = new_slu->first_query_block();

    m_where_cond = nullptr;
    m_having_cond = nullptr;
    new_derived->linkage = DERIVED_TABLE_TYPE;

    // inherit item counts for safe allocation of base_ref_items array
    new_derived->select_n_having_items = select_n_having_items;
    new_derived->select_n_where_fields = select_n_where_fields;
    new_derived->n_sum_items = n_sum_items;
    new_derived->n_child_sum_items = n_child_sum_items;
    // update condition counts
    new_derived->cond_count = cond_count;
    // between_count is updated if cond_count gets updated when there are any
    // transformations. So we do the same here too. However it needs to be
    // investigated if this is necessary or not.
    new_derived->between_count = between_count;

    with_sum_func = false;

    // Any moved Item_ident needs new name resolution context
    Item *conds[2] = {new_derived->m_where_cond, new_derived->m_having_cond};
    for (auto cond : conds) {
      if (update_context_to_derived(cond, new_derived)) return true;
    }

    assert(join == nullptr);

    // Move FROM tables under the new derived table with fix ups
    new_derived->m_table_list = m_table_list;
    m_table_list.clear();
    for (Table_ref *tables = new_derived->get_table_list(); tables != nullptr;
         tables = tables->next_local) {
      tables->query_block = new_derived;  // update query block context
      if (update_context_to_derived(tables->join_cond(), new_derived))
        return true; /* purecov: inspected */
    }

    new_derived->derived_table_count = this->derived_table_count;
    derived_table_count = 0;  // will soon become 1.

    assert(is_implicitly_grouped());  // only implicit grouping moved
    assert(group_list.elements == 0);
    assert(olap == UNSPECIFIED_OLAP_TYPE);

    // Let new derived take over grouping flags
    new_derived->m_agg_func_used = m_agg_func_used;
    m_agg_func_used = false;
    new_derived->m_json_agg_func_used = m_json_agg_func_used;
    m_json_agg_func_used = false;

    // Let new derived take over any semijoin candidates
    new_derived->sj_candidates = sj_candidates;
    sj_candidates = nullptr;

    assert(m_current_table_nest == &m_table_nest);
    new_derived->m_table_nest = std::move(m_table_nest);
    m_table_nest.clear();
    new_derived->m_current_table_nest = &new_derived->m_table_nest;
    new_derived->leaf_tables = leaf_tables;
    new_derived->leaf_table_count = leaf_table_count;
    leaf_tables = nullptr;
    leaf_table_count = 0;
    // Add the derived table to this query block's FROM list
    tl = synthesize_derived(thd, new_slu, nullptr, false, false);
    if (tl == nullptr) return true;

    if (!(tl->derived_result = new (thd->mem_root) Query_result_union()))
      return true; /* purecov: inspected */
    new_slu->set_query_result(tl->derived_result);

    m_table_nest.push_back(tl);

    // Update this query block's and the derived table's query block's name
    // resolution contexts
    context.table_list = tl;
    context.first_name_resolution_table = tl;
    assert(context.last_name_resolution_table == nullptr);
    new_derived->context.init();
    new_derived->context.table_list = get_table_list();
    new_derived->context.query_block = new_derived;
    new_derived->context.outer_context = &context;
    new_derived->context.first_name_resolution_table = get_table_list();

    /*
      Retain only subqueries from SELECT list in this block [2]; all other
      query expressions go to the new derived table [1]:
    */
    Item_subselect::Collect_subq_info subqueries(this);
    for (Item *item : fields) {
      if (item->walk(&Item::collect_subqueries, enum_walk::PREFIX,
                     pointer_cast<uchar *>(&subqueries)))
        return true; /* purecov: inspected */
    }

    assert(slave != nullptr);
    assert(new_derived->slave == nullptr);

    // Collect all query expressions in a container first, since we cannot rely
    // on old_slave's ::next pointer chain once we start inserting them.
    Mem_root_array<Query_expression *> old_slaves(thd->mem_root);
    for (Query_expression *cand = old_slave; cand != nullptr;
         cand = cand->next) {
      old_slaves.push_back(cand);
    }

    for (auto cand : old_slaves) {
      if (cand == new_slu) continue;  // already in place
      if (subqueries.contains(cand))
        cand->include_down(parent_lex, this);  // [2]
      else {
        cand->include_down(parent_lex, new_derived);  // [1]
        // These subqueries are now moving into a new query block, so we need
        // to update any outer references inside such subqueries from this block
        // to that of the new derived table.
        Item_ident::Depended_change info{this, new_derived};
        if (cand->walk(&Item::update_depended_from, enum_walk::SUBQUERY_PREFIX,
                       pointer_cast<uchar *>(&info)))
          return true; /* purecov: inspected */
      }
    }

    // Insert the aggregates in the derived table's query block
    int i = 0;
    for (Item_sum *agg : aggregates.list) {
      assert(agg->aggr_query_block == agg->base_query_block);
      agg->aggr_query_block = new_derived;
      agg->base_query_block = new_derived;
      if (agg->hidden) {
        // Because 'agg' is going to move to the derived table's SELECT list,
        // its 'hidden' flag will become true. Then, in the current query block,
        // 'agg' will be replaced by an Item_field for the column of that
        // derived table; such Item_field must have the original value of
        // agg->hidden, which we thus save here:
        aggregates.aggregates_that_were_hidden.insert(agg);
      }
      if (new_derived->add_item_to_list(agg)) return true;
      if (agg->item_name.length() == 0) {
        // Generate a name (required)
        char buff[100];
        std::snprintf(buff, sizeof(buff), "tmp_aggr_%d", ++i);
        agg->item_name.copy(buff);
        if (agg->item_name.length() == 0) return true;  // allocation error.
      }
    }

    // We will find all fields mentioned above by checking fields, which
    // has any hidden fields induced by ORDER BY or window specifications, in
    // addition to fields from the select expressions. We also make a note
    // of the expression's hidden status to mark the expression as hidden
    // when it is replaced with derived table expression later.
    for (Item *&item : fields) {
      contrib_exprs.emplace(&item, item->hidden);
    }

    // Collect fields in expr, but not from inside grouped aggregates.
    Item::Collect_item_fields_or_view_refs info{&item_fields_or_view_refs,
                                                this};
    for (auto expr : contrib_exprs) {
      if ((*expr.first)
              ->walk(&Item::collect_item_field_or_view_ref_processor,
                     enum_walk::SUBQUERY_PREFIX | enum_walk::POSTFIX,
                     pointer_cast<uchar *>(&info)))
        return true; /* purecov: inspected */
    }

    List_iterator<Item> lfi(item_fields_or_view_refs);
    Item *lf;

    // Remove irrelevant field references, i.e. those fields that are not local
    // to new_derived
    while ((lf = lfi++)) {
      if (lf->type() == Item::FIELD_ITEM) {
        Item_field *f = down_cast<Item_field *>(lf);
        if (!(f->context->query_block == this || f->depended_from == this))
          lfi.remove();
      }
    }
    // We now have all fields, default values and view references; now find only
    // unique ones.
    lfi.init(item_fields_or_view_refs);
    while ((lf = lfi++)) {
      if (lf->type() == Item::FIELD_ITEM) {
        Item_field *f = down_cast<Item_field *>(lf);
        if (unique_fields.find(f->field) == unique_fields.end()) {
          unique_fields.emplace(std::pair<Field *, Item_field *>(f->field, f));
        } else {
          // Should already have been deduplicated during collection
          assert(false);
        }
      } else if (lf->type() == Item::DEFAULT_VALUE_ITEM) {
        Item_default_value *dv = down_cast<Item_default_value *>(lf);
        Item_field *lf_field =
            down_cast<Item_field *>(dv->argument()->real_item());
        if (unique_default_values.find(lf_field->field) ==
            unique_default_values.end()) {
          unique_default_values.emplace(
              std::pair<Field *, Item_field *>(lf_field->field, dv));
        } else {
          // Should already have been deduplicated during collection
          assert(false);
        }
      } else {
        Item_view_ref *vr = down_cast<Item_view_ref *>(lf);
        for (auto curr : unique_view_refs) {
          if (curr->eq(vr, true)) goto continue_outer;
        }
        unique_view_refs.push_back(vr);
      }
    continue_outer:;
    }

    int field_no = 1;

    for (auto vr : unique_view_refs) {
      if (baptize_item(thd, vr, &field_no)) return true;
      if (new_derived->add_item_to_list(vr)) return true;
      if (update_context_to_derived(vr, new_derived)) return true;
      vr->depended_from = nullptr;
    }

    for (auto field_class : field_classes) {
      for (auto pair : *field_class) {
        Item_field *f = pair.second;
        Item *sl_item = f;
        if (f->type() == Item::FIELD_ITEM && f->protected_by_any_value()) {
          // The field was mentioned only ever inside arguments to ANY_VALUE, so
          // protect it likewise in new_derived, lest we get a
          // ER_MIX_OF_GROUP_FUNC_AND_FIELDS_V2. If not, we let the check
          // proceed, i.e. we do not add ANY_VALUE for the column.
          sl_item = new (thd->mem_root) Item_func_any_value(f);
          if (sl_item == nullptr) return true;
          if (sl_item->fix_fields(thd, &sl_item)) return true;
        }
        if (new_derived->add_item_to_list(sl_item)) return true;
        if (baptize_item(thd, sl_item, &field_no)) return true;
        if (update_context_to_derived(sl_item, new_derived)) return true;
        f->depended_from = nullptr;
      }
    }

    if (new_derived->has_sj_candidates() &&
        new_derived->flatten_subqueries(thd))
      return true;

    if (setup_tables(thd, get_table_list(), false)) return true;
  }  // Prepared_stmt_arena_holder scope

  // Resolving the new derived table needs normal arena
  if (resolve_placeholder_tables(thd, true)) return true;

  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    assert(tl->table != nullptr);

    /*
      We pushed the HAVING clause into new_derived above, but it is resolved to
      this query block, meaning it may have Item_aggregate_refs pointing into
      this->base_ref_items. We need to update such references to point into
      new_derived->base_ref_items instead, since this is where the aggregates
      are now also. We do this by adding them as hidden items and setting
      the Item_aggregate_refs::ref accordingly.
    */
    if (new_derived->m_having_cond != nullptr) {
      Item_sum::Collect_grouped_aggregate_info having_aggs(this);
      if (new_derived->m_having_cond->walk(&Item::collect_grouped_aggregates,
                                           enum_walk::PREFIX,
                                           pointer_cast<uchar *>(&having_aggs)))
        return true; /* purecov: inspected */

      for (Item_sum *agg : having_aggs.list) {
        Item::Aggregate_ref_update info(agg, new_derived);
        [[maybe_unused]] bool error = new_derived->m_having_cond->walk(
            &Item::update_aggr_refs, enum_walk::PREFIX,
            pointer_cast<uchar *>(&info));
        assert(!error);
        agg->aggr_query_block = new_derived;
      }
    }

    /*
      Permanently replace the aggregates in this select list and windowing
      clauses with fields from the derived table.
    */
    Field **field_ptr = tl->table->field;
    for (Item_sum *agg : aggregates.list) {
      Item_field *replaces_agg = new (thd->mem_root) Item_field(*field_ptr);
      if (replaces_agg == nullptr) return true;

      // So we can re-bind this field in EXECUTE phase of prepared statement
      // Remove after WL#6570.
      // replaces_agg->set_orig_names();

      /*
        The WHERE condition cannot contain group function from this level, so
        ignore. Only replace aggregates from the SELECT lists with fields from
        the derived table, then remove aggregates from top select lists.
      */
      Item::Aggregate_replacement info(agg, replaces_agg);
      if (replace_aggregate_in_list(
              info, aggregates.aggregates_that_were_hidden.count(agg) != 0,
              &fields, &base_ref_items))
        return true;

      // We only transform implicit grouping to a derived table: in such a case,
      // the order by is eliminated since the result set has only one row, so
      // skip processing of order_list.
      assert(group_list.elements == 0);
      assert(order_list.elements == 0);

      List_iterator<Window> wli(m_windows);
      for (Window *w = wli++; w != nullptr; w = wli++) {
        for (ORDER *it : {w->first_order_by(), w->first_partition_by()}) {
          if (it != nullptr) {
            for (auto ord = it; ord != nullptr; ord = ord->next) {
              Item *new_item;
              if (!(new_item = (*ord->item)
                                   ->transform(&Item::replace_aggregate,
                                               pointer_cast<uchar *>(&info))))
                return true; /* purecov: inspected */
              new_item->update_used_tables();
              if (new_item != *ord->item) {
                *ord->item = new_item;
              }
            }
          }
        }
        // Physical sorting order should not have been set up since we are
        // implicitly grouped, so no need to attempt substitution in it.
        assert(w->sorting_order(nullptr, false) == nullptr);
      }

      // Aggregate argument may contain identifiers that need correct
      // context. View references will have been replaced Item_fields,
      // so we have to be careful: these will be rolled back and to make
      // our transformation permanent we need to update the context of the
      // original Item_fields, not the Item_view_refs.
      if (update_context_to_derived(agg, new_derived)) return true;

      ++field_ptr;
    }

    /*
      Remove any moved aggregates from top query block that did not get
      replaced above.
    */
    if (remove_aggregates(thd, new_derived)) return true;

    // field_ptr now points to the first of any view references added to the
    // select list of the derived table's query block. We now create new fields
    // for this block which will point to the corresponding item in the derived
    // table and then we substitute the new fields for the view refs.
    for (auto vr : unique_view_refs) {
      for (const auto &[expr, was_hidden] : contrib_exprs) {
        Item::Item_view_ref_replacement info(vr->real_item(), *field_ptr, this);
        if (replace_item_in_expression(expr, was_hidden, &info,
                                       &Item::replace_item_view_ref))
          return true;
      }
      ++field_ptr;
    }
    for (auto field_class : field_classes) {
      // field_ptr now points to the first of the fields added to the select
      // list of the derived table's query block. We now create new fields for
      // this block which will point to the corresponding fields moved to the
      // derived table and then we substitute the new fields for the old ones.
      for (auto pair : *field_class) {
        auto replaces_field = new (thd->mem_root) Item_field(*field_ptr);
        if (replaces_field == nullptr) return true;

        // We can update context of the field moved into the derived table
        // now that replaces_field has inherited the upper context
        pair.second->context = &new_derived->context;

        replaces_field->increment_ref_count();

        for (const auto &[expr, was_hidden] : contrib_exprs) {
          Item_field *replacement = replaces_field;
          // If this expression was hidden, we need to make a copy of the
          // derived table field. The same derived table field cannot be marked
          // both hidden and visible if the field replaces two different
          // expressions in the transforming query block.
          if (was_hidden) {
            auto hidden_field = new (thd->mem_root) Item_field(*field_ptr);
            if (hidden_field == nullptr) return true;
            hidden_field->item_name.set(pair.second->orig_name.ptr());
            pair.second->context = &new_derived->context;
            replacement = hidden_field;
          }
          Item::Item_field_replacement info(
              pair.first, replacement, this,
              field_class == &unique_default_values
                  ? Item::Item_field_replacement::Mode::DEFAULT_VALUE
                  : Item::Item_field_replacement::Mode::FIELD);
          if (replace_item_in_expression(expr, was_hidden, &info,
                                         &Item::replace_item_field))
            return true;
        }
        ++field_ptr;
      }
    }

    OPT_TRACE_TRANSFORM(&thd->opt_trace, trace_wrapper, trace_object,
                        select_number, "grouped subquery",
                        "subquery over grouped derived table");
    opt_trace_print_expanded_query(thd, this, &trace_object);
  }  // Prepared_stmt_arena_holder scope
  return false;
}

/**
  A minion of transform_scalar_subqueries_to_join_with_derived.

  A transform creates a field representing the value of the derived table and
  adds it as a hidden field to the select list.  Next, it replaces the subquery
  in the item tree with this field.  If we replace in a HAVING condition, we
  build an Item_ref, cf. PTI_simple_ident_ident::itemize which also creates a
  Item_ref for a field reference in HAVING, because we may need to access the
  field in a tmp table.

  @param      thd       The session context
  @param      subquery  The scalar subquery
  @param      tr        The table reference for the derived table
  @param      expr      The expression we are replacing (in)
*/
bool Query_block::replace_subquery_in_expr(THD *thd, Item::Css_info *subquery,
                                           Table_ref *tr, Item **expr) {
  if (!(*expr)->has_subquery()) return false;

  Item_singlerow_subselect::Scalar_subquery_replacement info(
      subquery->item,
      // make sure to not replace with one of the hidden fields, if present,
      // e.g. for INTERSECT:
      tr->table->field[tr->table->hidden_field_count], this,
      subquery->m_add_coalesce);

  // ROLLUP wrappers might have been added to the expression at this point. Take
  // care to transform the inner item and keep the rollup wrappers as is.
  bool with_rollup_wrapper = is_rollup_group_wrapper(*expr);
  Item *orig_unwrapped_item = unwrap_rollup_group(*expr);
  Item *new_item = (*expr)->transform(&Item::replace_scalar_subquery,
                                      pointer_cast<uchar *>(&info));
  if (new_item == nullptr) return true;

  // If we replaced an item contained in the transformed query block,
  // retain its name so the metadata column name remains correct.
  if (*expr != new_item) {
    new_item->item_name.set((*expr)->item_name.ptr());
    *expr = new_item;
  } else if (with_rollup_wrapper) {
    // If the original expression was a rollup group item, the inner item of the
    // expression might have changed.
    Item *new_unwrapped_item = unwrap_rollup_group(new_item);
    if (new_unwrapped_item != orig_unwrapped_item)
      new_unwrapped_item->item_name.set((*expr)->item_name.ptr());
  }

  new_item->update_used_tables();

  // If this expression has aggregation and we have replaced a subquery
  // with a field, we need to recompute split_sum_func
  if ((new_item->has_aggregation() &&
       !(new_item->type() == Item::SUM_FUNC_ITEM &&
         !new_item->m_is_window_function)) ||  // (1)
      new_item->has_wf()) {                    // (2)
    if (new_item->split_sum_func(thd, base_ref_items, &fields)) {
      return true;
    }
  }
  assert(!thd->is_error());
  return false;
}

/**
  A minion of transform_scalar_subqueries_to_join_with_derived.

  Determine if the query expression is directly contained in the
  query block, i.e. it is a subquery.

  @param select  the query block
  @param slu     the query expression

  @returns true if slu is directly contained in select, else false
*/
static bool query_block_contains_subquery(Query_block *select,
                                          Query_expression *slu) {
  for (Query_expression *cand = select->first_inner_query_expression();
       cand != nullptr; cand = cand->next_query_expression()) {
    if (cand == slu) return true;
  }
  return false;
}

static bool walk_join_conditions(mem_root_deque<Table_ref *> &list,
                                 std::function<bool(Item **expr_p)> action,
                                 Item::Collect_scalar_subquery_info *info) {
  for (Table_ref *tl : list) {
    if (tl->join_cond() != nullptr) {
      info->m_join_condition_context = tl->join_cond();
      if (action(tl->join_cond_ref())) return true;
    }
    if (tl->nested_join != nullptr &&
        walk_join_conditions(tl->nested_join->m_tables, action, info))
      return true; /* purecov: inspected */
  }
  info->m_join_condition_context = nullptr;
  return false;
}

/**
 Remember if this transform was performed. It it was done by a secondary
 engine, it may need to be rolled back before falling back on primary engine
 execution.
 */
static void remember_transform(THD *thd, Query_block *select) {
  if (!thd->optimizer_switch_flag(OPTIMIZER_SWITCH_SUBQUERY_TO_DERIVED)) {
    // Transform was enabled not by switch, but by secondary enginee
    select->parent_lex->m_sql_cmd->set_optional_transform_prepared(true);
  }
}

/**
  Push the generated derived table to the correct location inside a join nest.
  It will be nested in a new nest along with the outer table to the join
  which owns the search condition in which we found the scalar subquery.
  For example:

      select t1.i,
             t2.i
      from t1
           left outer join
           t2 on
           (t1.i < (select max(t2.i) from t2));

      in transformed to

      select t1.i,
             t2.i
      from t1
           left join
           (select max(t2.i) AS `max(t2.i)` from t2) derived_1_0   [*]
           on(true)
           left join
           t2
           on((t1.i < derived_1_0.`max(t2.i)`))

  [*]: the derived table is nested in here, just ahead of the inner table
       t2 to which the join condition is attached.

  In the original join nest before transformation may look like this
  (the join order list is reversed relative to the logical order):

   (nest_join)
      t2  LEFT OUTER        ON .. = ..       (inner table)
      t1                                     (outer table)

   After the transformation we have this nest structure:

   (nest_join)
      t2 LEFT OUTER         ON  .. = ..
      (nest_last_join)
         derived_1_0 LEFT OUTER ON true
         t1

  The method will recursively inspect and rebuild join nests as needed since
  the join with the condition may be deeply nested.

  @param   thd           the session context
  @param   join_cond     the join condition which identifies the join we want to
                         nest into
  @param   nested_join_list
                         the join list at the current nesting level
  @param   derived_table the table we want to nest

  @returns true on error
*/
bool Query_block::nest_derived(THD *thd, Item *join_cond,
                               mem_root_deque<Table_ref *> *nested_join_list,
                               Table_ref *derived_table) {
  // Locate join nest in which the joinee with the condition sits
  const bool found [[maybe_unused]] = walk_join_list(
      *nested_join_list,
      [join_cond, &nested_join_list](Table_ref *tr) mutable -> bool {
        if (tr->join_cond() == join_cond) {
          nested_join_list = &tr->embedding->nested_join->m_tables;
          return true;  // break off walk
        }
        return false;
      });

  assert(found);

  // Make a copy of the join list, outer before inner joinees, so we
  // can rebuild the join_list after inserting the derived table in a nest
  // with the outer(s)
  mem_root_deque<Table_ref *> copy_list(*THR_MALLOC);
  auto &jlist = *nested_join_list;
  for (auto tl : jlist) copy_list.push_front(tl);
  jlist.clear();

  auto it = std::find_if(copy_list.begin(), copy_list.end(),
                         [join_cond](Table_ref *tl) -> bool {
                           return tl->join_cond() == join_cond;
                         });
  assert(it != copy_list.end());  // assert that we found it
  const size_t idx = it - copy_list.begin();

  // Insert back all outer tables to the inner containing the condition.
  // Normally only one.
  for (size_t i = 0; i < idx; i++) {
    jlist.push_front(copy_list[i]);
  }

  // Insert the derived table and nest it with the outer(s)
  jlist.push_front(derived_table);
  derived_table->join_list = &jlist;
  derived_table->embedding = copy_list[idx]->embedding;

  if (nest_join(thd, this, copy_list[idx]->embedding, &jlist, idx + 1,
                "(nest_join)") == nullptr)
    return true;

  // Insert back the inner containing the JOIN condition and any subsequent
  // joinees
  for (size_t i = idx; i < copy_list.size(); i++) {
    jlist.push_front(copy_list[i]);
  }

  return false;
}

/**
  Helper singleton class used to track information needed to perform the
  transform of a correlated scalar subquery in a derived table, as performed
  by \c decorrelate_derived_scalar_subquery_pre and
  \c decorrelate_derived_scalar_subquery_pre.
*/
struct Lifted_expressions_map {
  ///< list of fields in WHERE clauses eligible for lifting
  List<Item> m_inner_fields;
  ///< list of expressions that are not simple fields in WHERE clauses eligible
  ///< for lifting.
  List<Item> m_inner_func_calls;
  // Positions in derived table of corresponding field, indexed by the fields's
  // position in m_inner_fields
  Mem_root_array<uint> m_field_positions;
  // Positions in derived table of corresponding expression (function call)
  // indexed by call's position in m_inner_func_calls
  Mem_root_array<uint> m_func_call_positions;
  ///< The list of outer fields of the WHERE clauses eligible
  List<Item> m_outer_fields;
  explicit Lifted_expressions_map(MEM_ROOT *root)
      : m_field_positions(root), m_func_call_positions(root) {}
};

/**
  Given an expression, create an ORDER expression for that expression and add
  it to a window's ORDER BY list in preparation for synthesizing a window
  function, cf. \c setup_counts_over_partitions
 */
static bool add_partition_by_expr(THD *thd, PT_order_list *partition,
                                  Query_block *qb, Item *expr) {
  ORDER *o = new (thd->mem_root) PT_order_expr(POS(), expr, ORDER_ASC);
  if (o == nullptr) return true;
  o->in_field_list = true;
  (*o->item)->increment_ref_count();
  bool found [[maybe_unused]] = false;
  for (size_t idx = 0; idx < qb->fields.size(); idx++) {
    if (qb->base_ref_items[idx] == expr) {
      o->item = &qb->base_ref_items[idx];
      found = true;
      break;
    }
  }
  assert(found);
  o->used = expr->used_tables();
  // Add at back of list
  partition->value.link_in_list(o, &o->next);
  return false;
}

/**
  Add all COUNT(0) to SELECT list of the derived table to be used for
  cardinality checking of the transformed subquery. Minion of
  \c decorrelate_derived_scalar_subquery_pre

  a. Add COUNT(0) OVER (PARTITION BY group-by-list)
  b. Add COUNT(0) OVER (PARTITION BY inner-expr) for each inner-expression
     not already grouped on.
*/
bool Query_block::setup_counts_over_partitions(
    THD *thd, Table_ref *derived, Lifted_expressions_map *lifted_expressions,
    mem_root_deque<Item *> &exprs_added_to_group_by, uint hidden_fields) {
  for (size_t i = 0; i < exprs_added_to_group_by.size() + 1; i++) {
    // 1. Construct PARTITION BY
    PT_order_list *partition = new (thd->mem_root) PT_order_list(POS());
    if (i == 0) {
      // 1. a  partition for original group by list
      for (ORDER *group = group_list.first; group != nullptr;
           group = group->next) {
        if (add_partition_by_expr(thd, partition, this, *group->item))
          return true;
      }
    } else {
      // 1. b  partition for each added expression
      Item *f = exprs_added_to_group_by[i - 1];
      if (add_partition_by_expr(thd, partition, this, f)) return true;
    }

    // 2. Construct default frame
    auto start_bound =
        new (thd->mem_root) PT_border(POS(), WBT_UNBOUNDED_PRECEDING);
    if (start_bound == nullptr) return true;
    auto end_bound =
        new (thd->mem_root) PT_border(POS(), WBT_UNBOUNDED_FOLLOWING);
    if (end_bound == nullptr) return true;
    auto bounds = new (thd->mem_root) PT_borders(POS(), start_bound, end_bound);
    if (bounds == nullptr) return true;
    PT_frame *frame =
        new (thd->mem_root) PT_frame(POS(), WFU_ROWS, bounds, nullptr);
    if (frame == nullptr) return true;
    frame->m_originally_absent = true;

    // 3. Construct window and set it up (mini-version of what is normally done
    // by setup_windows1).
    PT_window *w = new (thd->mem_root)
        PT_window(POS(), partition, /*order_by*/ nullptr, frame);
    if (w == nullptr) return true;
    if (w->setup_ordering_cached_items(thd, this, partition, true)) return true;
    if (w->check_window_functions1(thd, this)) return true;
    // initialize the physical sorting order for the partition
    (void)w->sorting_order(thd, /*implicitly_grouped*/ false);
    char buff[NAME_LEN + 1];
    size_t namelen = snprintf(buff, NAME_LEN, "w%u", m_windows.size());
    Item_string *wname =
        new (thd->mem_root) Item_string(buff, namelen, thd->collation());
    if (wname == nullptr) return true;
    w->set_name(wname);
    if (m_windows.push_back(w)) return true;

    // 4. Construct window function COUNT and bind it
    //
    Item_int *number_0 = new (thd->mem_root) Item_int(int32{0}, 1);
    if (number_0 == nullptr) return true;

    Item_sum *cnt = new (thd->mem_root) Item_sum_count(POS(), number_0, w);
    if (cnt == nullptr) return true;
    cnt->m_is_window_function = true;
    cnt->set_wf();

    int item_no = fields.size() + 1;
    baptize_item(thd, cnt, &item_no);
    m_added_non_hidden_fields++;
    {
      // prelude to binding COUNT(*)
      Query_block *save_query_block = thd->lex->current_query_block();
      assert(save_query_block == outer_query_block());
      thd->lex->set_current_query_block(this);
      const auto save_allow_sum_func = thd->lex->allow_sum_func;
      thd->lex->allow_sum_func |= (nesting_map)1 << nest_level;
      Item *count = cnt;
      if (cnt->fix_fields(thd, &count)) return true;

      // postlude to binding COUNT(*)
      thd->lex->set_current_query_block(save_query_block);
      thd->lex->allow_sum_func = save_allow_sum_func;
    }

    // 5. Add window function to the SELECT list so we can reference it from
    //    outside the derived table (the cardinality check)

    base_ref_items[fields.size()] = cnt;
    lifted_expressions->m_field_positions.push_back(fields.size() -
                                                    hidden_fields);
    fields.push_back(cnt);
    cnt->increment_ref_count();
    // Add a new column to the derived table's query expression
    derived->derived_query_expression()->types.push_back(cnt);
  }
  return false;
}

/**
  Run through the inner expressions and add them to the block's GROUP BY if not
  already present.
*/
bool Query_block::add_inner_exprs_to_group_by(
    THD *thd, List_iterator<Item> &inner_exprs, Item *selected_item,
    bool *selected_expr_added_to_group_by,
    mem_root_deque<Item *> *exprs_added_to_group_by) {
  //
  inner_exprs.rewind();
  while (Item *expr = inner_exprs++) {
    bool found = false;
    for (ORDER *group = group_list.first; group != nullptr;
         group = group->next) {
      Item *gitem = *group->item;
      if (gitem->eq(expr, /*binary_cmp*/ false)) {
        found = true;
        break;
      }
    }

    if (!found) {
      Item *in_select = expr;
      if (selected_item != nullptr &&
          selected_item->real_item()->eq(in_select->real_item(),
                                         /*binary_cmp*/ false)) {
        in_select = selected_item;
        *selected_expr_added_to_group_by = true;
      }
      ORDER *o = new (thd->mem_root) PT_order_expr(POS(), in_select, ORDER_ASC);
      if (o == nullptr) return true;
      o->direction = ORDER_NOT_RELEVANT;  // ignored by constructor
      o->in_field_list = true;
      o->used = in_select->used_tables();
      // Add at back of list
      group_list.link_in_list(o, &o->next);
      exprs_added_to_group_by->push_back(in_select);
    }
  }
  return false;
}

/**
  Minion of \c decorrelate_derived_scalar_subquery_pre.

  Run through the inner fields and add them to the derived table's SELECT list
  if not already present (only one can be present, since it's a scalar
  subquery), and make a note of where in the derived table's field list they
  are positioned: we need that information in
  \c Query_block::decorrelate_derived_scalar_subquery_post

  @param thd              session context
  @param lifted_exprs     the structure containing de-correlation information
  @param selected_field_or_ref
                          the selected field item (actually a field or a ref to
                          a field) if that's what present in the select list, or
                          else a nullptr.
  @param first_non_hidden the index of the first non-hidden field in fields
  @return                 true if error, else false
*/
bool Query_block::add_inner_fields_to_select_list(
    THD *thd, Lifted_expressions_map *lifted_exprs, Item *selected_field_or_ref,
    uint first_non_hidden [[maybe_unused]]) {
  //
  List_iterator<Item> inner_fields(lifted_exprs->m_inner_fields);
  const uint hidden_fields = CountHiddenFields(fields);

  Item_field *const selected_field =
      selected_field_or_ref != nullptr
          ? down_cast<Item_field *>(selected_field_or_ref->real_item())
          : nullptr;

  while (Item *field_or_ref = inner_fields++) {
    Item_field *const f = down_cast<Item_field *>(field_or_ref->real_item());
    // Add non-correlated fields in WHERE clause to select_list if not
    // already present

    if (selected_field == nullptr || f->field != selected_field->field) {
      m_added_non_hidden_fields++;

      // If f->hidden, f should be among the hidden fields in 'fields'.
      assert(std::any_of(fields.cbegin(), fields.cbegin() + first_non_hidden,
                         [&f](const Item *item) { return f == item; }) ==
             f->hidden);

      Item_field *inner_field;

      if (f->hidden) {
        // Make a new Item_field to avoid changing the set of hidden
        // Item_fields.
        inner_field = new (thd->mem_root) Item_field(thd, f);
        if (inner_field == nullptr) return true;
        assert(!inner_field->hidden);
      } else {
        inner_field = f;
      }

      // select_n_where_fields is counted, so safe to add to base_ref_items
      base_ref_items[fields.size()] = inner_field;

      // Compute position in resulting derived table (TABLE::fields)
      // Note the corresponding slice position calculation performed in
      //     - change_to_use_tmp_fields_except_sums  (example figure
      //     expanded)
      //     - change_to_use_tmp_fields
      // takes this new situation into account.
      lifted_exprs->m_field_positions.push_back(fields.size() - hidden_fields);
      fields.push_back(inner_field);
      inner_field->increment_ref_count();
      // We have added to fields; master_query_expression->types must
      // always be equal to it;
      master_query_expression()->types.push_back(inner_field);
    } else {
      // This is the field present in the scalar subquery initially, so it
      // will be first in the derived table's set of fields.
      lifted_exprs->m_field_positions.push_back(0);
    }
  }
  return false;
}

bool Query_block::add_inner_func_calls_to_select_list(
    THD *thd, Lifted_expressions_map *lifted_exprs) {
  // Add non-correlated function calls in WHERE clause to select list, if not
  // present.
  List_iterator<Item> inner_calls(lifted_exprs->m_inner_func_calls);
  const uint hidden_fields = CountHiddenFields(fields);

  while (Item *func_item = inner_calls++) {
    Item_func *func = down_cast<Item_func *>(func_item);
    bool found = false;
    for (size_t i = 0; i < fields.size(); i++) {
      Item *fi = fields[i];
      if (fi->type() != Item::FUNC_ITEM) continue;
      if (down_cast<Item_func *>(fi)->eq(func, /*binary_cmp*/ false)) {
        found = true;
        break;
      }
    }

    if (found) {
      // we found the call in select list, use it
      lifted_exprs->m_func_call_positions.push_back(0);
    } else {
      // The function call is not in select list, add it
      m_added_non_hidden_fields++;

      // The next assignment is safe, because we have reserved space for
      // select_n_where_fields in base_ref_items, which during parsing
      // pessimistically counts all field references disregarding uniqueness,
      // selected or not, and this call contains at least one inner field
      // reference.  and we have a scalar subquery, so only one position is
      // taken by that.
      // For example:
      //   SELECT * FROM t1 WHERE(SELECT a+4 FROM t2
      //                          WHERE -t2.a = -t1.a AND
      //                                abs(t2.b) = t1.a AND
      //                                t2.a + 3 = t1.a AND
      //                                t2.a * 4 = t1.a AND
      //                                t2.a / 3 = t1.a
      //                            ) > 0;
      //
      //   we get these counts when allocating base_ref_items in
      //   Query_block::setup_base_ref_items():
      //
      //   n_sum_items: 0
      //   n_child_sum_items: 0
      //   fields.size(): 1
      //   select_n_having_items: 5  (includes select_list items,
      //                              cf. Item::itemize)
      //   select_n_where_fields: 11
      //   order_group_num: 0
      //   n_scalar_subqueries: 0
      //
      //   These numbers are set somewhat pessimistically during the itemize
      //   phase, i.e. we allocate allocating 17 slots in base_ref_items of
      //   which we end up using 6 - well within bounds, i.e.
      //
      //   [0]:  a+4
      //   [1]:  -t2.a
      //   [2]:  abs(t2.b)
      //   [3]:  t2.a + 3
      //   [4]:  t2.a * 4
      //   [5]:  t2.a / 3
      //
      // Adding another predicate will not change this, because we add only one
      // function from the inner side of the predicate to the select list but
      // have counted at least two field references for the predicate, the
      // inner and the outer, we are even better off.

      base_ref_items[fields.size()] = func;

      // Compute position in resulting derived table (TABLE::fields)
      // Note the corresponding slice position calculation performed in
      //     - change_to_use_tmp_fields_except_sums  (example figure
      //     expanded)
      //     - change_to_use_tmp_fields
      // takes this new situation into account.
      lifted_exprs->m_func_call_positions.push_back(fields.size() -
                                                    hidden_fields);
      int item_no = fields.size() + 1;
      baptize_item(thd, func, &item_no);
      fields.push_back(func);
      func->increment_ref_count();
      // We have added to fields; master_query_expression->types must
      // always be equal to it;
      master_query_expression()->types.push_back(func);
    }
  }
  return false;
}

/**
   We have a correlated scalar subquery, so we must do several things:

   1. Add the relevant non-correlated fields or function calls "NCF"[1] to the
      select list so they can be referenced in the JOIN condition which now
      holds the earlier WHERE AND predicates that were correlated.

      [1] We handle simple column references (fields) separately from general
          expressions ("function calls"), hence the unusual terminology: by
          function calls here we mean expressions that are not simple column
          references, i.e. \c Item_func's.  So, NCF are the inner
          fields/function calls operand of an equality predicate that contains
          an outer field reference in the other operand. These were identified
          in \c supported_correlated_scalar_subquery, and passed in as
          \c lifted_where.

   2. Add a COUNT to select list so it can be referenced from the
      transformed query's WHERE clause for cardinality check, if needed,
      i.e. when there is no aggregate function in the subquery's single[2]
      select expression.
      [2] single because we have a scalar subquery. Add this to NCF. If it
          *does* contain an aggregate function, there will be only one row per
          group iff the NCF are part of any GROUP BY list, and we add them to
          it, so that property holds.
   3. Add grouping on NCF to the subquery. If already grouped, add the NCF
      at end of grouping list. Note that this might result in a grouped query
      that might fail the functional dependency checks. So we wrap any
      non-grouped field in the select list in Item_func_any_value.
      We can safely add the Item_func_any_value because subqueries with
      cardinalities greater than one will be rejected anyway.
      For already grouped subqueries, we use possibly several windowed COUNT
      cardinality checks, cf. \c added_window_card_checks
   4. Remember the set of NCF so we can create derived.field and
      derived.count(field), after setting up the materialized derived
      table, cf. \c lifted_fields.
   5. Update the correlated fields in the JOIN condition to no longer be
      outer references, and the NCFs to refer to the derived table's fields,
      NCF.

  This logic is partially done *before* setting up the materialized derived
  table, in the present method (\c _pre), and partly *after* setting up the
  materialized derived table, cf. the companion method (\c _post).

  @param      thd              session context
  @param      derived          the derived table being created in the transform
  @param      lifted_where     the WHERE condition we move out to the JOIN cond
  @param[out] lifted_exprs     mapping of where inner fields and function calls
                               end up in the derived table's fields.
  @param[out] added_card_check set to true if we are adding a cardinality check
  @param[out] added_window_card_checks
                               set to # of window functions added to SELECT if
                               the subquery is initially grouped and we need
                               COUNT(*) OVER (...) to be checked
*/
bool Query_block::decorrelate_derived_scalar_subquery_pre(
    THD *thd, Table_ref *derived, Item *lifted_where,
    Lifted_expressions_map *lifted_exprs, bool *added_card_check,
    size_t *added_window_card_checks) {
  const uint hidden_fields = CountHiddenFields(fields);
  const uint first_non_hidden = hidden_fields;
  assert((fields.size() - hidden_fields) ==
         1);  // scalar subquery

#ifndef NDEBUG
              // Hidden fields should come before non-hidden.
  for (uint i = 0; i < fields.size(); i++) {
    assert((fields[i]->hidden) != (i >= hidden_fields));
  }
#endif

  Item *selected_field_or_ref = nullptr;
  Item_func *selected_func_call = nullptr;

  //****************************************************************
  // Save the select item in the appropriate lifted structure if any
  //****************************************************************
  if (fields[first_non_hidden]->type() == Item::FUNC_ITEM &&
      !fields[first_non_hidden]->has_aggregation()) {
    selected_func_call = down_cast<Item_func *>(fields[first_non_hidden]);
  } else if (fields[first_non_hidden]->real_item()->type() ==
             Item::FIELD_ITEM) {
    selected_field_or_ref = fields[first_non_hidden];
  }

  // Containers for collected outer and inner fields.
  Item::Collect_item_fields_or_refs outer_info{&lifted_exprs->m_outer_fields};
  Item::Collect_item_fields_or_refs inner_info_fields{
      &lifted_exprs->m_inner_fields};

  //**************************************************************
  // Walk where predicates and collect inner field references and
  // function calls
  //**************************************************************
  Item_cond_and *lw = down_cast<Item_cond_and *>(lifted_where);
  List_iterator<Item> eq_li(*lw->argument_list());

  while (Item *item = eq_li++) {
    Item_func_eq *eq = down_cast<Item_func_eq *>(item);
    for (size_t j = 0; j < 2; j++) {
      if (eq->arguments()[j]->is_outer_reference()) {
        if (eq->arguments()[j]->walk(&Item::collect_item_field_or_ref_processor,
                                     enum_walk::PREFIX | enum_walk::POSTFIX,
                                     pointer_cast<uchar *>(&outer_info)))
          return true;
      } else {
        // <outer> = <inner_ref>  or  <outer> = func( .., <inner_ref>, ..)
        Item *this_item = eq->arguments()[j];

        if (this_item->type() == Item::FUNC_ITEM) {
          // For function calls we collect separately the function call and
          // the field arguments, skipping any constant args
          Item_func *this_item_func = down_cast<Item_func *>(this_item);
          List_iterator<Item> item_list_it(lifted_exprs->m_inner_func_calls);
          Item *curr_item;
          bool found = false;
          while ((curr_item = item_list_it++)) {
            if (curr_item->eq(this_item, true)) {
              found = true;
              break;
            }
          }
          if (!found)
            lifted_exprs->m_inner_func_calls.push_back(this_item_func);
        } else {
          if (this_item->walk(&Item::collect_item_field_or_ref_processor,
                              enum_walk::PREFIX | enum_walk::POSTFIX,
                              pointer_cast<uchar *>(&inner_info_fields)))
            return true;
        }
      }
    }
  }

  //**************************************************************
  // Add inner fields and calls to the derived table's select list
  //**************************************************************
  if (add_inner_fields_to_select_list(thd, lifted_exprs, selected_field_or_ref,
                                      first_non_hidden))
    return true;

  if (add_inner_func_calls_to_select_list(thd, lifted_exprs)) return true;

  //****************************************************************
  // Add inner fields and calls to the derived table's GROUP BY list
  //****************************************************************
  const bool subquery_was_grouped =
      is_explicitly_grouped() || is_implicitly_grouped();
  const bool subquery_was_explicitly_grouped = is_explicitly_grouped();

  mem_root_deque<Item *> exprs_added_to_group_by(thd->mem_root);

  // True if selected expr was added by us to the group by list (not originally
  // present. Used to determine whther to wrap it in ANY_VALUE.
  bool selected_expr_added_to_group_by = false;

  List_iterator<Item> inner_fields(lifted_exprs->m_inner_fields);
  if (add_inner_exprs_to_group_by(thd, inner_fields, selected_field_or_ref,
                                  &selected_expr_added_to_group_by,
                                  &exprs_added_to_group_by))
    return true;

  // Prepare for setting m_no_of_added_exprs
  const auto sz = group_list.elements;

  List_iterator<Item> inner_calls(lifted_exprs->m_inner_func_calls);
  if (add_inner_exprs_to_group_by(thd, inner_calls, selected_func_call,
                                  &selected_expr_added_to_group_by,
                                  &exprs_added_to_group_by))
    return true;

  if (subquery_was_explicitly_grouped)
    // See definition of m_no_of_added_exprs for rationale
    m_no_of_added_exprs = group_list.elements - sz;

  //****************************************************************
  // Potentially protect a selected field item in ANY_VALUE
  //****************************************************************
  Item *const fnh = fields[first_non_hidden];
  if (!subquery_was_grouped && !selected_expr_added_to_group_by &&
      !fnh->const_item() &&
      !is_function_of_type(fnh, Item_func::ANY_VALUE_FUNC)) {
    Item *const old_expr = fnh;
    Item *func_any = new (thd->mem_root) Item_func_any_value(old_expr);
    if (func_any == nullptr) return true;
    if (func_any->fix_fields(thd, &func_any)) return true;
    fields[first_non_hidden] = func_any;
    replace_referenced_item(old_expr, func_any);
  }

  //****************************************************************
  // Add grouped COUNT(0) to the select list if subquery was not grouped, or
  // one or more windowed COUNT(0) if subquery was explicitly grouped
  //****************************************************************
  if (!subquery_was_grouped) {
    Item_int *number_0 = new (thd->mem_root) Item_int(int32{0}, 1);
    if (number_0 == nullptr) return true;
    Item *cnt = new (thd->mem_root) Item_sum_count(number_0);
    if (cnt == nullptr) return true;
    int item_no = fields.size() + 1;
    baptize_item(thd, cnt, &item_no);
    m_added_non_hidden_fields++;

    // prelude to binding COUNT(*)
    Query_block *save_query_block = thd->lex->current_query_block();
    assert(save_query_block == outer_query_block());
    thd->lex->set_current_query_block(this);
    const auto save_allow_sum_func = thd->lex->allow_sum_func;
    thd->lex->allow_sum_func |= (nesting_map)1 << nest_level;

    if (cnt->fix_fields(thd, &cnt)) return true;

    // postlude to binding COUNT(*)
    thd->lex->set_current_query_block(save_query_block);
    thd->lex->allow_sum_func = save_allow_sum_func;

    // This is safe, because we have reserved space for select_n_where_fields,
    // but at least one of them is an outer reference so this extra COUNT(*)
    // can use the first such space.
    base_ref_items[fields.size()] = cnt;
    lifted_exprs->m_field_positions.push_back(fields.size() - hidden_fields);
    fields.push_back(cnt);
    cnt->increment_ref_count();
    m_agg_func_used = true;
    // Add a new column to the derived table's query expression
    derived->derived_query_expression()->types.push_back(cnt);
    *added_card_check = true;
  } else if (subquery_was_explicitly_grouped) {
    // For this case (not implicit grouping and correlated), we need to make
    // sure the derived table has no more than one row of each partition on
    // a) the grouped expression list and b) any added inner expression: to do
    // this we add window function COUNT and check that it is less than or
    // equal to one.
    if (setup_counts_over_partitions(thd, derived, lifted_exprs,
                                     exprs_added_to_group_by, hidden_fields))
      return true;
    *added_window_card_checks = 1 + exprs_added_to_group_by.size();
  }
  return false;
}

/**
  Function calls in lifted join condition.

  Replace occurences for inner function calls in lifted predicates with the
  corresponding field in the derived table. This must be performed *before* we
  replace inner field (below) to avoid replacing function arguments of the
  derived table, eg. if we have ABS(t2.a) = \c outer_table.a in a subquery, we
  will lift this predicate. But we have also added ABS(t2.a) to the GROUP BY
  list in the derived table and its argument t2.a should not be replaced,
  rather we replace the entire function call with \c derived_table.`abs(t2.a)`
  in the lifted join condition.
*/
static bool replace_inner_function_calls_in_lifted_predicate(
    THD *thd, Table_ref *derived, Lifted_expressions_map *lifted_exprs,
    Query_block *qb) {
  uint call_pos_idx = 0;
  List_iterator<Item> li_funcs(lifted_exprs->m_inner_func_calls);
  Item *func_item;
  while ((func_item = li_funcs++)) {
    Item_func *func = down_cast<Item_func *>(func_item);
    Field *field_in_derived =
        derived->table
            ->field[lifted_exprs->m_func_call_positions[call_pos_idx++]];
    auto *replaces_field = new (thd->mem_root) Item_field(field_in_derived);
    if (replaces_field == nullptr) return true;

    Item::Item_func_call_replacement info(func, replaces_field, qb);

    Item *new_item = derived->join_cond()->transform(
        &Item::replace_func_call, pointer_cast<uchar *>(&info));
    if (new_item == nullptr) return true;
    if (new_item != derived->join_cond()) derived->set_join_cond(new_item);
  }
  return false;
}

/**
  Fields in lifted join condition.

  We added referenced inner fields to select list, now replace occurences of
  such fields in the join condition with derived.<Item_field-n>. Since we have
  now set up materialization for the derived table, we now know the Field to
  use for a new \c Item_field.
*/
static bool replace_inner_fields_in_lifted_predicate(
    THD *thd, Table_ref *derived, Lifted_expressions_map *lifted_exprs,
    Query_block *qb, uint *field_pos_idx) {
  Item *field_or_ref;
  List_iterator<Item> li(lifted_exprs->m_inner_fields);

  while ((field_or_ref = li++)) {
    Item_field *f = down_cast<Item_field *>(field_or_ref->real_item());

    Field *field_in_derived =
        derived->table->field[lifted_exprs->m_field_positions[*field_pos_idx]];
    *field_pos_idx += 1;

    auto *replaces_field = new (thd->mem_root) Item_field(field_in_derived);
    if (replaces_field == nullptr) return true;
    assert(replaces_field->data_type() == f->data_type());

    Item::Item_field_replacement info(f->field, replaces_field, qb);
    Item *new_item = derived->join_cond()->transform(
        &Item::replace_item_field, pointer_cast<uchar *>(&info));
    if (new_item == nullptr) return true;
    if (new_item != derived->join_cond()) derived->set_join_cond(new_item);
  }
  return false;
}

// Add derived.count(0) <= 1 assert condition to transformed query block's WHERE
// condition to preserve semantics of original query.
static bool build_reject_if(THD *thd, Table_ref *derived,
                            Lifted_expressions_map *lifted_exprs,
                            uint field_pos_idx) {
  const uint cnt_pos_in_fields = lifted_exprs->m_field_positions[field_pos_idx];
  Field *cnt_f = derived->table->field[cnt_pos_in_fields];
  auto *cnt_i = new (thd->mem_root) Item_field(cnt_f);
  if (cnt_i == nullptr) return true;

  auto *number_1 = new (thd->mem_root) Item_int(1);
  if (number_1 == nullptr) return true;
  auto *gt = new (thd->mem_root) Item_func_gt(cnt_i, number_1);
  if (gt == nullptr) return true;
  auto *check_card = new (thd->mem_root) Item_func_reject_if(gt);
  if (check_card == nullptr) return true;

  Item *new_cond = and_items(derived->join_cond(), check_card);
  if (new_cond == nullptr) return true;
  new_cond->apply_is_true();
  if (new_cond->fix_fields(thd, &new_cond)) return true;
  derived->set_join_cond(new_cond);
  return false;
}

/**
  See explanation in companion method
  \c decorrelate_derived_scalar_subquery_pre.
*/
bool Query_block::decorrelate_derived_scalar_subquery_post(
    THD *thd, Table_ref *derived, Lifted_expressions_map *lifted_exprs,
    bool added_card_check, size_t added_window_card_checks) {
  //****************************************************************
  // Replace fields and calls in the lifted predicates
  //****************************************************************
  if (replace_inner_function_calls_in_lifted_predicate(thd, derived,
                                                       lifted_exprs, this))
    return true;

  uint field_pos_idx = 0;

  if (replace_inner_fields_in_lifted_predicate(thd, derived, lifted_exprs, this,
                                               &field_pos_idx))
    return true;

  //****************************************************************
  // Replace outer references in the lifted predicates with inner
  // now that the predicates have been lifted out.
  //****************************************************************
  List_iterator<Item> li(lifted_exprs->m_outer_fields);
  while (Item *field_or_ref = li++) {
    Item_field *f = down_cast<Item_field *>(field_or_ref->real_item());
    // This field used to be correlated, but is now lifted out to ON
    // clause, so change its outer status
    if (field_or_ref->type() == Item::REF_ITEM) {
      down_cast<Item_ref *>(field_or_ref)->depended_from = nullptr;
      // If this is an outer ref, we need to replace the ref with the
      // underlying field as it is no more correlated. Else used_tables
      // will not be correct.
      if (down_cast<Item_ref *>(field_or_ref)->ref_type() ==
          Item_ref::OUTER_REF) {
        Item *new_item = derived->join_cond()->transform(
            &Item::replace_outer_ref, pointer_cast<uchar *>(field_or_ref));
        if (new_item != derived->join_cond()) derived->set_join_cond(new_item);
      }
    }
    f->depended_from = nullptr;
  }

  //****************************************************************
  // Add cardinality check (REJECT_IF) of derived table if necessary
  //****************************************************************
  if (added_card_check) {
    if (build_reject_if(thd, derived, lifted_exprs, field_pos_idx)) return true;
    cond_count++;
  } else {
    for (size_t wno = 0; wno < added_window_card_checks; wno++) {
      if (build_reject_if(thd, derived, lifted_exprs, field_pos_idx++))
        return true;
      cond_count++;
    }
  }

  derived->join_cond()->update_used_tables();
  // Simplify join condition if only a single condition in our AND node since
  // it looks slightly better in EXPLAIN output.
  auto *and_cond = down_cast<Item_cond_and *>(derived->join_cond());
  if (and_cond->argument_list()->elements == 1) {
    List_iterator<Item> it(*and_cond->argument_list());
    derived->set_join_cond(it++);
  }
  return false;
}

/**
  Replace item in select list and preserve its reference count.

  @param old_item  Item to be replaced.
  @param new_item  Item to replace the old item.

  If old item is present in base_ref_items, make sure it is replaced there.

  Also make sure that reference count for old item is preserved in new item.
*/
void Query_block::replace_referenced_item(Item *const old_item,
                                          Item *const new_item) {
  for (size_t i = 0; i < fields.size(); i++) {
    if (base_ref_items[i] == old_item) {
      base_ref_items[i] = new_item;
      break;
    }
  }
  // Keep the same number of references as for the old expression:
  new_item->increment_ref_count();
  while (old_item->decrement_ref_count() > 0) {
    new_item->increment_ref_count();
  }
}

/**
  Converts a subquery to a derived table and inserts it into the FROM
  clause of the owning query block

  @param thd            Connection handle
  @param[out]    out_tl The created derived table will be stored in this.
  @param subs_query_expression      Unit for the subquery
  @param subq           Item for the subquery
  @param use_inner_join Insert with INNER JOIN, or with LEFT JOIN
  @param reject_multiple_rows
                        For scalar subqueries where we need run-time cardinality
                        check: true, else false
  @param join_condition See join_cond in synthesize_derived()
  @param lifted_where_cond
                        The subquery's where condition, moving to JOIN cond of
                        JOIN with the derived table
*/
bool Query_block::transform_subquery_to_derived(
    THD *thd, Table_ref **out_tl, Query_expression *subs_query_expression,
    Item_subselect *subq, bool use_inner_join, bool reject_multiple_rows,
    Item *join_condition, Item *lifted_where_cond) {
  Table_ref *tl;
  {
    // We did not do the transformation yet
    remember_transform(thd, this);

    // We want the Table_ref, Table_ident and m_join_cond to be permanent
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    tl = synthesize_derived(thd, subs_query_expression, join_condition,
                            /*left_outer=*/true, use_inner_join);

    if (tl == nullptr) return true;

    if (lifted_where_cond != nullptr) {
      tl->set_join_cond(lifted_where_cond);
      cond_count += (lifted_where_cond->type() == Item::COND_ITEM)
                        ? down_cast<Item_cond *>(lifted_where_cond)
                              ->argument_list()
                              ->elements
                        : 1;
    }

    // Append to end of leaf tables list
    Table_ref *leaf;
    for (leaf = leaf_tables; leaf->next_leaf != nullptr;
         leaf = leaf->next_leaf) {
    }
    leaf->next_leaf = tl;

    // Adjust table no and map
    if (leaf_table_count >= MAX_TABLES) {
      my_error(ER_TOO_MANY_TABLES, MYF(0), static_cast<int>(MAX_TABLES));
      return true;
    }
    tl->set_tableno(leaf_table_count);

    tl->embedding->nested_join->query_block_id =
        subq->query_expr()->first_query_block()->select_number;
    leaf_table_count += 1;

    if (!(tl->derived_result = new (thd->mem_root) Query_result_union()))
      return true; /* purecov: inspected */
    subs_query_expression->m_reject_multiple_rows = reject_multiple_rows;
    subs_query_expression->set_explain_marker(thd, CTX_DERIVED);
    subs_query_expression->first_query_block()->linkage = DERIVED_TABLE_TYPE;

    // Break connection to the subquery expression:
    subs_query_expression->item = nullptr;
  }
  subs_query_expression->set_query_result(tl->derived_result);
  subs_query_expression->first_query_block()->set_query_result(
      tl->derived_result);

  materialized_derived_table_count++;
  derived_table_count++;

  Lifted_expressions_map lifted_where_expressions(thd->mem_root);
  bool added_cardinality_check = false;
  size_t added_window_cardinality_checks = 0;
  if (lifted_where_cond != nullptr) {
    assert(!subs_query_expression->is_set_operation());
    if (subs_query_expression->first_query_block()
            ->decorrelate_derived_scalar_subquery_pre(
                thd, tl, lifted_where_cond, &lifted_where_expressions,
                &added_cardinality_check, &added_window_cardinality_checks))
      return true;
  }
  // We skip resolve_derived(), as the subquery has already been resolved
  // before the conversion to derived table.
  assert(tl->table == nullptr);
  if (tl->setup_materialized_derived(thd)) return true; /* purecov: inspected */

  if (lifted_where_cond != nullptr) {
    assert(tl->join_cond() == lifted_where_cond);
    if (decorrelate_derived_scalar_subquery_post(
            thd, tl, &lifted_where_expressions, added_cardinality_check,
            added_window_cardinality_checks))
      return true;
  }

  *out_tl = tl;
  return false;
}

/**
  WL#15540 check that predicate operand item conforms to our requirements.
  - Single inner item, e.g item is t2.a:
       WHERE (SELECT ... FROM t2 WHERE t2.a = outer_column)
  - Function call containing one or more inner items (no outer!), e.g.
    item is ABS(t2.a) + t2.b:
       WHERE (SELECT ... FROM t2 WHERE ABS(t2.a) + t2.b = outer_column)

  Any constant arguments are ignored (supported also). The function call must
  be (recursively) deterministic and cannot contain subqueries.
  @returns pair<bool, bool> where
    first bool:  true if we found at least one inner field contained in the
                 item
    second bool: true if non-conforming (subquery or non-deterministic)
*/
static std::pair<bool, bool> item_containing_non_correlated_field(Item *item) {
  const Item::Type typ = item->real_item()->type();
  if (typ == Item::FIELD_ITEM) return {true, false};
  if (typ == Item::SUBQUERY_ITEM) return {false, true};
  if (typ != Item::FUNC_ITEM) return {false, false};
  Item_func *f = down_cast<Item_func *>(item->real_item());
  if (f->is_non_deterministic()) return {false, true};
  std::pair<bool, bool> result{false, false};
  for (uint i = 0; i < f->arg_count; i++) {
    std::pair<bool, bool> tmp =
        item_containing_non_correlated_field(f->arguments()[i]);
    result = {result.first || tmp.first, result.second || tmp.second};
  }
  return result;
}

/**
  Called to check if the provided correlated predicate is eligible for
  transformation. To be eligible, it must have one correlated operand
  and one non-orrelated ("inner") operand. The correlated operand cannot contain
  a non-correlated field, but must contain at least one correlated field.
  It can be a deterministic function over (other)
  deterministic functions and correlated field(s), recursively,
  or a simple correlated field.

  @param  cor_pred correlated predicate that needs to be examined
  @return true if predicate is eligible for transformation.
*/
bool is_correlated_predicate_eligible(Item *cor_pred) {
  assert(cor_pred->is_outer_reference());
  if (cor_pred->type() != Item::FUNC_ITEM ||
      down_cast<Item_func *>(cor_pred)->functype() != Item_func::EQ_FUNC)
    return false;
  Item_func *eq_func = down_cast<Item_func *>(cor_pred);
  bool non_correlated_operand = false;
  for (uint i = 0; i < eq_func->argument_count(); i++) {
    Item *item = eq_func->arguments()[i];
    if (!item->is_outer_reference()) {
      std::pair<bool, bool> result = item_containing_non_correlated_field(item);
      if (result.second) return false;  // illegal
      non_correlated_operand = result.first;
    } else if (item->used_tables() & ~PSEUDO_TABLE_BITS) {
      // Inner table reference mixed with outer table reference is not
      // allowed.
      return false;
    }
  }
  // We need to find one non-correlated operand in the correlated predicate
  return non_correlated_operand;
}

/**
  Extracts the top level correlated condition in an OR condition.

  For ex:
  (((t1.a = t2.b ) and (t1.c =10)) OR ((t1.a = t2.b) and (t1.d =10))) is the
  same as (t1.a = t2.b) and ((t1.c = 10) or (t1.d = 10))

  So we extract the (t1.a = t2.b) as the correlated condition and leave ((t1.c
  = 10) or (t1.d = 10)) in the original condition that is passed as the
  argument.

  The caller of the function has to send an OR condition. Only the top level
  correlated condition is extracted. Caller could repeatedly call this
  function to extract the inner level correlated conditions as well.

  @param thd session context
  @param[in,out] cond Original condition that is looked into, to extract the
                      correlated condition.
  @param[out]    correlated_cond correlated condition that is extracted

  @return false when a correlated condition is successfully extracted.
          true  when no correlated condition could be extracted.
*/

static bool extract_correlated_condition(THD *thd, Item **cond,
                                         Item **correlated_cond) {
  Item_cond *or_condition = down_cast<Item_cond *>(*cond);
  Item *cor_pred = nullptr;
  bool found = false;
  for (Item &item : *or_condition->argument_list()) {
    Mem_root_array<Item *> cond_parts(thd->mem_root);
    ExtractConditions(&item, &cond_parts);  // all elements AND'ed
    found = false;
    for (Item *pred : cond_parts) {
      // Check if we have a correlated condition that is present in all the
      // arguments to this OR condition. Only then we can extract it.
      if (pred->is_outer_reference()) {
        // If the correlated condition itself is disjuntive, we reject.
        if (pred->type() == Item::COND_ITEM) return true;
        // If this is the first argument to the OR condition, we need to be
        // finding this correlated condition in all other arguments of the OR
        // condition
        if (cor_pred == nullptr) cor_pred = pred;
        // If it is not the first argument to the OR condition, we already
        // have a predicate with us that we need to look for in this argument.
        // So, continue to search until we find it.
        else if (!cor_pred->eq(pred, false))
          continue;
        found = true;
        if (!is_correlated_predicate_eligible(cor_pred)) return true;
        break;
      }
    }
    if (!found) return true;
  }

  // We now have a correlated condition that could be extracted. So we remove
  // the condition from each of the arguments of the OR condition and return
  // the correlated condition to the caller.
  List_iterator<Item> li(*(or_condition->argument_list()));
  Item *item;
  while ((item = li++)) {
    Mem_root_array<Item *> cond_parts(thd->mem_root);
    ExtractConditions(item, &cond_parts);  // all elements AND'ed
    Mem_root_array<Item *> final_args(thd->mem_root);
    for (Item *pred : cond_parts) {
      if (!cor_pred->eq(pred, false)) final_args.push_back(pred);
    }
    if (final_args.size() == 0)
      li.remove();
    else {
      auto *tmp_cond = down_cast<Item_cond *>(*li.ref());
      tmp_cond->argument_list()->clear();
      for (Item *pred : final_args) tmp_cond->argument_list()->push_back(pred);
      li.replace(tmp_cond);
    }
  }
  or_condition->update_used_tables();
  *correlated_cond = cor_pred;
  return false;
}

/**

  Called when the scalar subquery is correlated. If the type of correlation is
  not supported, return false and leave \c *lifted_where unassigned. If it is
  supported,  \c *lifted_where contains a set of correlated predicates.
  Currently, we can only de-correlate the WHERE clause: if the clause is not a
  top level AND, we lift out the entire predicate to the JOIN clause. If it is
  a top level AND, we lift out only those AND operand predicates which are
  correlated, leaving un-correlated operand predicates in the subquery's WHERE
  clause, as lifting all out would be too ineffective, potentially creating
  large cartesian products in the subquery.

  @param        thd           session context
  @param        subquery      the subquery under consideration
  @param[out]   lifted_where  set of predicates lifted out of WHERE
  @returns true for error else false
*/
bool Query_block::supported_correlated_scalar_subquery(THD *thd,
                                                       Item::Css_info *subquery,
                                                       Item **lifted_where) {
  // Disallow if subquery is in a JOIN clause
  if (subquery->m_location &
      Item_aggregate_type::Collect_scalar_subquery_info::L_JOIN_COND)
    return false;

  // Check that we do no have correlation inside a derived table in the
  // FROM list
  for (Table_ref *tr = leaf_tables; tr != nullptr; tr = tr->next_leaf)
    if (tr->is_derived() && tr->derived_query_expression()->uncacheable)
      return false;

  // Disallow LIMIT, OFFSET
  if (has_limit()) return false;

  // Disallow window functions: transform not valid in their presence.
  if (has_windows()) return false;

  // Disallow ROLLUP
  if (olap == ROLLUP_TYPE) return false;

  const size_t first_selected = CountHiddenFields(fields);
  if (is_implicitly_grouped()) {
    Item_sum::Collect_grouped_aggregate_info aggregates(this);
    if (fields[first_selected]->walk(&Item::collect_grouped_aggregates,
                                     enum_walk::PREFIX,
                                     pointer_cast<uchar *>(&aggregates))) {
      return true;
    }
    bool saw_count{false};
    Item_sum *cnt_item{nullptr};
    for (auto a : aggregates.list) {
      if (a->sum_func() == Item_sum::COUNT_FUNC ||
          a->sum_func() == Item_sum::COUNT_DISTINCT_FUNC) {
        saw_count = true;
        cnt_item = a;
      }
    }

    if (saw_count) {
      // The COUNT() must be the selected item, no expression involved
      if (fields[first_selected] != cnt_item) return false;
      // If we have an occurrence of COUNT() in the selected expression and
      // implicit grouping , we know that the transform can yield NULL rather
      // than 0. In such a case, we need to add a COALESCE around the replaced
      // subquery expression, i.e. COALESCE(derived.`COUNT()`, 0). This is
      // because in a LEFT JOIN inner position, a COUNT(0) can yield NULL
      // which it could not in the original subquery position.
      subquery->m_add_coalesce = true;
    }
  }

  Item *select_item = single_visible_field();
  assert(select_item != nullptr);

  // Disallow subquery in selected expression
  if (select_item->has_subquery()) return false;

  // Disallow non deterministic functions
  if (select_item->type() == Item::FUNC_ITEM &&
      down_cast<Item_func *>(select_item)->is_non_deterministic())
    return false;

  // Check whether the select expression may change nulls to non-nulls:
  // it may change semantics of the transformed query if so, hence we deny this
  for (auto *sel_expr : visible_fields()) {
    if (WalkItem(sel_expr, enum_walk::PREFIX, [](Item *inner_item) {
          return inner_item->type() == Item::FUNC_ITEM &&
                 !down_cast<Item_func *>(inner_item)->is_null_on_null();
        }))
      return false;
  }

  // Only allow outer reference in the WHERE clause, check now

  // 1. select list
  if (select_item->is_outer_reference()) return false;

  // 2. group by clause
  if (is_grouped()) {
    for (ORDER *group = group_list.first; group != nullptr;
         group = group->next) {
      if ((*group->item)->is_outer_reference()) return false;
    }
  }

  // 3. HAVING clause
  if (having_cond() != nullptr && having_cond()->is_outer_reference())
    return false;

  // 4. ORDER BY clause
  if (is_ordered()) {
    for (ORDER *o = order_list.first; o != nullptr; o = o->next) {
      if ((*o->item)->is_outer_reference()) return false;
    }
  }

  if (m_where_cond == nullptr) {
    // We expect to find outer references (field of a FROM table of a query
    // block directly containing this subquery) in the WHERE, since all other
    // possibilities are exhausted.  But we didn't find any correlated field.
    // It may have disappeared due to ORDER BY elimination in the subquery.
    // The subquery will still be marked as using having correlated fields.
    // How to handle this?
    //  TODO.  Example:
    //  SELECT t1.a, SUM(t1.b)
    //  FROM t1
    //  WHERE t1.a = (SELECT SUM(t2.b)
    //               FROM t2 ORDER BY SUM(t2.b) + SUM(t1.b) LIMIT 1)
    //  GROUP BY t
    return false;
  }

  // Check that the WHERE clause doesn't contain an aggregate function which
  // aggregates outside this query block. We only want outer reference to
  // a field.
  Item_sum::Collect_grouped_aggregate_info aggregates(this);
  if (m_where_cond->walk(&Item::collect_grouped_aggregates, enum_walk::PREFIX,
                         pointer_cast<uchar *>(&aggregates)))
    return true;

  if (aggregates.m_outside)
    // some aggregate functions aggregate in an outer query, not supported
    return false;

  // Check that the WHERE clause doesn't contain any nested scalar subqueries
  // that are still there (correlated of a kind we couldn't handle: any nested
  // subqueries that did support transformation will already have been
  // transformed).
  Item::Collect_scalar_subquery_info subqueries;
  subqueries.m_collect_unconditionally = true;
  if (m_where_cond->walk(&Item::collect_scalar_subqueries, enum_walk::PREFIX,
                         pointer_cast<uchar *>(&subqueries)))
    return true;
  if (subqueries.m_list.size() > 0) return false;

  // Get all fields/refs referenced in the WHERE clause, and count the number
  // of correlated ones.
  List<Item> fields_or_refs;
  Item::Collect_item_fields_or_refs info{&fields_or_refs};
  if (m_where_cond->walk(&Item::collect_item_field_or_ref_processor,
                         enum_walk::PREFIX | enum_walk::POSTFIX,
                         pointer_cast<uchar *>(&info)))
    return true;

  int cnt = 0;
  List_iterator<Item> li(fields_or_refs);
  while (Item *i = li++) {
    cnt = cnt + (i->is_outer_reference() ? 1 : 0);
  }

  if (cnt == 0) {
    // We didn't find any correlated field. It may have disappeared due to
    // ORDER BY elimination in the subquery. The subquery would still be
    // marked as having correlated fields. Related case to missing WHERE
    // above.
    //
    // TODO: We can improve these two cases by returning, presuming no
    // correlation, but we would like to improve the status of the subquery's
    // used_tables instead.
    //
    // Example: (correlated field inside ORDER BY optimized away)
    // SELECT t1.a, SUM(t1.b)
    // FROM t1
    // WHERE t1.a = (SELECT SUM(t2.b)
    //               FROM t2
    //               WHERE t2.a > 4 ORDER BY t1.b)
    // GROUP BY t1.a ORDER BY t1.a LIMIT 30;
    return false;
  }

  // Extract the predicates that must be moved out to JOIN, i.e. those AND
  // constituents which contain an outer reference, and those which shall
  // remain.
  Mem_root_array<Item *> staying(thd->mem_root);
  List<Item> going;
  Mem_root_array<Item *> condition_parts(thd->mem_root);
  bool orig_where_modified = false;
  ExtractConditions(m_where_cond, &condition_parts);  // all elements AND'ed
  for (Item *cond_part : condition_parts) {
    // If the condition part extracted is an OR condition having correlated
    // fields, we extract top level correlated condition if possible. If not,
    // transformation cannot happen.
    if (cond_part->is_outer_reference()) {
      Item *cor_pred = nullptr;
      if (cond_part->type() == Item::COND_ITEM) {
        assert(down_cast<Item_cond *>(cond_part)->functype() ==
               Item_func::COND_OR_FUNC);
        if (extract_correlated_condition(thd, &cond_part, &cor_pred))
          return false;
        // Make a note if this extracted predicate is the same as the original
        // where condition.
        if (cond_part == m_where_cond) orig_where_modified = true;
      } else {
        cor_pred = cond_part;
        cond_part = nullptr;
      }
      if (!is_correlated_predicate_eligible(cor_pred)) return false;
      going.push_back(cor_pred);
    }
    if (cond_part) staying.push_back(cond_part);
  }

  // No correlated predicates. Note that we did find some fields earlier which
  // were marked as being an "outer reference". However, it might be that the
  // expression containing this outer reference is not marked as such due to
  // some optimizations. Reject such queries for transformation (Since we
  // anyways reject queries with non-correlated operands having expressions in
  // is_correlated_predicate_eligible())
  if (going.elements == 0) return false;

  // Construct a new, reduced, WHERE clause sans the lifted predicates, which
  // will stay in the subquery
  if (staying.size() == 0) {
    m_where_cond = nullptr;
  } else {
    // If the original where condition was a disjunctive correlated predicate,
    // it would have been modified when extracting the correlated condition.
    // So, just update the used tables.
    if (orig_where_modified)
      m_where_cond->update_used_tables();
    else {
      auto *new_where = down_cast<Item_cond *>(m_where_cond);
      new_where->argument_list()->clear();
      for (Item *pred : staying) new_where->argument_list()->push_back(pred);
      m_where_cond = new_where;
      new_where->update_used_tables();
    }
    assert(!m_where_cond->is_outer_reference());
  }

  // Construct the lifted part of the WHERE condition, which will go to the
  // JOIN condition
  auto *cond = new (thd->mem_root) Item_cond_and(going);
  if (cond == nullptr) return true;

  cond->update_used_tables();
  *lifted_where = cond;

  // there is no outer reference in this query expression/block anymore
  uncacheable &= ~UNCACHEABLE_DEPENDENT;
  master_query_expression()->uncacheable &= ~UNCACHEABLE_DEPENDENT;
  return false;
}

bool Query_block::transform_scalar_subqueries_to_join_with_derived(THD *thd) {
  if (thd->lex->m_subquery_to_derived_is_impossible) return false;

  // Need at least one FROM table. Also, we do not want to perform this
  // transformation if we have an assignment of a user variable in the query.
  if (leaf_table_count == 0 || thd->lex->set_var_list.elements > 0)
    return false;

  /*
    Collect list of eligible scalar subqueries used in JOIN conds, WHERE
    conds, SELECT list expressions and HAVING cond. NOTE: Join conditions need
    to be collected/transformed first since they have the be nested after the
    outer join table (i.e. before the inner). So, if we have scalar subqueries
    in other locations that the JOIN conditions, those need to be added after
    the JOIN conditions have been put in place.
  */

  Item::Collect_scalar_subquery_info subqueries;

  // Collect from join conditions
  if (walk_join_conditions(
          m_table_nest,
          [&](Item **expr_p) mutable -> bool {
            subqueries.m_location =
                Item::Collect_scalar_subquery_info::L_JOIN_COND;
            if ((*expr_p)->has_subquery() &&
                (*expr_p)->walk(&Item::collect_scalar_subqueries,
                                enum_walk::PREFIX | enum_walk::POSTFIX,
                                pointer_cast<uchar *>(&subqueries)))
              return true; /* purecov: inspected */
            return false;
          },
          &subqueries))
    return true; /* purecov: inspected */

  subqueries.m_location = Item::Collect_scalar_subquery_info::L_WHERE;

  Item **where_expr_p = &m_where_cond;
  if (*where_expr_p != nullptr && (*where_expr_p)->has_subquery()) {
    if ((*where_expr_p)
            ->walk(&Item::collect_scalar_subqueries,
                   enum_walk::PREFIX | enum_walk::POSTFIX,
                   pointer_cast<uchar *>(&subqueries)))
      return true; /* purecov: inspected */
  }

  subqueries.m_location =
      Item_singlerow_subselect::Collect_scalar_subquery_info::L_SELECT;
  for (Item *select_expr : visible_fields()) {
    if (select_expr->has_subquery() &&
        select_expr->walk(&Item::collect_scalar_subqueries,
                          enum_walk::PREFIX | enum_walk::POSTFIX,
                          pointer_cast<uchar *>(&subqueries)))
      return true; /* purecov: inspected */
  }

  subqueries.m_location = Item::Collect_scalar_subquery_info::L_HAVING;
  Item **having_expr_p = &m_having_cond;
  if (*having_expr_p != nullptr && (*having_expr_p)->has_subquery()) {
    if ((*having_expr_p)
            ->walk(&Item::collect_scalar_subqueries,
                   enum_walk::PREFIX | enum_walk::POSTFIX,
                   pointer_cast<uchar *>(&subqueries)))
      return true; /* purecov: inspected */
  }

  /*
    Loop through eligible subqueries and see if we need the extra transform of
    implicit grouping into a separate derived table before we can
    transform the scalar subqueries to more derived tables.  But we
    cannot do this if we have a HAVING expression which references or contains
    a subquery.
    In that case, we throw in the towel and don't do any transformations. E.g.

    1. SELECT SUM(a), (SELECT SUM(b) FROM t3) scalar
       FROM t1
       HAVING SUM(a) > scalar;

    2. SELECT MAX(a)
       FROM t1
       WHERE FALSE
       HAVING (SELECT MIN(a) FROM t1) > 0;

   TODO: we could solve this by not moving the HAVING condition into the
   derived table, but instead letting it remain in the transformed block as a
   WHERE predicate, e.g. in the case of example 1:

     SELECT derived0.summ, derived1.scalar
     FROM (SELECT SUM(a) AS summ FROM t1) AS derived0
           LEFT JOIN
           (SELECT SUM(b) AS scalar FROM t3) AS derived1
           ON TRUE
     WHERE derived0.sum > derived1.scalar;

   but this is not yet done.
  */
  if (is_implicitly_grouped()) {
    bool need_new_outer = false;
    for (auto subquery : subqueries.m_list) {
      auto *subq = subquery.item;
      if (!query_block_contains_subquery(this, subq->query_expr())) continue;

      // Possibly contradicting requirements
      // (1) Subquery is in SELECT list: new_outer
      // (2) No new outer possible if HAVING contains subquery
      if (subquery.m_location & Item::Collect_scalar_subquery_info::L_SELECT) {
        need_new_outer = true;
      }
      if (subquery.m_location & Item::Collect_scalar_subquery_info::L_HAVING)
        return false;
    }

    if (need_new_outer) {
      /*
        In this case, the default transform with a single new derived table
        and a LEFT OUTER JOIN isn't always correct - we need to first move the
        aggregated query to a new derived subquery before we can transform the
        scalar subqueries to other derived tables.
      */
      bool break_off = false;
      if (transform_grouped_to_derived(thd, &break_off)) return true;
      if (break_off) return false;  // skip transformation
    }
  }

  /*
    Loop through eligible subqueries and transform them to derived tables
    and replace occurrences in expression trees with a field of the relevant
    derived table.
  */
  for (auto subquery : subqueries.m_list) {
    Item_singlerow_subselect *const subq = subquery.item;
    Query_expression *const subs_query_expression = subq->query_expr();

    /*
      [1] A reference to a scalar subquery from another query expression can
          happen. We can't transform it here, but it may be replaced from
          another query block.
      [2] A constant scalar subquery will be evaluated at prepare time
    */
    if (!query_block_contains_subquery(this, subs_query_expression) ||  // [1]
        (subq->const_item() && subs_query_expression->is_optimized()))  // [2]
      continue;

    Table_ref *tl;

    // Do we need a run-time cardinality check?
    bool needs_cardinality_check = !subquery.m_implicitly_grouped_and_no_union;

    Item *lifted_where = nullptr;
    if (subquery.m_correlation_map != 0) {
      // We have a correlated subquery. Check if we can handle it or not (only
      // applicable for subqueries without set operations)
      if (!subs_query_expression->is_set_operation()) {
        if (subs_query_expression->first_query_block()
                ->supported_correlated_scalar_subquery(thd, &subquery,
                                                       &lifted_where))
          return true;
        if (lifted_where == nullptr) continue;
      } else
        continue;
      // Since we have a correlated subquery, we will use GROUP BY to
      // materialize so, we do not expect a single row result set. For
      // correlated scalar subquery, we use another run-time check.
      needs_cardinality_check = false;
    }
    // Create a derived table for the subquery and nest it. If we found the
    // subquery outside of a join condition, we simply nest it at the end
    // with a LEFT OUTER .. ON TRUE, e.g.
    //
    // SELECT (SELECT COUNT(a) FROM t2) + a FROM t1;
    // ->
    // SELECT derived.cnt + t1.a FROM
    //   t1 LEFT OUTER JOIN
    //   (select COUNT(a) AS cnt FROM t2) AS derived
    // ON TRUE;
    //
    // If we have a subquery inside a join condition we nest it after the
    // outer table:
    //
    // SELECT * FROM t1 LEFT JOIN
    //               t2
    //             ON (SELECT COUNT(a) AS cnt FROM t2) = t1.a;
    // ->
    // SELECT * FROM t1 LEFT JOIN
    //               (SELECT COUNT(t2.a) AS cnt
    //                FROM t2) derived_1_0
    //             ON(TRUE) LEFT JOIN
    //               t2
    //             ON derived_1_0.cnt = t1.a
    //
    if (transform_subquery_to_derived(thd, &tl, subs_query_expression, subq,
                                      /*use_inner_join=*/false,
                                      needs_cardinality_check,
                                      subquery.m_join_condition, lifted_where))
      return true;

    /*
      Replace the subquery with a field in the materialized tmp table
      in WHERE, JOIN conditions, HAVING clause or SELECT expressions (could be
      optimized by keeping track in which expression the subquery was found)
    */

    // Replace in WHERE clause?
    if (subquery.m_location & Item::Collect_scalar_subquery_info::L_WHERE) {
      if (*where_expr_p != nullptr &&
          replace_subquery_in_expr(thd, &subquery, tl, where_expr_p))
        return true; /* purecov: inspected */
    }

    // Replace in join conditions?
    if (subquery.m_location & Item::Collect_scalar_subquery_info::L_JOIN_COND) {
      if (walk_join_conditions(
              m_table_nest,
              [&](Item **expr_p) mutable -> bool {
                subqueries.m_location =
                    Item::Collect_scalar_subquery_info::L_JOIN_COND;
                if (*expr_p != nullptr &&
                    replace_subquery_in_expr(thd, &subquery, tl, expr_p))
                  return true; /* purecov: inspected */
                return false;
              },
              &subqueries))
        return true; /* purecov: inspected */
    }

    size_t old_size;
    do {
      old_size = fields.size();
      for (Item *&select_expr : fields) {
        // At this time, expression could be wrapped in a rollup group
        // wrapper. It is the inner item of the rollup group item that
        // gets replaced. We take care to retain the rollup wrappers.
        Item *prev_value = unwrap_rollup_group(select_expr);
        if (replace_subquery_in_expr(thd, &subquery, tl, &select_expr))
          return true;
        Item *unwrapped_select_expr = unwrap_rollup_group(select_expr);
        if (unwrapped_select_expr != prev_value) {
          replace_referenced_item(prev_value, unwrapped_select_expr);
        }
        if (fields.size() != old_size) {
          // The (implicit) iterator over fields has been invalidated,
          // probably due to a call to split_sum_func(), so we cannot
          // iterate any further. The simplest fix is just restarting
          // the loop, as it is idempotent.
          break;
        }
      }
    } while (old_size != fields.size());

    // Replace in HAVING clause?
    if (subquery.m_location & (Item::Collect_scalar_subquery_info::L_HAVING)) {
      if (*having_expr_p != nullptr &&
          replace_subquery_in_expr(thd, &subquery, tl, having_expr_p))
        return true; /* purecov: inspected */
    }

    // A subquery in the SELECT list can be present in the GROUP BY clause
    // so we potentially need to replace there too.
    for (ORDER *ord = group_list.first; ord != nullptr; ord = ord->next) {
      if (replace_subquery_in_expr(thd, &subquery, tl, ord->item)) return true;
    }

    OPT_TRACE_TRANSFORM(
        &thd->opt_trace, trace_wrapper, trace_object,
        tl->derived_query_expression()->first_query_block()->select_number,
        "scalar subquery", "derived table");
    opt_trace_print_expanded_query(thd, this, &trace_object);
  }

  return false;
}

bool Query_block::lift_fulltext_from_having_to_select_list(THD *thd) {
  Item *having_cond = m_having_cond;
  if (having_cond == nullptr) return false;

  Prealloced_array<Item **, 8> refs_to_fulltext(PSI_NOT_INSTRUMENTED);

  // Add all full-text search calls as hidden elements of the SELECT list, if
  // they are not already there.
  if (WalkItem(having_cond, enum_walk::PREFIX | enum_walk::POSTFIX,
               NonAggregatedFullTextSearchVisitor(
                   [this, thd, &refs_to_fulltext](Item_func_match *item) {
                     const auto it = find(fields.begin(), fields.end(), item);
                     Item **ref =
                         it != fields.end() ? &*it : add_hidden_item(item);
                     // The above is sufficient for the hypergraph optimizer.
                     // The old optimizer additionally needs to have references
                     // from the HAVING clause to the corresponding elements in
                     // the SELECT list, so that it knows that it should read
                     // results from a temporary table instead of evaluating the
                     // expressions if they have been materialized. So we wrap
                     // these items in an Item_ref later.
                     if (!thd->lex->using_hypergraph_optimizer()) {
                       return refs_to_fulltext.push_back(ref);
                     }
                     return false;
                   }))) {
    return true;
  }

  // Add Item_ref indirection in the old optimizer.
  for (Item **item_to_replace : refs_to_fulltext) {
    assert(!thd->lex->using_hypergraph_optimizer());
    having_cond = TransformItem(having_cond, [&](Item *sub_item) -> Item * {
      if (sub_item == *item_to_replace) {
        return new (thd->mem_root)
            Item_ref(&context, item_to_replace, "<fulltext>");
      } else {
        return sub_item;
      }
    });
    if (having_cond == nullptr) return true;
  }

  // The MATCH calls are always wrapped in other functions, since non-boolean
  // predicates in HAVING are made complete. The topmost Item should therefore
  // never be changed in the above calls to TransformItem().
  assert(having_cond == m_having_cond);
  return false;
}

/**
  @} (end of group Query_Resolver)
*/

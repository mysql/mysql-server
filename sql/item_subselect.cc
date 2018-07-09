/* Copyright (c) 2002, 2018, Oracle and/or its affiliates. All rights reserved.

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
  subselect Item

*/

#include "sql/item_subselect.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <memory>
#include <utility>

#include "decimal.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_pointer_arithmetic.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/basic_row_iterators.h"
#include "sql/check_stack.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/debug_sync.h"   // DEBUG_SYNC
#include "sql/derror.h"       // ER_THD
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"  // Item_sum_max
#include "sql/key.h"
#include "sql/my_decimal.h"
#include "sql/mysqld.h"  // in_left_expr_name
#include "sql/nested_join.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"  // OPT_TRACE_TRANSFORM
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_nodes.h"  // PT_subquery
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/records.h"
#include "sql/row_iterator.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_join_buffer.h"  // JOIN_CACHE
#include "sql/sql_lex.h"          // SELECT_LEX
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_test.h"       // print_where
#include "sql/sql_tmp_table.h"  // free_tmp_table
#include "sql/sql_union.h"      // Query_result_union
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"
#include "sql/temp_table_param.h"
#include "sql/thr_malloc.h"
#include "sql/window.h"
#include "sql_string.h"
#include "template_utils.h"
#include "thr_lock.h"

class Json_wrapper;

Item_subselect::Item_subselect()
    : Item_result_field(),
      value_assigned(false),
      traced_before(false),
      substitution(NULL),
      in_cond_of_tab(NO_PLAN_IDX),
      engine(NULL),
      old_engine(NULL),
      used_tables_cache(0),
      have_to_be_excluded(false),
      changed(false) {
  set_subquery();
  reset();
  /*
    Item value is NULL if Query_result_interceptor didn't change this value
    (i.e. some rows will be found returned)
  */
  null_value = true;
}

Item_subselect::Item_subselect(const POS &pos)
    : super(pos),
      value_assigned(false),
      traced_before(false),
      substitution(NULL),
      in_cond_of_tab(NO_PLAN_IDX),
      engine(NULL),
      old_engine(NULL),
      used_tables_cache(0),
      have_to_be_excluded(false),
      changed(false) {
  set_subquery();
  reset();
  /*
    Item value is NULL if Query_result_interceptor didn't change this value
    (i.e. some rows will be found returned)
  */
  null_value = true;
}

void Item_subselect::init(SELECT_LEX *select_lex,
                          Query_result_subquery *result) {
  /*
    Please see Item_singlerow_subselect::invalidate_and_restore_select_lex(),
    which depends on alterations to the parse tree implemented here.
  */

  DBUG_ENTER("Item_subselect::init");
  DBUG_PRINT("enter", ("select_lex: %p", select_lex));
  unit = select_lex->master_unit();

  if (unit->item) {
    /*
      Item can be changed in SELECT_LEX::prepare while engine in
      JOIN::optimize => we do not copy old_engine here
    */
    engine = unit->item->engine;
    parsing_place = unit->item->parsing_place;
    unit->item->engine = 0;
    unit->item = this;
    engine->change_query_result(this, result);
  } else {
    SELECT_LEX *outer_select = unit->outer_select();
    /*
      do not take into account expression inside aggregate functions because
      they can access original table fields
    */
    parsing_place =
        (outer_select->in_sum_expr ? CTX_NONE : outer_select->parsing_place);
    if (unit->is_union() || unit->fake_select_lex)
      engine = new (*THR_MALLOC) subselect_union_engine(unit, result, this);
    else
      engine = new (*THR_MALLOC)
          subselect_single_select_engine(select_lex, result, this);
  }
  {
    SELECT_LEX *upper = unit->outer_select();
    if (upper->parsing_place == CTX_HAVING) upper->subquery_in_having = 1;
  }
  DBUG_VOID_RETURN;
}

/**
  Accumulate missing used_tables information from embedded query expression
  into the subquery.
  This function relies on a few other functions to accumulate information:
    accumulate_expression(), accumulate_condition(), accumulate_join_condition()

  Currently, the only property that is accumulated is INNER_TABLE_BIT.
  Information about local tables and outer references are accumulated in
  mark_as_dependent() (@see item.cc).
  RAND_TABLE_BIT is currently not accumulated (but uncacheable is used instead).

  @todo - maybe_null is not set properly for all types of subqueries and
          expressions. Use this sketch as a guideline for further handling:

  - When constructing an Item_subselect, maybe_null is false and null_value
    is true. This is obviously wrong.

  - When constructing an Item_in_subselect (subclass of Item_subselect),
    maybe_null is set true and null_value is set false.

  We should probably keep both maybe_null and null_value as false in
  the constructor. Then, set maybe_null during preparation, according to
  type of subquery:

  - Scalar subquery is nullable when query block may have an empty result (not
    DUAL or implicitly grouped).

  - Scalar subquery is nullable when one of the selected expressions
    are nullable.

  - Scalar subquery is nullable when WHERE clause or HAVING clause is non-empty
    and not always true.

  - EXISTS subquery is never nullable!

  - IN subquery nullability ignores subquery cardinality.

  - IN subquery is nullable when one of the selected expressions are nullable.

  - UNIONed query blocks may cancel out nullability.

*/
void Item_subselect::accumulate_properties() {
  for (SELECT_LEX *select = unit->first_select(); select != NULL;
       select = select->next_select())
    accumulate_properties(select);

  if (unit->fake_select_lex != NULL) {
    /*
      This query block may only contain components with special table
      dependencies in the ORDER BY clause, so inspect these expressions only.
      (The SELECT list may contain table references that are valid only in
       a local scope - references to the UNION temporary table - and should
       not be propagated to the subquery level.)
    */
    for (ORDER *order = unit->fake_select_lex->order_list.first; order != NULL;
         order = order->next)
      accumulate_condition(*order->item);
  }
}

/**
  Accumulate missing used_tables information for a query block.

  @param select Reference to query block
*/
void Item_subselect::accumulate_properties(SELECT_LEX *select) {
  List_iterator<Item> li(select->item_list);
  Item *item;
  while ((item = li++)) accumulate_expression(item);

  if (select->where_cond()) accumulate_condition(select->where_cond());

  if (select->join_list) accumulate_join_condition(select->join_list);

  for (ORDER *group = select->group_list.first; group; group = group->next)
    accumulate_condition(*group->item);

  if (select->having_cond()) accumulate_condition(select->having_cond());

  for (ORDER *order = select->order_list.first; order; order = order->next)
    accumulate_expression(*order->item);
  if (select->table_list.elements) used_tables_cache |= INNER_TABLE_BIT;

  List_iterator<Window> wi(select->m_windows);
  Window *w;
  while ((w = wi++)) {
    for (ORDER *wp = w->first_partition_by(); wp != NULL; wp = wp->next)
      accumulate_expression(*wp->item);
    for (ORDER *wo = w->first_order_by(); wo != NULL; wo = wo->next)
      accumulate_expression(*wo->item);
  }
}

/**
  Accumulate used_tables information for an expression from a query block.

  @param item  Reference to expression.
*/
void Item_subselect::accumulate_expression(Item *item) {
  if (item->used_tables() & ~OUTER_REF_TABLE_BIT)
    used_tables_cache |= INNER_TABLE_BIT;
  maybe_null |= item->maybe_null;
}

/**
  Accumulate used_tables information for a condition from a query block.

  @param item  Reference to condition.
*/
void Item_subselect::accumulate_condition(Item *item) {
  if (item->used_tables() & ~OUTER_REF_TABLE_BIT)
    used_tables_cache |= INNER_TABLE_BIT;
}

/**
  Accumulate used_tables information for the join conditions from a query block.

  @param tables  References to joined tables.
*/
void Item_subselect::accumulate_join_condition(List<TABLE_LIST> *tables) {
  TABLE_LIST *table_ref;
  List_iterator<TABLE_LIST> li(*tables);

  while ((table_ref = li++)) {
    if (table_ref->join_cond()) accumulate_condition(table_ref->join_cond());

    if (table_ref->nested_join != NULL)
      accumulate_join_condition(&table_ref->nested_join->join_list);
  }
}

void Item_subselect::cleanup() {
  DBUG_ENTER("Item_subselect::cleanup");
  Item_result_field::cleanup();
  if (old_engine) {
    if (engine) {
      engine->cleanup();
      destroy(engine);
    }
    engine = old_engine;
    old_engine = 0;
  }
  if (engine) engine->cleanup();
  reset();
  value_assigned = 0;
  traced_before = false;
  in_cond_of_tab = NO_PLAN_IDX;
  DBUG_VOID_RETURN;
}

void Item_singlerow_subselect::cleanup() {
  DBUG_ENTER("Item_singlerow_subselect::cleanup");
  value = 0;
  row = 0;
  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
}

/**
  Decide whether to mark the injected left expression "outer" relative to
  the subquery. It should be marked as outer in the following cases:

  1) If the left expression is not constant.

  2) If the left expression could be a constant NULL and we care about the
  difference between UNKNOWN and FALSE. In this case, JOIN::optimize() for
  the subquery must be prevented from evaluating any triggered condition, as
  the triggers for such conditions have not yet been properly set by
  Item_in_optimizer::val_int(). By marking the left expression as outer, a
  triggered condition using it will not be considered constant, will not be
  evaluated by JOIN::optimize(); it will only be evaluated by JOIN::exec()
  which is called from Item_in_optimizer::val_int()

  3) If the left expression comes from a subquery and is not a basic
  constant. In this case, the value cannot be read until after the subquery
  has been evaluated. By marking it as outer, we prevent it from being read
  when JOIN::optimize() attempts to evaluate constant conditions.

  @param[in] left_row The item that represents the left operand of the IN
                      operator

  @param[in] col      The column number of the expression in the left operand
                      to possibly mark as dependant of the outer select

  @returns true if we should mark the injected left expression "outer"
                relative to the subquery
*/
bool Item_in_subselect::mark_as_outer(Item *left_row, size_t col) {
  const Item *left_col = left_row->element_index(col);
  return !left_col->const_item() || (!abort_on_null && left_col->maybe_null) ||
         (left_row->type() == SUBSELECT_ITEM && !left_col->basic_const_item());
}

bool Item_in_subselect::finalize_exists_transform(SELECT_LEX *select_lex) {
  DBUG_ASSERT(exec_method == EXEC_EXISTS_OR_MAT || exec_method == EXEC_EXISTS);
  /*
    Change
      SELECT expr1, expr2
    to
      SELECT 1,1
    because EXISTS does not care about the selected expressions, only about
    the existence of rows.

    If UNION, we have to modify the SELECT list of each SELECT in the
    UNION, fortunately this function is indeed called for each SELECT_LEX.

    If this is a prepared statement, we must allow the next execution to use
    materialization. So, we should back up the original SELECT list. If this
    is a UNION, this means backing up the N original SELECT lists. To
    avoid this constraint, we change the SELECT list only if this is not a
    prepared statement.
  */
  if (unit->thd->stmt_arena->is_regular())  // not prepared stmt
  {
    uint cnt = select_lex->item_list.elements;
    select_lex->item_list.empty();
    for (; cnt > 0; cnt--)
      select_lex->item_list.push_back(new Item_int(
          NAME_STRING("Not_used"), (longlong)1, MY_INT64_NUM_DECIMAL_DIGITS));
    Opt_trace_context *const trace = &unit->thd->opt_trace;
    OPT_TRACE_TRANSFORM(trace, oto0, oto1, select_lex->select_number,
                        "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
    oto1.add("put_1_in_SELECT_list", true);
  }
  /*
    Note that if the subquery is "SELECT1 UNION SELECT2" then this is not
    working optimally (Bug#14215895).
  */
  if (!(unit->global_parameters()->select_limit = new Item_int(1))) return true;

  if (unit->prepare_limit(unit->thd, unit->global_parameters()))
    return true; /* purecov: inspected */

  if (unit->set_limit(unit->thd, unit->global_parameters()))
    return true; /* purecov: inspected */

  select_lex->join->allow_outer_refs = true;  // for JOIN::set_prefix_tables()
  exec_method = EXEC_EXISTS;
  return false;
}

/*
  Removes every predicate injected by IN->EXISTS.

  This function is different from others:
  - it wants to remove all traces of IN->EXISTS (for
  materialization)
  - remove_subq_pushed_predicates() and remove_additional_cond() want to
  remove only the conditions of IN->EXISTS which index lookup already
  satisfies (they are just an optimization).

  @param conds  condition
  @returns      new condition
*/
Item *Item_in_subselect::remove_in2exists_conds(Item *conds) {
  if (conds->created_by_in2exists()) return NULL;
  if (conds->type() != Item::COND_ITEM) return conds;
  Item_cond *cnd = static_cast<Item_cond *>(conds);
  /*
    If IN->EXISTS has added something to 'conds', cnd must be AND list and we
    must inspect each member.
  */
  if (cnd->functype() != Item_func::COND_AND_FUNC) return conds;
  List_iterator<Item> li(*(cnd->argument_list()));
  Item *item;
  while ((item = li++)) {
    // remove() does not invalidate iterator.
    if (item->created_by_in2exists()) li.remove();
  }
  switch (cnd->argument_list()->elements) {
    case 0:
      return NULL;
    case 1:  // AND(x) is the same as x, return x
      return cnd->argument_list()->head();
    default:  // otherwise return AND
      return conds;
  }
}

bool Item_in_subselect::finalize_materialization_transform(JOIN *join) {
  DBUG_ASSERT(exec_method == EXEC_EXISTS_OR_MAT);
  DBUG_ASSERT(engine->engine_type() == subselect_engine::SINGLE_SELECT_ENGINE);
  THD *const thd = unit->thd;
  subselect_single_select_engine *old_engine_derived =
      static_cast<subselect_single_select_engine *>(engine);

  DBUG_ASSERT(join == old_engine_derived->select_lex->join);
  // No UNION in materialized subquery so this holds:
  DBUG_ASSERT(join->select_lex == unit->first_select());
  DBUG_ASSERT(join->unit == unit);
  DBUG_ASSERT(unit->global_parameters()->select_limit == NULL);

  exec_method = EXEC_MATERIALIZATION;

  /*
    We need to undo several changes which IN->EXISTS had done. But we first
    back them up, so that the next execution of the statement is allowed to
    choose IN->EXISTS.
  */

  /*
    Undo conditions injected by IN->EXISTS.
    Condition guards, which those conditions maybe used, are not needed
    anymore.
    Subquery becomes 'not dependent' again, as before IN->EXISTS.
  */
  if (join->where_cond)
    join->where_cond = remove_in2exists_conds(join->where_cond);
  if (join->having_cond)
    join->having_cond = remove_in2exists_conds(join->having_cond);
  DBUG_ASSERT(!in2exists_info->dependent_before);
  join->select_lex->uncacheable &= ~UNCACHEABLE_DEPENDENT;
  unit->uncacheable &= ~UNCACHEABLE_DEPENDENT;

  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1,
                      old_engine_derived->select_lex->select_number,
                      "IN (SELECT)", "materialization");
  oto1.add("chosen", true);

  subselect_hash_sj_engine *const new_engine =
      new (*THR_MALLOC) subselect_hash_sj_engine(this, old_engine_derived);
  if (!new_engine) return true;
  if (new_engine->setup(unit->get_unit_column_types())) {
    /*
      For some reason we cannot use materialization for this IN predicate.
      Delete all materialization-related objects, and return error.
    */
    new_engine->cleanup();
    destroy(new_engine);
    return true;
  }
  if (change_engine(new_engine)) return true;

  join->allow_outer_refs = false;  // for JOIN::set_prefix_tables()
  return false;
}

void Item_in_subselect::cleanup() {
  DBUG_ENTER("Item_in_subselect::cleanup");
  if (left_expr_cache) {
    left_expr_cache->destroy_elements();
    destroy(left_expr_cache);
    left_expr_cache = NULL;
  }
  left_expr_cache_filled = false;
  need_expr_cache = true;

  switch (exec_method) {
    case EXEC_MATERIALIZATION:
      if (in2exists_info->dependent_after) {
        unit->first_select()->uncacheable |= UNCACHEABLE_DEPENDENT;
        unit->uncacheable |= UNCACHEABLE_DEPENDENT;
      }
      // fall through
    case EXEC_EXISTS:
      /*
        Back to EXISTS_OR_MAT, so that next execution of this statement can
        choose between the two.
      */
      unit->global_parameters()->select_limit = NULL;
      exec_method = EXEC_EXISTS_OR_MAT;
      break;
    default:
      break;
  }

  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
}

Item_subselect::~Item_subselect() { destroy(engine); }

bool Item_subselect::fix_fields(THD *thd, Item **ref) {
  char const *save_where = thd->where;
  uint8 uncacheable;
  bool res;

  DBUG_ASSERT(fixed == 0);
  /*
    Pointers to THD must match. unit::thd may vary over the lifetime of the
    item (for example triggers, and thus their Item-s, are in a cache shared
    by all connections), but reinit_stmt_before_use() keeps it up-to-date,
    which we check here. subselect_union_engine functions also do sanity
    checks.
  */
  DBUG_ASSERT(thd == unit->thd);
#ifndef DBUG_OFF
  // Engine accesses THD via its 'item' pointer, check it:
  DBUG_ASSERT(engine->get_item() == this);
#endif

  engine->set_thd_for_result();

  if (check_stack_overrun(thd, STACK_MIN_SIZE, (uchar *)&res)) return true;

  if (!(res = engine->prepare())) {
    // all transformation is done (used by prepared statements)
    changed = 1;

    // Accumulate properties referring to "inner tables"
    accumulate_properties();

    /*
      Substitute the current item with an Item_in_optimizer that was
      created by Item_in_subselect::select_in_like_transformer and
      call fix_fields for the substituted item which in turn calls
      engine->prepare for the subquery predicate.
    */
    if (substitution) {
      int ret = 0;
      (*ref) = substitution;
      substitution->item_name = item_name;
      if (have_to_be_excluded) engine->exclude();
      substitution = 0;
      thd->where = "checking transformed subquery";
      if (!(*ref)->fixed) ret = (*ref)->fix_fields(thd, ref);
      thd->where = save_where;
      return ret;
    }
    // Is it one field subselect?
    if (engine->cols() > max_columns) {
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
      return true;
    }
    if (resolve_type(thd)) goto err;
  } else
    goto err;

  if ((uncacheable = engine->uncacheable())) {
    if (uncacheable & UNCACHEABLE_RAND) used_tables_cache |= RAND_TABLE_BIT;
  }

  /*
    If this subquery references window functions, per the SQL standard they
    are aggregated in the subquery's query block, and never outside of it, so:
  */
  DBUG_ASSERT(!has_wf());

  fixed = 1;

err:
  thd->where = save_where;
  return res;
}

/**
  Apply walk() processor to join conditions.

  JOINs may be nested. Walk nested joins recursively to apply the
  processor.
*/
static bool walk_join_condition(List<TABLE_LIST> *tables,
                                Item_processor processor, Item::enum_walk walk,
                                uchar *arg) {
  TABLE_LIST *table;
  List_iterator<TABLE_LIST> li(*tables);

  while ((table = li++)) {
    if (table->join_cond() && table->join_cond()->walk(processor, walk, arg))
      return true;

    if (table->nested_join != NULL &&
        walk_join_condition(&table->nested_join->join_list, processor, walk,
                            arg))
      return true;
  }
  return false;
}

/**
  Workaround for bug in gcc 4.1.
  @see Item_in_subselect::walk()
*/
bool Item_subselect::walk_body(Item_processor processor, enum_walk walk,
                               uchar *arg) {
  if ((walk & WALK_PREFIX) && (this->*processor)(arg)) return true;

  if (walk & WALK_SUBQUERY) {
    for (SELECT_LEX *lex = unit->first_select(); lex;
         lex = lex->next_select()) {
      List_iterator<Item> li(lex->item_list);
      Item *item;
      ORDER *order;

      while ((item = li++)) {
        if (item->walk(processor, walk, arg)) return true;
      }

      if (lex->join_list != NULL &&
          walk_join_condition(lex->join_list, processor, walk, arg))
        return true;

      // @todo: Roy thinks that we should always use lex->where_cond.
      Item *const where_cond = (lex->join && lex->join->is_optimized())
                                   ? lex->join->where_cond
                                   : lex->where_cond();

      if (where_cond && where_cond->walk(processor, walk, arg)) return true;

      for (order = lex->group_list.first; order; order = order->next) {
        if ((*order->item)->walk(processor, walk, arg)) return true;
      }

      if (lex->having_cond() && lex->having_cond()->walk(processor, walk, arg))
        return true;

      for (order = lex->order_list.first; order; order = order->next) {
        if ((*order->item)->walk(processor, walk, arg)) return true;
      }

      // walk windows' ORDER BY and PARTITION BY clauses.
      List_iterator<Window> liw(lex->m_windows);
      for (Window *w = liw++; w != nullptr; w = liw++) {
        /*
          We use first_order_by() instead of order() because if a window
          references another window and they thus share the same ORDER BY,
          we want to walk that clause only once here
          (Same for partition as well)".
        */
        for (auto it : {w->first_partition_by(), w->first_order_by()}) {
          if (it != nullptr) {
            for (ORDER *o = it; o != nullptr; o = o->next) {
              if ((*o->item)->walk(processor, walk, arg)) return true;
            }
          }
        }
      }
    }
  }

  return (walk & WALK_POSTFIX) && (this->*processor)(arg);
}

bool Item_subselect::walk(Item_processor processor, enum_walk walk,
                          uchar *arg) {
  return walk_body(processor, walk, arg);
}

/**
  Register subquery to the table where it is used within a condition.

  @param arg    qep_row to which the subquery belongs

  @retval false

  @note We always return "false" as far as we don't want to dive deeper because
        we explain inner subqueries in their joins contexts.
*/

bool Item_subselect::explain_subquery_checker(uchar **arg) {
  qep_row *qr = reinterpret_cast<qep_row *>(*arg);

  qr->register_where_subquery(unit);
  return false;
}

bool Item_subselect::exec() {
  DBUG_ENTER("Item_subselect::exec");
  /*
    Do not execute subselect in case of a fatal error
    or if the query has been killed.
  */
  THD *const thd = unit->thd;
  if (thd->is_error() || thd->killed) DBUG_RETURN(true);

  // No subqueries should be evaluated when analysing a view
  DBUG_ASSERT(!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW));
  /*
    Simulate a failure in sub-query execution. Used to test e.g.
    out of memory or query being killed conditions.
  */
  DBUG_EXECUTE_IF("subselect_exec_fail", DBUG_RETURN(true););

  /*
    Disable tracing of subquery execution if
    1) this is not the first time the subselect is executed, and
    2) REPEATED_SUBSELECT is disabled
  */
  Opt_trace_context *const trace = &thd->opt_trace;
  const bool disable_trace =
      traced_before &&
      !trace->feature_enabled(Opt_trace_context::REPEATED_SUBSELECT);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);
  traced_before = true;

  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "subselect_execution");
  trace_exec.add_select_number(unit->first_select()->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  // Statements like DO and SET may still rely on lazy optimization
  if (!unit->is_optimized() && unit->optimize(thd)) DBUG_RETURN(true);
  bool res = engine->exec();

  DBUG_RETURN(res);
}

/**
  Fix used tables information for a subquery after query transformations.
  Common actions for all predicates involving subqueries.
  Most actions here involve re-resolving information for conditions
  and items belonging to the subquery.
  Notice that the usage information from underlying expressions is not
  propagated to the subquery predicate, as it belongs to inner layers
  of the query operator structure.
  However, when underlying expressions contain outer references into
  a select_lex on this level, the relevant information must be updated
  when these expressions are resolved.
*/

void Item_subselect::fix_after_pullout(SELECT_LEX *parent_select,
                                       SELECT_LEX *removed_select)

{
  /* Clear usage information for this subquery predicate object */
  used_tables_cache = 0;

  /*
    Go through all query specification objects of the subquery and re-resolve
    all relevant expressions belonging to them.
  */
  for (SELECT_LEX *sel = unit->first_select(); sel; sel = sel->next_select()) {
    if (sel->where_cond())
      sel->where_cond()->fix_after_pullout(parent_select, removed_select);

    if (sel->having_cond())
      sel->having_cond()->fix_after_pullout(parent_select, removed_select);

    List_iterator<Item> li(sel->item_list);
    Item *item;
    while ((item = li++))
      item->fix_after_pullout(parent_select, removed_select);

    /*
      No need to call fix_after_pullout() for outer-join conditions, as these
      cannot have outer references.
    */

    /* Re-resolve ORDER BY and GROUP BY fields */

    for (ORDER *order = sel->order_list.first; order; order = order->next)
      (*order->item)->fix_after_pullout(parent_select, removed_select);

    for (ORDER *group = sel->group_list.first; group; group = group->next)
      (*group->item)->fix_after_pullout(parent_select, removed_select);
  }
  // Accumulate properties like INNER_TABLE_BIT
  accumulate_properties();
}

bool Item_in_subselect::walk(Item_processor processor, enum_walk walk,
                             uchar *arg) {
  if (left_expr->walk(processor, walk, arg)) return true;
  /*
    Cannot call "Item_subselect::walk(...)" because with gcc 4.1
    Item_in_subselect::walk() was incorrectly called instead.
    Using Item_subselect::walk_body() instead is a workaround.
  */
  return walk_body(processor, walk, arg);
}

/*
  Compute the IN predicate if the left operand's cache changed.
*/

bool Item_in_subselect::exec() {
  DBUG_ENTER("Item_in_subselect::exec");
  DBUG_ASSERT(exec_method != EXEC_MATERIALIZATION ||
              (exec_method == EXEC_MATERIALIZATION &&
               engine->engine_type() == subselect_engine::HASH_SJ_ENGINE));
  /*
    Initialize the cache of the left predicate operand. This has to be done as
    late as now, because Cached_item directly contains a resolved field (not
    an item, and in some cases (when temp tables are created), these fields
    end up pointing to the wrong field. One solution is to change Cached_item
    to not resolve its field upon creation, but to resolve it dynamically
    from a given Item_ref object.
    Do not init the cache if a previous execution decided that it is not needed.
    TODO: the cache should be applied conditionally based on:
    - rules - e.g. only if the left operand is known to be ordered, and/or
    - on a cost-based basis, that takes into account the cost of a cache
      lookup, the cache hit rate, and the savings per cache hit.
  */
  if (need_expr_cache && !left_expr_cache &&
      exec_method == EXEC_MATERIALIZATION && init_left_expr_cache())
    DBUG_RETURN(true);

  if (left_expr_cache != NULL) {
    const int result = update_item_cache_if_changed(*left_expr_cache);
    if (left_expr_cache_filled &&  // cache was previously filled
        result < 0)  // new value is identical to previous cached value
    {
      /*
        We needn't do a full execution, can just reuse "value", "was_null",
        "null_value" of the previous execution.
      */
      DBUG_RETURN(false);
    }
    left_expr_cache_filled = true;
  }

  if (unit->is_executed() && engine->uncacheable())
    null_value = was_null = false;
  const bool retval = Item_subselect::exec();
  DBUG_RETURN(retval);
}

Item::Type Item_subselect::type() const { return SUBSELECT_ITEM; }

bool Item_subselect::resolve_type(THD *) {
  engine->fix_length_and_dec(0);
  return false;
}

Item *Item_subselect::get_tmp_table_item(THD *thd_arg) {
  DBUG_ENTER("Item_subselect::get_tmp_table_item");
  if (!has_aggregation() && !const_item()) {
    Item *result = new Item_field(result_field);
    DBUG_RETURN(result);
  }
  Item *result = copy_or_same(thd_arg);
  DBUG_RETURN(result);
}

void Item_subselect::update_used_tables() {
  // did all used tables become const?
  if (!engine->uncacheable())
    used_tables_cache &= ~engine->upper_select_const_tables();
}

void Item_subselect::print(String *str, enum_query_type query_type) {
  if (engine) {
    str->append('(');
    engine->print(str, query_type);
    str->append(')');
  } else
    str->append("(...)");
}

/* Single value subselect interface class */
class Query_result_scalar_subquery : public Query_result_subquery {
 public:
  Query_result_scalar_subquery(THD *thd, Item_subselect *item_arg)
      : Query_result_subquery(thd, item_arg) {}
  bool send_data(List<Item> &items);
};

bool Query_result_scalar_subquery::send_data(List<Item> &items) {
  DBUG_ENTER("Query_result_scalar_subquery::send_data");
  Item_singlerow_subselect *it = (Item_singlerow_subselect *)item;
  if (it->assigned()) {
    my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
    DBUG_RETURN(true);
  }
  List_iterator_fast<Item> li(items);
  Item *val_item;
  for (uint i = 0; (val_item = li++); i++) it->store(i, val_item);
  if (thd->is_error()) DBUG_RETURN(true);

  it->assigned(true);
  DBUG_RETURN(false);
}

Item_singlerow_subselect::Item_singlerow_subselect(SELECT_LEX *select_lex)
    : Item_subselect(), value(0), no_rows(false) {
  DBUG_ENTER("Item_singlerow_subselect::Item_singlerow_subselect");
  init(select_lex,
       new (*THR_MALLOC) Query_result_scalar_subquery(current_thd, this));
  maybe_null = 1;  // if the subquery is empty, value is NULL
  max_columns = UINT_MAX;
  DBUG_VOID_RETURN;
}

SELECT_LEX *Item_singlerow_subselect::invalidate_and_restore_select_lex() {
  DBUG_ENTER("Item_singlerow_subselect::invalidate_and_restore_select_lex");
  SELECT_LEX *result = unit->first_select();

  DBUG_ASSERT(result);

  /*
    This code restore the parse tree in it's state before the execution of
    Item_singlerow_subselect::Item_singlerow_subselect(),
    and in particular decouples this object from the SELECT_LEX,
    so that the SELECT_LEX can be used with a different flavor
    or Item_subselect instead, as part of query rewriting.
  */
  unit->item = NULL;

  DBUG_RETURN(result);
}

/* used in independent ALL/ANY optimisation */
class Query_result_max_min_subquery final : public Query_result_subquery {
  Item_cache *cache;
  bool (Query_result_max_min_subquery::*op)();
  bool fmax;
  /**
    If ignoring NULLs, comparisons will skip NULL values. If not
    ignoring NULLs, the first (if any) NULL value discovered will be
    returned as the maximum/minimum value.
  */
  bool ignore_nulls;

 public:
  Query_result_max_min_subquery(THD *thd, Item_subselect *item_arg, bool mx,
                                bool ignore_nulls)
      : Query_result_subquery(thd, item_arg),
        cache(0),
        fmax(mx),
        ignore_nulls(ignore_nulls) {}
  void cleanup() override;
  bool send_data(List<Item> &items) override;

 private:
  bool cmp_real();
  bool cmp_int();
  bool cmp_decimal();
  bool cmp_str();
};

void Query_result_max_min_subquery::cleanup() {
  DBUG_ENTER("Query_result_max_min_subquery::cleanup");
  cache = 0;
  DBUG_VOID_RETURN;
}

bool Query_result_max_min_subquery::send_data(List<Item> &items) {
  DBUG_ENTER("Query_result_max_min_subquery::send_data");
  Item_maxmin_subselect *it = (Item_maxmin_subselect *)item;
  List_iterator_fast<Item> li(items);
  Item *val_item = li++;
  it->register_value();
  if (it->assigned()) {
    cache->store(val_item);
    if ((this->*op)()) it->store(0, cache);
  } else {
    if (!cache) {
      cache = Item_cache::get_cache(val_item);
      switch (val_item->result_type()) {
        case REAL_RESULT:
          op = &Query_result_max_min_subquery::cmp_real;
          break;
        case INT_RESULT:
          op = &Query_result_max_min_subquery::cmp_int;
          break;
        case STRING_RESULT:
          op = &Query_result_max_min_subquery::cmp_str;
          break;
        case DECIMAL_RESULT:
          op = &Query_result_max_min_subquery::cmp_decimal;
          break;
        case ROW_RESULT:
        case INVALID_RESULT:
          // This case should never be choosen
          DBUG_ASSERT(0);
          op = 0;
      }
    }
    cache->store(val_item);
    it->store(0, cache);
  }
  it->assigned(true);
  DBUG_RETURN(0);
}

/**
  Compare two floating point numbers for MAX or MIN.

  Compare two numbers and decide if the number should be cached as the
  maximum/minimum number seen this far. If fmax==true, this is a
  comparison for MAX, otherwise it is a comparison for MIN.

  val1 is the new numer to compare against the current
  maximum/minimum. val2 is the current maximum/minimum.

  ignore_nulls is used to control behavior when comparing with a NULL
  value. If ignore_nulls==false, the behavior is to store the first
  NULL value discovered (i.e, return true, that it is larger than the
  current maximum) and never replace it. If ignore_nulls==true, NULL
  values are not stored. ANY subqueries use ignore_nulls==true, ALL
  subqueries use ignore_nulls==false.

  @retval true if the new number should be the new maximum/minimum.
  @retval false if the maximum/minimum should stay unchanged.
 */
bool Query_result_max_min_subquery::cmp_real() {
  Item *maxmin = ((Item_singlerow_subselect *)item)->element_index(0);
  double val1 = cache->val_real(), val2 = maxmin->val_real();
  /*
    If we're ignoring NULLs and the current maximum/minimum is NULL
    (must have been placed there as the first value iterated over) and
    the new value is not NULL, return true so that a new, non-NULL
    maximum/minimum is set. Otherwise, return false to keep the
    current non-NULL maximum/minimum.

    If we're not ignoring NULLs and the current maximum/minimum is not
    NULL, return true to store NULL. Otherwise, return false to keep
    the NULL we've already got.
  */
  if (cache->null_value || maxmin->null_value)
    return (ignore_nulls) ? !(cache->null_value) : !(maxmin->null_value);
  return (fmax) ? (val1 > val2) : (val1 < val2);
}

/**
  Compare two integer numbers for MAX or MIN.

  @see Query_result_max_min_subquery::cmp_real()
*/
bool Query_result_max_min_subquery::cmp_int() {
  Item *maxmin = ((Item_singlerow_subselect *)item)->element_index(0);
  longlong val1 = cache->val_int(), val2 = maxmin->val_int();
  if (cache->null_value || maxmin->null_value)
    return (ignore_nulls) ? !(cache->null_value) : !(maxmin->null_value);
  return (fmax) ? (val1 > val2) : (val1 < val2);
}

/**
  Compare two decimal numbers for MAX or MIN.

  @see Query_result_max_min_subquery::cmp_real()
*/
bool Query_result_max_min_subquery::cmp_decimal() {
  Item *maxmin = ((Item_singlerow_subselect *)item)->element_index(0);
  my_decimal cval, *cvalue = cache->val_decimal(&cval);
  my_decimal mval, *mvalue = maxmin->val_decimal(&mval);
  if (cache->null_value || maxmin->null_value)
    return (ignore_nulls) ? !(cache->null_value) : !(maxmin->null_value);
  return (fmax) ? (my_decimal_cmp(cvalue, mvalue) > 0)
                : (my_decimal_cmp(cvalue, mvalue) < 0);
}

/**
  Compare two strings for MAX or MIN.

  @see Query_result_max_min_subquery::cmp_real()
*/
bool Query_result_max_min_subquery::cmp_str() {
  String *val1, *val2, buf1, buf2;
  Item *maxmin = ((Item_singlerow_subselect *)item)->element_index(0);
  /*
    as far as both operand is Item_cache buf1 & buf2 will not be used,
    but added for safety
  */
  val1 = cache->val_str(&buf1);
  val2 = maxmin->val_str(&buf1);
  if (cache->null_value || maxmin->null_value)
    return (ignore_nulls) ? !(cache->null_value) : !(maxmin->null_value);
  return (fmax) ? (sortcmp(val1, val2, cache->collation.collation) > 0)
                : (sortcmp(val1, val2, cache->collation.collation) < 0);
}

Item_maxmin_subselect::Item_maxmin_subselect(THD *thd_param,
                                             Item_subselect *parent,
                                             SELECT_LEX *select_lex,
                                             bool max_arg, bool ignore_nulls)
    : Item_singlerow_subselect(), was_values(false) {
  DBUG_ENTER("Item_maxmin_subselect::Item_maxmin_subselect");
  max = max_arg;
  init(select_lex, new (*THR_MALLOC) Query_result_max_min_subquery(
                       thd_param, this, max_arg, ignore_nulls));
  max_columns = 1;
  maybe_null = 1;
  max_columns = 1;

  /*
    Following information was collected during performing fix_fields()
    of Items belonged to subquery, which will be not repeated
  */
  used_tables_cache = parent->used_tables();

  DBUG_VOID_RETURN;
}

void Item_maxmin_subselect::cleanup() {
  DBUG_ENTER("Item_maxmin_subselect::cleanup");
  Item_singlerow_subselect::cleanup();

  was_values = false;
  DBUG_VOID_RETURN;
}

void Item_maxmin_subselect::print(String *str, enum_query_type query_type) {
  str->append(max ? "<max>" : "<min>", 5);
  Item_singlerow_subselect::print(str, query_type);
}

void Item_singlerow_subselect::reset() {
  null_value = true;
  if (value) value->null_value = true;
}

/**
  @todo
  - We cant change name of Item_field or Item_ref, because it will
  prevent it's correct resolving, but we should save name of
  removed item => we do not make optimization if top item of
  list is field or reference.
  - switch off this optimization for prepare statement,
  because we do not rollback this changes.
  Make rollback for it, or special name resolving mode in 5.0.
*/
Item_subselect::trans_res Item_singlerow_subselect::select_transformer(
    SELECT_LEX *select) {
  DBUG_ENTER("Item_singlerow_subselect::select_transformer");
  if (changed) DBUG_RETURN(RES_OK);

  THD *const thd = unit->thd;
  SELECT_LEX *outer = select->outer_select();

  if (!unit->is_union() && !select->table_list.elements &&
      select->item_list.elements == 1 &&
      !select->item_list.head()->has_aggregation() &&
      !select->item_list.head()->has_wf() &&
      /*
        We cant change name of Item_field or Item_ref, because it will
        prevent it's correct resolving, but we should save name of
        removed item => we do not make optimization if top item of
        list is field or reference.
        TODO: Fix this when WL#6570 is implemented.
      */
      (select->item_list.head()->const_item() ||
       select->item_list.head()->type() == SUBSELECT_ITEM) &&
      !select->where_cond() && !select->having_cond() &&
      /*
        For prepared statement, a subquery (SELECT 1) in the GROUP BY
        list might be transformed into a constant integer, which is
        re-interpreted as a select expression number of later resolving.
        because we do not rollback this changes
        TODO: Fix this when WL#6570 is implemented.
      */
      !thd->stmt_arena->is_stmt_prepare_or_first_sp_execute()) {
    have_to_be_excluded = 1;
    if (thd->lex->is_explain()) {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER_THD(thd, ER_SELECT_REDUCED), select->select_number);
      push_warning(thd, Sql_condition::SL_NOTE, ER_SELECT_REDUCED, warn_buff);
    }
    substitution = select->item_list.head();
    if (substitution->type() == SUBSELECT_ITEM) {
      Item_subselect *subs = (Item_subselect *)substitution;
      subs->unit->set_explain_marker_from(unit);
    }
    // Merge subquery's name resolution contexts into parent's
    outer->merge_contexts(select);

    // Fix query block contexts after merging the subquery
    substitution->fix_after_pullout(outer, select);
    DBUG_RETURN(RES_REDUCE);
  }
  DBUG_RETURN(RES_OK);
}

void Item_singlerow_subselect::store(uint i, Item *item) {
  row[i]->store(item);
  row[i]->cache_value();
}

enum Item_result Item_singlerow_subselect::result_type() const {
  return engine->type();
}

bool Item_singlerow_subselect::resolve_type(THD *) {
  if ((max_columns = engine->cols()) == 1) {
    engine->fix_length_and_dec(row = &value);
  } else {
    if (!(row = (Item_cache **)sql_alloc(sizeof(Item_cache *) * max_columns)))
      return true;
    engine->fix_length_and_dec(row);
    value = *row;
  }
  set_data_type(engine->field_type());
  unsigned_flag = value->unsigned_flag;
  /*
    Check if NULL values may be returned by the subquery. Either
    because one or more of the columns could be NULL, or because the
    subquery could return an empty result.
  */
  maybe_null = engine->may_be_null();
  return false;
}

void Item_singlerow_subselect::no_rows_in_result() {
  /*
    This is only possible if we have a dependent subquery in the SELECT list
    and an aggregated outer query based on zero rows, which is an illegal query
    according to the SQL standard. ONLY_FULL_GROUP_BY rejects such queries.
  */
  if (unit->uncacheable & UNCACHEABLE_DEPENDENT) no_rows = true;
}

uint Item_singlerow_subselect::cols() const { return engine->cols(); }

bool Item_singlerow_subselect::check_cols(uint c) {
  if (c != engine->cols()) {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}

bool Item_singlerow_subselect::null_inside() {
  for (uint i = 0; i < max_columns; i++) {
    if (row[i]->null_value) return 1;
  }
  return 0;
}

void Item_singlerow_subselect::bring_value() {
  if (!exec() && assigned())
    null_value = 0;
  else
    reset();
}

double Item_singlerow_subselect::val_real() {
  DBUG_ASSERT(fixed == 1);
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_real();
  } else {
    reset();
    return 0;
  }
}

longlong Item_singlerow_subselect::val_int() {
  DBUG_ASSERT(fixed == 1);
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_int();
  } else {
    reset();
    return 0;
  }
}

String *Item_singlerow_subselect::val_str(String *str) {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_str(str);
  } else {
    reset();
    return 0;
  }
}

my_decimal *Item_singlerow_subselect::val_decimal(my_decimal *decimal_value) {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_decimal(decimal_value);
  } else {
    reset();
    return 0;
  }
}

bool Item_singlerow_subselect::val_json(Json_wrapper *result) {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_json(result);
  } else {
    reset();
    return current_thd->is_error();
  }
}

bool Item_singlerow_subselect::get_date(MYSQL_TIME *ltime,
                                        my_time_flags_t fuzzydate) {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->get_date(ltime, fuzzydate);
  } else {
    reset();
    return true;
  }
}

bool Item_singlerow_subselect::get_time(MYSQL_TIME *ltime) {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->get_time(ltime);
  } else {
    reset();
    return true;
  }
}

bool Item_singlerow_subselect::val_bool() {
  if (!no_rows && !exec() && !value->null_value) {
    null_value = false;
    return value->val_bool();
  } else {
    reset();
    return 0;
  }
}

/* EXISTS subselect interface class */
class Query_result_exists_subquery : public Query_result_subquery {
 public:
  Query_result_exists_subquery(THD *thd, Item_subselect *item_arg)
      : Query_result_subquery(thd, item_arg) {}
  bool send_data(List<Item> &items);
};

bool Query_result_exists_subquery::send_data(List<Item> &) {
  DBUG_ENTER("Query_result_exists_subquery::send_data");
  Item_exists_subselect *it = (Item_exists_subselect *)item;
  /*
    A subquery may be evaluated 1) by executing the JOIN 2) by optimized
    functions (index_subquery, subquery materialization).
    It's only in (1) that we get here when we find a row. In (2) "value" is
    set elsewhere.
  */
  it->value = 1;
  it->assigned(true);
  DBUG_RETURN(0);
}

Item_exists_subselect::Item_exists_subselect(SELECT_LEX *select)
    : Item_subselect(),
      value(false),
      exec_method(EXEC_UNSPECIFIED),
      sj_convert_priority(0),
      embedding_join_nest(NULL) {
  DBUG_ENTER("Item_exists_subselect::Item_exists_subselect");
  init(select,
       new (*THR_MALLOC) Query_result_exists_subquery(current_thd, this));
  max_columns = UINT_MAX;
  null_value = false;  // can't be NULL
  maybe_null = 0;      // can't be NULL
  DBUG_VOID_RETURN;
}

void Item_exists_subselect::print(String *str, enum_query_type query_type) {
  str->append(STRING_WITH_LEN("exists"));
  Item_subselect::print(str, query_type);
}

bool Item_in_subselect::test_limit() {
  if (unit->fake_select_lex && unit->fake_select_lex->test_limit()) return true;

  for (SELECT_LEX *sl = unit->first_select(); sl; sl = sl->next_select()) {
    if (sl->test_limit()) return true;
  }
  return false;
}

Item_in_subselect::Item_in_subselect(Item *left_exp, SELECT_LEX *select)
    : Item_exists_subselect(),
      left_expr(left_exp),
      left_expr_cache(NULL),
      left_expr_cache_filled(false),
      need_expr_cache(true),
      m_injected_left_expr(NULL),
      optimizer(NULL),
      was_null(false),
      abort_on_null(false),
      in2exists_info(NULL),
      pushed_cond_guards(NULL),
      upper_item(NULL) {
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  init(select,
       new (*THR_MALLOC) Query_result_exists_subquery(current_thd, this));
  max_columns = UINT_MAX;
  maybe_null = 1;
  reset();
  // if test_limit will fail then error will be reported to client
  test_limit();
  DBUG_VOID_RETURN;
}

Item_in_subselect::Item_in_subselect(const POS &pos, Item *left_exp,
                                     PT_subquery *pt_subquery_arg)
    : super(pos),
      left_expr(left_exp),
      left_expr_cache(NULL),
      left_expr_cache_filled(false),
      need_expr_cache(true),
      m_injected_left_expr(NULL),
      optimizer(NULL),
      was_null(false),
      abort_on_null(false),
      in2exists_info(NULL),
      pushed_cond_guards(NULL),
      upper_item(NULL),
      pt_subselect(pt_subquery_arg) {
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  max_columns = UINT_MAX;
  maybe_null = 1;
  reset();
  DBUG_VOID_RETURN;
}

bool Item_in_subselect::itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::itemize(pc, res) || left_expr->itemize(pc, &left_expr) ||
      pt_subselect->contextualize(pc))
    return true;
  SELECT_LEX *select_lex = pt_subselect->value();
  init(select_lex,
       new (*THR_MALLOC) Query_result_exists_subquery(pc->thd, this));
  if (test_limit()) return true;
  return false;
}

Item_allany_subselect::Item_allany_subselect(Item *left_exp,
                                             chooser_compare_func_creator fc,
                                             SELECT_LEX *select, bool all_arg)
    : Item_in_subselect(), func_creator(fc), all(all_arg) {
  DBUG_ENTER("Item_allany_subselect::Item_allany_subselect");
  left_expr = left_exp;
  func = func_creator(all_arg);
  init(select,
       new (*THR_MALLOC) Query_result_exists_subquery(current_thd, this));
  max_columns = 1;
  abort_on_null = 0;
  reset();
  // if test_limit will fail then error will be reported to client
  test_limit();
  DBUG_VOID_RETURN;
}

bool Item_exists_subselect::resolve_type(THD *thd) {
  set_data_type_longlong();
  max_length = 1;
  max_columns = engine->cols();
  if (exec_method == EXEC_EXISTS) {
    DBUG_ASSERT(thd == unit->thd);
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    /*
      We need only 1 row to determine existence.
      Note that if the subquery is "SELECT1 UNION SELECT2" then this is not
      working optimally (Bug#14215895).
    */
    unit->global_parameters()->select_limit = new Item_int(1);
  }
  return false;
}

double Item_exists_subselect::val_real() {
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  return (double)value;
}

longlong Item_exists_subselect::val_int() {
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  return value;
}

/**
  Return the result of EXISTS as a string value

  Converts the true/false result into a string value.
  Note that currently this cannot be NULL, so if the query exection fails
  it will return 0.

  @param [out] str             buffer to hold the resulting string value
  @retval                      Pointer to the converted string.
                               Can't be a NULL pointer, as currently
                               EXISTS cannot return NULL.
*/

String *Item_exists_subselect::val_str(String *str) {
  DBUG_ASSERT(fixed == 1);
  if (exec()) reset();
  str->set((ulonglong)value, &my_charset_bin);
  return str;
}

/**
  Return the result of EXISTS as a decimal value

  Converts the true/false result into a decimal value.
  Note that currently this cannot be NULL, so if the query exection fails
  it will return 0.

  @param [out] decimal_value   Buffer to hold the resulting decimal value
  @retval                      Pointer to the converted decimal.
                               Can't be a NULL pointer, as currently
                               EXISTS cannot return NULL.
*/

my_decimal *Item_exists_subselect::val_decimal(my_decimal *decimal_value) {
  DBUG_ASSERT(fixed == 1);
  if (exec()) reset();
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}

bool Item_exists_subselect::val_bool() {
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  return value != 0;
}

double Item_in_subselect::val_real() {
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  if (was_null && !value) null_value = true;
  return (double)value;
}

longlong Item_in_subselect::val_int() {
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  if (was_null && !value) null_value = true;
  return value;
}

String *Item_in_subselect::val_str(String *str) {
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  if (was_null && !value) {
    null_value = true;
    return 0;
  }
  str->set((ulonglong)value, &my_charset_bin);
  return str;
}

bool Item_in_subselect::val_bool() {
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  if (was_null && !value) null_value = true;
  return value;
}

my_decimal *Item_in_subselect::val_decimal(my_decimal *decimal_value) {
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec()) {
    reset();
    return 0;
  }
  if (was_null && !value) null_value = true;
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}

/**
  Rewrite a single-column IN/ALL/ANY subselect

  DESCRIPTION
    Rewrite a single-column subquery using rule-based approach. The subquery

       oe $cmp$ (SELECT ie FROM ... WHERE subq_where ... HAVING subq_having)

    First, try to convert the subquery to scalar-result subquery in one of
    the forms:

       - oe $cmp$ (SELECT MAX(...) )  // handled by Item_singlerow_subselect
       - oe $cmp$ \<max\>(SELECT ...)   // handled by Item_maxmin_subselect

    If that fails, the subquery will be handled with class Item_in_optimizer.
    There are two possibilities:
    - If the subquery execution method is materialization, then the subquery is
      not transformed any further.
    - Otherwise the IN predicates is transformed into EXISTS by injecting
      equi-join predicates and possibly other helper predicates. For details
      see method single_value_in_like_transformer().

  @param select Query block of the subquery
  @param func   Subquery comparison creator

  @retval RES_OK     Either subquery was transformed, or appropriate
                       predicates where injected into it.
  @retval RES_REDUCE The subquery was reduced to non-subquery
  @retval RES_ERROR  Error
*/

Item_subselect::trans_res Item_in_subselect::single_value_transformer(
    SELECT_LEX *select, Comp_creator *func) {
  bool subquery_maybe_null = false;
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  /*
    Check that the right part of the subselect contains no more than one
    column. E.g. in SELECT 1 IN (SELECT * ..) the right part is (SELECT * ...)
  */
  // psergey: duplicated_subselect_card_check
  if (select->item_list.elements > 1) {
    my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
    DBUG_RETURN(RES_ERROR);
  }

  THD *const thd = unit->thd;

  /*
    Check the nullability of the subquery. The subquery should return
    only one column, so we check the nullability of the first item in
    SELECT_LEX::item_list. In case the subquery is a union, check the
    nullability of the first item of each query block belonging to the
    union.
  */
  for (SELECT_LEX *sel = unit->first_select(); sel != NULL;
       sel = sel->next_select()) {
    if ((subquery_maybe_null = sel->item_list.head()->maybe_null)) break;
  }
  /*
    If this is an ALL/ANY single-value subquery predicate, try to rewrite
    it with a MIN/MAX subquery.

    E.g. SELECT * FROM t1 WHERE b > ANY (SELECT a FROM t2) can be rewritten
    with SELECT * FROM t1 WHERE b > (SELECT MIN(a) FROM t2).

    A predicate may be transformed to use a MIN/MAX subquery if it:
    1. has a greater than/less than comparison operator, and
    2. is not correlated with the outer query, and
    3. UNKNOWN results are treated as FALSE, or can never be generated, and
  */
  if (!func->eqne_op() &&                                                   // 1
      !unit->uncacheable &&                                                 // 2
      (abort_on_null || (upper_item && upper_item->is_top_level_item()) ||  // 3
       (!left_expr->maybe_null && !subquery_maybe_null))) {
    if (substitution) {
      // It is second (third, ...) SELECT of UNION => All is done
      DBUG_RETURN(RES_OK);
    }

    Item *subs;
    if (!select->group_list.elements && !select->having_cond() &&
        // MIN/MAX(agg_or_window_func) would not be valid
        !select->with_sum_func && select->m_windows.elements == 0 &&
        !(select->next_select()) && select->table_list.elements &&
        !(substype() == ALL_SUBS && subquery_maybe_null)) {
      OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select->select_number,
                          "> ALL/ANY (SELECT)", "SELECT(MIN)");
      oto1.add("chosen", true);
      Item_sum_hybrid *item;
      nesting_map save_allow_sum_func;
      if (func->l_op()) {
        /*
          (ALL && (> || =>)) || (ANY && (< || =<))
          for ALL condition is inverted
        */
        item = new Item_sum_max(select->base_ref_items[0]);
      } else {
        /*
          (ALL && (< || =<)) || (ANY && (> || =>))
          for ALL condition is inverted
        */
        item = new Item_sum_min(select->base_ref_items[0]);
      }
      if (upper_item) upper_item->set_sum_test(item);
      select->base_ref_items[0] = item;
      {
        List_iterator<Item> it(select->item_list);
        it++;
        it.replace(item);

        /*
          If the item in the SELECT list has gone through a temporary
          transformation (like Item_field to Item_ref), make sure we
          are rolling it back based on location inside Item_sum arg list.
        */
        thd->replace_rollback_place(item->get_arg_ptr(0));
      }

      DBUG_EXECUTE("where",
                   print_where(item, "rewrite with MIN/MAX", QT_ORDINARY););

      save_allow_sum_func = thd->lex->allow_sum_func;
      thd->lex->allow_sum_func |= (nesting_map)1 << select->nest_level;
      /*
        Item_sum_(max|min) can't substitute other item => we can use 0 as
        reference, also Item_sum_(max|min) can't be fixed after creation, so
        we do not check item->fixed
      */
      if (item->fix_fields(thd, 0)) DBUG_RETURN(RES_ERROR);
      thd->lex->allow_sum_func = save_allow_sum_func;

      subs = new Item_singlerow_subselect(select);
    } else {
      OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select->select_number,
                          "> ALL/ANY (SELECT)", "MIN (SELECT)");
      oto1.add("chosen", true);
      Item_maxmin_subselect *item;
      subs = item = new Item_maxmin_subselect(thd, this, select, func->l_op(),
                                              substype() == ANY_SUBS);
      if (upper_item) upper_item->set_sub_test(item);
    }
    if (upper_item) upper_item->set_subselect(this);
    /*
      fix fields is already called for  left expression.
      Note that real_item() should be used for all the runtime
      created Ref items instead of original left expression
      because these items would be deleted at the end
      of the statement. Thus one of 'substitution' arguments
      can be broken in case of PS.

      @todo
      Why do we use real_item()/substitutional_item() instead of the plain
      left_expr?
      Because left_expr might be a rollbackable item, and we fail to properly
      rollback all copies of left_expr at end of execution, so we want to
      avoid creating copies of left_expr as much as possible, so we use
      real_item() instead.
      Doing a proper rollback is difficult: the change was registered for the
      original item which was the left argument of IN. Then this item was
      copied to left_expr, which is copied below to substitution->args[0]. To
      do a proper rollback, we would have to restore the content
      of both copies as well as the original item. There might be more copies,
      if AND items have been constructed.
      The same applies to the right expression.
      However, using real_item()/substitutional_item() brings its own
      problems: for example, we lose information that the item is an outer
      reference; the item can thus wrongly be considered for a Keyuse (causing
      bug#17766653).
      When WL#6570 removes the "rolling back" system, all
      real_item()/substitutional_item() in this file should be removed.
    */
    substitution = func->create(left_expr->substitutional_item(), subs);
    DBUG_RETURN(RES_OK);
  }

  if (!substitution) {
    /* We're invoked for the 1st (or the only) SELECT in the subquery UNION */
    substitution = optimizer;

    thd->lex->set_current_select(select->outer_select());
    // optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0)) {
      thd->lex->set_current_select(select); /* purecov: inspected */
      DBUG_RETURN(RES_ERROR);               /* purecov: inspected */
    }
    thd->lex->set_current_select(select);

    /* We will refer to upper level cache array => we have to save it for SP */
    optimizer->keep_top_level_cache();

    /*
      As far as  Item_ref_in_optimizer do not substitute itself on fix_fields
      we can use same item for all selects.
    */
    Item_ref *const left =
        new Item_ref(&select->context, (Item **)optimizer->get_cache(),
                     (char *)"<no matter>", (char *)in_left_expr_name);
    if (left == NULL) DBUG_RETURN(RES_ERROR);

    if (mark_as_outer(left_expr, 0))
      left->depended_from = select->outer_select();

    m_injected_left_expr = left;

    DBUG_ASSERT(in2exists_info == NULL);
    in2exists_info = new (*THR_MALLOC) In2exists_info;
    in2exists_info->dependent_before =
        unit->uncacheable & UNCACHEABLE_DEPENDENT;
    if (!left_expr->const_item()) unit->uncacheable |= UNCACHEABLE_DEPENDENT;
    in2exists_info->dependent_after = unit->uncacheable & UNCACHEABLE_DEPENDENT;
  }

  if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards) {
    if (!(pushed_cond_guards = (bool *)thd->alloc(sizeof(bool))))
      DBUG_RETURN(RES_ERROR);
    pushed_cond_guards[0] = true;
  }

  /* Perform the IN=>EXISTS transformation. */
  const trans_res retval = single_value_in_to_exists_transformer(select, func);
  DBUG_RETURN(retval);
}

/**
  Transform an IN predicate into EXISTS via predicate injection.

  @details The transformation injects additional predicates into the subquery
  (and makes the subquery correlated) as follows.

  - If the subquery has aggregates, GROUP BY, or HAVING, convert to

    SELECT ie FROM ...  HAVING subq_having AND
                               trigcond(oe $cmp$ ref_or_null_helper<ie>)

    the addition is wrapped into trigger only when we want to distinguish
    between NULL and FALSE results.

  - Otherwise (no aggregates/GROUP BY/HAVING) convert it to one of the
    following:

    = If we don't need to distinguish between NULL and FALSE subquery:

      SELECT 1 FROM ... WHERE (oe $cmp$ ie) AND subq_where

    = If we need to distinguish between those:

      SELECT 1 FROM ...
        WHERE  subq_where AND trigcond((oe $cmp$ ie) OR (ie IS NULL))
        HAVING trigcond(@<is_not_null_test@>(ie))

  At JOIN::optimize() we will compare costs of materialization and EXISTS; if
  the former is cheaper we will switch to it.

    @param select Query block of the subquery
    @param func   Subquery comparison creator

    @retval RES_OK     Either subquery was transformed, or appopriate
                       predicates where injected into it.
    @retval RES_REDUCE The subquery was reduced to non-subquery
    @retval RES_ERROR  Error
*/

Item_subselect::trans_res
Item_in_subselect::single_value_in_to_exists_transformer(SELECT_LEX *select,
                                                         Comp_creator *func) {
  THD *const thd = unit->thd;
  DBUG_ENTER("Item_in_subselect::single_value_in_to_exists_transformer");

  SELECT_LEX *outer = select->outer_select();

  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select->select_number,
                      "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
  oto1.add("chosen", true);

  // Transformation will make the subquery a dependent one.
  if (!left_expr->const_item()) select->uncacheable |= UNCACHEABLE_DEPENDENT;

  in2exists_info->added_to_where = false;

  if (select->having_cond() || select->with_sum_func ||
      select->group_list.elements || select->m_windows.elements > 0) {
    bool tmp;
    Item_ref_null_helper *ref_null = new Item_ref_null_helper(
        &select->context, this, &select->base_ref_items[0], (char *)"<ref>",
        this->full_name());
    Item_bool_func *item = func->create(m_injected_left_expr, ref_null);
    item->set_created_by_in2exists();

    /*
      Assume that the expression in the SELECT list, is a function of a group
      aggregate which is aggregated in an outer query, for example
      SELECT ... FROM t1 WHERE t1.b IN (SELECT <expr of SUM(t1.a)> FROM t2). We
      are changing it to
      SELECT ... FROM t1 WHERE t1.b IN (SELECT <expr of SUM(t1.a)> FROM t2
                                        HAVING t1.b=ref-to-<expr of SUM(t1.a)>).
      SUM is an "inner sum func", its fix_fields() has added it to
      inner_sum_func_list of the outer query; the outer query will do
      split_sum_func on it which will add SUM as a hidden item and replace it
      in 'expr' with a pointer to an Item_ref.
      If 'expr' is a function which has SUM as one of its arguments, the
      SELECT list and HAVING access 'expr' through two different pointers, but
      there's only one 'expr' Item, which accesses SUM through one pointer, so
      there's a single ref_by pointer to remember, we use ref_by[0].
      But if 'expr' is directly the SUM, with no Item in between, then there
      are two places where 'expr' should be replaced: the iterator in the
      SELECT list, and the 'ref-to-expr' in HAVING above. So we have to
      document those 2 places in ref_by[0] and ref_by[1].
    */
    Item *selected = select->base_ref_items[0];
    if (selected->type() == SUM_FUNC_ITEM) {
      Item_sum *selected_sum = static_cast<Item_sum *>(selected);
      if (!selected_sum->ref_by[0])
        selected_sum->ref_by[0] = ref_null->ref;
      else {
        // Slot 0 already occupied, use 1.
        DBUG_ASSERT(!selected_sum->ref_by[1]);
        selected_sum->ref_by[1] = ref_null->ref;
      }
    }
    if (!abort_on_null && left_expr->maybe_null) {
      /*
        We can encounter "NULL IN (SELECT ...)". Wrap the added condition
        within a trig_cond.
      */
      item =
          new Item_func_trig_cond(item, get_cond_guard(0), NULL, NO_PLAN_IDX,
                                  Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL);
      item->set_created_by_in2exists();
    }

    /*
      AND and comparison functions can't be changed during fix_fields()
      we can assign select_lex->having_cond here, and pass NULL as last
      argument (reference) to fix_fields()
    */
    select->set_having_cond(and_items(select->having_cond(), item));
    select->having_cond()->top_level_item();
    select->having_fix_field = true;
    /*
      we do not check having_cond()->fixed, because Item_and (from and_items)
      or comparison function (from func->create) can't be fixed after creation
    */
    Opt_trace_array having_trace(&thd->opt_trace,
                                 "evaluating_constant_having_conditions");
    tmp = select->having_cond()->fix_fields(thd, NULL);
    select->having_fix_field = false;
    if (tmp) DBUG_RETURN(RES_ERROR);
  } else {
    /*
      Grep for "WL#6570" to see the relevant comment about real_item.
    */
    Item *orig_item = select->item_list.head()->real_item();

    if (select->table_list.elements || select->where_cond()) {
      bool tmp;
      Item_bool_func *item = func->create(m_injected_left_expr, orig_item);
      /*
        We may soon add a 'OR inner IS NULL' to 'item', but that may later be
        removed if 'inner' is not nullable, so the in2exists mark must be on
        'item' too. Not only on the OR node.
      */
      item->set_created_by_in2exists();
      if (!abort_on_null && orig_item->maybe_null) {
        Item_bool_func *having = new Item_is_not_null_test(this, orig_item);
        having->set_created_by_in2exists();
        if (left_expr->maybe_null) {
          if (!(having = new Item_func_trig_cond(
                    having, get_cond_guard(0), NULL, NO_PLAN_IDX,
                    Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          having->set_created_by_in2exists();
        }
        /*
          Item_is_not_null_test can't be changed during fix_fields()
          we can assign select_lex->having_cond() here, and pass NULL as last
          argument (reference) to fix_fields()
        */
        select->set_having_cond(having);
        select->having_fix_field = true;
        /*
          No need to check select_lex->having_cond()->fixed, because Item_and
          (from and_items) or comparison function (from func->create)
          can't be fixed after creation.
        */
        Opt_trace_array having_trace(&thd->opt_trace,
                                     "evaluating_constant_having_conditions");
        tmp = select->having_cond()->fix_fields(thd, NULL);
        select->having_fix_field = false;
        if (tmp) DBUG_RETURN(RES_ERROR);
        item = new Item_cond_or(item, new Item_func_isnull(orig_item));
        item->set_created_by_in2exists();
      }
      /*
        If we may encounter NULL IN (SELECT ...) and care whether subquery
        result is NULL or FALSE, wrap condition in a trig_cond.
      */
      if (!abort_on_null && left_expr->maybe_null) {
        if (!(item = new Item_func_trig_cond(
                  item, get_cond_guard(0), NULL, NO_PLAN_IDX,
                  Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        item->set_created_by_in2exists();
      }
      /*
        AND can't be changed during fix_fields()
        we can assign select_lex->having_cond() here, and pass NULL as last
        argument (reference) to fix_fields()

        Note that if select_lex is the fake one of UNION, it does not make
        much sense to give it a WHERE clause below... we already give one to
        each member of the UNION.
      */
      select->set_where_cond(and_items(select->where_cond(), item));
      select->where_cond()->top_level_item();
      in2exists_info->added_to_where = true;
      /*
        No need to check select_lex->where_cond()->fixed, because Item_and
        can't be fixed after creation.
      */
      Opt_trace_array where_trace(&thd->opt_trace,
                                  "evaluating_constant_where_conditions");
      if (select->where_cond()->fix_fields(thd, NULL)) DBUG_RETURN(RES_ERROR);
    } else {
      bool tmp;
      if (unit->is_union()) {
        /*
          comparison functions can't be changed during fix_fields()
          we can assign select_lex->having_cond() here, and pass NULL as last
          argument (reference) to fix_fields()
        */
        Item_bool_func *new_having =
            func->create(m_injected_left_expr,
                         new Item_ref_null_helper(
                             &select->context, this, &select->base_ref_items[0],
                             (char *)"<no matter>", (char *)"<result>"));
        new_having->set_created_by_in2exists();
        if (!abort_on_null && left_expr->maybe_null) {
          if (!(new_having = new Item_func_trig_cond(
                    new_having, get_cond_guard(0), NULL, NO_PLAN_IDX,
                    Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          new_having->set_created_by_in2exists();
        }
        select->set_having_cond(new_having);
        select->having_fix_field = true;

        /*
          No need to check select_lex->having_cond()->fixed, because comparison
          function (from func->create) can't be fixed after creation.
        */
        Opt_trace_array having_trace(&thd->opt_trace,
                                     "evaluating_constant_having_conditions");
        tmp = select->having_cond()->fix_fields(thd, NULL);
        select->having_fix_field = false;
        if (tmp) DBUG_RETURN(RES_ERROR);
      } else {
        /*
          Single query block, without tables, without WHERE, HAVING, LIMIT:
          its content has one row and is equal to the item in the SELECT list,
          so we can replace the IN(subquery) with an equality.
          The expression is moved to the immediately outer query block, so it
          may no longer contain outer references.
        */
        outer->merge_contexts(select);
        orig_item->fix_after_pullout(outer, select);

        /*
          fix_field of substitution item will be done in time of
          substituting.
          Note that real_item() should be used for all the runtime
          created Ref items instead of original left expression
          because these items would be deleted at the end
          of the statement. Thus one of 'substitution' arguments
          can be broken in case of PS.
         */
        substitution =
            func->create(left_expr->substitutional_item(), orig_item);
        have_to_be_excluded = 1;
        if (thd->lex->is_explain()) {
          char warn_buff[MYSQL_ERRMSG_SIZE];
          sprintf(warn_buff, ER_THD(thd, ER_SELECT_REDUCED),
                  select->select_number);
          push_warning(thd, Sql_condition::SL_NOTE, ER_SELECT_REDUCED,
                       warn_buff);
        }
        DBUG_RETURN(RES_REDUCE);
      }
    }
  }

  DBUG_RETURN(RES_OK);
}

Item_subselect::trans_res Item_in_subselect::row_value_transformer(
    SELECT_LEX *select) {
  uint cols_num = left_expr->cols();

  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  // psergey: duplicated_subselect_card_check
  if (select->item_list.elements != left_expr->cols()) {
    my_error(ER_OPERAND_COLUMNS, MYF(0), left_expr->cols());
    DBUG_RETURN(RES_ERROR);
  }

  /*
    Wrap the current IN predicate in an Item_in_optimizer. The actual
    substitution in the Item tree takes place in Item_subselect::fix_fields.
  */
  if (!substitution) {
    // first call for this unit
    substitution = optimizer;

    THD *const thd = unit->thd;
    thd->lex->set_current_select(select->outer_select());
    // optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0)) {
      thd->lex->set_current_select(select); /* purecov: inspected */
      DBUG_RETURN(RES_ERROR);               /* purecov: inspected */
    }

    // we will refer to upper level cache array => we have to save it in PS
    optimizer->keep_top_level_cache();

    thd->lex->set_current_select(select);
    DBUG_ASSERT(in2exists_info == NULL);
    in2exists_info = new (*THR_MALLOC) In2exists_info;
    in2exists_info->dependent_before =
        unit->uncacheable & UNCACHEABLE_DEPENDENT;
    if (!left_expr->const_item()) unit->uncacheable |= UNCACHEABLE_DEPENDENT;
    in2exists_info->dependent_after = unit->uncacheable & UNCACHEABLE_DEPENDENT;

    if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards) {
      if (!(pushed_cond_guards =
                (bool *)thd->alloc(sizeof(bool) * left_expr->cols())))
        DBUG_RETURN(RES_ERROR);
      for (uint i = 0; i < cols_num; i++) pushed_cond_guards[i] = true;
    }
  }

  // Perform the IN=>EXISTS transformation.
  Item_subselect::trans_res res = row_value_in_to_exists_transformer(select);
  DBUG_RETURN(res);
}

/**
  Tranform a (possibly non-correlated) IN subquery into a correlated EXISTS.

  @todo
  The IF-ELSE below can be refactored so that there is no duplication of the
  statements that create the new conditions. For this we have to invert the IF
  and the FOR statements as this:
  for (each left operand)
    create the equi-join condition
    if (is_having_used || !abort_on_null)
      create the "is null" and is_not_null_test items
    if (is_having_used)
      add the equi-join and the null tests to HAVING
    else
      add the equi-join and the "is null" to WHERE
      add the is_not_null_test to HAVING
*/

Item_subselect::trans_res Item_in_subselect::row_value_in_to_exists_transformer(
    SELECT_LEX *select) {
  THD *const thd = unit->thd;
  Item_bool_func *having_item = NULL;
  uint cols_num = left_expr->cols();
  bool is_having_used = select->having_cond() || select->with_sum_func ||
                        select->group_list.first ||
                        !select->table_list.elements;

  DBUG_ENTER("Item_in_subselect::row_value_in_to_exists_transformer");
  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select->select_number,
                      "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
  oto1.add("chosen", true);

  // Transformation will make the subquery a dependent one.
  if (!left_expr->const_item()) select->uncacheable |= UNCACHEABLE_DEPENDENT;
  in2exists_info->added_to_where = false;

  if (is_having_used) {
    /*
      (l1, l2, l3) IN (SELECT v1, v2, v3 ... HAVING having) =>
      EXISTS (SELECT ... HAVING having and
                                (l1 = v1 or is null v1) and
                                (l2 = v2 or is null v2) and
                                (l3 = v3 or is null v3) and
                                is_not_null_test(v1) and
                                is_not_null_test(v2) and
                                is_not_null_test(v3))
      where is_not_null_test used to register nulls in case if we have
      not found matching to return correct NULL value
      TODO: say here explicitly if the order of AND parts matters or not.
    */
    Item_bool_func *item_having_part2 = NULL;
    for (uint i = 0; i < cols_num; i++) {
      Item *item_i = select->base_ref_items[i];
      Item **pitem_i = &select->base_ref_items[i];
      DBUG_ASSERT((left_expr->fixed && item_i->fixed) ||
                  (item_i->type() == REF_ITEM &&
                   ((Item_ref *)(item_i))->ref_type() == Item_ref::OUTER_REF));
      if (item_i->check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
      Item_ref *const left =
          new Item_ref(&select->context, (*optimizer->get_cache())->addr(i),
                       (char *)"<no matter>", (char *)in_left_expr_name);
      if (left == NULL) DBUG_RETURN(RES_ERROR); /* purecov: inspected */

      if (mark_as_outer(left_expr, i))
        left->depended_from = select->outer_select();

      Item_bool_func *item_eq = new Item_func_eq(
          left, new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                             (char *)"<list ref>"));
      item_eq->set_created_by_in2exists();
      Item_bool_func *item_isnull = new Item_func_isnull(
          new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                       (char *)"<list ref>"));
      item_isnull->set_created_by_in2exists();
      Item_bool_func *col_item = new Item_cond_or(item_eq, item_isnull);
      col_item->set_created_by_in2exists();
      if (!abort_on_null && left_expr->element_index(i)->maybe_null) {
        if (!(col_item = new Item_func_trig_cond(
                  col_item, get_cond_guard(i), NULL, NO_PLAN_IDX,
                  Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        col_item->set_created_by_in2exists();
      }

      having_item = and_items(having_item, col_item);
      having_item->set_created_by_in2exists();
      Item_bool_func *item_nnull_test = new Item_is_not_null_test(
          this, new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                             (char *)"<list ref>"));
      item_nnull_test->set_created_by_in2exists();
      if (!abort_on_null && left_expr->element_index(i)->maybe_null) {
        if (!(item_nnull_test = new Item_func_trig_cond(
                  item_nnull_test, get_cond_guard(i), NULL, NO_PLAN_IDX,
                  Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        item_nnull_test->set_created_by_in2exists();
      }
      item_having_part2 = and_items(item_having_part2, item_nnull_test);
      item_having_part2->set_created_by_in2exists();
      item_having_part2->top_level_item();
    }
    having_item = and_items(having_item, item_having_part2);
    having_item->set_created_by_in2exists();
    having_item->top_level_item();
  } else {
    /*
      (l1, l2, l3) IN (SELECT v1, v2, v3 ... WHERE where) =>
      EXISTS (SELECT ... WHERE where and
                               (l1 = v1 or is null v1) and
                               (l2 = v2 or is null v2) and
                               (l3 = v3 or is null v3)
                         HAVING is_not_null_test(v1) and
                                is_not_null_test(v2) and
                                is_not_null_test(v3))
      where is_not_null_test register NULLs values but reject rows

      in case when we do not need correct NULL, we have simplier construction:
      EXISTS (SELECT ... WHERE where and
                               (l1 = v1) and
                               (l2 = v2) and
                               (l3 = v3)
    */
    Item_bool_func *where_item = NULL;
    for (uint i = 0; i < cols_num; i++) {
      Item *item_i = select->base_ref_items[i];
      Item **pitem_i = &select->base_ref_items[i];
      DBUG_ASSERT((left_expr->fixed && item_i->fixed) ||
                  (item_i->type() == REF_ITEM &&
                   ((Item_ref *)(item_i))->ref_type() == Item_ref::OUTER_REF));
      if (item_i->check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
      Item_ref *const left =
          new Item_ref(&select->context, (*optimizer->get_cache())->addr(i),
                       (char *)"<no matter>", (char *)in_left_expr_name);
      if (left == NULL) DBUG_RETURN(RES_ERROR);

      if (mark_as_outer(left_expr, i))
        left->depended_from = select->outer_select();

      Item_bool_func *item = new Item_func_eq(
          left, new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                             (char *)"<list ref>"));
      item->set_created_by_in2exists();
      if (!abort_on_null) {
        Item_bool_func *having_col_item = new Item_is_not_null_test(
            this, new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                               (char *)"<list ref>"));

        having_col_item->set_created_by_in2exists();
        Item_bool_func *item_isnull = new Item_func_isnull(
            new Item_ref(&select->context, pitem_i, (char *)"<no matter>",
                         (char *)"<list ref>"));
        item_isnull->set_created_by_in2exists();
        item = new Item_cond_or(item, item_isnull);
        item->set_created_by_in2exists();
        /*
          TODO: why we create the above for cases where the right part
                cant be NULL?
        */
        if (left_expr->element_index(i)->maybe_null) {
          if (!(item = new Item_func_trig_cond(
                    item, get_cond_guard(i), NULL, NO_PLAN_IDX,
                    Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          item->set_created_by_in2exists();
          if (!(having_col_item = new Item_func_trig_cond(
                    having_col_item, get_cond_guard(i), NULL, NO_PLAN_IDX,
                    Item_func_trig_cond::OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          having_col_item->set_created_by_in2exists();
        }
        having_item = and_items(having_item, having_col_item);
        having_item->set_created_by_in2exists();
      }

      where_item = and_items(where_item, item);
      where_item->set_created_by_in2exists();
    }
    /*
      AND can't be changed during fix_fields()
      we can assign select->where_cond() here, and pass NULL as last
      argument (reference) to fix_fields()
    */
    select->set_where_cond(and_items(select->where_cond(), where_item));
    select->where_cond()->top_level_item();
    in2exists_info->added_to_where = true;
    Opt_trace_array where_trace(&thd->opt_trace,
                                "evaluating_constant_where_conditions");
    if (select->where_cond()->fix_fields(thd, NULL)) DBUG_RETURN(RES_ERROR);
  }
  if (having_item) {
    bool res;
    select->set_having_cond(and_items(select->having_cond(), having_item));
    select->having_cond()->top_level_item();
    /*
      AND can't be changed during fix_fields()
      we can assign select->having_cond() here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select->having_fix_field = true;
    Opt_trace_array having_trace(&thd->opt_trace,
                                 "evaluating_constant_having_conditions");
    res = select->having_cond()->fix_fields(thd, NULL);
    select->having_fix_field = false;
    if (res) {
      DBUG_RETURN(RES_ERROR);
    }
  }

  DBUG_RETURN(RES_OK);
}

Item_subselect::trans_res Item_in_subselect::select_transformer(
    SELECT_LEX *select) {
  return select_in_like_transformer(select, &eq_creator);
}

/**
  Prepare IN/ALL/ANY/SOME subquery transformation and call appropriate
  transformation function.

    To decide which transformation procedure (scalar or row) applicable here
    we have to call fix_fields() for left expression to be able to call
    cols() method on it. Also this method make arena management for
    underlying transformation methods.

  @param select  Query block of subquery being transformed
  @param func    creator of condition function of subquery

  @retval
    RES_OK      OK
  @retval
    RES_REDUCE  OK, and current subquery was reduced during
    transformation
  @retval
    RES_ERROR   Error
*/

Item_subselect::trans_res Item_in_subselect::select_in_like_transformer(
    SELECT_LEX *select, Comp_creator *func) {
  THD *const thd = unit->thd;
  const char *save_where = thd->where;
  Item_subselect::trans_res res = RES_ERROR;
  bool result;

  DBUG_ENTER("Item_in_subselect::select_in_like_transformer");

#ifndef DBUG_OFF
  /*
    IN/SOME/ALL/ANY subqueries don't support LIMIT clause. Without
    it, ORDER BY becomes meaningless and should already have been
    removed in resolve_subquery()
  */
  for (SELECT_LEX *sl = unit->first_select(); sl; sl = sl->next_select())
    DBUG_ASSERT(!sl->order_list.first);
#endif

  if (changed) DBUG_RETURN(RES_OK);

  thd->where = "IN/ALL/ANY subquery";

  /*
    In some optimisation cases we will not need this Item_in_optimizer
    object, but we can't know it here, but here we need address correct
    reference on left expresion.

    //psergey: he means confluent cases like "... IN (SELECT 1)"
  */
  if (!optimizer) {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    optimizer = new Item_in_optimizer(left_expr, this);

    if (!optimizer) goto err;
  }

  thd->lex->set_current_select(select->outer_select());
  result =
      (!left_expr->fixed && left_expr->fix_fields(thd, optimizer->arguments()));
  /* fix_fields can change reference to left_expr, we need reassign it */
  left_expr = optimizer->arguments()[0];

  thd->lex->set_current_select(select);
  if (result) goto err;

  /*
    If we didn't choose an execution method up to this point, we choose
    the IN=>EXISTS transformation, at least temporarily.
  */
  if (exec_method == EXEC_UNSPECIFIED) exec_method = EXEC_EXISTS_OR_MAT;

  /*
    Both transformers call fix_fields() only for Items created inside them,
    and all those items do not make permanent changes in the current item arena
    which allows us to call them with changed arena (if we do not know the
    nature of Item, we have to call fix_fields() for it only with the original
    arena to avoid memory leak).
  */

  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    if (left_expr->cols() == 1)
      res = single_value_transformer(select, func);
    else {
      /* we do not support row operation for ALL/ANY/SOME */
      if (func != &eq_creator) {
        my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
        DBUG_RETURN(RES_ERROR);
      }
      res = row_value_transformer(select);
    }
  }

err:
  thd->where = save_where;
  DBUG_RETURN(res);
}

void Item_in_subselect::print(String *str, enum_query_type query_type) {
  if (exec_method == EXEC_EXISTS_OR_MAT || exec_method == EXEC_EXISTS)
    str->append(STRING_WITH_LEN("<exists>"));
  else {
    left_expr->print(str, query_type);
    str->append(STRING_WITH_LEN(" in "));
  }
  Item_subselect::print(str, query_type);
}

bool Item_in_subselect::fix_fields(THD *thd_arg, Item **ref) {
  bool result = 0;

  if (exec_method == EXEC_SEMI_JOIN) return !((*ref) = new Item_int(1));

  if ((thd_arg->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      left_expr && !left_expr->fixed) {
    Disable_semijoin_flattening DSF(thd_arg->lex->current_select(), true);
    result = left_expr->fix_fields(thd_arg, &left_expr);
  }

  return result || Item_subselect::fix_fields(thd_arg, ref);
}

void Item_in_subselect::fix_after_pullout(SELECT_LEX *parent_select,
                                          SELECT_LEX *removed_select) {
  Item_subselect::fix_after_pullout(parent_select, removed_select);

  left_expr->fix_after_pullout(parent_select, removed_select);

  used_tables_cache |= left_expr->used_tables();
}

/**
  Initialize the cache of the left operand of the IN predicate.

  @note This method has the same purpose as alloc_group_fields(),
  but it takes a different kind of collection of items, and the
  list we push to is dynamically allocated.

  @retval true  if a memory allocation error occurred
  @retval false if success
*/

bool Item_in_subselect::init_left_expr_cache() {
  /*
    Check if the left operand is a subquery that yields an empty set of rows.
    If so, skip initializing a cache; for an empty set the subquery
    exec won't read any rows and so lead to uninitalized reads if attempted.
  */
  if (left_expr->type() == SUBSELECT_ITEM && left_expr->null_value) {
    return false;
  }

  JOIN *outer_join = unit->outer_select()->join;
  /*
    An IN predicate might be evaluated in a query for which all tables have
    been optimized away.
  */
  if (!(outer_join && outer_join->qep_tab)) {
    need_expr_cache = false;
    return false;
  }

  if (!(left_expr_cache = new (*THR_MALLOC) List<Cached_item>)) return true;

  for (uint i = 0; i < left_expr->cols(); i++) {
    Cached_item *cur_item_cache =
        new_Cached_item(unit->thd, left_expr->element_index(i));
    if (!cur_item_cache || left_expr_cache->push_front(cur_item_cache))
      return true;
  }
  return false;
}

/**
  Tells an Item that it is in the condition of a JOIN_TAB of a query block.

  @param arg  A std::pair: first argument is the query block, second is the
  index of JOIN_TAB in JOIN's array.

  The Item records this fact and can deduce from it the estimated number of
  times that it will be evaluated.
  If the JOIN_TAB doesn't belong to the query block owning this
  Item_subselect, it must belong to a more inner query block (not a more
  outer, as the walk() doesn't dive into subqueries); in that case, it must be
  that Item_subselect is the left-hand-side of a subquery transformed with
  IN-to-EXISTS and has been wrapped in Item_cache and then injected into the
  WHERE/HAVING of that subquery; but then the Item_subselect will not be
  evaluated when the JOIN_TAB's condition is evaluated (Item_cache will
  short-circuit it); it will be evaluated when the IN(subquery)
  (Item_in_optimizer) is - that's when the Item_cache is updated. Thus, we
  will ignore JOIN_TAB in this case.
*/
bool Item_subselect::inform_item_in_cond_of_tab(uchar *arg) {
  std::pair<SELECT_LEX *, int> *pair_object =
      pointer_cast<std::pair<SELECT_LEX *, int> *>(arg);
  if (pair_object->first == unit->outer_select())
    in_cond_of_tab = pair_object->second;
  return false;
}

/**
  Mark the subquery as optimized away, for EXPLAIN.
*/

bool Item_subselect::subq_opt_away_processor(uchar *) {
  unit->set_explain_marker(CTX_OPTIMIZED_AWAY_SUBQUERY);
  // Return false to continue marking all subqueries in the expression.
  return false;
}

/**
   Clean up after removing the subquery from the item tree.

   Call SELECT_LEX_UNIT::exclude_tree() to unlink it from its
   master and to unlink direct SELECT_LEX children from
   all_selects_list.

   Don't unlink subqueries that are not descendants of the starting
   point (root) of the removal and cleanup.
 */
bool Item_subselect::clean_up_after_removal(uchar *arg) {
  /*
    Some commands still execute subqueries during resolving.
    Make sure they are cleaned up properly.
    @todo: Remove this code when SET is also refactored.
  */
  if (unit->is_executed()) {
    DBUG_ASSERT(unit->first_select()->parent_lex->sql_command ==
                SQLCOM_SET_OPTION);
    unit->cleanup(true);
  }

  SELECT_LEX *root = static_cast<SELECT_LEX *>(static_cast<void *>(arg));
  SELECT_LEX *sl = unit->outer_select();

  /*
    While traversing the item tree with Item::walk(), Item_refs may
    point to Item_subselects at different positions in the query. We
    should only exclude units that are descendants of the starting
    point for the walk.

    Traverse the tree towards the root. Afterwards, we have:
    1) sl == root: unit is a descendant of the starting point, or
    2) sl == NULL: unit is not a descendant of the starting point
  */
  while (sl != root && sl != NULL) sl = sl->outer_select();
  if (sl == root) unit->exclude_tree();
  return false;
}

Item_subselect::trans_res Item_allany_subselect::select_transformer(
    SELECT_LEX *select) {
  DBUG_ENTER("Item_allany_subselect::select_transformer");
  if (upper_item) upper_item->show = 1;
  trans_res retval = select_in_like_transformer(select, func);
  DBUG_RETURN(retval);
}

bool Item_subselect::is_evaluated() const { return unit->is_executed(); }

void Item_allany_subselect::print(String *str, enum_query_type query_type) {
  if (exec_method == EXEC_EXISTS_OR_MAT || exec_method == EXEC_EXISTS)
    str->append(STRING_WITH_LEN("<exists>"));
  else {
    left_expr->print(str, query_type);
    str->append(' ');
    str->append(func->symbol(all));
    str->append(all ? " all " : " any ", 5);
  }
  Item_subselect::print(str, query_type);
}

void subselect_engine::set_thd_for_result() {
  /*
    Query_result's constructor sets neither Query_result::thd nor
    Query_result::unit.
  */
  if (result) result->set_thd(item->unit->thd);
}

subselect_single_select_engine::subselect_single_select_engine(
    SELECT_LEX *select, Query_result_interceptor *result_arg,
    Item_subselect *item_arg)
    : subselect_engine(item_arg, result_arg), select_lex(select) {
  select_lex->master_unit()->item = item_arg;
}

void subselect_single_select_engine::cleanup() {
  DBUG_ENTER("subselect_single_select_engine::cleanup");
  item->unit->reset_executed();
  result->cleanup();
  DBUG_VOID_RETURN;
}

void subselect_union_engine::cleanup() {
  DBUG_ENTER("subselect_union_engine::cleanup");
  item->unit->reset_executed();
  result->cleanup();
  DBUG_VOID_RETURN;
}

subselect_union_engine::subselect_union_engine(
    SELECT_LEX_UNIT *u, Query_result_interceptor *result_arg,
    Item_subselect *item_arg)
    : subselect_engine(item_arg, result_arg) {
  unit = u;
  unit->item = item_arg;
}

/**
  Prepare the query expression underlying the subquery.

  @details
  This function is called from Item_subselect::fix_fields. If the subquery is
  transformed with an Item_in_optimizer object, this function may be called
  twice, hence we need the check on 'is_prepared()' at the start, to avoid
  redoing the preparation.

  @returns false if success, true if error
*/

bool subselect_single_select_engine::prepare() {
  if (item->unit->is_prepared()) return false;

  SELECT_LEX_UNIT *const unit = item->unit;
  THD *const thd = unit->thd;

  DBUG_ASSERT(result);

  select_lex->set_query_result(result);
  select_lex->make_active_options(SELECT_NO_UNLOCK, 0);

  if (unit->prepare_limit(thd, unit->global_parameters()))
    return true; /* purecov: inspected */

  SELECT_LEX *save_select = thd->lex->current_select();
  thd->lex->set_current_select(select_lex);
  const bool ret = select_lex->prepare(thd);
  if (!ret) unit->set_prepared();
  thd->lex->set_current_select(save_select);
  return ret;
}

bool subselect_union_engine::prepare() {
  if (!unit->is_prepared())
    return unit->prepare(unit->thd, result, SELECT_NO_UNLOCK, 0);

  DBUG_ASSERT(result == unit->query_result());

  return false;
}

bool subselect_indexsubquery_engine::prepare() {
  /* Should never be called. */
  DBUG_ASSERT(false);
  return 1;
}

/**
  Makes storage for the output values for a scalar or row subquery and
  calculates their data and column types and their nullability. Only
  to be called on engines that represent scalar or row subqueries
  (that is, subselect_single_select_engine and subselect_union_engine).

  @param item_list       list of items in the select list of the subquery
  @param row             cache objects to hold the result row of the subquery
  @param possibly_empty  true if the subquery could return empty result
*/
void subselect_engine::set_row(List<Item> &item_list, Item_cache **row,
                               bool possibly_empty) {
  DBUG_ASSERT(engine_type() == SINGLE_SELECT_ENGINE ||
              engine_type() == UNION_ENGINE);

  /*
    Empty scalar or row subqueries evaluate to NULL, so if it is
    possibly empty, it is also possibly NULL.
  */
  maybe_null = possibly_empty;

  Item *sel_item;
  List_iterator_fast<Item> li(item_list);
  res_type = STRING_RESULT;
  res_field_type = MYSQL_TYPE_VARCHAR;
  for (uint i = 0; (sel_item = li++); i++) {
    item->max_length = sel_item->max_length;
    res_type = sel_item->result_type();
    res_field_type = sel_item->data_type();
    item->decimals = sel_item->decimals;
    item->unsigned_flag = sel_item->unsigned_flag;
    maybe_null |= sel_item->maybe_null;
    if (!(row[i] = Item_cache::get_cache(sel_item))) return;
    row[i]->setup(sel_item);
    row[i]->store(sel_item);
    row[i]->maybe_null = possibly_empty || sel_item->maybe_null;
  }
  if (item_list.elements > 1)
    res_type = ROW_RESULT;
  else
    item->set_data_type(res_field_type);
}

/**
  Check if a query block is guaranteed to return one row. We know that
  this is the case if it has no tables and is not filtered with WHERE,
  HAVING or LIMIT clauses.

  @param select_lex  the SELECT_LEX of the query block to check

  @return true if we are certain that the query block always returns
  one row, false otherwise
*/
static bool guaranteed_one_row(const SELECT_LEX *select_lex) {
  return select_lex->table_list.elements == 0 && !select_lex->where_cond() &&
         !select_lex->having_cond() && !select_lex->select_limit;
}

void subselect_single_select_engine::fix_length_and_dec(Item_cache **row) {
  DBUG_ASSERT(row || select_lex->item_list.elements == 1);
  set_row(select_lex->item_list, row, !guaranteed_one_row(select_lex));
  item->collation.set(row[0]->collation);
}

void subselect_union_engine::fix_length_and_dec(Item_cache **row) {
  DBUG_ASSERT(row || unit->first_select()->item_list.elements == 1);

  // A UNION is possibly empty only if all of its SELECTs are possibly empty.
  bool possibly_empty = true;
  for (SELECT_LEX *sl = unit->first_select(); sl; sl = sl->next_select()) {
    if (guaranteed_one_row(sl)) {
      possibly_empty = false;
      break;
    }
  }

  set_row(unit->item_list, row, possibly_empty);
  if (unit->first_select()->item_list.elements == 1)
    item->collation.set(row[0]->collation);
}

void subselect_indexsubquery_engine::fix_length_and_dec(Item_cache **) {
  // this never should be called
  DBUG_ASSERT(0);
}

bool subselect_single_select_engine::exec() {
  DBUG_ENTER("subselect_single_select_engine::exec");

  int rc = 0;
  SELECT_LEX_UNIT *const unit = item->unit;
  THD *const thd = unit->thd;
  char const *save_where = thd->where;
  SELECT_LEX *save_select = thd->lex->current_select();
  thd->lex->set_current_select(select_lex);

  JOIN *const join = select_lex->join;

  DBUG_ASSERT(join->is_optimized());

  if (select_lex->uncacheable && unit->is_executed()) {
    join->reset();
    item->reset();
    unit->reset_executed();
    item->assigned(false);
  }
  if (!unit->is_executed()) {
    item->reset_value_registration();

    if (unit->set_limit(thd, unit->global_parameters()))
      DBUG_RETURN(true); /* purecov: inspected */
    join->exec();
    unit->set_executed();

    rc = join->error || thd->is_fatal_error;
  }

  thd->where = save_where;
  thd->lex->set_current_select(save_select);
  DBUG_RETURN(rc);
}

bool subselect_union_engine::exec() {
  THD *const thd = unit->thd;
  DBUG_ASSERT(thd == item->unit->thd);
  DBUG_ASSERT(unit->is_optimized());
  char const *save_where = thd->where;
  const bool res = unit->execute(thd);
  thd->where = save_where;
  return res;
}

/**
  Search, using a table scan, for at least one row satisfying select
  condition.

  The caller must set item's 'value' to 'false' before calling this
  function. This function will set it to 'true' if it finds a matching row.

  @returns false if ok, true if read error.
*/
bool subselect_indexsubquery_engine::scan_table() {
  int error;
  TABLE *table = tab->table();
  DBUG_ENTER("subselect_indexsubquery_engine::scan_table");

  // We never need to do a table scan of the materialized table.
  DBUG_ASSERT(engine_type() != HASH_SJ_ENGINE);

  if ((table->file->inited && (error = table->file->ha_index_end())) ||
      (error = table->file->ha_rnd_init(1))) {
    (void)report_handler_error(table, error);
    DBUG_RETURN(true);
  }

  for (;;) {
    error = table->file->ha_rnd_next(table->record[0]);
    if (error && error != HA_ERR_END_OF_FILE) {
      error = report_handler_error(table, error);
      break;
    }
    /* No more rows */
    if (!table->has_row()) break;

    if (!cond || cond->val_int()) {
      static_cast<Item_in_subselect *>(item)->value = true;
      break;
    }
  }

  table->file->ha_rnd_end();
  DBUG_RETURN(error != 0);
}

/**
  Copy ref key and check for null parts in it

  Construct a search tuple to be used for index lookup. If one of the
  key parts have a NULL value, the following logic applies:

  For top level items, e.g.

     "WHERE <outer_value_list> IN (SELECT <inner_value_list>...)"

  where one of the outer values are NULL, the IN predicate evaluates
  to false/UNKNOWN (we don't care) and it's not necessary to evaluate
  the subquery. That shortcut is taken in
  Item_in_optimizer::val_int(). Thus, if a key part with a NULL value
  is found here, the NULL is either not outer or this subquery is not
  top level. Therefore we cannot shortcut subquery execution if a NULL
  is found here.

  Thus, if one of the key parts have a NULL value there are two
  possibilities:

  a) The NULL is from the outer_value_list. Since this is not a top
     level item (see above) we need to check whether this predicate
     evaluates to NULL or false. That is done by checking if the
     subquery has a row if the conditions based on outer NULL values
     are disabled. Index lookup cannot be used for this, so a table
     scan must be done.

  b) The NULL is local to the subquery, e.g.:

        "WHERE ... IN (SELECT ... WHERE inner_col IS NULL)"

     In this case we're looking for rows with the exact inner_col
     value of NULL, not rows that match if the "inner_col IS NULL"
     condition is disabled. Index lookup can be used for this.

  @see subselect_indexsubquery_engine::exec()
  @see Item_in_optimizer::val_int()

  @param[out] require_scan   true if a NULL value is found that falls
                             into category a) above, false if index
                             lookup can be used.
  @param[out] convert_error  true if an error occured during conversion
                             of values from one type to another, false
                             otherwise.

*/
void subselect_indexsubquery_engine::copy_ref_key(bool *require_scan,
                                                  bool *convert_error) {
  DBUG_ENTER("subselect_indexsubquery_engine::copy_ref_key");

  *require_scan = false;
  *convert_error = false;
  for (uint part_no = 0; part_no < tab->ref().key_parts; part_no++) {
    store_key *s_key = tab->ref().key_copy[part_no];
    if (s_key == NULL)
      continue;  // key is const and does not need to be reevaluated

    const enum store_key::store_key_result store_res = s_key->copy();
    tab->ref().key_err = store_res;

    if (s_key->null_key) {
      /*
        If we have materialized the subquery (HASH_SJ_ENGINE):
        - this NULL ref item cannot be local to the subquery (any such
        equality condition is attached to the subquery's JOIN and is thus
        handled during materialization (by join->exec() in
        subselect_hash_sj_engine::exec())
        - The case of an outer NULL ref item is caught in
        subselect_hash_sj_engine::exec() so shouldn't come here; but this is
        not guaranteed if the outer expression is not deterministic: this
        expression is evaluated early in Item_in_subselect::exec() (for
        left_expr_cache) and then in s_key->copy() just above; so it is
        possible that it is non-NULL (so, not caught) then NULL (so, coming
        here). In such case, there is no meaningful value for IN, any value
        will do.
      */

      /*
        NULL value is from the outer_value_list if the key part has a
        cond guard that deactivates the condition. @see
        TABLE_REF::cond_guards
      */
      if (tab->ref().cond_guards && tab->ref().cond_guards[part_no] &&
          !*tab->ref().cond_guards[part_no]) {
        DBUG_ASSERT(
            !(static_cast<Item_in_subselect *>(item)->is_top_level_item()));

        *require_scan = true;
        DBUG_VOID_RETURN;
      }
    }

    /*
      Check if the error is equal to STORE_KEY_FATAL. This is not expressed
      using the store_key::store_key_result enum because ref().key_err is a
      boolean and we want to detect both true and STORE_KEY_FATAL from the
      space of the union of the values of [TRUE, FALSE] and
      store_key::store_key_result.
      TODO: fix the variable an return types.
    */
    if (store_res == store_key::STORE_KEY_FATAL) {
      /*
       Error converting the left IN operand to the column type of the right
       IN operand.
      */
      tab->table()->set_no_row();
      *convert_error = true;
      DBUG_VOID_RETURN;
    }
  }
  DBUG_VOID_RETURN;
}

/*
  Index-lookup subselect 'engine' - run the subquery

  SYNOPSIS
    subselect_indexsubquery_engine:exec()
      full_scan

  DESCRIPTION
    The engine is used to resolve subqueries in form

      oe IN (SELECT key FROM tbl WHERE subq_where)

    The value of the predicate is calculated as follows:
    1. If oe IS NULL, this is a special case, do a full table scan on
       table tbl and search for row that satisfies subq_where. If such
       row is found, return NULL, otherwise return false.
    2. Make an index lookup via key=oe, search for a row that satisfies
       subq_where. If found, return true.
    3. If check_null==true, make another lookup via key=NULL, search for a
       row that satisfies subq_where. If found, return NULL, otherwise
       return false.
    4. If unique==true, there can be only one row with key=oe and only one row
       with key=NULL, we use that fact to shorten the search process.

  TODO
    The step #1 can be optimized further when the index has several key
    parts. Consider a subquery:

      (oe1, oe2) IN (SELECT keypart1, keypart2 FROM tbl WHERE subq_where)

    and suppose we need to evaluate it for {oe1, oe2}=={const1, NULL}.
    Current code will do a full table scan and obtain correct result. There
    is a better option: instead of evaluating

      SELECT keypart1, keypart2 FROM tbl WHERE subq_where            (1)

    and checking if it has produced any matching rows, evaluate

      SELECT keypart2 FROM tbl WHERE subq_where AND keypart1=const1  (2)

    If this query produces a row, the result is NULL (as we're evaluating
    "(const1, NULL) IN { (const1, X), ... }", which has a value of UNKNOWN,
    i.e. NULL).  If the query produces no rows, the result is false.

    We currently evaluate (1) by doing a full table scan. (2) can be
    evaluated by doing a "ref" scan on "keypart1=const1", which can be much
    cheaper. We can use index statistics to quickly check whether "ref" scan
    will be cheaper than full table scan.

  RETURN
    0
    1
*/

bool subselect_indexsubquery_engine::exec() {
  DBUG_ENTER("subselect_indexsubquery_engine::exec");
  int error;
  bool null_finding = 0;
  TABLE *const table = tab->table();
  uchar *key;
  uint key_length;
  key_part_map key_parts_map;
  ulonglong tmp_hash;
  const bool unique = tab->type() == JT_EQ_REF;
  const bool check_null = tab->type() == JT_REF_OR_NULL;

  // 'tl' is NULL if this is a tmp table created by subselect_hash_sj_engine.
  TABLE_LIST *const tl = tab->table_ref;
  Item_in_subselect *const item_in = static_cast<Item_in_subselect *>(item);
  item_in->value = false;

  if (tl && tl->uses_materialization() && !table->materialized) {
    THD *const thd = table->in_use;
    bool err = tl->create_materialized_table(thd);
    if (!err) {
      if (tl->is_table_function())
        err = tl->table_function->fill_result_table();
      else {
        err = tl->materialize_derived(thd);
        err |= tl->cleanup_derived();
      }
    }
    if (err) DBUG_RETURN(true); /* purecov: inspected */
  }

  if (check_null) {
    /* We need to check for NULL if there wasn't a matching value */
    *tab->ref().null_ref_key = 0;  // Search first for not null
    item_in->was_null = false;
  }

  /* Copy the ref key and check for nulls... */
  bool require_scan, convert_error;
  hash = 0;
  copy_ref_key(&require_scan, &convert_error);
  if (convert_error) DBUG_RETURN(0);

  if (require_scan) {
    const bool scan_result = scan_table();
    DBUG_RETURN(scan_result);
  }

  if (!table->file->inited && (error = table->file->ha_index_init(
                                   tab->ref().key, !unique /* sorted */))) {
    (void)report_handler_error(table, error);
    DBUG_RETURN(true);
  }
  if (table->hash_field) {
    /*
      Create key of proper endianness, hash_field->ptr can't be use directly
      as it will be overwritten during read.
    */
    table->hash_field->store(hash, true);
    memcpy(&tmp_hash, table->hash_field->ptr, sizeof(ulonglong));
    key = (uchar *)&tmp_hash;
    key_length = sizeof(hash);
    key_parts_map = 1;
  } else {
    key = tab->ref().key_buff;
    key_length = tab->ref().key_length;
    key_parts_map = make_prev_keypart_map(tab->ref().key_parts);
  }
  error = table->file->ha_index_read_map(table->record[0], key, key_parts_map,
                                         HA_READ_KEY_EXACT);
  if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    error = report_handler_error(table, error);
  else {
    for (;;) {
      error = 0;
      if (table->has_row()) {
        if ((!cond || cond->val_int()) && (!having || having->val_int())) {
          item_in->value = true;
          if (null_finding) {
            /*
              This is dead code; subqueries with check_null==true are always
              transformed with IN-to-EXISTS and thus their artificial HAVING
              rejects NULL values...
            */
            DBUG_ASSERT(false);
            item_in->was_null = true;
          }
          break;
        }
        if (unique) break;
        error =
            table->file->ha_index_next_same(table->record[0], key, key_length);
        if (error && error != HA_ERR_END_OF_FILE) {
          error = report_handler_error(table, error);
          break;
        }
      } else {
        if (!check_null || null_finding)
          break; /* We don't need to check nulls */
        /*
          Check if there exists a row with a null value in the index. We come
          here only if ref_or_null, and ref_or_null is always on a single
          column (first keypart of the index). So we have only one NULL bit to
          turn on:
        */
        *tab->ref().null_ref_key = 1;
        null_finding = 1;
        if ((error = (safe_index_read(tab) == 1))) break;
      }
    }
  }
  item->unit->set_executed();
  DBUG_RETURN(error != 0);
}

uint subselect_single_select_engine::cols() const {
  return select_lex->item_list.elements;
}

uint subselect_union_engine::cols() const {
  DBUG_ASSERT(unit->is_prepared());  // should be called after fix_fields()
  return unit->types.elements;
}

uint8 subselect_single_select_engine::uncacheable() const {
  return select_lex->uncacheable;
}

uint8 subselect_union_engine::uncacheable() const { return unit->uncacheable; }

void subselect_single_select_engine::exclude() {
  select_lex->master_unit()->exclude_level();
}

void subselect_union_engine::exclude() { unit->exclude_level(); }

void subselect_indexsubquery_engine::exclude() {
  // this never should be called
  DBUG_ASSERT(0);
}

table_map subselect_engine::calc_const_tables(TABLE_LIST *table) {
  table_map map = 0;
  for (; table; table = table->next_leaf) {
    TABLE *tbl = table->table;
    if (tbl && tbl->const_table) map |= table->map();
  }
  return map;
}

table_map subselect_single_select_engine::upper_select_const_tables() const {
  return calc_const_tables(select_lex->outer_select()->leaf_tables);
}

table_map subselect_union_engine::upper_select_const_tables() const {
  return calc_const_tables(unit->outer_select()->leaf_tables);
}

void subselect_single_select_engine::print(String *str,
                                           enum_query_type query_type) {
  item->unit->print(str, query_type);
}

void subselect_union_engine::print(String *str, enum_query_type query_type) {
  unit->print(str, query_type);
}

/*
TODO:
The ::print method below should be changed as follows. Do it after
all other tests pass.

void subselect_indexsubquery_engine::print(String *str)
{
  KEY *key_info= tab->table->key_info + tab->ref().key;
  str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  for (uint i= 0; i < key_info->key_parts; i++)
    tab->ref().items[i]->print(str);
  str->append(STRING_WITH_LEN(" in "));
  str->append(tab->table->s->table_name.str, tab->table->s->table_name.length);
  str->append(STRING_WITH_LEN(" on "));
  str->append(key_info->name);
  if (cond)
  {
    str->append(STRING_WITH_LEN(" where "));
    cond->print(str);
  }
  str->append(')');
}
*/

void subselect_indexsubquery_engine::print(String *str,
                                           enum_query_type query_type) {
  const bool unique = tab->type() == JT_EQ_REF;
  const bool check_null = tab->type() == JT_REF_OR_NULL;

  if (unique)
    str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  else
    str->append(STRING_WITH_LEN("<index_lookup>("));
  tab->ref().items[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" in "));
  TABLE *const table = tab->table();
  if (tab->table_ref && tab->table_ref->uses_materialization()) {
    /*
      For materialized derived tables/views use table/view alias instead of
      temporary table name, as it changes on each run and not acceptable for
      EXPLAIN EXTENDED.
    */
    str->append(table->alias, strlen(table->alias));
  } else if (table->s->table_category == TABLE_CATEGORY_TEMPORARY) {
    // Could be from subselect_hash_sj_engine.
    str->append(STRING_WITH_LEN("<temporary table>"));
  } else
    str->append(table->s->table_name.str, table->s->table_name.length);
  KEY *key_info = table->key_info + tab->ref().key;
  str->append(STRING_WITH_LEN(" on "));
  str->append(key_info->name);
  if (check_null) str->append(STRING_WITH_LEN(" checking NULL"));
  if (cond) {
    str->append(STRING_WITH_LEN(" where "));
    cond->print(str, query_type);
  }
  if (having) {
    str->append(STRING_WITH_LEN(" having "));
    having->print(str, query_type);
  }
  str->append(')');
}

/**
  change query result object of engine.

  @param si		new subselect Item
  @param res		new Query_result object

  @retval
    false OK
  @retval
    true  error
*/

bool subselect_single_select_engine::change_query_result(
    Item_subselect *si, Query_result_subquery *res) {
  item = si;
  result = res;
  return select_lex->change_query_result(result, NULL);
}

/**
  change query result object of engine.

  @param si		new subselect Item
  @param res		new Query_result object

  @retval
    false OK
  @retval
    true  error
*/

bool subselect_union_engine::change_query_result(Item_subselect *si,
                                                 Query_result_subquery *res) {
  item = si;
  int rc = unit->change_query_result(res, result);
  result = res;
  return rc;
}

/**
  change query result emulation, never should be called.

  @retval
    false OK
  @retval
    true  error
*/

bool subselect_indexsubquery_engine::change_query_result(
    Item_subselect *, Query_result_subquery *) {
  DBUG_ASSERT(0);
  return true;
}

/******************************************************************************
  WL#1110 - Implementation of class subselect_hash_sj_engine
******************************************************************************/

/**
  Create all structures needed for subquery execution using hash semijoin.

  @details
  - Create a temporary table to store the result of the IN subquery. The
    temporary table has one hash index on all its columns. If single-column,
    the index allows at most one NULL row.
  - Create a new result sink that sends the result stream of the subquery to
    the temporary table,
  - Create and initialize a new JOIN_TAB, and TABLE_REF objects to perform
    lookups into the indexed temporary table.

  @param tmp_columns  columns of temporary table

  @note
    Currently Item_subselect::init() already chooses and creates at parse
    time an engine with a corresponding JOIN to execute the subquery.

  @retval true  if error
  @retval false otherwise
*/

bool subselect_hash_sj_engine::setup(List<Item> *tmp_columns) {
  /* The result sink where we will materialize the subquery result. */
  Query_result_union *tmp_result_sink;
  /* The table into which the subquery is materialized. */
  TABLE *tmp_table;
  KEY *tmp_key;       /* The only index on the temporary table. */
  uint tmp_key_parts; /* Number of keyparts in tmp_key. */
  Item_in_subselect *item_in = (Item_in_subselect *)item;
  uint key_length;

  DBUG_ENTER("subselect_hash_sj_engine::setup");

  DBUG_EXECUTE_IF("hash_semijoin_fail_in_setup", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    DBUG_RETURN(true);
  });

  /* 1. Create/initialize materialization related objects. */

  /*
    Create and initialize a select result interceptor that stores the
    result stream in a temporary table. The temporary table itself is
    managed (created/filled/etc) internally by the interceptor.
  */
  THD *const thd = item->unit->thd;
  if (!(tmp_result_sink = new (*THR_MALLOC) Query_result_union(thd)))
    DBUG_RETURN(true);
  if (tmp_result_sink->create_result_table(
          thd, tmp_columns,
          true,  // Eliminate duplicates
          thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS,
          "materialized-subquery", true, true))
    DBUG_RETURN(true);

  tmp_table = tmp_result_sink->table;
  tmp_key = tmp_table->key_info;
  if (tmp_table->hash_field) {
    tmp_key_parts = tmp_columns->elements;
    key_length = ALIGN_SIZE(tmp_table->s->reclength);
  } else {
    tmp_key_parts = tmp_key->user_defined_key_parts;
    key_length = ALIGN_SIZE(tmp_key->key_length) * 2;
  }

  result = tmp_result_sink;

  /*
    Make sure there is only one index on the temp table.
  */
  DBUG_ASSERT(tmp_columns->elements == tmp_table->s->fields ||
              // Unique constraint is used and a hash field was added
              (tmp_table->hash_field &&
               tmp_columns->elements == (tmp_table->s->fields - 1)));
  /* 2. Create/initialize execution related objects. */

  /*
    Create and initialize the JOIN_TAB that represents an index lookup
    plan operator into the materialized subquery result. Notice that:
    - this JOIN_TAB has no corresponding JOIN (and doesn't need one), and
    - here we initialize only those members that are used by
      subselect_indexsubquery_engine, so these objects are incomplete.
  */

  QEP_TAB_standalone *tmp_tab_st = new (thd->mem_root) QEP_TAB_standalone;
  if (tmp_tab_st == NULL) DBUG_RETURN(true);
  tab = &tmp_tab_st->as_QEP_TAB();
  tab->set_table(tmp_table);
  tab->ref().key = 0; /* The only temp table index. */
  tab->ref().key_length = tmp_key->key_length;
  tab->set_type((tmp_table->key_info[0].flags & HA_NOSAME) ? JT_EQ_REF
                                                           : JT_REF);
  if (!(tab->ref().key_buff = (uchar *)thd->mem_calloc(key_length)) ||
      !(tab->ref().key_copy =
            (store_key **)thd->alloc((sizeof(store_key *) * tmp_key_parts))) ||
      !(tab->ref().items = (Item **)thd->alloc(sizeof(Item *) * tmp_key_parts)))
    DBUG_RETURN(true);

  uchar *cur_ref_buff = tab->ref().key_buff;

  /*
    Like semijoin-materialization-lookup (see create_subquery_equalities()),
    create an artificial condition to post-filter those rows matched by index
    lookups that cannot be distinguished by the index lookup procedure, for
    example:
    - because of truncation (if the outer column type's length is bigger than
    the inner column type's, index lookup will use a truncated outer
    value as search key, yielding false positives).
    - because the index is over hash_field and thus not unique.

    Prepared statements execution requires that fix_fields is called
    for every execution. In order to call fix_fields we need to create a
    Name_resolution_context and a corresponding TABLE_LIST for the temporary
    table for the subquery, so that all column references to the materialized
    subquery table can be resolved correctly.
  */
  DBUG_ASSERT(cond == NULL);
  if (!(cond = new Item_cond_and)) DBUG_RETURN(true);
  /*
    Table reference for tmp_table that is used to resolve column references
    (Item_fields) to columns in tmp_table.
  */
  TABLE_LIST *tmp_table_ref =
      new (thd->mem_root) TABLE_LIST(tmp_table, "", 0, "materialized-subquery",
                                     21, "materialized-subquery", TL_READ);
  if (tmp_table_ref == nullptr) DBUG_RETURN(true);

  /* Name resolution context for all tmp_table columns created below. */
  Name_resolution_context *context = new (*THR_MALLOC) Name_resolution_context;
  context->init();
  context->first_name_resolution_table = context->last_name_resolution_table =
      tmp_table_ref;

  KEY_PART_INFO *key_parts = tmp_key->key_part;
  for (uint part_no = 0; part_no < tmp_key_parts; part_no++) {
    /* New equi-join condition for the current column. */
    Item_func_eq *eq_cond;
    /* Item for the corresponding field from the materialized temp table. */
    Item_field *right_col_item;
    Field *field = tmp_table->visible_field_ptr()[part_no];
    const bool nullable = field->real_maybe_null();
    tab->ref().items[part_no] = item_in->left_expr->element_index(part_no);

    if (!(right_col_item = new Item_field(thd, context, field)) ||
        !(eq_cond =
              new Item_func_eq(tab->ref().items[part_no], right_col_item)) ||
        ((Item_cond_and *)cond)->add(eq_cond)) {
      delete cond;
      cond = NULL;
      DBUG_RETURN(true);
    }

    if (tmp_table->hash_field)
      tab->ref().key_copy[part_no] = new (*THR_MALLOC)
          store_key_hash_item(thd, field, cur_ref_buff, 0, field->pack_length(),
                              tab->ref().items[part_no], &hash);
    else
      tab->ref().key_copy[part_no] = new (*THR_MALLOC) store_key_item(
          thd, field,
          /* TODO:
             the NULL byte is taken into account in
             key_parts[part_no].store_length, so instead of
             cur_ref_buff + test(maybe_null), we could
             use that information instead.
           */
          cur_ref_buff + (nullable ? 1 : 0), nullable ? cur_ref_buff : 0,
          key_parts[part_no].length, tab->ref().items[part_no]);
    if (nullable &&  // nullable column in tmp table,
                     // and UNKNOWN should not be interpreted as FALSE
        !item_in->is_top_level_item()) {
      // It must be the single column, or we wouldn't be here
      DBUG_ASSERT(tmp_key_parts == 1);
      // Be ready to search for NULL into inner column:
      tab->ref().null_ref_key = cur_ref_buff;
      mat_table_has_nulls = NEX_UNKNOWN;
    } else {
      tab->ref().null_ref_key = NULL;
      mat_table_has_nulls = NEX_IRRELEVANT_OR_FALSE;
    }

    if (tmp_table->hash_field)
      cur_ref_buff += field->pack_length();
    else
      cur_ref_buff += key_parts[part_no].store_length;
  }
  tab->ref().key_err = 1;
  tab->ref().key_parts = tmp_key_parts;

  if (cond->fix_fields(thd, &cond)) DBUG_RETURN(true);

  /*
    Create and optimize the JOIN that will be used to materialize
    the subquery if not yet created.
  */
  materialize_engine->prepare();
  /* Let our engine reuse this query plan for materialization. */
  materialize_engine->select_lex->change_query_result(result, NULL);

  DBUG_RETURN(false);
}

subselect_hash_sj_engine::~subselect_hash_sj_engine() {
  /* Assure that cleanup has been called for this engine. */
  DBUG_ASSERT(!tab);

  destroy(result);
}

/**
  Cleanup performed after each PS execution.

  @details
  Called in the end of SELECT_LEX::prepare for PS from
  Item_subselect::cleanup.
*/

void subselect_hash_sj_engine::cleanup() {
  DBUG_ENTER("subselect_hash_sj_engine::cleanup");
  is_materialized = false;
  if (result != nullptr) result->cleanup(); /* Resets the temp table as well. */
  THD *const thd = item->unit->thd;
  DEBUG_SYNC(thd, "before_index_end_in_subselect");
  if (tab != nullptr) {
    TABLE *const table = tab->table();
    if (table->file->inited)
      table->file->ha_index_end();  // Close the scan over the index
    free_tmp_table(thd, table);
    // Note that tab->qep_cleanup() is not called
    tab = nullptr;
  }
  materialize_engine->cleanup();
  DBUG_VOID_RETURN;
}

/**
  Execute a subquery IN predicate via materialization.

  If needed materialize the subquery into a temporary table, then
  compute the predicate via a lookup into this table.

  @retval true  if error
  @retval false otherwise
*/

bool subselect_hash_sj_engine::exec() {
  Item_in_subselect *item_in = (Item_in_subselect *)item;
  TABLE *const table = tab->table();
  DBUG_ENTER("subselect_hash_sj_engine::exec");

  /*
    Optimize and materialize the subquery during the first execution of
    the subquery predicate.
  */
  if (!is_materialized) {
    bool res;
    THD *const thd = item->unit->thd;
    SELECT_LEX *save_select = thd->lex->current_select();
    thd->lex->set_current_select(materialize_engine->select_lex);
    DBUG_ASSERT(materialize_engine->select_lex->master_unit()->is_optimized());

    JOIN *join = materialize_engine->select_lex->join;

    join->exec();
    if ((res = join->error || thd->is_fatal_error)) goto err;

    /*
      TODO:
      - Unlock all subquery tables as we don't need them. To implement this
        we need to add new functionality to JOIN::join_free that can unlock
        all tables in a subquery (and all its subqueries).
      - The temp table used for grouping in the subquery can be freed
        immediately after materialization (yet it's done together with
        unlocking).
     */
    is_materialized = true;

    // Calculate row count:
    table->file->info(HA_STATUS_VARIABLE);

    if (!(table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT)) {
      // index must be closed before ha_records() is called
      if (table->file->inited) table->file->ha_index_or_rnd_end();
      ha_rows num_rows = 0;
      table->file->ha_records(&num_rows);
      table->file->stats.records = num_rows;
      res = thd->is_error();
    }

    /* Set tmp_param only if its usable, i.e. there are Copy_field's. */
    tmp_param = &(item_in->unit->outer_select()->join->tmp_table_param);
    if (tmp_param && tmp_param->copy_fields.empty()) tmp_param = nullptr;

  err:
    thd->lex->set_current_select(save_select);
    if (res) DBUG_RETURN(res);
  }  // if (!is_materialized)

  if (table->file->stats.records == 0) {
    // The correct answer is FALSE.
    item_in->value = false;
    DBUG_RETURN(false);
  }
  /*
    Here we could be brutal and set item_in->null_value. But we prefer to be
    well-behaved and rather set the properties which
    Item_in_subselect::val_bool() and Item_in_optimizer::val_int() expect,
    and then those functions will set null_value based on those properties.
  */
  if (item_in->left_expr->element_index(0)->null_value) {
    /*
      The first outer expression oe1 is NULL. It is the single outer
      expression because if there would be more ((oe1,oe2,...)IN(...)) then
      either they would be non-nullable (so we wouldn't be here) or the
      predicate would be top-level (so we wouldn't be here,
      Item_in_optimizer::val_int() would have short-cut). The correct answer
      is UNKNOWN. Do as if searching with all triggered conditions disabled:
      this would surely find a row. The caller will translate this to UNKNOWN.
    */
    DBUG_ASSERT(item_in->left_expr->element_index(0)->maybe_null);
    DBUG_ASSERT(item_in->left_expr->cols() == 1);
    item_in->value = true;
    DBUG_RETURN(false);
  }

  if (subselect_indexsubquery_engine::exec())  // Search with index
    DBUG_RETURN(true);

  if (!item_in->value &&  // no exact match
      mat_table_has_nulls != NEX_IRRELEVANT_OR_FALSE) {
    /*
      There is only one outer expression. It's not NULL. exec() above has set
      the answer to FALSE, but if there exists an inner NULL in the temporary
      table, then the correct answer is UNKNOWN, so let's find out.
    */
    if (mat_table_has_nulls == NEX_UNKNOWN)  // We do not know yet
    {
      // Search for NULL inside tmp table, and remember the outcome.
      *tab->ref().null_ref_key = 1;
      if (!table->file->inited &&
          table->file->ha_index_init(tab->ref().key, false /* sorted */))
        DBUG_RETURN(true);
      if (safe_index_read(tab) == 1) DBUG_RETURN(true);
      *tab->ref().null_ref_key = 0;  // prepare for next searches of non-NULL
      mat_table_has_nulls =
          table->has_row() ? NEX_TRUE : NEX_IRRELEVANT_OR_FALSE;
    }
    if (mat_table_has_nulls == NEX_TRUE) {
      /*
        There exists an inner NULL. The correct answer is UNKNOWN.
        Do as if searching with all triggered conditions enabled; that
        would not find any match, but Item_is_not_null_test would notice a
        NULL:
      */
      item_in->value = false;
      item_in->was_null = true;
    }
  }
  DBUG_RETURN(false);
}

/**
  Print the state of this engine into a string for debugging and views.
*/

void subselect_hash_sj_engine::print(String *str, enum_query_type query_type) {
  str->append(STRING_WITH_LEN(" <materialize> ("));
  materialize_engine->print(str, query_type);
  str->append(STRING_WITH_LEN(" ), "));
  if (tab)
    subselect_indexsubquery_engine::print(str, query_type);
  else
    str->append(
        STRING_WITH_LEN("<the access method for lookups is not yet created>"));
}

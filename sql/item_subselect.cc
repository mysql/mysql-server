/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file

  @brief
  subselect Item

  @todo
    - add function from mysql_select that use JOIN* as parameter to JOIN
    methods (sql_select.h/sql_select.cc)
*/

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // set_var.h: THD
#include "set_var.h"
#include "sql_select.h"
#include "opt_trace.h"
#include "sql_parse.h"                          // check_stack_overrun
#include "sql_derived.h"                        // mysql_derived_create, ...
#include "debug_sync.h"
#include "sql_test.h"
#include "sql_join_buffer.h"                    // JOIN_CACHE
#include "sql_optimizer.h"                      // JOIN
#include "opt_explain_format.h"

Item_subselect::Item_subselect():
  Item_result_field(), value_assigned(0), traced_before(false),
  substitution(NULL), in_cond_of_tab(INT_MIN), engine(NULL), old_engine(NULL),
  used_tables_cache(0), have_to_be_excluded(0), const_item_cache(1),
  engine_changed(false), changed(false)
{
  with_subselect= 1;
  reset();
  /*
    Item value is NULL if select_result_interceptor didn't change this value
    (i.e. some rows will be found returned)
  */
  null_value= TRUE;
}


void Item_subselect::init(st_select_lex *select_lex,
			  select_result_interceptor *result)
{
  /*
    Please see Item_singlerow_subselect::invalidate_and_restore_select_lex(),
    which depends on alterations to the parse tree implemented here.
  */

  DBUG_ENTER("Item_subselect::init");
  DBUG_PRINT("enter", ("select_lex: 0x%lx", (long) select_lex));
  unit= select_lex->master_unit();

  if (unit->item)
  {
    /*
      Item can be changed in JOIN::prepare while engine in JOIN::optimize
      => we do not copy old_engine here
    */
    engine= unit->item->engine;
    parsing_place= unit->item->parsing_place;
    unit->item->engine= 0;
    unit->item= this;
    engine->change_result(this, result);
  }
  else
  {
    SELECT_LEX *outer_select= unit->outer_select();
    /*
      do not take into account expression inside aggregate functions because
      they can access original table fields
    */
    parsing_place= (outer_select->in_sum_expr ?
                    NO_MATTER :
                    outer_select->parsing_place);
    if (unit->is_union())
      engine= new subselect_union_engine(unit, result, this);
    else
      engine= new subselect_single_select_engine(select_lex, result, this);
  }
  {
    SELECT_LEX *upper= unit->outer_select();
    if (upper->parsing_place == IN_HAVING)
      upper->subquery_in_having= 1;
  }
  DBUG_VOID_RETURN;
}


void Item_subselect::cleanup()
{
  DBUG_ENTER("Item_subselect::cleanup");
  Item_result_field::cleanup();
  if (old_engine)
  {
    if (engine)
    {
      engine->cleanup();
      delete engine;
    }
    engine= old_engine;
    old_engine= 0;
  }
  if (engine)
    engine->cleanup();
  reset();
  value_assigned= 0;
  traced_before= false;
  in_cond_of_tab= INT_MIN;
  DBUG_VOID_RETURN;
}


void Item_singlerow_subselect::cleanup()
{
  DBUG_ENTER("Item_singlerow_subselect::cleanup");
  value= 0; row= 0;
  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
}


bool Item_in_subselect::finalize_exists_transform(SELECT_LEX *select_lex)
{
  DBUG_ASSERT(exec_method == EXEC_EXISTS_OR_MAT ||
              exec_method == EXEC_EXISTS);
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
  if (unit->thd->stmt_arena->is_conventional()) // not prepared stmt
  {
    uint cnt= select_lex->item_list.elements;
    select_lex->item_list.empty();
    for(; cnt > 0; cnt--)
      select_lex->item_list.push_back(new Item_int(NAME_STRING("Not_used"),
                                                   (longlong) 1,
                                                   MY_INT64_NUM_DECIMAL_DIGITS));
    Opt_trace_context * const trace= &unit->thd->opt_trace;
    OPT_TRACE_TRANSFORM(trace, oto0, oto1,
                        select_lex->select_number,
                        "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
    oto1.add("put_1_in_SELECT_list", true);
  }
  /*
    Note that if the subquery is "SELECT1 UNION SELECT2" then this is not
    working optimally (Bug#14215895).
  */
  unit->global_parameters->select_limit= new Item_int((int32) 1);
  unit->set_limit(unit->global_parameters);

  select_lex->join->allow_outer_refs= true;   // for JOIN::set_prefix_tables()
  exec_method= EXEC_EXISTS;
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

  Code reading suggests that remove_additional_cond() is equivalent to
  "if in_subs->left_expr->cols()==1 then remove_in2exists_conds(where)"
  but that would still not fix Bug#13915291 of remove_additional_cond().

  @param conds  condition
  @returns      new condition
*/
Item *Item_in_subselect::remove_in2exists_conds(Item* conds)
{
  if (conds->created_by_in2exists())
    return NULL;
  if (conds->type() != Item::COND_ITEM)
    return conds;
  Item_cond *cnd= static_cast<Item_cond *>(conds);
  /*
    If IN->EXISTS has added something to 'conds', cnd must be AND list and we
    must inspect each member.
  */
  if (cnd->functype() != Item_func::COND_AND_FUNC)
    return conds;
  List_iterator<Item> li(*(cnd->argument_list()));
  Item *item;
  while ((item= li++))
  {
    // remove() does not invalidate iterator.
    if (item->created_by_in2exists())
      li.remove();
  }
  switch (cnd->argument_list()->elements)
  {
  case 0:
    return NULL;
  case 1: // AND(x) is the same as x, return x
    return cnd->argument_list()->head();
  default: // otherwise return AND
    return conds;
  }
}


bool Item_in_subselect::finalize_materialization_transform(JOIN *join)
{
  DBUG_ASSERT(exec_method == EXEC_EXISTS_OR_MAT);
  DBUG_ASSERT(engine->engine_type() == subselect_engine::SINGLE_SELECT_ENGINE);
  THD * const thd= unit->thd;
  subselect_single_select_engine *old_engine_derived=
    static_cast<subselect_single_select_engine*>(engine);

  DBUG_ASSERT(join == old_engine_derived->join);
  // No UNION in materialized subquery so this holds:
  DBUG_ASSERT(join->select_lex == unit->first_select());
  DBUG_ASSERT(join->unit == unit);
  DBUG_ASSERT(unit->global_parameters->select_limit == NULL);

  exec_method= EXEC_MATERIALIZATION;

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
  if (join->conds)
    join->conds= remove_in2exists_conds(join->conds);
  if (join->having)
    join->having= remove_in2exists_conds(join->having);
  DBUG_ASSERT(!originally_dependent());
  join->select_lex->uncacheable&= ~UNCACHEABLE_DEPENDENT;
  /*
    IN->EXISTS uses master_unit(); however, as we cannot have a UNION here,
    'unit' must be correct too.
  */
  DBUG_ASSERT(unit == join->select_lex->master_unit());
  unit->uncacheable&= ~UNCACHEABLE_DEPENDENT;

  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1,
                      old_engine_derived->join->select_lex->select_number,
                      "IN (SELECT)", "materialization");
  oto1.add("chosen", true);

  subselect_hash_sj_engine * const new_engine=
    new subselect_hash_sj_engine(thd, this, old_engine_derived);
  if (!new_engine)
    return true;
  if (new_engine->setup(unit->get_unit_column_types()))
  {
    /*
      For some reason we cannot use materialization for this IN predicate.
      Delete all materialization-related objects, and return error.
    */
    delete new_engine;
    return true;
  }
  if (change_engine(new_engine))
    return true;

  join->allow_outer_refs= false;              // for JOIN::set_prefix_tables()
  return false;
}


void Item_in_subselect::cleanup()
{
  DBUG_ENTER("Item_in_subselect::cleanup");
  if (left_expr_cache)
  {
    left_expr_cache->delete_elements();
    delete left_expr_cache;
    left_expr_cache= NULL;
  }
  left_expr_cache_filled= false;
  need_expr_cache= TRUE;

  switch(exec_method)
  {
  case EXEC_MATERIALIZATION:
    unit->first_select()->uncacheable|= UNCACHEABLE_DEPENDENT;
    unit->uncacheable|= UNCACHEABLE_DEPENDENT;
    // fall through
  case EXEC_EXISTS:
    /*
      Back to EXISTS_OR_MAT, so that next execution of this statement can
      choose between the two.
    */
    unit->global_parameters->select_limit= NULL;
    exec_method= EXEC_EXISTS_OR_MAT;
    break;
  default:
    break;
  }

  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
}

Item_subselect::~Item_subselect()
{
  delete engine;
}

Item_subselect::trans_res
Item_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_subselect::select_transformer");
  DBUG_RETURN(RES_OK);
}


bool Item_subselect::fix_fields(THD *thd, Item **ref)
{
  char const *save_where= thd->where;
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

  if (check_stack_overrun(thd, STACK_MIN_SIZE, (uchar*)&res))
    return TRUE;

  if (!(res= engine->prepare()))
  {
    // all transformation is done (used by prepared statements)
    changed= 1;

    /*
      Substitute the current item with an Item_in_optimizer that was
      created by Item_in_subselect::select_in_like_transformer and
      call fix_fields for the substituted item which in turn calls
      engine->prepare for the subquery predicate.
    */
    if (substitution)
    {
      int ret= 0;

      // did we changed top item of WHERE condition
      if (unit->outer_select()->where == (*ref))
	unit->outer_select()->where= substitution; // correct WHERE for PS
      else if (unit->outer_select()->having == (*ref))
	unit->outer_select()->having= substitution; // correct HAVING for PS

      (*ref)= substitution;
      substitution->item_name= item_name;
      if (have_to_be_excluded)
	engine->exclude();
      substitution= 0;
      thd->where= "checking transformed subquery";
      if (!(*ref)->fixed)
	ret= (*ref)->fix_fields(thd, ref);
      thd->where= save_where;
      return ret;
    }
    // Is it one field subselect?
    if (engine->cols() > max_columns)
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
      return TRUE;
    }
    fix_length_and_dec();
  }
  else
    goto err;
  
  if ((uncacheable= engine->uncacheable()))
  {
    const_item_cache= 0;
    if (uncacheable & UNCACHEABLE_RAND)
      used_tables_cache|= RAND_TABLE_BIT;
  }
  fixed= 1;

err:
  thd->where= save_where;
  return res;
}

/**
  Workaround for bug in gcc 4.1. @See Item_in_subselect::walk()
*/
bool Item_subselect::walk_body(Item_processor processor, bool walk_subquery,
                               uchar *argument)
{
  if (walk_subquery)
  {
    for (SELECT_LEX *lex= unit->first_select(); lex; lex= lex->next_select())
    {
      List_iterator<Item> li(lex->item_list);
      Item *item;
      ORDER *order;

      if (lex->where && (lex->where)->walk(processor, walk_subquery, argument))
        return 1;
      if (lex->having && (lex->having)->walk(processor, walk_subquery,
                                             argument))
        return 1;

      while ((item=li++))
      {
        if (item->walk(processor, walk_subquery, argument))
          return 1;
      }
      for (order= lex->order_list.first ; order; order= order->next)
      {
        if ((*order->item)->walk(processor, walk_subquery, argument))
          return 1;
      }
      for (order= lex->group_list.first ; order; order= order->next)
      {
        if ((*order->item)->walk(processor, walk_subquery, argument))
          return 1;
      }
    }
  }
  return (this->*processor)(argument);
}

bool Item_subselect::walk(Item_processor processor, bool walk_subquery,
                          uchar *argument)
{
  return walk_body(processor, walk_subquery, argument);
}


/**
  Mark a subquery unit with information provided

  A subquery may belong to WHERE, HAVING, ORDER BY or GROUP BY item trees.
  This "processor" qualifies subqueries by outer clause type.
  
  @note For the WHERE clause of the JOIN query this function also associates
        a related table with the unit.

  @param arg    Explain_subquery_marker structure

  @retval false

  @note We always return "false" as far as we don't want to dive deeper because
        we explain inner subqueries in their joins contexts.
*/

bool Item_subselect::explain_subquery_checker(uchar **arg)
{
  Explain_subquery_marker *m= 
    *reinterpret_cast<Explain_subquery_marker **>(arg);

  if (m->type == CTX_WHERE)
  {
    /*
      A subquery in the WHERE clause may be associated with a few JOIN_TABs
      simultaneously.
    */
    if (unit->explain_marker == CTX_NONE)
      unit->explain_marker= CTX_WHERE;
    else
      DBUG_ASSERT(unit->explain_marker == CTX_WHERE);
    m->destination->register_where_subquery(unit);
    return false;
  }

  if (m->type == CTX_HAVING && unit->explain_marker == CTX_WHERE)
  {
    /*
      This subquery was in SELECT list of outer subquery transformed
      with IN->EXISTS, so is referenced by WHERE and HAVING;
      see Item_in_subselect::single_value_in_to_exists_transformer()
    */
    return false;
  }

  if (unit->explain_marker == CTX_NONE)
    goto overwrite;

  if (unit->explain_marker == m->type)
    return false;

  /*
    GROUP BY subqueries may be listed in different item trees simultaneously:
     1) in GROUP BY items,
     2) in ORDER BY items and/or
     3) in SELECT list.
    If such a subquery in the SELECT list, we mark the subquery as if it
    belongs to SELECT list, otherwise we mark it as "GROUP BY" subquery.

    ORDER BY subqueries may be listed twice in SELECT list and ORDER BY list.
    In this case we mark such a subquery as "SELECT list" subquery.
  */
  if (unit->explain_marker == CTX_GROUP_BY_SQ && m->type == CTX_ORDER_BY_SQ)
    return false;
  if (unit->explain_marker == CTX_ORDER_BY_SQ && m->type == CTX_GROUP_BY_SQ)
    goto overwrite;

  if (unit->explain_marker == CTX_SELECT_LIST &&
      (m->type == CTX_ORDER_BY_SQ || m->type == CTX_GROUP_BY_SQ))
    return false;
  if ((unit->explain_marker == CTX_ORDER_BY_SQ ||
       unit->explain_marker == CTX_GROUP_BY_SQ) && m->type == CTX_SELECT_LIST)
    goto overwrite;

  DBUG_ASSERT(!"Unexpected combination of item trees!");
  return false;

overwrite:
  unit->explain_marker= m->type;
  return false;
}


bool Item_subselect::exec()
{
  DBUG_ENTER("Item_subselect::exec");
  /*
    Do not execute subselect in case of a fatal error
    or if the query has been killed.
  */
  THD * const thd= unit->thd;
  if (thd->is_error() || thd->killed)
    DBUG_RETURN(true);

  DBUG_ASSERT(!thd->lex->context_analysis_only);
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
#ifdef OPTIMIZER_TRACE
  Opt_trace_context * const trace= &thd->opt_trace;
  const bool disable_trace=
    traced_before &&
    !trace->feature_enabled(Opt_trace_context::REPEATED_SUBSELECT);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);
  traced_before= true;

  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "subselect_execution");
  trace_exec.add_select_number(unit->first_select()->select_number);
  Opt_trace_array trace_steps(trace, "steps");
#endif

  bool res= engine->exec();

  if (engine_changed)
  {
    engine_changed= 0;
    res= exec();
    DBUG_RETURN(res);
  }
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

void Item_subselect::fix_after_pullout(st_select_lex *parent_select,
                                       st_select_lex *removed_select)

{
  /* Clear usage information for this subquery predicate object */
  used_tables_cache= 0;

  /*
    Go through all query specification objects of the subquery and re-resolve
    all relevant expressions belonging to them.
  */
  for (SELECT_LEX *sel= unit->first_select(); sel; sel= sel->next_select())
  {
    if (sel->where)
      sel->where->fix_after_pullout(parent_select, removed_select);

    if (sel->having)
      sel->having->fix_after_pullout(parent_select, removed_select);

    List_iterator<Item> li(sel->item_list);
    Item *item;
    while ((item=li++))
      item->fix_after_pullout(parent_select, removed_select);

    /*
      No need to call fix_after_pullout() for outer-join conditions, as these
      cannot have outer references.
    */

    /* Re-resolve ORDER BY and GROUP BY fields */

    for (ORDER *order= (ORDER*) sel->order_list.first;
         order;
         order= order->next)
      (*order->item)->fix_after_pullout(parent_select, removed_select);

    for (ORDER *group= (ORDER*) sel->group_list.first;
         group;
         group= group->next)
      (*group->item)->fix_after_pullout(parent_select, removed_select);
  }
}

bool Item_in_subselect::walk(Item_processor processor, bool walk_subquery,
                             uchar *argument)
{
  if (left_expr->walk(processor, walk_subquery, argument))
    return true;
  /*
    Cannot call "Item_subselect::walk(...)" because with gcc 4.1
    Item_in_subselect::walk() was incorrectly called instead.
    Using Item_subselect::walk_body() instead is a workaround.
  */
  return walk_body(processor, walk_subquery, argument);
}

/*
  Compute the IN predicate if the left operand's cache changed.
*/

bool Item_in_subselect::exec()
{
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
      exec_method == EXEC_MATERIALIZATION &&
      init_left_expr_cache())
    DBUG_RETURN(TRUE);

  if (left_expr_cache != NULL)
  {
    const int result= test_if_item_cache_changed(*left_expr_cache);
    if (left_expr_cache_filled &&               // cache was previously filled
        result < 0)         // new value is identical to previous cached value
    {
      /*
        We needn't do a full execution, can just reuse "value", "was_null",
        "null_value" of the previous execution.
      */
      DBUG_RETURN(false);
    }
    left_expr_cache_filled= true;
  }

  null_value= was_null= false;
  const bool retval= Item_subselect::exec();
  DBUG_RETURN(retval);
}


Item::Type Item_subselect::type() const
{
  return SUBSELECT_ITEM;
}


void Item_subselect::fix_length_and_dec()
{
  engine->fix_length_and_dec(0);
}


table_map Item_subselect::used_tables() const
{
  return (table_map) (engine->uncacheable() ? used_tables_cache : 0L);
}


bool Item_subselect::const_item() const
{
  if (unit->thd->lex->context_analysis_only)
    return false;
  /* Not constant until tables are locked. */
  if (!unit->thd->lex->is_query_tables_locked())
    return false;
  return const_item_cache;
}

Item *Item_subselect::get_tmp_table_item(THD *thd_arg)
{
  if (!with_sum_func && !const_item())
    return new Item_field(result_field);
  return copy_or_same(thd_arg);
}

void Item_subselect::update_used_tables()
{
  if (!engine->uncacheable())
  {
    // did all used tables become static?
    if (!(used_tables_cache & ~engine->upper_select_const_tables()))
      const_item_cache= 1;
  }
}


void Item_subselect::print(String *str, enum_query_type query_type)
{
  if (engine)
  {
    str->append('(');
    engine->print(str, query_type);
    str->append(')');
  }
  else
    str->append("(...)");
}


Item_singlerow_subselect::Item_singlerow_subselect(st_select_lex *select_lex)
  :Item_subselect(), value(0), no_rows(false)
{
  DBUG_ENTER("Item_singlerow_subselect::Item_singlerow_subselect");
  init(select_lex, new select_singlerow_subselect(this));
  maybe_null= 1;
  max_columns= UINT_MAX;
  DBUG_VOID_RETURN;
}

st_select_lex *
Item_singlerow_subselect::invalidate_and_restore_select_lex()
{
  DBUG_ENTER("Item_singlerow_subselect::invalidate_and_restore_select_lex");
  st_select_lex *result= unit->first_select();

  DBUG_ASSERT(result);

  /*
    This code restore the parse tree in it's state before the execution of
    Item_singlerow_subselect::Item_singlerow_subselect(),
    and in particular decouples this object from the SELECT_LEX,
    so that the SELECT_LEX can be used with a different flavor
    or Item_subselect instead, as part of query rewriting.
  */
  unit->item= NULL;

  DBUG_RETURN(result);
}

Item_maxmin_subselect::Item_maxmin_subselect(THD *thd_param,
                                             Item_subselect *parent,
                                             st_select_lex *select_lex,
                                             bool max_arg,
                                             bool ignore_nulls)
  :Item_singlerow_subselect(), was_values(false)
{
  DBUG_ENTER("Item_maxmin_subselect::Item_maxmin_subselect");
  max= max_arg;
  init(select_lex, new select_max_min_finder_subselect(this, max_arg,
                                                       ignore_nulls));
  max_columns= 1;
  maybe_null= 1;
  max_columns= 1;

  /*
    Following information was collected during performing fix_fields()
    of Items belonged to subquery, which will be not repeated
  */
  used_tables_cache= parent->get_used_tables_cache();
  const_item_cache= parent->get_const_item_cache();

  DBUG_VOID_RETURN;
}

void Item_maxmin_subselect::cleanup()
{
  DBUG_ENTER("Item_maxmin_subselect::cleanup");
  Item_singlerow_subselect::cleanup();

  was_values= false;
  DBUG_VOID_RETURN;
}


void Item_maxmin_subselect::print(String *str, enum_query_type query_type)
{
  str->append(max?"<max>":"<min>", 5);
  Item_singlerow_subselect::print(str, query_type);
}


void Item_singlerow_subselect::reset()
{
  null_value= TRUE;
  if (value)
    value->null_value= TRUE;
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
Item_subselect::trans_res
Item_singlerow_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_singlerow_subselect::select_transformer");
  if (changed)
    DBUG_RETURN(RES_OK);

  SELECT_LEX *select_lex= join->select_lex;
  THD * const thd= unit->thd;
  Query_arena *arena= thd->stmt_arena;
 
  if (!select_lex->master_unit()->is_union() &&
      !select_lex->table_list.elements &&
      select_lex->item_list.elements == 1 &&
      !select_lex->item_list.head()->with_sum_func &&
      /*
	We cant change name of Item_field or Item_ref, because it will
	prevent it's correct resolving, but we should save name of
	removed item => we do not make optimization if top item of
	list is field or reference.
	TODO: solve above problem
      */
      !(select_lex->item_list.head()->type() == FIELD_ITEM ||
	select_lex->item_list.head()->type() == REF_ITEM) &&
      !join->conds && !join->having &&
      /*
        switch off this optimization for prepare statement,
        because we do not rollback this changes
        TODO: make rollback for it, or special name resolving mode in 5.0.
      */
      !arena->is_stmt_prepare_or_first_sp_execute()
      )
  {

    have_to_be_excluded= 1;
    if (thd->lex->describe)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
      push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
		   ER_SELECT_REDUCED, warn_buff);
    }
    substitution= select_lex->item_list.head();
    /*
      as far as we moved content to upper level, field which depend of
      'upper' select is not really dependent => we remove this dependence
    */
    substitution->walk(&Item::remove_dependence_processor, 0,
		       (uchar *) select_lex->outer_select());
    DBUG_RETURN(RES_REDUCE);
  }
  DBUG_RETURN(RES_OK);
}


void Item_singlerow_subselect::store(uint i, Item *item)
{
  row[i]->store(item);
  row[i]->cache_value();
}

enum Item_result Item_singlerow_subselect::result_type() const
{
  return engine->type();
}

/* 
 Don't rely on the result type to calculate field type. 
 Ask the engine instead.
*/
enum_field_types Item_singlerow_subselect::field_type() const
{
  return engine->field_type();
}

void Item_singlerow_subselect::fix_length_and_dec()
{
  if ((max_columns= engine->cols()) == 1)
  {
    engine->fix_length_and_dec(row= &value);
  }
  else
  {
    if (!(row= (Item_cache**) sql_alloc(sizeof(Item_cache*)*max_columns)))
      return;
    engine->fix_length_and_dec(row);
    value= *row;
  }
  unsigned_flag= value->unsigned_flag;
  /*
    If there are not tables in subquery then ability to have NULL value
    depends on SELECT list (if single row subquery have tables then it
    always can be NULL if there are not records fetched).
  */
  if (engine->no_tables())
    maybe_null= engine->may_be_null();
}

void Item_singlerow_subselect::no_rows_in_result()
{
  no_rows= true;
}

uint Item_singlerow_subselect::cols()
{
  return engine->cols();
}

bool Item_singlerow_subselect::check_cols(uint c)
{
  if (c != engine->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}

bool Item_singlerow_subselect::null_inside()
{
  for (uint i= 0; i < max_columns ; i++)
  {
    if (row[i]->null_value)
      return 1;
  }
  return 0;
}

void Item_singlerow_subselect::bring_value()
{
  if (!exec() && assigned())
    null_value= 0;
  else
    reset();
}

double Item_singlerow_subselect::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->val_real();
  }
  else
  {
    reset();
    return 0;
  }
}

longlong Item_singlerow_subselect::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->val_int();
  }
  else
  {
    reset();
    return 0;
  }
}

String *Item_singlerow_subselect::val_str(String *str)
{
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->val_str(str);
  }
  else
  {
    reset();
    return 0;
  }
}


my_decimal *Item_singlerow_subselect::val_decimal(my_decimal *decimal_value)
{
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->val_decimal(decimal_value);
  }
  else
  {
    reset();
    return 0;
  }
}


bool Item_singlerow_subselect::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= false;
    return value->get_date(ltime, fuzzydate);
  }
  else
  {
    reset();
    return true;
  }
}


bool Item_singlerow_subselect::get_time(MYSQL_TIME *ltime)
{
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= false;
    return value->get_time(ltime);
  }
  else
  {
    reset();
    return true;
  }
}


bool Item_singlerow_subselect::val_bool()
{
  if (!no_rows && !exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->val_bool();
  }
  else
  {
    reset();
    return 0;
  }
}


Item_exists_subselect::Item_exists_subselect(st_select_lex *select_lex):
  Item_subselect(), value(FALSE), exec_method(EXEC_UNSPECIFIED),
     sj_convert_priority(0), embedding_join_nest(NULL)
{
  DBUG_ENTER("Item_exists_subselect::Item_exists_subselect");
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  null_value= FALSE; //can't be NULL
  maybe_null= 0; //can't be NULL
  DBUG_VOID_RETURN;
}


void Item_exists_subselect::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("exists"));
  Item_subselect::print(str, query_type);
}


bool Item_in_subselect::test_limit(st_select_lex_unit *unit_arg)
{
  if (unit_arg->fake_select_lex &&
      unit_arg->fake_select_lex->test_limit())
    return(1);

  SELECT_LEX *sl= unit_arg->first_select();
  for (; sl; sl= sl->next_select())
  {
    if (sl->test_limit())
      return(1);
  }
  return(0);
}

Item_in_subselect::Item_in_subselect(Item * left_exp,
				     st_select_lex *select_lex):
  Item_exists_subselect(), left_expr(left_exp), left_expr_cache(NULL),
  left_expr_cache_filled(false), need_expr_cache(TRUE), expr(NULL),
  optimizer(NULL), was_null(FALSE), abort_on_null(FALSE),
  in2exists_info(NULL), pushed_cond_guards(NULL), upper_item(NULL)
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  maybe_null= 1;
  reset();
  //if test_limit will fail then error will be reported to client
  test_limit(select_lex->master_unit());
  DBUG_VOID_RETURN;
}

Item_allany_subselect::Item_allany_subselect(Item * left_exp,
                                             chooser_compare_func_creator fc,
					     st_select_lex *select_lex,
					     bool all_arg)
  :Item_in_subselect(), func_creator(fc), all(all_arg)
{
  DBUG_ENTER("Item_allany_subselect::Item_allany_subselect");
  left_expr= left_exp;
  func= func_creator(all_arg);
  init(select_lex, new select_exists_subselect(this));
  max_columns= 1;
  abort_on_null= 0;
  reset();
  //if test_limit will fail then error will be reported to client
  test_limit(select_lex->master_unit());
  DBUG_VOID_RETURN;
}


void Item_exists_subselect::fix_length_and_dec()
{
   decimals= 0;
   max_length= 1;
   max_columns= engine->cols();
   if (exec_method == EXEC_EXISTS)
   {
     /*
       We need only 1 row to determine existence.
       Note that if the subquery is "SELECT1 UNION SELECT2" then this is not
       working optimally (Bug#14215895).
     */
     unit->global_parameters->select_limit= new Item_int((int32) 1);
   }
}

double Item_exists_subselect::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  return (double) value;
}

longlong Item_exists_subselect::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
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

  @param decimal_value[out]    buffer to hold the resulting string value
  @retval                      Pointer to the converted string.
                               Can't be a NULL pointer, as currently
                               EXISTS cannot return NULL.
*/

String *Item_exists_subselect::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
    reset();
  str->set((ulonglong)value,&my_charset_bin);
  return str;
}


/**
  Return the result of EXISTS as a decimal value

  Converts the true/false result into a decimal value.
  Note that currently this cannot be NULL, so if the query exection fails
  it will return 0.

  @param decimal_value[out]    Buffer to hold the resulting decimal value
  @retval                      Pointer to the converted decimal.
                               Can't be a NULL pointer, as currently
                               EXISTS cannot return NULL.
*/

my_decimal *Item_exists_subselect::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
    reset();
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}


bool Item_exists_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  return value != 0;
}


double Item_in_subselect::val_real()
{
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
    null_value= TRUE;
  return (double) value;
}


longlong Item_in_subselect::val_int()
{
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
    null_value= TRUE;
  return value;
}


String *Item_in_subselect::val_str(String *str)
{
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
  {
    null_value= TRUE;
    return 0;
  }
  str->set((ulonglong)value, &my_charset_bin);
  return str;
}


bool Item_in_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
    null_value= TRUE;
  return value;
}

my_decimal *Item_in_subselect::val_decimal(my_decimal *decimal_value)
{
  /*
    As far as Item_in_subselect called only from Item_in_optimizer this
    method should not be used
  */
  DBUG_ASSERT(0);
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
    null_value= TRUE;
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}


/* 
  Rewrite a single-column IN/ALL/ANY subselect

  SYNOPSIS
    Item_in_subselect::single_value_transformer()
      join  Join object of the subquery (i.e. 'child' join).
      func  Subquery comparison creator

  DESCRIPTION
    Rewrite a single-column subquery using rule-based approach. The subquery
    
       oe $cmp$ (SELECT ie FROM ... WHERE subq_where ... HAVING subq_having)
    
    First, try to convert the subquery to scalar-result subquery in one of
    the forms:
    
       - oe $cmp$ (SELECT MAX(...) )  // handled by Item_singlerow_subselect
       - oe $cmp$ <max>(SELECT ...)   // handled by Item_maxmin_subselect
   
    If that fails, the subquery will be handled with class Item_in_optimizer.
    There are two possibilites:
    - If the subquery execution method is materialization, then the subquery is
      not transformed any further.
    - Otherwise the IN predicates is transformed into EXISTS by injecting
      equi-join predicates and possibly other helper predicates. For details
      see method single_value_in_like_transformer().

  RETURN
    RES_OK     Either subquery was transformed, or appopriate
                       predicates where injected into it.
    RES_REDUCE The subquery was reduced to non-subquery
    RES_ERROR  Error
*/

Item_subselect::trans_res
Item_in_subselect::single_value_transformer(JOIN *join,
					    Comp_creator *func)
{
  SELECT_LEX *select_lex= join->select_lex;
  bool subquery_maybe_null= false;
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  /*
    Check that the right part of the subselect contains no more than one
    column. E.g. in SELECT 1 IN (SELECT * ..) the right part is (SELECT * ...)
  */
  // psergey: duplicated_subselect_card_check
  if (select_lex->item_list.elements > 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
    DBUG_RETURN(RES_ERROR);
  }

  THD * const thd= unit->thd;

  /*
    Check the nullability of the subquery. The subquery should return
    only one column, so we check the nullability of the first item in
    SELECT_LEX::item_list. In case the subquery is a union, check the
    nullability of the first item of each SELECT_LEX belonging to the
    union.
  */
  for (SELECT_LEX* lex= select_lex->master_unit()->first_select();
       lex != NULL && lex->master_unit() == select_lex->master_unit();
       lex= lex->next_select())
    if (lex->item_list.head()->maybe_null)
      subquery_maybe_null= true;

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
  if (!func->eqne_op() &&                                             // 1
      !select_lex->master_unit()->uncacheable &&                      // 2
      (abort_on_null || (upper_item && upper_item->top_level()) ||    // 3
       (!left_expr->maybe_null && !subquery_maybe_null)))
  {
    if (substitution)
    {
      // It is second (third, ...) SELECT of UNION => All is done
      DBUG_RETURN(RES_OK);
    }

    Item *subs;
    if (!select_lex->group_list.elements &&
        !select_lex->having &&
        !select_lex->with_sum_func &&
        !(select_lex->next_select()) &&
        select_lex->table_list.elements &&
        !(substype() == ALL_SUBS && subquery_maybe_null))
    {
      OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1,
                          select_lex->select_number,
                          "> ALL/ANY (SELECT)", "SELECT(MIN)");
      oto1.add("chosen", true);
      Item_sum_hybrid *item;
      nesting_map save_allow_sum_func;
      if (func->l_op())
      {
	/*
	  (ALL && (> || =>)) || (ANY && (< || =<))
	  for ALL condition is inverted
	*/
	item= new Item_sum_max(join->ref_ptrs[0]);
      }
      else
      {
	/*
	  (ALL && (< || =<)) || (ANY && (> || =>))
	  for ALL condition is inverted
	*/
	item= new Item_sum_min(join->ref_ptrs[0]);
      }
      if (upper_item)
        upper_item->set_sum_test(item);
      join->ref_ptrs[0]= item;
      {
	List_iterator<Item> it(select_lex->item_list);
	it++;
	it.replace(item);
      }

      DBUG_EXECUTE("where",
                   print_where(item, "rewrite with MIN/MAX", QT_ORDINARY););
      if (thd->variables.sql_mode & MODE_ONLY_FULL_GROUP_BY)
      {
        /*
          If the argument is a field, we assume that fix_fields() has
          tagged the select_lex with non_agg_field_used.
          We reverse that decision after this rewrite with MIN/MAX.
         */
        if (item->get_arg(0)->type() == Item::FIELD_ITEM)
          DBUG_ASSERT(select_lex->non_agg_field_used());
        select_lex->set_non_agg_field_used(false);
      }

      save_allow_sum_func= thd->lex->allow_sum_func;
      thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;
      /*
	Item_sum_(max|min) can't substitute other item => we can use 0 as
        reference, also Item_sum_(max|min) can't be fixed after creation, so
        we do not check item->fixed
      */
      if (item->fix_fields(thd, 0))
	DBUG_RETURN(RES_ERROR);
      thd->lex->allow_sum_func= save_allow_sum_func; 
      /* we added aggregate function => we have to change statistic */
      count_field_types(select_lex, &join->tmp_table_param, join->all_fields, 
                        0);

      subs= new Item_singlerow_subselect(select_lex);
    }
    else
    {
      OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1,
                          select_lex->select_number,
                          "> ALL/ANY (SELECT)", "MIN (SELECT)");
      oto1.add("chosen", true);
      Item_maxmin_subselect *item;
      subs= item= new Item_maxmin_subselect(thd, this, select_lex, func->l_op(),
                                            substype()==ANY_SUBS);
      if (upper_item)
        upper_item->set_sub_test(item);
    }
    if (upper_item)
      upper_item->set_subselect(this);
    /* fix fields is already called for  left expression */
    substitution= func->create(left_expr, subs);
    DBUG_RETURN(RES_OK);
  }

  if (!substitution)
  {
    /* We're invoked for the 1st (or the only) SELECT in the subquery UNION */
    SELECT_LEX_UNIT *master_unit= select_lex->master_unit();
    substitution= optimizer;

    SELECT_LEX *current= thd->lex->current_select;

    thd->lex->current_select= current->outer_select();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0))
    {
      thd->lex->current_select= current;
      DBUG_RETURN(RES_ERROR);
    }
    thd->lex->current_select= current;

    /* We will refer to upper level cache array => we have to save it for SP */
    optimizer->keep_top_level_cache();

    /*
      As far as  Item_ref_in_optimizer do not substitute itself on fix_fields
      we can use same item for all selects.
    */
    expr= new Item_direct_ref(&select_lex->context,
                              (Item**)optimizer->get_cache(),
			      (char *)"<no matter>",
			      (char *)in_left_expr_name);

    DBUG_ASSERT(in2exists_info == NULL);
    in2exists_info= new In2exists_info;
    in2exists_info->originally_dependent=
      master_unit->uncacheable & UNCACHEABLE_DEPENDENT;
    master_unit->uncacheable|= UNCACHEABLE_DEPENDENT;
  }

  if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards)
  {
    if (!(pushed_cond_guards= (bool*)join->thd->alloc(sizeof(bool))))
      DBUG_RETURN(RES_ERROR);
    pushed_cond_guards[0]= TRUE;
  }

  /* Perform the IN=>EXISTS transformation. */
  const trans_res retval= single_value_in_to_exists_transformer(join, func);
  DBUG_RETURN(retval);
}


/**
  Transofrm an IN predicate into EXISTS via predicate injection.

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
        HAVING trigcond(<is_not_null_test>(ie))

  At JOIN::optimize() we will compare costs of materialization and EXISTS; if
  the former is cheaper we will switch to it.

    @param join  Join object of the subquery (i.e. 'child' join).
    @param func  Subquery comparison creator

    @retval RES_OK     Either subquery was transformed, or appopriate
                       predicates where injected into it.
    @retval RES_REDUCE The subquery was reduced to non-subquery
    @retval RES_ERROR  Error
*/

Item_subselect::trans_res
Item_in_subselect::single_value_in_to_exists_transformer(JOIN * join, Comp_creator *func)
{
  SELECT_LEX *select_lex= join->select_lex;
  THD * const thd= unit->thd;
  DBUG_ENTER("Item_in_subselect::single_value_in_to_exists_transformer");

  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select_lex->select_number,
                      "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
  oto1.add("chosen", true);

  select_lex->uncacheable|= UNCACHEABLE_DEPENDENT;
  in2exists_info->added_to_where= false;

  if (join->having || select_lex->with_sum_func ||
      select_lex->group_list.elements)
  {
    bool tmp;
    Item_bool_func *item= func->create(expr,
                             new Item_ref_null_helper(&select_lex->context,
                                                      this,
                                                      &join->ref_ptrs[0],
                                                      (char *)"<ref>",
                                                      this->full_name()));
    item->set_created_by_in2exists();
    if (!abort_on_null && left_expr->maybe_null)
    {
      /* 
        We can encounter "NULL IN (SELECT ...)". Wrap the added condition
        within a trig_cond.
      */
      item= new Item_func_trig_cond(item, get_cond_guard(0), NULL,
                                    Item_func_trig_cond::
                                    OUTER_FIELD_IS_NOT_NULL);
      item->set_created_by_in2exists();
    }

    /*
      AND and comparison functions can't be changed during fix_fields()
      we can assign select_lex->having here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->having= join->having= and_items(join->having, item);
    if (join->having == item)
      item->item_name.set(in_having_cond);
    select_lex->having->top_level_item();
    select_lex->having_fix_field= 1;
    /*
      we do not check join->having->fixed, because Item_and (from and_items)
      or comparison function (from func->create) can't be fixed after creation
    */
    Opt_trace_array having_trace(&thd->opt_trace,
                                 "evaluating_constant_having_conditions");
    tmp= join->having->fix_fields(thd, 0);
    select_lex->having_fix_field= 0;
    if (tmp)
      DBUG_RETURN(RES_ERROR);
  }
  else
  {
    Item *orig_item= select_lex->item_list.head()->real_item();

    if (select_lex->table_list.elements)
    {
      bool tmp;
      Item_bool_func *item= func->create(expr, orig_item);
      /*
        We may soon add a 'OR inner IS NULL' to 'item', but that may later be
        removed if 'inner' is not nullable, so the in2exists mark must be on
        'item' too. Not only on the OR node.
      */
      item->set_created_by_in2exists();
      if (!abort_on_null && orig_item->maybe_null)
      {
	Item_bool_func *having= new Item_is_not_null_test(this, orig_item);
        having->set_created_by_in2exists();
        if (left_expr->maybe_null)
        {
          if (!(having= new Item_func_trig_cond(having,
                                                get_cond_guard(0), NULL,
                                                Item_func_trig_cond::
                                                OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          having->set_created_by_in2exists();
        }
	/*
	  Item_is_not_null_test can't be changed during fix_fields()
	  we can assign select_lex->having here, and pass 0 as last
	  argument (reference) to fix_fields()
	*/
        having->item_name.set(in_having_cond);
	select_lex->having= join->having= having;
	select_lex->having_fix_field= 1;
        /*
          we do not check join->having->fixed, because Item_and (from
          and_items) or comparison function (from func->create) can't be
          fixed after creation
        */
        Opt_trace_array having_trace(&thd->opt_trace,
                                     "evaluating_constant_having_conditions");
	tmp= join->having->fix_fields(thd, 0);
        select_lex->having_fix_field= 0;
        if (tmp)
	  DBUG_RETURN(RES_ERROR);
	item= new Item_cond_or(item,
			       new Item_func_isnull(orig_item));
        item->set_created_by_in2exists();
      }
      /* 
        If we may encounter NULL IN (SELECT ...) and care whether subquery
        result is NULL or FALSE, wrap condition in a trig_cond.
      */
      if (!abort_on_null && left_expr->maybe_null)
      {
        if (!(item= new Item_func_trig_cond(item, get_cond_guard(0), NULL,
                                            Item_func_trig_cond::
                                            OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        item->set_created_by_in2exists();
      }
      /*
        The following is intentionally not done in row_value_transformer(),
        see comment of JOIN::remove_subq_pushed_predicates().
      */
      item->item_name.set(in_additional_cond);

      /*
	AND can't be changed during fix_fields()
	we can assign select_lex->having here, and pass 0 as last
	argument (reference) to fix_fields()
      */
      select_lex->where= join->conds= and_items(join->conds, item);
      select_lex->where->top_level_item();
      in2exists_info->added_to_where= true;
      /*
        we do not check join->conds->fixed, because Item_and can't be fixed
        after creation
      */
      Opt_trace_array where_trace(&thd->opt_trace,
                                  "evaluating_constant_where_conditions");
      if (join->conds->fix_fields(thd, 0))
	DBUG_RETURN(RES_ERROR);
    }
    else
    {
      bool tmp;
      if (select_lex->master_unit()->is_union())
      {
	/*
	  comparison functions can't be changed during fix_fields()
	  we can assign select_lex->having here, and pass 0 as last
	  argument (reference) to fix_fields()
	*/
        Item_bool_func *new_having=
          func->create(expr,
                       new Item_ref_null_helper(&select_lex->context, this,
                                            &join->ref_ptrs[0],
                                            (char *)"<no matter>",
                                            (char *)"<result>"));
        new_having->set_created_by_in2exists();
        if (!abort_on_null && left_expr->maybe_null)
        {
          if (!(new_having= new Item_func_trig_cond(new_having,
                                                    get_cond_guard(0),
                                                    NULL,
                                                    Item_func_trig_cond::
                                                    OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          new_having->set_created_by_in2exists();
        }
        new_having->item_name.set(in_having_cond);
	select_lex->having= join->having= new_having;
	select_lex->having_fix_field= 1;
        
        /*
          we do not check join->having->fixed, because comparison function
          (from func->create) can't be fixed after creation
        */
        Opt_trace_array having_trace(&thd->opt_trace,
                                     "evaluating_constant_having_conditions");
	tmp= join->having->fix_fields(thd, 0);
        select_lex->having_fix_field= 0;
        if (tmp)
	  DBUG_RETURN(RES_ERROR);
      }
      else
      {
	// it is single select without tables => possible optimization
        // remove the dependence mark since the item is moved to upper
        // select and is not outer anymore.
        orig_item->walk(&Item::remove_dependence_processor, 0,
                        (uchar *) select_lex->outer_select());
	Item_bool_func *item= func->create(left_expr, orig_item);
	// fix_field of item will be done in time of substituting
	substitution= item;
	have_to_be_excluded= 1;
	if (thd->lex->describe)
	{
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
	  push_warning(thd, Sql_condition::WARN_LEVEL_NOTE,
		       ER_SELECT_REDUCED, warn_buff);
	}
	DBUG_RETURN(RES_REDUCE);
      }
    }
  }
  join->having_for_explain= join->having;

  DBUG_RETURN(RES_OK);
}


Item_subselect::trans_res
Item_in_subselect::row_value_transformer(JOIN *join)
{
  SELECT_LEX *select_lex= join->select_lex;
  uint cols_num= left_expr->cols();

  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  // psergey: duplicated_subselect_card_check
  if (select_lex->item_list.elements != left_expr->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), left_expr->cols());
    DBUG_RETURN(RES_ERROR);
  }

  /*
    Wrap the current IN predicate in an Item_in_optimizer. The actual
    substitution in the Item tree takes place in Item_subselect::fix_fields.
  */
  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *master_unit= select_lex->master_unit();
    substitution= optimizer;

    THD * const thd= unit->thd;
    SELECT_LEX *current= thd->lex->current_select;
    thd->lex->current_select= current->outer_select();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0))
    {
      thd->lex->current_select= current;
      DBUG_RETURN(RES_ERROR);
    }

    // we will refer to upper level cache array => we have to save it in PS
    optimizer->keep_top_level_cache();

    thd->lex->current_select= current;
    DBUG_ASSERT(in2exists_info == NULL);
    in2exists_info= new In2exists_info;
    in2exists_info->originally_dependent=
      master_unit->uncacheable & UNCACHEABLE_DEPENDENT;
    master_unit->uncacheable|= UNCACHEABLE_DEPENDENT;

    if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards)
    {
      if (!(pushed_cond_guards= (bool*)join->thd->alloc(sizeof(bool) *
                                                        left_expr->cols())))
        DBUG_RETURN(RES_ERROR);
      for (uint i= 0; i < cols_num; i++)
        pushed_cond_guards[i]= TRUE;
    }
  }

  /* Perform the IN=>EXISTS transformation. */
  DBUG_RETURN(row_value_in_to_exists_transformer(join));
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

Item_subselect::trans_res
Item_in_subselect::row_value_in_to_exists_transformer(JOIN * join)
{
  SELECT_LEX *select_lex= join->select_lex;
  THD * const thd= unit->thd;
  Item *having_item= 0;
  uint cols_num= left_expr->cols();
  bool is_having_used= (join->having || select_lex->with_sum_func ||
                        select_lex->group_list.first ||
                        !select_lex->table_list.elements);

  DBUG_ENTER("Item_in_subselect::row_value_in_to_exists_transformer");
  OPT_TRACE_TRANSFORM(&thd->opt_trace, oto0, oto1, select_lex->select_number,
                      "IN (SELECT)", "EXISTS (CORRELATED SELECT)");
  oto1.add("chosen", true);

  select_lex->uncacheable|= UNCACHEABLE_DEPENDENT;
  in2exists_info->added_to_where= false;

  if (is_having_used)
  {
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
    Item *item_having_part2= 0;
    for (uint i= 0; i < cols_num; i++)
    {
      Item *item_i= join->ref_ptrs[i];
      Item **pitem_i= &join->ref_ptrs[i];
      DBUG_ASSERT((left_expr->fixed && item_i->fixed) ||
                  (item_i->type() == REF_ITEM &&
                   ((Item_ref*)(item_i))->ref_type() == Item_ref::OUTER_REF));
      if (item_i-> check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
      Item_bool_func *item_eq=
        new Item_func_eq(new
                         Item_ref(&select_lex->context,
                                  (*optimizer->get_cache())->
                                  addr(i),
                                  (char *)"<no matter>",
                                  (char *)in_left_expr_name),
                         new
                         Item_ref(&select_lex->context,
                                  pitem_i,
                                  (char *)"<no matter>",
                                  (char *)"<list ref>")
                        );
      item_eq->set_created_by_in2exists();
      Item_bool_func *item_isnull=
        new Item_func_isnull(new
                             Item_ref(&select_lex->context,
                                      pitem_i,
                                      (char *)"<no matter>",
                                      (char *)"<list ref>")
                            );
      item_isnull->set_created_by_in2exists();
      Item_bool_func *col_item= new Item_cond_or(item_eq, item_isnull);
      col_item->set_created_by_in2exists();
      if (!abort_on_null && left_expr->element_index(i)->maybe_null)
      {
        if (!(col_item= new Item_func_trig_cond(col_item, get_cond_guard(i),
                                                NULL, Item_func_trig_cond::
                                                OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        col_item->set_created_by_in2exists();
      }
      having_item= and_items(having_item, col_item);
      
      Item_bool_func *item_nnull_test= 
         new Item_is_not_null_test(this,
                                   new Item_ref(&select_lex->context,
                                                pitem_i,
                                                (char *)"<no matter>",
                                                (char *)"<list ref>"));
      item_nnull_test->set_created_by_in2exists();
      if (!abort_on_null && left_expr->element_index(i)->maybe_null)
      {
        if (!(item_nnull_test= 
              new Item_func_trig_cond(item_nnull_test, get_cond_guard(i),
                                      NULL, Item_func_trig_cond::
                                      OUTER_FIELD_IS_NOT_NULL)))
          DBUG_RETURN(RES_ERROR);
        item_nnull_test->set_created_by_in2exists();
      }
      item_having_part2= and_items(item_having_part2, item_nnull_test);
      item_having_part2->top_level_item();
    }
    having_item= and_items(having_item, item_having_part2);
    having_item->top_level_item();
  }
  else
  {
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
    Item *where_item= 0;
    for (uint i= 0; i < cols_num; i++)
    {
      Item *item_i= join->ref_ptrs[i];
      Item **pitem_i= &join->ref_ptrs[i];
      DBUG_ASSERT((left_expr->fixed && item_i->fixed) ||
                  (item_i->type() == REF_ITEM &&
                   ((Item_ref*)(item_i))->ref_type() == Item_ref::OUTER_REF));
      if (item_i->check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
      Item_bool_func *item=
        new Item_func_eq(new
                         Item_direct_ref(&select_lex->context,
                                         (*optimizer->get_cache())->
                                         addr(i),
                                         (char *)"<no matter>",
                                         (char *)in_left_expr_name),
                         new
                         Item_direct_ref(&select_lex->context,
                                         pitem_i,
                                         (char *)"<no matter>",
                                         (char *)"<list ref>")
                        );
      item->set_created_by_in2exists();
      if (!abort_on_null)
      {
        Item_bool_func *having_col_item=
          new Item_is_not_null_test(this,
                                    new
                                    Item_ref(&select_lex->context, 
                                             pitem_i,
                                             (char *)"<no matter>",
                                             (char *)"<list ref>"));
        
        having_col_item->set_created_by_in2exists();
        Item_bool_func *item_isnull= new
          Item_func_isnull(new
                           Item_direct_ref(&select_lex->context,
                                           pitem_i,
                                           (char *)"<no matter>",
                                           (char *)"<list ref>")
                          );
        item_isnull->set_created_by_in2exists();
        item= new Item_cond_or(item, item_isnull);
        item->set_created_by_in2exists();
        /* 
          TODO: why we create the above for cases where the right part
                cant be NULL?
        */
        if (left_expr->element_index(i)->maybe_null)
        {
          if (!(item= new Item_func_trig_cond(item, get_cond_guard(i), NULL,
                                              Item_func_trig_cond::
                                              OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          item->set_created_by_in2exists();
          if (!(having_col_item=
                new Item_func_trig_cond(having_col_item, get_cond_guard(i),
                                        NULL,
                                        Item_func_trig_cond::
                                        OUTER_FIELD_IS_NOT_NULL)))
            DBUG_RETURN(RES_ERROR);
          having_col_item->set_created_by_in2exists();
        }
        having_item= and_items(having_item, having_col_item);
      }

      where_item= and_items(where_item, item);
    }
    /*
      AND can't be changed during fix_fields()
      we can assign select_lex->where here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->where= join->conds= and_items(join->conds, where_item);
    select_lex->where->top_level_item();
    in2exists_info->added_to_where= true;
    Opt_trace_array where_trace(&thd->opt_trace,
                                "evaluating_constant_where_conditions");
    if (join->conds->fix_fields(thd, 0))
      DBUG_RETURN(RES_ERROR);
  }
  if (having_item)
  {
    bool res;
    select_lex->having= join->having= join->having_for_explain=
      and_items(join->having, having_item);
    if (having_item == select_lex->having)
      having_item->item_name.set(in_having_cond);
    select_lex->having->top_level_item();
    /*
      AND can't be changed during fix_fields()
      we can assign select_lex->having here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->having_fix_field= 1;
    Opt_trace_array having_trace(&thd->opt_trace,
                                 "evaluating_constant_having_conditions");
    res= join->having->fix_fields(thd, 0);
    select_lex->having_fix_field= 0;
    if (res)
    {
      DBUG_RETURN(RES_ERROR);
    }
  }

  DBUG_RETURN(RES_OK);
}


Item_subselect::trans_res
Item_in_subselect::select_transformer(JOIN *join)
{
  return select_in_like_transformer(join, &eq_creator);
}


/**
  Prepare IN/ALL/ANY/SOME subquery transformation and call appropriate
  transformation function.

    To decide which transformation procedure (scalar or row) applicable here
    we have to call fix_fields() for left expression to be able to call
    cols() method on it. Also this method make arena management for
    underlying transformation methods.

  @param join    JOIN object of transforming subquery
  @param func    creator of condition function of subquery

  @retval
    RES_OK      OK
  @retval
    RES_REDUCE  OK, and current subquery was reduced during
    transformation
  @retval
    RES_ERROR   Error
*/

Item_subselect::trans_res
Item_in_subselect::select_in_like_transformer(JOIN *join, Comp_creator *func)
{
  THD * const thd= unit->thd;
  SELECT_LEX *current= thd->lex->current_select;
  const char *save_where= thd->where;
  Item_subselect::trans_res res= RES_ERROR;
  bool result;

  DBUG_ENTER("Item_in_subselect::select_in_like_transformer");

#ifndef DBUG_OFF
  {
    /*
      IN/SOME/ALL/ANY subqueries don't support LIMIT clause. Without
      it, ORDER BY becomes meaningless and should already have been
      removed in resolve_subquery()
    */
    SELECT_LEX *sl= current->master_unit()->first_select();
    for (; sl; sl= sl->next_select())
      if (sl->join)
        DBUG_ASSERT(!sl->join->order || sl->join->order.src == ESC_GROUP_BY);
  }
#endif

  if (changed)
    DBUG_RETURN(RES_OK);

  thd->where= "IN/ALL/ANY subquery";

  /*
    In some optimisation cases we will not need this Item_in_optimizer
    object, but we can't know it here, but here we need address correct
    reference on left expresion.

    //psergey: he means confluent cases like "... IN (SELECT 1)"
  */
  if (!optimizer)
  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);
    optimizer= new Item_in_optimizer(left_expr, this);

    if (!optimizer)
      goto err;
  }

  thd->lex->current_select= current->outer_select();
  result= (!left_expr->fixed &&
           left_expr->fix_fields(thd, optimizer->arguments()));
  /* fix_fields can change reference to left_expr, we need reassign it */
  left_expr= optimizer->arguments()[0];

  thd->lex->current_select= current;
  if (result)
    goto err;

  /*
    If we didn't choose an execution method up to this point, we choose
    the IN=>EXISTS transformation, at least temporarily.
  */
  if (exec_method == EXEC_UNSPECIFIED)
    exec_method= EXEC_EXISTS_OR_MAT;

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
      res= single_value_transformer(join, func);
    else
    {
      /* we do not support row operation for ALL/ANY/SOME */
      if (func != &eq_creator)
      {
        my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
        DBUG_RETURN(RES_ERROR);
      }
      res= row_value_transformer(join);
    }
  }

err:
  thd->where= save_where;
  DBUG_RETURN(res);
}


void Item_in_subselect::print(String *str, enum_query_type query_type)
{
  if (exec_method == EXEC_EXISTS_OR_MAT || exec_method == EXEC_EXISTS)
    str->append(STRING_WITH_LEN("<exists>"));
  else
  {
    left_expr->print(str, query_type);
    str->append(STRING_WITH_LEN(" in "));
  }
  Item_subselect::print(str, query_type);
}


bool Item_in_subselect::fix_fields(THD *thd_arg, Item **ref)
{
  bool result = 0;

  if (exec_method == EXEC_SEMI_JOIN)
    return !( (*ref)= new Item_int(1));

  if ((thd_arg->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      left_expr && !left_expr->fixed)
    result = left_expr->fix_fields(thd_arg, &left_expr);

  return result || Item_subselect::fix_fields(thd_arg, ref);
}


void Item_in_subselect::fix_after_pullout(st_select_lex *parent_select,
                                          st_select_lex *removed_select)
{
  Item_subselect::fix_after_pullout(parent_select, removed_select);

  left_expr->fix_after_pullout(parent_select, removed_select);

  used_tables_cache|= left_expr->used_tables();
}


/**
  Initialize the cache of the left operand of the IN predicate.

  @note This method has the same purpose as alloc_group_fields(),
  but it takes a different kind of collection of items, and the
  list we push to is dynamically allocated.

  @retval TRUE  if a memory allocation error occurred
  @retval FALSE if success
*/

bool Item_in_subselect::init_left_expr_cache()
{
  JOIN *outer_join;
  Next_select_func end_select;
  bool use_result_field= FALSE;

  outer_join= unit->outer_select()->join;
  /*
    An IN predicate might be evaluated in a query for which all tables have
    been optimzied away.
  */ 
  if (!outer_join || !outer_join->tables || !outer_join->tables_list)
  {
    need_expr_cache= FALSE;
    return FALSE;
  }

  /*
    If we use end_[send | write]_group to handle complete rows of the outer
    query, make the cache of the left IN operand use Item_field::result_field
    instead of Item_field::field.  We need this because normally
    Cached_item_field uses Item::field to fetch field data, while
    copy_ref_key() that copies the left IN operand into a lookup key uses
    Item::result_field. In the case end_[send | write]_group result_field is
    one row behind field.
  */
  end_select= outer_join->join_tab[outer_join->primary_tables-1].next_select;
  if (end_select == end_send_group || end_select == end_write_group)
    use_result_field= TRUE;

  if (!(left_expr_cache= new List<Cached_item>))
    return TRUE;

  for (uint i= 0; i < left_expr->cols(); i++)
  {
    Cached_item *cur_item_cache= new_Cached_item(unit->thd,
                                                 left_expr->element_index(i),
                                                 use_result_field);
    if (!cur_item_cache || left_expr_cache->push_front(cur_item_cache))
      return TRUE;
  }
  return FALSE;
}


/**
   Tells an Item that it is in the condition of a JOIN_TAB

  @param 'join_tab_index'  index of JOIN_TAB in JOIN's array

   The Item records this fact and can deduce from it the estimated number of
   times that it will be evaluated.
*/
bool Item_subselect::inform_item_in_cond_of_tab(uchar *join_tab_index)
{
  in_cond_of_tab= *reinterpret_cast<int *>(join_tab_index);
  return false;
}


Item_subselect::trans_res
Item_allany_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_allany_subselect::select_transformer");
  if (upper_item)
    upper_item->show= 1;
  trans_res retval= select_in_like_transformer(join, func);
  DBUG_RETURN(retval);
}


void Item_allany_subselect::print(String *str, enum_query_type query_type)
{
  if (exec_method == EXEC_EXISTS_OR_MAT || exec_method == EXEC_EXISTS)
    str->append(STRING_WITH_LEN("<exists>"));
  else
  {
    left_expr->print(str, query_type);
    str->append(' ');
    str->append(func->symbol(all));
    str->append(all ? " all " : " any ", 5);
  }
  Item_subselect::print(str, query_type);
}


void subselect_engine::set_thd_for_result()
{
  /*
    select_result's constructor sets neither select_result::thd nor
    select_result::unit.
  */
  if (result)
    result->set_thd(item->unit->thd);
}


subselect_single_select_engine::
subselect_single_select_engine(st_select_lex *select,
			       select_result_interceptor *result_arg,
			       Item_subselect *item_arg)
  :subselect_engine(item_arg, result_arg),
   prepared(0), executed(0), optimize_error(0), select_lex(select), join(0)
{
  select_lex->master_unit()->item= item_arg;
}


void subselect_single_select_engine::cleanup()
{
  DBUG_ENTER("subselect_single_select_engine::cleanup");
  prepared= executed= optimize_error= false;
  join= 0;
  result->cleanup();
  DBUG_VOID_RETURN;
}


void subselect_union_engine::cleanup()
{
  DBUG_ENTER("subselect_union_engine::cleanup");
  result->cleanup();
  DBUG_VOID_RETURN;
}


bool subselect_union_engine::is_executed() const
{
  return unit->executed;
}


subselect_union_engine::subselect_union_engine(st_select_lex_unit *u,
					       select_result_interceptor *result_arg,
					       Item_subselect *item_arg)
  :subselect_engine(item_arg, result_arg)
{
  unit= u;
  unit->item= item_arg;
}


/**
  Create and prepare the JOIN object that represents the query execution
  plan for the subquery.

  @detail
  This method is called from Item_subselect::fix_fields. For prepared
  statements it is called both during the PREPARE and EXECUTE phases in the
  following ways:
  - During PREPARE the optimizer needs some properties
    (join->fields_list.elements) of the JOIN to proceed with preparation of
    the remaining query (namely to complete ::fix_fields for the subselect
    related classes. In the end of PREPARE the JOIN is deleted.
  - When we EXECUTE the query, Item_subselect::fix_fields is called again, and
    the JOIN object is re-created again, prepared and executed. In the end of
    execution it is deleted.
  In all cases the JOIN is created in runtime memory (not in the permanent
  memory root).

  @todo
  Re-check what properties of 'join' are needed during prepare, and see if
  we can avoid creating a JOIN during JOIN::prepare of the outer join.

  @retval 0  if success
  @retval 1  if error
*/

bool subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
  THD * const thd= item->unit->thd;
  join= new JOIN(thd, select_lex->item_list,
		 select_lex->options | SELECT_NO_UNLOCK, result);
  if (!join || !result)
    return 1; /* Fatal error is set already. */
  prepared= 1;
  SELECT_LEX *save_select= thd->lex->current_select;
  thd->lex->current_select= select_lex;
  if (join->prepare(select_lex->table_list.first,
		    select_lex->with_wild,
		    select_lex->where,
		    select_lex->order_list.elements +
		    select_lex->group_list.elements,
		    select_lex->order_list.first,
		    select_lex->group_list.first,
		    select_lex->having,
		    select_lex,
		    select_lex->master_unit()))
    return 1;
  thd->lex->current_select= save_select;
  return 0;
}


bool subselect_union_engine::prepare()
{
  THD * const thd= unit->thd;
  // We can access THD as above, or via 'item', verify equality:
  DBUG_ASSERT(thd == item->unit->thd);
  return unit->prepare(thd, result, SELECT_NO_UNLOCK);
}


bool subselect_indexsubquery_engine::prepare()
{
  /* Should never be called. */
  DBUG_ASSERT(FALSE);
  return 1;
}


/* 
 makes storage for the output values for the subquery and calcuates 
 their data and column types and their nullability.
*/ 
void subselect_engine::set_row(List<Item> &item_list, Item_cache **row)
{
  Item *sel_item;
  List_iterator_fast<Item> li(item_list);
  res_type= STRING_RESULT;
  res_field_type= MYSQL_TYPE_VAR_STRING;
  for (uint i= 0; (sel_item= li++); i++)
  {
    item->max_length= sel_item->max_length;
    res_type= sel_item->result_type();
    res_field_type= sel_item->field_type();
    item->decimals= sel_item->decimals;
    item->unsigned_flag= sel_item->unsigned_flag;
    maybe_null= sel_item->maybe_null;
    if (!(row[i]= Item_cache::get_cache(sel_item)))
      return;
    row[i]->setup(sel_item);
    row[i]->store(sel_item);
  }
  if (item_list.elements > 1)
    res_type= ROW_RESULT;
}

void subselect_single_select_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || select_lex->item_list.elements==1);
  set_row(select_lex->item_list, row);
  item->collation.set(row[0]->collation);
  if (cols() != 1)
    maybe_null= 0;
}

void subselect_union_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || unit->first_select()->item_list.elements==1);

  if (unit->first_select()->item_list.elements == 1)
  {
    set_row(unit->types, row);
    item->collation.set(row[0]->collation);
  }
  else
  {
    bool maybe_null_saved= maybe_null;
    set_row(unit->types, row);
    maybe_null= maybe_null_saved;
  }
}

void subselect_indexsubquery_engine::fix_length_and_dec(Item_cache **row)
{
  //this never should be called
  DBUG_ASSERT(0);
}

int read_first_record_seq(JOIN_TAB *tab);
int rr_sequential(READ_RECORD *info);

bool subselect_single_select_engine::exec()
{
  DBUG_ENTER("subselect_single_select_engine::exec");

  if (optimize_error)
    DBUG_RETURN(true);

  int rc= 0;
  THD * const thd= item->unit->thd;
  char const *save_where= thd->where;
  SELECT_LEX *save_select= thd->lex->current_select;
  thd->lex->current_select= select_lex;
  if (!join->optimized)
  {
    SELECT_LEX_UNIT *unit= select_lex->master_unit();

    unit->set_limit(unit->global_parameters);

    DBUG_EXECUTE_IF("bug11747970_simulate_error",
                    DBUG_SET("+d,bug11747970_raise_error"););

    if (join->optimize())
    {
      optimize_error= true;
      rc= join->error ? join->error : 1;
      goto exit;
    }
    if (item->engine_changed)
    {
      rc= 1;
      goto exit;
    }
  }
  if (select_lex->uncacheable &&
      select_lex->uncacheable != UNCACHEABLE_EXPLAIN
      && executed)
  {
    join->reset();
    item->reset();
    item->assigned((executed= 0));
  }
  if (!executed)
  {
    item->reset_value_registration();
    JOIN_TAB *changed_tabs[MAX_TABLES];
    JOIN_TAB **last_changed_tab= changed_tabs;
    if (item->have_guarded_conds())
    {
      /*
        For at least one of the pushed predicates the following is true:
        We should not apply optimizations based on the condition that was
        pushed down into the subquery. Those optimizations are ref[_or_null]
        acceses. Change them to be full table scans.
      */
      for (uint i= join->const_tables; i < join->primary_tables; i++)
      {
        JOIN_TAB *tab=join->join_tab+i;
        if (tab && tab->keyuse)
        {
          for (uint i= 0; i < tab->ref.key_parts; i++)
          {
            bool *cond_guard= tab->ref.cond_guards[i];
            if (cond_guard && !*cond_guard)
            {
              /*
                Can't use BKA if switching from ref to "full scan on
                NULL key" 

                @see Item_in_optimizer::val_int()
                @see TABLE_REF::cond_guards
                @see push_index_cond()
                @see setup_join_buffering()
              */
              DBUG_ASSERT(tab->use_join_cache != JOIN_CACHE::ALG_BKA);
              DBUG_ASSERT(tab->use_join_cache != JOIN_CACHE::ALG_BKA_UNIQUE);

              /* Change the access method to full table scan */
              tab->save_read_first_record= tab->read_first_record;
              tab->save_read_record= tab->read_record.read_record;
              tab->read_record.read_record= rr_sequential;
              tab->read_first_record= read_first_record_seq;
              tab->read_record.record= tab->table->record[0];
              tab->read_record.thd= join->thd;
              tab->read_record.ref_length= tab->table->file->ref_length;
              tab->read_record.unlock_row= rr_unlock_row;
              *(last_changed_tab++)= tab;
              break;
            }
          }
        }
      }
    }
    
    join->exec();

    /* Enable the optimizations back */
    for (JOIN_TAB **ptab= changed_tabs; ptab != last_changed_tab; ptab++)
    {
      JOIN_TAB *tab= *ptab;
      tab->read_record.record= 0;
      tab->read_record.ref_length= 0;
      tab->read_first_record= tab->save_read_first_record; 
      tab->read_record.read_record= tab->save_read_record;
      tab->save_read_first_record= NULL;
    }
    executed= true;
    
    rc= join->error || thd->is_fatal_error;
  }

exit:
  thd->where= save_where;
  thd->lex->current_select= save_select;
  DBUG_RETURN(rc);
}

bool subselect_union_engine::exec()
{
  THD * const thd= unit->thd;
  DBUG_ASSERT(thd == item->unit->thd);
  char const *save_where= thd->where;
  const bool res= (unit->optimize() || unit->exec());
  thd->where= save_where;
  return res;
}


/**
  Search, using a table scan, for at least one row satisfying select
  condition.

  The caller must set item's 'value' to 'false' before calling this
  function. This function will set it to 'true' if it finds a matching row.

  @returns false if ok, true if read error.
*/
bool subselect_indexsubquery_engine::scan_table()
{
  int error;
  TABLE *table= tab->table;
  DBUG_ENTER("subselect_indexsubquery_engine::scan_table");

  // We never need to do a table scan of the materialized table.
  DBUG_ASSERT(engine_type() != HASH_SJ_ENGINE);

  if ((table->file->inited &&
       (error= table->file->ha_index_end())) ||
      (error= table->file->ha_rnd_init(1)))
  {
    (void) report_handler_error(table, error);
    DBUG_RETURN(true);
  }

  table->file->extra_opt(HA_EXTRA_CACHE,
                         current_thd->variables.read_buff_size);
  table->null_row= 0;
  for (;;)
  {
    error=table->file->ha_rnd_next(table->record[0]);
    if (error && error != HA_ERR_END_OF_FILE)
    {
      error= report_handler_error(table, error);
      break;
    }
    /* No more rows */
    if (table->status)
      break;

    if (!cond || cond->val_int())
    {
      static_cast<Item_in_subselect*>(item)->value= true;
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
                                                  bool *convert_error)
{
  DBUG_ENTER("subselect_indexsubquery_engine::copy_ref_key");

  *require_scan= false;
  *convert_error= false;
  for (uint part_no= 0; part_no < tab->ref.key_parts; part_no++)
  {
    store_key *s_key= tab->ref.key_copy[part_no];
    if (s_key == NULL)
      continue; // key is const and does not need to be reevaluated

    const enum store_key::store_key_result store_res= s_key->copy();
    tab->ref.key_err= store_res;

    if (s_key->null_key)
    {
      /*
        If we have materialized the subquery:
        - this NULL ref item cannot be local to the subquery (any such
        conditions was handled during materialization)
        - neither can it be outer, because this case is
        separately managed in subselect_hash_sj_engine::exec().
      */
      DBUG_ASSERT(engine_type() != HASH_SJ_ENGINE);

      const bool *cond_guard= tab->ref.cond_guards[part_no];

      /*
        NULL value is from the outer_value_list if the key part has a
        cond guard that deactivates the condition. @see
        TABLE_REF::cond_guards

      */
      if (cond_guard && !*cond_guard)
      {
        DBUG_ASSERT(!(static_cast <Item_in_subselect*>(item)
                      ->is_top_level_item()));

        *require_scan= true;
        DBUG_VOID_RETURN;
      }
    }

    /*
      Check if the error is equal to STORE_KEY_FATAL. This is not expressed 
      using the store_key::store_key_result enum because ref.key_err is a 
      boolean and we want to detect both TRUE and STORE_KEY_FATAL from the 
      space of the union of the values of [TRUE, FALSE] and 
      store_key::store_key_result.  
      TODO: fix the variable an return types.
    */
    if (store_res == store_key::STORE_KEY_FATAL)
    {
      /*
       Error converting the left IN operand to the column type of the right
       IN operand. 
      */
      tab->table->status= STATUS_NOT_FOUND;
      *convert_error= true;
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
       row is found, return NULL, otherwise return FALSE.
    2. Make an index lookup via key=oe, search for a row that satisfies
       subq_where. If found, return TRUE.
    3. If check_null==TRUE, make another lookup via key=NULL, search for a 
       row that satisfies subq_where. If found, return NULL, otherwise
       return FALSE.
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
    i.e. NULL).  If the query produces no rows, the result is FALSE.

    We currently evaluate (1) by doing a full table scan. (2) can be
    evaluated by doing a "ref" scan on "keypart1=const1", which can be much
    cheaper. We can use index statistics to quickly check whether "ref" scan
    will be cheaper than full table scan.

  RETURN
    0
    1
*/

bool subselect_indexsubquery_engine::exec()
{
  DBUG_ENTER("subselect_indexsubquery_engine::exec");
  int error;
  bool null_finding= 0;
  TABLE *table= tab->table;
  // 'tl' is NULL if this is a tmp table created by subselect_hash_sj_engine.
  TABLE_LIST *tl= table->pos_in_table_list;
  Item_in_subselect *const item_in= static_cast<Item_in_subselect*>(item);
  item_in->value= false;
  table->status= 0;

  if (tl && tl->uses_materialization() && !tab->materialized)
  {
    bool err= mysql_handle_single_derived(table->in_use->lex, tl,
                                          mysql_derived_create) ||
              mysql_handle_single_derived(table->in_use->lex, tl,
                                          mysql_derived_materialize);
    if (!tab->table->in_use->lex->describe)
      mysql_handle_single_derived(table->in_use->lex, tl,
                                  mysql_derived_cleanup);
    if (err)
      DBUG_RETURN(1);

    tab->materialized= true;
  }

  if (check_null)
  {
    /* We need to check for NULL if there wasn't a matching value */
    *tab->ref.null_ref_key= 0;			// Search first for not null
    item_in->was_null= false;
  }

  /* Copy the ref key and check for nulls... */
  bool require_scan, convert_error;
  copy_ref_key(&require_scan, &convert_error);
  if (convert_error)
    DBUG_RETURN(0);

  if (require_scan)
  {
    const bool scan_result= scan_table();
    DBUG_RETURN(scan_result);
  }

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref.key, !unique /* sorted */)))
  {
    (void) report_handler_error(table, error);
    DBUG_RETURN(true);
  }
  error= table->file->ha_index_read_map(table->record[0],
                                        tab->ref.key_buff,
                                        make_prev_keypart_map(tab->ref.key_parts),
                                        HA_READ_KEY_EXACT);
  if (error &&
      error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    error= report_handler_error(table, error);
  else
  {
    for (;;)
    {
      error= 0;
      table->null_row= 0;
      if (!table->status)
      {
        if ((!cond || cond->val_int()) && (!having || having->val_int()))
        {
          item_in->value= true;
          if (null_finding)
          {
            /*
              This is dead code; subqueries with check_null==true are always
              transformed with IN-to-EXISTS and thus their artificial HAVING
              rejects NULL values...
            */
            DBUG_ASSERT(false);
            item_in->was_null= true;
          }
          break;
        }
        if (unique)
          break;
        error= table->file->ha_index_next_same(table->record[0],
                                              tab->ref.key_buff,
                                              tab->ref.key_length);
        if (error && error != HA_ERR_END_OF_FILE)
        {
          error= report_handler_error(table, error);
          break;
        }
      }
      else
      {
        if (!check_null || null_finding)
          break;			/* We don't need to check nulls */
        /*
          Check if there exists a row with a null value in the index. We come
          here only if ref_or_null, and ref_or_null is always on a single
          column (first keypart of the index). So we have only one NULL bit to
          turn on:
        */
        *tab->ref.null_ref_key= 1;
        null_finding= 1;
        if ((error= (safe_index_read(tab) == 1)))
          break;
      }
    }
  }
  DBUG_RETURN(error != 0);
}


uint subselect_single_select_engine::cols() const
{
  return select_lex->item_list.elements;
}


uint subselect_union_engine::cols() const
{
  DBUG_ASSERT(unit->is_prepared());  // should be called after fix_fields()
  return unit->types.elements;
}


uint8 subselect_single_select_engine::uncacheable() const
{
  return select_lex->uncacheable;
}


uint8 subselect_union_engine::uncacheable() const
{
  return unit->uncacheable;
}


void subselect_single_select_engine::exclude()
{
  select_lex->master_unit()->exclude_level();
}

void subselect_union_engine::exclude()
{
  unit->exclude_level();
}


void subselect_indexsubquery_engine::exclude()
{
  //this never should be called
  DBUG_ASSERT(0);
}


table_map subselect_engine::calc_const_tables(TABLE_LIST *table)
{
  table_map map= 0;
  for (; table; table= table->next_leaf)
  {
    TABLE *tbl= table->table;
    if (tbl && tbl->const_table)
      map|= tbl->map;
  }
  return map;
}


table_map subselect_single_select_engine::upper_select_const_tables() const
{
  return calc_const_tables(select_lex->outer_select()->leaf_tables);
}


table_map subselect_union_engine::upper_select_const_tables() const
{
  return calc_const_tables(unit->outer_select()->leaf_tables);
}


void subselect_single_select_engine::print(String *str,
                                           enum_query_type query_type)
{
  select_lex->print(item->unit->thd, str, query_type);
}


void subselect_union_engine::print(String *str, enum_query_type query_type)
{
  unit->print(str, query_type);
}


/*
TODO:
The ::print method below should be changed as follows. Do it after
all other tests pass.

void subselect_indexsubquery_engine::print(String *str)
{
  KEY *key_info= tab->table->key_info + tab->ref.key;
  str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  for (uint i= 0; i < key_info->key_parts; i++)
    tab->ref.items[i]->print(str);
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
                                           enum_query_type query_type)
{
  if (unique)
    str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  else
    str->append(STRING_WITH_LEN("<index_lookup>("));
  tab->ref.items[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" in "));
  if (tab->table->pos_in_table_list &&
           tab->table->pos_in_table_list->uses_materialization())
  {
    /*
      For materialized derived tables/views use table/view alias instead of
      temporary table name, as it changes on each run and not acceptable for
      EXPLAIN EXTENDED.
    */
    str->append(tab->table->alias, strlen(tab->table->alias));
  }
  else if (tab->table->s->table_category == TABLE_CATEGORY_TEMPORARY)
  {
    // Could be from subselect_hash_sj_engine.
    str->append(STRING_WITH_LEN("<temporary table>"));
  }
  else
    str->append(tab->table->s->table_name.str, tab->table->s->table_name.length);
  KEY *key_info= tab->table->key_info+ tab->ref.key;
  str->append(STRING_WITH_LEN(" on "));
  str->append(key_info->name);
  if (check_null)
    str->append(STRING_WITH_LEN(" checking NULL"));
  if (cond)
  {
    str->append(STRING_WITH_LEN(" where "));
    cond->print(str, query_type);
  }
  if (having)
  {
    str->append(STRING_WITH_LEN(" having "));
    having->print(str, query_type);
  }
  str->append(')');
}

/**
  change select_result object of engine.

  @param si		new subselect Item
  @param res		new select_result object

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool subselect_single_select_engine::change_result(Item_subselect *si,
                                                 select_result_interceptor *res)
{
  item= si;
  result= res;
  return select_lex->join->change_result(result);
}


/**
  change select_result object of engine.

  @param si		new subselect Item
  @param res		new select_result object

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool subselect_union_engine::change_result(Item_subselect *si,
                                           select_result_interceptor *res)
{
  item= si;
  int rc= unit->change_result(res, result);
  result= res;
  return rc;
}


/**
  change select_result emulation, never should be called.

  @param si		new subselect Item
  @param res		new select_result object

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool subselect_indexsubquery_engine::change_result(Item_subselect *si,
                                                   select_result_interceptor *res)
{
  DBUG_ASSERT(0);
  return TRUE;
}


/**
  Report about presence of tables in subquery.

  @retval
    TRUE  there are not tables used in subquery
  @retval
    FALSE there are some tables in subquery
*/
bool subselect_single_select_engine::no_tables() const
{
  return(select_lex->table_list.elements == 0);
}


/*
  Check statically whether the subquery can return NULL

  SINOPSYS
    subselect_single_select_engine::may_be_null()

  RETURN
    FALSE  can guarantee that the subquery never return NULL
    TRUE   otherwise
*/
bool subselect_single_select_engine::may_be_null() const
{
  return ((no_tables() && !join->conds && !join->having) ? maybe_null : 1);
}


/**
  Report about presence of tables in subquery.

  @retval
    TRUE  there are not tables used in subquery
  @retval
    FALSE there are some tables in subquery
*/
bool subselect_union_engine::no_tables() const
{
  for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
  {
    if (sl->table_list.elements)
      return FALSE;
  }
  return TRUE;
}


/**
  Report about presence of tables in subquery.

  @retval
    TRUE  there are not tables used in subquery
  @retval
    FALSE there are some tables in subquery
*/

bool subselect_indexsubquery_engine::no_tables() const
{
  /* returning value is correct, but this method should never be called */
  return 0;
}


/******************************************************************************
  WL#1110 - Implementation of class subselect_hash_sj_engine
******************************************************************************/


/**
  Create all structures needed for subquery execution using hash semijoin.

  @detail
  - Create a temporary table to store the result of the IN subquery. The
    temporary table has one hash index on all its columns. If single-column,
    the index allows at most one NULL row.
  - Create a new result sink that sends the result stream of the subquery to
    the temporary table,
  - Create and initialize a new JOIN_TAB, and TABLE_REF objects to perform
    lookups into the indexed temporary table.

  @param tmp_columns  columns of temporary table

  @notice:
    Currently Item_subselect::init() already chooses and creates at parse
    time an engine with a corresponding JOIN to execute the subquery.

  @retval TRUE  if error
  @retval FALSE otherwise
*/

bool subselect_hash_sj_engine::setup(List<Item> *tmp_columns)
{
  /* The result sink where we will materialize the subquery result. */
  select_union  *tmp_result_sink;
  /* The table into which the subquery is materialized. */
  TABLE         *tmp_table;
  KEY           *tmp_key; /* The only index on the temporary table. */
  uint          tmp_key_parts; /* Number of keyparts in tmp_key. */
  Item_in_subselect *item_in= (Item_in_subselect *) item;

  DBUG_ENTER("subselect_hash_sj_engine::setup");

  /* 1. Create/initialize materialization related objects. */

  /*
    Create and initialize a select result interceptor that stores the
    result stream in a temporary table. The temporary table itself is
    managed (created/filled/etc) internally by the interceptor.
  */
  if (!(tmp_result_sink= new select_union))
    DBUG_RETURN(TRUE);
  THD * const thd= item->unit->thd;
  if (tmp_result_sink->create_result_table(
                         thd, tmp_columns, true,
                         thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS,
                         "materialized-subquery", true, true))
    DBUG_RETURN(TRUE);

  tmp_table= tmp_result_sink->table;
  tmp_key= tmp_table->key_info;
  tmp_key_parts= tmp_key->user_defined_key_parts;

  /*
     If the subquery has blobs, or the total key lenght is bigger than some
     length, then the created index cannot be used for lookups and we
     can't use hash semi join. If this is the case, delete the temporary
     table since it will not be used, and tell the caller we failed to
     initialize the engine.
  */
  if (tmp_table->s->keys == 0)
  {
    DBUG_ASSERT(tmp_table->s->db_type() == myisam_hton);
    DBUG_ASSERT(
      tmp_table->s->uniques ||
      tmp_table->key_info->key_length >= tmp_table->file->max_key_length() ||
      tmp_table->key_info->user_defined_key_parts >
      tmp_table->file->max_key_parts());
    free_tmp_table(thd, tmp_table);
    delete result;
    result= NULL;
    DBUG_RETURN(TRUE);
  }
  result= tmp_result_sink;

  /*
    Make sure there is only one index on the temp table, and it doesn't have
    the extra key part created when s->uniques > 0.
  */
  DBUG_ASSERT(tmp_table->s->keys == 1 &&
              tmp_columns->elements == tmp_key_parts);

  /* 2. Create/initialize execution related objects. */

  /*
    Create and initialize the JOIN_TAB that represents an index lookup
    plan operator into the materialized subquery result. Notice that:
    - this JOIN_TAB has no corresponding JOIN (and doesn't need one), and
    - here we initialize only those members that are used by
      subselect_indexsubquery_engine, so these objects are incomplete.
  */ 
  JOIN_TAB * const tmp_tab= new (thd->mem_root) JOIN_TAB;
  if (tmp_tab == NULL)
    DBUG_RETURN(TRUE);
  tmp_tab->table= tmp_table;
  tmp_tab->ref.key= 0; /* The only temp table index. */
  tmp_tab->ref.key_length= tmp_key->key_length;
  if (!(tmp_tab->ref.key_buff=
        (uchar*) thd->calloc(ALIGN_SIZE(tmp_key->key_length) * 2)) ||
      !(tmp_tab->ref.key_copy=
        (store_key**) thd->alloc((sizeof(store_key*) * tmp_key_parts))) ||
      !(tmp_tab->ref.items=
        (Item**) thd->alloc(sizeof(Item*) * tmp_key_parts)))
    DBUG_RETURN(TRUE);

  uchar *cur_ref_buff= tmp_tab->ref.key_buff;

  /*
    Like semijoin-materialization-lookup (see create_subquery_equalities()),
    create an artificial condition to post-filter those rows matched by index
    lookups that cannot be distinguished by the index lookup procedure, e.g.
    because of truncation (if the outer column type's length is bigger than
    the inner column type's, index lookup will use a truncated outer
    value as search key, yielding false positives).
    Prepared statements execution requires that fix_fields is called
    for every execution. In order to call fix_fields we need to create a
    Name_resolution_context and a corresponding TABLE_LIST for the temporary
    table for the subquery, so that all column references to the materialized
    subquery table can be resolved correctly.
  */
  DBUG_ASSERT(cond == NULL);
  if (!(cond= new Item_cond_and))
    DBUG_RETURN(TRUE);
  /*
    Table reference for tmp_table that is used to resolve column references
    (Item_fields) to columns in tmp_table.
  */
  TABLE_LIST *tmp_table_ref;
  if (!(tmp_table_ref= (TABLE_LIST*) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(TRUE);

  tmp_table_ref->init_one_table("", 0, "materialized-subquery", 21,
                                "materialized-subquery", TL_READ);
  tmp_table_ref->table= tmp_table;

  /* Name resolution context for all tmp_table columns created below. */
  Name_resolution_context *context= new Name_resolution_context;
  context->init();
  context->first_name_resolution_table=
    context->last_name_resolution_table= tmp_table_ref;
  
  KEY_PART_INFO *key_parts= tmp_key->key_part;
  for (uint part_no= 0; part_no < tmp_key_parts; part_no++)
  {
    /* New equi-join condition for the current column. */
    Item_func_eq *eq_cond; 
    /* Item for the corresponding field from the materialized temp table. */
    Item_field *right_col_item;
    const bool nullable= key_parts[part_no].field->real_maybe_null();
    tmp_tab->ref.items[part_no]= item_in->left_expr->element_index(part_no);

    if (!(right_col_item= new Item_field(thd, context, 
                                         key_parts[part_no].field)) ||
        !(eq_cond= new Item_func_eq(tmp_tab->ref.items[part_no],
                                    right_col_item)) ||
        ((Item_cond_and*)cond)->add(eq_cond))
    {
      delete cond;
      cond= NULL;
      DBUG_RETURN(TRUE);
    }

    tmp_tab->ref.key_copy[part_no]= 
      new store_key_item(thd, key_parts[part_no].field,
                         /* TODO:
                            the NULL byte is taken into account in
                            key_parts[part_no].store_length, so instead of
                            cur_ref_buff + test(maybe_null), we could
                            use that information instead.
                         */
                         cur_ref_buff + (nullable ? 1 : 0),
                         nullable ? cur_ref_buff : 0,
                         key_parts[part_no].length,
                         tmp_tab->ref.items[part_no]);
    if (nullable &&          // nullable column in tmp table,
        // and UNKNOWN should not be interpreted as FALSE
        !item_in->is_top_level_item())
    {
      // It must be the single column, or we wouldn't be here
      DBUG_ASSERT(tmp_key_parts == 1);
      // Be ready to search for NULL into inner column:
      tmp_tab->ref.null_ref_key= cur_ref_buff;
      mat_table_has_nulls= NEX_UNKNOWN;
    }
    else
    {
      tmp_tab->ref.null_ref_key= NULL;
      mat_table_has_nulls= NEX_IRRELEVANT_OR_FALSE;
    }

    cur_ref_buff+= key_parts[part_no].store_length;
  }
  tmp_tab->ref.key_err= 1;
  tmp_tab->ref.key_parts= tmp_key_parts;

  if (cond->fix_fields(thd, &cond))
    DBUG_RETURN(TRUE);

  // Set 'tab' only when function cannot fail, because of assert in destructor
  tab= tmp_tab;

  /*
    Create and optimize the JOIN that will be used to materialize
    the subquery if not yet created.
  */
  materialize_engine->prepare();
  /* Let our engine reuse this query plan for materialization. */
  materialize_engine->join->change_result(result);

  DBUG_RETURN(FALSE);
}


subselect_hash_sj_engine::~subselect_hash_sj_engine()
{
  /* Assure that cleanup has been called for this engine. */
  DBUG_ASSERT(!tab);

  delete result;
}


/**
  Cleanup performed after each PS execution.

  @detail
  Called in the end of JOIN::prepare for PS from Item_subselect::cleanup.
*/

void subselect_hash_sj_engine::cleanup()
{
  DBUG_ENTER("subselect_hash_sj_engine::cleanup");
  is_materialized= false;
  result->cleanup(); /* Resets the temp table as well. */
  THD * const thd= item->unit->thd;
  DEBUG_SYNC(thd, "before_index_end_in_subselect");
  if (tab->table->file->inited)
    tab->table->file->ha_index_end();  // Close the scan over the index
  free_tmp_table(thd, tab->table);
  tab= NULL;
  materialize_engine->cleanup();
  DBUG_VOID_RETURN;
}


/**
  Execute a subquery IN predicate via materialization.

  @detail
  If needed materialize the subquery into a temporary table, then
  copmpute the predicate via a lookup into this table.

  @retval TRUE  if error
  @retval FALSE otherwise
*/

bool subselect_hash_sj_engine::exec()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;
  TABLE *const table= tab->table;
  DBUG_ENTER("subselect_hash_sj_engine::exec");

  /*
    Optimize and materialize the subquery during the first execution of
    the subquery predicate.
  */
  if (!is_materialized)
  {
    bool res;
    THD * const thd= item->unit->thd;
    SELECT_LEX *save_select= thd->lex->current_select;
    thd->lex->current_select= materialize_engine->select_lex;
    if ((res= materialize_engine->join->optimize()))
      goto err; /* purecov: inspected */

    materialize_engine->join->exec();
    if ((res= test(materialize_engine->join->error || thd->is_fatal_error)))
      goto err;

    /*
      TODO:
      - Unlock all subquery tables as we don't need them. To implement this
        we need to add new functionality to JOIN::join_free that can unlock
        all tables in a subquery (and all its subqueries).
      - The temp table used for grouping in the subquery can be freed
        immediately after materialization (yet it's done together with
        unlocking).
     */
    is_materialized= TRUE;

    // Calculate row count:
    table->file->info(HA_STATUS_VARIABLE);

    /* Set tmp_param only if its usable, i.e. tmp_param->copy_field != NULL. */
    tmp_param= &(item_in->unit->outer_select()->join->tmp_table_param);
    if (tmp_param && !tmp_param->copy_field)
      tmp_param= NULL;

err:
    thd->lex->current_select= save_select;
    if (res)
      DBUG_RETURN(res);
  } // if (!is_materialized)

  if (table->file->stats.records == 0)
  {
    // The correct answer is FALSE.
    item_in->value= false;
    DBUG_RETURN(false);
  }
  /*
    Here we could be brutal and set item_in->null_value. But we prefer to be
    well-behaved and rather set the properties which
    Item_in_subselect::val_bool() and Item_in_optimizer::val_int() expect,
    and then those functions will set null_value based on those properties.
  */
  if (item_in->left_expr->element_index(0)->null_value)
  {
    /*
      The first outer expression oe1 is NULL. It is the single outer
      expression because if there would be more ((oe1,oe2,...)IN(...)) then
      either they would be non-nullable (so we wouldn't be here) or the
      predicate would be top-level (so we wouldn't be here,
      Item_in_optimizer::val_int() would have short-cut). The correct answer
      is UNKNOWN. Do as if searching with all triggered conditions disabled:
      this would surely find a row. The caller will translate this to UNKNOWN.
    */
    DBUG_ASSERT(item_in->left_expr->cols() == 1);
    item_in->value= true;
    DBUG_RETURN(false);
  }

  if (subselect_indexsubquery_engine::exec())  // Search with index
    DBUG_RETURN(true);

  if (!item_in->value && // no exact match
      mat_table_has_nulls != NEX_IRRELEVANT_OR_FALSE)
  {
    /*
      There is only one outer expression. It's not NULL. exec() above has set
      the answer to FALSE, but if there exists an inner NULL in the temporary
      table, then the correct answer is UNKNOWN, so let's find out.
    */
    if (mat_table_has_nulls == NEX_UNKNOWN)   // We do not know yet
    {
      // Search for NULL inside tmp table, and remember the outcome.
      *tab->ref.null_ref_key= 1;
      if (!table->file->inited &&
          table->file->ha_index_init(tab->ref.key, false /* sorted */))
        DBUG_RETURN(true);
      if (safe_index_read(tab) == 1)
        DBUG_RETURN(true);
      *tab->ref.null_ref_key= 0; // prepare for next searches of non-NULL
      mat_table_has_nulls=
        (table->status == 0) ? NEX_TRUE : NEX_IRRELEVANT_OR_FALSE;
    }
    if (mat_table_has_nulls == NEX_TRUE)
    {
      /*
        There exists an inner NULL. The correct answer is UNKNOWN.
        Do as if searching with all triggered conditions enabled; that
        would not find any match, but Item_is_not_null_test would notice a
        NULL:
      */
      item_in->value= false;
      item_in->was_null= true;
    }
  }
  DBUG_RETURN(false);
}


/**
  Print the state of this engine into a string for debugging and views.
*/

void subselect_hash_sj_engine::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN(" <materialize> ("));
  materialize_engine->print(str, query_type);
  str->append(STRING_WITH_LEN(" ), "));
  if (tab)
    subselect_indexsubquery_engine::print(str, query_type);
  else
    str->append(STRING_WITH_LEN(
           "<the access method for lookups is not yet created>"
         ));
}

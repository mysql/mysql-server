/* Copyright (c) 2002, 2011, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file

  @brief
  subselect Item

  @todo
    - add function from mysql_select that use JOIN* as parameter to JOIN
    methods (sql_select.h/sql_select.cc)
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

double get_post_group_estimate(JOIN* join, double join_op_rows);


Item_subselect::Item_subselect():
  Item_result_field(), value_assigned(0), own_engine(0), thd(0), old_engine(0), 
  used_tables_cache(0), have_to_be_excluded(0), const_item_cache(1),
  inside_first_fix_fields(0), done_first_fix_fields(FALSE), 
  expr_cache(0), forced_const(FALSE), substitution(0), engine(0), eliminated(FALSE),
  engine_changed(0), changed(0), is_correlated(FALSE)
{
  DBUG_ENTER("Item_subselect::Item_subselect");
  DBUG_PRINT("enter", ("this: 0x%lx", (ulong) this));
#ifndef DBUG_OFF
  exec_counter= 0;
#endif
  with_subselect= 1;
  reset();
  /*
    Item value is NULL if select_result_interceptor didn't change this value
    (i.e. some rows will be found returned)
  */
  null_value= TRUE;
  DBUG_VOID_RETURN;
}


void Item_subselect::init(st_select_lex *select_lex,
			  select_result_interceptor *result)
{
  /*
    Please see Item_singlerow_subselect::invalidate_and_restore_select_lex(),
    which depends on alterations to the parse tree implemented here.
  */

  DBUG_ENTER("Item_subselect::init");
  DBUG_PRINT("enter", ("select_lex: 0x%lx  this: 0x%lx",
                       (ulong) select_lex, (ulong) this));
  unit= select_lex->master_unit();
  thd= unit->thd;

  if (unit->item)
  {
    /*
      Item can be changed in JOIN::prepare while engine in JOIN::optimize
      => we do not copy old_engine here
    */
    engine= unit->item->engine;
    own_engine= FALSE;
    parsing_place= unit->item->parsing_place;
    thd->change_item_tree((Item**)&unit->item, this);
    engine->change_result(this, result, TRUE);
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
      engine= new subselect_union_engine(thd, unit, result, this);
    else
      engine= new subselect_single_select_engine(thd, select_lex, result, this);
  }
  {
    SELECT_LEX *upper= unit->outer_select();
    if (upper->parsing_place == IN_HAVING)
      upper->subquery_in_having= 1;
    /* The subquery is an expression cache candidate */
    upper->expr_cache_may_be_used[upper->parsing_place]= TRUE;
  }
  DBUG_PRINT("info", ("engine: 0x%lx", (ulong)engine));
  DBUG_VOID_RETURN;
}

st_select_lex *
Item_subselect::get_select_lex()
{
  return unit->first_select();
}

void Item_subselect::cleanup()
{
  DBUG_ENTER("Item_subselect::cleanup");
  Item_result_field::cleanup();
  if (old_engine)
  {
    if (engine)
      engine->cleanup();
    engine= old_engine;
    old_engine= 0;
  }
  if (engine)
    engine->cleanup();
  reset();
  value_assigned= 0;
  expr_cache= 0;
  forced_const= FALSE;
  DBUG_PRINT("info", ("exec_counter: %d", exec_counter));
#ifndef DBUG_OFF
  exec_counter= 0;
#endif
  DBUG_VOID_RETURN;
}


void Item_singlerow_subselect::cleanup()
{
  DBUG_ENTER("Item_singlerow_subselect::cleanup");
  value= 0; row= 0;
  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
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
  /*
    TODO: This breaks the commented assert in add_strategy().
    in_strategy&= ~SUBS_STRATEGY_CHOSEN;
  */
  first_execution= TRUE;
  pushed_cond_guards= NULL;
  Item_subselect::cleanup();
  DBUG_VOID_RETURN;
}


void Item_allany_subselect::cleanup()
{
  /*
    The MAX/MIN transformation through injection is reverted through the
    change_item_tree() mechanism. Revert the select_lex object of the
    query to its initial state.
  */
  for (SELECT_LEX *sl= unit->first_select();
       sl; sl= sl->next_select())
    if (test_set_strategy(SUBS_MAXMIN_INJECTED))
      sl->with_sum_func= false;
  Item_in_subselect::cleanup();
}


Item_subselect::~Item_subselect()
{
  DBUG_ENTER("Item_subselect::~Item_subselect");
  DBUG_PRINT("enter", ("this: 0x%lx", (ulong) this));
  if (own_engine)
    delete engine;
  else
    engine->cleanup();
  engine= NULL;
  DBUG_VOID_RETURN;
}

bool
Item_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_subselect::select_transformer");
  DBUG_RETURN(false);
}


bool Item_subselect::fix_fields(THD *thd_param, Item **ref)
{
  char const *save_where= thd_param->where;
  uint8 uncacheable;
  bool res;

  DBUG_ASSERT(fixed == 0);
  engine->set_thd((thd= thd_param));
  if (!done_first_fix_fields)
  {
    done_first_fix_fields= TRUE;
    inside_first_fix_fields= TRUE;
    upper_refs.empty();
    /*
      psergey-todo: remove _first_fix_fields calls, we need changes on every
      execution
    */
  }

  eliminated= FALSE;
  parent_select= thd_param->lex->current_select;

  if (check_stack_overrun(thd, STACK_MIN_SIZE, (uchar*)&res))
    return TRUE;
  
  
  if (!(res= engine->prepare()))
  {
    // all transformation is done (used by prepared statements)
    changed= 1;
    inside_first_fix_fields= FALSE;

    /*
      Substitute the current item with an Item_in_optimizer that was
      created by Item_in_subselect::select_in_like_transformer and
      call fix_fields for the substituted item which in turn calls
      engine->prepare for the subquery predicate.
    */
    if (substitution)
    {
      /*
        If the top item of the WHERE/HAVING condition changed,
        set correct WHERE/HAVING for PS.
      */
      if (unit->outer_select()->where == (*ref))
        unit->outer_select()->where= substitution;
      else if (unit->outer_select()->having == (*ref))
        unit->outer_select()->having= substitution;

      (*ref)= substitution;
      substitution->name= name;
      substitution->name_length= name_length;
      if (have_to_be_excluded)
	engine->exclude();
      substitution= 0;
      thd->where= "checking transformed subquery";
      if (!(*ref)->fixed)
	res= (*ref)->fix_fields(thd, ref);
      goto end;

    }
    // Is it one field subselect?
    if (engine->cols() > max_columns)
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);

      goto end;
    }
    fix_length_and_dec();
  }
  else
    goto end;
  
  if ((uncacheable= engine->uncacheable()))
  {
    const_item_cache= 0;
    if (uncacheable & UNCACHEABLE_RAND)
      used_tables_cache|= RAND_TABLE_BIT;
  }
  fixed= 1;

end:
  done_first_fix_fields= FALSE;
  inside_first_fix_fields= FALSE;
  thd->where= save_where;
  return res;
}


bool Item_subselect::enumerate_field_refs_processor(uchar *arg)
{
  List_iterator<Ref_to_outside> it(upper_refs);
  Ref_to_outside *upper;
  
  while ((upper= it++))
  {
    if (upper->item->walk(&Item::enumerate_field_refs_processor, FALSE, arg))
      return TRUE;
  }
  return FALSE;
}

bool Item_subselect::mark_as_eliminated_processor(uchar *arg)
{
  eliminated= TRUE;
  return FALSE;
}


/**
  Remove a subselect item from its unit so that the unit no longer
  represents a subquery.

  @param arg  unused parameter

  @return
    FALSE to force the evaluation of the processor for the subsequent items.
*/

bool Item_subselect::eliminate_subselect_processor(uchar *arg)
{
  unit->item= NULL;
  unit->exclude_from_tree();
  eliminated= TRUE;
  return FALSE;
}


/**
  Adjust the master select of the subquery to be the fake_select which
  represents the whole UNION right above the subquery, instead of the
  last query of the UNION.

  @param arg  pointer to the fake select

  @return
    FALSE to force the evaluation of the processor for the subsequent items.
*/

bool Item_subselect::set_fake_select_as_master_processor(uchar *arg)
{
  SELECT_LEX *fake_select= (SELECT_LEX*) arg;
  /*
    Move the st_select_lex_unit of a subquery from a global ORDER BY clause to
    become a direct child of the fake_select of a UNION. In this way the
    ORDER BY that is applied to the temporary table that contains the result of
    the whole UNION, and all columns in the subquery are resolved against this
    table. The transformation is applied only for immediate child subqueries of
    a UNION query.
  */
  if (unit->outer_select()->master_unit()->fake_select_lex == fake_select)
  {
    /*
      Set the master of the subquery to be the fake select (i.e. the whole
      UNION), instead of the last query in the UNION.
    */
    fake_select->add_slave(unit);
    DBUG_ASSERT(unit->outer_select() == fake_select);
    /* Adjust the name resolution context hierarchy accordingly. */
    for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
      sl->context.outer_context= &(fake_select->context);
    /*
      Undo Item_subselect::eliminate_subselect_processor because at that phase
      we don't know yet that the ORDER clause will be moved to the fake select.
    */
    unit->item= this;
    eliminated= FALSE;
  }
  return FALSE;
}


bool Item_subselect::mark_as_dependent(THD *thd, st_select_lex *select, 
                                       Item *item)
{
  if (inside_first_fix_fields)
  {
    is_correlated= TRUE;
    Ref_to_outside *upper;
    if (!(upper= new (thd->stmt_arena->mem_root) Ref_to_outside()))
      return TRUE;
    upper->select= select;
    upper->item= item;
    if (upper_refs.push_back(upper, thd->stmt_arena->mem_root))
      return TRUE;
  }
  return FALSE;
}


/*
  Adjust attributes after our parent select has been merged into grandparent

  DESCRIPTION
    Subquery is a composite object which may be correlated, that is, it may
    have
    1. references to tables of the parent select (i.e. one that has the clause
      with the subquery predicate)
    2. references to tables of the grandparent select
    3. references to tables of further ancestors.
    
    Before the pullout, this item indicates:
    - #1 with table bits in used_tables()
    - #2 and #3 with OUTER_REF_TABLE_BIT.

    After parent has been merged with grandparent:
    - references to parent and grandparent tables should be indicated with 
      table bits.
    - references to greatgrandparent and further ancestors - with
      OUTER_REF_TABLE_BIT.
*/

void Item_subselect::fix_after_pullout(st_select_lex *new_parent, Item **ref)
{
  recalc_used_tables(new_parent, TRUE);
  parent_select= new_parent;
}


class Field_fixer: public Field_enumerator
{
public:
  table_map used_tables; /* Collect used_tables here */
  st_select_lex *new_parent; /* Select we're in */
  virtual void visit_field(Item_field *item)
  {
    //for (TABLE_LIST *tbl= new_parent->leaf_tables; tbl; tbl= tbl->next_local)
    //{
    //  if (tbl->table == field->table)
    //  {
        used_tables|= item->field->table->map;
    //    return;
    //  }
    //}
    //used_tables |= OUTER_REF_TABLE_BIT;
  }
};


/*
  Recalculate used_tables_cache 
*/

void Item_subselect::recalc_used_tables(st_select_lex *new_parent, 
                                        bool after_pullout)
{
  List_iterator<Ref_to_outside> it(upper_refs);
  Ref_to_outside *upper;
  
  used_tables_cache= 0;
  while ((upper= it++))
  {
    bool found= FALSE;
    /*
      Check if
        1. the upper reference refers to the new immediate parent select, or
        2. one of the further ancestors.

      We rely on the fact that the tree of selects is modified by some kind of
      'flattening', i.e. a process where child selects are merged into their
      parents.
      The merged selects are removed from the select tree but keep pointers to
      their parents.
    */
    for (st_select_lex *sel= upper->select; sel; sel= sel->outer_select())
    {
      /* 
        If we've reached the new parent select by walking upwards from
        reference's original select, this means that the reference is now 
        referring to the direct parent:
      */
      if (sel == new_parent)
      {
        found= TRUE;
        /* 
          upper->item may be NULL when we've referred to a grouping function,
          in which case we don't care about what it's table_map really is,
          because item->with_sum_func==1 will ensure correct placement of the
          item.
        */
        if (upper->item)
        {
          // Now, iterate over fields and collect used_tables() attribute:
          Field_fixer fixer;
          fixer.used_tables= 0;
          fixer.new_parent= new_parent;
          upper->item->walk(&Item::enumerate_field_refs_processor, FALSE,
                            (uchar*)&fixer);
          used_tables_cache |= fixer.used_tables;
          upper->item->walk(&Item::update_table_bitmaps_processor, FALSE, NULL);
/*
          if (after_pullout)
            upper->item->fix_after_pullout(new_parent, &(upper->item));
          upper->item->update_used_tables();
*/          
        }
      }
    }
    if (!found)
      used_tables_cache|= OUTER_REF_TABLE_BIT;
  }
  /* 
    Don't update const_tables_cache yet as we don't yet know which of the
    parent's tables are constant. Parent will call update_used_tables() after
    he has done const table detection, and that will be our chance to update
    const_tables_cache.
  */
}

bool Item_subselect::walk(Item_processor processor, bool walk_subquery,
                          uchar *argument)
{
  if (!(unit->uncacheable & ~UNCACHEABLE_DEPENDENT) && engine->is_executed() &&
      !unit->describe)
  {
    /*
      The subquery has already been executed (for real, it wasn't EXPLAIN's
      fake execution) so it should not matter what it has inside.
      
      The actual reason for not walking inside is that parts of the subquery
      (e.g. JTBM join nests and their IN-equality conditions may have been 
       invalidated by irreversible cleanups (those happen after an uncorrelated 
       subquery has been executed).
    */
    return (this->*processor)(argument);
  }

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
      /* TODO: why does this walk WHERE/HAVING but not ON expressions of outer joins? */

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


bool Item_subselect::exec()
{
  int res;

  /*
    Do not execute subselect in case of a fatal error
    or if the query has been killed.
  */
  if (thd->is_error() || thd->killed)
    return 1;

  DBUG_ASSERT(!thd->lex->context_analysis_only);
  /*
    Simulate a failure in sub-query execution. Used to test e.g.
    out of memory or query being killed conditions.
  */
  DBUG_EXECUTE_IF("subselect_exec_fail", return 1;);

  res= engine->exec();
#ifndef DBUG_OFF
  ++exec_counter;
#endif
  if (engine_changed)
  {
    engine_changed= 0;
    return exec();
  }
  return (res);
}


void Item_subselect::get_cache_parameters(List<Item> &parameters)
{
  Collect_deps_prm prm= {&parameters,
    unit->first_select()->nest_level_base,
    unit->first_select()->nest_level};
  walk(&Item::collect_outer_ref_processor, TRUE, (uchar*)&prm);
}

int Item_in_subselect::optimize(double *out_rows, double *cost)
{
  int res;
  DBUG_ENTER("Item_in_subselect::optimize");
  SELECT_LEX *save_select= thd->lex->current_select;
  JOIN *join= unit->first_select()->join;

  thd->lex->current_select= join->select_lex;
  if ((res= join->optimize()))
    DBUG_RETURN(res);

  /* Calculate #rows and cost of join execution */
  join->get_partial_cost_and_fanout(join->table_count - join->const_tables, 
                                    table_map(-1),
                                    cost, out_rows);

  /*
    Adjust join output cardinality. There can be these cases:
    - Have no GROUP BY and no aggregate funcs: we won't get into this 
      function because such join will be processed as a merged semi-join 
      (TODO: does it really mean we don't need to handle such cases here at 
       all? put ASSERT)
    - Have no GROUP BY but have aggregate funcs: output is 1 record.
    - Have GROUP BY and have (or not) aggregate funcs:  need to adjust output 
      cardinality.
  */
  thd->lex->current_select= save_select;
  if (!join->group_list && !join->group_optimized_away &&
      join->tmp_table_param.sum_func_count)
  {
    DBUG_PRINT("info",("Materialized join will have only 1 row (it has "
                       "aggregates but no GROUP BY"));
    *out_rows= 1;
  }
  
  /* Now with grouping */
  if (join->group_list)
  {
    DBUG_PRINT("info",("Materialized join has grouping, trying to estimate it"));
    double output_rows= get_post_group_estimate(join, *out_rows);
    DBUG_PRINT("info",("Got value of %g", output_rows));
    *out_rows= output_rows;
  }

  DBUG_RETURN(res);

}


/**
  Check if an expression cache is needed for this subquery

  @param thd             Thread handle

  @details
  The function checks whether a cache is needed for a subquery and whether
  the result of the subquery can be put in cache.

  @retval TRUE  cache is needed
  @retval FALSE otherwise
*/

bool Item_subselect::expr_cache_is_needed(THD *thd)
{
  return ((engine->uncacheable() & UNCACHEABLE_DEPENDENT) &&
          engine->cols() == 1 &&
          optimizer_flag(thd, OPTIMIZER_SWITCH_SUBQUERY_CACHE) &&
          !(engine->uncacheable() & (UNCACHEABLE_RAND |
                                     UNCACHEABLE_SIDEEFFECT)));
}


/**
  Check if the left IN argument contains NULL values.

  @retval TRUE  there are NULLs
  @retval FALSE otherwise
*/

inline bool Item_in_subselect::left_expr_has_null()
{
  return (*(optimizer->get_cache()))->null_value;
}


/**
  Check if an expression cache is needed for this subquery

  @param thd             Thread handle

  @details
  The function checks whether a cache is needed for a subquery and whether
  the result of the subquery can be put in cache.

  @note
  This method allows many columns in the subquery because it is supported by
  Item_in optimizer and result of the IN subquery will be scalar in this
  case.

  @retval TRUE  cache is needed
  @retval FALSE otherwise
*/

bool Item_in_subselect::expr_cache_is_needed(THD *thd)
{
  return (optimizer_flag(thd, OPTIMIZER_SWITCH_SUBQUERY_CACHE) &&
          !(engine->uncacheable() & (UNCACHEABLE_RAND |
                                     UNCACHEABLE_SIDEEFFECT)));
}


/*
  Compute the IN predicate if the left operand's cache changed.
*/

bool Item_in_subselect::exec()
{
  DBUG_ENTER("Item_in_subselect::exec");
  /*
    Initialize the cache of the left predicate operand. This has to be done as
    late as now, because Cached_item directly contains a resolved field (not
    an item, and in some cases (when temp tables are created), these fields
    end up pointing to the wrong field. One solution is to change Cached_item
    to not resolve its field upon creation, but to resolve it dynamically
    from a given Item_ref object.
    TODO: the cache should be applied conditionally based on:
    - rules - e.g. only if the left operand is known to be ordered, and/or
    - on a cost-based basis, that takes into account the cost of a cache
      lookup, the cache hit rate, and the savings per cache hit.
  */
  if (!left_expr_cache && (test_strategy(SUBS_MATERIALIZATION)))
    init_left_expr_cache();

  /*
    If the new left operand is already in the cache, reuse the old result.
    Use the cached result only if this is not the first execution of IN
    because the cache is not valid for the first execution.
  */
  if (!first_execution && left_expr_cache &&
      test_if_item_cache_changed(*left_expr_cache) < 0)
    DBUG_RETURN(FALSE);

  /*
    The exec() method below updates item::value, and item::null_value, thus if
    we don't call it, the next call to item::val_int() will return whatever
    result was computed by its previous call.
  */
  DBUG_RETURN(Item_subselect::exec());
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
  return (table_map) ((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN)? 
                      used_tables_cache : 0L);
}


bool Item_subselect::const_item() const
{
  return (thd->lex->context_analysis_only ?
          FALSE :
          forced_const || const_item_cache);
}

Item *Item_subselect::get_tmp_table_item(THD *thd_arg)
{
  if (!with_sum_func && !const_item())
    return new Item_field(result_field);
  return copy_or_same(thd_arg);
}

void Item_subselect::update_used_tables()
{
  if (!forced_const)
  {
    recalc_used_tables(parent_select, FALSE);
    if (!engine->uncacheable())
    {
      // did all used tables become static?
      if (!(used_tables_cache & ~engine->upper_select_const_tables()))
        const_item_cache= 1;
    }
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
  :Item_subselect(), value(0)
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
  st_select_lex *result= get_select_lex();

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
					     bool max_arg)
  :Item_singlerow_subselect(), was_values(TRUE)
{
  DBUG_ENTER("Item_maxmin_subselect::Item_maxmin_subselect");
  max= max_arg;
  init(select_lex,
       new select_max_min_finder_subselect(this, max_arg,
                                           parent->substype() ==
                                           Item_subselect::ALL_SUBS));
  max_columns= 1;
  maybe_null= 1;
  max_columns= 1;

  /*
    Following information was collected during performing fix_fields()
    of Items belonged to subquery, which will be not repeated
  */
  used_tables_cache= parent->get_used_tables_cache();
  const_item_cache= parent->const_item();

  /*
    this subquery always creates during preparation, so we can assign
    thd here
  */
  thd= thd_param;

  DBUG_VOID_RETURN;
}

void Item_maxmin_subselect::cleanup()
{
  DBUG_ENTER("Item_maxmin_subselect::cleanup");
  Item_singlerow_subselect::cleanup();

  /*
    By default it is TRUE to avoid TRUE reporting by
    Item_func_not_all/Item_func_nop_all if this item was never called.

    Engine exec() set it to FALSE by reset_value_registration() call.
    select_max_min_finder_subselect::send_data() set it back to TRUE if some
    value will be found.
  */
  was_values= TRUE;
  DBUG_VOID_RETURN;
}


void Item_maxmin_subselect::print(String *str, enum_query_type query_type)
{
  str->append(max?"<max>":"<min>", 5);
  Item_singlerow_subselect::print(str, query_type);
}


void Item_maxmin_subselect::no_rows_in_result()
{
  /*
    Subquery predicates outside of the SELECT list must be evaluated in order
    to possibly filter the special result row generated for implicit grouping
    if the subquery is in the HAVING clause.
    If the predicate is constant, we need its actual value in the only result
    row for queries with implicit grouping.
  */
  if (parsing_place != SELECT_LIST || const_item())
    return;
  value= Item_cache::get_cache(new Item_null());
  null_value= 0;
  was_values= 0;
  make_const();
}


void Item_singlerow_subselect::no_rows_in_result()
{
  /*
    Subquery predicates outside of the SELECT list must be evaluated in order
    to possibly filter the special result row generated for implicit grouping
    if the subquery is in the HAVING clause.
    If the predicate is constant, we need its actual value in the only result
    row for queries with implicit grouping.
  */
  if (parsing_place != SELECT_LIST || const_item())
    return;
  value= Item_cache::get_cache(new Item_null());
  reset();
  make_const();
}


void Item_singlerow_subselect::reset()
{
  Item_subselect::reset();
  if (value)
  {
    for(uint i= 0; i < engine->cols(); i++)
      row[i]->set_null();
  }
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

  @param join  Join object of the subquery (i.e. 'child' join).

  @retval false  The subquery was transformed
*/
bool
Item_singlerow_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_singlerow_subselect::select_transformer");
  if (changed)
    DBUG_RETURN(false);

  SELECT_LEX *select_lex= join->select_lex;
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
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		   ER_SELECT_REDUCED, warn_buff);
    }
    substitution= select_lex->item_list.head();
    /*
      as far as we moved content to upper level we have to fix dependences & Co
    */
    substitution->fix_after_pullout(select_lex->outer_select(), &substitution);
  }
  DBUG_RETURN(false);
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
  else
  {
    for (uint i= 0; i < max_columns; i++)
      row[i]->maybe_null= TRUE;
  }
}


/**
  Add an expression cache for this subquery if it is needed

  @param thd_arg         Thread handle

  @details
  The function checks whether an expression cache is needed for this item
  and if if so wraps the item into an item of the class
  Item_exp_cache_wrapper with an appropriate expression cache set up there.

  @note
  used from Item::transform()

  @return
  new wrapper item if an expression cache is needed,
  this item - otherwise
*/

Item* Item_singlerow_subselect::expr_cache_insert_transformer(uchar *thd_arg)
{
  THD *thd= (THD*) thd_arg;
  DBUG_ENTER("Item_singlerow_subselect::expr_cache_insert_transformer");

  if (expr_cache)
    DBUG_RETURN(expr_cache);

  if (expr_cache_is_needed(thd) &&
      (expr_cache= set_expr_cache(thd)))
    DBUG_RETURN(expr_cache);
  DBUG_RETURN(this);
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
  if (forced_const)
    return value->val_real();
  if (!exec() && !value->null_value)
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
  if (forced_const)
    return value->val_int();
  if (!exec() && !value->null_value)
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
  DBUG_ASSERT(fixed == 1);
  if (forced_const)
    return value->val_str(str);
  if (!exec() && !value->null_value)
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
  DBUG_ASSERT(fixed == 1);
  if (forced_const)
    return value->val_decimal(decimal_value);
  if (!exec() && !value->null_value)
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


bool Item_singlerow_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  if (forced_const)
    return value->val_bool();
  if (!exec() && !value->null_value)
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


bool Item_singlerow_subselect::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  if (forced_const)
    return value->get_date(ltime, fuzzydate);
  if (!exec() && !value->null_value)
  {
    null_value= FALSE;
    return value->get_date(ltime, fuzzydate);
  }
  else
  {
    reset();
    return 1;
  }
}


Item_exists_subselect::Item_exists_subselect(st_select_lex *select_lex):
  Item_subselect()
{
  DBUG_ENTER("Item_exists_subselect::Item_exists_subselect");
  bool val_bool();
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  null_value= FALSE; //can't be NULL
  maybe_null= 0; //can't be NULL
  value= 0;
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
  Item_exists_subselect(), 
  left_expr_cache(0), first_execution(TRUE), in_strategy(SUBS_NOT_TRANSFORMED),
  optimizer(0), pushed_cond_guards(NULL), emb_on_expr_nest(NULL),
  is_jtbm_merged(FALSE), is_jtbm_const_tab(FALSE), 
  is_flattenable_semijoin(FALSE),
  is_registered_semijoin(FALSE), 
  upper_item(0)
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  left_expr= left_exp;
  func= &eq_creator;
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  maybe_null= 1;
  abort_on_null= 0;
  reset();
  //if test_limit will fail then error will be reported to client
  test_limit(select_lex->master_unit());
  DBUG_VOID_RETURN;
}

int Item_in_subselect::get_identifier()
{
  return engine->get_identifier();
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


/**
  Initialize length and decimals for EXISTS  and inherited (IN/ALL/ANY)
  subqueries
*/

void Item_exists_subselect::init_length_and_dec()
{
  decimals= 0;
  max_length= 1;
  max_columns= engine->cols();
}


void Item_exists_subselect::fix_length_and_dec()
{
  DBUG_ENTER("Item_exists_subselect::fix_length_and_dec");
  init_length_and_dec();
  /*
    We need only 1 row to determine existence (i.e. any EXISTS that is not
    an IN always requires LIMIT 1)
  */
  thd->change_item_tree(&unit->global_parameters->select_limit,
                        new Item_int((int32) 1));
  DBUG_PRINT("info", ("Set limit to 1"));
  DBUG_VOID_RETURN;
}


void Item_in_subselect::fix_length_and_dec()
{
  DBUG_ENTER("Item_in_subselect::fix_length_and_dec");
  init_length_and_dec();
  /*
    Unlike Item_exists_subselect, LIMIT 1 is set later for
    Item_in_subselect, depending on the chosen strategy.
  */
  DBUG_VOID_RETURN;
}


/**
  Add an expression cache for this subquery if it is needed

  @param thd_arg         Thread handle

  @details
  The function checks whether an expression cache is needed for this item
  and if if so wraps the item into an item of the class
  Item_exp_cache_wrapper with an appropriate expression cache set up there.

  @note
  used from Item::transform()

  @return
  new wrapper item if an expression cache is needed,
  this item - otherwise
*/

Item* Item_exists_subselect::expr_cache_insert_transformer(uchar *thd_arg)
{
  THD *thd= (THD*) thd_arg;
  DBUG_ENTER("Item_exists_subselect::expr_cache_insert_transformer");

  if (expr_cache)
    DBUG_RETURN(expr_cache);

  if (substype() == EXISTS_SUBS && expr_cache_is_needed(thd) &&
      (expr_cache= set_expr_cache(thd)))
    DBUG_RETURN(expr_cache);
  DBUG_RETURN(this);
}


void Item_exists_subselect::no_rows_in_result()
{
  /*
    Subquery predicates outside of the SELECT list must be evaluated in order
    to possibly filter the special result row generated for implicit grouping
    if the subquery is in the HAVING clause.
    If the predicate is constant, we need its actual value in the only result
    row for queries with implicit grouping.
  */
  if (parsing_place != SELECT_LIST || const_item())
    return;
  value= 0;
  null_value= 0;
  make_const();
}

double Item_exists_subselect::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!forced_const && exec())
  {
    reset();
    return 0;
  }
  return (double) value;
}

longlong Item_exists_subselect::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (!forced_const && exec())
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
  if (!forced_const && exec())
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
  if (!forced_const && exec())
    reset();
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}


bool Item_exists_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  if (!forced_const && exec())
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
  if (forced_const)
    return value;
  DBUG_ASSERT((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN) ||
              ! engine->is_executed());
  null_value= was_null= FALSE;
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
  if (forced_const)
    return value;
  DBUG_ASSERT((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN) ||
              ! engine->is_executed());
  null_value= was_null= FALSE;
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
  if (forced_const)
    goto value_is_ready;
  DBUG_ASSERT((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN) ||
              ! engine->is_executed());
  null_value= was_null= FALSE;
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
value_is_ready:
  str->set((ulonglong)value, &my_charset_bin);
  return str;
}


bool Item_in_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  if (forced_const)
    return value;
  DBUG_ASSERT((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN) ||
              ! engine->is_executed());
  null_value= was_null= FALSE;
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
  if (forced_const)
    goto value_is_ready;
  DBUG_ASSERT((engine->uncacheable() & ~UNCACHEABLE_EXPLAIN) ||
              ! engine->is_executed());
  null_value= was_null= FALSE;
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  if (was_null && !value)
    null_value= TRUE;
value_is_ready:
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}


/**
  Prepare a single-column IN/ALL/ANY subselect for rewriting.

  @param join  Join object of the subquery (i.e. 'child' join).

  @details

  Prepare a single-column subquery to be rewritten. Given the subquery.

  If the subquery has no tables it will be turned to an expression between
  left part and SELECT list.

  In other cases the subquery will be wrapped with  Item_in_optimizer which
  allow later to turn it to EXISTS or MAX/MIN.

  @retval false  The subquery was transformed
  @retval true   Error
*/

bool
Item_in_subselect::single_value_transformer(JOIN *join)
{
  SELECT_LEX *select_lex= join->select_lex;
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  /*
    Check that the right part of the subselect contains no more than one
    column. E.g. in SELECT 1 IN (SELECT * ..) the right part is (SELECT * ...)
  */
  // psergey: duplicated_subselect_card_check
  if (select_lex->item_list.elements > 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
    DBUG_RETURN(true);
  }

  Item* join_having= join->having ? join->having : join->tmp_having;
  if (!(join_having || select_lex->with_sum_func ||
        select_lex->group_list.elements) &&
      select_lex->table_list.elements == 0 &&
      !select_lex->master_unit()->is_union())
  {
    Item *where_item= (Item*) select_lex->item_list.head();
    /*
      it is single select without tables => possible optimization
      remove the dependence mark since the item is moved to upper
      select and is not outer anymore.
    */
    where_item->walk(&Item::remove_dependence_processor, 0,
                     (uchar *) select_lex->outer_select());
    substitution= func->create(left_expr, where_item);
    have_to_be_excluded= 1;
    if (thd->lex->describe)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                   ER_SELECT_REDUCED, warn_buff);
    }
    DBUG_RETURN(false);
  }

  /*
    Wrap the current IN predicate in an Item_in_optimizer. The actual
    substitution in the Item tree takes place in Item_subselect::fix_fields.
  */
  if (!substitution)
  {
    /* We're invoked for the 1st (or the only) SELECT in the subquery UNION */
    substitution= optimizer;

    SELECT_LEX *current= thd->lex->current_select;

    thd->lex->current_select= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0))
    {
      thd->lex->current_select= current;
      DBUG_RETURN(true);
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
  }

  DBUG_RETURN(false);
}


/**
  Apply transformation max/min  transwormation to ALL/ANY subquery if it is
  possible.

  @param join  Join object of the subquery (i.e. 'child' join).

  @details

  If this is an ALL/ANY single-value subselect, try to rewrite it with
  a MIN/MAX subselect. We can do that if a possible NULL result of the
  subselect can be ignored.
  E.g. SELECT * FROM t1 WHERE b > ANY (SELECT a FROM t2) can be rewritten
  with SELECT * FROM t1 WHERE b > (SELECT MAX(a) FROM t2).
  We can't check that this optimization is safe if it's not a top-level
  item of the WHERE clause (e.g. because the WHERE clause can contain IS
  NULL/IS NOT NULL functions). If so, we rewrite ALL/ANY with NOT EXISTS
  later in this method.

  @retval false  The subquery was transformed
  @retval true   Error
*/

bool Item_allany_subselect::transform_into_max_min(JOIN *join)
{
  DBUG_ENTER("Item_allany_subselect::transform_into_max_min");
  if (!test_strategy(SUBS_MAXMIN_INJECTED | SUBS_MAXMIN_ENGINE))
    DBUG_RETURN(false);
  Item **place= optimizer->arguments() + 1;
  THD *thd= join->thd;
  SELECT_LEX *select_lex= join->select_lex;
  Item *subs;

  /*
  */
  DBUG_ASSERT(!substitution);

  /*
    Check if optimization with aggregate min/max possible
    1 There is no aggregate in the subquery
    2 It is not UNION
    3 There is tables
    4 It is not ALL subquery with possible NULLs in the SELECT list
  */
  if (!select_lex->group_list.elements &&                /*1*/
      !select_lex->having &&                             /*1*/
      !select_lex->with_sum_func &&                      /*1*/
      !(select_lex->next_select()) &&                    /*2*/
      select_lex->table_list.elements &&                 /*3*/
      (!select_lex->ref_pointer_array[0]->maybe_null ||  /*4*/
       substype() != Item_subselect::ALL_SUBS))          /*4*/
  {
    Item_sum_hybrid *item;
    nesting_map save_allow_sum_func;
    if (func->l_op())
    {
      /*
        (ALL && (> || =>)) || (ANY && (< || =<))
        for ALL condition is inverted
      */
      item= new Item_sum_max(*select_lex->ref_pointer_array);
    }
    else
    {
      /*
        (ALL && (< || =<)) || (ANY && (> || =>))
        for ALL condition is inverted
      */
      item= new Item_sum_min(*select_lex->ref_pointer_array);
    }
    if (upper_item)
      upper_item->set_sum_test(item);
    thd->change_item_tree(select_lex->ref_pointer_array, item);
    {
      List_iterator<Item> it(select_lex->item_list);
      it++;
      thd->change_item_tree(it.ref(), item);
    }

    DBUG_EXECUTE("where",
                 print_where(item, "rewrite with MIN/MAX", QT_ORDINARY););

    save_allow_sum_func= thd->lex->allow_sum_func;
    thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;
    /*
      Item_sum_(max|min) can't substitute other item => we can use 0 as
      reference, also Item_sum_(max|min) can't be fixed after creation, so
      we do not check item->fixed
    */
    if (item->fix_fields(thd, 0))
      DBUG_RETURN(true);
    thd->lex->allow_sum_func= save_allow_sum_func; 
    /* we added aggregate function => we have to change statistic */
    count_field_types(select_lex, &join->tmp_table_param, join->all_fields, 
                      0);
    if (join->prepare_stage2())
      DBUG_RETURN(true);
    subs= new Item_singlerow_subselect(select_lex);

    /*
      Remove other strategies if any (we already changed the query and
      can't apply other strategy).
    */
    set_strategy(SUBS_MAXMIN_INJECTED);
  }
  else
  {
    Item_maxmin_subselect *item;
    subs= item= new Item_maxmin_subselect(thd, this, select_lex, func->l_op());
    if (upper_item)
      upper_item->set_sub_test(item);
    /*
      Remove other strategies if any (we already changed the query and
      can't apply other strategy).
    */
    set_strategy(SUBS_MAXMIN_ENGINE);
  }
  /*
    The swap is needed for expressions of type 'f1 < ALL ( SELECT ....)'
    where we want to evaluate the sub query even if f1 would be null.
  */
  subs= func->create_swap(*(optimizer->get_cache()), subs);
  thd->change_item_tree(place, subs);
  if (subs->fix_fields(thd, &subs))
    DBUG_RETURN(true);
  DBUG_ASSERT(subs == (*place)); // There was no substitutions

  select_lex->master_unit()->uncacheable&= ~UNCACHEABLE_DEPENDENT_INJECTED;
  select_lex->uncacheable&= ~UNCACHEABLE_DEPENDENT_INJECTED;

  DBUG_RETURN(false);
}


bool Item_in_subselect::fix_having(Item *having, SELECT_LEX *select_lex)
{
  bool fix_res= 0;
  if (!having->fixed)
  {
    select_lex->having_fix_field= 1;
    fix_res= having->fix_fields(thd, 0);
    select_lex->having_fix_field= 0;
  }
  return fix_res;
}

bool Item_allany_subselect::is_maxmin_applicable(JOIN *join)
{
  /*
    Check if max/min optimization applicable: It is top item of
    WHERE condition.
  */
  return (abort_on_null || (upper_item && upper_item->is_top_level_item())) &&
      !join->select_lex->master_unit()->uncacheable && !func->eqne_op();
}


/**
  Create the predicates needed to transform a single-column IN/ALL/ANY
  subselect into a correlated EXISTS via predicate injection.

  @param join[in]  Join object of the subquery (i.e. 'child' join).
  @param where_item[out]   the in-to-exists addition to the where clause
  @param having_item[out]  the in-to-exists addition to the having clause

  @details
  The correlated predicates are created as follows:

  - If the subquery has aggregates, GROUP BY, or HAVING, convert to

    SELECT ie FROM ...  HAVING subq_having AND 
                               trigcond(oe $cmp$ ref_or_null_helper<ie>)
                                   
    the addition is wrapped into trigger only when we want to distinguish
    between NULL and FALSE results.

  - Otherwise (no aggregates/GROUP BY/HAVING) convert it to one of the
    following:

    = If we don't need to distinguish between NULL and FALSE subquery:
        
      SELECT ie FROM ... WHERE subq_where AND (oe $cmp$ ie)

    = If we need to distinguish between those:

      SELECT ie FROM ...
        WHERE  subq_where AND trigcond((oe $cmp$ ie) OR (ie IS NULL))
        HAVING trigcond(<is_not_null_test>(ie))

  @retval false If the new conditions were created successfully
  @retval true  Error
*/

bool
Item_in_subselect::create_single_in_to_exists_cond(JOIN * join,
                                                   Item **where_item,
                                                   Item **having_item)
{
  SELECT_LEX *select_lex= join->select_lex;
  /*
    The non-transformed HAVING clause of 'join' may be stored in two ways
    during JOIN::optimize: this->tmp_having= this->having; this->having= 0;
  */
  Item* join_having= join->having ? join->having : join->tmp_having;

  DBUG_ENTER("Item_in_subselect::create_single_in_to_exists_cond");

  *where_item= NULL;
  *having_item= NULL;

  if (join_having || select_lex->with_sum_func ||
      select_lex->group_list.elements)
  {
    Item *item= func->create(expr,
                             new Item_ref_null_helper(&select_lex->context,
                                                      this,
                                                      select_lex->
                                                      ref_pointer_array,
                                                      (char *)"<ref>",
                                                      this->full_name()));
    if (!abort_on_null && left_expr->maybe_null)
    {
      /* 
        We can encounter "NULL IN (SELECT ...)". Wrap the added condition
        within a trig_cond.
      */
      item= new Item_func_trig_cond(item, get_cond_guard(0));
    }

    if (!join_having)
      item->name= (char*) in_having_cond;
    if (fix_having(item, select_lex))
      DBUG_RETURN(true);
    *having_item= item;
  }
  else
  {
    Item *item= (Item*) select_lex->item_list.head()->real_item();

    if (select_lex->table_list.elements)
    {
      Item *having= item;
      Item *orig_item= item;
       
      item= func->create(expr, item);
      if (!abort_on_null && orig_item->maybe_null)
      {
	having= new Item_is_not_null_test(this, having);
        if (left_expr->maybe_null)
        {
          if (!(having= new Item_func_trig_cond(having,
                                                get_cond_guard(0))))
            DBUG_RETURN(true);
        }
        having->name= (char*) in_having_cond;
        if (fix_having(having, select_lex))
          DBUG_RETURN(true);
        *having_item= having;

	item= new Item_cond_or(item,
			       new Item_func_isnull(orig_item));
      }
      /* 
        If we may encounter NULL IN (SELECT ...) and care whether subquery
        result is NULL or FALSE, wrap condition in a trig_cond.
      */
      if (!abort_on_null && left_expr->maybe_null)
      {
        if (!(item= new Item_func_trig_cond(item, get_cond_guard(0))))
          DBUG_RETURN(true);
      }

      /*
        TODO: figure out why the following is done here in 
        single_value_transformer but there is no corresponding action in
        row_value_transformer?
      */
      item->name= (char *) in_additional_cond;
      if (!item->fixed && item->fix_fields(thd, 0))
        DBUG_RETURN(true);
      *where_item= item;
    }
    else
    {
      if (select_lex->master_unit()->is_union())
      {
        Item *new_having=
          func->create(expr,
                       new Item_ref_null_helper(&select_lex->context, this,
                                            select_lex->ref_pointer_array,
                                            (char *)"<no matter>",
                                            (char *)"<result>"));
        if (!abort_on_null && left_expr->maybe_null)
        {
          if (!(new_having= new Item_func_trig_cond(new_having,
                                                    get_cond_guard(0))))
            DBUG_RETURN(true);
        }

        new_having->name= (char*) in_having_cond;
        if (fix_having(new_having, select_lex))
          DBUG_RETURN(true);
        *having_item= new_having;
      }
      else
        DBUG_ASSERT(false);
    }
  }

  DBUG_RETURN(false);
}


/**
  Wrap a multi-column IN/ALL/ANY subselect into an Item_in_optimizer.

  @param join  Join object of the subquery (i.e. 'child' join).

  @details
  The subquery predicate is wrapped into an Item_in_optimizer. Later the query
  optimization phase chooses whether the subquery under the Item_in_optimizer
  will be further transformed into an equivalent correlated EXISTS by injecting
  additional predicates, or will be executed via subquery materialization in its
  unmodified form.

  @retval false  The subquery was transformed
  @retval true   Error
*/

bool
Item_in_subselect::row_value_transformer(JOIN *join)
{
  SELECT_LEX *select_lex= join->select_lex;
  uint cols_num= left_expr->cols();

  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  // psergey: duplicated_subselect_card_check
  if (select_lex->item_list.elements != cols_num)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), cols_num);
    DBUG_RETURN(true);
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

    SELECT_LEX *current= thd->lex->current_select;
    thd->lex->current_select= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0))
    {
      thd->lex->current_select= current;
      DBUG_RETURN(true);
    }

    // we will refer to upper level cache array => we have to save it in PS
    optimizer->keep_top_level_cache();

    thd->lex->current_select= current;
    /*
      The uncacheable property controls a number of actions, e.g. whether to
      save/restore (via init_save_join_tab/restore_tmp) the original JOIN for
      plans with a temp table where the original JOIN was overriden by
      make_simple_join. The UNCACHEABLE_EXPLAIN is ignored by EXPLAIN, thus
      non-correlated subqueries will not appear as such to EXPLAIN.
    */
    master_unit->uncacheable|= UNCACHEABLE_EXPLAIN;
    select_lex->uncacheable|= UNCACHEABLE_EXPLAIN;
  }

  DBUG_RETURN(false);
}


/**
  Create the predicates needed to transform a multi-column IN/ALL/ANY
  subselect into a correlated EXISTS via predicate injection.

  @details
  The correlated predicates are created as follows:

  - If the subquery has aggregates, GROUP BY, or HAVING, convert to

    (l1, l2, l3) IN (SELECT v1, v2, v3 ... HAVING having)
    =>
    EXISTS (SELECT ... HAVING having and
                              (l1 = v1 or is null v1) and
                              (l2 = v2 or is null v2) and
                              (l3 = v3 or is null v3) and
                              is_not_null_test(v1) and
                              is_not_null_test(v2) and
                              is_not_null_test(v3))

    where is_not_null_test used to register nulls in case if we have
    not found matching to return correct NULL value.

  - Otherwise (no aggregates/GROUP BY/HAVING) convert the subquery as follows:

    (l1, l2, l3) IN (SELECT v1, v2, v3 ... WHERE where)
    =>
    EXISTS (SELECT ... WHERE where and
                             (l1 = v1 or is null v1) and
                             (l2 = v2 or is null v2) and
                             (l3 = v3 or is null v3)
                       HAVING is_not_null_test(v1) and
                              is_not_null_test(v2) and
                              is_not_null_test(v3))
    where is_not_null_test registers NULLs values but reject rows.

    in case when we do not need correct NULL, we have simplier construction:
    EXISTS (SELECT ... WHERE where and
                             (l1 = v1) and
                             (l2 = v2) and
                             (l3 = v3)

  @param join[in]  Join object of the subquery (i.e. 'child' join).
  @param where_item[out]   the in-to-exists addition to the where clause
  @param having_item[out]  the in-to-exists addition to the having clause

  @retval false  If the new conditions were created successfully
  @retval true   Error
*/

bool
Item_in_subselect::create_row_in_to_exists_cond(JOIN * join,
                                                Item **where_item,
                                                Item **having_item)
{
  SELECT_LEX *select_lex= join->select_lex;
  uint cols_num= left_expr->cols();
  /*
    The non-transformed HAVING clause of 'join' may be stored in two ways
    during JOIN::optimize: this->tmp_having= this->having; this->having= 0;
  */
  Item* join_having= join->having ? join->having : join->tmp_having;
  bool is_having_used= (join_having || select_lex->with_sum_func ||
                        select_lex->group_list.first ||
                        !select_lex->table_list.elements);

  DBUG_ENTER("Item_in_subselect::create_row_in_to_exists_cond");

  *where_item= NULL;
  *having_item= NULL;

  if (is_having_used)
  {
    /* TODO: say here explicitly if the order of AND parts matters or not. */
    Item *item_having_part2= 0;
    for (uint i= 0; i < cols_num; i++)
    {
      DBUG_ASSERT((left_expr->fixed &&

                  select_lex->ref_pointer_array[i]->fixed) ||
                  (select_lex->ref_pointer_array[i]->type() == REF_ITEM &&
                   ((Item_ref*)(select_lex->ref_pointer_array[i]))->ref_type() ==
                    Item_ref::OUTER_REF));
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(true);
      Item *item_eq=
        new Item_func_eq(new
                         Item_ref(&select_lex->context,
                                  (*optimizer->get_cache())->
                                  addr(i),
                                  (char *)"<no matter>",
                                  (char *)in_left_expr_name),
                         new
                         Item_ref(&select_lex->context,
                                  select_lex->ref_pointer_array + i,
                                  (char *)"<no matter>",
                                  (char *)"<list ref>"));
      Item *item_isnull=
        new Item_func_isnull(new
                             Item_ref(&select_lex->context,
                                      select_lex->ref_pointer_array+i,
                                      (char *)"<no matter>",
                                      (char *)"<list ref>"));
      Item *col_item= new Item_cond_or(item_eq, item_isnull);
      if (!abort_on_null && left_expr->element_index(i)->maybe_null)
      {
        if (!(col_item= new Item_func_trig_cond(col_item, get_cond_guard(i))))
          DBUG_RETURN(true);
      }
      *having_item= and_items(*having_item, col_item);

      Item *item_nnull_test= 
         new Item_is_not_null_test(this,
                                   new Item_ref(&select_lex->context,
                                                select_lex->
                                                ref_pointer_array + i,
                                                (char *)"<no matter>",
                                                (char *)"<list ref>"));
      if (!abort_on_null && left_expr->element_index(i)->maybe_null)
      {
        if (!(item_nnull_test= 
              new Item_func_trig_cond(item_nnull_test, get_cond_guard(i))))
          DBUG_RETURN(true);
      }
      item_having_part2= and_items(item_having_part2, item_nnull_test);
      item_having_part2->top_level_item();
    }
    *having_item= and_items(*having_item, item_having_part2);
  }
  else
  {
    for (uint i= 0; i < cols_num; i++)
    {
      Item *item, *item_isnull;
      DBUG_ASSERT((left_expr->fixed &&
                  select_lex->ref_pointer_array[i]->fixed) ||
                  (select_lex->ref_pointer_array[i]->type() == REF_ITEM &&
                   ((Item_ref*)(select_lex->ref_pointer_array[i]))->ref_type() ==
                    Item_ref::OUTER_REF));
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(true);
      item=
        new Item_func_eq(new
                         Item_direct_ref(&select_lex->context,
                                         (*optimizer->get_cache())->
                                         addr(i),
                                         (char *)"<no matter>",
                                         (char *)in_left_expr_name),
                         new
                         Item_direct_ref(&select_lex->context,
                                         select_lex->
                                         ref_pointer_array+i,
                                         (char *)"<no matter>",
                                         (char *)"<list ref>"));
      if (!abort_on_null)
      {
        Item *having_col_item=
          new Item_is_not_null_test(this,
                                    new
                                    Item_ref(&select_lex->context, 
                                             select_lex->ref_pointer_array + i,
                                             (char *)"<no matter>",
                                             (char *)"<list ref>"));
        
        
        item_isnull= new
          Item_func_isnull(new
                           Item_direct_ref(&select_lex->context,
                                           select_lex->
                                           ref_pointer_array+i,
                                           (char *)"<no matter>",
                                           (char *)"<list ref>"));
        item= new Item_cond_or(item, item_isnull);
        /* 
          TODO: why we create the above for cases where the right part
                cant be NULL?
        */
        if (left_expr->element_index(i)->maybe_null)
        {
          if (!(item= new Item_func_trig_cond(item, get_cond_guard(i))))
            DBUG_RETURN(true);
          if (!(having_col_item= 
                  new Item_func_trig_cond(having_col_item, get_cond_guard(i))))
            DBUG_RETURN(true);
        }
        *having_item= and_items(*having_item, having_col_item);
      }
      *where_item= and_items(*where_item, item);
    }
  }

  if (*where_item)
  {
    if (!(*where_item)->fixed && (*where_item)->fix_fields(thd, 0))
      DBUG_RETURN(true);
    (*where_item)->top_level_item();
  }

  if (*having_item)
  {
    if (!join_having)
      (*having_item)->name= (char*) in_having_cond;
    if (fix_having(*having_item, select_lex))
      DBUG_RETURN(true);
    (*having_item)->top_level_item();
  }

  DBUG_RETURN(false);
}


bool
Item_in_subselect::select_transformer(JOIN *join)
{
  return select_in_like_transformer(join);
}


/**
  Create the predicates needed to transform an IN/ALL/ANY subselect into a
  correlated EXISTS via predicate injection.

  @param join_arg  Join object of the subquery.

  @retval FALSE  ok
  @retval TRUE   error
*/

bool Item_in_subselect::create_in_to_exists_cond(JOIN *join_arg)
{
  bool res;

  DBUG_ASSERT(engine->engine_type() == subselect_engine::SINGLE_SELECT_ENGINE ||
              engine->engine_type() == subselect_engine::UNION_ENGINE);
  /*
    TODO: the call to init_cond_guards allocates and initializes an
    array of booleans that may not be used later because we may choose
    materialization.
    The two calls below to create_XYZ_cond depend on this boolean array.
    If the dependency is removed, the call can be moved to a later phase.
  */
  init_cond_guards();
  if (left_expr->cols() == 1)
    res= create_single_in_to_exists_cond(join_arg,
                                         &(join_arg->in_to_exists_where),
                                         &(join_arg->in_to_exists_having));
  else
    res= create_row_in_to_exists_cond(join_arg,
                                      &(join_arg->in_to_exists_where),
                                      &(join_arg->in_to_exists_having));

  /*
    The IN=>EXISTS transformation makes non-correlated subqueries correlated.
  */
  if (!left_expr->const_item() || left_expr->is_expensive())
  {
    join_arg->select_lex->uncacheable|= UNCACHEABLE_DEPENDENT_INJECTED;
    join_arg->select_lex->master_unit()->uncacheable|= 
                                         UNCACHEABLE_DEPENDENT_INJECTED;
  }
  /*
    The uncacheable property controls a number of actions, e.g. whether to
    save/restore (via init_save_join_tab/restore_tmp) the original JOIN for
    plans with a temp table where the original JOIN was overriden by
    make_simple_join. The UNCACHEABLE_EXPLAIN is ignored by EXPLAIN, thus
    non-correlated subqueries will not appear as such to EXPLAIN.
  */
  join_arg->select_lex->master_unit()->uncacheable|= UNCACHEABLE_EXPLAIN;
  join_arg->select_lex->uncacheable|= UNCACHEABLE_EXPLAIN;
  return (res);
}


/**
  Transform an IN/ALL/ANY subselect into a correlated EXISTS via injecting
  correlated in-to-exists predicates.

  @param join_arg  Join object of the subquery.

  @retval FALSE  ok
  @retval TRUE   error
*/

bool Item_in_subselect::inject_in_to_exists_cond(JOIN *join_arg)
{
  SELECT_LEX *select_lex= join_arg->select_lex;
  Item *where_item= join_arg->in_to_exists_where;
  Item *having_item= join_arg->in_to_exists_having;

  DBUG_ENTER("Item_in_subselect::inject_in_to_exists_cond");

  if (where_item)
  {
    List<Item> *and_args= NULL;
    /*
      If the top-level Item of the WHERE clause is an AND, detach the multiple
      equality list that was attached to the end of the AND argument list by
      build_equal_items_for_cond(). The multiple equalities must be detached
      because fix_fields merges lower level AND arguments into the upper AND.
      As a result, the arguments from lower-level ANDs are concatenated after
      the multiple equalities. When the multiple equality list is treated as
      such, it turns out that it contains non-Item_equal object which is wrong.
    */
    if (join_arg->conds && join_arg->conds->type() == Item::COND_ITEM &&
        ((Item_cond*) join_arg->conds)->functype() == Item_func::COND_AND_FUNC)
    {
      and_args= ((Item_cond*) join_arg->conds)->argument_list();
      if (join_arg->cond_equal)
        and_args->disjoin((List<Item> *) &join_arg->cond_equal->current_level);
    }

    where_item= and_items(join_arg->conds, where_item);
    if (!where_item->fixed && where_item->fix_fields(thd, 0))
      DBUG_RETURN(true);
    // TIMOUR TODO: call optimize_cond() for the new where clause
    thd->change_item_tree(&select_lex->where, where_item);
    select_lex->where->top_level_item();
    join_arg->conds= select_lex->where;

    /* Attach back the list of multiple equalities to the new top-level AND. */
    if (and_args && join_arg->cond_equal)
    {
      /* The argument list of the top-level AND may change after fix fields. */
      and_args= ((Item_cond*) join_arg->conds)->argument_list();
      List_iterator<Item_equal> li(join_arg->cond_equal->current_level);
      Item_equal *elem;
      while ((elem= li++))
      {
        and_args->push_back(elem);
      }
    }
  }

  if (having_item)
  {
    Item* join_having= join_arg->having ? join_arg->having:join_arg->tmp_having;
    having_item= and_items(join_having, having_item);
    if (fix_having(having_item, select_lex))
      DBUG_RETURN(true);
    // TIMOUR TODO: call optimize_cond() for the new having clause
    thd->change_item_tree(&select_lex->having, having_item);
    select_lex->having->top_level_item();
    join_arg->having= select_lex->having;
  }
  join_arg->thd->change_item_tree(&unit->global_parameters->select_limit,
                                  new Item_int((int32) 1));
  unit->select_limit_cnt= 1;

  DBUG_RETURN(false);
}


/**
  Prepare IN/ALL/ANY/SOME subquery transformation and call the appropriate
  transformation function.

  @param join    JOIN object of transforming subquery

  @notes
  To decide which transformation procedure (scalar or row) applicable here
  we have to call fix_fields() for the left expression to be able to call
  cols() method on it. Also this method makes arena management for
  underlying transformation methods.

  @retval  false  OK
  @retval  true   Error
*/

bool
Item_in_subselect::select_in_like_transformer(JOIN *join)
{
  Query_arena *arena, backup;
  SELECT_LEX *current= thd->lex->current_select;
  const char *save_where= thd->where;
  bool trans_res= true;
  bool result;

  DBUG_ENTER("Item_in_subselect::select_in_like_transformer");

  /*
    IN/SOME/ALL/ANY subqueries aren't support LIMIT clause. Without it
    ORDER BY clause becomes meaningless thus we drop it here.
  */
  for (SELECT_LEX *sl= current->master_unit()->first_select();
       sl; sl= sl->next_select())
  {
    if (sl->join)
    {
      sl->join->order= 0;
      sl->join->skip_sort_order= 1;
    }
  }

  if (changed)
    DBUG_RETURN(false);

  thd->where= "IN/ALL/ANY subquery";

  /*
    In some optimisation cases we will not need this Item_in_optimizer
    object, but we can't know it here, but here we need address correct
    reference on left expresion.

    note: we won't need Item_in_optimizer when handling degenerate cases
    like "... IN (SELECT 1)"
  */
  if (!optimizer)
  {
    arena= thd->activate_stmt_arena_if_needed(&backup);
    result= (!(optimizer= new Item_in_optimizer(left_expr, this)));
    if (arena)
      thd->restore_active_arena(arena, &backup);
    if (result)
      goto err;
  }

  thd->lex->current_select= current->return_after_parsing();
  result= (!left_expr->fixed &&
           left_expr->fix_fields(thd, optimizer->arguments()));
  /* fix_fields can change reference to left_expr, we need reassign it */
  left_expr= optimizer->arguments()[0];

  thd->lex->current_select= current;
  if (result)
    goto err;

  /*
    Both transformers call fix_fields() only for Items created inside them,
    and all that items do not make permanent changes in current item arena
    which allow to us call them with changed arena (if we do not know nature
    of Item, we have to call fix_fields() for it only with original arena to
    avoid memory leack)
  */
  arena= thd->activate_stmt_arena_if_needed(&backup);
  if (left_expr->cols() == 1)
    trans_res= single_value_transformer(join);
  else
  {
    /* we do not support row operation for ALL/ANY/SOME */
    if (func != &eq_creator)
    {
      if (arena)
        thd->restore_active_arena(arena, &backup);
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
      DBUG_RETURN(true);
    }
    trans_res= row_value_transformer(join);
  }
  if (arena)
    thd->restore_active_arena(arena, &backup);
err:
  thd->where= save_where;
  DBUG_RETURN(trans_res);
}


void Item_in_subselect::print(String *str, enum_query_type query_type)
{
  if (test_strategy(SUBS_IN_TO_EXISTS))
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
  uint outer_cols_num;
  List<Item> *inner_cols;

  if (test_strategy(SUBS_SEMI_JOIN))
    return !( (*ref)= new Item_int(1));

  /*
    Check if the outer and inner IN operands match in those cases when we
    will not perform IN=>EXISTS transformation. Currently this is when we
    use subquery materialization.

    The condition below is true when this method was called recursively from
    inside JOIN::prepare for the JOIN object created by the call chain
    Item_subselect::fix_fields -> subselect_single_select_engine::prepare,
    which creates a JOIN object for the subquery and calls JOIN::prepare for
    the JOIN of the subquery.
    Notice that in some cases, this doesn't happen, and the check_cols()
    test for each Item happens later in
    Item_in_subselect::row_value_in_to_exists_transformer.
    The reason for this mess is that our JOIN::prepare phase works top-down
    instead of bottom-up, so we first do name resoluton and semantic checks
    for the outer selects, then for the inner.
  */
  if (engine &&
      engine->engine_type() == subselect_engine::SINGLE_SELECT_ENGINE &&
      ((subselect_single_select_engine*)engine)->join)
  {
    outer_cols_num= left_expr->cols();

    if (unit->is_union())
      inner_cols= &(unit->types);
    else
      inner_cols= &(unit->first_select()->item_list);
    if (outer_cols_num != inner_cols->elements)
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), outer_cols_num);
      return TRUE;
    }
    if (outer_cols_num > 1)
    {
      List_iterator<Item> inner_col_it(*inner_cols);
      Item *inner_col;
      for (uint i= 0; i < outer_cols_num; i++)
      {
        inner_col= inner_col_it++;
        if (inner_col->check_cols(left_expr->element_index(i)->cols()))
          return TRUE;
      }
    }
  }

  if ((thd_arg->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      left_expr && !left_expr->fixed &&
      left_expr->fix_fields(thd_arg, &left_expr))
    return TRUE;
  else
  if (Item_subselect::fix_fields(thd_arg, ref))
    return TRUE;
  fixed= TRUE;
  return FALSE;
}


void Item_in_subselect::fix_after_pullout(st_select_lex *new_parent, Item **ref)
{
  left_expr->fix_after_pullout(new_parent, &left_expr);
  Item_subselect::fix_after_pullout(new_parent, ref);
  used_tables_cache |= left_expr->used_tables();
}

void Item_in_subselect::update_used_tables()
{
  Item_subselect::update_used_tables();
  left_expr->update_used_tables();
  //used_tables_cache |= left_expr->used_tables();
  used_tables_cache= Item_subselect::used_tables() | left_expr->used_tables();
}


/**
  Try to create and initialize an engine to compute a subselect via
  materialization.

  @details
  The method creates a new engine for materialized execution, and initializes
  the engine. The initialization may fail
  - either because it wasn't possible to create the needed temporary table
    and its index,
  - or because of a memory allocation error,

  @returns
    @retval TRUE  memory allocation error occurred
    @retval FALSE an execution method was chosen successfully
*/

bool Item_in_subselect::setup_mat_engine()
{
  subselect_hash_sj_engine       *mat_engine= NULL;
  subselect_single_select_engine *select_engine;

  DBUG_ENTER("Item_in_subselect::setup_mat_engine");

  /*
    The select_engine (that executes transformed IN=>EXISTS subselects) is
    pre-created at parse time, and is stored in statment memory (preserved
    across PS executions).
  */
  DBUG_ASSERT(engine->engine_type() == subselect_engine::SINGLE_SELECT_ENGINE);
  select_engine= (subselect_single_select_engine*) engine;

  /* Create/initialize execution objects. */
  if (!(mat_engine= new subselect_hash_sj_engine(thd, this, select_engine)))
    DBUG_RETURN(TRUE);

  if (mat_engine->init(&select_engine->join->fields_list,
                       engine->get_identifier()))
    DBUG_RETURN(TRUE);

  engine= mat_engine;
  DBUG_RETURN(FALSE);
}


/**
  Initialize the cache of the left operand of the IN predicate.

  @note This method has the same purpose as alloc_group_fields(),
  but it takes a different kind of collection of items, and the
  list we push to is dynamically allocated.

  @retval TRUE  if a memory allocation error occurred or the cache is
                not applicable to the current query
  @retval FALSE if success
*/

bool Item_in_subselect::init_left_expr_cache()
{
  JOIN *outer_join;

  outer_join= unit->outer_select()->join;
  /*
    An IN predicate might be evaluated in a query for which all tables have
    been optimzied away.
  */ 
  if (!outer_join || !outer_join->table_count || !outer_join->tables_list)
    return TRUE;

  if (!(left_expr_cache= new List<Cached_item>))
    return TRUE;

  for (uint i= 0; i < left_expr->cols(); i++)
  {
    Cached_item *cur_item_cache= new_Cached_item(thd,
                                                 left_expr->element_index(i),
                                                 FALSE);
    if (!cur_item_cache || left_expr_cache->push_front(cur_item_cache))
      return TRUE;
  }
  return FALSE;
}


bool Item_in_subselect::init_cond_guards()
{
  uint cols_num= left_expr->cols();
  if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards)
  {
    if (!(pushed_cond_guards= (bool*)thd->alloc(sizeof(bool) * cols_num)))
        return TRUE;
    for (uint i= 0; i < cols_num; i++)
      pushed_cond_guards[i]= TRUE;
  }
  return FALSE;
}


bool
Item_allany_subselect::select_transformer(JOIN *join)
{
  DBUG_ENTER("Item_allany_subselect::select_transformer");
  DBUG_ASSERT((in_strategy & ~(SUBS_MAXMIN_INJECTED | SUBS_MAXMIN_ENGINE |
                               SUBS_IN_TO_EXISTS | SUBS_STRATEGY_CHOSEN)) == 0);
  if (upper_item)
    upper_item->show= 1;
  DBUG_RETURN(select_in_like_transformer(join));
}


void Item_allany_subselect::print(String *str, enum_query_type query_type)
{
  if (test_strategy(SUBS_IN_TO_EXISTS))
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


void Item_allany_subselect::no_rows_in_result()
{
  /*
    Subquery predicates outside of the SELECT list must be evaluated in order
    to possibly filter the special result row generated for implicit grouping
    if the subquery is in the HAVING clause.
    If the predicate is constant, we need its actual value in the only result
    row for queries with implicit grouping.
  */
  if (parsing_place != SELECT_LIST || const_item())
    return;
  value= 0;
  null_value= 0;
  was_null= 0;
  make_const();
}


void subselect_engine::set_thd(THD *thd_arg)
{
  thd= thd_arg;
  if (result)
    result->set_thd(thd_arg);
}


subselect_single_select_engine::
subselect_single_select_engine(THD *thd_arg, st_select_lex *select,
			       select_result_interceptor *result_arg,
			       Item_subselect *item_arg)
  :subselect_engine(thd_arg, item_arg, result_arg),
   prepared(0), executed(0), optimize_error(0),
   select_lex(select), join(0)
{
  select_lex->master_unit()->item= item_arg;
}

int subselect_single_select_engine::get_identifier()
{
  return select_lex->select_number; 
}

void subselect_single_select_engine::cleanup()
{
  DBUG_ENTER("subselect_single_select_engine::cleanup");
  prepared= executed= optimize_error= 0;
  join= 0;
  result->cleanup();
  select_lex->uncacheable&= ~UNCACHEABLE_DEPENDENT_INJECTED;
  DBUG_VOID_RETURN;
}


void subselect_union_engine::cleanup()
{
  DBUG_ENTER("subselect_union_engine::cleanup");
  unit->reinit_exec_mechanism();
  result->cleanup();
  unit->uncacheable&= ~UNCACHEABLE_DEPENDENT_INJECTED;
  for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
    sl->uncacheable&= ~UNCACHEABLE_DEPENDENT_INJECTED;
  DBUG_VOID_RETURN;
}


bool subselect_union_engine::is_executed() const
{
  return unit->executed;
}


/*
  Check if last execution of the subquery engine produced any rows

  SYNOPSIS
    subselect_union_engine::no_rows()

  DESCRIPTION
    Check if last execution of the subquery engine produced any rows. The
    return value is undefined if last execution ended in an error.

  RETURN
    TRUE  - Last subselect execution has produced no rows
    FALSE - Otherwise
*/

bool subselect_union_engine::no_rows()
{
  /* Check if we got any rows when reading UNION result from temp. table: */
  return test(!unit->fake_select_lex->join->send_records);
}


void subselect_uniquesubquery_engine::cleanup()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::cleanup");
  DBUG_VOID_RETURN;
}


subselect_union_engine::subselect_union_engine(THD *thd_arg, st_select_lex_unit *u,
					       select_result_interceptor *result_arg,
					       Item_subselect *item_arg)
  :subselect_engine(thd_arg, item_arg, result_arg)
{
  unit= u;
  if (!result_arg)				//out of memory
    current_thd->fatal_error();
  unit->item= item_arg;
}


/**
  Create and prepare the JOIN object that represents the query execution
  plan for the subquery.

  @details
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

int subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
  if (select_lex->join)
  {
    select_lex->cleanup();
  }
  join= new JOIN(thd, select_lex->item_list,
		 select_lex->options | SELECT_NO_UNLOCK, result);
  if (!join || !result)
  {
    thd->fatal_error();				//out of memory
    return 1;
  }
  prepared= 1;
  SELECT_LEX *save_select= thd->lex->current_select;
  thd->lex->current_select= select_lex;
  if (join->prepare(&select_lex->ref_pointer_array,
		    select_lex->table_list.first,
		    select_lex->with_wild,
		    select_lex->where,
		    select_lex->order_list.elements +
		    select_lex->group_list.elements,
		    select_lex->order_list.first,
		    select_lex->group_list.first,
		    select_lex->having,
		    NULL, select_lex,
		    select_lex->master_unit()))
    return 1;
  thd->lex->current_select= save_select;
  return 0;
}

int subselect_union_engine::prepare()
{
  return unit->prepare(thd, result, SELECT_NO_UNLOCK);
}

int subselect_uniquesubquery_engine::prepare()
{
  /* Should never be called. */
  DBUG_ASSERT(FALSE);
  return 1;
}


/*
  Check if last execution of the subquery engine produced any rows

  SYNOPSIS
    subselect_single_select_engine::no_rows()

  DESCRIPTION
    Check if last execution of the subquery engine produced any rows. The
    return value is undefined if last execution ended in an error.

  RETURN
    TRUE  - Last subselect execution has produced no rows
    FALSE - Otherwise
*/

bool subselect_single_select_engine::no_rows()
{ 
  return !item->assigned();
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
    if (!(row[i]= Item_cache::get_cache(sel_item, sel_item->cmp_type())))
      return;
    row[i]->setup(sel_item);
 //psergey-backport-timours:   row[i]->store(sel_item);
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

void subselect_uniquesubquery_engine::fix_length_and_dec(Item_cache **row)
{
  //this never should be called
  DBUG_ASSERT(0);
}

int  init_read_record_seq(JOIN_TAB *tab);
int join_read_always_key_or_null(JOIN_TAB *tab);
int join_read_next_same_or_null(READ_RECORD *info);

int subselect_single_select_engine::exec()
{
  DBUG_ENTER("subselect_single_select_engine::exec");

  if (optimize_error)
    DBUG_RETURN(1);

  char const *save_where= thd->where;
  SELECT_LEX *save_select= thd->lex->current_select;
  thd->lex->current_select= select_lex;

  if (!join->optimized)
  {
    SELECT_LEX_UNIT *unit= select_lex->master_unit();

    unit->set_limit(unit->global_parameters);
    if (join->optimize())
    {
      thd->where= save_where;
      executed= optimize_error= 1;
      thd->lex->current_select= save_select;
      DBUG_RETURN(join->error ? join->error : 1);
    }
    if (!select_lex->uncacheable && thd->lex->describe && 
        !(join->select_options & SELECT_DESCRIBE))
    {
      item->update_used_tables();
      if (item->const_item())
      {
        /*
          It's necessary to keep original JOIN table because
          create_sort_index() function may overwrite original
          JOIN_TAB::type and wrong optimization method can be
          selected on re-execution.
        */
        select_lex->uncacheable|= UNCACHEABLE_EXPLAIN;
        select_lex->master_unit()->uncacheable|= UNCACHEABLE_EXPLAIN;
        /*
          Force join->join_tmp creation, because this subquery will be replaced
          by a simple select from the materialization temp table by optimize()
          called by EXPLAIN and we need to preserve the initial query structure
          so we can display it.
        */
        if (join->need_tmp && join->init_save_join_tab())
          DBUG_RETURN(1);                        /* purecov: inspected */
      }
    }
    if (item->engine_changed)
    {
      DBUG_RETURN(1);
    }
  }
  if (select_lex->uncacheable &&
      select_lex->uncacheable != UNCACHEABLE_EXPLAIN
      && executed)
  {
    if (join->reinit())
    {
      thd->where= save_where;
      thd->lex->current_select= save_select;
      DBUG_RETURN(1);
    }
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
      for (JOIN_TAB *tab= first_linear_tab(join, WITHOUT_CONST_TABLES); tab;
           tab= next_linear_tab(join, tab, WITH_BUSH_ROOTS))
      {
        if (tab && tab->keyuse)
        {
          for (uint i= 0; i < tab->ref.key_parts; i++)
          {
            bool *cond_guard= tab->ref.cond_guards[i];
            if (cond_guard && !*cond_guard)
            {
              /* Change the access method to full table scan */
              tab->save_read_first_record= tab->read_first_record;
              tab->save_read_record= tab->read_record.read_record;
              tab->read_first_record= init_read_record_seq;
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
    }
    executed= 1;
    if (!(uncacheable() & ~UNCACHEABLE_EXPLAIN))
      item->make_const();
    thd->where= save_where;
    thd->lex->current_select= save_select;
    DBUG_RETURN(join->error || thd->is_fatal_error || thd->is_error());
  }
  thd->where= save_where;
  thd->lex->current_select= save_select;
  DBUG_RETURN(0);
}

int subselect_union_engine::exec()
{
  char const *save_where= thd->where;
  int res= unit->exec();
  thd->where= save_where;
  return res;
}


/*
  Search for at least one row satisfying select condition
 
  SYNOPSIS
    subselect_uniquesubquery_engine::scan_table()

  DESCRIPTION
    Scan the table using sequential access until we find at least one row
    satisfying select condition.
    
    The caller must set this->empty_result_set=FALSE before calling this
    function. This function will set it to TRUE if it finds a matching row.

  RETURN
    FALSE - OK
    TRUE  - Error
*/

int subselect_uniquesubquery_engine::scan_table()
{
  int error;
  TABLE *table= tab->table;
  DBUG_ENTER("subselect_uniquesubquery_engine::scan_table");

  if (table->file->inited)
    table->file->ha_index_end();
 
  if (table->file->ha_rnd_init_with_error(1))
    DBUG_RETURN(1);
  table->file->extra_opt(HA_EXTRA_CACHE,
                         current_thd->variables.read_buff_size);
  table->null_row= 0;
  for (;;)
  {
    error=table->file->ha_rnd_next(table->record[0]);
    if (error) {
      if (error == HA_ERR_RECORD_DELETED)
      {
        error= 0;
        continue;
      }
      if (error == HA_ERR_END_OF_FILE)
      {
        error= 0;
        break;
      }
      else
      {
        error= report_error(table, error);
        break;
      }
    }

    if (!cond || cond->val_int())
    {
      empty_result_set= FALSE;
      break;
    }
  }

  table->file->ha_rnd_end();
  DBUG_RETURN(error != 0);
}


/**
  Copy ref key for index access into the only subquery table.

  @details
    Copy ref key and check for conversion problems.
    If there is an error converting the left IN operand to the column type of
    the right IN operand count it as no match. In this case IN has the value of
    FALSE. We mark the subquery table cursor as having no more rows (to ensure
    that the processing that follows will not find a match) and return FALSE,
    so IN is not treated as returning NULL.

  @returns
    @retval FALSE The outer ref was copied into an index lookup key.
    @retval TRUE  The outer ref cannot possibly match any row, IN is FALSE.
*/

bool subselect_uniquesubquery_engine::copy_ref_key(bool skip_constants)
{
  DBUG_ENTER("subselect_uniquesubquery_engine::copy_ref_key");

  for (store_key **copy= tab->ref.key_copy ; *copy ; copy++)
  {
    enum store_key::store_key_result store_res;
    if (skip_constants && (*copy)->store_key_is_const())
      continue;
    store_res= (*copy)->copy();
    tab->ref.key_err= store_res;

    if (store_res == store_key::STORE_KEY_FATAL)
    {
      /*
       Error converting the left IN operand to the column type of the right
       IN operand. 
      */
      DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(false);
}


/**
  Execute subselect via unique index lookup

  @details
    Find rows corresponding to the ref key using index access.
    If some part of the lookup key is NULL, then we're evaluating
      NULL IN (SELECT ... )
    This is a special case, we don't need to search for NULL in the table,
    instead, the result value is 
      - NULL  if select produces empty row set
      - FALSE otherwise.

    In some cases (IN subselect is a top level item, i.e. abort_on_null==TRUE)
    the caller doesn't distinguish between NULL and FALSE result and we just
    return FALSE. 
    Otherwise we make a full table scan to see if there is at least one 
    matching row.
    
    The result of this function (info about whether a row was found) is
    stored in this->empty_result_set.
    
  @returns
    @retval 0  OK
    @retval 1  notify caller to call Item_subselect::reset(),
               in most cases reset() sets the result to NULL
*/

int subselect_uniquesubquery_engine::exec()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::exec");
  int error;
  TABLE *table= tab->table;
  empty_result_set= TRUE;
  table->status= 0;
  Item_in_subselect *in_subs= (Item_in_subselect *) item;

  if (!tab->preread_init_done && tab->preread_init())
    DBUG_RETURN(1);
 
  if (in_subs->left_expr_has_null())
  {
    /*
      The case when all values in left_expr are NULL is handled by
      Item_in_optimizer::val_int().
    */
    if (in_subs->is_top_level_item())
      DBUG_RETURN(1); /* notify caller to call reset() and set NULL value. */
    else
      DBUG_RETURN(scan_table());
  }

  if (copy_ref_key(true))
  {
    /* We know that there will be no rows even if we scan. */
    in_subs->value= 0;
    DBUG_RETURN(0);
  }

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 0);
  error= table->file->ha_index_read_map(table->record[0],
                                        tab->ref.key_buff,
                                        make_prev_keypart_map(tab->
                                                              ref.key_parts),
                                        HA_READ_KEY_EXACT);
  if (error &&
      error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    error= report_error(table, error);
  else
  {
    error= 0;
    table->null_row= 0;
    if (!table->status && (!cond || cond->val_int()))
    {
      ((Item_in_subselect *) item)->value= 1;
      empty_result_set= FALSE;
    }
    else
      ((Item_in_subselect *) item)->value= 0;
  }

  DBUG_RETURN(error != 0);
}


/*
  TIMOUR: write comment
*/

int subselect_uniquesubquery_engine::index_lookup()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::index_lookup");
  int error;
  TABLE *table= tab->table;
 
  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 0);
  error= table->file->ha_index_read_map(table->record[0],
                                        tab->ref.key_buff,
                                        make_prev_keypart_map(tab->
                                                              ref.key_parts),
                                        HA_READ_KEY_EXACT);
  DBUG_PRINT("info", ("lookup result: %i", error));

  if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
  {
    /*
      TIMOUR: I don't understand at all when do we need to call report_error.
      In most places where we access an index, we don't do this. Why here?
    */
    error= report_error(table, error);
    DBUG_RETURN(error);
  }

  table->null_row= 0;
  if (!error && (!cond || cond->val_int()))
    ((Item_in_subselect *) item)->value= 1;
  else
    ((Item_in_subselect *) item)->value= 0;

  DBUG_RETURN(0);
}



subselect_uniquesubquery_engine::~subselect_uniquesubquery_engine()
{
  /* Tell handler we don't need the index anymore */
  //psergey-merge-todo: the following was gone in 6.0:
 //psergey-merge: don't need this after all: tab->table->file->ha_index_end();
}


/**
  Execute subselect via unique index lookup

  @details
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

  @todo
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

  @returns
    @retval 0  OK
    @retval 1  notify caller to call Item_subselect::reset(),
               in most cases reset() sets the result to NULL
*/

int subselect_indexsubquery_engine::exec()
{
  DBUG_ENTER("subselect_indexsubquery_engine");
  int error;
  bool null_finding= 0;
  TABLE *table= tab->table;
  Item_in_subselect *in_subs= (Item_in_subselect *) item;

  ((Item_in_subselect *) item)->value= 0;
  empty_result_set= TRUE;
  table->status= 0;

  if (check_null)
  {
    /* We need to check for NULL if there wasn't a matching value */
    *tab->ref.null_ref_key= 0;			// Search first for not null
    ((Item_in_subselect *) item)->was_null= 0;
  }

  if (!tab->preread_init_done && tab->preread_init())
    DBUG_RETURN(1);

  if (in_subs->left_expr_has_null())
  {
    /*
      The case when all values in left_expr are NULL is handled by
      Item_in_optimizer::val_int().
    */
    if (in_subs->is_top_level_item())
      DBUG_RETURN(1); /* notify caller to call reset() and set NULL value. */
    else
      DBUG_RETURN(scan_table());
  }

  if (copy_ref_key(true))
  {
    /* We know that there will be no rows even if we scan. */
    in_subs->value= 0;
    DBUG_RETURN(0);
  }

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 1);
  error= table->file->ha_index_read_map(table->record[0],
                                        tab->ref.key_buff,
                                        make_prev_keypart_map(tab->
                                                              ref.key_parts),
                                        HA_READ_KEY_EXACT);
  if (error &&
      error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    error= report_error(table, error);
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
          empty_result_set= FALSE;
          if (null_finding)
            ((Item_in_subselect *) item)->was_null= 1;
          else
            ((Item_in_subselect *) item)->value= 1;
          break;
        }
        error= table->file->ha_index_next_same(table->record[0],
                                               tab->ref.key_buff,
                                               tab->ref.key_length);
        if (error && error != HA_ERR_END_OF_FILE)
        {
          error= report_error(table, error);
          break;
        }
      }
      else
      {
        if (!check_null || null_finding)
          break;			/* We don't need to check nulls */
        *tab->ref.null_ref_key= 1;
        null_finding= 1;
        /* Check if there exists a row with a null value in the index */
        if ((error= (safe_index_read(tab) == 1)))
          break;
      }
    }
  }
  DBUG_RETURN(error != 0);
}


uint subselect_single_select_engine::cols()
{
  //psergey-sj-backport: the following assert was gone in 6.0:
  //DBUG_ASSERT(select_lex->join != 0); // should be called after fix_fields()
  //return select_lex->join->fields_list.elements;
  return select_lex->item_list.elements;
}


uint subselect_union_engine::cols()
{
  DBUG_ASSERT(unit->is_prepared());  // should be called after fix_fields()
  return unit->types.elements;
}


uint8 subselect_single_select_engine::uncacheable()
{
  return select_lex->uncacheable;
}


uint8 subselect_union_engine::uncacheable()
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


void subselect_uniquesubquery_engine::exclude()
{
  //this never should be called
  DBUG_ASSERT(0);
}


table_map subselect_engine::calc_const_tables(List<TABLE_LIST> &list)
{
  table_map map= 0;
  List_iterator<TABLE_LIST> ti(list);
  TABLE_LIST *table;
  //for (; table; table= table->next_leaf)
  while ((table= ti++))
  {
    TABLE *tbl= table->table;
    if (tbl && tbl->const_table)
      map|= tbl->map;
  }
  return map;
}


table_map subselect_single_select_engine::upper_select_const_tables()
{
  return calc_const_tables(select_lex->outer_select()->leaf_tables);
}


table_map subselect_union_engine::upper_select_const_tables()
{
  return calc_const_tables(unit->outer_select()->leaf_tables);
}


void subselect_single_select_engine::print(String *str,
                                           enum_query_type query_type)
{
  select_lex->print(thd, str, query_type);
}


void subselect_union_engine::print(String *str, enum_query_type query_type)
{
  unit->print(str, query_type);
}


void subselect_uniquesubquery_engine::print(String *str,
                                            enum_query_type query_type)
{
  char *table_name= tab->table->s->table_name.str;
  str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  tab->ref.items[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" in "));
  if (tab->table->s->table_category == TABLE_CATEGORY_TEMPORARY)
  {
    /*
      Temporary tables' names change across runs, so they can't be used for
      EXPLAIN EXTENDED.
    */
    str->append(STRING_WITH_LEN("<temporary table>"));
  }
  else
    str->append(table_name, tab->table->s->table_name.length);
  KEY *key_info= tab->table->key_info+ tab->ref.key;
  str->append(STRING_WITH_LEN(" on "));
  str->append(key_info->name);
  if (cond)
  {
    str->append(STRING_WITH_LEN(" where "));
    cond->print(str, query_type);
  }
  str->append(')');
}

/*
TODO:
The above ::print method should be changed as below. Do it after
all other tests pass.

void subselect_uniquesubquery_engine::print(String *str)
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
  str->append(STRING_WITH_LEN("<index_lookup>("));
  tab->ref.items[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" in "));
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
  @param temp           temporary assignment

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool
subselect_single_select_engine::change_result(Item_subselect *si,
                                              select_result_interceptor *res,
                                              bool temp)
{
  item= si;
  if (temp)
  {
    /*
      Here we reuse change_item_tree to roll back assignment.  It has
      nothing special about Item* pointer so it is safe conversion. We do
      not change the interface to be compatible with MySQL.
    */
    thd->change_item_tree((Item**) &result, (Item*)res);
  }
  else
    result= res;

  /*
    We can't use 'result' below as gcc 4.2.4's alias optimization
    assumes that result was not changed by thd->change_item_tree().
    I tried to find a solution to make gcc happy, but could not find anything
    that would not require a lot of extra code that would be harder to manage
    than the current code.
  */
  return select_lex->join->change_result(res);
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
                                           select_result_interceptor *res,
                                           bool temp)
{
  item= si;
  int rc= unit->change_result(res, result);
  if (temp)
    thd->change_item_tree((Item**) &result, (Item*)res);
  else
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

bool
subselect_uniquesubquery_engine::change_result(Item_subselect *si,
                                               select_result_interceptor *res,
                                               bool temp
                                               __attribute__((unused)))
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
bool subselect_single_select_engine::no_tables()
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
bool subselect_single_select_engine::may_be_null()
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
bool subselect_union_engine::no_tables()
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

bool subselect_uniquesubquery_engine::no_tables()
{
  /* returning value is correct, but this method should never be called */
  DBUG_ASSERT(FALSE);
  return 0;
}


/******************************************************************************
  WL#1110 - Implementation of class subselect_hash_sj_engine
******************************************************************************/


/**
  Check if an IN predicate should be executed via partial matching using
  only schema information.

  @details
  This test essentially has three results:
  - partial matching is applicable, but cannot be executed due to a
    limitation in the total number of indexes, as a result we can't
    use subquery materialization at all.
  - partial matching is either applicable or not, and this can be
    determined by looking at 'this->max_keys'.
  If max_keys > 1, then we need partial matching because there are
  more indexes than just the one we use during materialization to
  remove duplicates.

  @note
  TIMOUR: The schema-based analysis for partial matching can be done once for
  prepared statement and remembered. It is done here to remove the need to
  save/restore all related variables between each re-execution, thus making
  the code simpler.

  @retval PARTIAL_MATCH  if a partial match should be used
  @retval COMPLETE_MATCH if a complete match (index lookup) should be used
*/

subselect_hash_sj_engine::exec_strategy
subselect_hash_sj_engine::get_strategy_using_schema()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;

  if (item_in->is_top_level_item())
    return COMPLETE_MATCH;
  else
  {
    List_iterator<Item> inner_col_it(*item_in->unit->get_unit_column_types());
    Item *outer_col, *inner_col;

    for (uint i= 0; i < item_in->left_expr->cols(); i++)
    {
      outer_col= item_in->left_expr->element_index(i);
      inner_col= inner_col_it++;

      if (!inner_col->maybe_null && !outer_col->maybe_null)
        bitmap_set_bit(&non_null_key_parts, i);
      else
      {
        bitmap_set_bit(&partial_match_key_parts, i);
        ++count_partial_match_columns;
      }
    }
  }

  /* If no column contains NULLs use regular hash index lookups. */
  if (count_partial_match_columns)
    return PARTIAL_MATCH;
  return COMPLETE_MATCH;
}


/**
  Test whether an IN predicate must be computed via partial matching
  based on the NULL statistics for each column of a materialized subquery.

  @details The procedure analyzes column NULL statistics, updates the
  matching type of columns that cannot be NULL or that contain only NULLs.
  Based on this, the procedure determines the final execution strategy for
  the [NOT] IN predicate.

  @retval PARTIAL_MATCH  if a partial match should be used
  @retval COMPLETE_MATCH if a complete match (index lookup) should be used
*/

subselect_hash_sj_engine::exec_strategy
subselect_hash_sj_engine::get_strategy_using_data()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;
  select_materialize_with_stats *result_sink=
    (select_materialize_with_stats *) result;
  Item *outer_col;

  /*
    If we already determined that a complete match is enough based on schema
    information, nothing can be better.
  */
  if (strategy == COMPLETE_MATCH)
    return COMPLETE_MATCH;

  for (uint i= 0; i < item_in->left_expr->cols(); i++)
  {
    if (!bitmap_is_set(&partial_match_key_parts, i))
      continue;
    outer_col= item_in->left_expr->element_index(i);
    /*
      If column 'i' doesn't contain NULLs, and the corresponding outer reference
      cannot have a NULL value, then 'i' is a non-nullable column.
    */
    if (result_sink->get_null_count_of_col(i) == 0 && !outer_col->maybe_null)
    {
      bitmap_clear_bit(&partial_match_key_parts, i);
      bitmap_set_bit(&non_null_key_parts, i);
      --count_partial_match_columns;
    }
    if (result_sink->get_null_count_of_col(i) == tmp_table->file->stats.records)
      ++count_null_only_columns;
    if (result_sink->get_null_count_of_col(i))
      ++count_columns_with_nulls;
  }

  /* If no column contains NULLs use regular hash index lookups. */
  if (!count_partial_match_columns)
    return COMPLETE_MATCH;
  return PARTIAL_MATCH;
}


void
subselect_hash_sj_engine::choose_partial_match_strategy(
  bool has_non_null_key, bool has_covering_null_row,
  MY_BITMAP *partial_match_key_parts)
{
  ulonglong pm_buff_size;

  DBUG_ASSERT(strategy == PARTIAL_MATCH);
  /*
    Choose according to global optimizer switch. If only one of the switches is
    'ON', then the remaining strategy is the only possible one. The only cases
    when this will be overriden is when the total size of all buffers for the
    merge strategy is bigger than the 'rowid_merge_buff_size' system variable,
    or if there isn't enough physical memory to allocate the buffers.
  */
  if (!optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_ROWID_MERGE) &&
       optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_TABLE_SCAN))
    strategy= PARTIAL_MATCH_SCAN;
  else if
     ( optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_ROWID_MERGE) &&
      !optimizer_flag(thd, OPTIMIZER_SWITCH_PARTIAL_MATCH_TABLE_SCAN))
    strategy= PARTIAL_MATCH_MERGE;

  /*
    If both switches are ON, or both are OFF, we interpret that as "let the
    optimizer decide". Perform a cost based choice between the two partial
    matching strategies.
  */
  /*
    TIMOUR: the above interpretation of the switch values could be changed to:
    - if both are ON - let the optimizer decide,
    - if both are OFF - do not use partial matching, therefore do not use
      materialization in non-top-level predicates.
    The problem with this is that we know for sure if we need partial matching
    only after the subquery is materialized, and this is too late to revert to
    the IN=>EXISTS strategy.
  */
  if (strategy == PARTIAL_MATCH)
  {
    /*
      TIMOUR: Currently we use a super simplistic measure. This will be
      addressed in a separate task.
    */
    if (tmp_table->file->stats.records < 100)
      strategy= PARTIAL_MATCH_SCAN;
    else
      strategy= PARTIAL_MATCH_MERGE;
  }

  /* Check if there is enough memory for the rowid merge strategy. */
  if (strategy == PARTIAL_MATCH_MERGE)
  {
    pm_buff_size= rowid_merge_buff_size(has_non_null_key,
                                        has_covering_null_row,
                                        partial_match_key_parts);
    if (pm_buff_size > thd->variables.rowid_merge_buff_size)
      strategy= PARTIAL_MATCH_SCAN;
  }
}


/*
  Compute the memory size of all buffers proportional to the number of rows
  in tmp_table.

  @details
  If the result is bigger than thd->variables.rowid_merge_buff_size, partial
  matching via merging is not applicable.
*/

ulonglong subselect_hash_sj_engine::rowid_merge_buff_size(
  bool has_non_null_key, bool has_covering_null_row,
  MY_BITMAP *partial_match_key_parts)
{
  /* Total size of all buffers used by partial matching. */
  ulonglong buff_size;
  ha_rows row_count= tmp_table->file->stats.records;
  uint rowid_length= tmp_table->file->ref_length;
  select_materialize_with_stats *result_sink=
    (select_materialize_with_stats *) result;
  ha_rows max_null_row;

  /* Size of the subselect_rowid_merge_engine::row_num_to_rowid buffer. */
  buff_size= row_count * rowid_length * sizeof(uchar);

  if (has_non_null_key)
  {
    /* Add the size of Ordered_key::key_buff of the only non-NULL key. */
    buff_size+= row_count * sizeof(rownum_t);
  }

  if (!has_covering_null_row)
  {
    for (uint i= 0; i < partial_match_key_parts->n_bits; i++)
    {
      if (!bitmap_is_set(partial_match_key_parts, i) ||
          result_sink->get_null_count_of_col(i) == row_count)
        continue; /* In these cases we wouldn't construct Ordered keys. */

      /* Add the size of Ordered_key::key_buff */
      buff_size+= (row_count - result_sink->get_null_count_of_col(i)) *
                         sizeof(rownum_t);
      /* Add the size of Ordered_key::null_key */
      max_null_row= result_sink->get_max_null_of_col(i);
      if (max_null_row >= UINT_MAX)
      {
        /*
          There can be at most UINT_MAX bits in a MY_BITMAP that is used to
          store NULLs in an Ordered_key. Return a number of bytes bigger than
          the maximum allowed memory buffer for partial matching to disable
          the rowid merge strategy.
        */
        return ULONGLONG_MAX;
      }
      buff_size+= bitmap_buffer_size(max_null_row);
    }
  }

  return buff_size;
}


/*
  Initialize a MY_BITMAP with a buffer allocated on the current
  memory root.
  TIMOUR: move to bitmap C file?
*/

static my_bool
bitmap_init_memroot(MY_BITMAP *map, uint n_bits, MEM_ROOT *mem_root)
{
  my_bitmap_map *bitmap_buf;

  if (!(bitmap_buf= (my_bitmap_map*) alloc_root(mem_root,
                                                bitmap_buffer_size(n_bits))) ||
      bitmap_init(map, bitmap_buf, n_bits, FALSE))
    return TRUE;
  bitmap_clear_all(map);
  return FALSE;
}


/**
  Create all structures needed for IN execution that can live between PS
  reexecution.

  @param tmp_columns the items that produce the data for the temp table
  @param subquery_id subquery's identifier (to make "<subquery%d>" name for
                                            EXPLAIN)

  @details
  - Create a temporary table to store the result of the IN subquery. The
    temporary table has one hash index on all its columns.
  - Create a new result sink that sends the result stream of the subquery to
    the temporary table,

  @notice:
    Currently Item_subselect::init() already chooses and creates at parse
    time an engine with a corresponding JOIN to execute the subquery.

  @retval TRUE  if error
  @retval FALSE otherwise
*/

bool subselect_hash_sj_engine::init(List<Item> *tmp_columns, uint subquery_id)
{
  select_union *result_sink;
  /* Options to create_tmp_table. */
  ulonglong tmp_create_options= thd->options | TMP_TABLE_ALL_COLUMNS;
                             /* | TMP_TABLE_FORCE_MYISAM; TIMOUR: force MYISAM */

  DBUG_ENTER("subselect_hash_sj_engine::init");

  if (bitmap_init_memroot(&non_null_key_parts, tmp_columns->elements,
                            thd->mem_root) ||
      bitmap_init_memroot(&partial_match_key_parts, tmp_columns->elements,
                            thd->mem_root))
    DBUG_RETURN(TRUE);

  /*
    Create and initialize a select result interceptor that stores the
    result stream in a temporary table. The temporary table itself is
    managed (created/filled/etc) internally by the interceptor.
  */
/*
  TIMOUR:
  Select a more efficient result sink when we know there is no need to collect
  data statistics.

  if (strategy == COMPLETE_MATCH)
  {
    if (!(result= new select_union))
      DBUG_RETURN(TRUE);
  }
  else if (strategy == PARTIAL_MATCH)
  {
  if (!(result= new select_materialize_with_stats))
    DBUG_RETURN(TRUE);
  }
*/
  if (!(result_sink= new select_materialize_with_stats))
    DBUG_RETURN(TRUE);
    
  char buf[32];
  uint len= my_snprintf(buf, sizeof(buf), "<subquery%d>", subquery_id);
  char *name;
  if (!(name= (char*)thd->alloc(len + 1)))
    DBUG_RETURN(TRUE);
  memcpy(name, buf, len+1);

  result_sink->get_tmp_table_param()->materialized_subquery= true;
  if (item->substype() == Item_subselect::IN_SUBS && 
      ((Item_in_subselect*)item)->is_jtbm_merged)
  {
    result_sink->get_tmp_table_param()->force_not_null_cols= true;
  }
  if (result_sink->create_result_table(thd, tmp_columns, TRUE,
                                       tmp_create_options,
				       name, TRUE, TRUE))
    DBUG_RETURN(TRUE);

  tmp_table= result_sink->table;
  result= result_sink;

  /*
    If the subquery has blobs, or the total key lenght is bigger than
    some length, or the total number of key parts is more than the
    allowed maximum (currently MAX_REF_PARTS == 16), then the created
    index cannot be used for lookups and we can't use hash semi
    join. If this is the case, delete the temporary table since it
    will not be used, and tell the caller we failed to initialize the
    engine.
  */
  if (tmp_table->s->keys == 0)
  {
    //fprintf(stderr, "Q: %s\n", current_thd->query());
    DBUG_ASSERT(0);
    DBUG_ASSERT(
      tmp_table->s->uniques ||
      tmp_table->key_info->key_length >= tmp_table->file->max_key_length() ||
      tmp_table->key_info->key_parts > tmp_table->file->max_key_parts());
    free_tmp_table(thd, tmp_table);
    tmp_table= NULL;
    delete result;
    result= NULL;
    DBUG_RETURN(TRUE);
  }

  /*
    Make sure there is only one index on the temp table, and it doesn't have
    the extra key part created when s->uniques > 0.
  */
  DBUG_ASSERT(tmp_table->s->keys == 1 &&
              ((Item_in_subselect *) item)->left_expr->cols() ==
              tmp_table->key_info->key_parts);

  if (make_semi_join_conds() ||
      /* A unique_engine is used both for complete and partial matching. */
      !(lookup_engine= make_unique_engine()))
    DBUG_RETURN(TRUE);

  /*
    Repeat name resolution for 'cond' since cond is not part of any
    clause of the query, and it is not 'fixed' during JOIN::prepare.
  */
  if (semi_join_conds && !semi_join_conds->fixed &&
      semi_join_conds->fix_fields(thd, (Item**)&semi_join_conds))
    DBUG_RETURN(TRUE);
  /* Let our engine reuse this query plan for materialization. */
  materialize_join= materialize_engine->join;
  materialize_join->change_result(result);

  DBUG_RETURN(FALSE);
}


/*
  Create an artificial condition to post-filter those rows matched by index
  lookups that cannot be distinguished by the index lookup procedure.

  @notes
  The need for post-filtering may occur e.g. because of
  truncation. Prepared statements execution requires that fix_fields is
  called for every execution. In order to call fix_fields we need to
  create a Name_resolution_context and a corresponding TABLE_LIST for
  the temporary table for the subquery, so that all column references
  to the materialized subquery table can be resolved correctly.

  @returns
    @retval TRUE  memory allocation error occurred
    @retval FALSE the conditions were created and resolved (fixed)
*/

bool subselect_hash_sj_engine::make_semi_join_conds()
{
  /*
    Table reference for tmp_table that is used to resolve column references
    (Item_fields) to columns in tmp_table.
  */
  TABLE_LIST *tmp_table_ref;
  /* Name resolution context for all tmp_table columns created below. */
  Name_resolution_context *context;
  Item_in_subselect *item_in= (Item_in_subselect *) item;

  DBUG_ENTER("subselect_hash_sj_engine::make_semi_join_conds");
  DBUG_ASSERT(semi_join_conds == NULL);

  if (!(semi_join_conds= new Item_cond_and))
    DBUG_RETURN(TRUE);

  if (!(tmp_table_ref= (TABLE_LIST*) thd->alloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(TRUE);

  tmp_table_ref->init_one_table("", tmp_table->alias.c_ptr(), TL_READ);
  tmp_table_ref->table= tmp_table;

  context= new Name_resolution_context;
  context->init();
  context->first_name_resolution_table=
    context->last_name_resolution_table= tmp_table_ref;
  semi_join_conds_context= context;
  
  for (uint i= 0; i < item_in->left_expr->cols(); i++)
  {
    Item_func_eq *eq_cond; /* New equi-join condition for the current column. */
    /* Item for the corresponding field from the materialized temp table. */
    Item_field *right_col_item;

    if (!(right_col_item= new Item_field(thd, context, tmp_table->field[i])) ||
        !(eq_cond= new Item_func_eq(item_in->left_expr->element_index(i),
                                    right_col_item)) ||
        (((Item_cond_and*)semi_join_conds)->add(eq_cond)))
    {
      delete semi_join_conds;
      semi_join_conds= NULL;
      DBUG_RETURN(TRUE);
    }
  }
  if (semi_join_conds->fix_fields(thd, (Item**)&semi_join_conds))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}


/**
  Create a new uniquesubquery engine for the execution of an IN predicate.

  @details
  Create and initialize a new JOIN_TAB, and Table_ref objects to perform
  lookups into the indexed temporary table.

  @retval A new subselect_hash_sj_engine object
  @retval NULL if a memory allocation error occurs
*/

subselect_uniquesubquery_engine*
subselect_hash_sj_engine::make_unique_engine()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;
  Item_iterator_row it(item_in->left_expr);
  /* The only index on the temporary table. */
  KEY *tmp_key= tmp_table->key_info;
  JOIN_TAB *tab;

  DBUG_ENTER("subselect_hash_sj_engine::make_unique_engine");

  /*
    Create and initialize the JOIN_TAB that represents an index lookup
    plan operator into the materialized subquery result. Notice that:
    - this JOIN_TAB has no corresponding JOIN (and doesn't need one), and
    - here we initialize only those members that are used by
      subselect_uniquesubquery_engine, so these objects are incomplete.
  */
  if (!(tab= (JOIN_TAB*) thd->alloc(sizeof(JOIN_TAB))))
    DBUG_RETURN(NULL);

  tab->table= tmp_table;
  tab->preread_init_done= FALSE;
  tab->ref.tmp_table_index_lookup_init(thd, tmp_key, it, FALSE);

  DBUG_RETURN(new subselect_uniquesubquery_engine(thd, tab, item,
                                                  semi_join_conds));
}


subselect_hash_sj_engine::~subselect_hash_sj_engine()
{
  delete lookup_engine;
  delete result;
  if (tmp_table)
    free_tmp_table(thd, tmp_table);
}


int subselect_hash_sj_engine::prepare()
{
  /*
    Create and optimize the JOIN that will be used to materialize
    the subquery if not yet created.
  */
  return materialize_engine->prepare();
}


/**
  Cleanup performed after each PS execution.

  @details
  Called in the end of JOIN::prepare for PS from Item_subselect::cleanup.
*/

void subselect_hash_sj_engine::cleanup()
{
  enum_engine_type lookup_engine_type= lookup_engine->engine_type();
  is_materialized= FALSE;
  bitmap_clear_all(&non_null_key_parts);
  bitmap_clear_all(&partial_match_key_parts);
  count_partial_match_columns= 0;
  count_null_only_columns= 0;
  strategy= UNDEFINED;
  materialize_engine->cleanup();
  /*
    Restore the original Item_in_subselect engine. This engine is created once
    at parse time and stored across executions, while all other materialization
    related engines are created and chosen for each execution.
  */
  ((Item_in_subselect *) item)->engine= materialize_engine;
  if (lookup_engine_type == TABLE_SCAN_ENGINE ||
      lookup_engine_type == ROWID_MERGE_ENGINE)
  {
    subselect_engine *inner_lookup_engine;
    inner_lookup_engine=
      ((subselect_partial_match_engine*) lookup_engine)->lookup_engine;
    /*
      Partial match engines are recreated for each PS execution inside
      subselect_hash_sj_engine::exec().
    */
    delete lookup_engine;
    lookup_engine= inner_lookup_engine;
  }
  DBUG_ASSERT(lookup_engine->engine_type() == UNIQUESUBQUERY_ENGINE);
  lookup_engine->cleanup();
  result->cleanup(); /* Resets the temp table as well. */
  DBUG_ASSERT(tmp_table);
  free_tmp_table(thd, tmp_table);
  tmp_table= NULL;
}


/*
  Get fanout produced by tables specified in the table_map
*/

double get_fanout_with_deps(JOIN *join, table_map tset)
{
  /* Handle the case of "Impossible WHERE" */
  if (join->table_count == 0)
    return 0.0;

  /* First, recursively get all tables we depend on */
  table_map deps_to_check= tset;
  table_map checked_deps= 0;
  table_map further_deps;
  do
  {
    further_deps= 0;
    Table_map_iterator tm_it(deps_to_check);
    int tableno;
    while ((tableno = tm_it.next_bit()) != Table_map_iterator::BITMAP_END)
    {
      /* get tableno's dependency tables that are not in needed_set */
      further_deps |= join->map2table[tableno]->ref.depend_map & ~checked_deps;
    }

    checked_deps |= deps_to_check;
    deps_to_check= further_deps;
  } while (further_deps != 0);

  
  /* Now, walk the join order and calculate the fanout */
  double fanout= 1;
  for (JOIN_TAB *tab= first_top_level_tab(join, WITHOUT_CONST_TABLES); tab;
       tab= next_top_level_tab(join, tab))
  {
    /* 
      Ignore SJM nests. They have tab->table==NULL. There is no point to walk
      inside them, because GROUP BY clause cannot refer to tables from within
      subquery.
    */
    if (!tab->is_sjm_nest() && (tab->table->map & checked_deps) && 
        !tab->emb_sj_nest && 
        tab->records_read != 0)
    {
      fanout *= rows2double(tab->records_read);
    }
  } 
  return fanout;
}


#if 0
void check_out_index_stats(JOIN *join)
{
  ORDER *order;
  uint n_order_items;

  /*
    First, collect the keys that we can use in each table.
    We can use a key if 
    - all tables refer to it.
  */
  key_map key_start_use[MAX_TABLES];
  key_map key_infix_use[MAX_TABLES];
  table_map key_used=0;
  table_map non_key_used= 0;
  
  bzero(&key_start_use, sizeof(key_start_use)); //psergey-todo: safe initialization!
  bzero(&key_infix_use, sizeof(key_infix_use));
  
  for (order= join->group_list; order; order= order->next)
  {
    Item *item= order->item[0];

    if (item->real_type() == Item::FIELD_ITEM)
    {
      if (item->used_tables() & OUTER_REF_TABLE_BIT)
        continue; /* outside references are like constants for us */

      Field *field= ((Item_field*)item->real_item())->field;
      uint table_no= field->table->tablenr;
      if (!(non_key_used && table_map(1) << table_no) && 
          !field->part_of_key.is_clear_all())
      {
        key_map infix_map= field->part_of_key;
        infix_map.subtract(field->key_start);
        key_start_use[table_no].merge(field->key_start);
        key_infix_use[table_no].merge(infix_map);
        key_used |= table_no;
      }
      continue;
    }
    /* 
      Note: the below will cause clauses like GROUP BY YEAR(date) not to be
      handled. 
    */
    non_key_used |= item->used_tables();
  }
  
  Table_map_iterator tm_it(key_used & ~non_key_used);
  int tableno;
  while ((tableno = tm_it.next_bit()) != Table_map_iterator::BITMAP_END)
  {
    key_map::iterator key_it(key_start_use);
    int keyno;
    while ((keyno = tm_it.next_bit()) != key_map::iterator::BITMAP_END)
    {
      for (order= join->group_list; order; order= order->next)
      {
        Item *item= order->item[0];
        if (item->used_tables() & (table_map(1) << tableno))
        {
          DBUG_ASSERT(item->real_type() == Item::FIELD_ITEM);
        }
      }
      /*
      if (continuation)
      {
        walk through list and find which key parts are occupied;
        // note that the above can't be made any faster.
      }
      else
        use rec_per_key[0];
      
      find out the cardinality.
      check if cardinality decreases if we use it;
      */
    }
  }
}
#endif


/*
  Get an estimate of how many records will be produced after the GROUP BY
  operation.

  @param join           Join we're operating on 
  @param join_op_rows   How many records will be produced by the join
                        operations (this is what join optimizer produces)
  
  @seealso
     See also optimize_semijoin_nests(), grep for "Adjust output cardinality 
     estimates".  Very similar code there that is not joined with this one
     because we operate on different data structs and too much effort is
     needed to abstract them out.

  @return
     Number of records we expect to get after the GROUP BY operation
*/

double get_post_group_estimate(JOIN* join, double join_op_rows)
{
  table_map tables_in_group_list= table_map(0);

  /* Find out which tables are used in GROUP BY list */
  for (ORDER *order= join->group_list; order; order= order->next)
  {
    Item *item= order->item[0];
    if (item->used_tables() & RAND_TABLE_BIT)
    {
      /* Each join output record will be in its own group */
      return join_op_rows;
    }
    tables_in_group_list|= item->used_tables();
  }
  tables_in_group_list &= ~PSEUDO_TABLE_BITS;

  /*
    Use join fanouts to calculate the max. number of records in the group-list
  */
  double fanout_rows[MAX_KEY];
  bzero(&fanout_rows, sizeof(fanout_rows));
  double out_rows;
  
  out_rows= get_fanout_with_deps(join, tables_in_group_list);

#if 0
  /* The following will be needed when making use of index stats: */
  /* 
    Also generate max. number of records for each of the tables mentioned 
    in the group-list. We'll use that a baseline number that we'll try to 
    reduce by using
     - #table-records 
     - index statistics.
  */
  Table_map_iterator tm_it(tables_in_group_list);
  int tableno;
  while ((tableno = tm_it.next_bit()) != Table_map_iterator::BITMAP_END)
  {
    fanout_rows[tableno]= get_fanout_with_deps(join, table_map(1) << tableno);
  }
  
  /*
    Try to bring down estimates using index statistics.
  */
  //check_out_index_stats(join);
#endif

  return out_rows;
}


/**
  Execute a subquery IN predicate via materialization.

  @details
  If needed materialize the subquery into a temporary table, then
  copmpute the predicate via a lookup into this table.

  @retval TRUE  if error
  @retval FALSE otherwise
*/

int subselect_hash_sj_engine::exec()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;
  SELECT_LEX *save_select= thd->lex->current_select;
  subselect_partial_match_engine *pm_engine= NULL;
  int res= 0;

  DBUG_ENTER("subselect_hash_sj_engine::exec");

  /*
    Optimize and materialize the subquery during the first execution of
    the subquery predicate.
  */
  thd->lex->current_select= materialize_engine->select_lex;
  /* The subquery should be optimized, and materialized only once. */
  DBUG_ASSERT(materialize_join->optimized && !is_materialized);
  materialize_join->exec();
  if ((res= test(materialize_join->error || thd->is_fatal_error ||
                 thd->is_error())))
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
  /*
    If the subquery returned no rows, the temporary table is empty, so we know
    directly that the result of IN is FALSE. We first update the table
    statistics, then we test if the temporary table for the query result is
    empty.
  */
  tmp_table->file->info(HA_STATUS_VARIABLE);
  if (!tmp_table->file->stats.records)
  {
    /* The value of IN will not change during this execution. */
    item_in->reset();
    item_in->make_const();
    item_in->set_first_execution();
    DBUG_RETURN(FALSE);
  }

  /*
    TIMOUR: The schema-based analysis for partial matching can be done once for
    prepared statement and remembered. It is done here to remove the need to
    save/restore all related variables between each re-execution, thus making
    the code simpler.
  */
  strategy= get_strategy_using_schema();
  /* This call may discover that we don't need partial matching at all. */
  strategy= get_strategy_using_data();
  if (strategy == PARTIAL_MATCH)
  {
    uint count_pm_keys; /* Total number of keys needed for partial matching. */
    MY_BITMAP *nn_key_parts= NULL; /* Key parts of the only non-NULL index. */
    uint count_non_null_columns= 0; /* Number of columns in nn_key_parts. */
    bool has_covering_null_row;
    bool has_covering_null_columns;
    select_materialize_with_stats *result_sink=
      (select_materialize_with_stats *) result;
    uint field_count= tmp_table->s->fields;

    if (count_partial_match_columns < field_count)
    {
      nn_key_parts= &non_null_key_parts;
      count_non_null_columns= bitmap_bits_set(nn_key_parts);
    }
    has_covering_null_row= (result_sink->get_max_nulls_in_row() == field_count);
    has_covering_null_columns= (count_non_null_columns +
                                count_null_only_columns == field_count);

    if (has_covering_null_row && has_covering_null_columns)
    {
      /*
        The whole table consist of only NULL values. The result of IN is
        a constant UNKNOWN.
      */
      DBUG_ASSERT(tmp_table->file->stats.records == 1);
      item_in->value= 0;
      item_in->null_value= 1;
      item_in->make_const();
      item_in->set_first_execution();
      DBUG_RETURN(FALSE);
    }

    if (has_covering_null_row)
    {
      DBUG_ASSERT(count_partial_match_columns = field_count);
      count_pm_keys= 0;
    }
    else if (has_covering_null_columns)
      count_pm_keys= 1;
    else
      count_pm_keys= count_partial_match_columns - count_null_only_columns +
                     (nn_key_parts ? 1 : 0);

    choose_partial_match_strategy(test(nn_key_parts),
                                  has_covering_null_row,
                                  &partial_match_key_parts);
    DBUG_ASSERT(strategy == PARTIAL_MATCH_MERGE ||
                strategy == PARTIAL_MATCH_SCAN);
    if (strategy == PARTIAL_MATCH_MERGE)
    {
      pm_engine=
        new subselect_rowid_merge_engine(thd, (subselect_uniquesubquery_engine*)
                                         lookup_engine, tmp_table,
                                         count_pm_keys,
                                         has_covering_null_row,
                                         has_covering_null_columns,
                                         count_columns_with_nulls,
                                         item, result,
                                         semi_join_conds->argument_list());
      if (!pm_engine ||
          ((subselect_rowid_merge_engine*) pm_engine)->
            init(nn_key_parts, &partial_match_key_parts))
      {
        /*
          The call to init() would fail if there was not enough memory to allocate
          all buffers for the rowid merge strategy. In this case revert to table
          scanning which doesn't need any big buffers.
        */
        delete pm_engine;
        pm_engine= NULL;
        strategy= PARTIAL_MATCH_SCAN;
      }
    }

    if (strategy == PARTIAL_MATCH_SCAN)
    {
      if (!(pm_engine=
            new subselect_table_scan_engine(thd, (subselect_uniquesubquery_engine*)
                                            lookup_engine, tmp_table,
                                            item, result,
                                            semi_join_conds->argument_list(),
                                            has_covering_null_row,
                                            has_covering_null_columns,
                                            count_columns_with_nulls)))
      {
        /* This is an irrecoverable error. */
        res= 1;
        goto err;
      }
    }
  }

  if (pm_engine)
    lookup_engine= pm_engine;
  item_in->change_engine(lookup_engine);

err:
  thd->lex->current_select= save_select;
  DBUG_RETURN(res);
}


/**
  Print the state of this engine into a string for debugging and views.
*/

void subselect_hash_sj_engine::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN(" <materialize> ("));
  materialize_engine->print(str, query_type);
  str->append(STRING_WITH_LEN(" ), "));

  if (lookup_engine)
    lookup_engine->print(str, query_type);
  else
    str->append(STRING_WITH_LEN(
           "<engine selected at execution time>"
         ));
}

void subselect_hash_sj_engine::fix_length_and_dec(Item_cache** row)
{
  DBUG_ASSERT(FALSE);
}

void subselect_hash_sj_engine::exclude()
{
  DBUG_ASSERT(FALSE);
}

bool subselect_hash_sj_engine::no_tables()
{
  DBUG_ASSERT(FALSE);
  return FALSE;
}

bool subselect_hash_sj_engine::change_result(Item_subselect *si,
                                             select_result_interceptor *res,
                                             bool temp __attribute__((unused)))
{
  DBUG_ASSERT(FALSE);
  return TRUE;
}


Ordered_key::Ordered_key(uint keyid_arg, TABLE *tbl_arg, Item *search_key_arg,
                         ha_rows null_count_arg, ha_rows min_null_row_arg,
                         ha_rows max_null_row_arg, uchar *row_num_to_rowid_arg)
  : keyid(keyid_arg), tbl(tbl_arg), search_key(search_key_arg),
    row_num_to_rowid(row_num_to_rowid_arg), null_count(null_count_arg)
{
  DBUG_ASSERT(tbl->file->stats.records > null_count);
  key_buff_elements= tbl->file->stats.records - null_count;
  cur_key_idx= HA_POS_ERROR;

  DBUG_ASSERT((null_count && min_null_row_arg && max_null_row_arg) ||
              (!null_count && !min_null_row_arg && !max_null_row_arg));
  if (null_count)
  {
    /* The counters are 1-based, for key access we need 0-based indexes. */
    min_null_row= min_null_row_arg - 1;
    max_null_row= max_null_row_arg - 1;
  }
  else
    min_null_row= max_null_row= 0;
}


Ordered_key::~Ordered_key()
{
  my_free((char*) key_buff, MYF(0));
  bitmap_free(&null_key);
}


/*
  Cleanup that needs to be done for each PS (re)execution.
*/

void Ordered_key::cleanup()
{
  /*
    Currently these keys are recreated for each PS re-execution, thus
    there is nothing to cleanup, the whole object goes away after execution
    is over. All handler related initialization/deinitialization is done by
    the parent subselect_rowid_merge_engine object.
  */
}


/*
  Initialize a multi-column index.
*/

bool Ordered_key::init(MY_BITMAP *columns_to_index)
{
  THD *thd= tbl->in_use;
  uint cur_key_col= 0;
  Item_field *cur_tmp_field;
  Item_func_lt *fn_less_than;

  key_column_count= bitmap_bits_set(columns_to_index);
  key_columns= (Item_field**) thd->alloc(key_column_count *
                                         sizeof(Item_field*));
  compare_pred= (Item_func_lt**) thd->alloc(key_column_count *
                                            sizeof(Item_func_lt*));

  if (!key_columns || !compare_pred)
    return TRUE; /* Revert to table scan partial match. */

  for (uint i= 0; i < columns_to_index->n_bits; i++)
  {
    if (!bitmap_is_set(columns_to_index, i))
      continue;
    cur_tmp_field= new Item_field(tbl->field[i]);
    /* Create the predicate (tmp_column[i] < outer_ref[i]). */
    fn_less_than= new Item_func_lt(cur_tmp_field,
                                   search_key->element_index(i));
    fn_less_than->fix_fields(thd, (Item**) &fn_less_than);
    key_columns[cur_key_col]= cur_tmp_field;
    compare_pred[cur_key_col]= fn_less_than;
    ++cur_key_col;
  }

  if (alloc_keys_buffers())
  {
    /* TIMOUR revert to partial match via table scan. */
    return TRUE;
  }
  return FALSE;
}


/*
  Initialize a single-column index.
*/

bool Ordered_key::init(int col_idx)
{
  THD *thd= tbl->in_use;

  key_column_count= 1;

  // TIMOUR: check for mem allocation err, revert to scan

  key_columns= (Item_field**) thd->alloc(sizeof(Item_field*));
  compare_pred= (Item_func_lt**) thd->alloc(sizeof(Item_func_lt*));

  key_columns[0]= new Item_field(tbl->field[col_idx]);
  /* Create the predicate (tmp_column[i] < outer_ref[i]). */
  compare_pred[0]= new Item_func_lt(key_columns[0],
                                    search_key->element_index(col_idx));
  compare_pred[0]->fix_fields(thd, (Item**)&compare_pred[0]);

  if (alloc_keys_buffers())
  {
    /* TIMOUR revert to partial match via table scan. */
    return TRUE;
  }
  return FALSE;
}


/*
  Allocate the buffers for both the row number, and the NULL-bitmap indexes.
*/

bool Ordered_key::alloc_keys_buffers()
{
  DBUG_ASSERT(key_buff_elements > 0);

  if (!(key_buff= (rownum_t*) my_malloc((size_t)(key_buff_elements * 
    sizeof(rownum_t)), MYF(MY_WME))))
    return TRUE;

  /*
    TIMOUR: it is enough to create bitmaps with size
    (max_null_row - min_null_row), and then use min_null_row as
    lookup offset.
  */
  /* Notice that max_null_row is max array index, we need count, so +1. */
  if (bitmap_init(&null_key, NULL, (uint)(max_null_row + 1), FALSE))
    return TRUE;

  cur_key_idx= HA_POS_ERROR;

  return FALSE;
}


/*
  Quick sort comparison function that compares two rows of the same table
  indentfied with their row numbers.

  @retval -1
  @retval  0
  @retval +1
*/

int
Ordered_key::cmp_keys_by_row_data(ha_rows a, ha_rows b)
{
  uchar *rowid_a, *rowid_b;
  int error, cmp_res;
  /* The length in bytes of the rowids (positions) of tmp_table. */
  uint rowid_length= tbl->file->ref_length;

  if (a == b)
    return 0;
  /* Get the corresponding rowids. */
  rowid_a= row_num_to_rowid + a * rowid_length;
  rowid_b= row_num_to_rowid + b * rowid_length;
  /* Fetch the rows for comparison. */
  error= tbl->file->ha_rnd_pos(tbl->record[0], rowid_a);
  DBUG_ASSERT(!error);
  error= tbl->file->ha_rnd_pos(tbl->record[1], rowid_b);
  DBUG_ASSERT(!error);
  /*
    Compare the two rows by the corresponding values of the indexed
    columns.
  */
  for (uint i= 0; i < key_column_count; i++)
  {
    Field *cur_field= key_columns[i]->field;
    if ((cmp_res= cur_field->cmp_offset(tbl->s->rec_buff_length)))
      return (cmp_res > 0 ? 1 : -1);
  }
  return 0;
}


int
Ordered_key::cmp_keys_by_row_data_and_rownum(Ordered_key *key,
                                             rownum_t* a, rownum_t* b)
{
  /* The result of comparing the two keys according to their row data. */
  int cmp_row_res= key->cmp_keys_by_row_data(*a, *b);
  if (cmp_row_res)
    return cmp_row_res;
  return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}


void Ordered_key::sort_keys()
{
  my_qsort2(key_buff, (size_t) key_buff_elements, sizeof(rownum_t),
            (qsort2_cmp) &cmp_keys_by_row_data_and_rownum, (void*) this);
  /* Invalidate the current row position. */
  cur_key_idx= HA_POS_ERROR;
}


/*
  The fraction of rows that do not contain NULL in the columns indexed by
  this key.

  @retval  1  if there are no NULLs
  @retval  0  if only NULLs
*/

double Ordered_key::null_selectivity()
{
  /* We should not be processing empty tables. */
  DBUG_ASSERT(tbl->file->stats.records);
  return (1 - (double) null_count / (double) tbl->file->stats.records);
}


/*
  Compare the value(s) of the current key in 'search_key' with the
  data of the current table record.

  @notes The comparison result follows from the way compare_pred
  is created in Ordered_key::init. Currently compare_pred compares
  a field in of the current row with the corresponding Item that
  contains the search key.

  @param row_num  Number of the row (not index in the key_buff array)

  @retval -1  if (current row  < search_key)
  @retval  0  if (current row == search_key)
  @retval +1  if (current row  > search_key)
*/

int Ordered_key::cmp_key_with_search_key(rownum_t row_num)
{
  /* The length in bytes of the rowids (positions) of tmp_table. */
  uint rowid_length= tbl->file->ref_length;
  uchar *cur_rowid= row_num_to_rowid + row_num * rowid_length;
  int error, cmp_res;

  error= tbl->file->ha_rnd_pos(tbl->record[0], cur_rowid);
  DBUG_ASSERT(!error);

  for (uint i= 0; i < key_column_count; i++)
  {
    cmp_res= compare_pred[i]->get_comparator()->compare();
    /* Unlike Arg_comparator::compare_row() here there should be no NULLs. */
    DBUG_ASSERT(!compare_pred[i]->null_value);
    if (cmp_res)
      return (cmp_res > 0 ? 1 : -1);
  }
  return 0;
}


/*
  Find a key in a sorted array of keys via binary search.

  see create_subq_in_equalities()
*/

bool Ordered_key::lookup()
{
  DBUG_ASSERT(key_buff_elements);

  ha_rows lo= 0;
  ha_rows hi= key_buff_elements - 1;
  ha_rows mid;
  int cmp_res;

  while (lo <= hi)
  {
    mid= lo + (hi - lo) / 2;
    cmp_res= cmp_key_with_search_key(key_buff[mid]);
    /*
      In order to find the minimum match, check if the pevious element is
      equal or smaller than the found one. If equal, we need to search further
      to the left.
    */
    if (!cmp_res && mid > 0)
      cmp_res= !cmp_key_with_search_key(key_buff[mid - 1]) ? 1 : 0;

    if (cmp_res == -1)
    {
      /* row[mid] < search_key */
      lo= mid + 1;
    }
    else if (cmp_res == 1)
    {
      /* row[mid] > search_key */
      if (!mid)
        goto not_found;
      hi= mid - 1;
    }
    else
    {
      /* row[mid] == search_key */
      cur_key_idx= mid;
      return TRUE;
    }
  }
not_found:
  cur_key_idx= HA_POS_ERROR;
  return FALSE;
}


/*
  Move the current index pointer to the next key with the same column
  values as the current key. Since the index is sorted, all such keys
  are contiguous.
*/

bool Ordered_key::next_same()
{
  DBUG_ASSERT(key_buff_elements);

  if (cur_key_idx < key_buff_elements - 1)
  {
    /*
      TIMOUR:
      The below is quite inefficient, since as a result we will fetch every
      row (except the last one) twice. There must be a more efficient way,
      e.g. swapping record[0] and record[1], and reading only the new record.
    */
    if (!cmp_keys_by_row_data(key_buff[cur_key_idx], key_buff[cur_key_idx + 1]))
    {
      ++cur_key_idx;
      return TRUE;
    }
  }
  return FALSE;
}


void Ordered_key::print(String *str)
{
  uint i;
  str->append("{idx=");
  str->qs_append(keyid);
  str->append(", (");
  for (i= 0; i < key_column_count - 1; i++)
  {
    str->append(key_columns[i]->field->field_name);
    str->append(", ");
  }
  str->append(key_columns[i]->field->field_name);
  str->append("), ");

  str->append("null_bitmap: (bits=");
  str->qs_append(null_key.n_bits);
  str->append(", nulls= ");
  str->qs_append((double)null_count);
  str->append(", min_null= ");
  str->qs_append((double)min_null_row);
  str->append(", max_null= ");
  str->qs_append((double)max_null_row);
  str->append("), ");

  str->append('}');
}


subselect_partial_match_engine::subselect_partial_match_engine(
  THD *thd_arg, subselect_uniquesubquery_engine *engine_arg,
  TABLE *tmp_table_arg, Item_subselect *item_arg,
  select_result_interceptor *result_arg,
  List<Item> *equi_join_conds_arg,
  bool has_covering_null_row_arg,
  bool has_covering_null_columns_arg,
  uint count_columns_with_nulls_arg)
  :subselect_engine(thd_arg, item_arg, result_arg),
   tmp_table(tmp_table_arg), lookup_engine(engine_arg),
   equi_join_conds(equi_join_conds_arg),
   has_covering_null_row(has_covering_null_row_arg),
   has_covering_null_columns(has_covering_null_columns_arg),
   count_columns_with_nulls(count_columns_with_nulls_arg)
{}


int subselect_partial_match_engine::exec()
{
  Item_in_subselect *item_in= (Item_in_subselect *) item;
  int lookup_res;

  DBUG_ASSERT(!(item_in->left_expr_has_null() &&
                item_in->is_top_level_item()));

  if (!item_in->left_expr_has_null())
  {
    /* Try to find a matching row by index lookup. */
    if (lookup_engine->copy_ref_key(false))
    {
      /* The result is FALSE based on the outer reference. */
      item_in->value= 0;
      item_in->null_value= 0;
      return 0;
    }
    else
    {
      /* Search for a complete match. */
      if ((lookup_res= lookup_engine->index_lookup()))
      {
        /* An error occured during lookup(). */
        item_in->value= 0;
        item_in->null_value= 0;
        return lookup_res;
      }
      else if (item_in->value || !count_columns_with_nulls)
      {
        /*
          A complete match was found, the result of IN is TRUE.
          If no match was found, and there are no NULLs in the materialized
          subquery, then the result is guaranteed to be false because this
          branch is executed when the outer reference has no NULLs as well.
          Notice: (this->item == lookup_engine->item)
        */
        return 0;
      }
    }
  }

  if (has_covering_null_row)
  {
    /*
      If there is a NULL-only row that coveres all columns the result of IN
      is UNKNOWN. 
    */
    item_in->value= 0;
    /*
      TIMOUR: which one is the right way to propagate an UNKNOWN result?
      Should we also set empty_result_set= FALSE; ???
    */
    //item_in->was_null= 1;
    item_in->null_value= 1;
    return 0;
  }

  /*
    There is no complete match. Look for a partial match (UNKNOWN result), or
    no match (FALSE).
  */
  if (tmp_table->file->inited)
    tmp_table->file->ha_index_end();

  if (partial_match())
  {
    /* The result of IN is UNKNOWN. */
    item_in->value= 0;
    /*
      TIMOUR: which one is the right way to propagate an UNKNOWN result?
      Should we also set empty_result_set= FALSE; ???
    */
    //item_in->was_null= 1;
    item_in->null_value= 1;
  }
  else
  {
    /* The result of IN is FALSE. */
    item_in->value= 0;
    /*
      TIMOUR: which one is the right way to propagate an UNKNOWN result?
      Should we also set empty_result_set= FALSE; ???
    */
    //item_in->was_null= 0;
    item_in->null_value= 0;
  }

  return 0;
}


void subselect_partial_match_engine::print(String *str,
                                           enum_query_type query_type)
{
  /*
    Should never be called as the actual engine cannot be known at query
    optimization time.
    DBUG_ASSERT(FALSE);
  */
}


/*
  @param non_null_key_parts  
  @param partial_match_key_parts  A union of all single-column NULL key parts.

  @retval FALSE  the engine was initialized successfully
  @retval TRUE   there was some (memory allocation) error during initialization,
                 such errors should be interpreted as revert to other strategy
*/

bool
subselect_rowid_merge_engine::init(MY_BITMAP *non_null_key_parts,
                                   MY_BITMAP *partial_match_key_parts)
{
  /* The length in bytes of the rowids (positions) of tmp_table. */
  uint rowid_length= tmp_table->file->ref_length;
  ha_rows row_count= tmp_table->file->stats.records;
  rownum_t cur_rownum= 0;
  select_materialize_with_stats *result_sink=
    (select_materialize_with_stats *) result;
  uint cur_keyid= 0;
  Item_in_subselect *item_in= (Item_in_subselect*) item;
  int error;

  if (merge_keys_count == 0)
  {
    DBUG_ASSERT(bitmap_bits_set(partial_match_key_parts) == 0 ||
                has_covering_null_row);
    /* There is nothing to initialize, we will only do regular lookups. */
    return FALSE;
  }

  /*
    If all nullable columns contain only NULLs, there must be one index
    over all non-null columns.
  */
  DBUG_ASSERT(!has_covering_null_columns ||
              (has_covering_null_columns &&
               merge_keys_count == 1 && non_null_key_parts));
  /*
    Allocate buffers to hold the merged keys and the mapping between rowids and
    row numbers. All small buffers are allocated in the runtime memroot. Big
    buffers are allocated from the OS via malloc.
  */
  if (!(merge_keys= (Ordered_key**) thd->alloc(merge_keys_count *
                                               sizeof(Ordered_key*))) ||
      !(null_bitmaps= (MY_BITMAP**) thd->alloc(merge_keys_count *
                                               sizeof(MY_BITMAP*))) ||
      !(row_num_to_rowid= (uchar*) my_malloc((size_t)(row_count * rowid_length),
        MYF(MY_WME))))
    return TRUE;

  /* Create the only non-NULL key if there is any. */
  if (non_null_key_parts)
  {
    non_null_key= new Ordered_key(cur_keyid, tmp_table, item_in->left_expr,
                                  0, 0, 0, row_num_to_rowid);
    if (non_null_key->init(non_null_key_parts))
      return TRUE;
    merge_keys[cur_keyid]= non_null_key;
    merge_keys[cur_keyid]->first();
    ++cur_keyid;
  }

  /*
    If all nullable columns contain NULLs, the only key that is needed is the
    only non-NULL key that is already created above.
  */
  if (!has_covering_null_columns)
  {
    if (bitmap_init_memroot(&matching_keys, merge_keys_count, thd->mem_root) ||
        bitmap_init_memroot(&matching_outer_cols, merge_keys_count, thd->mem_root))
      return TRUE;

    /*
      Create one single-column NULL-key for each column in
      partial_match_key_parts.
    */
    for (uint i= 0; i < partial_match_key_parts->n_bits; i++)
    {
      /* Skip columns that have no NULLs, or contain only NULLs. */
      if (!bitmap_is_set(partial_match_key_parts, i) ||
          result_sink->get_null_count_of_col(i) == row_count)
        continue;

      merge_keys[cur_keyid]= new Ordered_key(
                                     cur_keyid, tmp_table,
                                     item_in->left_expr->element_index(i),
                                     result_sink->get_null_count_of_col(i),
                                     result_sink->get_min_null_of_col(i),
                                     result_sink->get_max_null_of_col(i),
                                     row_num_to_rowid);
      if (merge_keys[cur_keyid]->init(i))
        return TRUE;
      merge_keys[cur_keyid]->first();
      ++cur_keyid;
    }
  }
  DBUG_ASSERT(cur_keyid == merge_keys_count);

  /* Populate the indexes with data from the temporary table. */
  if (tmp_table->file->ha_rnd_init_with_error(1))
    return TRUE;
  tmp_table->file->extra_opt(HA_EXTRA_CACHE,
                             current_thd->variables.read_buff_size);
  tmp_table->null_row= 0;
  while (TRUE)
  {
    error= tmp_table->file->ha_rnd_next(tmp_table->record[0]);
    if (error == HA_ERR_RECORD_DELETED)
    {
      /* We get this for duplicate records that should not be in tmp_table. */
      continue;
    }
    /*
      This is a temp table that we fully own, there should be no other
      cause to stop the iteration than EOF.
    */
    DBUG_ASSERT(!error || error == HA_ERR_END_OF_FILE);
    if (error == HA_ERR_END_OF_FILE)
    {
      DBUG_ASSERT(cur_rownum == tmp_table->file->stats.records);
      break;
    }

    /*
      Save the position of this record in the row_num -> rowid mapping.
    */
    tmp_table->file->position(tmp_table->record[0]);
    memcpy(row_num_to_rowid + cur_rownum * rowid_length,
           tmp_table->file->ref, rowid_length);

    /* Add the current row number to the corresponding keys. */
    if (non_null_key)
    {
      /* By definition there are no NULLs in the non-NULL key. */
      non_null_key->add_key(cur_rownum);
    }

    for (uint i= (non_null_key ? 1 : 0); i < merge_keys_count; i++)
    {
      /*
        Check if the first and only indexed column contains NULL in the curent
        row, and add the row number to the corresponding key.
      */
      if (tmp_table->field[merge_keys[i]->get_field_idx(0)]->is_null())
        merge_keys[i]->set_null(cur_rownum);
      else
        merge_keys[i]->add_key(cur_rownum);
    }
    ++cur_rownum;
  }

  tmp_table->file->ha_rnd_end();

  /* Sort all the keys by their NULL selectivity. */
  my_qsort(merge_keys, merge_keys_count, sizeof(Ordered_key*),
           (qsort_cmp) cmp_keys_by_null_selectivity);

  /* Sort the keys in each of the indexes. */
  for (uint i= 0; i < merge_keys_count; i++)
    merge_keys[i]->sort_keys();

  if (init_queue(&pq, merge_keys_count, 0, FALSE,
                 subselect_rowid_merge_engine::cmp_keys_by_cur_rownum, NULL,
                 0, 0))
    return TRUE;

  return FALSE;
}


subselect_rowid_merge_engine::~subselect_rowid_merge_engine()
{
  /* None of the resources below is allocated if there are no ordered keys. */
  if (merge_keys_count)
  {
    my_free((char*) row_num_to_rowid, MYF(0));
    for (uint i= 0; i < merge_keys_count; i++)
      delete merge_keys[i];
    delete_queue(&pq);
    if (tmp_table->file->inited == handler::RND)
      tmp_table->file->ha_rnd_end();
  }
}


void subselect_rowid_merge_engine::cleanup()
{
}


/*
  Quick sort comparison function to compare keys in order of decreasing bitmap
  selectivity, so that the most selective keys come first.

  @param  k1 first key to compare
  @param  k2 second key to compare

  @retval  1  if k1 is less selective than k2
  @retval  0  if k1 is equally selective as k2
  @retval -1  if k1 is more selective than k2
*/

int
subselect_rowid_merge_engine::cmp_keys_by_null_selectivity(Ordered_key **k1,
                                                           Ordered_key **k2)
{
  double k1_sel= (*k1)->null_selectivity();
  double k2_sel= (*k2)->null_selectivity();
  if (k1_sel < k2_sel)
    return 1;
  if (k1_sel > k2_sel)
    return -1;
  return 0;
}


/*
*/

int
subselect_rowid_merge_engine::cmp_keys_by_cur_rownum(void *arg,
                                                     uchar *k1, uchar *k2)
{
  rownum_t r1= ((Ordered_key*) k1)->current();
  rownum_t r2= ((Ordered_key*) k2)->current();

  return (r1 < r2) ? -1 : (r1 > r2) ? 1 : 0;
}


/*
  Check if certain table row contains a NULL in all columns for which there is
  no match in the corresponding value index.

  @note
  There is no need to check the columns that contain only NULLs, because
  those are guaranteed to match.

  @retval TRUE if a NULL row exists
  @retval FALSE otherwise
*/

bool subselect_rowid_merge_engine::test_null_row(rownum_t row_num)
{
  Ordered_key *cur_key;
  for (uint i = 0; i < merge_keys_count; i++)
  {
    cur_key= merge_keys[i];
    if (bitmap_is_set(&matching_keys, cur_key->get_keyid()))
    {
      /*
        The key 'i' (with id 'cur_keyid') already matches a value in row
        'row_num', thus we skip it as it can't possibly match a NULL.
      */
      continue;
    }
    if (!cur_key->is_null(row_num))
      return FALSE;
  }
  return TRUE;
}


/**
  Test if a subset of NULL-able columns contains a row of NULLs.
  @retval TRUE  if such a row exists
  @retval FALSE no complementing null row
*/

bool subselect_rowid_merge_engine::
exists_complementing_null_row(MY_BITMAP *keys_to_complement)
{
  rownum_t highest_min_row= 0;
  rownum_t lowest_max_row= UINT_MAX;
  uint count_null_keys, i;
  Ordered_key *cur_key;

  if (!count_columns_with_nulls)
  {
    /*
      If there are both NULLs and non-NUll values in the outer reference, and
      the subquery contains no NULLs, a complementing NULL row cannot exist.
    */
    return FALSE;
  }

  for (i= (non_null_key ? 1 : 0), count_null_keys= 0; i < merge_keys_count; i++)
  {
    cur_key= merge_keys[i];
    if (bitmap_is_set(keys_to_complement, cur_key->get_keyid()))
      continue;
    if (!cur_key->get_null_count())
    {
      /* If there is column without NULLs, there cannot be a partial match. */
      return FALSE;
    }
    if (cur_key->get_min_null_row() > highest_min_row)
      highest_min_row= cur_key->get_min_null_row();
    if (cur_key->get_max_null_row() < lowest_max_row)
      lowest_max_row= cur_key->get_max_null_row();
    null_bitmaps[count_null_keys++]= cur_key->get_null_key();
  }

  if (lowest_max_row < highest_min_row)
  {
    /* The intersection of NULL rows is empty. */
    return FALSE;
  }

  return bitmap_exists_intersection((const MY_BITMAP**) null_bitmaps,
                                    count_null_keys,
                                    (uint)highest_min_row, (uint)lowest_max_row);
}


/*
  @retval TRUE  there is a partial match (UNKNOWN)
  @retval FALSE  there is no match at all (FALSE)
*/

bool subselect_rowid_merge_engine::partial_match()
{
  Ordered_key *min_key; /* Key that contains the current minimum position. */
  rownum_t min_row_num; /* Current row number of min_key. */
  Ordered_key *cur_key;
  rownum_t cur_row_num;
  uint count_nulls_in_search_key= 0;
  uint max_null_in_any_row=
    ((select_materialize_with_stats *) result)->get_max_nulls_in_row();
  bool res= FALSE;

  /* If there is a non-NULL key, it must be the first key in the keys array. */
  DBUG_ASSERT(!non_null_key || (non_null_key && merge_keys[0] == non_null_key));
  /* The prioryty queue for keys must be empty. */
  DBUG_ASSERT(!pq.elements);

  /* All data accesses during execution are via handler::ha_rnd_pos() */
  if (tmp_table->file->ha_rnd_init_with_error(0))
  {
    res= FALSE;
    goto end;
  }

  /* Check if there is a match for the columns of the only non-NULL key. */
  if (non_null_key && !non_null_key->lookup())
  {
    res= FALSE;
    goto end;
  }

  /*
    If all nullable columns contain only NULLs, then there is a guranteed
    partial match, and we don't need to search for a matching row.
  */
  if (has_covering_null_columns)
  {
    res= TRUE;
    goto end;
  }

  if (non_null_key)
    queue_insert(&pq, (uchar *) non_null_key);
  /*
    Do not add the non_null_key, since it was already processed above.
  */
  bitmap_clear_all(&matching_outer_cols);
  for (uint i= test(non_null_key); i < merge_keys_count; i++)
  {
    DBUG_ASSERT(merge_keys[i]->get_column_count() == 1);
    if (merge_keys[i]->get_search_key(0)->null_value)
    {
      ++count_nulls_in_search_key;
      bitmap_set_bit(&matching_outer_cols, merge_keys[i]->get_keyid());
    }
    else if (merge_keys[i]->lookup())
      queue_insert(&pq, (uchar *) merge_keys[i]);
  }

  /*
    If the outer reference consists of only NULLs, or if it has NULLs in all
    nullable columns (above we guarantee there is a match for the non-null
    coumns), the result is UNKNOWN.
  */
  if (count_nulls_in_search_key == merge_keys_count - test(non_null_key))
  {
    res= TRUE;
    goto end;
  }

  /*
    If the outer row has NULLs in some columns, and
    there is no match for any of the remaining columns, and
    there is a subquery row with NULLs in all unmatched columns,
    then there is a partial match, otherwise the result is FALSE.
  */
  if (count_nulls_in_search_key && !pq.elements)
  {
    DBUG_ASSERT(!non_null_key);
    /*
      Check if the intersection of all NULL bitmaps of all keys that
      are not in matching_outer_cols is non-empty.
    */
    res= exists_complementing_null_row(&matching_outer_cols);
    goto end;
  }

  /*
    If there is no NULL (sub)row that covers all NULL columns, and there is no
    match for any of the NULL columns, the result is FALSE. Notice that if there
    is a non-null key, and there is only one matching key, the non-null key is
    the matching key. This is so, because this method returns FALSE if the
    non-null key doesn't have a match.
  */
  if (!count_nulls_in_search_key &&
      (!pq.elements ||
       (pq.elements == 1 && non_null_key &&
        max_null_in_any_row < merge_keys_count-1)))
  {
    if (!pq.elements)
    {
      DBUG_ASSERT(!non_null_key);
      /*
        The case of a covering null row is handled by
        subselect_partial_match_engine::exec()
      */
      DBUG_ASSERT(max_null_in_any_row != tmp_table->s->fields);
    }
    res= FALSE;
    goto end;
  }

  DBUG_ASSERT(pq.elements);

  min_key= (Ordered_key*) queue_remove_top(&pq);
  min_row_num= min_key->current();
  bitmap_set_bit(&matching_keys, min_key->get_keyid());
  bitmap_union(&matching_keys, &matching_outer_cols);
  if (min_key->next_same())
    queue_insert(&pq, (uchar *) min_key);

  if (pq.elements == 0)
  {
    /*
      Check the only matching row of the only key min_key for NULL matches
      in the other columns.
    */
    res= test_null_row(min_row_num);
    goto end;
  }

  while (TRUE)
  {
    cur_key= (Ordered_key*) queue_remove_top(&pq);
    cur_row_num= cur_key->current();

    if (cur_row_num == min_row_num)
      bitmap_set_bit(&matching_keys, cur_key->get_keyid());
    else
    {
      /* Follows from the correct use of priority queue. */
      DBUG_ASSERT(cur_row_num > min_row_num);
      if (test_null_row(min_row_num))
      {
        res= TRUE;
        goto end;
      }
      else
      {
        min_key= cur_key;
        min_row_num= cur_row_num;
        bitmap_clear_all(&matching_keys);
        bitmap_set_bit(&matching_keys, min_key->get_keyid());
        bitmap_union(&matching_keys, &matching_outer_cols);
      }
    }

    if (cur_key->next_same())
      queue_insert(&pq, (uchar *) cur_key);

    if (pq.elements == 0)
    {
      /* Check the last row of the last column in PQ for NULL matches. */
      res= test_null_row(min_row_num);
      goto end;
    }
  }

  /* We should never get here - all branches must be handled explicitly above. */
  DBUG_ASSERT(FALSE);

end:
  if (!has_covering_null_columns)
    bitmap_clear_all(&matching_keys);
  queue_remove_all(&pq);
  tmp_table->file->ha_rnd_end();
  return res;
}


subselect_table_scan_engine::subselect_table_scan_engine(
  THD *thd_arg, subselect_uniquesubquery_engine *engine_arg,
  TABLE *tmp_table_arg,
  Item_subselect *item_arg,
  select_result_interceptor *result_arg,
  List<Item> *equi_join_conds_arg,
  bool has_covering_null_row_arg,
  bool has_covering_null_columns_arg,
  uint count_columns_with_nulls_arg)
  :subselect_partial_match_engine(thd_arg, engine_arg, tmp_table_arg, item_arg,
                                  result_arg, equi_join_conds_arg,
                                  has_covering_null_row_arg,
                                  has_covering_null_columns_arg,
                                  count_columns_with_nulls_arg)
{}


/*
  TIMOUR:
  This method is based on subselect_uniquesubquery_engine::scan_table().
  Consider refactoring somehow, 80% of the code is the same.

  for each row_i in tmp_table
  {
    count_matches= 0;
    for each row element row_i[j]
    {
      if (outer_ref[j] is NULL || row_i[j] is NULL || outer_ref[j] == row_i[j])
        ++count_matches;
    }
    if (count_matches == outer_ref.elements)
      return TRUE
  }
  return FALSE
*/

bool subselect_table_scan_engine::partial_match()
{
  List_iterator_fast<Item> equality_it(*equi_join_conds);
  Item *cur_eq;
  uint count_matches;
  int error;
  bool res;

  if (tmp_table->file->ha_rnd_init_with_error(1))
  {
    res= FALSE;
    goto end;
  }

  tmp_table->file->extra_opt(HA_EXTRA_CACHE,
                             current_thd->variables.read_buff_size);
  for (;;)
  {
    error= tmp_table->file->ha_rnd_next(tmp_table->record[0]);
    if (error) {
      if (error == HA_ERR_RECORD_DELETED)
      {
        error= 0;
        continue;
      }
      if (error == HA_ERR_END_OF_FILE)
      {
        error= 0;
        break;
      }
      else
      {
        error= report_error(tmp_table, error);
        break;
      }
    }

    equality_it.rewind();
    count_matches= 0;
    while ((cur_eq= equality_it++))
    {
      DBUG_ASSERT(cur_eq->type() == Item::FUNC_ITEM &&
                  ((Item_func*)cur_eq)->functype() == Item_func::EQ_FUNC);
      if (!cur_eq->val_int() && !cur_eq->null_value)
        break;
      ++count_matches;
    }
    if (count_matches == tmp_table->s->fields)
    {
      res= TRUE; /* Found a matching row. */
      goto end;
    }
  }

  res= FALSE;
end:
  tmp_table->file->ha_rnd_end();
  return res;
}


void subselect_table_scan_engine::cleanup()
{
}


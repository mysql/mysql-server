/* Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // set_var.h: THD
#include "set_var.h"
#include "sql_select.h"
#include "sql_parse.h"                          // check_stack_overrun
#include "sql_test.h"

inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}

Item_subselect::Item_subselect():
  Item_result_field(), value_assigned(0), thd(0), substitution(0),
  engine(0), old_engine(0), used_tables_cache(0), have_to_be_excluded(0),
  const_item_cache(1), engine_changed(0), changed(0), is_correlated(FALSE)
{
  with_subselect= 1;
  reset();
  /*
    item value is NULL if select_subselect not changed this value
    (i.e. some rows will be found returned)
  */
  null_value= TRUE;
}


void Item_subselect::init(st_select_lex *select_lex,
			  select_subselect *result)
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
  DBUG_VOID_RETURN;
}


void Item_singlerow_subselect::cleanup()
{
  DBUG_ENTER("Item_singlerow_subselect::cleanup");
  value= 0; row= 0;
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


bool Item_subselect::fix_fields(THD *thd_param, Item **ref)
{
  char const *save_where= thd_param->where;
  uint8 uncacheable;
  bool res;

  DBUG_ASSERT(fixed == 0);
  engine->set_thd((thd= thd_param));

  if (check_stack_overrun(thd, STACK_MIN_SIZE, (uchar*)&res))
    return TRUE;

  if (!(res= engine->prepare()))
  {
    // all transformation is done (used by prepared statements)
    changed= 1;

    if (substitution)
    {
      int ret= 0;

      // did we changed top item of WHERE condition
      if (unit->outer_select()->where == (*ref))
	unit->outer_select()->where= substitution; // correct WHERE for PS
      else if (unit->outer_select()->having == (*ref))
	unit->outer_select()->having= substitution; // correct HAVING for PS

      (*ref)= substitution;
      substitution->name= name;
      substitution->name_length= name_length;
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


bool Item_subselect::walk(Item_processor processor, bool walk_subquery,
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


bool Item_subselect::exec()
{
  DBUG_ENTER("Item_subselect::exec");

  /*
    Do not execute subselect in case of a fatal error
    or if the query has been killed.
  */
  if (thd->is_error() || thd->killed)
    DBUG_RETURN(true);

  DBUG_ASSERT(!thd->lex->context_analysis_only);
  /*
    Simulate a failure in sub-query execution. Used to test e.g.
    out of memory or query being killed conditions.
  */
  DBUG_EXECUTE_IF("subselect_exec_fail", DBUG_RETURN(true););

  bool res= engine->exec();

  if (engine_changed)
  {
    engine_changed= 0;
    res= exec();
    DBUG_RETURN(res);
  }
  DBUG_RETURN(res);
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
  return thd->lex->context_analysis_only ? FALSE : const_item_cache;
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
  init(select_lex, new select_max_min_finder_subselect(this, max_arg));
  max_columns= 1;
  maybe_null= 1;
  max_columns= 1;

  /*
    Following information was collected during performing fix_fields()
    of Items belonged to subquery, which will be not repeated
  */
  used_tables_cache= parent->get_used_tables_cache();
  const_item_cache= parent->get_const_item_cache();

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
  if (changed)
    return RES_OK;

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
      as far as we moved content to upper level, field which depend of
      'upper' select is not really dependent => we remove this dependence
    */
    substitution->walk(&Item::remove_dependence_processor, 0,
		       (uchar *) select_lex->outer_select());
    return RES_REDUCE;
  }
  return RES_OK;
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
  Item_exists_subselect(), optimizer(0), transformed(0),
  pushed_cond_guards(NULL), upper_item(0)
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  left_expr= left_exp;
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  maybe_null= 1;
  abort_on_null= 0;
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
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
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
  /* We need only 1 row to determine existence */
  unit->global_parameters->select_limit= new Item_int((int32) 1);
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
  str->set((ulonglong)value, &my_charset_bin);
  return str;
}


bool Item_in_subselect::val_bool()
{
  DBUG_ASSERT(fixed == 1);
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
  null_value= was_null= FALSE;
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
   
    If that fails, the subquery will be handled with class Item_in_optimizer, 
    Inject the predicates into subquery, i.e. convert it to:

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

  RETURN
    RES_OK     - OK, either subquery was transformed, or appopriate
                 predicates where injected into it.
    RES_REDUCE - The subquery was reduced to non-subquery
    RES_ERROR  - Error
*/

Item_subselect::trans_res
Item_in_subselect::single_value_transformer(JOIN *join,
					    Comp_creator *func)
{
  SELECT_LEX *select_lex= join->select_lex;
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  /*
    Check that the right part of the subselect contains no more than one
    column. E.g. in SELECT 1 IN (SELECT * ..) the right part is (SELECT * ...)
  */
  if (select_lex->item_list.elements > 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
    DBUG_RETURN(RES_ERROR);
  }

  /*
    If this is an ALL/ANY single-value subselect, try to rewrite it with
    a MIN/MAX subselect. We can do that if a possible NULL result of the
    subselect can be ignored.
    E.g. SELECT * FROM t1 WHERE b > ANY (SELECT a FROM t2) can be rewritten
    with SELECT * FROM t1 WHERE b > (SELECT MAX(a) FROM t2).
    We can't check that this optimization is safe if it's not a top-level
    item of the WHERE clause (e.g. because the WHERE clause can contain IS
    NULL/IS NOT NULL functions). If so, we rewrite ALL/ANY with NOT EXISTS
    later in this method.
  */
  if ((abort_on_null || (upper_item && upper_item->top_level())) &&
      !select_lex->master_unit()->uncacheable && !func->eqne_op())
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
        select_lex->table_list.elements)
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
      *select_lex->ref_pointer_array= item;
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
      thd->lex->allow_sum_func|=
        (nesting_map)1 << thd->lex->current_select->nest_level;
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
      Item_maxmin_subselect *item;
      subs= item= new Item_maxmin_subselect(thd, this, select_lex, func->l_op());
      if (upper_item)
        upper_item->set_sub_test(item);
    }
    /*
      fix fields is already called for  left expression.
      Note that real_item() should be used instead of
      original left expression because left_expr can be
      runtime created Ref item which is deleted at the end
      of the statement. Thus one of 'substitution' arguments
      can be broken in case of PS.
    */ 
    substitution= func->create(left_expr->real_item(), subs);
    DBUG_RETURN(RES_OK);
  }

  if (!substitution)
  {
    /* We're invoked for the 1st (or the only) SELECT in the subquery UNION */
    SELECT_LEX_UNIT *master_unit= select_lex->master_unit();
    substitution= optimizer;

    SELECT_LEX *current= thd->lex->current_select, *up;

    thd->lex->current_select= up= current->return_after_parsing();
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

    master_unit->uncacheable|= UNCACHEABLE_DEPENDENT;
  }
  if (!abort_on_null && left_expr->maybe_null && !pushed_cond_guards)
  {
    if (!(pushed_cond_guards= (bool*)join->thd->alloc(sizeof(bool))))
      DBUG_RETURN(RES_ERROR);
    pushed_cond_guards[0]= TRUE;
  }

  select_lex->uncacheable|= UNCACHEABLE_DEPENDENT;
  if (join->having || select_lex->with_sum_func ||
      select_lex->group_list.elements)
  {
    bool tmp;
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
    
    /*
      AND and comparison functions can't be changed during fix_fields()
      we can assign select_lex->having here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->having= join->having= and_items(join->having, item);
    if (join->having == item)
      item->name= (char*)in_having_cond;
    select_lex->having->top_level_item();
    select_lex->having_fix_field= 1;
    /*
      we do not check join->having->fixed, because Item_and (from and_items)
      or comparison function (from func->create) can't be fixed after creation
    */
    tmp= join->having->fix_fields(thd, 0);
    select_lex->having_fix_field= 0;
    if (tmp)
      DBUG_RETURN(RES_ERROR);
  }
  else
  {
    Item *item= (Item*) select_lex->item_list.head()->real_item();

    if (select_lex->table_list.elements)
    {
      bool tmp;
      Item *having= item, *orig_item= item;
      select_lex->item_list.empty();
      select_lex->item_list.push_back(new Item_int("Not_used",
                                                   (longlong) 1,
                                                   MY_INT64_NUM_DECIMAL_DIGITS));
      select_lex->ref_pointer_array[0]= select_lex->item_list.head();
       
      item= func->create(expr, item);
      if (!abort_on_null && orig_item->maybe_null)
      {
	having= new Item_is_not_null_test(this, having);
        if (left_expr->maybe_null)
        {
          if (!(having= new Item_func_trig_cond(having,
                                                get_cond_guard(0))))
            DBUG_RETURN(RES_ERROR);
        }
	/*
	  Item_is_not_null_test can't be changed during fix_fields()
	  we can assign select_lex->having here, and pass 0 as last
	  argument (reference) to fix_fields()
	*/
        having->name= (char*)in_having_cond;
	select_lex->having= join->having= having;
	select_lex->having_fix_field= 1;
        /*
          we do not check join->having->fixed, because Item_and (from
          and_items) or comparison function (from func->create) can't be
          fixed after creation
        */
	tmp= join->having->fix_fields(thd, 0);
        select_lex->having_fix_field= 0;
        if (tmp)
	  DBUG_RETURN(RES_ERROR);
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
          DBUG_RETURN(RES_ERROR);
      }
      /*
        TODO: figure out why the following is done here in 
        single_value_transformer but there is no corresponding action in
        row_value_transformer?
      */
      item->name= (char *)in_additional_cond;

      /*
	AND can't be changed during fix_fields()
	we can assign select_lex->having here, and pass 0 as last
	argument (reference) to fix_fields()
      */
      select_lex->where= join->conds= and_items(join->conds, item);
      select_lex->where->top_level_item();
      /*
        we do not check join->conds->fixed, because Item_and can't be fixed
        after creation
      */
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
            DBUG_RETURN(RES_ERROR);
        }
        new_having->name= (char*)in_having_cond;
	select_lex->having= join->having= new_having;
	select_lex->having_fix_field= 1;
        
        /*
          we do not check join->having->fixed, because comparison function
          (from func->create) can't be fixed after creation
        */
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
        item->walk(&Item::remove_dependence_processor, 0,
                           (uchar *) select_lex->outer_select());
        item= func->create(left_expr->real_item(), item);
        /*
          fix_field of substitution item will be done in time of
          substituting.
          Note that real_item() should be used instead of
          original left expression because left_expr can be
          runtime created Ref item which is deleted at the end
          of the statement. Thus one of 'substitution' arguments
          can be broken in case of PS.
        */ 
	substitution= item;
	have_to_be_excluded= 1;
	if (thd->lex->describe)
	{
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		       ER_SELECT_REDUCED, warn_buff);
	}
	DBUG_RETURN(RES_REDUCE);
      }
    }
  }

  DBUG_RETURN(RES_OK);
}


Item_subselect::trans_res
Item_in_subselect::row_value_transformer(JOIN *join)
{
  SELECT_LEX *select_lex= join->select_lex;
  Item *having_item= 0;
  uint cols_num= left_expr->cols();
  bool is_having_used= (join->having || select_lex->with_sum_func ||
                        select_lex->group_list.first ||
                        !select_lex->table_list.elements);
  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  if (select_lex->item_list.elements != left_expr->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), left_expr->cols());
    DBUG_RETURN(RES_ERROR);
  }

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *master_unit= select_lex->master_unit();
    substitution= optimizer;

    SELECT_LEX *current= thd->lex->current_select, *up;
    thd->lex->current_select= up= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, 0))
    {
      thd->lex->current_select= current;
      DBUG_RETURN(RES_ERROR);
    }

    // we will refer to upper level cache array => we have to save it in PS
    optimizer->keep_top_level_cache();

    thd->lex->current_select= current;
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

  select_lex->uncacheable|= UNCACHEABLE_DEPENDENT;
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
      DBUG_ASSERT((left_expr->fixed &&
                  select_lex->ref_pointer_array[i]->fixed) ||
                  (select_lex->ref_pointer_array[i]->type() == REF_ITEM &&
                   ((Item_ref*)(select_lex->ref_pointer_array[i]))->ref_type() ==
                    Item_ref::OUTER_REF));
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
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
                                  (char *)"<list ref>")
                        );
      Item *item_isnull=
        new Item_func_isnull(new
                             Item_ref(&select_lex->context,
                                      select_lex->ref_pointer_array+i,
                                      (char *)"<no matter>",
                                      (char *)"<list ref>")
                            );
      Item *col_item= new Item_cond_or(item_eq, item_isnull);
      if (!abort_on_null && left_expr->element_index(i)->maybe_null)
      {
        if (!(col_item= new Item_func_trig_cond(col_item, get_cond_guard(i))))
          DBUG_RETURN(RES_ERROR);
      }
      having_item= and_items(having_item, col_item);
      
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
          DBUG_RETURN(RES_ERROR);
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
      Item *item, *item_isnull;
      DBUG_ASSERT((left_expr->fixed &&
                  select_lex->ref_pointer_array[i]->fixed) ||
                  (select_lex->ref_pointer_array[i]->type() == REF_ITEM &&
                   ((Item_ref*)(select_lex->ref_pointer_array[i]))->ref_type() ==
                    Item_ref::OUTER_REF));
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->element_index(i)->cols()))
        DBUG_RETURN(RES_ERROR);
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
                                         (char *)"<list ref>")
                        );
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
                                           (char *)"<list ref>")
                          );
        item= new Item_cond_or(item, item_isnull);
        /* 
          TODO: why we create the above for cases where the right part
                cant be NULL?
        */
        if (left_expr->element_index(i)->maybe_null)
        {
          if (!(item= new Item_func_trig_cond(item, get_cond_guard(i))))
            DBUG_RETURN(RES_ERROR);
          if (!(having_col_item= 
                  new Item_func_trig_cond(having_col_item, get_cond_guard(i))))
            DBUG_RETURN(RES_ERROR);
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
    if (join->conds->fix_fields(thd, 0))
      DBUG_RETURN(RES_ERROR);
  }
  if (having_item)
  {
    bool res;
    select_lex->having= join->having= and_items(join->having, having_item);
    if (having_item == select_lex->having)
      having_item->name= (char*)in_having_cond;
    select_lex->having->top_level_item();
    /*
      AND can't be changed during fix_fields()
      we can assign select_lex->having here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->having_fix_field= 1;
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
  Query_arena *arena, backup;
  SELECT_LEX *current= thd->lex->current_select, *up;
  const char *save_where= thd->where;
  Item_subselect::trans_res res= RES_ERROR;
  bool result;

  DBUG_ENTER("Item_in_subselect::select_in_like_transformer");

  {
    /*
      IN/SOME/ALL/ANY subqueries aren't support LIMIT clause. Without it
      ORDER BY clause becomes meaningless thus we drop it here.
    */
    SELECT_LEX *sl= current->master_unit()->first_select();
    for (; sl; sl= sl->next_select())
    {
      if (sl->join)
        sl->join->order= 0;
    }
  }

  if (changed)
  {
    DBUG_RETURN(RES_OK);
  }

  thd->where= "IN/ALL/ANY subquery";

  /*
    In some optimisation cases we will not need this Item_in_optimizer
    object, but we can't know it here, but here we need address correct
    reference on left expresion.
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

  thd->lex->current_select= up= current->return_after_parsing();
  result= (!left_expr->fixed &&
           left_expr->fix_fields(thd, optimizer->arguments()));
  /* fix_fields can change reference to left_expr, we need reassign it */
  left_expr= optimizer->arguments()[0];

  thd->lex->current_select= current;
  if (result)
    goto err;

  transformed= 1;
  arena= thd->activate_stmt_arena_if_needed(&backup);

  /*
    Both transformers call fix_fields() only for Items created inside them,
    and all that items do not make permanent changes in current item arena
    which allow to us call them with changed arena (if we do not know nature
    of Item, we have to call fix_fields() for it only with original arena to
    avoid memory leack)
  */
  if (left_expr->cols() == 1)
    res= single_value_transformer(join, func);
  else
  {
    /* we do not support row operation for ALL/ANY/SOME */
    if (func != &eq_creator)
    {
      if (arena)
        thd->restore_active_arena(arena, &backup);
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
      DBUG_RETURN(RES_ERROR);
    }
    res= row_value_transformer(join);
  }
  if (arena)
    thd->restore_active_arena(arena, &backup);
err:
  thd->where= save_where;
  DBUG_RETURN(res);
}


void Item_in_subselect::print(String *str, enum_query_type query_type)
{
  if (transformed)
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
  
  if ((thd_arg->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      left_expr && !left_expr->fixed)
    result = left_expr->fix_fields(thd_arg, &left_expr);

  return result || Item_subselect::fix_fields(thd_arg, ref);
}


Item_subselect::trans_res
Item_allany_subselect::select_transformer(JOIN *join)
{
  transformed= 1;
  if (upper_item)
    upper_item->show= 1;
  return select_in_like_transformer(join, func);
}


void Item_allany_subselect::print(String *str, enum_query_type query_type)
{
  if (transformed)
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


void subselect_engine::set_thd(THD *thd_arg)
{
  thd= thd_arg;
  if (result)
    result->set_thd(thd_arg);
}


subselect_single_select_engine::
subselect_single_select_engine(st_select_lex *select,
			       select_subselect *result_arg,
			       Item_subselect *item_arg)
  :subselect_engine(item_arg, result_arg),
   prepared(0), optimized(0), executed(0), optimize_error(0),
   select_lex(select), join(0)
{
  select_lex->master_unit()->item= item_arg;
}


void subselect_single_select_engine::cleanup()
{
  DBUG_ENTER("subselect_single_select_engine::cleanup");
  prepared= optimized= executed= optimize_error= 0;
  join= 0;
  result->cleanup();
  DBUG_VOID_RETURN;
}


void subselect_union_engine::cleanup()
{
  DBUG_ENTER("subselect_union_engine::cleanup");
  unit->reinit_exec_mechanism();
  result->cleanup();
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
  /*
    subselect_uniquesubquery_engine have not 'result' assigbed, so we do not
    cleanup() it
  */
  DBUG_VOID_RETURN;
}


subselect_union_engine::subselect_union_engine(st_select_lex_unit *u,
					       select_subselect *result_arg,
					       Item_subselect *item_arg)
  :subselect_engine(item_arg, result_arg)
{
  unit= u;
  unit->item= item_arg;
}


int subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
  join= new JOIN(thd, select_lex->item_list,
		 select_lex->options | SELECT_NO_UNLOCK, result);
  if (!join || !result)
    return 1; /* Fatal error is set already. */
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
  //this never should be called
  DBUG_ASSERT(0);
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

void subselect_uniquesubquery_engine::fix_length_and_dec(Item_cache **row)
{
  //this never should be called
  DBUG_ASSERT(0);
}

int  read_first_record_seq(JOIN_TAB *tab);
int rr_sequential(READ_RECORD *info);
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
  if (!optimized)
  {
    SELECT_LEX_UNIT *unit= select_lex->master_unit();

    DBUG_EXECUTE_IF("bug11747970_simulate_error",
                    DBUG_SET("+d,bug11747970_raise_error"););

    optimized= 1;
    unit->set_limit(unit->global_parameters);
    if (join->optimize())
    {
      thd->where= save_where;
      optimize_error= 1;
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
      for (uint i=join->const_tables ; i < join->tables ; i++)
      {
        JOIN_TAB *tab=join->join_tab+i;
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
    }
    executed= 1;
    thd->where= save_where;
    thd->lex->current_select= save_select;
    DBUG_RETURN(join->error||thd->is_fatal_error);
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

  if ((table->file->inited &&
       (error= table->file->ha_index_end())) ||
      (error= table->file->ha_rnd_init(1)))
  {
    (void) report_error(table, error);
    DBUG_RETURN(true);
  }

  table->file->extra_opt(HA_EXTRA_CACHE,
                         current_thd->variables.read_buff_size);
  table->null_row= 0;
  for (;;)
  {
    error=table->file->rnd_next(table->record[0]);
    if (error && error != HA_ERR_END_OF_FILE)
    {
      error= report_error(table, error);
      break;
    }
    /* No more rows */
    if (table->status)
      break;

    if (!cond || cond->val_int())
    {
      empty_result_set= FALSE;
      break;
    }
  }

  table->file->ha_rnd_end();
  DBUG_RETURN(error != 0);
}


/*
  Copy ref key and check for null parts in it

  SYNOPSIS
    subselect_uniquesubquery_engine::copy_ref_key()

  DESCRIPTION
    Copy ref key and check for null parts in it.
    Depending on the nullability and conversion problems this function
    recognizes and processes the following states :
      1. Partial match on top level. This means IN has a value of FALSE
         regardless of the data in the subquery table.
         Detected by finding a NULL in the left IN operand of a top level
         expression.
         We may actually skip reading the subquery, so return TRUE to skip
         the table scan in subselect_uniquesubquery_engine::exec and make
         the value of the IN predicate a NULL (that is equal to FALSE on
         top level).
      2. No exact match when IN is nested inside another predicate.
         Detected by finding a NULL in the left IN operand when IN is not
         a top level predicate.
         We cannot have an exact match. But we must proceed further with a
         table scan to find out if it's a partial match (and IN has a value
         of NULL) or no match (and IN has a value of FALSE).
         So we return FALSE to continue with the scan and see if there are
         any record that would constitute a partial match (as we cannot
         determine that from the index).
      3. Error converting the left IN operand to the column type of the
         right IN operand. This counts as no match (and IN has the value of
         FALSE). We mark the subquery table cursor as having no more rows
         (to ensure that the processing that follows will not find a match)
         and return FALSE, so IN is not treated as returning NULL.


  RETURN
    FALSE - The value of the IN predicate is not known. Proceed to find the
            value of the IN predicate using the determined values of
            null_keypart and table->status.
    TRUE  - IN predicate has a value of NULL. Stop the processing right there
            and return NULL to the outer predicates.
*/

bool subselect_uniquesubquery_engine::copy_ref_key()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::copy_ref_key");

  for (store_key **copy= tab->ref.key_copy ; *copy ; copy++)
  {
    tab->ref.key_err= (*copy)->copy();

    /*
      When there is a NULL part in the key we don't need to make index
      lookup for such key thus we don't need to copy whole key.
      If we later should do a sequential scan return OK. Fail otherwise.

      See also the comment for the subselect_uniquesubquery_engine::exec()
      function.
    */
    null_keypart= (*copy)->null_key;
    if (null_keypart)
    {
      bool top_level= ((Item_in_subselect *) item)->is_top_level_item();
      if (top_level)
      {
        /* Partial match on top level */
        DBUG_RETURN(1);
      }
      else
      {
        /* No exact match when IN is nested inside another predicate */
        break;
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
    if (tab->ref.key_err & 1)
    {
      /*
       Error converting the left IN operand to the column type of the right
       IN operand. 
      */
      tab->table->status= STATUS_NOT_FOUND;
      break;
    }
  }
  DBUG_RETURN(0);
}


/*
  Execute subselect

  SYNOPSIS
    subselect_uniquesubquery_engine::exec()

  DESCRIPTION
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
  NOTE
    
  RETURN
    FALSE - ok
    TRUE  - an error occured while scanning
*/

int subselect_uniquesubquery_engine::exec()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::exec");
  int error;
  TABLE *table= tab->table;
  empty_result_set= TRUE;
  table->status= 0;
 
  /* TODO: change to use of 'full_scan' here? */
  if (copy_ref_key())
    DBUG_RETURN(1);
  if (table->status)
  {
    /* 
      We know that there will be no rows even if we scan. 
      Can be set in copy_ref_key.
    */
    ((Item_in_subselect *) item)->value= 0;
    DBUG_RETURN(0);
  }

  if (null_keypart)
    DBUG_RETURN(scan_table());

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref.key, 0)))
  {
    (void) report_error(table, error);
    DBUG_RETURN(true);
  }

  error= table->file->index_read_map(table->record[0],
                                     tab->ref.key_buff,
                                     make_prev_keypart_map(tab->ref.key_parts),
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


subselect_uniquesubquery_engine::~subselect_uniquesubquery_engine()
{
  /* Tell handler we don't need the index anymore */
  tab->table->file->ha_index_end();
}


/*
  Index-lookup subselect 'engine' - run the subquery

  SYNOPSIS
    subselect_uniquesubquery_engine:exec()
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

int subselect_indexsubquery_engine::exec()
{
  DBUG_ENTER("subselect_indexsubquery_engine::exec");
  int error;
  bool null_finding= 0;
  TABLE *table= tab->table;

  ((Item_in_subselect *) item)->value= 0;
  empty_result_set= TRUE;
  null_keypart= 0;
  table->status= 0;

  if (check_null)
  {
    /* We need to check for NULL if there wasn't a matching value */
    *tab->ref.null_ref_key= 0;			// Search first for not null
    ((Item_in_subselect *) item)->was_null= 0;
  }

  /* Copy the ref key and check for nulls... */
  if (copy_ref_key())
    DBUG_RETURN(1);

  if (table->status)
  {
    /* 
      We know that there will be no rows even if we scan. 
      Can be set in copy_ref_key.
    */
    ((Item_in_subselect *) item)->value= 0;
    DBUG_RETURN(0);
  }

  if (null_keypart)
    DBUG_RETURN(scan_table());

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref.key, 1)))
  {
    (void) report_error(table, error);
    DBUG_RETURN(true);
  }
  error= table->file->index_read_map(table->record[0],
                                     tab->ref.key_buff,
                                     make_prev_keypart_map(tab->ref.key_parts),
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
        error= table->file->index_next_same(table->record[0],
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
  DBUG_ASSERT(select_lex->join != 0); // should be called after fix_fields()
  return select_lex->join->fields_list.elements;
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
  str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  tab->ref.items[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" in "));
  str->append(tab->table->s->table_name.str, tab->table->s->table_name.length);
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

  @retval
    FALSE OK
  @retval
    TRUE  error
*/

bool subselect_single_select_engine::change_result(Item_subselect *si,
                                                 select_subselect *res)
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
                                         select_subselect *res)
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

bool subselect_uniquesubquery_engine::change_result(Item_subselect *si,
                                                  select_subselect *res)
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
  return 0;
}

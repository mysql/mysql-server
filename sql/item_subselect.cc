/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
   subselect Item

SUBSELECT TODO:
   - add function from mysql_select that use JOIN* as parameter to JOIN methods
     (sql_select.h/sql_select.cc)
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}

Item_subselect::Item_subselect():
  Item_result_field(), value_assigned(0), thd(0), substitution(0),
  engine(0), old_engine(0), used_tables_cache(0), have_to_be_excluded(0),
  const_item_cache(1), engine_changed(0), changed(0)
{
  with_subselect= 1;
  reset();
  /*
    item value is NULL if select_subselect not changed this value
    (i.e. some rows will be found returned)
  */
  null_value= 1;
}


void Item_subselect::init(st_select_lex *select_lex,
			  select_subselect *result)
{

  DBUG_ENTER("Item_subselect::init");
  DBUG_PRINT("enter", ("select_lex: 0x%x", (ulong) select_lex));
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
    if (select_lex->next_select())
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

  if (check_stack_overrun(thd, STACK_MIN_SIZE, (gptr)&res))
    return TRUE;

  res= engine->prepare();

  // all transformation is done (used by prepared statements)
  changed= 1;

  if (!res)
  {
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
                          byte *argument)
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
      for (order= (ORDER*) lex->order_list.first ; order; order= order->next)
      {
        if ((*order->item)->walk(processor, walk_subquery, argument))
          return 1;
      }
      for (order= (ORDER*) lex->group_list.first ; order; order= order->next)
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

  res= engine->exec();

  if (engine_changed)
  {
    engine_changed= 0;
    return exec();
  }
  return (res);
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
  return const_item_cache;
}

Item *Item_subselect::get_tmp_table_item(THD *thd)
{
  if (!with_sum_func && !const_item())
    return new Item_field(result_field);
  return copy_or_same(thd);
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


void Item_subselect::print(String *str)
{
  str->append('(');
  engine->print(str);
  str->append(')');
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


void Item_maxmin_subselect::print(String *str)
{
  str->append(max?"<max>":"<min>", 5);
  Item_singlerow_subselect::print(str);
}


void Item_singlerow_subselect::reset()
{
  null_value= 1;
  if (value)
    value->null_value= 1;
}


Item_subselect::trans_res
Item_singlerow_subselect::select_transformer(JOIN *join)
{
  if (changed)
    return RES_OK;

  SELECT_LEX *select_lex= join->select_lex;
  Query_arena *arena= thd->stmt_arena;
 
  if (!select_lex->master_unit()->first_select()->next_select() &&
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
		       (byte *) select_lex->outer_select());
    /* SELECT without FROM clause can't have WHERE or HAVING clause */
    DBUG_ASSERT(join->conds == 0 && join->having == 0);
    return RES_REDUCE;
  }
  return RES_OK;
}


void Item_singlerow_subselect::store(uint i, Item *item)
{
  row[i]->store(item);
}

enum Item_result Item_singlerow_subselect::result_type() const
{
  return engine->type();
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
  exec();
}

double Item_singlerow_subselect::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!exec() && !value->null_value)
  {
    null_value= 0;
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
    null_value= 0;
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
    null_value= 0;
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
    null_value= 0;
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
    null_value= 0;
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
  null_value= 0; //can't be NULL
  maybe_null= 0; //can't be NULL
  value= 0;
  DBUG_VOID_RETURN;
}


void Item_exists_subselect::print(String *str)
{
  str->append(STRING_WITH_LEN("exists"));
  Item_subselect::print(str);
}


bool Item_in_subselect::test_limit(SELECT_LEX_UNIT *unit)
{
  if (unit->fake_select_lex &&
      unit->fake_select_lex->test_limit())
    return(1);

  SELECT_LEX *sl= unit->first_select();
  for (; sl; sl= sl->next_select())
  {
    if (sl->test_limit())
      return(1);
  }
  return(0);
}

Item_in_subselect::Item_in_subselect(Item * left_exp,
				     st_select_lex *select_lex):
  Item_exists_subselect(), optimizer(0), transformed(0), upper_item(0)
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

String *Item_exists_subselect::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
  str->set((ulonglong)value,&my_charset_bin);
  return str;
}


my_decimal *Item_exists_subselect::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  if (exec())
  {
    reset();
    return 0;
  }
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
    null_value= 1;
    return 0;
  }
  if (was_null && !value)
    null_value= 1;
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
    null_value= 1;
    return 0;
  }
  if (was_null && !value)
    null_value= 1;
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
    null_value= 1;
    return 0;
  }
  if (was_null && !value)
  {
    null_value= 1;
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
    null_value= 1;
    return 0;
  }
  if (was_null && !value)
    null_value= 1;
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
    null_value= 1;
    return 0;
  }
  if (was_null && !value)
    null_value= 1;
  int2my_decimal(E_DEC_FATAL_ERROR, value, 0, decimal_value);
  return decimal_value;
}


/* Rewrite a single-column IN/ALL/ANY subselect. */

Item_subselect::trans_res
Item_in_subselect::single_value_transformer(JOIN *join,
					    Comp_creator *func)
{
  Item_subselect::trans_res result= RES_ERROR;
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
      count_field_types(&join->tmp_table_param, join->all_fields, 0);

      subs= new Item_singlerow_subselect(select_lex);
    }
    else
    {
      Item_maxmin_subselect *item;
      subs= item= new Item_maxmin_subselect(thd, this, select_lex, func->l_op());
      if (upper_item)
        upper_item->set_sub_test(item);
    }
    /* fix fields is already called for  left expression */
    substitution= func->create(left_expr, subs);
    DBUG_RETURN(RES_OK);
  }

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
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

    /*
      As far as  Item_ref_in_optimizer do not substitute itself on fix_fields
      we can use same item for all selects.
    */
    expr= new Item_direct_ref(&select_lex->context,
                              (Item**)optimizer->get_cache(),
			      (char *)"<no matter>",
			      (char *)in_left_expr_name);

    unit->uncacheable|= UNCACHEABLE_DEPENDENT;
  }

  select_lex->uncacheable|= UNCACHEABLE_DEPENDENT;
  /*
    Add the left part of a subselect to a WHERE or HAVING clause of
    the right part, e.g. SELECT 1 IN (SELECT a FROM t1)  =>
    SELECT Item_in_optimizer(1, SELECT a FROM t1 WHERE a=1)
    HAVING is used only if the right part contains a SUM function, a GROUP
    BY or a HAVING clause.
  */
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
#ifdef CORRECT_BUT_TOO_SLOW_TO_BE_USABLE
    if (!abort_on_null && left_expr->maybe_null)
      item= new Item_cond_or(new Item_func_isnull(left_expr), item); 
#endif
    /*
      AND and comparison functions can't be changed during fix_fields()
      we can assign select_lex->having here, and pass 0 as last
      argument (reference) to fix_fields()
    */
    select_lex->having= join->having= and_items(join->having, item);
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
    Item *item= (Item*) select_lex->item_list.head();

    if (select_lex->table_list.elements)
    {
      bool tmp;
      Item *having= item, *orig_item= item;
      select_lex->item_list.empty();
      select_lex->item_list.push_back(new Item_int("Not_used",
                                                   (longlong) 1, 21));
      select_lex->ref_pointer_array[0]= select_lex->item_list.head();
      item= func->create(expr, item);
      if (!abort_on_null && orig_item->maybe_null)
      {
	having= new Item_is_not_null_test(this, having);
	/*
	  Item_is_not_null_test can't be changed during fix_fields()
	  we can assign select_lex->having here, and pass 0 as last
	  argument (reference) to fix_fields()
	*/
	select_lex->having=
	  join->having= (join->having ?
			 new Item_cond_and(having, join->having) :
			 having);
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
#ifdef CORRECT_BUT_TOO_SLOW_TO_BE_USABLE
        if (left_expr->maybe_null)
          item= new Item_cond_or(new Item_func_isnull(left_expr), item);
#endif
      }
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
      if (select_lex->master_unit()->first_select()->next_select())
      {
	/*
	  comparison functions can't be changed during fix_fields()
	  we can assign select_lex->having here, and pass 0 as last
	  argument (reference) to fix_fields()
	*/
	select_lex->having=
	  join->having=
	  func->create(expr,
                       new Item_ref_null_helper(&select_lex->context, this,
                                            select_lex->ref_pointer_array,
                                            (char *)"<no matter>",
                                            (char *)"<result>"));

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
	item= func->create(left_expr, item);
	// fix_field of item will be done in time of substituting
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
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
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
    unit->uncacheable|= UNCACHEABLE_DEPENDENT;
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
    */
    Item *item_having_part2= 0;
    for (uint i= 0; i < cols_num; i++)
    {
      DBUG_ASSERT(left_expr->fixed && select_lex->ref_pointer_array[i]->fixed);
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->el(i)->cols()))
        DBUG_RETURN(RES_ERROR);
      Item *item_eq=
        new Item_func_eq(new
                         Item_direct_ref(&select_lex->context,
                                         (*optimizer->get_cache())->
                                         addr(i),
                                         (char *)"<no matter>",
                                         (char *)in_left_expr_name),
                         new
                         Item_direct_ref(&select_lex->context,
                                         select_lex->ref_pointer_array + i,
                                         (char *)"<no matter>",
                                         (char *)"<list ref>")
                        );
      Item *item_isnull=
        new Item_func_isnull(new
                             Item_direct_ref(&select_lex->context,
                                             select_lex->
                                             ref_pointer_array+i,
                                             (char *)"<no matter>",
                                             (char *)"<list ref>")
                            );
      having_item=
        and_items(having_item,
                  new Item_cond_or(item_eq, item_isnull));
      item_having_part2=
        and_items(item_having_part2,
                  new
                  Item_is_not_null_test(this,
                                        new
                                        Item_direct_ref(&select_lex->context,
                                                        select_lex->
                                                        ref_pointer_array + i,
                                                        (char *)"<no matter>",
                                                        (char *)"<list ref>")
                                       )
                 );
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
      DBUG_ASSERT(left_expr->fixed && select_lex->ref_pointer_array[i]->fixed);
      if (select_lex->ref_pointer_array[i]->
          check_cols(left_expr->el(i)->cols()))
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
        having_item=
          and_items(having_item,
                    new
                    Item_is_not_null_test(this,
                                          new
                                          Item_direct_ref(&select_lex->context,
                                                          select_lex->
                                                          ref_pointer_array + i,
                                                          (char *)"<no matter>",
                                                          (char *)"<list ref>")
                                         )
                  );
        item_isnull= new
          Item_func_isnull(new
                           Item_direct_ref(&select_lex->context,
                                           select_lex->
                                           ref_pointer_array+i,
                                           (char *)"<no matter>",
                                           (char *)"<list ref>")
                          );

        item= new Item_cond_or(item, item_isnull);
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


/*
  Prepare IN/ALL/ANY/SOME subquery transformation and call appropriate
  transformation function

  SYNOPSIS
    Item_in_subselect::select_in_like_transformer()
    join    JOIN object of transforming subquery
    func    creator of condition function of subquery

  DESCRIPTION
    To decide which transformation procedure (scalar or row) applicable here
    we have to call fix_fields() for left expression to be able to call
    cols() method on it. Also this method make arena management for
    underlying transformation methods.

  RETURN
    RES_OK      OK
    RES_REDUCE  OK, and current subquery was reduced during transformation
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


void Item_in_subselect::print(String *str)
{
  if (transformed)
    str->append(STRING_WITH_LEN("<exists>"));
  else
  {
    left_expr->print(str);
    str->append(STRING_WITH_LEN(" in "));
  }
  Item_subselect::print(str);
}


bool Item_in_subselect::fix_fields(THD *thd, Item **ref)
{
  bool result = 0;
  
  if(thd->lex->view_prepare_mode && left_expr && !left_expr->fixed)
    result = left_expr->fix_fields(thd, &left_expr);

  return result || Item_subselect::fix_fields(thd, ref);
}


Item_subselect::trans_res
Item_allany_subselect::select_transformer(JOIN *join)
{
  transformed= 1;
  if (upper_item)
    upper_item->show= 1;
  return select_in_like_transformer(join, func);
}


void Item_allany_subselect::print(String *str)
{
  if (transformed)
    str->append(STRING_WITH_LEN("<exists>"));
  else
  {
    left_expr->print(str);
    str->append(' ');
    str->append(func->symbol(all));
    str->append(all ? " all " : " any ", 5);
  }
  Item_subselect::print(str);
}


void subselect_engine::set_thd(THD *thd_arg)
{
  thd= thd_arg;
  if (result)
    result->set_thd(thd_arg);
}


subselect_single_select_engine::
subselect_single_select_engine(st_select_lex *select,
			       select_subselect *result,
			       Item_subselect *item)
  :subselect_engine(item, result),
   prepared(0), optimized(0), executed(0),
   select_lex(select), join(0)
{
  select_lex->master_unit()->item= item;
}


void subselect_single_select_engine::cleanup()
{
  DBUG_ENTER("subselect_single_select_engine::cleanup");
  prepared= optimized= executed= 0;
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
  if (!result_arg)				//out of memory
    current_thd->fatal_error();
  unit->item= item_arg;
}


int subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
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
		    (TABLE_LIST*) select_lex->table_list.first,
		    select_lex->with_wild,
		    select_lex->where,
		    select_lex->order_list.elements +
		    select_lex->group_list.elements,
		    (ORDER*) select_lex->order_list.first,
		    (ORDER*) select_lex->group_list.first,
		    select_lex->having,
		    (ORDER*) 0, select_lex,
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

static Item_result set_row(List<Item> &item_list, Item *item,
			   Item_cache **row, bool *maybe_null)
{
  Item_result res_type= STRING_RESULT;
  Item *sel_item;
  List_iterator_fast<Item> li(item_list);
  for (uint i= 0; (sel_item= li++); i++)
  {
    item->max_length= sel_item->max_length;
    res_type= sel_item->result_type();
    item->decimals= sel_item->decimals;
    *maybe_null= sel_item->maybe_null;
    if (!(row[i]= Item_cache::get_cache(res_type)))
      return STRING_RESULT; // we should return something
    row[i]->setup(sel_item);
  }
  if (item_list.elements > 1)
    res_type= ROW_RESULT;
  return res_type;
}

void subselect_single_select_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || select_lex->item_list.elements==1);
  res_type= set_row(select_lex->item_list, item, row, &maybe_null);
  item->collation.set(row[0]->collation);
  if (cols() != 1)
    maybe_null= 0;
}

void subselect_union_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || unit->first_select()->item_list.elements==1);

  if (unit->first_select()->item_list.elements == 1)
  {
    res_type= set_row(unit->types, item, row, &maybe_null);
    item->collation.set(row[0]->collation);
  }
  else
  {
    bool fake= 0;
    res_type= set_row(unit->types, item, row, &fake);
  }
}

void subselect_uniquesubquery_engine::fix_length_and_dec(Item_cache **row)
{
  //this never should be called
  DBUG_ASSERT(0);
}

int subselect_single_select_engine::exec()
{
  DBUG_ENTER("subselect_single_select_engine::exec");
  char const *save_where= thd->where;
  SELECT_LEX *save_select= thd->lex->current_select;
  thd->lex->current_select= select_lex;
  if (!optimized)
  {
    SELECT_LEX_UNIT *unit= select_lex->master_unit();

    optimized= 1;
    unit->set_limit(unit->global_parameters);
    if (join->optimize())
    {
      thd->where= save_where;
      executed= 1;
      thd->lex->current_select= save_select;
      DBUG_RETURN(join->error ? join->error : 1);
    }
    if (item->engine_changed)
    {
      DBUG_RETURN(1);
    }
  }
  if (select_lex->uncacheable && executed)
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
    join->exec();
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


int subselect_uniquesubquery_engine::exec()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::exec");
  int error;
  TABLE *table= tab->table;
  for (store_key **copy=tab->ref.key_copy ; *copy ; copy++)
  {
    if ((tab->ref.key_err= (*copy)->copy()) & 1)
    {
      table->status= STATUS_NOT_FOUND;
      DBUG_RETURN(1);
    }
  }

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 0);
  error= table->file->index_read(table->record[0],
                                 tab->ref.key_buff,
                                 tab->ref.key_length,HA_READ_KEY_EXACT);
  if (error &&
      error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    error= report_error(table, error);
  else
  {
    error= 0;
    table->null_row= 0;
    ((Item_in_subselect *) item)->value= (!table->status &&
                                          (!cond || cond->val_int()) ? 1 :
                                          0);
  }

  DBUG_RETURN(error != 0);
}


subselect_uniquesubquery_engine::~subselect_uniquesubquery_engine()
{
  /* Tell handler we don't need the index anymore */
  tab->table->file->ha_index_end();
}


int subselect_indexsubquery_engine::exec()
{
  DBUG_ENTER("subselect_indexsubselect_engine::exec");
  int error;
  bool null_finding= 0;
  TABLE *table= tab->table;

  ((Item_in_subselect *) item)->value= 0;

  if (check_null)
  {
    /* We need to check for NULL if there wasn't a matching value */
    *tab->ref.null_ref_key= 0;			// Search first for not null
    ((Item_in_subselect *) item)->was_null= 0;
  }

  for (store_key **copy=tab->ref.key_copy ; *copy ; copy++)
  {
    if ((tab->ref.key_err= (*copy)->copy()) & 1)
    {
      table->status= STATUS_NOT_FOUND;
      DBUG_RETURN(1);
    }
  }

  if (!table->file->inited)
    table->file->ha_index_init(tab->ref.key, 1);
  error= table->file->index_read(table->record[0],
                                 tab->ref.key_buff,
                                 tab->ref.key_length,HA_READ_KEY_EXACT);
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
        if (!cond || cond->val_int())
        {
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
  return calc_const_tables((TABLE_LIST *) select_lex->outer_select()->
			   leaf_tables);
}


table_map subselect_union_engine::upper_select_const_tables()
{
  return calc_const_tables((TABLE_LIST *) unit->outer_select()->leaf_tables);
}


void subselect_single_select_engine::print(String *str)
{
  select_lex->print(thd, str);
}


void subselect_union_engine::print(String *str)
{
  unit->print(str);
}


void subselect_uniquesubquery_engine::print(String *str)
{
  str->append(STRING_WITH_LEN("<primary_index_lookup>("));
  tab->ref.items[0]->print(str);
  str->append(STRING_WITH_LEN(" in "));
  str->append(tab->table->s->table_name.str, tab->table->s->table_name.length);
  KEY *key_info= tab->table->key_info+ tab->ref.key;
  str->append(STRING_WITH_LEN(" on "));
  str->append(key_info->name);
  if (cond)
  {
    str->append(STRING_WITH_LEN(" where "));
    cond->print(str);
  }
  str->append(')');
}


void subselect_indexsubquery_engine::print(String *str)
{
  str->append(STRING_WITH_LEN("<index_lookup>("));
  tab->ref.items[0]->print(str);
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
    cond->print(str);
  }
  str->append(')');
}

/*
  change select_result object of engine

  SYNOPSIS
    subselect_single_select_engine::change_result()
    si		new subselect Item
    res		new select_result object

  RETURN
    FALSE OK
    TRUE  error
*/

bool subselect_single_select_engine::change_result(Item_subselect *si,
                                                 select_subselect *res)
{
  item= si;
  result= res;
  return select_lex->join->change_result(result);
}


/*
  change select_result object of engine

  SYNOPSIS
    subselect_single_select_engine::change_result()
    si		new subselect Item
    res		new select_result object

  RETURN
    FALSE OK
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


/*
  change select_result emulation, never should be called

  SYNOPSIS
    subselect_single_select_engine::change_result()
    si		new subselect Item
    res		new select_result object

  RETURN
    FALSE OK
    TRUE  error
*/

bool subselect_uniquesubquery_engine::change_result(Item_subselect *si,
                                                  select_subselect *res)
{
  DBUG_ASSERT(0);
  return TRUE;
}


/*
  Report about presence of tables in subquery

  SYNOPSIS
    subselect_single_select_engine::no_tables()

  RETURN
    TRUE  there are not tables used in subquery
    FALSE there are some tables in subquery
*/
bool subselect_single_select_engine::no_tables()
{
  return(select_lex->table_list.elements == 0);
}


/*
  Report about presence of tables in subquery

  SYNOPSIS
    subselect_union_engine::no_tables()

  RETURN
    TRUE  there are not tables used in subquery
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


/*
  Report about presence of tables in subquery

  SYNOPSIS
    subselect_uniquesubquery_engine::no_tables()

  RETURN
    TRUE  there are not tables used in subquery
    FALSE there are some tables in subquery
*/

bool subselect_uniquesubquery_engine::no_tables()
{
  /* returning value is correct, but this method should never be called */
  return 0;
}

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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

inline Item * and_items(Item* cond, Item *item)
{
  return (cond? (new Item_cond_and(cond, item)) : item);
}

Item_subselect::Item_subselect():
  Item_result_field(), engine_owner(1), value_assigned(0), substitution(0),
  engine(0), used_tables_cache(0), have_to_be_excluded(0),
  const_item_cache(1), engine_changed(0)
{
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
  DBUG_PRINT("subs", ("select_lex 0x%xl", (ulong) select_lex));

  if (select_lex->next_select())
    engine= new subselect_union_engine(select_lex->master_unit(), result,
				       this);
  else
    engine= new subselect_single_select_engine(select_lex, result, this);
  DBUG_VOID_RETURN;
}


Item_subselect::~Item_subselect()
{
  if (engine_owner)
    delete engine;
}

Item_subselect::trans_res
Item_subselect::select_transformer(JOIN *join) 
{
  DBUG_ENTER("Item_subselect::select_transformer");
  DBUG_RETURN(RES_OK);
}


bool Item_subselect::fix_fields(THD *thd_param, TABLE_LIST *tables, Item **ref)
{
  engine->set_thd((thd= thd_param));

  char const *save_where= thd->where;
  int res= engine->prepare();
  if (!res)
  {
    if (substitution)
    {
      (*ref)= substitution;
      substitution->name= name;
      if (have_to_be_excluded)
	engine->exclude();
      substitution= 0;
      fixed= 1;
      thd->where= "checking transformed subquery";
      int ret= (*ref)->fix_fields(thd, tables, ref);
      // We can't substitute aggregate functions (like (SELECT (max(i)))
      if ((*ref)->with_sum_func)
      {
	my_error(ER_INVALID_GROUP_FUNC_USE, MYF(0));
	return 1;
      }
      return ret;
    }
    // Is it one field subselect?
    if (engine->cols() > max_columns)
    {  
      my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
      return 1;
    }
    fix_length_and_dec();
  }
  if (engine->uncacheable())
  {
    const_item_cache= 0;
    used_tables_cache|= RAND_TABLE_BIT;
  }
  fixed= 1;
  thd->where= save_where;
  return res;
}

bool Item_subselect::exec()
{
  int res;
  MEM_ROOT *old_root= my_pthread_getspecific_ptr(MEM_ROOT*, THR_MALLOC);
  if (&thd->mem_root != old_root)
  {
    my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root);
    res= engine->exec();
    my_pthread_setspecific_ptr(THR_MALLOC, old_root);
  }
  else
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
  return (table_map) (engine->dependent() ? used_tables_cache : 0L);
}

bool Item_subselect::const_item() const
{
  return const_item_cache;
}

void Item_subselect::update_used_tables()
{
  if (!engine->uncacheable())
  {
    // did all used tables become ststic?
    if ((used_tables_cache & ~engine->upper_select_const_tables()))
      const_item_cache= 1;
  }
}

Item_singlerow_subselect::Item_singlerow_subselect(st_select_lex *select_lex)
  :Item_subselect(), value(0)
{
  DBUG_ENTER("Item_singlerow_subselect::Item_singlerow_subselect");
  init(select_lex, new select_singlerow_subselect(this));
  max_columns= 1;
  maybe_null= 1;
  max_columns= UINT_MAX;
  DBUG_VOID_RETURN;
}

Item_maxmin_subselect::Item_maxmin_subselect(st_select_lex *select_lex,
					     bool max)
  :Item_singlerow_subselect()
{
  DBUG_ENTER("Item_maxmin_subselect::Item_maxmin_subselect");
  init(select_lex, new select_max_min_finder_subselect(this, max));
  max_columns= 1;
  maybe_null= 1;
  max_columns= 1;
  DBUG_VOID_RETURN;
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
  SELECT_LEX *select_lex= join->select_lex;
  
  if (!select_lex->master_unit()->first_select()->next_select() &&
      !select_lex->table_list.elements &&
      select_lex->item_list.elements == 1 &&
      /*
	We cant change name of Item_field or Item_ref, because it will
	prevent it's correct resolving, but we should save name of
	removed item => we do not make optimization if top item of
	list is field or reference.
	TODO: solve above problem
      */
      !(select_lex->item_list.head()->type() == FIELD_ITEM ||
	select_lex->item_list.head()->type() == REF_ITEM) 
      )
  {
    
    have_to_be_excluded= 1;
    if (join->thd->lex.describe)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
      push_warning(join->thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		   ER_SELECT_REDUCED, warn_buff);
    }
    substitution= select_lex->item_list.head();
    /*
      as far as we moved content to upper leven, field which depend of
      'upper' select is not really dependent => we remove this dependence
    */
    substitution->walk(&Item::remove_dependence_processor,
		       (byte *) select_lex->outer_select());
    if (select_lex->where || select_lex->having)
    {
      Item *cond;
      if (!join->having)
	cond= join->conds;
      else if (!join->conds)
	cond= join->having;
      else
	if (!(cond= new Item_cond_and(join->conds, join->having)))
	  return RES_ERROR;
      if (!(substitution= new Item_func_if(cond, substitution,
					   new Item_null())))
	return RES_ERROR;
    }
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
    if (!(value= Item_cache::get_cache(engine->type())))
      return;
  }
  else
  {
    THD *thd= current_thd;
    if (!(row= (Item_cache**)thd->alloc(sizeof(Item_cache*)*max_columns)))
      return;
    engine->fix_length_and_dec(row);
    value= *row;
  }
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

double Item_singlerow_subselect::val () 
{
  if (!exec() && !value->null_value)
  {
    null_value= 0;
    return value->val();
  }
  else
  {
    reset();
    return 0;
  }
}

longlong Item_singlerow_subselect::val_int () 
{
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

String *Item_singlerow_subselect::val_str (String *str) 
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

Item_exists_subselect::Item_exists_subselect(st_select_lex *select_lex):
  Item_subselect()
{
  DBUG_ENTER("Item_exists_subselect::Item_exists_subselect");
  init(select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  null_value= 0; //can't be NULL
  maybe_null= 0; //can't be NULL
  value= 0;
  // We need only 1 row to determinate existence
  select_lex->master_unit()->global_parameters->select_limit= 1;
  DBUG_VOID_RETURN;
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
  Item_exists_subselect(), upper_not(0)
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
					     compare_func_creator fn,
					     st_select_lex *select_lex)
  :Item_in_subselect()
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  left_expr= left_exp;
  func= fn;
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
}

double Item_exists_subselect::val () 
{
  if (exec())
  {
    reset();
    return 0;
  }
  return (double) value;
}

longlong Item_exists_subselect::val_int () 
{
  if (exec())
  {
    reset();
    return 0;
  }
  return value;
}

String *Item_exists_subselect::val_str(String *str)
{
  if (exec())
  {
    reset();
    return 0;
  }
  str->set(value,default_charset());
  return str;
}

double Item_in_subselect::val () 
{
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

longlong Item_in_subselect::val_int () 
{
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
  str->set(value,default_charset());
  return str;
}


Item_subselect::trans_res
Item_in_subselect::single_value_transformer(JOIN *join,
					    Item *left_expr,
					    compare_func_creator func)
{
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  SELECT_LEX *select_lex= join->select_lex;

  THD *thd= join->thd;
  thd->where= "scalar IN/ALL/ANY subquery";

  if ((abort_on_null || (upper_not && upper_not->top_level())) &&
      !select_lex->master_unit()->dependent &&
      (func == &Item_bool_func2::gt_creator ||
       func == &Item_bool_func2::lt_creator ||
       func == &Item_bool_func2::ge_creator ||
       func == &Item_bool_func2::le_creator))
  {
    Item *subs;
    if (!select_lex->group_list.elements &&
	!select_lex->with_sum_func)
    {
      Item *item;
      subs_type type= substype();
      if (func == &Item_bool_func2::le_creator ||
	  func == &Item_bool_func2::lt_creator)
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
      *select_lex->ref_pointer_array= item;
      select_lex->item_list.empty();
      select_lex->item_list.push_back(item);
    
      if (item->fix_fields(thd, join->tables_list, &item))
      {
	DBUG_RETURN(RES_ERROR);
      }
      subs= new Item_singlerow_subselect(select_lex);
    }
    else
    {
      // remove LIMIT placed  by ALL/ANY subquery
      select_lex->master_unit()->global_parameters->select_limit=
	HA_POS_ERROR;
      subs= new Item_maxmin_subselect(select_lex,
				      (func == &Item_bool_func2::le_creator ||
				       func == &Item_bool_func2::lt_creator));
    }
    // left expression belong to outer select
    SELECT_LEX *current= thd->lex.current_select, *up;
    thd->lex.current_select= up= current->return_after_parsing();
    if (left_expr->fix_fields(thd, up->get_table_list(), 0))
    {
      thd->lex.current_select= current;
      DBUG_RETURN(RES_ERROR);
    }
    thd->lex.current_select= current;
    substitution= (*func)(left_expr, subs);
    DBUG_RETURN(RES_OK);
  }

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
    substitution= optimizer= new Item_in_optimizer(left_expr, this);

    SELECT_LEX *current= thd->lex.current_select, *up;

    thd->lex.current_select= up= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, up->get_table_list(), 0))
    {
      thd->lex.current_select= current;
      DBUG_RETURN(RES_ERROR);
    }
    thd->lex.current_select= current;

    /*
      As far as  Item_ref_in_optimizer do not substitude itself on fix_fields
      we can use same item for all selects.
    */
    expr= new Item_ref((Item**)optimizer->get_cache(), 
		       (char *)"<no matter>",
		       (char *)in_left_expr_name);

    unit->dependent= 1;
  }

  select_lex->dependent= 1;
  Item *item;
  if (select_lex->item_list.elements > 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
    DBUG_RETURN(RES_ERROR);
  }

  item= (Item*) select_lex->item_list.head();

  if (join->having || select_lex->with_sum_func ||
      select_lex->group_list.elements)
  {
    item= (*func)(expr,
		  new Item_ref_null_helper(this,
					   select_lex->ref_pointer_array,
					   (char *)"<ref>",
					   this->full_name()));
    join->having= and_items(join->having, item);
    select_lex->having_fix_field= 1;
    if (join->having->fix_fields(thd, join->tables_list, &join->having))
    {
      select_lex->having_fix_field= 0;
      DBUG_RETURN(RES_ERROR);
    }
    select_lex->having_fix_field= 0;
  }
  else
  {
    select_lex->item_list.empty();
    select_lex->item_list.push_back(new Item_int("Not_used",
						 (longlong) 1, 21));
    select_lex->ref_pointer_array[0]= select_lex->item_list.head();
    if (select_lex->table_list.elements)
    {
      Item *having= item, *isnull= item;
      item= (*func)(expr, item);
      if (!abort_on_null)
      {
	having= new Item_is_not_null_test(this, having);
	join->having= (join->having ?
		       new Item_cond_and(having, join->having) :
		       having);
	select_lex->having_fix_field= 1;
	if (join->having->fix_fields(thd, join->tables_list, &join->having))
	{
	  select_lex->having_fix_field= 0;
	  DBUG_RETURN(RES_ERROR);
	}
	select_lex->having_fix_field= 0;
	item= new Item_cond_or(item,
			       new Item_func_isnull(isnull));
      }
      item->name= (char *)in_additional_cond;
      join->conds= and_items(join->conds, item);
      if (join->conds->fix_fields(thd, join->tables_list, &join->conds))
	DBUG_RETURN(RES_ERROR);
    }
    else
    {
      if (select_lex->master_unit()->first_select()->next_select())
      {
	join->having= (*func)(expr, 
			      new Item_null_helper(this, item,
						   (char *)"<no matter>",
						   (char *)"<result>"));
	select_lex->having_fix_field= 1;
	if (join->having->fix_fields(thd, join->tables_list, &join->having))
	{
	  select_lex->having_fix_field= 0;
	  DBUG_RETURN(RES_ERROR);
	}
	select_lex->having_fix_field= 0;
      }
      else
      {
	// it is single select without tables => possible optimization
	item= (*func)(left_expr, item);
	// fix_field of item will be done in time of substituting 
	substitution= item;
	have_to_be_excluded= 1;
	if (thd->lex.describe)
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
Item_in_subselect::row_value_transformer(JOIN *join,
					 Item *left_expr)
{
  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  THD *thd= join->thd;
  thd->where= "row IN/ALL/ANY subquery";

  SELECT_LEX *select_lex= join->select_lex;

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
    substitution= optimizer= new Item_in_optimizer(left_expr, this);

    SELECT_LEX *current= thd->lex.current_select, *up;
    thd->lex.current_select= up= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, up->get_table_list(), 0))
    {
      thd->lex.current_select= current;
      DBUG_RETURN(RES_ERROR);
    }
    thd->lex.current_select= current;

    unit->dependent= 1;
  }

  uint n= left_expr->cols();

  select_lex->dependent= 1;
  select_lex->setup_ref_array(thd,
			      select_lex->order_list.elements +
			      select_lex->group_list.elements);
  Item *item= 0;
  List_iterator_fast<Item> li(select_lex->item_list);
  for (uint i= 0; i < n; i++)
  {
    Item *func= new Item_ref_null_helper(this, 
					 select_lex->ref_pointer_array+i,
					 (char *) "<no matter>",
					 (char *) "<list ref>");
    func=
      Item_bool_func2::eq_creator(new Item_ref((*optimizer->get_cache())->
					       addr(i), 
					       (char *)"<no matter>",
					       (char *)in_left_expr_name),
				  func);
    item= and_items(item, func);
  }

  if (join->having || select_lex->with_sum_func ||
      select_lex->group_list.first ||
      !select_lex->table_list.elements)
  {
    join->having= and_items(join->having, item);
    select_lex->having_fix_field= 1;
    if (join->having->fix_fields(thd, join->tables_list, &join->having))
    {
      select_lex->having_fix_field= 0;
      DBUG_RETURN(RES_ERROR);
    }
    select_lex->having_fix_field= 0;
  }
  else
  {
    join->conds= and_items(join->conds, item);
    if (join->conds->fix_fields(thd, join->tables_list, &join->having))
      DBUG_RETURN(RES_ERROR);
  }
  DBUG_RETURN(RES_OK);
}


Item_subselect::trans_res
Item_in_subselect::select_transformer(JOIN *join)
{
  if (left_expr->cols() == 1)
    return single_value_transformer(join, left_expr,
				    &Item_bool_func2::eq_creator);
  return row_value_transformer(join, left_expr);
}


Item_subselect::trans_res
Item_allany_subselect::select_transformer(JOIN *join)
{
  return single_value_transformer(join, left_expr, func);
}

subselect_single_select_engine::
subselect_single_select_engine(st_select_lex *select,
			       select_subselect *result,
			       Item_subselect *item)
  :subselect_engine(item, result),
   prepared(0), optimized(0), executed(0), join(0)
{
  select_lex= select;
  SELECT_LEX_UNIT *unit= select_lex->master_unit();
  unit->offset_limit_cnt= unit->global_parameters->offset_limit;
  unit->select_limit_cnt= unit->global_parameters->select_limit+
    unit->global_parameters ->offset_limit;
  if (unit->select_limit_cnt < unit->global_parameters->select_limit)
    unit->select_limit_cnt= HA_POS_ERROR;		// no limit
  if (unit->select_limit_cnt == HA_POS_ERROR)
    select_lex->options&= ~OPTION_FOUND_ROWS;
  unit->item= item;
  this->select_lex= select_lex;
}


subselect_union_engine::subselect_union_engine(st_select_lex_unit *u,
					       select_subselect *result,
					       Item_subselect *item)
  :subselect_engine(item, result)
{
  unit= u;
  if (!result)
    //out of memory
    current_thd->fatal_error();
  unit->item= item;
}

int subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
  join= new JOIN(thd, select_lex->item_list, select_lex->options, result);
  if (!join || !result)
  {
    //out of memory
    thd->fatal_error();
    return 1;
  }
  prepared= 1;
  SELECT_LEX *save_select= thd->lex.current_select;
  thd->lex.current_select= select_lex;
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
		    select_lex->master_unit(), 0))
    return 1;
  thd->lex.current_select= save_select;
  return 0;
}

int subselect_union_engine::prepare()
{
  return unit->prepare(thd, result, 0);
}

int subselect_uniquesubquery_engine::prepare()
{
  //this never should be called
  DBUG_ASSERT(0);
  return 1;
}

static Item_result set_row(SELECT_LEX *select_lex, Item * item,
			   Item_cache **row, bool *maybe_null)
{
  Item_result res_type= STRING_RESULT;
  Item *sel_item;
  List_iterator_fast<Item> li(select_lex->item_list);
  for (uint i= 0; (sel_item= li++); i++)
  {
    item->max_length= sel_item->max_length;
    res_type= sel_item->result_type();
    item->decimals= sel_item->decimals;
    *maybe_null= sel_item->maybe_null;
    if (row)
    {
      if (!(row[i]= Item_cache::get_cache(res_type)))
	return STRING_RESULT; // we should return something
      row[i]->set_len_n_dec(sel_item->max_length, sel_item->decimals);
      row[i]->collation.set(sel_item->collation);
    }
  }
  if (select_lex->item_list.elements > 1)
    res_type= ROW_RESULT;
  return res_type;
}

void subselect_single_select_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || select_lex->item_list.elements==1);
  res_type= set_row(select_lex, item, row, &maybe_null);
  item->collation.set(row[0]->collation);
  if (cols() != 1)
    maybe_null= 0;
}

void subselect_union_engine::fix_length_and_dec(Item_cache **row)
{
  DBUG_ASSERT(row || unit->first_select()->item_list.elements==1);

  if (unit->first_select()->item_list.elements == 1)
  {
    uint32 mlen= 0, len;
    Item *sel_item= 0;
    for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
    {
      List_iterator_fast<Item> li(sl->item_list);
      Item *s_item= li++;
      if ((len= s_item->max_length) > mlen)
	mlen= len;
      if (!sel_item)
	sel_item= s_item;
      maybe_null= s_item->maybe_null;
    }
    item->max_length= mlen;
    res_type= sel_item->result_type();
    item->decimals= sel_item->decimals;
    if (row)
    {
      if (!(row[0]= Item_cache::get_cache(res_type)))
	return;
      row[0]->set_len_n_dec(mlen, sel_item->decimals);
    }
  }
  else
  {
    SELECT_LEX *sl= unit->first_select();
    bool fake= 0;
    res_type= set_row(sl, item, row, &fake);
    for (sl= sl->next_select(); sl; sl= sl->next_select())
    {
      List_iterator_fast<Item> li(sl->item_list);
      Item *sel_item;
      for (uint i= 0; (sel_item= li++); i++)
      {
	if (sel_item->max_length > row[i]->max_length)
	  row[i]->max_length= sel_item->max_length;
      }
    }
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
  char const *save_where= join->thd->where;
  SELECT_LEX *save_select= join->thd->lex.current_select;
  join->thd->lex.current_select= select_lex;
  if (!optimized)
  {
    optimized=1;
    if (join->optimize())
    {
      join->thd->where= save_where;
      executed= 1;
      join->thd->lex.current_select= save_select;
      DBUG_RETURN(join->error?join->error:1);
    }
    if (item->engine_changed)
    {
      DBUG_RETURN(1);
    }
  }
  if ((select_lex->dependent || select_lex->uncacheable) && executed)
  {
    if (join->reinit())
    {
      join->thd->where= save_where;
      join->thd->lex.current_select= save_select;
      DBUG_RETURN(1);
    }
    item->reset();
    item->assigned((executed= 0));
  }
  if (!executed)
  {
    join->exec();
    executed= 1;
    join->thd->where= save_where;
    join->thd->lex.current_select= save_select;
    DBUG_RETURN(join->error||thd->is_fatal_error);
  }
  join->thd->where= save_where;
  join->thd->lex.current_select= save_select;
  DBUG_RETURN(0);
}

int subselect_union_engine::exec()
{
  char const *save_where= unit->thd->where;
  int res= unit->exec();
  unit->thd->where= save_where;
  return res;
}


int subselect_uniquesubquery_engine::exec()
{
  DBUG_ENTER("subselect_uniquesubquery_engine::exec");
  int error;
  TABLE *table= tab->table;
  if ((tab->ref.key_err= (*tab->ref.key_copy)->copy()))
  {
    table->status= STATUS_NOT_FOUND;
    error= -1;
  }
  else
  {
    error= table->file->index_read(table->record[0],
				   tab->ref.key_buff,
				   tab->ref.key_length,HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND)
      error= report_error(table, error);
    else
    {
      error= 0;
      table->null_row= 0;
      ((Item_in_subselect *) item)->value= (!table->status &&
					    (!cond || cond->val_int()) ? 1 :
					    0);
    }
  }
  DBUG_RETURN(error != 0);
}


subselect_uniquesubquery_engine::~subselect_uniquesubquery_engine()
{
  /* Tell handler we don't need the index anymore */
  tab->table->file->index_end();
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
    *tab->null_ref_key= 0;			// Search first for not null
    ((Item_in_subselect *) item)->was_null= 0;
  }

  if ((*tab->ref.key_copy) && (tab->ref.key_err= (*tab->ref.key_copy)->copy()))
  {
    table->status= STATUS_NOT_FOUND;
    error= -1;
  }
  else
  {
    error= table->file->index_read(table->record[0],
				   tab->ref.key_buff,
				   tab->ref.key_length,HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND)
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
	  *tab->null_ref_key= 1;
	  null_finding= 1;
	  /* Check if there exists a row with a null value in the index */
	  if ((error= (safe_index_read(tab) == 1)))
	    break;
	}
      }
    }
  }
  DBUG_RETURN(error != 0);
}


uint subselect_single_select_engine::cols()
{
  return select_lex->item_list.elements;
}

uint subselect_union_engine::cols()
{
  return unit->first_select()->item_list.elements;
}

bool subselect_single_select_engine::dependent()
{
  return select_lex->dependent;
}

bool subselect_union_engine::dependent()
{
  return unit->dependent;
}

bool subselect_single_select_engine::uncacheable()
{
  return select_lex->uncacheable;
}

bool subselect_union_engine::uncacheable()
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
  for(; table; table= table->next)
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
			   table_list.first);
}

table_map subselect_union_engine::upper_select_const_tables()
{
  return calc_const_tables((TABLE_LIST *) unit->outer_select()->
			   table_list.first);
}

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
   - remove double 'having' & 'having_list' from JOIN
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
  have_to_be_excluded(0)
{
  reset();
  /*
    item value is NULL if select_subselect not changed this value 
    (i.e. some rows will be found returned)
  */
  null_value= 1;
}

void Item_subselect::init(THD *thd, st_select_lex *select_lex,
			  select_subselect *result)
{

  DBUG_ENTER("Item_subselect::init");
  DBUG_PRINT("subs", ("select_lex 0x%xl", (ulong) select_lex));

  if (select_lex->next_select())
    engine= new subselect_union_engine(thd, select_lex->master_unit(), result,
				       this);
  else
    engine= new subselect_single_select_engine(thd, select_lex, result,
					       this);
  DBUG_VOID_RETURN;
}

Item_subselect::~Item_subselect()
{
  if (engine_owner)
    delete engine;
}

Item_subselect::trans_res
Item_subselect::select_transformer(THD *thd,
				   JOIN *join) 
{
  DBUG_ENTER("Item_subselect::select_transformer");
  DBUG_RETURN(OK);
}


bool Item_subselect::fix_fields(THD *thd_param, TABLE_LIST *tables, Item **ref)
{
  thd= thd_param;

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
      my_error(ER_CARDINALITY_COL, MYF(0), 1);
      return 1;
    }
    fix_length_and_dec();
  }
  fixed= 1;
  thd->where= save_where;
  return res;
}

bool Item_subselect::exec()
{
  MEM_ROOT *old_root= my_pthread_getspecific_ptr(MEM_ROOT*, THR_MALLOC);
  if (&thd->mem_root != old_root)
  {
    my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root);
    int res= engine->exec();
    my_pthread_setspecific_ptr(THR_MALLOC, old_root);
    return (res);
  }
  else
    return engine->exec();
}

Item::Type Item_subselect::type() const 
{
  return SUBSELECT_ITEM;
}

void Item_subselect::fix_length_and_dec()
{
  engine->fix_length_and_dec(0);
}

inline table_map Item_subselect::used_tables() const
{
  return (table_map) (engine->dependent() ? 1L :
		      (engine->uncacheable() ? OUTER_REF_TABLE_BIT : 0L));
}

Item_singlerow_subselect::Item_singlerow_subselect(THD *thd,
						   st_select_lex *select_lex):
  Item_subselect(), value(0)
{
  DBUG_ENTER("Item_singlerow_subselect::Item_singlerow_subselect");
  init(thd, select_lex, new select_singlerow_subselect(this));
  max_columns= 1;
  maybe_null= 1;
  max_columns= UINT_MAX;
  DBUG_VOID_RETURN;
}

void Item_singlerow_subselect::reset()
{
  null_value= 1;
  if (value)
    value->null_value= 1;
}

Item_subselect::trans_res
Item_singlerow_subselect::select_transformer(THD *thd,
						    JOIN *join)
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
    if (thd->lex.describe)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_SELECT_REDUCED), select_lex->select_number);
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		   ER_SELECT_REDUCED, warn_buff);
    }
    substitution= select_lex->item_list.head();
    substitution->set_outer_resolving();
     
    if (select_lex->where || select_lex->having)
    {
      Item *cond;
      if (!join->having)
	cond= join->conds;
      else if (!join->conds)
	cond= join->having;
      else
	if (!(cond= new Item_cond_and(join->conds, join->having)))
	  return ERROR;
      if (!(substitution= new Item_func_if(cond, substitution,
					   new Item_null())))
	return ERROR;
    }
    return REDUCE;
  }
  return OK;
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
    my_error(ER_CARDINALITY_COL, MYF(0), c);
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

Item_exists_subselect::Item_exists_subselect(THD *thd,
					     st_select_lex *select_lex):
  Item_subselect()
{
  DBUG_ENTER("Item_exists_subselect::Item_exists_subselect");
  init(thd, select_lex, new select_exists_subselect(this));
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
  SELECT_LEX_NODE *global= unit->global_parameters;
  if (global->select_limit != HA_POS_ERROR)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
	     "LIMIT & IN/ALL/ANY/SOME subquery");
    return(1);
  }
  SELECT_LEX *sl= unit->first_select();
  for (; sl; sl= sl->next_select())
  {
    if (sl->select_limit != HA_POS_ERROR)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0),
	       "LIMIT & IN/ALL/ANY/SOME subquery");
      return(1);
    }
    // We need only 1 row to determinate existence
    sl->select_limit= 1;
    // no sense in ORDER BY without LIMIT
    sl->order_list.empty();
  }
  // no sense in ORDER BY without LIMIT
  global->order_list.empty();
  // We need only 1 row to determinate existence
  global->select_limit= 1;

  return(0);
}

Item_in_subselect::Item_in_subselect(THD *thd, Item * left_exp,
				     st_select_lex *select_lex):
  Item_exists_subselect()
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  left_expr= left_exp;
  init(thd, select_lex, new select_exists_subselect(this));
  max_columns= UINT_MAX;
  maybe_null= 1;
  abort_on_null= 0;
  reset();
  test_limit(select_lex->master_unit());
  DBUG_VOID_RETURN;
}

Item_allany_subselect::Item_allany_subselect(THD *thd, Item * left_exp,
					     compare_func_creator fn,
					     st_select_lex *select_lex):
  Item_in_subselect()
{
  DBUG_ENTER("Item_in_subselect::Item_in_subselect");
  left_expr= left_exp;
  func= fn;
  init(thd, select_lex, new select_exists_subselect(this));
  max_columns= 1;
  abort_on_null= 0;
  reset();
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

Item_in_subselect::Item_in_subselect(Item_in_subselect *item):
  Item_exists_subselect(item)
{
  left_expr= item->left_expr;
  abort_on_null= item->abort_on_null;
}

Item_allany_subselect::Item_allany_subselect(Item_allany_subselect *item):
  Item_in_subselect(item)
{
  func= item->func;
}

Item_subselect::trans_res
Item_in_subselect::single_value_transformer(THD *thd,
					    JOIN *join,
					    Item *left_expr,
					    compare_func_creator func)
{
  DBUG_ENTER("Item_in_subselect::single_value_transformer");

  SELECT_LEX *select_lex= join->select_lex;

  thd->where= "scalar IN/ALL/ANY subquery";

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
    substitution= optimizer= new Item_in_optimizer(left_expr, this);

    SELECT_LEX_NODE *current= thd->lex.current_select, *up;
    thd->lex.current_select= up= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, up->get_table_list(), 0))
    {
      thd->lex.current_select= current;
      DBUG_RETURN(ERROR);
    }
    thd->lex.current_select= current;

    /*
      As far as  Item_ref_in_optimizer do not substitude itself on fix_fields
      we can use same item for all selects.
    */
    expr= new Item_ref((Item**)optimizer->get_cache(), 
		       (char *)"<no matter>",
		       (char *)"<left expr>");

    unit->dependent= 1;
  }

  select_lex->dependent= 1;
  Item *item;
  if (select_lex->item_list.elements > 1)
  {
    my_error(ER_CARDINALITY_COL, MYF(0), 1);
    DBUG_RETURN(ERROR);
  }
  else
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
      DBUG_RETURN(ERROR);
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
      if (item->type() == Item::FIELD_ITEM &&
	  ((Item_field*) item)->field_name[0] == '*')
      {
	Item_asterisk_remover *remover;
	item= remover= new Item_asterisk_remover(this, item,
						 (char *)"<no matter>",
						 (char *)"<result>");
	if (!abort_on_null)
	{
	  having= 
	    new Item_is_not_null_test(this,
				      new Item_ref(remover->storage(),
						   (char *)"<no matter>",
						   (char *)"<null test>"));
	  isnull=
	    new Item_is_not_null_test(this,
				      new Item_ref(remover->storage(),
						   (char *)"<no matter>",
						   (char *)"<null test>"));
	}
      }
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
	  DBUG_RETURN(ERROR);
	}
	select_lex->having_fix_field= 0;
	item= new Item_cond_or(item,
			       new Item_func_isnull(isnull));
      }
      join->conds= and_items(join->conds, item);
      if (join->conds->fix_fields(thd, join->tables_list, &join->conds))
	DBUG_RETURN(ERROR);
    }
    else
    {
      if (item->type() == Item::FIELD_ITEM &&
	  ((Item_field*) item)->field_name[0] == '*')
      {
	my_error(ER_NO_TABLES_USED, MYF(0));
	DBUG_RETURN(ERROR);
      }
      if (select_lex->master_unit()->first_select()->next_select())
      {
	/* 
	   It is in union => we should perform it.
	   Item_asterisk_remover used only as wrapper to receine NULL value
	*/
	join->having= (*func)(expr, 
			      new Item_asterisk_remover(this, item,
							(char *)"<no matter>",
							(char *)"<result>"));
	select_lex->having_fix_field= 1;
	if (join->having->fix_fields(thd, join->tables_list, &join->having))
	{
	  select_lex->having_fix_field= 0;
	  DBUG_RETURN(ERROR);
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
	DBUG_RETURN(REDUCE);
      }
    }
  }
  DBUG_RETURN(OK);
}

Item_subselect::trans_res
Item_in_subselect::row_value_transformer(THD *thd,
					      JOIN *join,
					      Item *left_expr)
{
  DBUG_ENTER("Item_in_subselect::row_value_transformer");

  thd->where= "row IN/ALL/ANY subquery";

  SELECT_LEX *select_lex= join->select_lex;

  if (!substitution)
  {
    //first call for this unit
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
    substitution= optimizer= new Item_in_optimizer(left_expr, this);

    SELECT_LEX_NODE *current= thd->lex.current_select, *up;
    thd->lex.current_select= up= current->return_after_parsing();
    //optimizer never use Item **ref => we can pass 0 as parameter
    if (!optimizer || optimizer->fix_left(thd, up->get_table_list(), 0))
    {
      thd->lex.current_select= current;
      DBUG_RETURN(ERROR);
    }
    thd->lex.current_select= current;

    unit->dependent= 1;
  }

  uint n= left_expr->cols();

  select_lex->dependent= 1;

  Item *item= 0;
  List_iterator_fast<Item> li(select_lex->item_list);
  for (uint i= 0; i < n; i++)
  {
    Item *func=
      new Item_ref_on_list_position(this, select_lex, i,
				    (char *) "<no matter>",
				    (char *) "<list ref>");
    func=
      Item_bool_func2::eq_creator(new Item_ref((*optimizer->get_cache())->
					       addr(i), 
					       (char *)"<no matter>",
					       (char *)"<left expr>"),
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
      DBUG_RETURN(ERROR);
    }
    select_lex->having_fix_field= 0;
  }
  else
  {
    join->conds= and_items(join->conds, item);
    if (join->conds->fix_fields(thd, join->tables_list, &join->having))
      DBUG_RETURN(ERROR);
  }
  DBUG_RETURN(OK);
}

Item_subselect::trans_res
Item_in_subselect::select_transformer(THD *thd, JOIN *join)
{
  if (left_expr->cols() == 1)
    return single_value_transformer(thd, join, left_expr,
				    &Item_bool_func2::eq_creator);
  else
    return row_value_transformer(thd, join, left_expr);
}

Item_subselect::trans_res
Item_allany_subselect::select_transformer(THD *thd,
					  JOIN *join)
{
  return single_value_transformer(thd, join, left_expr, func);
}

subselect_single_select_engine::subselect_single_select_engine(THD *thd, 
							       st_select_lex *select,
							       select_subselect *result,
							       Item_subselect *item):
  subselect_engine(thd, item, result),
    prepared(0), optimized(0), executed(0)
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
  join= new JOIN(thd, select_lex->item_list, select_lex->options, result);
  if (!join || !result)
    //out of memory
    thd->fatal_error();
  unit->item= item;
  this->select_lex= select_lex;
}

subselect_union_engine::subselect_union_engine(THD *thd,
					       st_select_lex_unit *u,
					       select_subselect *result,
					       Item_subselect *item):
  subselect_engine(thd, item, result)
{
  unit= u;
  if (!result)
    //out of memory
    thd->fatal_error();
  unit->item= item;
}

int subselect_single_select_engine::prepare()
{
  if (prepared)
    return 0;
  prepared= 1;
  SELECT_LEX_NODE *save_select= thd->lex.current_select;
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
    for (sl= sl->next_select(); sl; sl->next_select())
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

int subselect_single_select_engine::exec()
{
  DBUG_ENTER("subselect_single_select_engine::exec");
  char const *save_where= join->thd->where;
  if (!optimized)
  {
    optimized=1;
    if (join->optimize())
    {
      join->thd->where= save_where;
      executed= 1;
      DBUG_RETURN(join->error?join->error:1);
    }
  }
  if ((select_lex->dependent || select_lex->uncacheable) && executed)
  {
    if (join->reinit())
    {
      join->thd->where= save_where;
      DBUG_RETURN(1);
    }
    item->reset();
    item->assigned((executed= 0));
  }
  if (!executed)
  {
    SELECT_LEX_NODE *save_select= join->thd->lex.current_select;
    join->thd->lex.current_select= select_lex;
    join->exec();
    join->thd->lex.current_select= save_select;
    executed= 1;
    join->thd->where= save_where;
    DBUG_RETURN(join->error||thd->is_fatal_error);
  }
  join->thd->where= save_where;
  DBUG_RETURN(0);
}

int subselect_union_engine::exec()
{
  char const *save_where= unit->thd->where;
  int res= unit->exec();
  unit->thd->where= save_where;
  return res;
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

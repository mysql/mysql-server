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

   - subselect in HAVING clause
   - add subselect union select (sql_union.cc)
   
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

Item_subselect::Item_subselect(THD *thd, st_select_lex *select_lex,
			       select_subselect *result):
  assigned(0), executed(0), optimized(0), error(0)
{
  DBUG_ENTER("Item_subselect::Item_subselect");
  DBUG_PRINT("subs", ("select_lex 0x%xl", (long) select_lex));
  this->result= result;
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
  {
    //out of memory
    thd->fatal_error= 1;
    my_printf_error(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
  } 
  this->select_lex= select_lex;
  assign_null();
  /*
    item value is NULL if select_subselect not changed this value 
    (i.e. some rows will be found returned)
  */
  null_value= 1;
  DBUG_VOID_RETURN;
}

void Item_subselect::make_field (Send_field *tmp_field)
{
  if (null_value)
  {
    init_make_field(tmp_field,FIELD_TYPE_NULL);
    tmp_field->length=4;
  } else {
    init_make_field(tmp_field, ((result_type() == STRING_RESULT) ?
				FIELD_TYPE_VAR_STRING :
				(result_type() == INT_RESULT) ?
				FIELD_TYPE_LONGLONG : FIELD_TYPE_DOUBLE));
  }
}

bool Item_subselect::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  // Is it one field subselect?
  if (select_lex->item_list.elements > max_columns)
  {  
    my_printf_error(ER_SUBSELECT_NO_1_COL, ER(ER_SUBSELECT_NO_1_COL), MYF(0));
    return 1;
  }
  SELECT_LEX *save_select= thd->lex.select;
  thd->lex.select= select_lex;
  if(join->prepare((TABLE_LIST*) select_lex->table_list.first,
		   select_lex->where,
		   (ORDER*) select_lex->order_list.first,
		   (ORDER*) select_lex->group_list.first,
		   select_lex->having,
		   (ORDER*) 0, select_lex, 
		   select_lex->master_unit()))
    return 1;
  thd->lex.select= save_select;
  return 0;
}

int Item_subselect::exec()
{
  DBUG_ENTER("Item_subselect::exec");
  if (!optimized)
  {
    optimized=1;
    if (join->optimize())
    {
      executed= 1;
      DBUG_RETURN(join->error?join->error:1);
    }
  }
  if (join->select_lex->depended && executed)
  {
    if (join->reinit())
    {
      error= 1;
      DBUG_RETURN(1);
    }
    assign_null();
    executed= assigned= 0;
  }
  if (!executed)
  {
    SELECT_LEX *save_select= join->thd->lex.select;
    join->thd->lex.select= select_lex;
    join->exec();
    join->thd->lex.select= save_select;
    executed= 1;
    DBUG_RETURN(join->error);
  }
  DBUG_RETURN(0);
}

inline table_map Item_subselect::used_tables() const
{
  return (table_map) select_lex->depended ? 1L : 0L; 
}

Item_singleval_subselect::Item_singleval_subselect(THD *thd,
						   st_select_lex *select_lex):
  Item_subselect(thd, select_lex, new select_singleval_subselect(this))
{
  max_columns= 1;
  maybe_null= 1;
}

Item::Type Item_subselect::type() const 
{
  return SUBSELECT_ITEM;
}

double Item_singleval_subselect::val () 
{
  if (exec())
    return 0;
  return real_value;
}

longlong Item_singleval_subselect::val_int () 
{
  if (exec())
    return 0;
  return int_value;
}

String *Item_singleval_subselect::val_str (String *str) 
{
  if (exec() || null_value)
    return 0;
  return &str_value;
}

Item_exists_subselect::Item_exists_subselect(THD *thd,
					     st_select_lex *select_lex):
  Item_subselect(thd, select_lex, new select_exists_subselect(this))
{
  max_columns= UINT_MAX;
  null_value= 0; //can't be NULL
  maybe_null= 0; //can't be NULL
  value= 0;
  select_lex->select_limit= 1; // we need only 1 row to determinate existence
}

double Item_exists_subselect::val () 
{
  if (exec())
    return 0;
  return (double) value;
}

longlong Item_exists_subselect::val_int () 
{
  if (exec())
    return 0;
  return value;
}

String *Item_exists_subselect::val_str(String *str)
{
  if (exec())
    return 0;
  str->set(value);
  return str;
}


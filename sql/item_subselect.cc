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

   - add subselect union select (sql_union.cc)
   - depended from outer select subselects
   
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

Item_subselect::Item_subselect(THD *thd, st_select_lex *select_lex):
  executed(0), optimized(0), error(0)
{
  DBUG_ENTER("Item_subselect::Item_subselect");
  DBUG_PRINT("subs", ("select_lex 0x%xl", (long) select_lex));
  result= new select_subselect(this);
  join= new JOIN(thd, select_lex->item_list, select_lex->options, result);
  this->select_lex= select_lex;
  maybe_null= 1;
  /*
    item value is NULL if select_subselect not changed this value 
    (i.e. some rows will be found returned)
  */
  assign_null();
  DBUG_VOID_RETURN;
}

Item::Type Item_subselect::type() const 
{
  return SUBSELECT_ITEM;
}

double Item_subselect::val () 
{
  if (exec())
    return 0;
  return real_value;
}

longlong Item_subselect::val_int () 
{
  if (exec())
    return 0;
  return int_value;
}

String *Item_subselect::val_str (String *str) 
{
  if (exec() || null_value)
    return 0;
  return &str_value;
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

bool Item_subselect::fix_fields(THD *thd,TABLE_LIST *tables)
{
  // Is it one field subselect?
  if (select_lex->item_list.elements != 1)
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
  if (!optimized)
  {
    optimized=1;
    if (join->optimize())
    {
      executed= 1;
      return (join->error?join->error:1);
    }
  }
  if (join->select_lex->depended && executed)
  {
    if (join->reinit())
    {
      error= 1;
      return 1;
    }
    assign_null();
    executed= 0;
  }
  if (!executed)
  {
    SELECT_LEX *save_select= join->thd->lex.select;
    join->thd->lex.select= select_lex;
    join->exec();
    join->thd->lex.select= save_select;
    if (!executed)
      //No rows returned => value is null (returned as inited)
      executed= 1;
    return join->error;
  }
  return 0;
}

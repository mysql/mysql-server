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
  UNION  of select's
  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/


#include "mysql_priv.h"
#include "sql_select.h"


int mysql_union(THD *thd, LEX *lex,select_result *result)
{
  SELECT_LEX *sl;
  SELECT_LEX_UNIT *unit= &(lex->unit);
  List<Item> item_list;
  TABLE *table;
  int describe=(lex->select_lex.options & SELECT_DESCRIBE) ? 1 : 0;
  int res;
  bool found_rows_for_union=false;
  TABLE_LIST result_table_list;
  TMP_TABLE_PARAM tmp_table_param;
  select_union *union_result;
  DBUG_ENTER("mysql_union");
  st_select_lex_node * global;

  /* Global option */
  if (((void*)(global= unit->global_parameters)) == ((void*)unit))
  {
    found_rows_for_union = lex->select_lex.options & OPTION_FOUND_ROWS && 
      !describe && global->select_limit;
    if (found_rows_for_union)
      lex->select_lex.options ^=  OPTION_FOUND_ROWS;
  }
  
  if (describe)
  {
    Item *item;
    item_list.push_back(new Item_empty_string("table",NAME_LEN));
    item_list.push_back(new Item_empty_string("type",10));
    item_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_LEN*MAX_KEY));
    item->maybe_null=1;
    item_list.push_back(item=new Item_empty_string("key",NAME_LEN));
    item->maybe_null=1;
    item_list.push_back(item=new Item_int("key_len",0,3));
    item->maybe_null=1;
    item_list.push_back(item=new Item_empty_string("ref",
						    NAME_LEN*MAX_REF_PARTS));
    item->maybe_null=1;
    item_list.push_back(new Item_real("rows",0.0,0,10));
    item_list.push_back(new Item_empty_string("Extra",255));
  }
  else
  {
    Item *item;
    List_iterator<Item> it(lex->select_lex.item_list);
    TABLE_LIST *first_table= (TABLE_LIST*) lex->select_lex.table_list.first;

    /* Create a list of items that will be in the result set */
    while ((item= it++))
      if (item_list.push_back(item))
	DBUG_RETURN(-1);
    if (setup_fields(thd,first_table,item_list,0,0,1))
      DBUG_RETURN(-1);
  }

  bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
  tmp_table_param.field_count=item_list.elements;
  if (!(table= create_tmp_table(thd, &tmp_table_param, item_list,
				(ORDER*) 0, !describe & !lex->union_option,
				1, 0,
				(lex->select_lex.options | thd->options |
				 TMP_TABLE_ALL_COLUMNS),
				unit)))
    DBUG_RETURN(-1);
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  bzero((char*) &result_table_list,sizeof(result_table_list));
  result_table_list.db= (char*) "";
  result_table_list.real_name=result_table_list.name=(char*) "union";
  result_table_list.table=table;

  if (!(union_result=new select_union(table)))
  {
    res= -1;
    goto exit;
  }
  union_result->save_time_stamp=!describe;

  for (sl= &lex->select_lex; sl; sl= (SELECT_LEX*) sl->next)
  {
    lex->select=sl;
    unit->offset_limit_cnt= sl->offset_limit;
    unit->select_limit_cnt= sl->select_limit+sl->offset_limit;
    if (unit->select_limit_cnt < sl->select_limit)
      unit->select_limit_cnt= HA_POS_ERROR;		// no limit
    if (unit->select_limit_cnt == HA_POS_ERROR)
      sl->options&= ~OPTION_FOUND_ROWS;

    res= mysql_select(thd, 
		      (TABLE_LIST*) sl->table_list.first,
		      sl->item_list,
		      sl->where,
		      (sl->braces) ? 
		      (ORDER *)sl->order_list.first : (ORDER *) 0,
		      (ORDER*) sl->group_list.first,
		      sl->having,
		      (ORDER*) NULL,
		      sl->options | thd->options | 
		      SELECT_NO_UNLOCK | ((describe) ? SELECT_DESCRIBE : 0),
		      union_result, unit);
    if (res)
      goto exit;
  }
  if (union_result->flush())
  {
    res= 1;					// Error is already sent
    goto exit;
  }
  delete union_result;

  /* Send result to 'result' */
  lex->select = &lex->select_lex;
  res =-1;
  {
    /* Create a list of fields in the temporary table */
    List_iterator<Item> it(item_list);
    Field **field;
#if 0
    List<Item_func_match> ftfunc_list;
    ftfunc_list.empty();
#else
    thd->lex.select_lex.ftfunc_list.empty();
#endif

    for (field=table->field ; *field ; field++)
    {
      (void) it++;
      (void) it.replace(new Item_field(*field));
    }
    if (!thd->fatal_error)			// Check if EOM
    {
      st_select_lex_node * global= unit->global_parameters;
      unit->offset_limit_cnt= global->offset_limit;
      unit->select_limit_cnt= global->select_limit+global->offset_limit;
      if (unit->select_limit_cnt < global->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;		// no limit
      if (unit->select_limit_cnt == HA_POS_ERROR)
	thd->options&= ~OPTION_FOUND_ROWS;
      if (describe)
	unit->select_limit_cnt= HA_POS_ERROR;		// no limit
      res= mysql_select(thd,&result_table_list,
			item_list, NULL,
			(describe) ? 0 : (ORDER*)global->order_list.first,
			(ORDER*) NULL, NULL, (ORDER*) NULL,
			thd->options, result, unit);
      if (found_rows_for_union && !res)
	thd->limit_found_rows = (ulonglong)table->file->records;
    }
  }

exit:
  free_tmp_table(thd,table);
  DBUG_RETURN(res);
}


/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

select_union::select_union(TABLE *table_par)
    :table(table_par)
{
  bzero((char*) &info,sizeof(info));
  /*
    We can always use DUP_IGNORE because the temporary table will only
    contain a unique key if we are using not using UNION ALL
  */
  info.handle_duplicates= DUP_IGNORE;
}

select_union::~select_union()
{
}


int select_union::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;
  if (save_time_stamp && list.elements != table->fields)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
	       ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
    return -1;
  }
  return 0;
}

bool select_union::send_data(List<Item> &values)
{
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  fill_record(table->field,values);
  return write_record(table,&info) ? 1 : 0;
}

bool select_union::send_eof()
{
  return 0;
}

bool select_union::flush()
{
  int error;
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {
    table->file->print_error(error,MYF(0));
    ::send_error(&thd->net);
    return 1;
  }
  return 0;
}

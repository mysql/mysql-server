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
  SELECT_LEX *sl, *last_sl;
  ORDER *order;
  List<Item> item_list;
  TABLE *table;
  TABLE_LIST *first_table, result_table_list;
  TMP_TABLE_PARAM tmp_table_param;
  select_union *union_result;
  int res;
  DBUG_ENTER("mysql_union");

  /* Fix tables 'to-be-unioned-from' list to point at opened tables */
  for (sl=&lex->select_lex; sl; sl=sl->next)
  {
    for (TABLE_LIST *cursor= (TABLE_LIST *)sl->table_list.first;
	 cursor;
	 cursor=cursor->next)
      cursor->table= ((TABLE_LIST*) cursor->table)->table;
  }

  /* Find last select part as it's here ORDER BY and GROUP BY is stored */
  for (last_sl= &lex->select_lex;
       last_sl->next;
       last_sl=last_sl->next) ;

  if (lex->select_lex.options & SELECT_DESCRIBE)
  {
    for (sl= &lex->select_lex; sl; sl=sl->next)
    {
      lex->select=sl;
      res=mysql_select(thd, (TABLE_LIST*) sl->table_list.first,
		       sl->item_list,
		       sl->where,
		       sl->ftfunc_list,
		       (ORDER*) 0,
		       (ORDER*) sl->group_list.first,
		       sl->having,
		       (ORDER*) NULL,
		       (sl->options | thd->options | SELECT_NO_UNLOCK |
			SELECT_DESCRIBE),
		       result);
    }
    DBUG_RETURN(0);
  }

  order = (ORDER *) last_sl->order_list.first;
  {
    Item *item;
    List_iterator<Item> it(lex->select_lex.item_list);

    /* Create a list of items that will be in the result set */
    first_table= (TABLE_LIST*) lex->select_lex.table_list.first;
    while ((item= it++))
      if (item_list.push_back(item))
	DBUG_RETURN(-1);
    if (setup_fields(thd,first_table,item_list,0,0,1))
      DBUG_RETURN(-1);
  }

  bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
  tmp_table_param.field_count=item_list.elements;
  if (!(table=create_tmp_table(thd, &tmp_table_param, item_list,
			       (ORDER*) 0, !lex->union_option,
			       1, 0,
			       (lex->select_lex.options | thd->options |
				TMP_TABLE_ALL_COLUMNS))))
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
  for (sl= &lex->select_lex; sl; sl=sl->next)
  {
    thd->offset_limit=sl->offset_limit;
    thd->select_limit=sl->select_limit+sl->offset_limit;
    if (thd->select_limit < sl->select_limit)
      thd->select_limit= HA_POS_ERROR;		// no limit
    if (thd->select_limit == HA_POS_ERROR)
      sl->options&= ~OPTION_FOUND_ROWS;

    res=mysql_select(thd, (TABLE_LIST*) sl->table_list.first,
		     sl->item_list,
		     sl->where,
		     sl->ftfunc_list,
		     (ORDER*) 0,
		     (ORDER*) sl->group_list.first,
		     sl->having,
		     (ORDER*) NULL,
		     sl->options | thd->options | SELECT_NO_UNLOCK,
		     union_result);
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
  res =-1;
  {
    /* Create a list of fields in the temporary table */
    List_iterator<Item> it(item_list);
    Field **field;
    List<Item_func_match> ftfunc_list;
    ftfunc_list.empty();

    for (field=table->field ; *field ; field++)
    {
      (void) it++;
      (void) it.replace(new Item_field(*field));
    }
    if (!thd->fatal_error)			// Check if EOM
      res=mysql_select(thd,&result_table_list,
		       item_list, NULL, ftfunc_list, order,
		       (ORDER*) NULL, NULL, (ORDER*) NULL,
		       thd->options, result);
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
  info.handle_duplicates=DUP_IGNORE;
}

select_union::~select_union()
{
}


int select_union::prepare(List<Item> &list)
{
  if (list.elements != table->fields)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
	       ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
    return -1;
  }
  return 0;
}

bool select_union::send_data(List<Item> &values)
{
  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
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

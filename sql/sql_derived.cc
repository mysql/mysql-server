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
  Derived tables
  These were introduced by Monty and Sinisa <sinisa@mysql.com>
*/


#include "mysql_priv.h"
#include "sql_select.h"
#include "sql_acl.h"

static const char *any_db="*any*";	// Special symbol for check_access


int mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *unit, TABLE_LIST *t)
{
  /*
    TODO: make derived tables with union inside (now only 1 SELECT may be
    procesed)
  */
  SELECT_LEX *sl= (SELECT_LEX*)unit->slave;
  List<Item> item_list;
  TABLE *table;
  int res;
  select_union *derived_result;
  TABLE_LIST *tables= (TABLE_LIST *)sl->table_list.first;
  TMP_TABLE_PARAM tmp_table_param;
  DBUG_ENTER("mysql_derived");
  
  if (tables)
    res= check_table_access(thd,SELECT_ACL, tables);
  else
    res= check_access(thd, SELECT_ACL, any_db);
  if (res)
    DBUG_RETURN(-1);

  for (TABLE_LIST *cursor= (TABLE_LIST *)tables;
       cursor;
       cursor=cursor->next)
  {
    if (cursor->derived)
    {
      res=mysql_derived(thd, lex, (SELECT_LEX_UNIT *)cursor->derived, cursor);
      if (res) DBUG_RETURN(res);
    }
  }
  Item *item;
  List_iterator<Item> it(sl->item_list);

  while ((item= it++))
    item_list.push_back(item);
    
  if (!(res=open_and_lock_tables(thd,tables)))
  {
    if (tables && setup_fields(thd,tables,item_list,0,0,1))
    {
      res=-1;
      goto exit;
    }
    bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
    tmp_table_param.field_count=item_list.elements;
    if (!(table= create_tmp_table(thd, &tmp_table_param, sl->item_list,
				  (ORDER*) 0, 0, 1, 0,
				  (sl->options | thd->options | 
				   TMP_TABLE_ALL_COLUMNS),
				  unit)))
    {
      res=-1;
      goto exit;
    }
  
    if ((derived_result=new select_union(table)))
    {
      unit->offset_limit_cnt= sl->offset_limit;
      unit->select_limit_cnt= sl->select_limit+sl->offset_limit;
      if (unit->select_limit_cnt < sl->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;
      if (unit->select_limit_cnt == HA_POS_ERROR)
	sl->options&= ~OPTION_FOUND_ROWS;
    
      res=mysql_select(thd, tables,  sl->item_list,
		       sl->where, (ORDER *) sl->order_list.first,
		       (ORDER*) sl->group_list.first,
		       sl->having, (ORDER*) NULL,
		       sl->options | thd->options | SELECT_NO_UNLOCK,
		       derived_result, unit);
      if (!res)
      {
// Here we entirely fix both TABLE_LIST and list of SELECT's as there were no derived tables
	if (derived_result->flush())
	  res=1;
	else
	{
	  t->real_name=table->real_name;
	  t->table=table;
	  sl->exclude();
	  t->derived=(SELECT_LEX *)0; // just in case ...
	}
      }
      delete derived_result;
    }
    if (res)
      free_tmp_table(thd,table);
exit:
    close_thread_tables(thd);
    if (res > 0)
      send_error(&thd->net, ER_UNKNOWN_COM_ERROR); // temporary only ...
  }
  DBUG_RETURN(res);
}

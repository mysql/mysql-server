/* Copyright (C) 2002-2003 MySQL AB

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
  These were introduced by Sinisa <sinisa@mysql.com>
*/


#include "mysql_priv.h"
#include "sql_select.h"
#include "sql_acl.h"

//extern const char *any_db="*any*";	// Special symbol for check_access
extern const char *any_db;

/*
  Resolve derived tables in all queries

  SYNOPSIS
    mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *unit, TABLE_LIST *t)
    thd			Thread handle
    lex                 LEX for this thread
    unit                node that contains all SELECT's for derived tables
    t                   TABLE_LIST for the upper SELECT

  IMPLEMENTATION
    Derived table is resolved with temporary table. It is created based on the
    queries defined. After temporary table is created, if this is not EXPLAIN,
    then the entire unit / node is deleted. unit is deleted if UNION is used 
    for derived table and node is deleted is it is a  simple SELECT.

    After table creation, the above TABLE_LIST is updated with a new table.

    This function is called before any command containing derived table
    is executed.

    Derived tables is stored in thd->derived_tables and freed in
    close_thread_tables()

  TODO
    Move creation of derived tables in open_and_lock_tables()

  RETURN
    0	ok
    1	Error
    -1	Error and error message given
*/  


int mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *unit, TABLE_LIST *t)
{
  SELECT_LEX *sl= unit->first_select();
  List<Item> item_list;
  TABLE *table;
  int res;
  select_union *derived_result;
  TABLE_LIST *tables= (TABLE_LIST *)sl->table_list.first;
  TMP_TABLE_PARAM tmp_table_param;
  bool is_union=sl->next_select() && sl->next_select()->linkage == UNION_TYPE;
  SELECT_LEX_NODE *save_current_select= lex->current_select;
  DBUG_ENTER("mysql_derived");
  
  /*
    In create_total_list, derived tables have to be treated in case of
    EXPLAIN, This is because unit/node is not deleted in that
    case. Current code in this function has to be improved to
    recognize better when this function is called from derived tables
    and when from other functions.
  */
  if (is_union && unit->create_total_list(thd, lex, &tables))
    DBUG_RETURN(-1);

  if (tables)
    res= check_table_access(thd,SELECT_ACL, tables);
  else
    res= check_access(thd, SELECT_ACL, any_db);
  if (res)
    DBUG_RETURN(-1);

  Item *item;
  List_iterator<Item> it(sl->item_list);

  while ((item= it++))
    item_list.push_back(item);
    
  if (!(res=open_and_lock_tables(thd,tables)))
  {
    if (is_union)
    {
      /* 
	 The following code is a re-do of fix_tables_pointers() found
	 in sql_select.cc for UNION's within derived tables. The only
	 difference is in navigation, as in derived tables we care for
	 this level only.

      */
      for (SELECT_LEX *sel= sl; sel; sel= sel->next_select())
	relink_tables(sel);
    }

    lex->current_select= sl;
    if (setup_fields(thd,tables,item_list,0,0,1))
    {
      res= -1;
      goto exit;
    }
    bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
    tmp_table_param.field_count=item_list.elements;
    if (!(table=create_tmp_table(thd, &tmp_table_param, item_list,
			         (ORDER*) 0, 
				 is_union && !unit->union_option, 1,
			         (sl->options | thd->options |
				  TMP_TABLE_ALL_COLUMNS),
                                 HA_POS_ERROR)))
    {
      res= -1;
      goto exit;
    }
  
    if ((derived_result=new select_union(table)))
    {
      derived_result->tmp_table_param=&tmp_table_param;
      unit->offset_limit_cnt= sl->offset_limit;
      unit->select_limit_cnt= sl->select_limit+sl->offset_limit;
      if (unit->select_limit_cnt < sl->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;
      if (unit->select_limit_cnt == HA_POS_ERROR)
	sl->options&= ~OPTION_FOUND_ROWS;

      if (is_union)
	res= mysql_union(thd,lex,derived_result,unit);
      else
	res= mysql_select(thd, tables,  sl->item_list,
			sl->where, (ORDER *) sl->order_list.first,
			(ORDER*) sl->group_list.first,
			sl->having, (ORDER*) NULL,
			sl->options | thd->options  | SELECT_NO_UNLOCK,
			derived_result, unit, sl, 0);

      if (!res)
      {
	/*
	  Here we entirely fix both TABLE_LIST and list of SELECT's as
	  there were no derived tables
	*/
	if (derived_result->flush())
	  res= 1;
	else
	{
	  t->real_name=table->real_name;
	  t->table=table;
	  table->derived_select_number= sl->select_number;
	  table->tmp_table=TMP_TABLE;
	  if (lex->describe)
	  {
	    // to fix a problem in EXPLAIN
	    if (tables)
	      tables->table_list->table=tables->table;
	  }
	  else
	    unit->exclude();
	  t->db= (char *)"";
	  t->derived=(SELECT_LEX *) 1; // just in case ...
	  table->file->info(HA_STATUS_VARIABLE);
	}
      }
      delete derived_result;
    }
    if (res)
      free_tmp_table(thd, table);
    else
    {
      table->next= thd->derived_tables;
      thd->derived_tables= table;
    }
exit:
    lex->current_select= save_current_select;
    close_thread_tables(thd, 0, 1);
  }
  DBUG_RETURN(res);
}

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


int mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *unit,
		  TABLE_LIST *org_table_list)
{
  SELECT_LEX *select_cursor= unit->first_select();
  List<Item> item_list;
  TABLE *table;
  int res;
  select_union *derived_result;
  TABLE_LIST *tables= (TABLE_LIST *)select_cursor->table_list.first;
  TMP_TABLE_PARAM tmp_table_param;
  bool is_union= select_cursor->next_select() && 
    select_cursor->next_select()->linkage == UNION_TYPE;
  bool is_subsel= select_cursor->first_inner_unit() ? 1: 0;
  SELECT_LEX *save_current_select= lex->current_select;
  DBUG_ENTER("mysql_derived");
  
  /*
    In create_total_list, derived tables have to be treated in case of
    EXPLAIN, This is because unit/node is not deleted in that
    case. Current code in this function has to be improved to
    recognize better when this function is called from derived tables
    and when from other functions.
  */
  if ((is_union || is_subsel) && unit->create_total_list(thd, lex, &tables, 1))
    DBUG_RETURN(-1);

  /*
    We have to do access checks here as this code is executed before any
    sql command is started to execute.
  */
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (tables)
    res= check_table_access(thd,SELECT_ACL, tables,0);
  else
    res= check_access(thd, SELECT_ACL, any_db,0,0,0);
  if (res)
    DBUG_RETURN(1);
#endif

  if (!(res=open_and_lock_tables(thd,tables)))
  {
    if (is_union || is_subsel)
    {
      /* 
	 The following code is a re-do of fix_tables_pointers() found
	 in sql_select.cc for UNION's within derived tables. The only
	 difference is in navigation, as in derived tables we care for
	 this level only.

      */
      fix_tables_pointers(unit);
    }

    lex->current_select= select_cursor;
    TABLE_LIST *first_table= (TABLE_LIST*) select_cursor->table_list.first;
    /* Setting up. A must if a join or IGNORE, USE or similar are utilised */
    if (setup_tables(first_table) ||
	setup_wild(thd, first_table, select_cursor->item_list, 0,
		   select_cursor->with_wild))
    {
      res= -1;
      goto exit;
    }

    /* 
       This is done in order to redo all field optimisations when any of the 
       involved tables is used in the outer query 
    */
    if (tables)
    {
      for (TABLE_LIST *cursor= tables;  cursor;  cursor= cursor->next)
	cursor->table->clear_query_id= 1;
    }
	
    item_list= select_cursor->item_list;
    select_cursor->with_wild= 0;
    if (select_cursor->setup_ref_array(thd,
				       select_cursor->order_list.elements +
				       select_cursor->group_list.elements) ||
	setup_fields(thd, select_cursor->ref_pointer_array, first_table,
		     item_list, 0, 0, 1))
    {
      res= -1;
      goto exit;
    }
    // Item list should be fix_fielded yet another time in JOIN::prepare
    unfix_item_list(item_list);

    bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
    tmp_table_param.field_count= item_list.elements;
    /*
      Temp table is created so that it hounours if UNION without ALL is to be 
      processed
    */
    if (!(table= create_tmp_table(thd, &tmp_table_param, item_list,
				  (ORDER*) 0, 
				  is_union && !unit->union_option, 1,
				  (select_cursor->options | thd->options |
				   TMP_TABLE_ALL_COLUMNS),
				  HA_POS_ERROR,
				  org_table_list->alias)))
    {
      res= -1;
      goto exit;
    }
  
    if ((derived_result=new select_union(table)))
    {
      derived_result->tmp_table_param=tmp_table_param;
      unit->offset_limit_cnt= select_cursor->offset_limit;
      unit->select_limit_cnt= select_cursor->select_limit+
	select_cursor->offset_limit;
      if (unit->select_limit_cnt < select_cursor->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;
      if (unit->select_limit_cnt == HA_POS_ERROR)
	select_cursor->options&= ~OPTION_FOUND_ROWS;

      if (is_union)
	res= mysql_union(thd, lex, derived_result, unit, 1);
      else
        res= mysql_select(thd, &select_cursor->ref_pointer_array, 
			  (TABLE_LIST*) select_cursor->table_list.first,
			  select_cursor->with_wild,
			  select_cursor->item_list, select_cursor->where,
			  (select_cursor->order_list.elements+
			   select_cursor->group_list.elements),
			  (ORDER *) select_cursor->order_list.first,
			  (ORDER *) select_cursor->group_list.first,
			  select_cursor->having, (ORDER*) NULL,
			  (select_cursor->options | thd->options |
			   SELECT_NO_UNLOCK),
			  derived_result, unit, select_cursor, 1);

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
	  org_table_list->real_name=table->real_name;
	  org_table_list->table=table;
	  table->derived_select_number= select_cursor->select_number;
	  table->tmp_table= TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
	  org_table_list->grant.privilege= SELECT_ACL;
#endif
	  if (lex->describe)
	  {
	    // to fix a problem in EXPLAIN
	    if (tables)
	    {
	      for (TABLE_LIST *cursor= tables;  cursor;  cursor= cursor->next)
		if (cursor->table_list)
		  cursor->table_list->table=cursor->table;
	    }
	  }
	  else
	    unit->exclude_tree();
	  org_table_list->db= (char *)"";
#if 0
	  /* QQ This was #ifndef DBUG_OFF, but that caused crashes with
	   *    certain subselect args to SPs. Since ->derived is tested
	   *    for non-null value in some places in the code, this seems
	   *    to be the wrong way to do it. Simply letting derived be 0
	   *    appears to work fine. /pem
	   */
	  /* Try to catch errors if this is accessed */
	  org_table_list->derived=(SELECT_LEX_UNIT *) 1;
#endif
	  // Force read of table stats in the optimizer
	  table->file->info(HA_STATUS_VARIABLE);
	}
      }
      delete derived_result;
    }
    if (res)
      free_tmp_table(thd, table);
    else
    {
      /* Add new temporary table to list of open derived tables */
      table->next= thd->derived_tables;
      thd->derived_tables= table;
    }

exit:
    lex->current_select= save_current_select;
    close_thread_tables(thd, 0, 1);
  }
  DBUG_RETURN(res);
}

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
  SELECT_LEX *first_select= unit->first_select();
  TABLE *table;
  int res;
  select_union *derived_result;
  TABLE_LIST *tables= (TABLE_LIST *)first_select->table_list.first;
  bool is_union= first_select->next_select() && 
    first_select->next_select()->linkage == UNION_TYPE;
  bool is_subsel= first_select->first_inner_unit() ? 1: 0;
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

    if (!(derived_result= new select_union(0)))
      DBUG_RETURN(1); // out of memory

    // st_select_lex_unit::prepare correctly work for single select
    if ((res= unit->prepare(thd, derived_result)))
      goto exit;

    /* 
       This is done in order to redo all field optimisations when any of the 
       involved tables is used in the outer query 
    */
    if (tables)
    {
      for (TABLE_LIST *cursor= tables;  cursor;  cursor= cursor->next)
	cursor->table->clear_query_id= 1;
    }
	
    derived_result->tmp_table_param.init();
    derived_result->tmp_table_param.field_count= unit->types.elements;
    /*
      Temp table is created so that it hounours if UNION without ALL is to be 
      processed
    */
    if (!(table= create_tmp_table(thd, &derived_result->tmp_table_param,
				  unit->types, (ORDER*) 0, 
				  is_union && !unit->union_option, 1,
				  (first_select->options | thd->options |
				   TMP_TABLE_ALL_COLUMNS),
				  HA_POS_ERROR,
				  org_table_list->alias)))
    {
      res= -1;
      goto exit;
    }
    derived_result->set_table(table);

    unit->offset_limit_cnt= first_select->offset_limit;
    unit->select_limit_cnt= first_select->select_limit+
      first_select->offset_limit;
    if (unit->select_limit_cnt < first_select->select_limit)
      unit->select_limit_cnt= HA_POS_ERROR;
    if (unit->select_limit_cnt == HA_POS_ERROR)
      first_select->options&= ~OPTION_FOUND_ROWS;

    if (is_union)
      res= mysql_union(thd, lex, derived_result, unit);
    else
      res= mysql_select(thd, &first_select->ref_pointer_array, 
			(TABLE_LIST*) first_select->table_list.first,
			first_select->with_wild,
			first_select->item_list, first_select->where,
			(first_select->order_list.elements+
			 first_select->group_list.elements),
			(ORDER *) first_select->order_list.first,
			(ORDER *) first_select->group_list.first,
			first_select->having, (ORDER*) NULL,
			(first_select->options | thd->options |
			 SELECT_NO_UNLOCK),
			derived_result, unit, first_select);

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
	table->derived_select_number= first_select->select_number;
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
	  // Force read of table stats in the optimizer
	table->file->info(HA_STATUS_VARIABLE);
      }
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
    delete derived_result;
    lex->current_select= save_current_select;
    close_thread_tables(thd, 0, 1);
  }
  DBUG_RETURN(res);
}

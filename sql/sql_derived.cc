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

static int mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *s,
			 TABLE_LIST *t);

/*
  Resolve derived tables in all queries

  SYNOPSIS
    mysql_handle_derived()
    lex                 LEX for this thread

  RETURN
    0	ok
    -1	Error
    1	Error and error message given
*/

int
mysql_handle_derived(LEX *lex)
{
  if (lex->derived_tables)
  {
    for (SELECT_LEX *sl= lex->all_selects_list;
	 sl;
	 sl= sl->next_select_in_list())
    {
      for (TABLE_LIST *cursor= sl->get_table_list();
	   cursor;
	   cursor= cursor->next)
      {
	int res;
	if (cursor->derived && (res=mysql_derived(lex->thd, lex,
						  cursor->derived,
						  cursor)))
	{
	  return res;
	}
      }
      if (lex->describe)
      {
	/*
	  Force join->join_tmp creation, because we will use this JOIN
	  twice for EXPLAIN and we have to have unchanged join for EXPLAINing
	*/
	sl->uncacheable|= UNCACHEABLE_EXPLAIN;
	sl->master_unit()->uncacheable|= UNCACHEABLE_EXPLAIN;
      }
    }
  }
  return 0;
}


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

  RETURN
    0	ok
    1	Error
    -1	Error and error message given
*/  


static int mysql_derived(THD *thd, LEX *lex, SELECT_LEX_UNIT *unit,
			 TABLE_LIST *org_table_list)
{
  SELECT_LEX *first_select= unit->first_select();
  TABLE *table;
  int res;
  select_union *derived_result;
  bool is_union= first_select->next_select() && 
    first_select->next_select()->linkage == UNION_TYPE;
  SELECT_LEX *save_current_select= lex->current_select;
  DBUG_ENTER("mysql_derived");

  if (!(derived_result= new select_union(0)))
    DBUG_RETURN(1); // out of memory

  // st_select_lex_unit::prepare correctly work for single select
  if ((res= unit->prepare(thd, derived_result, 0)))
    goto exit;

	
  derived_result->tmp_table_param.init();
  derived_result->tmp_table_param.field_count= unit->types.elements;
  /*
    Temp table is created so that it hounours if UNION without ALL is to be 
    processed
  */
  if (!(table= create_tmp_table(thd, &derived_result->tmp_table_param,
				unit->types, (ORDER*) 0, 
				is_union && unit->union_distinct, 1,
				(first_select->options | thd->options |
				 TMP_TABLE_ALL_COLUMNS),
				HA_POS_ERROR,
				org_table_list->alias)))
  {
    res= -1;
    goto exit;
  }
  derived_result->set_table(table);

  /*
    if it is preparation PS only then we do not need real data and we
    can skip execution (and parameters is not defined, too)
  */
  if (! thd->current_arena->is_stmt_prepare())
  {
    if (is_union)
    {
      // execute union without clean up
      if (!(res= unit->prepare(thd, derived_result, SELECT_NO_UNLOCK)))
	res= unit->exec();
    }
    else
    {
      unit->offset_limit_cnt= first_select->offset_limit;
      unit->select_limit_cnt= first_select->select_limit+
	first_select->offset_limit;
      if (unit->select_limit_cnt < first_select->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;
      if (unit->select_limit_cnt == HA_POS_ERROR)
	first_select->options&= ~OPTION_FOUND_ROWS;

      lex->current_select= first_select;
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
    }
  }

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
      org_table_list->real_name= table->real_name;
      org_table_list->table= table;
      if (org_table_list->table_list)
      {
	org_table_list->table_list->real_name= table->real_name;
	org_table_list->table_list->table= table;
      }
      table->derived_select_number= first_select->select_number;
      table->tmp_table= TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      table->grant.privilege= SELECT_ACL;
#endif
      org_table_list->db= (char *)"";
      // Force read of table stats in the optimizer
      table->file->info(HA_STATUS_VARIABLE);
    }

    if (!lex->describe)
      unit->cleanup();
    if (res)
      free_tmp_table(thd, table);
    else
    {
      /* Add new temporary table to list of open derived tables */
      table->next= thd->derived_tables;
      thd->derived_tables= table;
    }
  }
  else
    free_tmp_table(thd, table);

exit:
  delete derived_result;
  lex->current_select= save_current_select;
  DBUG_RETURN(res);
}

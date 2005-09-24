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



/*
  Call given derived table processor (preparing or filling tables)

  SYNOPSIS
    mysql_handle_derived()
    lex                 LEX for this thread
    processor           procedure of derived table processing

  RETURN
    0	ok
    1	Error and error message given
*/

int
mysql_handle_derived(LEX *lex, int (*processor)(THD*, LEX*, TABLE_LIST*))
{
  int res= 0;
  if (lex->derived_tables)
  {
    lex->thd->derived_tables_processing= TRUE;
    for (SELECT_LEX *sl= lex->all_selects_list;
	 sl;
	 sl= sl->next_select_in_list())
    {
      for (TABLE_LIST *cursor= sl->get_table_list();
	   cursor;
	   cursor= cursor->next_local)
      {
	if ((res= (*processor)(lex->thd, lex, cursor)))
	  goto out;
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
out:
  lex->thd->derived_tables_processing= FALSE;
  return res;
}


/*
  Create temporary table structure (but do not fill it)

  SYNOPSIS
    mysql_derived_prepare()
    thd			Thread handle
    lex                 LEX for this thread
    orig_table_list     TABLE_LIST for the upper SELECT

  IMPLEMENTATION
    Derived table is resolved with temporary table.

    After table creation, the above TABLE_LIST is updated with a new table.

    This function is called before any command containing derived table
    is executed.

    Derived tables is stored in thd->derived_tables and freed in
    close_thread_tables()

  RETURN
    0	ok
    1	Error and an error message was given
*/

int mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *orig_table_list)
{
  SELECT_LEX_UNIT *unit= orig_table_list->derived;
  int res= 0;
  ulonglong create_options;
  DBUG_ENTER("mysql_derived_prepare");
  if (unit)
  {
    SELECT_LEX *first_select= unit->first_select();
    TABLE *table= 0;
    select_union *derived_result;
    bool is_union= first_select->next_select() && 
      first_select->next_select()->linkage == UNION_TYPE;

    /* prevent name resolving out of derived table */
    for (SELECT_LEX *sl= first_select; sl; sl= sl->next_select())
      sl->context.outer_context= 0;

    if (!(derived_result= new select_union))
      DBUG_RETURN(1); // out of memory

    // st_select_lex_unit::prepare correctly work for single select
    if ((res= unit->prepare(thd, derived_result, 0)))
      goto exit;

    if ((res= check_duplicate_names(unit->types, 0)))
      goto exit;

    create_options= (first_select->options | thd->options |
                     TMP_TABLE_ALL_COLUMNS);
    /*
      Temp table is created so that it hounours if UNION without ALL is to be 
      processed

      As 'distinct' parameter we always pass FALSE (0), because underlying
      query will control distinct condition by itself. Correct test of
      distinct underlying query will be is_union &&
      !unit->union_distinct->next_select() (i.e. it is union and last distinct
      SELECT is last SELECT of UNION).
    */
    if ((res= derived_result->create_result_table(thd, &unit->types, FALSE,
                                                 create_options,
                                                 orig_table_list->alias)))
      goto exit;

    table= derived_result->table;

exit:
    /* Hide "Unknown column" or "Unknown function" error */
    if (orig_table_list->view)
    {
      if (thd->net.last_errno == ER_BAD_FIELD_ERROR ||
          thd->net.last_errno == ER_SP_DOES_NOT_EXIST)
      {
        thd->clear_error();
        my_error(ER_VIEW_INVALID, MYF(0), orig_table_list->db,
                 orig_table_list->table_name);
      }
    }

    /*
      if it is preparation PS only or commands that need only VIEW structure
      then we do not need real data and we can skip execution (and parameters
      is not defined, too)
    */
    if (res)
    {
      if (table)
	free_tmp_table(thd, table);
      delete derived_result;
    }
    else
    {
      if (!thd->fill_derived_tables())
      {
	delete derived_result;
	derived_result= NULL;
      }
      orig_table_list->derived_result= derived_result;
      orig_table_list->table= table;
      orig_table_list->table_name= (char*) table->s->table_name;
      orig_table_list->table_name_length= strlen((char*)table->s->table_name);
      table->derived_select_number= first_select->select_number;
      table->s->tmp_table= TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      table->grant.privilege= SELECT_ACL;
#endif
      orig_table_list->db= (char *)"";
      orig_table_list->db_length= 0;
      // Force read of table stats in the optimizer
      table->file->info(HA_STATUS_VARIABLE);
      /* Add new temporary table to list of open derived tables */
      table->next= thd->derived_tables;
      thd->derived_tables= table;
    }
  }
  else if (orig_table_list->ancestor)
    orig_table_list->set_ancestor();
  DBUG_RETURN(res);
}


/*
  fill derived table

  SYNOPSIS
    mysql_derived_filling()
    thd			Thread handle
    lex                 LEX for this thread
    unit                node that contains all SELECT's for derived tables
    orig_table_list     TABLE_LIST for the upper SELECT

  IMPLEMENTATION
    Derived table is resolved with temporary table. It is created based on the
    queries defined. After temporary table is filled, if this is not EXPLAIN,
    then the entire unit / node is deleted. unit is deleted if UNION is used
    for derived table and node is deleted is it is a  simple SELECT.
    If you use this function, make sure it's not called at prepare.
    Due to evaluation of LIMIT clause it can not be used at prepared stage.

  RETURN
    0	ok
    1   Error and an error message was given
*/

int mysql_derived_filling(THD *thd, LEX *lex, TABLE_LIST *orig_table_list)
{
  TABLE *table= orig_table_list->table;
  SELECT_LEX_UNIT *unit= orig_table_list->derived;
  int res= 0;

  /*check that table creation pass without problem and it is derived table */
  if (table && unit)
  {
    SELECT_LEX *first_select= unit->first_select();
    select_union *derived_result= orig_table_list->derived_result;
    SELECT_LEX *save_current_select= lex->current_select;
    bool is_union= first_select->next_select() &&
      first_select->next_select()->linkage == UNION_TYPE;
    if (is_union)
    {
      // execute union without clean up
      res= unit->exec();
    }
    else
    {
      unit->set_limit(first_select);
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

    if (!res)
    {
      /*
        Here we entirely fix both TABLE_LIST and list of SELECT's as
        there were no derived tables
      */
      if (derived_result->flush())
        res= 1;

      if (!lex->describe)
        unit->cleanup();
    }
    else
      unit->cleanup();
    lex->current_select= save_current_select;
  }
  return res;
}

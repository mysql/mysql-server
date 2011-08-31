/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/*
  Derived tables
  These were introduced by Sinisa <sinisa@mysql.com>
*/


#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"
#include "sql_derived.h"
#include "sql_select.h"
#include "sql_view.h"                         // check_duplicate_names
#include "sql_acl.h"                          // SELECT_ACL


/*
  Call given derived table processor (preparing or filling tables)

  SYNOPSIS
    mysql_handle_derived()
    lex                 LEX for this thread
    processor           procedure of derived table processing

  RETURN
    FALSE  OK
    TRUE   Error
*/

bool
mysql_handle_derived(LEX *lex, bool (*processor)(THD*, LEX*, TABLE_LIST*))
{
  bool res= FALSE;
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


/**
  @brief Create temporary table structure (but do not fill it).

  @param thd Thread handle
  @param lex LEX for this thread
  @param orig_table_list TABLE_LIST for the upper SELECT

  @details 

  This function is called before any command containing derived tables is
  executed. Currently the function is used for derived tables, i.e.

  - Anonymous derived tables, or 
  - Named derived tables (aka views) with the @c TEMPTABLE algorithm.
   
  The table reference, contained in @c orig_table_list, is updated with the
  fields of a new temporary table.

  Derived tables are stored in @c thd->derived_tables and closed by
  close_thread_tables().

  This function is part of the procedure that starts in
  open_and_lock_tables(), a procedure that - among other things - introduces
  new table and table reference objects (to represent derived tables) that
  don't exist in the privilege database. This means that normal privilege
  checking cannot handle them. Hence this function does some extra tricks in
  order to bypass normal privilege checking, by exploiting the fact that the
  current state of privilege verification is attached as GRANT_INFO structures
  on the relevant TABLE and TABLE_REF objects.

  For table references, the current state of accrued access is stored inside
  TABLE_LIST::grant. Hence this function must update the state of fulfilled
  privileges for the new TABLE_LIST, an operation which is normally performed
  exclusively by the table and database access checking functions,
  check_access() and check_grant(), respectively. This modification is done
  for both views and anonymous derived tables: The @c SELECT privilege is set
  as fulfilled by the user. However, if a view is referenced and the table
  reference is queried against directly (see TABLE_LIST::referencing_view),
  the state of privilege checking (GRANT_INFO struct) is copied as-is to the
  temporary table.

  This function implements a signature called "derived table processor", and
  is passed as a function pointer to mysql_handle_derived().

  @note This function sets @c SELECT_ACL for @c TEMPTABLE views as well as
  anonymous derived tables, but this is ok since later access checking will
  distinguish between them.

  @see mysql_handle_derived(), mysql_derived_filling(), GRANT_INFO

  @return
    false  OK
    true   Error
*/

bool mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *orig_table_list)
{
  SELECT_LEX_UNIT *unit= orig_table_list->derived;
  ulonglong create_options;
  DBUG_ENTER("mysql_derived_prepare");
  bool res= FALSE;
  if (unit)
  {
    SELECT_LEX *first_select= unit->first_select();
    TABLE *table= 0;
    select_union *derived_result;

    /* prevent name resolving out of derived table */
    for (SELECT_LEX *sl= first_select; sl; sl= sl->next_select())
      sl->context.outer_context= 0;

    if (!(derived_result= new select_union))
      DBUG_RETURN(TRUE); // out of memory

    lex->context_analysis_only|= CONTEXT_ANALYSIS_ONLY_DERIVED;
    // st_select_lex_unit::prepare correctly work for single select
    if ((res= unit->prepare(thd, derived_result, 0)))
      goto exit;
    lex->context_analysis_only&= ~CONTEXT_ANALYSIS_ONLY_DERIVED;
    if ((res= check_duplicate_names(unit->types, 0)))
      goto exit;

    create_options= (first_select->options | thd->variables.option_bits |
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
      if (thd->is_error() &&
          (thd->stmt_da->sql_errno() == ER_BAD_FIELD_ERROR ||
          thd->stmt_da->sql_errno() == ER_FUNC_INEXISTENT_NAME_COLLISION ||
          thd->stmt_da->sql_errno() == ER_SP_DOES_NOT_EXIST))
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
      orig_table_list->table_name=        table->s->table_name.str;
      orig_table_list->table_name_length= table->s->table_name.length;
      table->derived_select_number= first_select->select_number;
      table->s->tmp_table= NON_TRANSACTIONAL_TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (orig_table_list->referencing_view)
        table->grant= orig_table_list->grant;
      else
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
  else if (orig_table_list->merge_underlying_list)
    orig_table_list->set_underlying_merge();
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
    FALSE  OK
    TRUE   Error
*/

bool mysql_derived_filling(THD *thd, LEX *lex, TABLE_LIST *orig_table_list)
{
  TABLE *table= orig_table_list->table;
  SELECT_LEX_UNIT *unit= orig_table_list->derived;
  bool res= FALSE;

  /*check that table creation pass without problem and it is derived table */
  if (table && unit)
  {
    SELECT_LEX *first_select= unit->first_select();
    select_union *derived_result= orig_table_list->derived_result;
    SELECT_LEX *save_current_select= lex->current_select;
    if (unit->is_union())
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
			first_select->table_list.first,
			first_select->with_wild,
			first_select->item_list, first_select->where,
			(first_select->order_list.elements+
			 first_select->group_list.elements),
			first_select->order_list.first,
			first_select->group_list.first,
			first_select->having, (ORDER*) NULL,
			(first_select->options | thd->variables.option_bits |
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
        res= TRUE;
    }
    lex->current_select= save_current_select;
  }
  return res;
}


/**
   Cleans up the SELECT_LEX_UNIT for the derived table (if any).
*/

bool mysql_derived_cleanup(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->derived;
  if (unit)
    unit->cleanup();
  return false;
}

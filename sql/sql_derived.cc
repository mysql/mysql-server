/* Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


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
      for (TABLE_LIST *table_ref= sl->get_table_list();
	   table_ref;
	   table_ref= table_ref->next_local)
      {
	if (table_ref->is_view_or_derived() &&
            (res= (*processor)(lex->thd, lex, table_ref)))
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
  Run processor on the given derived table.
*/

bool
mysql_handle_single_derived(LEX *lex, TABLE_LIST *derived,
                            bool (*processor)(THD*, LEX*, TABLE_LIST*))
{
  return (derived->is_view_or_derived() &&
          (*processor)(lex->thd, lex, derived));
}

/**
  @brief Create temporary table structure (but do not fill it).

  @param thd Thread handle
  @param lex LEX for this thread
  @param derived TABLE_LIST of the derived table in the upper SELECT

  @details 

  This function is called before any command containing derived tables is
  executed. Currently the function is used for derived tables, i.e.

  - Anonymous derived tables, or 
  - Named derived tables (aka views) with the @c TEMPTABLE algorithm.
   
  The table reference, contained in @c derived, is updated with the
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

bool mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->get_unit();
  ulonglong create_options;
  DBUG_ENTER("mysql_derived_prepare");
  bool res= FALSE;
  DBUG_ASSERT(unit);
  if (derived->is_materialized_derived())
  {
    SELECT_LEX *first_select= unit->first_select();
    TABLE *table= 0;
    select_union *derived_result;

    /* prevent name resolving out of derived table */
    for (SELECT_LEX *sl= first_select; sl; sl= sl->next_select())
      sl->context.outer_context= 0;

    if (!(derived_result= new select_union))
      DBUG_RETURN(TRUE); // out of memory

    // st_select_lex_unit::prepare correctly work for single select
    if ((res= unit->prepare(thd, derived_result, 0)))
      goto exit;

    if ((res= check_duplicate_names(unit->types, 0)))
      goto exit;

    create_options= (first_select->options | thd->variables.option_bits |
                     TMP_TABLE_ALL_COLUMNS);
    /*
      Temp table is created so that it honors if UNION without ALL is to be 
      processed

      As 'distinct' parameter we always pass FALSE (0), because underlying
      query will control distinct condition by itself. Correct test of
      distinct underlying query will be is_union &&
      !unit->union_distinct->next_select() (i.e. it is union and last distinct
      SELECT is last SELECT of UNION).
    */
    if ((res= derived_result->create_result_table(thd, &unit->types, FALSE,
                                                 create_options,
                                                 derived->alias, FALSE, FALSE)))
      goto exit;

    table= derived_result->table;
    derived->materialized= FALSE;
exit:
    /* Hide "Unknown column" or "Unknown function" error */
    if (derived->view)
    {
      if (thd->is_error() &&
          (thd->stmt_da->sql_errno() == ER_BAD_FIELD_ERROR ||
          thd->stmt_da->sql_errno() == ER_FUNC_INEXISTENT_NAME_COLLISION ||
          thd->stmt_da->sql_errno() == ER_SP_DOES_NOT_EXIST))
      {
        thd->clear_error();
        my_error(ER_VIEW_INVALID, MYF(0), derived->db,
                 derived->table_name);
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
      derived->derived_result= derived_result;
      derived->table= table;
      derived->table_name=        table->s->table_name.str;
      derived->table_name_length= table->s->table_name.length;
      table->derived_select_number= first_select->select_number;
      table->s->tmp_table= NON_TRANSACTIONAL_TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (derived->referencing_view)
        table->grant= derived->grant;
      else
        table->grant.privilege= SELECT_ACL;
#endif
      derived->db= (char *)"";
      derived->db_length= 0;
      /* Add new temporary table to list of open derived tables */
      table->next= thd->derived_tables;
      thd->derived_tables= table;
    }
  }
  else
    derived->set_underlying_merge();
  DBUG_RETURN(res);
}


/**
  @brief
  Runs optimize phase for the query expression that represents a derived
  table/view.

  @details
  Runs optimize phase for the query expression that represents a derived
  table/view. If optimizer finds out that the derived table/view is of the type
  "SELECT a_constant" this functions also materializes it.

  @param thd thread handle
  @param lex current LEX
  @param derived TABLE_LIST of derived table

  @return FALSE ok.
  @return TRUE if an error occur.
*/

bool mysql_derived_optimize(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->get_unit();

  DBUG_ASSERT(unit);

  // optimize union without execution
  if (unit->optimize() || thd->is_error())
    goto err;

  if (unit->result->estimated_rowcount <= 1 &&
      (mysql_derived_create(thd, lex, derived) ||
       mysql_derived_materialize(thd, lex, derived)))
    goto err;
  return FALSE;
err:
  return TRUE;
}


/**
  @brief
  Create result table for a materialized derived table/view.

  @param thd     thread handle
  @param lex     LEX of the embedding query.
  @param derived reference to the derived table.

  @details
  This function actually creates the result table for given 'derived'
  table/view, but it doesn't fill it.
  'thd' and 'lex' parameters are not used  by this function.

  @return FALSE ok.
  @return TRUE if an error occur.
*/

bool mysql_derived_create(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  TABLE *table= derived->table;
  SELECT_LEX_UNIT *unit= derived->get_unit();

  DBUG_ASSERT(unit);

  /*
   Don't create result table in following cases:
   *) It's a mergeable view.
   *) Some commands, like show table status, doesn't prepare views/derived
      tables => no need to create result table also.
   *) Table is already created.
  */
  if (!derived->is_materialized_derived() || !table || table->created)
    return FALSE;
  /* create tmp table */
  select_union *result= (select_union*)unit->result;

  if (table->s->db_type() == myisam_hton)
  {
    if (create_myisam_tmp_table(table, table->key_info,
                                  result->tmp_table_param.start_recinfo,
                                  &result->tmp_table_param.recinfo,
                                  (unit->first_select()->options |
                                   thd->lex->select_lex.options |
                                   thd->variables.option_bits |
                                   TMP_TABLE_ALL_COLUMNS),
                                   thd->variables.big_tables))
      return TRUE;
  }
  if (open_tmp_table(table))
    return TRUE;

  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  table->created= TRUE;

  return FALSE;
}


/**
  @brief
  Fill derived table

  @param  thd	    Thread handle
  @param  lex       LEX for this thread
  @param  unit      node that contains all SELECT's for derived tables
  @param  derived   TABLE_LIST for the upper SELECT

  @details
  Derived table is resolved with temporary table. It is created based on the
  queries defined. After temporary table is filled, if this is not EXPLAIN,
  then the entire unit / node is deleted. unit is deleted if UNION is used
  for derived table and node is deleted is it is a  simple SELECT.
  If you use this function, make sure it's not called at prepare.
  Due to evaluation of LIMIT clause it can not be used at prepared stage.

  @return  FALSE  OK
  @return  TRUE   Error
  */

bool mysql_derived_materialize(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  TABLE *table= derived->table;
  SELECT_LEX_UNIT *unit= derived->get_unit();
  bool res= FALSE;

  DBUG_ASSERT(unit && table && table->created);

  if (derived->materialized)
    return FALSE;

  SELECT_LEX *first_select= unit->first_select();
  select_union *derived_result= derived->derived_result;
  SELECT_LEX *save_current_select= lex->current_select;

  if (unit->is_union())
  {
    // execute union without clean up
    res= unit->exec();
  }
  else
  {
    JOIN *join= first_select->join;
    unit->set_limit(first_select);
    if (unit->select_limit_cnt == HA_POS_ERROR)
      first_select->options&= ~OPTION_FOUND_ROWS;

    lex->current_select= first_select;

    DBUG_ASSERT(join && join->optimized);

    if (thd->lex->describe & DESCRIBE_EXTENDED)
    {
      join->conds_history= join->conds;
      join->having_history= (join->having?join->having:join->tmp_having);
    }

    join->exec();

    if (thd->lex->describe & DESCRIBE_EXTENDED)
    {
      first_select->where= join->conds_history;
      first_select->having= join->having_history;
    }
  }

  if (!res)
  {
    /*
      Here we entirely fix both TABLE_LIST and list of SELECT's as
      there were no derived tables
    */
    if (derived_result->flush())
      res= TRUE;

    if (!lex->describe)
        unit->cleanup();
    derived->materialized= TRUE;
  }
  else
    unit->cleanup();
  lex->current_select= save_current_select;

  return res;
}

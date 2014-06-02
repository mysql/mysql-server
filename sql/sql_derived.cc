/*
   Copyright (c) 2002, 2011, Oracle and/or its affiliates.

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
#include "sql_base.h"
#include "sql_view.h"                         // check_duplicate_names
#include "sql_acl.h"                          // SELECT_ACL

typedef bool (*dt_processor)(THD *thd, LEX *lex, TABLE_LIST *derived);

bool mysql_derived_init(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_optimize(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_merge(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_create(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_fill(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_reinit(THD *thd, LEX *lex, TABLE_LIST *derived);
bool mysql_derived_merge_for_insert(THD *thd, LEX *lex, TABLE_LIST *derived);


dt_processor processors[]=
{
  &mysql_derived_init,
  &mysql_derived_prepare,
  &mysql_derived_optimize,
  &mysql_derived_merge,
  &mysql_derived_merge_for_insert,
  &mysql_derived_create,
  &mysql_derived_fill,
  &mysql_derived_reinit,
};

/*
  Run specified phases on all derived tables/views in given LEX.

  @param lex              LEX for this thread
  @param phases           phases to run derived tables/views through

  @return FALSE  OK
  @return TRUE   Error
*/
bool
mysql_handle_derived(LEX *lex, uint phases)
{
  bool res= FALSE;
  THD *thd= lex->thd;
  DBUG_ENTER("mysql_handle_derived");
  DBUG_PRINT("enter", ("phases: 0x%x", phases));
  if (!lex->derived_tables)
    DBUG_RETURN(FALSE);

  lex->thd->derived_tables_processing= TRUE;

  for (uint phase= 0; phase < DT_PHASES && !res; phase++)
  {
    uint phase_flag= DT_INIT << phase;
    if (phase_flag > phases)
      break;
    if (!(phases & phase_flag))
      continue;
    if (phase_flag >= DT_CREATE && !thd->fill_derived_tables())
      break;

    for (SELECT_LEX *sl= lex->all_selects_list;
	 sl && !res;
	 sl= sl->next_select_in_list())
    {
      TABLE_LIST *cursor= sl->get_table_list();
      /*
        DT_MERGE_FOR_INSERT is not needed for views/derived tables inside
        subqueries. Views and derived tables of subqueries should be
        processed normally.
      */
      if (phases == DT_MERGE_FOR_INSERT &&
          cursor && cursor->top_table()->select_lex != &lex->select_lex)
        continue;
      for (;
	   cursor && !res;
	   cursor= cursor->next_local)
      {
        if (!cursor->is_view_or_derived() && phases == DT_MERGE_FOR_INSERT)
          continue;
        uint8 allowed_phases= (cursor->is_merged_derived() ? DT_PHASES_MERGE :
                               DT_PHASES_MATERIALIZE | DT_MERGE_FOR_INSERT);
        /*
          Skip derived tables to which the phase isn't applicable.
          TODO: mark derived at the parse time, later set it's type
          (merged or materialized)
        */
        if ((phase_flag != DT_PREPARE && !(allowed_phases & phase_flag)) ||
            (cursor->merged_for_insert && phase_flag != DT_REINIT &&
             phase_flag != DT_PREPARE))
          continue;
	res= (*processors[phase])(lex->thd, lex, cursor);
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
  lex->thd->derived_tables_processing= FALSE;
  DBUG_RETURN(res);
}

/*
  Run through phases for the given derived table/view.

  @param lex             LEX for this thread
  @param derived         the derived table to handle
  @param phase_map       phases to process tables/views through

  @details

  This function process the derived table (view) 'derived' to performs all
  actions that are to be done on the table at the phases specified by
  phase_map. The processing is carried out starting from the actions
  performed at the earlier phases (those having smaller ordinal numbers).

  @note
  This function runs specified phases of the derived tables handling on the
  given derived table/view. This function is used in the chain of calls:
    SELECT_LEX::handle_derived ->
      TABLE_LIST::handle_derived ->
        mysql_handle_single_derived
  This chain of calls implements the bottom-up handling of the derived tables:
  i.e. most inner derived tables/views are handled first. This order is
  required for the all phases except the merge and the create steps.
  For the sake of code simplicity this order is kept for all phases.

  @return FALSE ok
  @return TRUE  error
*/

bool
mysql_handle_single_derived(LEX *lex, TABLE_LIST *derived, uint phases)
{
  bool res= FALSE;
  THD *thd= lex->thd;
  uint8 allowed_phases= (derived->is_merged_derived() ? DT_PHASES_MERGE :
                         DT_PHASES_MATERIALIZE);
  DBUG_ENTER("mysql_handle_single_derived");
  DBUG_PRINT("enter", ("phases: 0x%x  allowed: 0x%x", phases, allowed_phases));
  if (!lex->derived_tables)
    DBUG_RETURN(FALSE);

  lex->thd->derived_tables_processing= TRUE;

  for (uint phase= 0; phase < DT_PHASES; phase++)
  {
    uint phase_flag= DT_INIT << phase;
    if (phase_flag > phases)
      break;
    if (!(phases & phase_flag))
      continue;
    /* Skip derived tables to which the phase isn't applicable.  */
    if (phase_flag != DT_PREPARE &&
        !(allowed_phases & phase_flag))
      continue;
    if (phase_flag >= DT_CREATE && !thd->fill_derived_tables())
      break;

    if ((res= (*processors[phase])(lex->thd, lex, derived)))
      break;
  }
  lex->thd->derived_tables_processing= FALSE;
  DBUG_RETURN(res);
}


/**
  Run specified phases for derived tables/views in the given list

  @param lex        LEX for this thread
  @param table_list list of derived tables/view to handle
  @param phase_map  phases to process tables/views through

  @details
  This function runs phases specified by the 'phases_map' on derived
  tables/views found in the 'dt_list' with help of the
  TABLE_LIST::handle_derived function.
  'lex' is passed as an argument to the TABLE_LIST::handle_derived.

  @return FALSE ok
  @return TRUE  error
*/

bool
mysql_handle_list_of_derived(LEX *lex, TABLE_LIST *table_list, uint phases)
{
  for (TABLE_LIST *tl= table_list; tl; tl= tl->next_local)
  {
    if (tl->is_view_or_derived() &&
        tl->handle_derived(lex, phases))
      return TRUE;
  }
  return FALSE;
}


/**
  Merge a derived table/view into the embedding select

  @param thd     thread handle
  @param lex     LEX of the embedding query.
  @param derived reference to the derived table.

  @details
  This function merges the given derived table / view into the parent select
  construction. Any derived table/reference to view occurred in the FROM
  clause of the embedding select is represented by a TABLE_LIST structure a
  pointer to which is passed to the function as in the parameter 'derived'.
  This structure contains  the number/map, alias, a link to SELECT_LEX of the
  derived table and other info. If the 'derived' table is used in a nested join
  then additionally the structure contains a reference to the ON expression
  for this join.

  The merge process results in elimination of the derived table (or the
  reference to a view) such that:
    - the FROM list of the derived table/view is wrapped into a nested join
      after which the nest is added to the FROM list of the embedding select
    - the WHERE condition of the derived table (view) is ANDed with the ON
      condition attached to the table.

  @note
  Tables are merged into the leaf_tables list, original derived table is removed
  from this list also. SELECT_LEX::table_list list is left untouched.
  Where expression is merged with derived table's on_expr and can be found after
  the merge through the SELECT_LEX::table_list.

  Examples of the derived table/view merge:

  Schema:
  Tables: t1(f1), t2(f2), t3(f3)
  View v1: SELECT f1 FROM t1 WHERE f1 < 1

  Example with a view:
    Before merge:

    The query (Q1): SELECT f1,f2 FROM t2 LEFT JOIN v1 ON f1 = f2

       (LEX of the main query)
                 |
           (select_lex)
                 |
         (FROM table list)
                 |
            (join list)= t2, v1
                             / \
                            /  (on_expr)= (f1 = f2)
                            |
                    (LEX of the v1 view)
                            |
                       (select_lex)= SELECT f1 FROM t1 WHERE f1 < 1


    After merge:

    The rewritten query Q1 (Q1'):
      SELECT f1,f2 FROM t2 LEFT JOIN (t1) ON ((f1 = f2) and (f1 < 1))

        (LEX of the main query)
                   |
             (select_lex)
                   |
           (FROM table list)
                   |
               (join list)= t2, (t1)
                                    \
                                   (on_expr)= (f1 = f2) and (f1 < 1)

    In this example table numbers are assigned as follows:
      (outer select): t2 - 1, v1 - 2
      (inner select): t1 - 1
    After the merge table numbers will be:
      (outer select): t2 - 1, t1 - 2

  Example with a derived table:
    The query Q2:
      SELECT f1,f2
       FROM (SELECT f1 FROM t1, t3 WHERE f1=f3 and f1 < 1) tt, t2
       WHERE f1 = f2

    Before merge:
              (LEX of the main query)
                        |
                  (select_lex)
                  /           \
       (FROM table list)   (WHERE clause)= (f1 = f2)
                  |
           (join list)= tt, t2
                       / \
                      /  (on_expr)= (empty)
                     /
           (select_lex)= SELECT f1 FROM t1, t3 WHERE f1 = f3 and f1 < 1

    After merge:

    The rewritten query Q2 (Q2'):
      SELECT f1,f2
       FROM (t1, t3) JOIN t2 ON (f1 = f3 and f1 < 1)
       WHERE f1 = f2

              (LEX of the main query)
                        |
                  (select_lex)
                  /           \
       (FROM table list)   (WHERE clause)= (f1 = f2)
                 |
          (join list)= t2, (t1, t3)
                                   \
                                 (on_expr)= (f1 = f3 and f1 < 1)

  In this example table numbers are assigned as follows:
    (outer select): tt - 1, t2 - 2
    (inner select): t1 - 1, t3 - 2
  After the merge table numbers will be:
    (outer select): t1 - 1, t2 - 2, t3 - 3

  @return FALSE if derived table/view were successfully merged.
  @return TRUE if an error occur.
*/

bool mysql_derived_merge(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  bool res= FALSE;
  SELECT_LEX *dt_select= derived->get_single_select();
  table_map map;
  uint tablenr;
  SELECT_LEX *parent_lex= derived->select_lex;
  Query_arena *arena, backup;
  DBUG_ENTER("mysql_derived_merge");

  if (derived->merged)
    DBUG_RETURN(FALSE);

  if (dt_select->uncacheable & UNCACHEABLE_RAND)
  {
    /* There is random function => fall back to materialization. */
    derived->change_refs_to_fields();
    derived->set_materialized_derived();
    DBUG_RETURN(FALSE);
  }

 if (thd->lex->sql_command == SQLCOM_UPDATE_MULTI ||
     thd->lex->sql_command == SQLCOM_DELETE_MULTI)
   thd->save_prep_leaf_list= TRUE;

  arena= thd->activate_stmt_arena_if_needed(&backup);  // For easier test
  derived->merged= TRUE;

  if (!derived->merged_for_insert || 
      (derived->is_multitable() && 
       (thd->lex->sql_command == SQLCOM_UPDATE_MULTI ||
        thd->lex->sql_command == SQLCOM_DELETE_MULTI)))
  {
    /*
      Check whether there is enough free bits in table map to merge subquery.
      If not - materialize it. This check isn't cached so when there is a big
      and small subqueries, and the bigger one can't be merged it wouldn't
      block the smaller one.
    */
    if (parent_lex->get_free_table_map(&map, &tablenr))
    {
      /* There is no enough table bits, fall back to materialization. */
      goto unconditional_materialization;
    }

    if (dt_select->leaf_tables.elements + tablenr > MAX_TABLES)
    {
      /* There is no enough table bits, fall back to materialization. */
      goto unconditional_materialization;
    }

    if (dt_select->options & OPTION_SCHEMA_TABLE)
      parent_lex->options |= OPTION_SCHEMA_TABLE;

    if (!derived->get_unit()->prepared)
    {
      dt_select->leaf_tables.empty();
      make_leaves_list(dt_select->leaf_tables, derived, TRUE, 0);
    } 

    derived->nested_join= (NESTED_JOIN*) thd->calloc(sizeof(NESTED_JOIN));
    if (!derived->nested_join)
    {
      res= TRUE;
      goto exit_merge;
    }

    /* Merge derived table's subquery in the parent select. */
    if (parent_lex->merge_subquery(thd, derived, dt_select, tablenr, map))
    {
      res= TRUE;
      goto exit_merge;
    }

    /*
      exclude select lex so it doesn't show up in explain.
      do this only for derived table as for views this is already done.

      From sql_view.cc
        Add subqueries units to SELECT into which we merging current view.
        unit(->next)* chain starts with subqueries that are used by this
        view and continues with subqueries that are used by other views.
        We must not add any subquery twice (otherwise we'll form a loop),
        to do this we remember in end_unit the first subquery that has
        been already added.
    */
    derived->get_unit()->exclude_level();
    if (parent_lex->join) 
      parent_lex->join->table_count+= dt_select->join->table_count - 1;
  }
  if (derived->get_unit()->prepared)
  {
    Item *expr= derived->on_expr;
    expr= and_conds(expr, dt_select->join ? dt_select->join->conds : 0);
    if (expr && (derived->prep_on_expr || expr != derived->on_expr))
    {
      derived->on_expr= expr;
      derived->prep_on_expr= expr->copy_andor_structure(thd);
    }
    if (derived->on_expr &&
        ((!derived->on_expr->fixed &&
          derived->on_expr->fix_fields(thd, &derived->on_expr)) ||
          derived->on_expr->check_cols(1)))
    {
      res= TRUE; /* purecov: inspected */
      goto exit_merge;
    }
    // Update used tables cache according to new table map
    if (derived->on_expr)
    {
      derived->on_expr->fix_after_pullout(parent_lex, &derived->on_expr);
      fix_list_after_tbl_changes(parent_lex, &derived->nested_join->join_list);
    }
  }

exit_merge:
  if (arena)
    thd->restore_active_arena(arena, &backup);
  DBUG_RETURN(res);

unconditional_materialization:
  derived->change_refs_to_fields();
  derived->set_materialized_derived();
  if (!derived->table || !derived->table->created)
    res= mysql_derived_create(thd, lex, derived);
  if (!res)
    res= mysql_derived_fill(thd, lex, derived);
  goto exit_merge;
}


/**
  Merge a view for the embedding INSERT/UPDATE/DELETE

  @param thd     thread handle
  @param lex     LEX of the embedding query.
  @param derived reference to the derived table.

  @details
  This function substitutes the derived table for the first table from
  the query of the derived table thus making it a correct target table for the
  INSERT/UPDATE/DELETE statements. As this operation is correct only for
  single table views only, for multi table views this function does nothing.
  The derived parameter isn't checked to be a view as derived tables aren't
  allowed for INSERT/UPDATE/DELETE statements.

  @return FALSE if derived table/view were successfully merged.
  @return TRUE if an error occur.
*/

bool mysql_derived_merge_for_insert(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  DBUG_ENTER("mysql_derived_merge_for_insert");
  if (derived->merged_for_insert)
    DBUG_RETURN(FALSE);
  if (derived->is_materialized_derived())
    DBUG_RETURN(mysql_derived_prepare(thd, lex, derived));
  if (!derived->is_multitable())
  {
    if (!derived->single_table_updatable())
      DBUG_RETURN(derived->create_field_translation(thd));
    if (derived->merge_underlying_list)
    {
      derived->table= derived->merge_underlying_list->table;
      derived->schema_table= derived->merge_underlying_list->schema_table;
      derived->merged_for_insert= TRUE;
    }
  }  
  DBUG_RETURN(FALSE);
}


/*
  Initialize a derived table/view

  @param thd	     Thread handle
  @param lex         LEX of the embedding query.
  @param derived     reference to the derived table.

  @detail
  Fill info about derived table/view without preparing an
  underlying select. Such as: create a field translation for views, mark it as
  a multitable if it is and so on.

  @return
    false  OK
    true   Error
*/


bool mysql_derived_init(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->get_unit();
  DBUG_ENTER("mysql_derived_init");

  // Skip already prepared views/DT
  if (!unit || unit->prepared)
    DBUG_RETURN(FALSE);

  DBUG_RETURN(derived->init_derived(thd, TRUE));
}


/*
  Create temporary table structure (but do not fill it)

  @param thd	     Thread handle
  @param lex         LEX of the embedding query.
  @param derived     reference to the derived table.

  @detail
  Prepare underlying select for a derived table/view. To properly resolve
  names in the embedding query the TABLE structure is created. Actual table
  is created later by the mysql_derived_create function.

  This function is called before any command containing derived table
  is executed. All types of derived tables are handled by this function:
  - Anonymous derived tables, or
  - Named derived tables (aka views).

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

  Only the TABLE structure is created here, actual table is created by the
  mysql_derived_create function.

  @note This function sets @c SELECT_ACL for @c TEMPTABLE views as well as
  anonymous derived tables, but this is ok since later access checking will
  distinguish between them.

  @see mysql_handle_derived(), mysql_derived_fill(), GRANT_INFO

  @return
    false  OK
    true   Error
*/

bool mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->get_unit();
  DBUG_ENTER("mysql_derived_prepare");
  bool res= FALSE;

  // Skip already prepared views/DT
  if (!unit || unit->prepared ||
      (derived->merged_for_insert && 
       !(derived->is_multitable() &&
         (thd->lex->sql_command == SQLCOM_UPDATE_MULTI ||
          thd->lex->sql_command == SQLCOM_DELETE_MULTI))))
    DBUG_RETURN(FALSE);

  Query_arena *arena, backup;
  arena= thd->activate_stmt_arena_if_needed(&backup);

  SELECT_LEX *first_select= unit->first_select();

  /* prevent name resolving out of derived table */
  for (SELECT_LEX *sl= first_select; sl; sl= sl->next_select())
  {
    sl->context.outer_context= 0;
    // Prepare underlying views/DT first.
    if ((res= sl->handle_derived(lex, DT_PREPARE)))
      goto exit;

    if (derived->outer_join && sl->first_cond_optimization)
    {
      /* Mark that table is part of OUTER JOIN and fields may be NULL */
      for (TABLE_LIST *cursor= (TABLE_LIST*) sl->table_list.first;
           cursor;
           cursor= cursor->next_local)
        cursor->outer_join|= JOIN_TYPE_OUTER;
    }
  }

  unit->derived= derived;

  if (!(derived->derived_result= new select_union))
    DBUG_RETURN(TRUE); // out of memory

  lex->context_analysis_only|= CONTEXT_ANALYSIS_ONLY_DERIVED;
  // st_select_lex_unit::prepare correctly work for single select
  if ((res= unit->prepare(thd, derived->derived_result, 0)))
    goto exit;
  lex->context_analysis_only&= ~CONTEXT_ANALYSIS_ONLY_DERIVED;
  if ((res= check_duplicate_names(unit->types, 0)))
    goto exit;

  /*
    Check whether we can merge this derived table into main select.
    Depending on the result field translation will or will not
    be created.
  */
  if (derived->init_derived(thd, FALSE))
    goto exit;

  /*
    Temp table is created so that it hounours if UNION without ALL is to be 
    processed

    As 'distinct' parameter we always pass FALSE (0), because underlying
    query will control distinct condition by itself. Correct test of
    distinct underlying query will be is_union &&
    !unit->union_distinct->next_select() (i.e. it is union and last distinct
    SELECT is last SELECT of UNION).
  */
  thd->create_tmp_table_for_derived= TRUE;
  if (derived->derived_result->create_result_table(thd, &unit->types, FALSE,
                                                (first_select->options |
                                                 thd->variables.option_bits |
                                                 TMP_TABLE_ALL_COLUMNS),
                                                derived->alias,
                                                FALSE, FALSE))
  { 
    thd->create_tmp_table_for_derived= FALSE;
    goto exit;
  }
  thd->create_tmp_table_for_derived= FALSE;

  derived->table= derived->derived_result->table;
  if (derived->is_derived() && derived->is_merged_derived())
    first_select->mark_as_belong_to_derived(derived);

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
    if (derived->table)
      free_tmp_table(thd, derived->table);
    delete derived->derived_result;
  }
  else
  {
    TABLE *table= derived->table;
    table->derived_select_number= first_select->select_number;
    table->s->tmp_table= INTERNAL_TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (derived->referencing_view)
      table->grant= derived->grant;
    else
    {
      table->grant.privilege= SELECT_ACL;
      if (derived->is_derived())
        derived->grant.privilege= SELECT_ACL;
    }
#endif
    /* Add new temporary table to list of open derived tables */
    table->next= thd->derived_tables;
    thd->derived_tables= table;

    /* If table is used by a left join, mark that any column may be null */
    if (derived->outer_join)
      table->maybe_null= 1;
  }
  if (arena)
    thd->restore_active_arena(arena, &backup);
  DBUG_RETURN(res);
}


/**
  Runs optimize phase for a derived table/view.

  @param thd     thread handle
  @param lex     LEX of the embedding query.
  @param derived reference to the derived table.

  @details
  Runs optimize phase for given 'derived' derived table/view.
  If optimizer finds out that it's of the type "SELECT a_constant" then this
  functions also materializes it.

  @return FALSE ok.
  @return TRUE if an error occur.
*/

bool mysql_derived_optimize(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  SELECT_LEX_UNIT *unit= derived->get_unit();
  SELECT_LEX *first_select= unit->first_select();
  SELECT_LEX *save_current_select= lex->current_select;

  bool res= FALSE;
  DBUG_ENTER("mysql_derived_optimize");

  if (unit->optimized)
    DBUG_RETURN(FALSE);
  lex->current_select= first_select;

  if (unit->is_union())
  {
    // optimize union without execution
    res= unit->optimize();
  }
  else if (unit->derived)
  {
    if (!derived->is_merged_derived())
    {
      JOIN *join= first_select->join;
      unit->set_limit(unit->global_parameters);
      unit->optimized= TRUE;
      if ((res= join->optimize()))
        goto err;
      if (join->table_count == join->const_tables)
        derived->fill_me= TRUE;
    }
  }
  /*
    Materialize derived tables/views of the "SELECT a_constant" type.
    Such tables should be materialized at the optimization phase for
    correct constant evaluation.
  */
  if (!res && derived->fill_me && !derived->merged_for_insert)
  {
    if (derived->is_merged_derived())
    {
      derived->change_refs_to_fields();
      derived->set_materialized_derived();
    }
    if ((res= mysql_derived_create(thd, lex, derived)))
      goto err;
    if ((res= mysql_derived_fill(thd, lex, derived)))
      goto err;
  }
err:
  lex->current_select= save_current_select;
  DBUG_RETURN(res);
}


/**
  Actually create result table for a materialized derived table/view.

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
  DBUG_ENTER("mysql_derived_create");
  TABLE *table= derived->table;
  SELECT_LEX_UNIT *unit= derived->get_unit();

  if (table->created)
    DBUG_RETURN(FALSE);
  select_union *result= (select_union*)unit->result;
  if (table->s->db_type() == TMP_ENGINE_HTON)
  {
    result->tmp_table_param.keyinfo= table->s->key_info;
    if (create_internal_tmp_table(table, result->tmp_table_param.keyinfo,
                                  result->tmp_table_param.start_recinfo,
                                  &result->tmp_table_param.recinfo,
                                  (unit->first_select()->options |
                                   thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS)))
      DBUG_RETURN(TRUE);
  }
  if (open_tmp_table(table))
    DBUG_RETURN(TRUE);
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  DBUG_RETURN(FALSE);
}


/*
  Execute subquery of a materialized derived table/view and fill the result
  table.

  @param thd      Thread handle
  @param lex      LEX for this thread
  @param derived  reference to the derived table.

  @details
  Execute subquery of given 'derived' table/view and fill the result
  table. After result table is filled, if this is not the EXPLAIN statement,
  the entire unit / node is deleted. unit is deleted if UNION is used
  for derived table and node is deleted is it is a simple SELECT.
  'lex' is unused and 'thd' is passed as an argument to an underlying function.

  @note
  If you use this function, make sure it's not called at prepare.
  Due to evaluation of LIMIT clause it can not be used at prepared stage.

  @return FALSE  OK
  @return TRUE   Error
*/

bool mysql_derived_fill(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  DBUG_ENTER("mysql_derived_fill");
  SELECT_LEX_UNIT *unit= derived->get_unit();
  bool res= FALSE;

  if (unit->executed && !unit->uncacheable && !unit->describe)
    DBUG_RETURN(FALSE);
  /*check that table creation passed without problems. */
  DBUG_ASSERT(derived->table && derived->table->created);
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
    unit->set_limit(unit->global_parameters);
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
                      (first_select->options |thd->variables.option_bits |
                       SELECT_NO_UNLOCK),
                      derived_result, unit, first_select);
  }

  if (!res)
  {
    if (derived_result->flush())
      res= TRUE;
    unit->executed= TRUE;
  }
  if (res || !lex->describe) 
    unit->cleanup();
  lex->current_select= save_current_select;

  DBUG_RETURN(res);
}


/**
  Re-initialize given derived table/view for the next execution.

  @param  thd         thread handle
  @param  lex         LEX for this thread
  @param  derived     reference to the derived table.

  @details
  Re-initialize given 'derived' table/view for the next execution.
  All underlying views/derived tables are recursively reinitialized prior
  to re-initialization of given derived table.
  'thd' and 'lex' are passed as arguments to called functions.

  @return FALSE  OK
  @return TRUE   Error
*/

bool mysql_derived_reinit(THD *thd, LEX *lex, TABLE_LIST *derived)
{
  DBUG_ENTER("mysql_derived_reinit");
  st_select_lex_unit *unit= derived->get_unit();

  if (derived->table)
    derived->merged_for_insert= FALSE;
  unit->unclean();
  unit->types.empty();
  /* for derived tables & PS (which can't be reset by Item_subquery) */
  unit->reinit_exec_mechanism();
  unit->set_thd(thd);
  DBUG_RETURN(FALSE);
}


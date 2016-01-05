/* Copyright (c) 2002, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_derived.h"
#include "sql_select.h"
#include "sql_resolver.h"
#include "sql_optimizer.h"                    // JOIN
#include "sql_view.h"                         // check_duplicate_names
#include "auth_common.h"                      // SELECT_ACL
#include "sql_tmp_table.h"                    // Tmp tables
#include "sql_union.h"                        // Query_result_union
#include "opt_trace.h"                        // opt_trace_disable_etc


/**
  Resolve a derived table or view reference, including recursively resolving
  contained subqueries.

  @param thd thread handle
  @param apply_semijoin Apply possible semi-join transforms if this is true

  @returns false if success, true if error
*/

bool TABLE_LIST::resolve_derived(THD *thd, bool apply_semijoin)
{
  DBUG_ENTER("TABLE_LIST::resolve_derived");

  if (!is_view_or_derived() || is_merged())
    DBUG_RETURN(false);

  const bool derived_tables_saved= thd->derived_tables_processing;

  thd->derived_tables_processing= true;

#ifndef DBUG_OFF
  for (SELECT_LEX *sl= derived->first_select(); sl; sl= sl->next_select())
  {
    // Make sure there are no outer references
    DBUG_ASSERT(sl->context.outer_context == NULL);
  }
#endif
  if (!(derived_result= new (thd->mem_root) Query_result_union))
    DBUG_RETURN(true);

  /*
    Prepare the underlying query expression of the derived table.
    The SELECT_STRAIGHT_JOIN option prevents semi-join transformation.
  */
  if (derived->prepare(thd, derived_result,
                       !apply_semijoin ? SELECT_STRAIGHT_JOIN : 0, 0))
    DBUG_RETURN(true);

  if (check_duplicate_names(derived->types, 0))
    DBUG_RETURN(true);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /*
    A derived table is transparent with respect to privilege checking.
    This setting means that privilege checks ignore the derived table
    and are done properly in underlying base tables and views.
    SELECT_ACL is used because derived tables cannot be used for update,
    delete or insert.
  */
  if (is_derived())
    set_privileges(SELECT_ACL);
#endif

  thd->derived_tables_processing= derived_tables_saved;

  DBUG_RETURN(false);
}


/**
  Prepare a derived table or view for materialization.

  @param  thd   THD pointer

  @return false if successful, true if error
*/
bool TABLE_LIST::setup_materialized_derived(THD *thd)

{
  DBUG_ENTER("TABLE_LIST::setup_materialized_derived");

  DBUG_ASSERT(is_view_or_derived() && !is_merged() && table == NULL);

  DBUG_PRINT("info", ("algorithm: TEMPORARY TABLE"));

  Opt_trace_context *const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_derived(trace, is_view() ? "view" : "derived");
  trace_derived.add_utf8_table(this).
    add("select#", derived->first_select()->select_number).
    add("materialized", true);

  set_uses_materialization();

  // Create the result table for the materialization
  const ulonglong create_options= derived->first_select()->active_options() |
                                  TMP_TABLE_ALL_COLUMNS;
  if (derived_result->create_result_table(thd, &derived->types, false, 
                                          create_options,
                                          alias, false, false))
    DBUG_RETURN(true);        /* purecov: inspected */

  table= derived_result->table;
  table->pos_in_table_list= this;

  // Make table's name same as the underlying materialized table
  set_name_temporary();

  table->s->tmp_table= NON_TRANSACTIONAL_TMP_TABLE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (referencing_view)
    table->grant= grant;
  else
    table->grant.privilege= SELECT_ACL;
#endif

  // Table is "nullable" if inner table of an outer_join
  if (is_inner_table_of_outer_join())
    table->set_nullable();

  // Add new temporary table to list of open derived tables
  table->next= thd->derived_tables;
  thd->derived_tables= table;

  for (SELECT_LEX *sl= derived->first_select(); sl; sl= sl->next_select())
  {
    /*
      Derived tables/view are materialized prior to UPDATE, thus we can skip
      them from table uniqueness check
    */
    sl->propagate_unique_test_exclusion();

    /*
      SELECT privilege is needed for all materialized derived tables and views,
      and columns must be marked for read, unless command is SHOW FIELDS.
    */
    if (thd->lex->sql_command == SQLCOM_SHOW_FIELDS)
      continue;

    if (sl->check_view_privileges(thd, SELECT_ACL, SELECT_ACL))
      DBUG_RETURN(true);

    // Set all selected fields to be read:
    // @todo Do not set fields that are not referenced from outer query
    DBUG_ASSERT(thd->mark_used_columns == MARK_COLUMNS_READ);
    List_iterator<Item> it(sl->all_fields);
    Item *item;
    Column_privilege_tracker tracker(thd, SELECT_ACL);
    Mark_field mf(thd->mark_used_columns);
    while ((item= it++))
    {
      if (item->walk(&Item::check_column_privileges, Item::WALK_PREFIX,
                     (uchar *)thd))
        DBUG_RETURN(true);
      item->walk(&Item::mark_field_in_map, Item::WALK_POSTFIX, (uchar *)&mf);
    }
  }

  DBUG_RETURN(false);
}


/**
  Optimize the query expression representing a derived table/view.

  @note
  If optimizer finds out that the derived table/view is of the type
  "SELECT a_constant" this functions also materializes it.

  @param thd thread handle

  @returns false if success, true if error.
*/

bool TABLE_LIST::optimize_derived(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::optimize_derived");

  SELECT_LEX_UNIT *const unit= derived_unit();

  DBUG_ASSERT(unit && !unit->is_optimized());

  if (unit->optimize(thd) || thd->is_error())
    DBUG_RETURN(true);

  if (materializable_is_const() &&
      (create_derived(thd) || materialize_derived(thd)))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/**
  Create result table for a materialized derived table/view.

  @param thd     thread handle

  This function actually creates the result table for given 'derived'
  table/view, but it doesn't fill it.

  @returns false if success, true if error.
*/

bool TABLE_LIST::create_derived(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::create_derived");

  SELECT_LEX_UNIT *const unit= derived_unit();

  // @todo: Be able to assert !table->is_created() as well
  DBUG_ASSERT(unit && uses_materialization() && table);

  /*
    Don't create result table if:
    1) Table is already created, or
    2) Table is a constant one with all NULL values.
  */
  if (table->is_created() ||                              // 1
      (select_lex->join != NULL &&                        // 2
       (select_lex->join->const_table_map & map())))      // 2
  {
    /*
      At this point, JT_CONST derived tables should be null rows. Otherwise
      they would have been materialized already.
    */
#ifndef DBUG_OFF
    if (table != NULL)
    {
      QEP_TAB *tab= table->reginfo.qep_tab;
      DBUG_ASSERT(tab == NULL ||
                  tab->type() != JT_CONST ||
                  table->has_null_row());
    }
#endif
    DBUG_RETURN(false);
  }
  /* create tmp table */
  Query_result_union *result= (Query_result_union*)unit->query_result();

  if (instantiate_tmp_table(table, table->key_info,
                            result->tmp_table_param.start_recinfo,
                            &result->tmp_table_param.recinfo,
                            unit->first_select()->active_options() |
                            thd->lex->select_lex->active_options() |
                            TMP_TABLE_ALL_COLUMNS,
                            thd->variables.big_tables, &thd->opt_trace))
    DBUG_RETURN(true);        /* purecov: inspected */

  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);

  table->set_created();

  DBUG_RETURN(false);
}


/**
  Materialize derived table

  @param  thd	    Thread handle

  Derived table is resolved with temporary table. It is created based on the
  queries defined. After temporary table is materialized, if this is not
  EXPLAIN, then the entire unit / node is deleted. unit is deleted if UNION is
  used for derived table and node is deleted is it is a  simple SELECT.
  If you use this function, make sure it's not called at prepare.
  Due to evaluation of LIMIT clause it can not be used at prepared stage.

  @returns false if success, true if error.
*/

bool TABLE_LIST::materialize_derived(THD *thd)
{
  DBUG_ENTER("TABLE_LIST::materialize_derived");

  DBUG_ASSERT(is_view_or_derived() && uses_materialization());

  SELECT_LEX_UNIT *const unit= derived_unit();
  bool res= false;

  DBUG_ASSERT(table && table->is_created());

  if (unit->is_union())
  {
    // execute union without clean up
    res= unit->execute(thd);
  }
  else
  {
    SELECT_LEX *first_select= unit->first_select();
    JOIN *join= first_select->join;
    SELECT_LEX *save_current_select= thd->lex->current_select();
    thd->lex->set_current_select(first_select);

    DBUG_ASSERT(join && join->is_optimized());

    unit->set_limit(first_select);

    join->exec();
    res= join->error;
    thd->lex->set_current_select(save_current_select);
  }

  if (!res)
  {
    /*
      Here we entirely fix both TABLE_LIST and list of SELECT's as
      there were no derived tables
    */
    if (derived_result->flush())
      res= true;                  /* purecov: inspected */
  }

  DBUG_RETURN(res);
}


/**
   Clean up the query expression for a materialized derived table
*/

bool TABLE_LIST::cleanup_derived()
{
  DBUG_ASSERT(is_view_or_derived() && uses_materialization());

  return derived_unit()->cleanup(false);
}

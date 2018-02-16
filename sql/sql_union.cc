/* Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Process query expressions that are composed of

  1. UNION of query blocks, and/or

  2. have ORDER BY / LIMIT clauses in more than one level.

  An example of 2) is:

    (SELECT * FROM t1 ORDER BY a LIMIT 10) ORDER BY b LIMIT 5

  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/

#include "sql_union.h"
#include "sql_select.h"
#include "sql_cursor.h"
#include "sql_base.h"                           // fill_record
#include "filesort.h"                           // filesort_free_buffers
#include "sql_tmp_table.h"                      // tmp tables
#include "sql_optimizer.h"                      // JOIN
#include "opt_explain.h"                        // explain_no_table
#include "opt_explain_format.h"


int Query_result_union::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;
  return 0;
}


bool Query_result_union::send_data(List<Item> &values)
{
  // Skip "offset" number of rows before producing rows
  if (unit->offset_limit_cnt > 0)
  {
    unit->offset_limit_cnt--;
    return false;
  }
  if (fill_record(thd, table, table->visible_field_ptr(), values, NULL, NULL))
    return true;                /* purecov: inspected */

  if (!check_unique_constraint(table))
    return false;

  const int error= table->file->ha_write_row(table->record[0]);
  if (error)
  {
    // create_ondisk_from_heap will generate error if needed
    if (!table->file->is_ignorable_error(error) &&
        create_ondisk_from_heap(thd, table, tmp_table_param.start_recinfo, 
                                &tmp_table_param.recinfo, error, true, NULL))
      return true;            /* purecov: inspected */
    // Table's engine changed, index is not initialized anymore
    if (table->hash_field)
      table->file->ha_index_init(0, false);
  }
  return false;
}


bool Query_result_union::send_eof()
{
  return false;
}


bool Query_result_union::flush()
{
  const int error= table->file->extra(HA_EXTRA_NO_CACHE);
  if (error)
  {
    table->file->print_error(error, MYF(0)); /* purecov: inspected */
    return true;                             /* purecov: inspected */
  }
  return false;
}

/**
  Create a temporary table to store the result of Query_result_union.

  @param thd                thread handle
  @param column_types       a list of items used to define columns of the
                            temporary table
  @param is_union_distinct  if set, the temporary table will eliminate
                            duplicates on insert
  @param options            create options
  @param table_alias        name of the temporary table
  @param bit_fields_as_long convert bit fields to ulonglong

  @details
    Create a temporary table that is used to store the result of a UNION,
    derived table, or a materialized cursor.

  @returns false if table created, true if error
*/

bool Query_result_union::create_result_table(THD *thd_arg,
                                             List<Item> *column_types,
                                             bool is_union_distinct,
                                             ulonglong options,
                                             const char *table_alias,
                                             bool bit_fields_as_long,
                                             bool create_table)
{
  DBUG_ASSERT(table == NULL);
  tmp_table_param= Temp_table_param();
  count_field_types(thd_arg->lex->current_select(), &tmp_table_param,
                    *column_types, false, true);
  tmp_table_param.skip_create_table= !create_table;
  tmp_table_param.bit_fields_as_long= bit_fields_as_long;
  tmp_table_param.can_use_pk_for_unique= !is_union_mixed_with_union_all;

  if (! (table= create_tmp_table(thd_arg, &tmp_table_param, *column_types,
                                 NULL, is_union_distinct, true,
                                 options, HA_POS_ERROR, (char*) table_alias)))
    return true;
  if (create_table)
  {
    table->file->extra(HA_EXTRA_WRITE_CACHE);
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table->hash_field)
      table->file->ha_index_init(0, 0);
  }
  return false;
}


/**
  Reset and empty the temporary table that stores the materialized query result.

  @note The cleanup performed here is exactly the same as for the two temp
  tables of JOIN - exec_tmp_table_[1 | 2].
*/

void Query_result_union::cleanup()
{
  if (table == NULL)
    return;
  table->file->extra(HA_EXTRA_RESET_STATE);
  if (table->hash_field)
    table->file->ha_index_or_rnd_end();
  table->file->ha_delete_all_rows();
  free_io_cache(table);
  filesort_free_buffers(table,0);
}


/**
  UNION result that is passed directly to the receiving Query_result
  without filling a temporary table.

  Function calls are forwarded to the wrapped Query_result, but some
  functions are expected to be called only once for each query, so
  they are only executed for the first query block in the union (except
  for send_eof(), which is executed only for the last query block).

  This Query_result is used when a UNION is not DISTINCT and doesn't
  have a global ORDER BY clause. @see st_select_lex_unit::prepare().
*/
class Query_result_union_direct :public Query_result_union
{
private:
  /// Result object that receives all rows
  Query_result *result;
  /// The last query block of the union
  SELECT_LEX *last_select_lex;

  /// Wrapped result has received metadata
  bool done_send_result_set_metadata;
  /// Wrapped result has initialized tables
  bool done_initialize_tables;

  /// Accumulated current_found_rows
  ulonglong current_found_rows;

  /// Number of rows offset
  ha_rows offset;
  /// Number of rows limit + offset, @see Query_result_union_direct::send_data()
  ha_rows limit;

public:
  Query_result_union_direct(Query_result *result, SELECT_LEX *last_select_lex)
    :result(result), last_select_lex(last_select_lex),
    done_send_result_set_metadata(false), done_initialize_tables(false),
    current_found_rows(0)
  {}
  bool change_query_result(Query_result *new_result);
  uint field_count(List<Item> &fields) const
  {
    // Only called for top-level Query_results, usually Query_result_send
    DBUG_ASSERT(false); /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  bool postponed_prepare(List<Item> &types);
  bool send_result_set_metadata(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join= NULL);
  void send_error(uint errcode, const char *err)
  {
    result->send_error(errcode, err); /* purecov: inspected */
  }
  bool send_eof();
  bool flush() { return false; }
  bool check_simple_select() const
  {
    // Only called for top-level Query_results, usually Query_result_send
    DBUG_ASSERT(false); /* purecov: inspected */
    return false; /* purecov: inspected */
  }
  void abort_result_set()
  {
    result->abort_result_set(); /* purecov: inspected */
  }
  void cleanup() {}
  void set_thd(THD *thd_arg)
  {
    /*
      Only called for top-level Query_results, usually Query_result_send,
      and for the results of subquery engines
      (select_<something>_subselect).
    */
    DBUG_ASSERT(false); /* purecov: inspected */
  }
  void begin_dataset()
  {
    // Only called for sp_cursor::Select_fetch_into_spvars
    DBUG_ASSERT(false); /* purecov: inspected */
  }
};


/**
  Replace the current query result with new_result and prepare it.  

  @param new_result New query result

  @returns false if success, true if error
*/
bool Query_result_union_direct::change_query_result(Query_result *new_result)
{
  result= new_result;
  return (result->prepare(unit->types, unit) || result->prepare2());
}


bool Query_result_union_direct::postponed_prepare(List<Item> &types)
{
  if (result != NULL)
    return (result->prepare(types, unit) || result->prepare2());
  else
    return false;
}


bool Query_result_union_direct::send_result_set_metadata(List<Item> &list,
                                                         uint flags)
{
  if (done_send_result_set_metadata)
    return false;
  done_send_result_set_metadata= true;

  /*
    Set global offset and limit to be used in send_data(). These can
    be variables in prepared statements or stored programs, so they
    must be reevaluated for each execution.
   */
  offset= unit->global_parameters()->get_offset();
  limit= unit->global_parameters()->get_limit();
  if (limit + offset >= limit)
    limit+= offset;
  else
    limit= HA_POS_ERROR; /* purecov: inspected */

  return result->send_result_set_metadata(unit->types, flags);
}


bool Query_result_union_direct::send_data(List<Item> &items)
{
  if (limit == 0)
    return false;
  limit--;
  if (offset)
  {
    offset--;
    return false;
  }

  if (fill_record(thd, table, table->field, items, NULL, NULL))
    return true; /* purecov: inspected */

  return result->send_data(unit->item_list);
}


bool Query_result_union_direct::initialize_tables(JOIN *join)
{
  if (done_initialize_tables)
    return false;
  done_initialize_tables= true;

  return result->initialize_tables(join);
}


bool Query_result_union_direct::send_eof()
{
  /*
    Accumulate the found_rows count for the current query block into the UNION.
    Number of rows returned from a query block is always non-negative.
  */
  ulonglong offset= thd->lex->current_select()->get_offset();
  current_found_rows+= thd->current_found_rows > offset ?
                       thd->current_found_rows - offset : 0;

  if (unit->thd->lex->current_select() == last_select_lex)
  {
    /*
      If SQL_CALC_FOUND_ROWS is not enabled, adjust the current_found_rows
      according to the global limit and offset defined.
    */
    if (!(unit->first_select()->active_options() & OPTION_FOUND_ROWS))
    {
      ha_rows global_limit= unit->global_parameters()->get_limit();
      ha_rows global_offset= unit->global_parameters()->get_offset();

      if (global_limit != HA_POS_ERROR)
      {
        if (global_offset != HA_POS_ERROR)
          global_limit+= global_offset;

        if (current_found_rows > global_limit)
          current_found_rows= global_limit;
      }
    }
    thd->current_found_rows= current_found_rows;

    // Reset and make ready for re-execution
    // @todo: Dangerous if we have an error midway?
    done_send_result_set_metadata= false;
    done_initialize_tables= false;

    return result->send_eof();
  }
  else
    return false;
}


/**
  Prepare the fake_select_lex query block

  @param thd		 Thread handler

  @returns false if success, true if error
*/

bool st_select_lex_unit::prepare_fake_select_lex(THD *thd_arg)
{
  DBUG_ENTER("st_select_lex_unit::prepare_fake_select_lex");

  DBUG_ASSERT(thd_arg->lex->current_select() == fake_select_lex);

  // The UNION result table is input table for this query block
  fake_select_lex->table_list.link_in_list(&result_table_list,
                                           &result_table_list.next_local);

  // Set up the result table for name resolution
  fake_select_lex->context.table_list= 
    fake_select_lex->context.first_name_resolution_table= 
    fake_select_lex->get_table_list();
  if (!fake_select_lex->first_execution)
  {
    for (ORDER *order= fake_select_lex->order_list.first;
         order;
         order= order->next)
      order->item= &order->item_ptr;
  }
  for (ORDER *order= fake_select_lex->order_list.first;
       order;
       order=order->next)
  {
    (*order->item)->walk(&Item::change_context_processor,
                         Item::WALK_POSTFIX,
                         (uchar*) &fake_select_lex->context);
  }
  fake_select_lex->set_query_result(query_result());

  /*
    For subqueries in form "a IN (SELECT .. UNION SELECT ..):
    when optimizing the fake_select_lex that reads the results of the union
    from a temporary table, do not mark the temp. table as constant because
    the contents in it may vary from one subquery execution to another.
  */
  fake_select_lex->make_active_options(
     (first_select()->active_options() & OPTION_FOUND_ROWS) |
     OPTION_NO_CONST_TABLES |
     SELECT_NO_UNLOCK,
     0);
  fake_select_lex->fields_list= item_list;

  /*
    We need to add up n_sum_items in order to make the correct
    allocation in setup_ref_array().
    Don't add more sum_items if we have already done SELECT_LEX::prepare
    for this (with a different join object)
  */
  if (fake_select_lex->ref_pointer_array.is_null())
    fake_select_lex->n_child_sum_items+= fake_select_lex->n_sum_items;

  DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
              fake_select_lex->master_unit() == this &&
              !fake_select_lex->group_list.elements &&
              fake_select_lex->where_cond() == NULL &&
              fake_select_lex->having_cond() == NULL);

  if (fake_select_lex->prepare(thd_arg))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/**
  Prepares all query blocks of a query expression, including fake_select_lex

  @param thd_arg       Thread handler
  @param sel_result    Result object where the unit's output should go.
  @param added_options These options will be added to the query blocks.
  @param removed_options Options that cannot be used for this query

  @returns false if success, true if error
 */
bool st_select_lex_unit::prepare(THD *thd_arg, Query_result *sel_result,
                                 ulonglong added_options,
                                 ulonglong removed_options)
{
  DBUG_ENTER("st_select_lex_unit::prepare");

  DBUG_ASSERT(!is_prepared());

  SELECT_LEX *lex_select_save= thd_arg->lex->current_select();

  Query_result *tmp_result;
  bool instantiate_tmp_table= false;

  SELECT_LEX *last_select= first_select();
  while (last_select->next_select())
    last_select= last_select->next_select();

  set_query_result(sel_result);

  thd_arg->lex->set_current_select(first_select());

  // Save fake_select_lex in case we don't need it for anything but
  // global parameters.
  if (saved_fake_select_lex == NULL && // Don't overwrite on PS second prepare
      fake_select_lex != NULL)
  {
    thd->lock_query_plan();
    saved_fake_select_lex= fake_select_lex;
    thd->unlock_query_plan();
  }

  const bool simple_query_expression= is_simple();

  // Create query result object for use by underlying query blocks
  if (!simple_query_expression)
  {
    if (is_union() && !union_needs_tmp_table())
    {
      if (!(tmp_result= union_result=
              new Query_result_union_direct(sel_result, last_select)))
        goto err; /* purecov: inspected */
      if (fake_select_lex != NULL)
      {
        thd->lock_query_plan();
        fake_select_lex= NULL;
        thd->unlock_query_plan();
      }
      instantiate_tmp_table= false;
    }
    else
    {
      if (!(tmp_result= union_result= new Query_result_union()))
        goto err; /* purecov: inspected */
      instantiate_tmp_table= true;
    }
  }
  else
  {
    // Only one query block, and no "fake" object: No extra result needed:
    tmp_result= sel_result;
  }

  first_select()->context.resolve_in_select_list= true;

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    sl->set_query_result(tmp_result);
    sl->make_active_options(added_options | SELECT_NO_UNLOCK, removed_options);
    sl->fields_list= sl->item_list;

    /*
      setup_tables_done_option should be set only for very first SELECT,
      because it protect from second setup_tables call for select-like non
      select commands (DELETE/INSERT/...) and they use only very first
      SELECT (for union it can be only INSERT ... SELECT).
    */
    added_options&= ~OPTION_SETUP_TABLES_DONE;

    thd_arg->lex->set_current_select(sl);

    if (sl->prepare(thd_arg))
      goto err;

    /*
      Use items list of underlaid select for derived tables to preserve
      information about fields lengths and exact types
    */
    if (simple_query_expression)
      types= first_select()->item_list;
    else if (sl == first_select())
    {
      types.empty();
      List_iterator_fast<Item> it(sl->item_list);
      Item *item_tmp;
      while ((item_tmp= it++))
      {
        /*
          If the outer query has a GROUP BY clause, an outer reference to this
          query block may have been wrapped in a Item_outer_ref, which has not
          been fixed yet. An Item_type_holder must be created based on a fixed
          Item, so use the inner Item instead.
        */
        DBUG_ASSERT(item_tmp->fixed ||
                    (item_tmp->type() == Item::REF_ITEM &&
                     down_cast<Item_ref *>(item_tmp)->ref_type() ==
                     Item_ref::OUTER_REF));
        if (!item_tmp->fixed)
          item_tmp= item_tmp->real_item();

	/* Error's in 'new' will be detected after loop */
	types.push_back(new Item_type_holder(thd_arg, item_tmp));
      }
      if (thd_arg->is_error())
	goto err; // out of memory
    }
    else
    {
      if (types.elements != sl->item_list.elements)
      {
	my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
		   ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
	goto err;
      }
      List_iterator_fast<Item> it(sl->item_list);
      List_iterator_fast<Item> tp(types);	
      Item *type, *item_tmp;
      while ((type= tp++, item_tmp= it++))
      {
        if (((Item_type_holder*)type)->join_types(thd_arg, item_tmp))
	  DBUG_RETURN(true);
      }
    }
  }

  /*
    If the query is using Query_result_union_direct, we have postponed
    preparation of the underlying Query_result until column types are known.
  */
  if (union_result != NULL && union_result->postponed_prepare(types))
    DBUG_RETURN(true);

  if (!simple_query_expression)
  {
    /*
      Check that it was possible to aggregate all collations together for UNION.
      We need this in case of UNION DISTINCT, to filter out duplicates using
      the proper collation.

      TODO: consider removing this test in case of UNION ALL.
    */
    List_iterator_fast<Item> tp(types);
    Item *type;

    while ((type= tp++))
    {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE)
      {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }
    ulonglong create_options= first_select()->active_options() |
                              TMP_TABLE_ALL_COLUMNS;
    /*
      Force the temporary table to be a MyISAM table if we're going to use
      fulltext functions (MATCH ... AGAINST .. IN BOOLEAN MODE) when reading
      from it (this should be removed when fulltext search is moved
      out of MyISAM).
    */
    if (fake_select_lex && fake_select_lex->ftfunc_list->elements)
      create_options|= TMP_TABLE_FORCE_MYISAM;

    if (union_distinct)
    {
      // Mixed UNION and UNION ALL
      if (union_distinct != last_select)
        union_result->is_union_mixed_with_union_all= true;
    }
    if (union_result->create_result_table(thd, &types, MY_TEST(union_distinct),
                                          create_options, "", false,
                                          instantiate_tmp_table))
      goto err;
    new (&result_table_list) TABLE_LIST;
    result_table_list.db= (char*) "";
    result_table_list.table_name= result_table_list.alias= (char*) "union";
    result_table_list.table= table= union_result->table;
    table->pos_in_table_list= &result_table_list;
    result_table_list.select_lex= fake_select_lex ?
                                     fake_select_lex : saved_fake_select_lex;
    result_table_list.set_tableno(0);

    result_table_list.set_privileges(SELECT_ACL);

    if (!item_list.elements)
    {
      Prepared_stmt_arena_holder ps_arena_holder(thd);
      if (table->fill_item_list(&item_list))
        goto err;            /* purecov: inspected */
    }
    else
    {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      table->reset_item_list(&item_list);
    }
    if (fake_select_lex != NULL)
    {
      thd_arg->lex->set_current_select(fake_select_lex);

      if (prepare_fake_select_lex(thd_arg))
        goto err;
    }
  }

  thd_arg->lex->set_current_select(lex_select_save);

  set_prepared();          // All query blocks prepared, update the state

  DBUG_RETURN(false);

err:
  (void) cleanup(false);
  DBUG_RETURN(true);
}


/**
  Optimize all query blocks of a query expression, including fake_select_lex

  @param thd    thread handler

  @returns false if optimization successful, true if error
*/

bool st_select_lex_unit::optimize(THD *thd)
{
  DBUG_ENTER("st_select_lex_unit::optimize");

  DBUG_ASSERT(is_prepared() && !is_optimized());

  SELECT_LEX *save_select= thd->lex->current_select();

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    thd->lex->set_current_select(sl);

    // LIMIT is required for optimization
    set_limit(sl);

    if (sl->optimize(thd))
      DBUG_RETURN(true);

    /*
      Accumulate estimated number of rows.
      1. Implicitly grouped query has one row (with HAVING it has zero or one
         rows).
      2. If GROUP BY clause is optimized away because it was a constant then
         query produces at most one row.
    */
    if (query_result())
      query_result()->estimated_rowcount+=
        sl->is_implicitly_grouped() || sl->join->group_optimized_away ?
          1 :  sl->join->best_rowcount;

  }
  if (fake_select_lex)
  {
    thd->lex->set_current_select(fake_select_lex);

    set_limit(fake_select_lex);

    /*
      In EXPLAIN command, constant subqueries that do not use any
      tables are executed two times:
       - 1st time is a real evaluation to get the subquery value
       - 2nd time is to produce EXPLAIN output rows.
      1st execution sets certain members (e.g. Query_result) to perform
      subquery execution rather than EXPLAIN line production. In order 
      to reset them back, we re-do all of the actions (yes it is ugly).
    */
    DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
                fake_select_lex->master_unit() == this &&
                !fake_select_lex->group_list.elements &&
                fake_select_lex->get_table_list() == &result_table_list &&
                fake_select_lex->where_cond() == NULL &&
                fake_select_lex->having_cond() == NULL);

    if (fake_select_lex->optimize(thd))
      DBUG_RETURN(true);
  }
  set_optimized();    // All query blocks optimized, update the state
  thd->lex->set_current_select(save_select);

  DBUG_RETURN(false);
}


/**
  Explain query starting from this unit.

  @param ethd  THD of explaining thread

  @return false if success, true if error
*/

bool st_select_lex_unit::explain(THD *ethd)
{
  DBUG_ENTER("st_select_lex_unit::explain");

#ifndef DBUG_OFF
  SELECT_LEX *lex_select_save= thd->lex->current_select();
#endif
  Explain_format *fmt= ethd->lex->explain_format;
  const bool other= (thd != ethd);
  bool ret= false;

  if (!other)
  {
    DBUG_ASSERT(!is_simple() && is_optimized());
    set_executed();
  }

  if (fmt->begin_context(CTX_UNION))
    DBUG_RETURN(true);

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (fmt->begin_context(CTX_QUERY_SPEC))
      DBUG_RETURN(true);
    if (explain_query_specification(ethd, sl, CTX_JOIN) ||
        fmt->end_context(CTX_QUERY_SPEC))
      DBUG_RETURN(true);
  }

  if (fake_select_lex != NULL)
  {
    // Don't save result as it's needed only for consequent exec.
    ret= explain_query_specification(ethd, fake_select_lex, CTX_UNION_RESULT);
  }
  if (!other)
    DBUG_ASSERT(thd->lex->current_select() == lex_select_save);

  if (ret)
    DBUG_RETURN(true);
  fmt->end_context(CTX_UNION);

  DBUG_RETURN(false);
}


/**
  Execute a query expression that may be a UNION and/or have an ordered result.

  @param thd          thread handle

  @returns false if success, true if error
*/

bool st_select_lex_unit::execute(THD *thd)
{
  DBUG_ENTER("st_select_lex_unit::exec");
  DBUG_ASSERT(!is_simple() && is_optimized());

  if (is_executed() && !uncacheable)
    DBUG_RETURN(false);

  SELECT_LEX *lex_select_save= thd->lex->current_select();

  bool status= false;          // Execution error status

  // Set "executed" state, even though execution may end with an error
  set_executed();
  
  if (item)
  {
    item->reset_value_registration();

    if (item->assigned())
    {
      item->assigned(false); // Prepare for re-execution of this unit
      item->reset();
      if (table->is_created())
      {
        table->file->ha_delete_all_rows();
        table->file->info(HA_STATUS_VARIABLE);
      }
    }
    // re-enable indexes for next subquery execution
    if (union_distinct && table->file->ha_enable_indexes(HA_KEY_SWITCH_ALL))
      DBUG_RETURN(true);       /* purecov: inspected */
  }

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    thd->lex->set_current_select(sl);

    if (sl->join->is_executed())
      sl->join->reset();

    // Set limit and offset for each execution:
    set_limit(sl);

    // Execute this query block
    sl->join->exec();
    status= sl->join->error != 0;

    if (sl == union_distinct)
    {
      // This is UNION DISTINCT, so there should be a fake_select_lex
      DBUG_ASSERT(fake_select_lex != NULL);
      if (table->file->ha_disable_indexes(HA_KEY_SWITCH_ALL))
        DBUG_RETURN(true); /* purecov: inspected */
      table->no_keyread= 1;
    }
    if (status)
      DBUG_RETURN(true);

    if (union_result->flush())
      DBUG_RETURN(true); /* purecov: inspected */
  }

  if (fake_select_lex != NULL)
  {
    thd->lex->set_current_select(fake_select_lex);

    int error= table->file->info(HA_STATUS_VARIABLE);
    if (error)
    {
      table->file->print_error(error, MYF(0)); /* purecov: inspected */
      DBUG_RETURN(true);                       /* purecov: inspected */
    }
    // Index might have been used to weedout duplicates for UNION DISTINCT
    table->file->ha_index_or_rnd_end();
    set_limit(fake_select_lex);
    JOIN *join= fake_select_lex->join;
    join->reset();
    join->exec();
    status= join->error != 0;
    fake_select_lex->table_list.empty();
    thd->current_found_rows= (ulonglong)table->file->stats.records;
  }

  thd->lex->set_current_select(lex_select_save);
  DBUG_RETURN(status);
}


/**
  Cleanup this query expression object after preparation or one round
  of execution. After the cleanup, the object can be reused for a
  new round of execution, but a new optimization will be needed before
  the execution.

  @return false if previous execution was successful, and true otherwise
*/

bool st_select_lex_unit::cleanup(bool full)
{
  DBUG_ENTER("st_select_lex_unit::cleanup");

  DBUG_ASSERT(thd == current_thd);

  if (cleaned >= (full ? UC_CLEAN : UC_PART_CLEAN))
    DBUG_RETURN(false);

  cleaned= (full ? UC_CLEAN : UC_PART_CLEAN);

  bool error= false;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    error|= sl->cleanup(full);

  if (fake_select_lex)
    error|= fake_select_lex->cleanup(full);

  // fake_select_lex's table depends on Temp_table_param inside union_result
  if (full && union_result)
  {
    union_result->cleanup();
    delete union_result;
    union_result= NULL; // Safety
    if (table)
      free_tmp_table(thd, table);
    table= NULL; // Safety
  }

  /*
    explain_marker is (mostly) a property determined at prepare time and must
    thus be preserved for the next execution, if this is a prepared statement.
  */

  DBUG_RETURN(error);
}


#ifndef DBUG_OFF
void st_select_lex_unit::assert_not_fully_clean()
{
  DBUG_ASSERT(cleaned < UC_CLEAN);
  SELECT_LEX *sl= first_select();
  for (;;)
  {
    if (!sl)
    {
      sl= fake_select_lex;
      if (!sl)
        break;
    }
    for (SELECT_LEX_UNIT *lex_unit= sl->first_inner_unit(); lex_unit ;
         lex_unit= lex_unit->next_unit())
      lex_unit->assert_not_fully_clean();
    if (sl == fake_select_lex)
      break;
    else
      sl= sl->next_select();
  }
}
#endif


void st_select_lex_unit::reinit_exec_mechanism()
{
  prepared= optimized= executed= false;
#ifndef DBUG_OFF
  if (is_union())
  {
    List_iterator_fast<Item> it(item_list);
    Item *field;
    while ((field= it++))
    {
      /*
	we can't cleanup here, because it broke link to temporary table field,
	but have to drop fixed flag to allow next fix_field of this field
	during re-executing
      */
      field->fixed= 0;
    }
  }
#endif
}


/**
  Change the query result object used to return the final result of
  the unit, replacing occurences of old_result with new_result.

  @param new_result New query result object
  @param old_result Old query result object

  @retval false Success
  @retval true  Error
*/

bool
st_select_lex_unit::change_query_result(Query_result_interceptor *new_result,
                                        Query_result_interceptor *old_result)
{
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl->query_result() && sl->change_query_result(new_result, old_result))
      return true; /* purecov: inspected */
  }
  set_query_result(new_result);
  return false;
}

/**
  Get column type information for this query expression.

  For a single query block the column types are taken from the list
  of selected items of this block.

  For a union this function assumes that st_select_lex_unit::prepare()
  has been called and returns the type holders that were created for unioned
  column types of all query blocks.

  @note
    The implementation of this function should be in sync with
    st_select_lex_unit::prepare()

  @returns List of items as specified in function description
*/

List<Item> *st_select_lex_unit::get_unit_column_types()
{
  DBUG_ASSERT(is_prepared());

  return is_union() ? &types : &first_select()->item_list;
}


/**
  Get field list for this query expression.

  For a UNION of query blocks, return the field list generated during prepare.
  For a single query block, return the field list after all possible
  intermediate query processing steps are done (optimization is complete).

  @returns List containing fields of the query expression.
*/

List<Item> *st_select_lex_unit::get_field_list()
{
  DBUG_ASSERT(is_optimized());

  return is_union() ? &types : first_select()->join->fields;
}


/**
  Cleanup after preparation or one round of execution.

  @return false if previous execution was successful, and true otherwise
*/

bool st_select_lex::cleanup(bool full)
{
  DBUG_ENTER("st_select_lex::cleanup()");

  bool error= false;
  if (join)
  {
    if (full)
    {
      DBUG_ASSERT(join->select_lex == this);
      error= join->destroy();
      delete join;
      join= NULL;
    }
    else
      join->cleanup();
  }

  for (SELECT_LEX_UNIT *lex_unit= first_inner_unit(); lex_unit ;
       lex_unit= lex_unit->next_unit())
  {
    error|= lex_unit->cleanup(full);
  }
  inner_refs_list.empty();
  DBUG_RETURN(error);
}


void st_select_lex::cleanup_all_joins()
{
  if (join)
    join->cleanup();

  for (SELECT_LEX_UNIT *unit= first_inner_unit(); unit; unit= unit->next_unit())
  {
    for (SELECT_LEX *sl= unit->first_select(); sl; sl= sl->next_select())
      sl->cleanup_all_joins();
  }
}

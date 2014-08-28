/* Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.

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
  UNION  of select's
  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/


#include "sql_priv.h"
#include "unireg.h"
#include "sql_union.h"
#include "sql_select.h"
#include "sql_cursor.h"
#include "sql_base.h"                           // fill_record
#include "filesort.h"                           // filesort_free_buffers
#include "sql_tmp_table.h"                      // tmp tables
#include "sql_optimizer.h"                      // JOIN
#include "opt_explain.h"                        // explain_no_table
#include "opt_explain_format.h"


/**
   Optimize all inner units which are already prepared.
*/
bool mysql_optimize_prepared_inner_units(THD *thd, SELECT_LEX_UNIT *unit,
                                         ulong options)
{
  SELECT_LEX *select_lex= unit->first_select();
  do
  {
    if (!select_lex)
    {
      if (unit->fake_select_lex)
        select_lex= unit->fake_select_lex;
      else
        break;
    }
    for (SELECT_LEX_UNIT *unt= select_lex->first_inner_unit();
         unt;
         unt= unt->next_unit())
    {
      if (unt->first_select_prepared() &&
          // result=NULL is meaningless but will not be used
          mysql_union_prepare_and_optimize(thd, thd->lex, NULL, unt, options))
        return true; /* purecov: inspected */
    }
    if (select_lex == unit->fake_select_lex)
      break;
    select_lex= select_lex->next_select();
  } while (true);
  return false;
}

/**
  Prepare and optimize all selects in the union.

  @param thd           thread handler
  @param lex           lex handler
  @param result        select result for top unit
  @param unit          union to prepare and optimize
  @param union_options options to pass to prepare

  @returns
    false - ok
    true  - error
*/

bool mysql_union_prepare_and_optimize(THD *thd, LEX *lex,
                                      select_result *result,
                                      SELECT_LEX_UNIT *unit,
                                      ulong union_options)
{
  DBUG_ASSERT(thd == unit->thd);

  if (!unit->optimized)
  {
    if (unit->is_union() || unit->fake_select_lex)
    {
      if (unit->prepare(thd, result, SELECT_NO_UNLOCK | union_options))
        return true; /* purecov: inspected */

      /*
        In case of non-EXPLAIN statement tables are not locked at this point,
        it means that we have delayed this step until after prepare stage (i.e.
        this moment). This allows to do better partition pruning and avoid locking
        unused partitions.
        As a consequence, in such a case, prepare stage can rely only on
        metadata about tables used and not data from them.
        We need to lock tables now in order to proceed with the remaning
        stages of query optimization and execution.
      */
      if (!thd->lex->is_query_tables_locked() &&
          lock_tables(thd, lex->query_tables, lex->table_count, 0))
        return true; /* purecov: inspected */

      /*
        Tables must be locked before storing the query in the query cache.
        Transactional engines must been signalled that the statement started,
        which external_lock signals.
      */
      query_cache.store_query(thd, thd->lex->query_tables);

      if (unit->optimize())
        return true;
    }
    else
    {
      SELECT_LEX *const first= unit->first_select();
      SELECT_LEX *const select_save= thd->lex->current_select();
      bool free_join= false; // It's ignored for EXPLAIN
      thd->lex->set_current_select(first);
      unit->set_limit(unit->global_parameters());
      if (mysql_prepare_and_optimize_select(thd,
                        first->item_list,
                        (first->options | thd->variables.option_bits |
                           union_options),
                        result, first, &free_join))
        return true;
      thd->lex->set_current_select(select_save);
      unit->optimized= true;
    }
  }
  /*
    The preparation of a unit automatically prepares, through
    mysql_derived_prepare () or Item_subselect::fix_fields(), all inner
    units which will be needed. Other inner units can be considered as
    eliminated without evaluation thus we don't prepare them here. 
    For example:
        SELECT (subquery) etc. ORDER BY (same subquery);
    contains two inner units but the ORDER BY one will not be used (its
    Item_subselect is replaced by a reference to the SELECT list subquery).
  */
  if (mysql_optimize_prepared_inner_units(thd, unit, union_options))
    return true; /* purecov: inspected */
  return false;
}


/**
  Entry point for handling UNIONs: prepare, optimize and execute.

  @param thd           thread handler
  @param lex           lex handler
  @param result        select result
  @param unit          union to prepare and optimize
  @param union_options options to pass to prepare

  @returns
    false - ok
    true  - error
*/

bool mysql_union(THD *thd, LEX *lex, select_result *result,
                 SELECT_LEX_UNIT *unit, ulong setup_tables_done_option)
{
  bool res;
  DBUG_ENTER("mysql_union");

  res= (mysql_union_prepare_and_optimize(thd, lex, result, unit,
                                         setup_tables_done_option) ||
        unit->exec());
  /* Do partial cleanup (preserve plans for EXPLAIN) otherwise. */
  res|= unit->cleanup(false);
  DBUG_RETURN(res);
}

/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

int select_union::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;
  return 0;
}


bool select_union::send_data(List<Item> &values)
{
  int error= 0;
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  fill_record(thd, table->field, values,
              (table->hash_field ?
               &table->hash_field_bitmap : NULL), NULL);
  if (thd->is_error())
    return 1;

  if (!check_unique_constraint(table, 0))
    return 0;

  if ((error= table->file->ha_write_row(table->record[0])))
  {
    /* create_ondisk_from_heap will generate error if needed */
    if (!table->file->is_ignorable_error(error) &&
        create_ondisk_from_heap(thd, table, tmp_table_param.start_recinfo, 
                                &tmp_table_param.recinfo, error, TRUE, NULL))
      return 1;
  }
  return 0;
}


bool select_union::send_eof()
{
  return 0;
}


bool select_union::flush()
{
  int error;
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {
    table->file->print_error(error, MYF(0));
    return 1;
  }
  return 0;
}

/*
  Create a temporary table to store the result of select_union.

  SYNOPSIS
    select_union::create_result_table()
      thd                thread handle
      column_types       a list of items used to define columns of the
                         temporary table
      is_union_distinct  if set, the temporary table will eliminate
                         duplicates on insert
      options            create options
      table_alias        name of the temporary table
      bit_fields_as_long convert bit fields to ulonglong

  DESCRIPTION
    Create a temporary table that is used to store the result of a UNION,
    derived table, or a materialized cursor.

  RETURN VALUE
    0                    The table has been created successfully.
    1                    create_tmp_table failed.
*/

bool
select_union::create_result_table(THD *thd_arg, List<Item> *column_types,
                                  bool is_union_distinct, ulonglong options,
                                  const char *table_alias,
                                  bool bit_fields_as_long, bool create_table)
{
  DBUG_ASSERT(table == 0);
  tmp_table_param= Temp_table_param();
  count_field_types(thd_arg->lex->current_select(), &tmp_table_param,
                    *column_types, false, true);
  tmp_table_param.skip_create_table= !create_table;
  tmp_table_param.bit_fields_as_long= bit_fields_as_long;

  if (! (table= create_tmp_table(thd_arg, &tmp_table_param, *column_types,
                                 (ORDER*) 0, is_union_distinct, 1,
                                 options, HA_POS_ERROR, (char*) table_alias)))
    return TRUE;
  if (create_table)
  {
    table->file->extra(HA_EXTRA_WRITE_CACHE);
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table->hash_field)
      table->file->ha_index_init(0, 0);
  }
  return FALSE;
}


/**
  Reset and empty the temporary table that stores the materialized query result.

  @note The cleanup performed here is exactly the same as for the two temp
  tables of JOIN - exec_tmp_table_[1 | 2].
*/

void select_union::cleanup()
{
  if (!table)
    return;
  table->file->extra(HA_EXTRA_RESET_STATE);
  if (table->hash_field)
    table->file->ha_index_or_rnd_end();
  table->file->ha_delete_all_rows();
  free_io_cache(table);
  filesort_free_buffers(table,0);
}


/**
   This is a bit different from is_prepared(). Indeed, single-SELECT does not
   always go through SELECT_LEX_UNIT code, thus it is possible to have a unit
   where "prepared" is false but the single SELECT_LEX inside it has a
   prepared JOIN.
   @todo if we always used SELECT_LEX_UNIT code, this function could be
   replaced with is_prepared().
 */
bool st_select_lex_unit::first_select_prepared()
{
  return first_select()->join != NULL;
}


/**
  Replace the current result with new_result and prepare it.  

  @param new_result New result pointer

  @retval FALSE Success
  @retval TRUE  Error
*/
bool select_union_direct::change_result(select_result *new_result)
{
  result= new_result;
  return (result->prepare(unit->types, unit) || result->prepare2());
}


bool select_union_direct::postponed_prepare(List<Item> &types)
{
  if (result != NULL)
    return (result->prepare(types, unit) || result->prepare2());
  else
    return false;
}


bool select_union_direct::send_result_set_metadata(List<Item> &list, uint flags)
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


bool select_union_direct::send_data(List<Item> &items)
{
  if (!limit)
    return false;
  limit--;
  if (offset)
  {
    offset--;
    return false;
  }

  fill_record(thd, table->field, items, NULL, NULL);
  if (thd->is_error())
    return true; /* purecov: inspected */

  return result->send_data(unit->item_list);
}


bool select_union_direct::initialize_tables (JOIN *join)
{
  if (done_initialize_tables)
    return false;
  done_initialize_tables= true;

  return result->initialize_tables(join);
}


bool select_union_direct::send_eof()
{
  // Reset for each SELECT_LEX, so accumulate here
  limit_found_rows+= thd->limit_found_rows -
                     thd->lex->current_select()->get_offset();

  if (unit->thd->lex->current_select() == last_select_lex)
  {
    thd->limit_found_rows= limit_found_rows;

    // Reset and make ready for re-execution
    done_send_result_set_metadata= false;
    done_initialize_tables= false;

    return result->send_eof();
  }
  else
    return false;
}


/**
  Initialization procedures before fake_select_lex preparation()

  @param thd		 Thread handler
  @param no_const_tables Skip reading const tables. TRUE for EXPLAIN.

  @returns
    TRUE  OOM
    FALSE Ok
*/

bool
st_select_lex_unit::init_prepare_fake_select_lex(THD *thd_arg,
                                                 bool no_const_tables)
{
  DBUG_ENTER("st_select_lex_unit::init_prepare_fake_select_lex");
  DBUG_ASSERT(thd_arg->lex->current_select() == fake_select_lex);
  fake_select_lex->table_list.link_in_list(&result_table_list,
                                           &result_table_list.next_local);
  fake_select_lex->context.table_list= 
    fake_select_lex->context.first_name_resolution_table= 
    fake_select_lex->get_table_list();
  if (!fake_select_lex->first_execution)
  {
    for (ORDER *order= global_parameters()->order_list.first;
         order;
         order= order->next)
      order->item= &order->item_ptr;
  }
  for (ORDER *order= global_parameters()->order_list.first;
       order;
       order=order->next)
  {
    (*order->item)->walk(&Item::change_context_processor,
                         Item::WALK_POSTFIX,
                         (uchar*) &fake_select_lex->context);
  }
  if (!fake_select_lex->join)
  {
    /*
      allocate JOIN for fake select only once (prevent
      mysql_select automatic allocation)
      TODO: The above is nonsense. mysql_select() will not allocate the
      join if one already exists. There must be some other reason why we
      don't let it allocate the join. Perhaps this is because we need
      some special parameter values passed to join constructor?
    */
    /*
      We can come here from st_select_lex_unit::exec(), at this moment
      explain_query_specification(fake_select_lex) may be running and thus
      read fake_select_lex->join so we must use set_join():
    */
    JOIN *new_join;
    if (!(new_join=
          new JOIN(thd, item_list,
                   fake_select_lex->options | found_rows_for_union, result)))
    {
      fake_select_lex->table_list.empty();
      DBUG_RETURN(true);
    }
    fake_select_lex->set_join(new_join);
    fake_select_lex->join->no_const_tables= no_const_tables;

    /*
      Fake st_select_lex should have item list for correct ref_array
      allocation.
    */
    fake_select_lex->item_list= item_list;

    /*
      We need to add up n_sum_items in order to make the correct
      allocation in setup_ref_array().
      Don't add more sum_items if we have already done SELECT_LEX::prepare
      for this (with a different join object)
    */
    if (fake_select_lex->ref_pointer_array.is_null())
      fake_select_lex->n_child_sum_items+= global_parameters()->n_sum_items;
  }
  DBUG_RETURN(false);
}


/**
   Prepares the unit.

   @param thd_arg
   @param sel_result         Result object where the unit's output should go.
   @param additional_options The JOIN constructor will be passed the union of
   these flags, those of THD, those of SELECT_LEX.

   @todo Arguments are ignored if the unit is already prepared. This is
   unfriendly to the users of this function.
 */
bool st_select_lex_unit::prepare(THD *thd_arg, select_result *sel_result,
                                 ulong additional_options)
{
  SELECT_LEX *lex_select_save= thd_arg->lex->current_select();
  SELECT_LEX *sl, *first_sl= first_select();
  select_result *tmp_result;
  bool is_union_select;
  bool instantiate_tmp_table= false;
  DBUG_ENTER("st_select_lex_unit::prepare");

  if (prepared)
    DBUG_RETURN(FALSE);

  prepared= 1;
  saved_error= FALSE;
  result= sel_result;

  thd_arg->lex->set_current_select(sl= first_sl);
  found_rows_for_union= first_sl->options & OPTION_FOUND_ROWS;
  is_union_select= is_union() || fake_select_lex;

  // Save fake_select_lex in case we don't need it for anything but
  // global parameters.
  mysql_mutex_lock(&thd->LOCK_query_plan);
  if (saved_fake_select_lex == NULL) // Don't overwrite on PS second prepare
    saved_fake_select_lex= fake_select_lex;
  mysql_mutex_unlock(&thd->LOCK_query_plan);



  /* Global option */

  if (is_union_select)
  {
    if (is_union() && !union_needs_tmp_table())
    {
      SELECT_LEX *last= first_select();
      while (last->next_select())
        last= last->next_select();
      if (!(tmp_result= union_result= new select_union_direct(sel_result, last)))
        goto err; /* purecov: inspected */
      mysql_mutex_lock(&thd->LOCK_query_plan);
      fake_select_lex= NULL;
      mysql_mutex_unlock(&thd->LOCK_query_plan);
      instantiate_tmp_table= false;
    }
    else
    {
      if (!(tmp_result= union_result= new select_union()))
        goto err; /* purecov: inspected */
      instantiate_tmp_table= true;
    }
  }
  else
    tmp_result= sel_result;

  sl->context.resolve_in_select_list= TRUE;

  for (;sl; sl= sl->next_select())
  {
    sl->options|=  SELECT_NO_UNLOCK;
    JOIN *join= new JOIN(thd_arg, sl->item_list, 
			 sl->options | thd_arg->variables.option_bits | additional_options,
			 tmp_result);
    /*
      setup_tables_done_option should be set only for very first SELECT,
      because it protect from secont setup_tables call for select-like non
      select commands (DELETE/INSERT/...) and they use only very first
      SELECT (for union it can be only INSERT ... SELECT).
    */
    additional_options&= ~OPTION_SETUP_TABLES_DONE;
    if (!join)
      goto err;

    thd_arg->lex->set_current_select(sl);

    if (is_union_select && !(sl->braces && sl->explicit_limit))
      sl->order_list.empty();      // Can skip ORDER BY

    saved_error= sl->prepare(join);
    /* There are no * in the statement anymore (for PS) */
    sl->with_wild= 0;

    if (saved_error || (saved_error= thd_arg->is_fatal_error))
      goto err;
    /*
      Use items list of underlaid select for derived tables to preserve
      information about fields lengths and exact types
    */
    if (!is_union_select)
      types= first_sl->item_list;
    else if (sl == first_sl)
    {
      types.empty();
      List_iterator_fast<Item> it(sl->item_list);
      Item *item_tmp;
      while ((item_tmp= it++))
      {
	/* Error's in 'new' will be detected after loop */
	types.push_back(new Item_type_holder(thd_arg, item_tmp));
      }

      if (thd_arg->is_fatal_error)
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
	  DBUG_RETURN(TRUE);
      }
    }
  }

  // If the query is using select_union_direct, we have postponed
  // preparation of the underlying select_result until column types
  // are known.
  if (union_result != NULL && union_result->postponed_prepare(types))
    DBUG_RETURN(true);

  if (is_union_select)
  {
    /*
      Check that it was possible to aggregate
      all collations together for UNION.
      We need this in case of UNION DISTINCT, to filter
      out duplicates using the proper collation.

      TODO: consider removing this test in case of UNION ALL.
    */
    List_iterator_fast<Item> tp(types);
    Item *type;
    ulonglong create_options;

    while ((type= tp++))
    {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE)
      {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }
    create_options= (first_sl->options | thd_arg->variables.option_bits |
                     TMP_TABLE_ALL_COLUMNS);
    /*
      Force the temporary table to be a MyISAM table if we're going to use
      fullext functions (MATCH ... AGAINST .. IN BOOLEAN MODE) when reading
      from it (this should be removed in 5.2 when fulltext search is moved 
      out of MyISAM).
    */
    if (global_parameters()->ftfunc_list->elements)
      create_options= create_options | TMP_TABLE_FORCE_MYISAM;

    if (union_result->create_result_table(thd, &types, MY_TEST(union_distinct),
                                          create_options, "", false,
                                          instantiate_tmp_table))
      goto err;
    memset(&result_table_list, 0, sizeof(result_table_list));
    result_table_list.db= (char*) "";
    result_table_list.table_name= result_table_list.alias= (char*) "union";
    result_table_list.table= table= union_result->table;

    if (!item_list.elements)
    {
      {
        Prepared_stmt_arena_holder ps_arena_holder(thd);
        // Create fields list, but skip hash field that could be added at
        // the end.
        saved_error= table->fill_item_list(&item_list, types.elements);

        if (saved_error)
          goto err;
      }

      if (fake_select_lex != NULL && thd->stmt_arena->is_stmt_prepare())
      {
        thd_arg->lex->set_current_select(fake_select_lex);
        /* Validate the global parameters of this union */
        init_prepare_fake_select_lex(thd, false);
        fake_select_lex->set_where_cond(NULL);
        fake_select_lex->set_having_cond(NULL);
        DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
                    fake_select_lex->master_unit() == this &&
                    !fake_select_lex->group_list.elements);
	saved_error= fake_select_lex->prepare(fake_select_lex->join);
	fake_select_lex->table_list.empty();
      }
    }
    else
    {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      table->reset_item_list(&item_list, types.elements);
    }
  }

  thd_arg->lex->set_current_select(lex_select_save);

  DBUG_RETURN(saved_error || thd_arg->is_fatal_error);

err:
  thd_arg->lex->set_current_select(lex_select_save);
  (void) cleanup(false);
  DBUG_RETURN(TRUE);
}


/**
  Run optimization phase.

  @return FALSE unit successfully passed optimization phase.
  @return TRUE an error occur.
*/

bool st_select_lex_unit::optimize()
{
  SELECT_LEX *save_select= thd->lex->current_select();
  DBUG_ENTER("st_select_lex_unit::optimize");

  if (optimized && item && item->assigned() && !uncacheable)
    DBUG_RETURN(FALSE);

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    DBUG_ASSERT(sl->join);
    if (optimized)
    {
      saved_error= false;
      sl->join->reset();
    }
    else
    {
      thd->lex->set_current_select(sl);
      set_limit(sl);
      if (sl == global_parameters() && is_union())
      {
        offset_limit_cnt= 0;
        /*
          We can't use LIMIT at this stage if we are using ORDER BY for the
          whole UNION.
        */
        if (sl->order_list.first)
          select_limit_cnt= HA_POS_ERROR;
      }

      /*
        When using braces, SQL_CALC_FOUND_ROWS affects the whole query:
        we don't calculate found_rows() per union part.
        Otherwise, SQL_CALC_FOUND_ROWS should be done on all sub parts.
      */
      sl->join->select_options= 
        (select_limit_cnt == HA_POS_ERROR || sl->braces) ?
        sl->options & ~OPTION_FOUND_ROWS : sl->options | found_rows_for_union;

      saved_error= sl->join->optimize();
      /*
        Accumulate estimated number of rows. Notice that an implicitly grouped
        query has one row (with HAVING it has zero or one rows).
      */
      if (result)
        result->estimated_rowcount+=
          sl->with_sum_func && sl->group_list.elements == 0 ?
            1 :  sl->join->best_rowcount;
    }
    if (saved_error)
      break;
  }
  if (fake_select_lex)
  {
    thd->lex->set_current_select(fake_select_lex);
    set_limit(fake_select_lex);
    fake_select_lex->options|= SELECT_NO_UNLOCK;
    if (init_prepare_fake_select_lex(thd, true) ||
        thd->is_fatal_error)
      DBUG_RETURN(true); /* purecov: inspected */

    JOIN *const join= fake_select_lex->join;

    /*
      In EXPLAIN command, constant subqueries that do not use any
      tables are executed two times:
       - 1st time is a real evaluation to get the subquery value
       - 2nd time is to produce EXPLAIN output rows.
      1st execution sets certain members (e.g. select_result) to perform
      subquery execution rather than EXPLAIN line production. In order 
      to reset them back, we re-do all of the actions (yes it is ugly).
    */
    if (!join->optimized || !join->tables)
    {
      bool dummy= false;
      /*
        IN(SELECT UNION SELECT)->EXISTS injects an equality in
        fake_select_lex->where_cond, which is meaningless.
      */
      fake_select_lex->set_where_cond(NULL);
      fake_select_lex->set_having_cond(NULL);
      DBUG_ASSERT(fake_select_lex->with_wild == 0 &&
                  fake_select_lex->master_unit() == this &&
                  !fake_select_lex->group_list.elements &&
                  fake_select_lex->get_table_list() == &result_table_list);
      saved_error= mysql_prepare_and_optimize_select(thd,
                            item_list,
                            fake_select_lex->options | SELECT_NO_UNLOCK,
                            result, fake_select_lex, &dummy);
    }
  }
  if (!saved_error)
    optimized= 1;
  thd->lex->set_current_select(save_select);

  DBUG_RETURN(saved_error);
}


/**
  Explain query starting from this unit.

  @param ethd  THD of explaining thread
*/

bool st_select_lex_unit::explain(THD *ethd)
{
#ifndef DBUG_OFF
  SELECT_LEX *lex_select_save= thd->lex->current_select();
#endif
  Explain_format *fmt= ethd->lex->explain_format;
  DBUG_ENTER("st_select_lex_unit::explain");
  const bool other= (thd != ethd);
  bool ret= false;

  if (!other)
  {
    DBUG_ASSERT((is_union() || fake_select_lex) && optimized);
    executed= true;
  }

  if (fmt->begin_context(CTX_UNION))
    DBUG_RETURN(true);

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (fmt->begin_context(CTX_QUERY_SPEC))
      DBUG_RETURN(true);
    if (explain_query_specification(ethd, sl, CTX_JOIN) ||
        // Explain code runs in ethd, should check it.
        ethd->is_error() ||
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

  if (ret || ethd->is_error())
    DBUG_RETURN(true);
  fmt->end_context(CTX_UNION);

  DBUG_RETURN(false);
}


/**
  Execute UNION.
*/

bool st_select_lex_unit::exec()
{
  SELECT_LEX *lex_select_save= thd->lex->current_select();
  ulonglong add_rows=0;
  DBUG_ENTER("st_select_lex_unit::exec");
  DBUG_ASSERT((is_union() || fake_select_lex) && optimized);

  if (executed && !uncacheable)
    DBUG_RETURN(false);
  executed= true;
  
  if (uncacheable || !item || !item->assigned())
  {
    if (item)
      item->reset_value_registration();
    if (optimized && item)
    {
      if (item->assigned())
      {
        item->assigned(false); // We will reinit & rexecute unit
        item->reset();
        if (table->is_created())
        {
          table->file->ha_delete_all_rows();
          table->file->info(HA_STATUS_VARIABLE);
        }
      }
      /* re-enabling indexes for next subselect iteration */
      if (union_distinct && table->file->ha_enable_indexes(HA_KEY_SWITCH_ALL))
      {
        DBUG_ASSERT(0);
      }
    }

    for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    {
      ha_rows records_at_start= 0;
      DBUG_ASSERT(sl->join);
      thd->lex->set_current_select(sl);
      if (sl->join->is_executed())
      {
        saved_error= false;
        sl->join->reset();
      }

      set_limit(sl);
      if (sl == global_parameters())
      {
        offset_limit_cnt= 0;
        /*
          We can't use LIMIT at this stage if we are using ORDER BY for the
          whole query
        */
        if (sl->order_list.first)
          select_limit_cnt= HA_POS_ERROR;
      }
      if (!saved_error)
      {
        records_at_start= table->file->stats.records;
        sl->join->exec();
        if (sl == union_distinct)
        {
          // This is UNION DISTINCT, so there should be a fake_select_lex
          DBUG_ASSERT(fake_select_lex != NULL);
          if (table->file->ha_disable_indexes(HA_KEY_SWITCH_ALL))
            DBUG_RETURN(true);
          table->no_keyread=1;
        }
        saved_error= sl->join->error;
        offset_limit_cnt= (ha_rows)(sl->offset_limit ?
                                    sl->offset_limit->val_uint() :
                                    0);
        if (!saved_error)
        {
          if (union_result->flush())
          {
            thd->lex->set_current_select(lex_select_save); /* purecov: inspected */
            DBUG_RETURN(true); /* purecov: inspected */
          }
        }
      }
      if (saved_error)
      {
        thd->lex->set_current_select(lex_select_save); /* purecov: inspected */
        DBUG_RETURN(saved_error); /* purecov: inspected */
      }
      if (fake_select_lex != NULL)
      {
        /* Needed for the following test and for records_at_start in next loop */
        int error= table->file->info(HA_STATUS_VARIABLE);
        if(error)
        {
          table->file->print_error(error, MYF(0)); /* purecov: inspected */
          DBUG_RETURN(true); /* purecov: inspected */
        }
      }
      if (found_rows_for_union && !sl->braces && 
          select_limit_cnt != HA_POS_ERROR)
      {
        /*
          This is a union without braces. Remember the number of rows that
          could also have been part of the result set.
          We get this from the difference of between total number of possible
          rows and actual rows added to the temporary table.
        */
        add_rows+= (ulonglong) (thd->limit_found_rows -
                   (ulonglong)(table->file->stats.records - records_at_start));
      }
    }
  }

  if (fake_select_lex != NULL && !saved_error && !thd->is_fatal_error)
  {
    /* Send result to 'result' */
    saved_error= true;
    // Index might have been used to weedout duplicates for UNION DISTINCT
    table->file->ha_index_or_rnd_end();
    set_limit(fake_select_lex);
    JOIN *join= fake_select_lex->join;
    DBUG_ASSERT(join && join->optimized);
    join->examined_rows= 0;
    saved_error= false;
    join->reset();
    join->exec();

    fake_select_lex->table_list.empty();
    thd->limit_found_rows = (ulonglong)table->file->stats.records + add_rows;
  }

  thd->lex->set_current_select(lex_select_save);
  DBUG_RETURN(saved_error);
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
  bool error= false;
  DBUG_ENTER("st_select_lex_unit::cleanup");

  DBUG_ASSERT(thd == current_thd);

  if (cleaned >= (full ? UC_CLEAN : UC_PART_CLEAN))
  {
    DBUG_RETURN(FALSE);
  }
  cleaned= (full ? UC_CLEAN : UC_PART_CLEAN);

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    error|= sl->cleanup(full);

  if (fake_select_lex)
  {
    error|= fake_select_lex->cleanup(full);
  }

  // fake_select_lex's table depends on Temp_table_param inside union_result
  if (full && union_result)
  {
    union_result->cleanup();
    delete union_result;
    union_result=0; // Safety
    if (table)
      free_tmp_table(thd, table);
    table= 0; // Safety
  }

  /*
    explain_marker is (mostly) a property determined at parsing time and must
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
  prepared= optimized= executed= 0;
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
  Change the select_result object used to return the final result of
  the unit, replacing occurences of old_result with new_result.

  @param new_result New select_result object
  @param old_result Old select_result object

  @retval false Success
  @retval true  Error
*/

bool st_select_lex_unit::change_result(select_result_interceptor *new_result,
                                       select_result_interceptor *old_result)
{
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl->join)
      if (sl->join->change_result(new_result, old_result))
	return true; /* purecov: inspected */
  }
  /*
    If there were a fake_select_lex->join, we would have to change the
    result of that also, but change_result() is called before such an
    object is created.
  */
  DBUG_ASSERT(fake_select_lex == NULL || fake_select_lex->join == NULL);
  return false;
}

/*
  Get column type information for this unit.

  SYNOPSIS
    st_select_lex_unit::get_unit_column_types()

  DESCRIPTION
    For a single-select the column types are taken
    from the list of selected items. For a union this function
    assumes that st_select_lex_unit::prepare has been called
    and returns the type holders that were created for unioned
    column types of all selects.

  NOTES
    The implementation of this function should be in sync with
    st_select_lex_unit::prepare()
*/

List<Item> *st_select_lex_unit::get_unit_column_types()
{
  if (is_union())
  {
    DBUG_ASSERT(prepared);
    /* Types are generated during prepare */
    return &types;
  }

  return &first_select()->item_list;
}


/**
  Get field list for this query expression.

  For a UNION of query blocks, return the field list generated
  during prepare.
  For a single query block, return the field list after all possible
  intermediate query processing steps are completed.

  @returns List containing fields of the query expression.
*/

List<Item> *st_select_lex_unit::get_field_list()
{
  if (is_union())
  {
    DBUG_ASSERT(prepared);
    /* Types are generated during prepare */
    return &types;
  }

  return first_select()->join->fields;
}


/**
  Cleanup after preparation or one round of execution.

  @return false if previous execution was successful, and true otherwise
*/

bool st_select_lex::cleanup(bool full)
{
  bool error= FALSE;
  DBUG_ENTER("st_select_lex::cleanup()");

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
  SELECT_LEX_UNIT *unit;
  SELECT_LEX *sl;

  if (join)
    join->cleanup();

  for (unit= first_inner_unit(); unit; unit= unit->next_unit())
    for (sl= unit->first_select(); sl; sl= sl->next_select())
      sl->cleanup_all_joins();
}

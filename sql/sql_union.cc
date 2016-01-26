/* Copyright (c) 2000, 2014, Oracle and/or its affiliates.
   Copyright (c) 2010, 2014, SkySQL Ab.

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

bool mysql_union(THD *thd, LEX *lex, select_result *result,
                 SELECT_LEX_UNIT *unit, ulong setup_tables_done_option)
{
  DBUG_ENTER("mysql_union");
  bool res;
  if (!(res= unit->prepare(thd, result, SELECT_NO_UNLOCK |
                           setup_tables_done_option)))
    res= unit->exec();
  res|= unit->cleanup();
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


int select_union::send_data(List<Item> &values)
{
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  if (thd->killed == ABORT_QUERY)
    return 0;
  if (table->no_rows_with_nulls)
    table->null_catch_flags= CHECK_ROW_FOR_NULLS_TO_REJECT;
  fill_record(thd, table->field, values, TRUE, FALSE);
  if (thd->is_error())
    return 1;
  if (table->no_rows_with_nulls)
  {
    table->null_catch_flags&= ~CHECK_ROW_FOR_NULLS_TO_REJECT;
    if (table->null_catch_flags)
      return 0;
  }

  if ((write_err= table->file->ha_write_tmp_row(table->record[0])))
  {
    if (write_err == HA_ERR_FOUND_DUPP_KEY)
    {
      /*
        Inform upper level that we found a duplicate key, that should not
        be counted as part of limit
      */
      return -1;
    }
    bool is_duplicate= FALSE;
    /* create_internal_tmp_table_from_heap will generate error if needed */
    if (table->file->is_fatal_error(write_err, HA_CHECK_DUP) &&
        create_internal_tmp_table_from_heap(thd, table,
                                            tmp_table_param.start_recinfo, 
                                            &tmp_table_param.recinfo,
                                            write_err, 1, &is_duplicate))
      return 1;
    if (is_duplicate)
      return -1;
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
      create_table       whether to physically create result table
      keep_row_order     keep rows in order as they were inserted

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
                                  const char *alias,
                                  bool bit_fields_as_long, bool create_table,
                                  bool keep_row_order)
{
  DBUG_ASSERT(table == 0);
  tmp_table_param.init();
  tmp_table_param.field_count= column_types->elements;
  tmp_table_param.bit_fields_as_long= bit_fields_as_long;

  if (! (table= create_tmp_table(thd_arg, &tmp_table_param, *column_types,
                                 (ORDER*) 0, is_union_distinct, 1,
                                 options, HA_POS_ERROR, alias,
                                 !create_table, keep_row_order)))
    return TRUE;

  table->keys_in_use_for_query.clear_all();
  for (uint i=0; i < table->s->fields; i++)
    table->field[i]->flags &= ~PART_KEY_FLAG;

  if (create_table)
  {
    table->file->extra(HA_EXTRA_WRITE_CACHE);
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
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
  table->file->extra(HA_EXTRA_RESET_STATE);
  table->file->ha_delete_all_rows();
  free_io_cache(table);
  filesort_free_buffers(table,0);
}


/*
  initialization procedures before fake_select_lex preparation()

  SYNOPSIS
    st_select_lex_unit::init_prepare_fake_select_lex()
    thd		- thread handler
    first_execution - TRUE at the first execution of the union 

  RETURN
    options of SELECT
*/

void
st_select_lex_unit::init_prepare_fake_select_lex(THD *thd_arg,
                                                  bool first_execution) 
{
  thd_arg->lex->current_select= fake_select_lex;
  fake_select_lex->table_list.link_in_list(&result_table_list,
                                           &result_table_list.next_local);
  fake_select_lex->context.table_list= 
    fake_select_lex->context.first_name_resolution_table= 
    fake_select_lex->get_table_list();
  /*
    The flag fake_select_lex->first_execution indicates whether this is
    called at the first execution of the statement, while first_execution
    shows whether this is called at the first execution of the union that
    may form just a subselect.
  */    
  if (!fake_select_lex->first_execution && first_execution)
  {
    for (ORDER *order= global_parameters->order_list.first;
         order;
         order= order->next)
      order->item= &order->item_ptr;
  }
  for (ORDER *order= global_parameters->order_list.first;
       order;
       order=order->next)
  {
    (*order->item)->walk(&Item::change_context_processor, 0,
                         (uchar*) &fake_select_lex->context);
    (*order->item)->walk(&Item::set_fake_select_as_master_processor, 0,
                         (uchar*) fake_select_lex);
  }
}


bool st_select_lex_unit::prepare(THD *thd_arg, select_result *sel_result,
                                 ulong additional_options)
{
  SELECT_LEX *lex_select_save= thd_arg->lex->current_select;
  SELECT_LEX *sl, *first_sl= first_select();
  select_result *tmp_result;
  bool is_union_select;
  DBUG_ENTER("st_select_lex_unit::prepare");

  describe= test(additional_options & SELECT_DESCRIBE);

  /*
    result object should be reassigned even if preparing already done for
    max/min subquery (ALL/ANY optimization)
  */
  result= sel_result;

  if (prepared)
  {
    if (describe)
    {
      /* fast reinit for EXPLAIN */
      for (sl= first_sl; sl; sl= sl->next_select())
      {
	sl->join->result= result;
	select_limit_cnt= HA_POS_ERROR;
	offset_limit_cnt= 0;
	if (!sl->join->procedure &&
	    result->prepare(sl->join->fields_list, this))
	{
	  DBUG_RETURN(TRUE);
	}
	sl->join->select_options|= SELECT_DESCRIBE;
	sl->join->reinit();
      }
    }
    DBUG_RETURN(FALSE);
  }
  prepared= 1;
  saved_error= FALSE;
  
  thd_arg->lex->current_select= sl= first_sl;
  found_rows_for_union= first_sl->options & OPTION_FOUND_ROWS;
  is_union_select= is_union() || fake_select_lex;

  /* Global option */

  if (is_union_select)
  {
    if (!(tmp_result= union_result= new select_union))
      goto err;
    if (describe)
      tmp_result= sel_result;
  }
  else
    tmp_result= sel_result;

  sl->context.resolve_in_select_list= TRUE;

  for (;sl; sl= sl->next_select())
  {
    bool can_skip_order_by;
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

    thd_arg->lex->current_select= sl;

    can_skip_order_by= is_union_select && !(sl->braces && sl->explicit_limit);

    saved_error= join->prepare(&sl->ref_pointer_array,
                               sl->table_list.first,
                               sl->with_wild,
                               sl->where,
                               (can_skip_order_by ? 0 :
                                sl->order_list.elements) +
                               sl->group_list.elements,
                               can_skip_order_by ?
                               NULL : sl->order_list.first,
                               can_skip_order_by,
                               sl->group_list.first,
                               sl->having,
                               (is_union_select ? NULL :
                                thd_arg->lex->proc_list.first),
                               sl, this);

    /* There are no * in the statement anymore (for PS) */
    sl->with_wild= 0;
    last_procedure= join->procedure;

    if (saved_error || (saved_error= thd_arg->is_fatal_error))
      goto err;
    /*
      Remove all references from the select_lex_units to the subqueries that
      are inside the ORDER BY clause.
    */
    if (can_skip_order_by)
    {
      for (ORDER *ord= (ORDER *)sl->order_list.first; ord; ord= ord->next)
      {
        (*ord->item)->walk(&Item::eliminate_subselect_processor, FALSE, NULL);
      }
    }

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

  if (is_union_select)
  {
    /*
      Check that it was possible to aggregate
      all collations together for UNION.
    */
    List_iterator_fast<Item> tp(types);
    Item *type;
    ulonglong create_options;
    uint save_tablenr= 0;
    table_map save_map= 0;
    uint save_maybe_null= 0;

    while ((type= tp++))
    {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE)
      {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }
    
    /*
      Disable the usage of fulltext searches in the last union branch.
      This is a temporary 5.x limitation because of the way the fulltext
      search functions are handled by the optimizer.
      This is manifestation of the more general problems of "taking away"
      parts of a SELECT statement post-fix_fields(). This is generally not
      doable since various flags are collected in various places (e.g. 
      SELECT_LEX) that carry information about the presence of certain 
      expressions or constructs in the parts of the query.
      When part of the query is taken away it's not clear how to "divide" 
      the meaning of these accumulated flags and what to carry over to the
      recipient query (SELECT_LEX).
    */
    if (global_parameters->ftfunc_list->elements && 
        global_parameters->order_list.elements &&
        global_parameters != fake_select_lex)
    {
      ORDER *ord;
      Item_func::Functype ft=  Item_func::FT_FUNC;
      for (ord= global_parameters->order_list.first; ord; ord= ord->next)
        if ((*ord->item)->walk (&Item::find_function_processor, FALSE, 
                                (uchar *) &ft))
        {
          my_error (ER_CANT_USE_OPTION_HERE, MYF(0), "MATCH()");
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
    if (global_parameters->ftfunc_list->elements)
      create_options= create_options | TMP_TABLE_FORCE_MYISAM;

    if (union_result->create_result_table(thd, &types, test(union_distinct),
                                          create_options, "", FALSE, TRUE))
      goto err;
    if (fake_select_lex && !fake_select_lex->first_cond_optimization)
    {
      save_tablenr= result_table_list.tablenr_exec;
      save_map= result_table_list.map_exec;
      save_maybe_null= result_table_list.maybe_null_exec;
    }
    bzero((char*) &result_table_list, sizeof(result_table_list));
    result_table_list.db= (char*) "";
    result_table_list.table_name= result_table_list.alias= (char*) "union";
    result_table_list.table= table= union_result->table;
    if (fake_select_lex && !fake_select_lex->first_cond_optimization)
    {
      result_table_list.tablenr_exec= save_tablenr;
      result_table_list.map_exec= save_map;
      result_table_list.maybe_null_exec= save_maybe_null;
    }

    thd_arg->lex->current_select= lex_select_save;
    if (!item_list.elements)
    {
      Query_arena *arena, backup_arena;

      arena= thd->activate_stmt_arena_if_needed(&backup_arena);
      
      saved_error= table->fill_item_list(&item_list);

      if (arena)
        thd->restore_active_arena(arena, &backup_arena);

      if (saved_error)
        goto err;

      if (thd->stmt_arena->is_stmt_prepare())
      {
        /* Validate the global parameters of this union */

	init_prepare_fake_select_lex(thd, TRUE);
        /* Should be done only once (the only item_list per statement) */
        DBUG_ASSERT(fake_select_lex->join == 0);
	if (!(fake_select_lex->join= new JOIN(thd, item_list, thd->variables.option_bits,
					      result)))
	{
	  fake_select_lex->table_list.empty();
	  DBUG_RETURN(TRUE);
	}

        /*
          Fake st_select_lex should have item list for correct ref_array
          allocation.
        */
	fake_select_lex->item_list= item_list;

	thd_arg->lex->current_select= fake_select_lex;

        /*
          We need to add up n_sum_items in order to make the correct
          allocation in setup_ref_array().
        */
        fake_select_lex->n_child_sum_items+= global_parameters->n_sum_items;

	saved_error= fake_select_lex->join->
	  prepare(&fake_select_lex->ref_pointer_array,
		  fake_select_lex->table_list.first,
		  0, 0,
                  global_parameters->order_list.elements, // og_num
                  global_parameters->order_list.first,    // order
                  false, NULL, NULL, NULL,
		  fake_select_lex, this);
	fake_select_lex->table_list.empty();
      }
    }
    else
    {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      table->reset_item_list(&item_list);
    }
  }

  thd_arg->lex->current_select= lex_select_save;

  DBUG_RETURN(saved_error || thd_arg->is_fatal_error);

err:
  thd_arg->lex->current_select= lex_select_save;
  (void) cleanup();
  DBUG_RETURN(TRUE);
}


/**
  Run optimization phase.

  @return FALSE unit successfully passed optimization phase.
  @return TRUE an error occur.
*/
bool st_select_lex_unit::optimize()
{
  SELECT_LEX *lex_select_save= thd->lex->current_select;
  SELECT_LEX *select_cursor=first_select();
  DBUG_ENTER("st_select_lex_unit::optimize");

  if (optimized && !uncacheable && !describe)
    DBUG_RETURN(FALSE);

  if (uncacheable || !item || !item->assigned() || describe)
  {
    if (item)
      item->reset_value_registration();
    if (optimized && item)
    {
      if (item->assigned())
      {
        item->assigned(0); // We will reinit & rexecute unit
        item->reset();
        table->file->ha_delete_all_rows();
      }
      /* re-enabling indexes for next subselect iteration */
      if (union_distinct && table->file->ha_enable_indexes(HA_KEY_SWITCH_ALL))
      {
        DBUG_ASSERT(0);
      }
    }
    for (SELECT_LEX *sl= select_cursor; sl; sl= sl->next_select())
    {
      thd->lex->current_select= sl;

      if (optimized)
	saved_error= sl->join->reinit();
      else
      {
        set_limit(sl);
	if (sl == global_parameters || describe)
	{
	  offset_limit_cnt= 0;
	  /*
	    We can't use LIMIT at this stage if we are using ORDER BY for the
	    whole query
	  */
	  if (sl->order_list.first || describe)
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
      }

      if (saved_error)
      {
	thd->lex->current_select= lex_select_save;
	DBUG_RETURN(saved_error);
      }
    }
  }
  optimized= 1;

  thd->lex->current_select= lex_select_save;
  DBUG_RETURN(saved_error);
}


bool st_select_lex_unit::exec()
{
  SELECT_LEX *lex_select_save= thd->lex->current_select;
  SELECT_LEX *select_cursor=first_select();
  ulonglong add_rows=0;
  ha_rows examined_rows= 0;
  bool first_execution= !executed;
  DBUG_ENTER("st_select_lex_unit::exec");

  if (executed && !uncacheable && !describe)
    DBUG_RETURN(FALSE);
  executed= 1;
  if (!(uncacheable & ~UNCACHEABLE_EXPLAIN) && item)
    item->make_const();
  
  if ((saved_error= optimize()))
    DBUG_RETURN(saved_error);

  if (uncacheable || !item || !item->assigned() || describe)
  {
    for (SELECT_LEX *sl= select_cursor; sl; sl= sl->next_select())
    {
      ha_rows records_at_start= 0;
      thd->lex->current_select= sl;
      if (sl != &thd->lex->select_lex)
        fake_select_lex->uncacheable|= sl->uncacheable;
      else
        fake_select_lex->uncacheable= 0;

      {
        set_limit(sl);
	if (sl == global_parameters || describe)
	{
	  offset_limit_cnt= 0;
	  /*
	    We can't use LIMIT at this stage if we are using ORDER BY for the
	    whole query
	  */
	  if (sl->order_list.first || describe)
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
      }
      if (!saved_error)
      {
	records_at_start= table->file->stats.records;
	sl->join->exec();
        if (sl == union_distinct)
	{
	  if (table->file->ha_disable_indexes(HA_KEY_SWITCH_ALL))
	    DBUG_RETURN(TRUE);
	  table->no_keyread=1;
	}
	saved_error= sl->join->error;
	offset_limit_cnt= (ha_rows)(sl->offset_limit ?
                                    sl->offset_limit->val_uint() :
                                    0);
	if (!saved_error)
	{
          /*
            Save the current examined row count locally and clear the global
            counter, so that we can accumulate the number of evaluated rows for
            the current query block.
          */
	  examined_rows+= thd->examined_row_count;
          thd->examined_row_count= 0;
	  if (union_result->flush())
	  {
	    thd->lex->current_select= lex_select_save;
	    DBUG_RETURN(1);
	  }
	}
      }
      if (saved_error)
      {
	thd->lex->current_select= lex_select_save;
	DBUG_RETURN(saved_error);
      }
      /* Needed for the following test and for records_at_start in next loop */
      int error= table->file->info(HA_STATUS_VARIABLE);
      if(error)
      {
        table->file->print_error(error, MYF(0));
        DBUG_RETURN(1);
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
	add_rows+= (ulonglong) (thd->limit_found_rows - (ulonglong)
			      ((table->file->stats.records -  records_at_start)));
      }
      if (thd->killed == ABORT_QUERY)
      {
        /*
          Stop execution of the remaining queries in the UNIONS, and produce
          the current result.
        */
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_QUERY_EXCEEDED_ROWS_EXAMINED_LIMIT,
                            ER(ER_QUERY_EXCEEDED_ROWS_EXAMINED_LIMIT),
                            thd->accessed_rows_and_keys,
                            thd->lex->limit_rows_examined->val_uint());
        thd->reset_killed();
        break;
      }
    }
  }

  /* Send result to 'result' */
  saved_error= TRUE;
  {
    List<Item_func_match> empty_list;
    empty_list.empty();
    /*
      Disable LIMIT ROWS EXAMINED in order to produce the possibly incomplete
      result of the UNION without interruption due to exceeding the limit.
    */
    thd->lex->limit_rows_examined_cnt= ULONGLONG_MAX;

    if (!thd->is_fatal_error)				// Check if EOM
    {
      set_limit(global_parameters);
      init_prepare_fake_select_lex(thd, first_execution);
      JOIN *join= fake_select_lex->join;
      if (!join)
      {
	/*
	  allocate JOIN for fake select only once (prevent
	  mysql_select automatic allocation)
          TODO: The above is nonsense. mysql_select() will not allocate the
          join if one already exists. There must be some other reason why we
          don't let it allocate the join. Perhaps this is because we need
          some special parameter values passed to join constructor?
	*/
	if (!(fake_select_lex->join= new JOIN(thd, item_list,
					      fake_select_lex->options, result)))
	{
	  fake_select_lex->table_list.empty();
	  goto err;
	}
        fake_select_lex->join->no_const_tables= TRUE;

        /*
          Fake st_select_lex should have item list for correct ref_array
          allocation.
        */
        fake_select_lex->item_list= item_list;

        /*
          We need to add up n_sum_items in order to make the correct
          allocation in setup_ref_array().
          Don't add more sum_items if we have already done JOIN::prepare
          for this (with a different join object)
        */
        if (!fake_select_lex->ref_pointer_array)
          fake_select_lex->n_child_sum_items+= global_parameters->n_sum_items;

        saved_error= mysql_select(thd, &fake_select_lex->ref_pointer_array,
                              &result_table_list,
                              0, item_list, NULL,
                              global_parameters->order_list.elements,
                              global_parameters->order_list.first,
                              NULL, NULL, NULL,
                              fake_select_lex->options | SELECT_NO_UNLOCK,
                              result, this, fake_select_lex);
      }
      else
      {
        if (describe)
        {
          /*
            In EXPLAIN command, constant subqueries that do not use any
            tables are executed two times:
             - 1st time is a real evaluation to get the subquery value
             - 2nd time is to produce EXPLAIN output rows.
            1st execution sets certain members (e.g. select_result) to perform
            subquery execution rather than EXPLAIN line production. In order 
            to reset them back, we re-do all of the actions (yes it is ugly):
          */
	  join->init(thd, item_list, fake_select_lex->options, result);
          saved_error= mysql_select(thd, &fake_select_lex->ref_pointer_array,
                                &result_table_list,
                                0, item_list, NULL,
                                global_parameters->order_list.elements,
                                global_parameters->order_list.first,
                                NULL, NULL, NULL,
                                fake_select_lex->options | SELECT_NO_UNLOCK,
                                result, this, fake_select_lex);
        }
        else
        {
          join->examined_rows= 0;
          saved_error= join->reinit();
          join->exec();
        }
      }

      fake_select_lex->table_list.empty();
      if (!saved_error)
      {
	thd->limit_found_rows = (ulonglong)table->file->stats.records + add_rows;
        thd->examined_row_count+= examined_rows;
      }
      /*
	Mark for slow query log if any of the union parts didn't use
	indexes efficiently
      */
    }
  }
  thd->lex->current_select= lex_select_save;
err:
  thd->lex->set_limit_rows_examined();
  DBUG_RETURN(saved_error);
}


bool st_select_lex_unit::cleanup()
{
  int error= 0;
  DBUG_ENTER("st_select_lex_unit::cleanup");

  if (cleaned)
  {
    DBUG_RETURN(FALSE);
  }
  cleaned= 1;

  if (union_result)
  {
    delete union_result;
    union_result=0; // Safety
    if (table)
      free_tmp_table(thd, table);
    table= 0; // Safety
  }

  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    error|= sl->cleanup();

  if (fake_select_lex)
  {
    JOIN *join;
    if ((join= fake_select_lex->join))
    {
      join->tables_list= 0;
      join->table_count= 0;
      join->top_join_tab_count= 0;
      if (join->tmp_join && join->tmp_join != join)
      {
        join->tmp_join->tables_list= 0;
        join->tmp_join->table_count= 0;
        join->tmp_join->top_join_tab_count= 0;
      }
    }
    error|= fake_select_lex->cleanup();
    /*
      There are two cases when we should clean order items:
      1. UNION with SELECTs which all enclosed into braces
        in this case global_parameters == fake_select_lex
      2. UNION where last SELECT is not enclosed into braces
        in this case global_parameters == 'last select'
      So we should use global_parameters->order_list for
      proper order list clean up.
      Note: global_parameters and fake_select_lex are always
            initialized for UNION
    */
    DBUG_ASSERT(global_parameters);
    if (global_parameters->order_list.elements)
    {
      ORDER *ord;
      for (ord= global_parameters->order_list.first; ord; ord= ord->next)
        (*ord->item)->walk (&Item::cleanup_processor, 0, 0);
    }
  }

  DBUG_RETURN(error);
}


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


/*
  change select_result object of unit

  SYNOPSIS
    st_select_lex_unit::change_result()
    result	new select_result object
    old_result	old select_result object

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool st_select_lex_unit::change_result(select_result_interceptor *new_result,
                                       select_result_interceptor *old_result)
{
  bool res= FALSE;
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl->join && sl->join->result == old_result)
      if (sl->join->change_result(new_result))
	return TRUE;
  }
  if (fake_select_lex && fake_select_lex->join)
    res= fake_select_lex->join->change_result(new_result);
  return (res);
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
  SELECT_LEX *sl= first_select();
  bool is_procedure= test(sl->join->procedure);

  if (is_procedure)
  {
    /* Types for "SELECT * FROM t1 procedure analyse()"
       are generated during execute */
    return &sl->join->procedure_fields_list;
  }


  if (is_union())
  {
    DBUG_ASSERT(prepared);
    /* Types are generated during prepare */
    return &types;
  }

  return &sl->item_list;
}

bool st_select_lex::cleanup()
{
  bool error= FALSE;
  DBUG_ENTER("st_select_lex::cleanup()");

  if (join)
  {
    DBUG_ASSERT((st_select_lex*)join->select_lex == this);
    error= join->destroy();
    delete join;
    join= 0;
  }
  for (SELECT_LEX_UNIT *lex_unit= first_inner_unit(); lex_unit ;
       lex_unit= lex_unit->next_unit())
  {
    error= (bool) ((uint) error | (uint) lex_unit->cleanup());
  }
  inner_refs_list.empty();
  exclude_from_table_unique_test= FALSE;
  DBUG_RETURN(error);
}


void st_select_lex::cleanup_all_joins(bool full)
{
  SELECT_LEX_UNIT *unit;
  SELECT_LEX *sl;
  DBUG_ENTER("st_select_lex::cleanup_all_joins");

  if (join)
    join->cleanup(full);

  for (unit= first_inner_unit(); unit; unit= unit->next_unit())
    for (sl= unit->first_select(); sl; sl= sl->next_select())
      sl->cleanup_all_joins(full);
  DBUG_VOID_RETURN;
}


/**
  Set exclude_from_table_unique_test for selects of this unit and all
  underlying selects.

  @note used to exclude materialized derived tables (views) from unique
  table check.
*/

void st_select_lex_unit::set_unique_exclude()
{
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    sl->exclude_from_table_unique_test= TRUE;
    for (SELECT_LEX_UNIT *unit= sl->first_inner_unit();
         unit;
         unit= unit->next_unit())
    {
      unit->set_unique_exclude();
    }
  }
}


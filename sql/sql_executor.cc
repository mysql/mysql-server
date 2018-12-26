/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Query execution


  @defgroup Query_Executor  Query Executor
  @{
*/

#include "sql_executor.h"

#include "debug_sync.h"       // DEBUG_SYNC
#include "item_sum.h"         // Item_sum
#include "key.h"              // key_cmp
#include "log.h"              // sql_print_error
#include "opt_trace.h"        // Opt_trace_object
#include "sql_base.h"         // fill_record
#include "sql_join_buffer.h"  // st_cache_field
#include "sql_optimizer.h"    // JOIN
#include "sql_show.h"         // get_schema_tables_result
#include "sql_tmp_table.h"    // create_tmp_table
#include "json_dom.h"    // Json_wrapper

#include <algorithm>
using std::max;
using std::min;

static void return_zero_rows(JOIN *join, List<Item> &fields);
static void save_const_null_info(JOIN *join, table_map *save_nullinfo);
static void restore_const_null_info(JOIN *join, table_map save_nullinfo);
static int do_select(JOIN *join);

static enum_nested_loop_state
evaluate_join_record(JOIN *join, QEP_TAB *qep_tab);
static enum_nested_loop_state
evaluate_null_complemented_join_record(JOIN *join, QEP_TAB *qep_tab);
static enum_nested_loop_state
end_send(JOIN *join, QEP_TAB *qep_tab, bool end_of_records);
static enum_nested_loop_state
end_write(JOIN *join, QEP_TAB *qep_tab, bool end_of_records);
static enum_nested_loop_state
end_update(JOIN *join, QEP_TAB *qep_tab, bool end_of_records);
static void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr);

static int read_system(TABLE *table);
static int join_read_const(QEP_TAB *tab);
static int read_const(TABLE *table, TABLE_REF *ref);
static int join_read_key(QEP_TAB *tab);
static int join_read_always_key(QEP_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_read_next_same(READ_RECORD *info);
static int join_read_prev(READ_RECORD *info);
static int join_ft_read_first(QEP_TAB *tab);
static int join_ft_read_next(READ_RECORD *info);
static int join_read_always_key_or_null(QEP_TAB *tab);
static int join_read_next_same_or_null(READ_RECORD *info);
static int create_sort_index(THD *thd, JOIN *join, QEP_TAB *tab);
static bool remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
                                    ulong offset,Item *having);
static bool remove_dup_with_hash_index(THD *thd,TABLE *table,
                                       uint field_count, Field **first_field,
                                       ulong key_length,Item *having);
static int join_read_linked_first(QEP_TAB *tab);
static int join_read_linked_next(READ_RECORD *info);
static int do_sj_reset(SJ_TMP_TABLE *sj_tbl);
static bool cmp_buffer_with_ref(THD *thd, TABLE *table, TABLE_REF *tab_ref);

/**
  Execute select, executor entry point.

  @todo
    When can we have here thd->net.report_error not zero?

  @note that EXPLAIN may come here (single-row derived table, uncorrelated
    scalar subquery in WHERE clause...).
*/

void
JOIN::exec()
{
  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "join_execution");
  trace_exec.add_select_number(select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  List<Item> *columns_list= &fields_list;
  DBUG_ENTER("JOIN::exec");

  DBUG_ASSERT(select_lex == thd->lex->current_select());

  /*
    Check that we either
    - have no tables, or
    - have tables and have locked them, or
    - called for fake_select_lex, which may have temporary tables which do
      not need locking up front.
  */
  DBUG_ASSERT(!tables || thd->lex->is_query_tables_locked() ||
              select_lex == unit->fake_select_lex);

  THD_STAGE_INFO(thd, stage_executing);
  DEBUG_SYNC(thd, "before_join_exec");

  set_executed();

  if (prepare_result())
    DBUG_VOID_RETURN;

  Query_result *const query_result= select_lex->query_result();

  do_send_rows = unit->select_limit_cnt > 0;

  if (!tables_list && (tables || !select_lex->with_sum_func))
  {                                           // Only test of functions
    /*
      We have to test for 'conds' here as the WHERE may not be constant
      even if we don't have any tables for prepared statements or if
      conds uses something like 'rand()'.

      Don't evaluate the having clause here. return_zero_rows() should
      be called only for cases where there are no matching rows after
      evaluating all conditions except the HAVING clause.
    */
    if (select_lex->cond_value != Item::COND_FALSE &&
        (!where_cond || where_cond->val_int()))
    {
      if (query_result->send_result_set_metadata(*columns_list,
                                                 Protocol::SEND_NUM_ROWS |
                                                 Protocol::SEND_EOF))
        DBUG_VOID_RETURN;

      /*
        If the HAVING clause is either impossible or always true, then
        JOIN::having is set to NULL by optimize_cond.
        In this case JOIN::exec must check for JOIN::having_value, in the
        same way it checks for JOIN::cond_value.
      */
      if (((select_lex->having_value != Item::COND_FALSE) &&
           (!having_cond || having_cond->val_int()))
          && do_send_rows && query_result->send_data(fields_list))
        error= 1;
      else
      {
        error= (int) query_result->send_eof();
        send_records= calc_found_rows ? 1 : thd->get_sent_row_count();
      }
      /* Query block (without union) always returns 0 or 1 row */
      thd->current_found_rows= send_records;
    }
    else
    {
      return_zero_rows(this, *columns_list);
    }
    DBUG_VOID_RETURN;
  }

  if (zero_result_cause)
  {
    return_zero_rows(this, *columns_list);
    DBUG_VOID_RETURN;
  }
  
  /*
    Initialize examined rows here because the values from all join parts
    must be accumulated in examined_row_count. Hence every join
    iteration must count from zero.
  */
  examined_rows= 0;

  /* XXX: When can we have here thd->is_error() not zero? */
  if (thd->is_error())
  {
    error= thd->is_error();
    DBUG_VOID_RETURN;
  }

  THD_STAGE_INFO(thd, stage_sending_data);
  DBUG_PRINT("info", ("%s", thd->proc_info));
  query_result->send_result_set_metadata(*fields,
                                   Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
  error= do_select(this);
  /* Accumulate the counts from all join iterations of all join parts. */
  thd->inc_examined_row_count(examined_rows);
  DBUG_PRINT("counts", ("thd->examined_row_count: %lu",
                        (ulong) thd->get_examined_row_count()));

  DBUG_VOID_RETURN;
}


bool
JOIN::create_intermediate_table(QEP_TAB *const tab,
                                List<Item> *tmp_table_fields,
                                ORDER_with_src &tmp_table_group,
                                bool save_sum_fields)
{
  DBUG_ENTER("JOIN::create_intermediate_table");
  THD_STAGE_INFO(thd, stage_creating_tmp_table);

  /*
    Pushing LIMIT to the temporary table creation is not applicable
    when there is ORDER BY or GROUP BY or there is no GROUP BY, but
    there are aggregate functions, because in all these cases we need
    all result rows.
  */
  ha_rows tmp_rows_limit= ((order == NULL || skip_sort_order) &&
                           !tmp_table_group &&
                           !select_lex->with_sum_func) ?
    m_select_limit : HA_POS_ERROR;

  tab->tmp_table_param= new (thd->mem_root) Temp_table_param(tmp_table_param);
  tab->tmp_table_param->skip_create_table= true;
  TABLE* table= create_tmp_table(thd, tab->tmp_table_param, *tmp_table_fields,
                               tmp_table_group, select_distinct && !group_list,
                               save_sum_fields, select_lex->active_options(),
                               tmp_rows_limit, "");
  if (!table)
    DBUG_RETURN(true);
  tmp_table_param.using_outer_summary_function=
    tab->tmp_table_param->using_outer_summary_function;

  DBUG_ASSERT(tab->idx() > 0);
  tab[-1].next_select= sub_select_op;
  if (!(tab->op= new (thd->mem_root) QEP_tmp_table(tab)))
    goto err;

  tab->set_table(table);

  if (table->group)
  {
    explain_flags.set(tmp_table_group.src, ESP_USING_TMPTABLE);
  }
  if (table->distinct || select_distinct)
  {
    explain_flags.set(ESC_DISTINCT, ESP_USING_TMPTABLE);
  }
  if ((!group_list && !order && !select_distinct) ||
      (select_lex->active_options() &
       (SELECT_BIG_RESULT | OPTION_BUFFER_RESULT)))
  {
    explain_flags.set(ESC_BUFFER_RESULT, ESP_USING_TMPTABLE);
  }
  /* if group or order on first table, sort first */
  if (group_list && simple_group)
  {
    DBUG_PRINT("info",("Sorting for group"));
    THD_STAGE_INFO(thd, stage_sorting_for_group);

    if (ordered_index_usage != ordered_index_group_by &&
        qep_tab[const_tables].type() != JT_CONST && // Don't sort 1 row
        add_sorting_to_table(const_tables, &group_list))
      goto err;

    if (alloc_group_fields(this, group_list))
      goto err;
    if (make_sum_func_list(all_fields, fields_list, true))
      goto err;
    const bool need_distinct=
      !(tab->quick() && tab->quick()->is_agg_loose_index_scan());
    if (prepare_sum_aggregators(sum_funcs, need_distinct))
      goto err;
    if (setup_sum_funcs(thd, sum_funcs))
      goto err;
    group_list= NULL;
  }
  else
  {
    if (make_sum_func_list(all_fields, fields_list, false))
      goto err;
    const bool need_distinct=
      !(tab->quick() && tab->quick()->is_agg_loose_index_scan());
    if (prepare_sum_aggregators(sum_funcs, need_distinct))
      goto err;
    if (setup_sum_funcs(thd, sum_funcs))
      goto err;

    if (!group_list && !table->distinct && order && simple_order)
    {
      DBUG_PRINT("info",("Sorting for order"));
      THD_STAGE_INFO(thd, stage_sorting_for_order);

      if (ordered_index_usage != ordered_index_order_by &&
          add_sorting_to_table(const_tables, &order))
        goto err;
      order= NULL;
    }
  }
  DBUG_RETURN(false);

err:
  if (table != NULL)
  {
    free_tmp_table(thd, table);
    tab->set_table(NULL);
  }
  DBUG_RETURN(true);
}


/**
  Send all rollup levels higher than the current one to the client.

  @b SAMPLE
    @code
      SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx		Level we are on:
                        - 0 = Total sum level
                        - 1 = First group changed  (a)
                        - 2 = Second group changed (a,b)

  @retval
    0   ok
  @retval
    1   If send_data_failed()
*/

int JOIN::rollup_send_data(uint idx)
{
  uint i;
  for (i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    copy_ref_ptr_array(ref_ptrs, rollup.ref_pointer_arrays[i]);
    if ((!having_cond || having_cond->val_int()))
    {
      if (send_records < unit->select_limit_cnt && do_send_rows &&
	  select_lex->query_result()->send_data(rollup.fields[i]))
	return 1;
      send_records++;
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_ptrs);
  return 0;
}


/**
  Write all rollup levels higher than the current one to a temp table.

  @b SAMPLE
    @code
      SELECT a, b, SUM(c) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx                 Level we are on:
                               - 0 = Total sum level
                               - 1 = First group changed  (a)
                               - 2 = Second group changed (a,b)
  @param table               reference to temp table

  @retval
    0   ok
  @retval
    1   if write_data_failed()
*/

int JOIN::rollup_write_data(uint idx, TABLE *table_arg)
{
  uint i;
  for (i= send_group_parts ; i-- > idx ; )
  {
    /* Get reference pointers to sum functions in place */
    copy_ref_ptr_array(ref_ptrs, rollup.ref_pointer_arrays[i]);
    if ((!having_cond || having_cond->val_int()))
    {
      int write_error;
      Item *item;
      List_iterator_fast<Item> it(rollup.fields[i]);
      while ((item= it++))
      {
        if (item->type() == Item::NULL_ITEM && item->is_result_field())
          item->save_in_result_field(1);
      }
      copy_sum_funcs(sum_funcs_end[i+1], sum_funcs_end[i]);
      if ((write_error= table_arg->file->ha_write_row(table_arg->record[0])))
      {
  if (create_ondisk_from_heap(thd, table_arg, 
                                    tmp_table_param.start_recinfo,
                                    &tmp_table_param.recinfo,
                                    write_error, FALSE, NULL))
	  return 1;		     
      }
    }
  }
  /* Restore ref_pointer_array */
  set_items_ref_array(current_ref_ptrs);
  return 0;
}


void
JOIN::optimize_distinct()
{
  for (int i= primary_tables - 1; i >= 0; --i)
  {
    QEP_TAB *last_tab= qep_tab + i;
    if (select_lex->select_list_tables & last_tab->table_ref->map())
      break;
    last_tab->not_used_in_distinct= true;
  }

  /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
  if (order && skip_sort_order)
  {
    /* Should already have been optimized away */
    DBUG_ASSERT(ordered_index_usage == ordered_index_order_by);
    if (ordered_index_usage == ordered_index_order_by)
    {
      order= NULL;
    }
  }
}

bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct)
{
  Item_sum *func;
  DBUG_ENTER("prepare_sum_aggregators");
  while ((func= *(func_ptr++)))
  {
    if (func->set_aggregator(need_distinct && func->has_with_distinct() ?
                             Aggregator::DISTINCT_AGGREGATOR :
                             Aggregator::SIMPLE_AGGREGATOR))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/******************************************************************************
  Code for calculating functions
******************************************************************************/


/**
  Call ::setup for all sum functions.

  @param thd           thread handler
  @param func_ptr      sum function list

  @retval
    FALSE  ok
  @retval
    TRUE   error
*/

bool setup_sum_funcs(THD *thd, Item_sum **func_ptr)
{
  Item_sum *func;
  DBUG_ENTER("setup_sum_funcs");
  while ((func= *(func_ptr++)))
  {
    if (func->aggregator_setup(thd))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


static void
init_tmptable_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->reset_field();
}


/** Update record 0 in tmp_table from record 1. */

static void
update_tmptable_sum_func(Item_sum **func_ptr,
			 TABLE *tmp_table MY_ATTRIBUTE((unused)))
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->update_field();
}


/** Copy result of sum functions to record in tmp_table. */

static void
copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ; func_ptr++)
    (*func_ptr)->save_in_result_field(1);
  return;
}


static bool
init_sum_functions(Item_sum **func_ptr, Item_sum **end_ptr)
{
  for (; func_ptr != end_ptr ;func_ptr++)
  {
    if ((*func_ptr)->reset_and_add())
      return 1;
  }
  /* If rollup, calculate the upper sum levels */
  for ( ; *func_ptr ; func_ptr++)
  {
    if ((*func_ptr)->aggregator_add())
      return 1;
  }
  return 0;
}


static bool
update_sum_func(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= *func_ptr) ; func_ptr++)
    if (func->aggregator_add())
      return 1;
  return 0;
}

/** 
  Copy result of functions to record in tmp_table. 

  Uses the thread pointer to check for errors in 
  some of the val_xxx() methods called by the 
  save_in_result_field() function.
  TODO: make the Item::val_xxx() return error code

  @param func_ptr  array of the function Items to copy to the tmp table
  @param thd       pointer to the current thread for error checking
  @retval
    FALSE if OK
  @retval
    TRUE on error  
*/

bool
copy_funcs(Func_ptr_array *func_ptr, const THD *thd)
{
  for (size_t ix= 0; ix < func_ptr->size(); ++ix)
  {
    Item *func= func_ptr->at(ix);
    func->save_in_result_field(1);
    /*
      Need to check the THD error state because Item::val_xxx() don't
      return error code, but can generate errors
      TODO: change it for a real status check when Item::val_xxx()
      are extended to return status code.
    */  
    if (thd->is_error())
      return TRUE;
  }
  return FALSE;
}

/*
  end_select-compatible function that writes the record into a sjm temptable
  
  SYNOPSIS
    end_sj_materialize()
      join            The join 
      join_tab        Last join table
      end_of_records  FALSE <=> This call is made to pass another record 
                                combination
                      TRUE  <=> EOF (no action)

  DESCRIPTION
    This function is used by semi-join materialization to capture suquery's
    resultset and write it into the temptable (that is, materialize it).

  NOTE
    This function is used only for semi-join materialization. Non-semijoin
    materialization uses different mechanism.

  RETURN 
    NESTED_LOOP_OK
    NESTED_LOOP_ERROR
*/

static enum_nested_loop_state 
end_sj_materialize(JOIN *join, QEP_TAB *qep_tab, bool end_of_records)
{
  int error;
  THD *thd= join->thd;
  Semijoin_mat_exec *sjm= qep_tab[-1].sj_mat_exec();
  DBUG_ENTER("end_sj_materialize");
  if (!end_of_records)
  {
    TABLE *table= sjm->table;

    List_iterator<Item> it(sjm->sj_nest->nested_join->sj_inner_exprs);
    Item *item;
    while ((item= it++))
    {
      if (item->is_null())
        DBUG_RETURN(NESTED_LOOP_OK);
    }
    fill_record(thd, table, table->visible_field_ptr(),
                sjm->sj_nest->nested_join->sj_inner_exprs,
                NULL, NULL);
    if (thd->is_error())
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if (!check_unique_constraint(table))
      DBUG_RETURN(NESTED_LOOP_OK);
    if ((error= table->file->ha_write_row(table->record[0])))
    {
      /* create_ondisk_from_heap will generate error if needed */
      if (!table->file->is_ignorable_error(error))
      {
        if (create_ondisk_from_heap(thd, table,
                                  sjm->table_param.start_recinfo, 
                                  &sjm->table_param.recinfo, error,
                                  TRUE, NULL))
          DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
        /* Initialize the index, since create_ondisk_from_heap does
           not replicate the earlier index initialization */
        if (table->hash_field)
          table->file->ha_index_init(0, false);
      }
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


/**
  Check appearance of new constant items in multiple equalities
  of a condition after reading a constant table.

    The function retrieves the cond condition and for each encountered
    multiple equality checks whether new constants have appeared after
    reading the constant (single row) table tab. If so it adjusts
    the multiple equality appropriately.

  @param thd        thread handler
  @param cond       condition whose multiple equalities are to be checked
  @param tab        constant table that has been read
*/

static bool update_const_equal_items(THD *thd, Item *cond, JOIN_TAB *tab)
{
  if (!(cond->used_tables() & tab->table_ref->map()))
    return false;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item> *cond_list= ((Item_cond*) cond)->argument_list(); 
    List_iterator_fast<Item> li(*cond_list);
    Item *item;
    while ((item= li++))
    {
      if (update_const_equal_items(thd, item, tab))
        return true;
    }
  }
  else if (cond->type() == Item::FUNC_ITEM && 
           ((Item_cond*) cond)->functype() == Item_func::MULT_EQUAL_FUNC)
  {
    Item_equal *item_equal= (Item_equal *) cond;
    bool contained_const= item_equal->get_const() != NULL;
    if (item_equal->update_const(thd))
      return true;
    if (!contained_const && item_equal->get_const())
    {
      /* Update keys for range analysis */
      Item_equal_iterator it(*item_equal);
      Item_field *item_field;
      while ((item_field= it++))
      {
        Field *field= item_field->field;
        JOIN_TAB *stat= field->table->reginfo.join_tab;
        key_map possible_keys= field->key_start;
        possible_keys.intersect(field->table->keys_in_use_for_query);
        stat[0].const_keys.merge(possible_keys);
        stat[0].keys().merge(possible_keys);

        /*
          For each field in the multiple equality (for which we know that it 
          is a constant) we have to find its corresponding key part, and set 
          that key part in const_key_parts.
        */  
        if (!possible_keys.is_clear_all())
        {
          TABLE *const table= field->table;
          for (Key_use *use= stat->keyuse();
               use && use->table_ref == item_field->table_ref;
               use++)
          {
            if (possible_keys.is_set(use->key) && 
                table->key_info[use->key].key_part[use->keypart].field == field)
              table->const_key_parts[use->key]|= use->keypart_map;
          }
        }
      }
    }
  }
  return false;
}

/**
  For some reason, e.g. due to an impossible WHERE clause, the tables cannot
  possibly contain any rows that will be in the result. This function
  is used to return with a result based on no matching rows (i.e., an
  empty result or one row with aggregates calculated without using
  rows in the case of implicit grouping) before the execution of
  nested loop join.

  This function may evaluate the HAVING clause and is only meant for
  result sets that are empty due to an impossible HAVING clause. Do
  not use it if HAVING has already been evaluated.

  @param join    The join that does not produce a row
  @param fields  Fields in result
*/
static void
return_zero_rows(JOIN *join, List<Item> &fields)
{
  DBUG_ENTER("return_zero_rows");

  join->join_free();

  /* Update results for FOUND_ROWS */
  if (!join->send_row_on_empty_set())
  {
    join->thd->current_found_rows= 0;
  }

  SELECT_LEX *const select= join->select_lex;

  if (!(select->query_result()->send_result_set_metadata(fields,
                                               Protocol::SEND_NUM_ROWS | 
                                               Protocol::SEND_EOF)))
  {
    bool send_error= FALSE;
    if (join->send_row_on_empty_set())
    {
      // Mark tables as containing only NULL values
      for (TABLE_LIST *table= select->leaf_tables; table;
           table= table->next_leaf)
        table->table->set_null_row();

      // Calculate aggregate functions for no rows

      /*
        Must notify all fields that there are no rows (not only those
        that will be returned) because join->having may refer to
        fields that are not part of the result columns.
       */
      List_iterator_fast<Item> it(join->all_fields);
      Item *item;
      while ((item= it++))
        item->no_rows_in_result();

      if (!join->having_cond || join->having_cond->val_int())
        send_error= select->query_result()->send_data(fields);
    }
    if (!send_error)
      select->query_result()->send_eof();                 // Should be safe
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Setup write_func of QEP_tmp_table object

  @param join_tab JOIN_TAB of a tmp table

  @details
  Function sets up write_func according to how QEP_tmp_table object that
  is attached to the given join_tab will be used in the query.
*/

void setup_tmptable_write_func(QEP_TAB *tab)
{
  JOIN *join= tab->join();
  TABLE *table= tab->table();
  QEP_tmp_table *op= (QEP_tmp_table *)tab->op;
  Temp_table_param *const tmp_tbl= tab->tmp_table_param;

  DBUG_ASSERT(table && op);

  if (table->group && tmp_tbl->sum_func_count && 
      !tmp_tbl->precomputed_group_by)
  {
    /*
      Note for MyISAM tmp tables: if uniques is true keys won't be
      created.
    */
    if (table->s->keys)
    {
      DBUG_PRINT("info",("Using end_update"));
      op->set_write_func(end_update);
    }
  }
  else if (join->sort_and_group && !tmp_tbl->precomputed_group_by)
  {
    DBUG_PRINT("info",("Using end_write_group"));
    op->set_write_func(end_write_group);
  }
  else
  {
    DBUG_PRINT("info",("Using end_write"));
    op->set_write_func(end_write);
    if (tmp_tbl->precomputed_group_by)
    {
      Item_sum **func_ptr= join->sum_funcs;
      Item_sum *func;
      while ((func= *(func_ptr++)))
      {
        tmp_tbl->items_to_copy->push_back(func);
      }
    }
  }
}


/**
  @details
  Rows produced by a join sweep may end up in a temporary table or be sent
  to a client. Setup the function of the nested loop join algorithm which
  handles final fully constructed and matched records.

  @return
    end_select function to use. This function can't fail.
*/
Next_select_func JOIN::get_end_select_func()
{
  /* 
     Choose method for presenting result to user. Use end_send_group
     if the query requires grouping (has a GROUP BY clause and/or one or
     more aggregate functions). Use end_send if the query should not
     be grouped.
   */
  if (sort_and_group && !tmp_table_param.precomputed_group_by)
  {
    DBUG_PRINT("info",("Using end_send_group"));
    return end_send_group;
  }
  DBUG_PRINT("info",("Using end_send"));
  return end_send;
}


/**
  Make a join of all tables and write it on socket or to table.

  @retval
    0  if ok
  @retval
    1  if error is sent
  @retval
    -1  if error should be sent
*/

static int
do_select(JOIN *join)
{
  int rc= 0;
  enum_nested_loop_state error= NESTED_LOOP_OK;
  DBUG_ENTER("do_select");

  join->send_records=0;
  if (join->plan_is_const() && !join->need_tmp)
  {
    Next_select_func end_select= join->get_end_select_func();
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)

      @todo: consider calling end_select instead of duplicating code
    */
    if (!join->where_cond || join->where_cond->val_int())
    {
      // HAVING will be checked by end_select
      error= (*end_select)(join, 0, 0);
      if (error >= NESTED_LOOP_OK)
	error= (*end_select)(join, 0, 1);

      /*
        If we don't go through evaluate_join_record(), do the counting
        here.  join->send_records is increased on success in end_send(),
        so we don't touch it here.
      */
      join->examined_rows++;
      DBUG_ASSERT(join->examined_rows <= 1);
    }
    else if (join->send_row_on_empty_set())
    {
      table_map save_nullinfo= 0;
      /*
        If this is a subquery, we need to save and later restore
        the const table NULL info before clearing the tables
        because the following executions of the subquery do not
        reevaluate constant fields. @see save_const_null_info
        and restore_const_null_info
      */
      if (join->select_lex->master_unit()->item && join->const_tables)
        save_const_null_info(join, &save_nullinfo);

      // Calculate aggregate functions for no rows
      List_iterator_fast<Item> it(*join->fields);
      Item *item;
      while ((item= it++))
        item->no_rows_in_result();

      // Mark tables as containing only NULL values
      if (join->clear())
        error= NESTED_LOOP_ERROR;
      else
      {
        if (!join->having_cond || join->having_cond->val_int())
          rc= join->select_lex->query_result()->send_data(*join->fields);

        if (save_nullinfo)
          restore_const_null_info(join, save_nullinfo);
      }
    }
    /*
      An error can happen when evaluating the conds 
      (the join condition and piece of where clause 
      relevant to this join table).
    */
    if (join->thd->is_error())
      error= NESTED_LOOP_ERROR;
  }
  else
  {
    QEP_TAB *qep_tab= join->qep_tab + join->const_tables;
    DBUG_ASSERT(join->primary_tables);
    error= join->first_select(join,qep_tab,0);
    if (error >= NESTED_LOOP_OK)
      error= join->first_select(join,qep_tab,1);
  }

  join->thd->current_found_rows= join->send_records;
  /*
    For "order by with limit", we cannot rely on send_records, but need
    to use the rowcount read originally into the join_tab applying the
    filesort. There cannot be any post-filtering conditions, nor any
    following join_tabs in this case, so this rowcount properly represents
    the correct number of qualifying rows.
  */
  if (join->qep_tab && join->order)
  {
    // Save # of found records prior to cleanup
    QEP_TAB *sort_tab;
    uint const_tables= join->const_tables;

    // Take record count from first non constant table or from last tmp table
    if (join->tmp_tables > 0)
      sort_tab= &join->qep_tab[join->primary_tables + join->tmp_tables - 1];
    else
    {
      DBUG_ASSERT(!join->plan_is_const());
      sort_tab= &join->qep_tab[const_tables];
    }
    if (sort_tab->filesort &&
        join->calc_found_rows &&
        sort_tab->filesort->sortorder &&
        sort_tab->filesort->limit != HA_POS_ERROR)
    {
      join->thd->current_found_rows= sort_tab->records();
    }
  }

  {
    /*
      The following will unlock all cursors if the command wasn't an
      update command
    */
    join->join_free();			// Unlock all cursors
  }
  if (error == NESTED_LOOP_OK)
  {
    /*
      Sic: this branch works even if rc != 0, e.g. when
      send_data above returns an error.
    */
    if (join->select_lex->query_result()->send_eof())
      rc= 1;                                  // Don't send error
    DBUG_PRINT("info",("%ld records output", (long) join->send_records));
  }
  else
    rc= -1;
#ifndef DBUG_OFF
  if (rc)
  {
    DBUG_PRINT("error",("Error: do_select() failed"));
  }
#endif
  rc= join->thd->is_error() ? -1 : rc;
  DBUG_RETURN(rc);
}


/**
  @brief Accumulate full or partial join result in operation and send
  operation's result further.

  @param join  pointer to the structure providing all context info for the query
  @param join_tab the JOIN_TAB object to which the operation is attached
  @param end_records  TRUE <=> all records were accumulated, send them further

  @details
  This function accumulates records, one by one, in QEP operation's buffer by
  calling op->put_record(). When there is no more records to save, in this
  case the end_of_records argument == true, function tells QEP operation to
  send records further by calling op->send_records().
  When all records are sent this function passes 'end_of_records' signal
  further by calling sub_select() with end_of_records argument set to
  true. After that op->end_send() is called to tell QEP operation that
  it could end internal buffer scan.

  @note
  This function is not expected to be called when dynamic range scan is
  used to scan join_tab because join cache is disabled for such scan
  and range scans aren't used for tmp tables.
  @see setup_join_buffering
  For caches the function implements the algorithmic schema for both
  Blocked Nested Loop Join and Batched Key Access Join. The difference can
  be seen only at the level of of the implementation of the put_record and
  send_records virtual methods for the cache object associated with the
  join_tab.

  @return
    return one of enum_nested_loop_state.
*/

enum_nested_loop_state
sub_select_op(JOIN *join, QEP_TAB *qep_tab, bool end_of_records)
{
  DBUG_ENTER("sub_select_op");

  if (join->thd->killed)
  {
    /* The user has aborted the execution of the query */
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);
  }

  enum_nested_loop_state rc;
  QEP_operation *op= qep_tab->op;

  /* This function cannot be called if qep_tab has no associated operation */
  DBUG_ASSERT(op != NULL);

  if (end_of_records)
  {
    rc= op->end_send();
    if (rc >= NESTED_LOOP_OK)
      rc= sub_select(join, qep_tab, end_of_records);
    DBUG_RETURN(rc);
  }
  if (qep_tab->prepare_scan())
    DBUG_RETURN(NESTED_LOOP_ERROR);

  /*
    setup_join_buffering() disables join buffering if QS_DYNAMIC_RANGE is
    enabled.
  */
  DBUG_ASSERT(!qep_tab->dynamic_range());

  rc= op->put_record();

  DBUG_RETURN(rc);
}


/**
  Retrieve records ends with a given beginning from the result of a join.

  SYNPOSIS
    sub_select()
    join      pointer to the structure providing all context info for the query
    join_tab  the first next table of the execution plan to be retrieved
    end_records  true when we need to perform final steps of retrival   

  DESCRIPTION
    For a given partial join record consisting of records from the tables 
    preceding the table join_tab in the execution plan, the function
    retrieves all matching full records from the result set and
    send them to the result set stream. 

  @note
    The function effectively implements the  final (n-k) nested loops
    of nested loops join algorithm, where k is the ordinal number of
    the join_tab table and n is the total number of tables in the join query.
    It performs nested loops joins with all conjunctive predicates from
    the where condition pushed as low to the tables as possible.
    E.g. for the query
    @code
      SELECT * FROM t1,t2,t3
      WHERE t1.a=t2.a AND t2.b=t3.b AND t1.a BETWEEN 5 AND 9
    @endcode
    the predicate (t1.a BETWEEN 5 AND 9) will be pushed to table t1,
    given the selected plan prescribes to nest retrievals of the
    joined tables in the following order: t1,t2,t3.
    A pushed down predicate are attached to the table which it pushed to,
    at the field join_tab->cond.
    When executing a nested loop of level k the function runs through
    the rows of 'join_tab' and for each row checks the pushed condition
    attached to the table.
    If it is false the function moves to the next row of the
    table. If the condition is true the function recursively executes (n-k-1)
    remaining embedded nested loops.
    The situation becomes more complicated if outer joins are involved in
    the execution plan. In this case the pushed down predicates can be
    checked only at certain conditions.
    Suppose for the query
    @code
      SELECT * FROM t1 LEFT JOIN (t2,t3) ON t3.a=t1.a
      WHERE t1>2 AND (t2.b>5 OR t2.b IS NULL)
    @endcode
    the optimizer has chosen a plan with the table order t1,t2,t3.
    The predicate P1=t1>2 will be pushed down to the table t1, while the
    predicate P2=(t2.b>5 OR t2.b IS NULL) will be attached to the table
    t2. But the second predicate can not be unconditionally tested right
    after a row from t2 has been read. This can be done only after the
    first row with t3.a=t1.a has been encountered.
    Thus, the second predicate P2 is supplied with a guarded value that are
    stored in the field 'found' of the first inner table for the outer join
    (table t2). When the first row with t3.a=t1.a for the  current row 
    of table t1  appears, the value becomes true. For now on the predicate
    is evaluated immediately after the row of table t2 has been read.
    When the first row with t3.a=t1.a has been encountered all
    conditions attached to the inner tables t2,t3 must be evaluated.
    Only when all of them are true the row is sent to the output stream.
    If not, the function returns to the lowest nest level that has a false
    attached condition.
    The predicates from on expressions are also pushed down. If in the 
    the above example the on expression were (t3.a=t1.a AND t2.a=t1.a),
    then t1.a=t2.a would be pushed down to table t2, and without any
    guard.
    If after the run through all rows of table t2, the first inner table
    for the outer join operation, it turns out that no matches are
    found for the current row of t1, then current row from table t1
    is complemented by nulls  for t2 and t3. Then the pushed down predicates
    are checked for the composed row almost in the same way as it had
    been done for the first row with a match. The only difference is
    the predicates from on expressions are not checked. 

  @par
  @b IMPLEMENTATION
  @par
    The function forms output rows for a current partial join of k
    tables tables recursively.
    For each partial join record ending with a certain row from
    join_tab it calls sub_select that builds all possible matching
    tails from the result set.
    To be able  check predicates conditionally items of the class
    Item_func_trig_cond are employed.
    An object of  this class is constructed from an item of class COND
    and a pointer to a guarding boolean variable.
    When the value of the guard variable is true the value of the object
    is the same as the value of the predicate, otherwise it's just returns
    true. 
    To carry out a return to a nested loop level of join table t the pointer 
    to t is remembered in the field 'return_tab' of the join structure.
    Consider the following query:
    @code
        SELECT * FROM t1,
                      LEFT JOIN
                      (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                      ON t4.a=t2.a
           WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    @endcode
    Suppose the chosen execution plan dictates the order t1,t2,t3,t4,t5
    and suppose for a given joined rows from tables t1,t2,t3 there are
    no rows in the result set yet.
    When first row from t5 that satisfies the on condition
    t5.a=t3.a is found, the pushed down predicate t4.b=2 OR t4.b IS NULL
    becomes 'activated', as well the predicate t4.a=t2.a. But
    the predicate (t2.b=5 OR t2.b IS NULL) can not be checked until
    t4.a=t2.a becomes true. 
    In order not to re-evaluate the predicates that were already evaluated
    as attached pushed down predicates, a pointer to the the first
    most inner unmatched table is maintained in join_tab->first_unmatched.
    Thus, when the first row from t5 with t5.a=t3.a is found
    this pointer for t5 is changed from t4 to t2.             

    @par
    @b STRUCTURE @b NOTES
    @par
    join_tab->first_unmatched points always backwards to the first inner
    table of the embedding nested join, if any.

  @param join      pointer to the structure providing all context info for
                   the query
  @param join_tab  the first next table of the execution plan to be retrieved
  @param end_records  true when we need to perform final steps of retrival   

  @return
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

enum_nested_loop_state
sub_select(JOIN *join, QEP_TAB *const qep_tab,bool end_of_records)
{
  DBUG_ENTER("sub_select");

  qep_tab->table()->reset_null_row();

  if (end_of_records)
  {
    enum_nested_loop_state nls=
      (*qep_tab->next_select)(join,qep_tab+1,end_of_records);
    DBUG_RETURN(nls);
  }
  READ_RECORD *info= &qep_tab->read_record;

  if (qep_tab->prepare_scan())
    DBUG_RETURN(NESTED_LOOP_ERROR);

  if (qep_tab->starts_weedout())
  {
    do_sj_reset(qep_tab->flush_weedout_table);
  }

  const plan_idx qep_tab_idx= qep_tab->idx();
  join->return_tab= qep_tab_idx;
  qep_tab->not_null_compl= true;
  qep_tab->found_match= false;

  if (qep_tab->last_inner() != NO_PLAN_IDX)
  {
    /* qep_tab is the first inner table for an outer join operation. */

    /* Set initial state of guard variables for this table.*/
    qep_tab->found= false;

    /* Set first_unmatched for the last inner table of this group */
    QEP_AT(qep_tab, last_inner()).first_unmatched= qep_tab_idx;
  }
  if (qep_tab->do_firstmatch() || qep_tab->do_loosescan())
  {
    /*
      qep_tab is the first table of a LooseScan range, or has a "jump"
      address in a FirstMatch range.
      Reset the matching for this round of execution.
    */
    QEP_AT(qep_tab, match_tab).found_match= false;
  }

  join->thd->get_stmt_da()->reset_current_row_for_condition();

  enum_nested_loop_state rc= NESTED_LOOP_OK;
  bool in_first_read= true;
  const bool pfs_batch_update= qep_tab->pfs_batch_update(join);
  if (pfs_batch_update)
    qep_tab->table()->file->start_psi_batch_mode();
  while (rc == NESTED_LOOP_OK && join->return_tab >= qep_tab_idx)
  {
    int error;
    if (in_first_read)
    {
      in_first_read= false;
      error= (*qep_tab->read_first_record)(qep_tab);
    }
    else
      error= info->read_record(info);

    DBUG_EXECUTE_IF("bug13822652_1", join->thd->killed= THD::KILL_QUERY;);

    if (error > 0 || (join->thd->is_error()))   // Fatal error
      rc= NESTED_LOOP_ERROR;
    else if (error < 0)
      break;
    else if (join->thd->killed)			// Aborted by user
    {
      join->thd->send_kill_message();
      rc= NESTED_LOOP_KILLED;
    }
    else
    {
      if (qep_tab->keep_current_rowid)
        qep_tab->table()->file->position(qep_tab->table()->record[0]);
      rc= evaluate_join_record(join, qep_tab);
    }
  }

  if (rc == NESTED_LOOP_OK &&
      qep_tab->last_inner() != NO_PLAN_IDX &&
      !qep_tab->found)
    rc= evaluate_null_complemented_join_record(join, qep_tab);

  if (pfs_batch_update)
    qep_tab->table()->file->end_psi_batch_mode();

  DBUG_RETURN(rc);
}


/**
  @brief Prepare table to be scanned.

  @details This function is the place to do any work on the table that
  needs to be done before table can be scanned. Currently it
  only materialized derived tables and semi-joined subqueries and binds
  buffer for current rowid.

  @returns false - Ok, true  - error
*/

bool QEP_TAB::prepare_scan()
{
  // Check whether materialization is required.
  if (!materialize_table || materialized)
    return false;

  // Materialize table prior to reading it
  if ((*materialize_table)(this))
    return true;

  materialized= true;

  // Bind to the rowid buffer managed by the TABLE object.
  if (copy_current_rowid)
    copy_current_rowid->bind_buffer(table()->file->ref);

  return false;
}


/**
  SemiJoinDuplicateElimination: Weed out duplicate row combinations

  SYNPOSIS
    do_sj_dups_weedout()
      thd    Thread handle
      sjtbl  Duplicate weedout table

  DESCRIPTION
    Try storing current record combination of outer tables (i.e. their
    rowids) in the temporary table. This records the fact that we've seen 
    this record combination and also tells us if we've seen it before.

  RETURN
    -1  Error
    1   The row combination is a duplicate (discard it)
    0   The row combination is not a duplicate (continue)
*/

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl) 
{
  int error;
  SJ_TMP_TABLE::TAB *tab= sjtbl->tabs;
  SJ_TMP_TABLE::TAB *tab_end= sjtbl->tabs_end;

  DBUG_ENTER("do_sj_dups_weedout");

  if (sjtbl->is_confluent)
  {
    if (sjtbl->have_confluent_row) 
      DBUG_RETURN(1);
    else
    {
      sjtbl->have_confluent_row= TRUE;
      DBUG_RETURN(0);
    }
  }

  uchar *ptr= sjtbl->tmp_table->visible_field_ptr()[0]->ptr;
  // Put the rowids tuple into table->record[0]:
  // 1. Store the length 
  if (((Field_varstring*)(sjtbl->tmp_table->visible_field_ptr()[0]))->
                          length_bytes == 1)
  {
    *ptr= (uchar)(sjtbl->rowid_len + sjtbl->null_bytes);
    ptr++;
  }
  else
  {
    int2store(ptr, sjtbl->rowid_len + sjtbl->null_bytes);
    ptr += 2;
  }

  // 2. Zero the null bytes 
  uchar *const nulls_ptr= ptr;
  if (sjtbl->null_bytes)
  {
    memset(ptr, 0, sjtbl->null_bytes);
    ptr += sjtbl->null_bytes; 
  }

  // 3. Put the rowids
  for (uint i=0; tab != tab_end; tab++, i++)
  {
    handler *h= tab->qep_tab->table()->file;
    if (tab->qep_tab->table()->is_nullable() &&
        tab->qep_tab->table()->has_null_row())
    {
      /* It's a NULL-complemented row */
      *(nulls_ptr + tab->null_byte) |= tab->null_bit;
      memset(ptr + tab->rowid_offset, 0, h->ref_length);
    }
    else
    {
      /* Copy the rowid value */
      memcpy(ptr + tab->rowid_offset, h->ref, h->ref_length);
    }
  }

  if (!check_unique_constraint(sjtbl->tmp_table))
      DBUG_RETURN(1);
  error= sjtbl->tmp_table->file->ha_write_row(sjtbl->tmp_table->record[0]);
  if (error)
  {
    /* If this is a duplicate error, return immediately */
    if (sjtbl->tmp_table->file->is_ignorable_error(error))
      DBUG_RETURN(1);
    /*
      Other error than duplicate error: Attempt to create a temporary table.
    */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, sjtbl->tmp_table,
                                sjtbl->start_recinfo, &sjtbl->recinfo,
                                error, TRUE, &is_duplicate))
      DBUG_RETURN(-1);
    DBUG_RETURN(is_duplicate ? 1 : 0);
  }
  DBUG_RETURN(0);
}


/**
  SemiJoinDuplicateElimination: Reset the temporary table
*/

static int do_sj_reset(SJ_TMP_TABLE *sj_tbl)
{
  DBUG_ENTER("do_sj_reset");
  if (sj_tbl->tmp_table)
  {
    int rc= sj_tbl->tmp_table->file->ha_delete_all_rows();
    DBUG_RETURN(rc);
  }
  sj_tbl->have_confluent_row= FALSE;
  DBUG_RETURN(0);
}

/**
  @brief Process one row of the nested loop join.

  This function will evaluate parts of WHERE/ON clauses that are
  applicable to the partial row on hand and in case of success
  submit this row to the next level of the nested loop.
  join_tab->return_tab may be modified to cause a return to a previous
  join_tab.

  @param  join     - The join object
  @param  join_tab - The most inner join_tab being processed

  @return Nested loop state
*/

static enum_nested_loop_state
evaluate_join_record(JOIN *join, QEP_TAB *const qep_tab)
{
  bool not_used_in_distinct= qep_tab->not_used_in_distinct;
  ha_rows found_records=join->found_records;
  Item *condition= qep_tab->condition();
  const plan_idx qep_tab_idx= qep_tab->idx();
  bool found= TRUE;
  DBUG_ENTER("evaluate_join_record");
  DBUG_PRINT("enter",
             ("join: %p join_tab index: %d table: %s cond: %p",
              join, static_cast<int>(qep_tab_idx),
              qep_tab->table()->alias, condition));

  if (condition)
  {
    found= MY_TEST(condition->val_int());

    if (join->thd->killed)
    {
      join->thd->send_kill_message();
      DBUG_RETURN(NESTED_LOOP_KILLED);
    }

    /* check for errors evaluating the condition */
    if (join->thd->is_error())
      DBUG_RETURN(NESTED_LOOP_ERROR);
  }
  if (found)
  {
    /*
      There is no condition on this join_tab or the attached pushed down
      condition is true => a match is found.
    */
    while (qep_tab->first_unmatched != NO_PLAN_IDX && found)
    {
      /*
        The while condition is always false if join_tab is not
        the last inner join table of an outer join operation.
      */
      QEP_TAB *first_unmatched= &QEP_AT(qep_tab, first_unmatched);
      /*
        Mark that a match for the current row of the outer table is found.
        This activates WHERE clause predicates attached the inner tables of
        the outer join.
      */
      first_unmatched->found= true;
      for (QEP_TAB *tab= first_unmatched; tab <= qep_tab; tab++)
      {
        /*
          Check all predicates that have just been activated.

          Actually all predicates non-guarded by first_unmatched->found
          will be re-evaluated again. It could be fixed, but, probably,
          it's not worth doing now.

          not_exists_optimize has been created from a
          condition containing 'is_null'. This 'is_null'
          predicate is still present on any 'tab' with
          'not_exists_optimize'. Furthermore, the usual rules
          for condition guards also applies for
          'not_exists_optimize' -> When 'is_null==false' we
          know all cond. guards are open and we can apply
          the 'not_exists_optimize'.
        */
        DBUG_ASSERT(!(tab->table()->reginfo.not_exists_optimize &&
                     !tab->condition()));

        if (tab->condition() && !tab->condition()->val_int())
        {
          /* The condition attached to table tab is false */

          if (tab->table()->reginfo.not_exists_optimize)
          {
            /*
              When not_exists_optimizer is set and a matching row is found, the
              outer row should be excluded from the result set: no need to
              explore this record, thus we don't call the next_select.
              And, no need to explore other following records of 'tab', so we
              set join->return_tab.
              As we set join_tab->found above, evaluate_join_record() at the
              upper level will not yield a NULL-complemented record.
              Note that the calculation below can set return_tab to -1
              i.e. PRE_FIRST_PLAN_IDX.
            */
            join->return_tab= qep_tab_idx - 1;
            DBUG_RETURN(NESTED_LOOP_OK);
          }

          if (tab == qep_tab)
            found= 0;
          else
          {
            /*
              Set a return point if rejected predicate is attached
              not to the last table of the current nest level.
            */
            join->return_tab= tab->idx();
            DBUG_RETURN(NESTED_LOOP_OK);
          }
        }
        /* check for errors evaluating the condition */
        if (join->thd->is_error())
          DBUG_RETURN(NESTED_LOOP_ERROR);
      }
      /*
        Check whether join_tab is not the last inner table
        for another embedding outer join.
      */
      plan_idx f_u= first_unmatched->first_upper();
      if (f_u != NO_PLAN_IDX && join->qep_tab[f_u].last_inner() != qep_tab_idx)
        f_u= NO_PLAN_IDX;
      qep_tab->first_unmatched= f_u;
    }

    plan_idx return_tab= join->return_tab;

    if (qep_tab->finishes_weedout() && found)
    {
      int res= do_sj_dups_weedout(join->thd, qep_tab->check_weed_out_table);
      if (res == -1)
        DBUG_RETURN(NESTED_LOOP_ERROR);
      else if (res == 1)
        found= FALSE;
    }
    else if (qep_tab->do_loosescan() &&
             QEP_AT(qep_tab, match_tab).found_match)
    { 
      /*
         Loosescan algorithm requires an access method that gives 'sorted'
         retrieval of keys, or an access method that provides only one
         row (which is inherently sorted).
         EQ_REF and LooseScan may happen if dependencies in subquery (e.g.,
         outer join) prevents table pull-out.
       */  
      DBUG_ASSERT(qep_tab->use_order() || qep_tab->type() == JT_EQ_REF);

      /* 
         Previous row combination for duplicate-generating range,
         generated a match.  Compare keys of this row and previous row
         to determine if this is a duplicate that should be skipped.
       */
      if (key_cmp(qep_tab->table()->key_info[qep_tab->index()].key_part,
                  qep_tab->loosescan_buf, qep_tab->loosescan_key_len))
        /* 
           Keys do not match.  
           Reset found_match for last table of duplicate-generating range, 
           to avoid comparing keys until a new match has been found.
        */
        QEP_AT(qep_tab, match_tab).found_match= false;
      else
        found= false;
    }

    /*
      It was not just a return to lower loop level when one
      of the newly activated predicates is evaluated as false
      (See above join->return_tab= tab).
    */
    join->examined_rows++;
    DBUG_PRINT("counts", ("evaluate_join_record join->examined_rows++: %lu",
                          (ulong) join->examined_rows));

    if (found)
    {
      enum enum_nested_loop_state rc;
      // A match is found for the current partial join prefix.
      qep_tab->found_match= true;

      rc= (*qep_tab->next_select)(join, qep_tab+1, 0);
      join->thd->get_stmt_da()->inc_current_row_for_condition();
      if (rc != NESTED_LOOP_OK)
        DBUG_RETURN(rc);

      /* check for errors evaluating the condition */
      if (join->thd->is_error())
        DBUG_RETURN(NESTED_LOOP_ERROR);

      if (qep_tab->do_loosescan() &&
          QEP_AT(qep_tab,match_tab).found_match)
      {
        /* 
           A match was found for a duplicate-generating range of a semijoin. 
           Copy key to be able to determine whether subsequent rows
           will give duplicates that should be skipped.
        */
        KEY *key= qep_tab->table()->key_info + qep_tab->index();
        key_copy(qep_tab->loosescan_buf, qep_tab->table()->record[0],
                 key, qep_tab->loosescan_key_len);
      }
      else if (qep_tab->do_firstmatch() &&
               QEP_AT(qep_tab, match_tab).found_match)
      {
        /* 
          We should return to join_tab->firstmatch_return after we have 
          enumerated all the suffixes for current prefix row combination
        */
        set_if_smaller(return_tab, qep_tab->firstmatch_return);
      }

      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        set_if_smaller(return_tab, qep_tab_idx - 1);

      set_if_smaller(join->return_tab, return_tab);
    }
    else
    {
      join->thd->get_stmt_da()->inc_current_row_for_condition();
      if (qep_tab->not_null_compl)
      {
        /* a NULL-complemented row is not in a table so cannot be locked */
        qep_tab->read_record.unlock_row(qep_tab);
      }
    }
  }
  else
  {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    join->examined_rows++;
    join->thd->get_stmt_da()->inc_current_row_for_condition();
    if (qep_tab->not_null_compl)
      qep_tab->read_record.unlock_row(qep_tab);
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


/**

  @details
    Construct a NULL complimented partial join record and feed it to the next
    level of the nested loop. This function is used in case we have
    an OUTER join and no matching record was found.
*/

static enum_nested_loop_state
evaluate_null_complemented_join_record(JOIN *join, QEP_TAB *qep_tab)
{
  /*
    The table join_tab is the first inner table of a outer join operation
    and no matches has been found for the current outer row.
  */
  QEP_TAB *first_inner_tab= qep_tab;
  QEP_TAB *last_inner_tab= &QEP_AT(qep_tab, last_inner());

  DBUG_ENTER("evaluate_null_complemented_join_record");

  for ( ; qep_tab <= last_inner_tab ; qep_tab++)
  {
    // Make sure that the rowid buffer is bound, duplicates weedout needs it
    if (qep_tab->copy_current_rowid &&
        !qep_tab->copy_current_rowid->buffer_is_bound())
      qep_tab->copy_current_rowid->bind_buffer(qep_tab->table()->file->ref);

    /* Change the the values of guard predicate variables. */
    qep_tab->found= true;
    qep_tab->not_null_compl= false;
    /* The outer row is complemented by nulls for each inner tables */
    restore_record(qep_tab->table(),s->default_values);  // Make empty record
    qep_tab->table()->set_null_row();       // For group by without error
    if (qep_tab->starts_weedout() && qep_tab > first_inner_tab)
    {
      // sub_select() has not performed a reset for this table.
      do_sj_reset(qep_tab->flush_weedout_table);
    }
    /* Check all attached conditions for inner table rows. */
    if (qep_tab->condition() && !qep_tab->condition()->val_int())
    {
      if (join->thd->killed)
      {
        join->thd->send_kill_message();
        DBUG_RETURN(NESTED_LOOP_KILLED);
      }

      /* check for errors */
      if (join->thd->is_error())
        DBUG_RETURN(NESTED_LOOP_ERROR);
      else
        DBUG_RETURN(NESTED_LOOP_OK);
    }
  }
  qep_tab= last_inner_tab;
  /*
    From the point of view of the rest of execution, this record matches
    (it has been built and satisfies conditions, no need to do more evaluation
    on it). See similar code in evaluate_join_record().
  */
  plan_idx f_u= QEP_AT(qep_tab, first_unmatched).first_upper();
  if (f_u != NO_PLAN_IDX &&
      join->qep_tab[f_u].last_inner() != qep_tab->idx())
    f_u= NO_PLAN_IDX;
  qep_tab->first_unmatched= f_u;
  /*
    The row complemented by nulls satisfies all conditions
    attached to inner tables.
    Finish evaluation of record and send it to be joined with
    remaining tables.
    Note that evaluate_join_record will re-evaluate the condition attached
    to the last inner table of the current outer join. This is not deemed to
    have a significant performance impact.
  */
  const enum_nested_loop_state rc= evaluate_join_record(join, qep_tab);

  for (QEP_TAB *tab= first_inner_tab; tab <= last_inner_tab; tab++)
    tab->table()->reset_null_row();

  DBUG_RETURN(rc);
}


/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/** Help function when we get some an error from the table handler. */

int report_handler_error(TABLE *table, int error)
{
  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND)
  {
    table->status= STATUS_GARBAGE;
    return -1;					// key not found; ok
  }
  /*
    Do not spam the error log with these temporary errors:
       LOCK_DEADLOCK LOCK_WAIT_TIMEOUT TABLE_DEF_CHANGED
    Also skip printing to error log if the current thread has been killed.
  */
  if (error != HA_ERR_LOCK_DEADLOCK &&
      error != HA_ERR_LOCK_WAIT_TIMEOUT &&
      error != HA_ERR_TABLE_DEF_CHANGED &&
      !table->in_use->killed)
    sql_print_error("Got error %d when reading table '%s'",
		    error, table->s->path.str);
  table->file->print_error(error,MYF(0));
  return 1;
}


int safe_index_read(QEP_TAB *tab)
{
  int error;
  TABLE *table= tab->table();
  if ((error=table->file->ha_index_read_map(table->record[0],
                                            tab->ref().key_buff,
                                            make_prev_keypart_map(tab->ref().key_parts),
                                            HA_READ_KEY_EXACT)))
    return report_handler_error(table, error);
  return 0;
}


/**
   Reads content of constant table
   @param tab  table
   @param pos  position of table in query plan
   @retval 0   ok, one row was found or one NULL-complemented row was created
   @retval -1  ok, no row was found and no NULL-complemented row was created
   @retval 1   error
*/

int
join_read_const_table(JOIN_TAB *tab, POSITION *pos)
{
  int error;
  DBUG_ENTER("join_read_const_table");
  TABLE *table=tab->table();
  table->const_table=1;
  table->reset_null_row();
  table->status= STATUS_GARBAGE | STATUS_NOT_FOUND;

  if (table->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE)
  {
    const enum_sql_command sql_command= tab->join()->thd->lex->sql_command;
    if (sql_command == SQLCOM_UPDATE_MULTI ||
        sql_command == SQLCOM_DELETE_MULTI)
    {
      /*
        In a multi-UPDATE, if we represent "depends on" with "->", we have:
        "what columns to read (read_set)" ->
        "whether table will be updated on-the-fly or with tmp table" ->
        "whether to-be-updated columns are used by access path"
        "access path to table (range, ref, scan...)" ->
        "query execution plan" ->
        "what tables are const" ->
        "reading const tables" ->
        "what columns to read (read_set)".
        To break this loop, we always read all columns of a constant table if
        it is going to be updated.
        Another case is in multi-UPDATE and multi-DELETE, when the table has a
        trigger: bits of columns needed by the trigger are turned on in
        result->initialize_tables(), which has not yet been called when we do
        the reading now, so we must read all columns.
      */
      bitmap_set_all(table->read_set);
      /* Virtual generated columns must be writable */
      for (Field **vfield_ptr= table->vfield; vfield_ptr && *vfield_ptr; vfield_ptr++)
        bitmap_set_bit(table->write_set, (*vfield_ptr)->field_index);
      table->file->column_bitmaps_signal();
    }
  }

  if (tab->type() == JT_SYSTEM)
    error= read_system(table);
  else
  {
    if (!table->key_read && table->covering_keys.is_set(tab->ref().key) &&
	!table->no_keyread &&
        (int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY)
    {
      table->set_keyread(TRUE);
      tab->set_index(tab->ref().key);
    }
    error= read_const(table, &tab->ref());
    table->set_keyread(FALSE);
  }

  if (error)
  {
    /* Mark for EXPLAIN that the row was not found */
    pos->filter_effect= 1.0;
    pos->rows_fetched= 0.0;
    pos->prefix_rowcount= 0.0;
    pos->ref_depend_map= 0;
    if (!tab->table_ref->outer_join || error > 0)
      DBUG_RETURN(error);
  }

  if (tab->join_cond() && !table->has_null_row())
  {
    // We cannot handle outer-joined tables with expensive join conditions here:
    DBUG_ASSERT(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0)
      table->set_null_row();
  }

  /* Check appearance of new constant items in Item_equal objects */
  JOIN *const join= tab->join();
  THD *const thd= join->thd;
  if (join->where_cond &&
      update_const_equal_items(thd, join->where_cond, tab))
    DBUG_RETURN(1);
  TABLE_LIST *tbl;
  for (tbl= join->select_lex->leaf_tables; tbl; tbl= tbl->next_leaf)
  {
    TABLE_LIST *embedded;
    TABLE_LIST *embedding= tbl;
    do
    {
      embedded= embedding;
      if (embedded->join_cond_optim() &&
          update_const_equal_items(thd, embedded->join_cond_optim(), tab))
        DBUG_RETURN(1);
      embedding= embedded->embedding;
    }
    while (embedding &&
           embedding->nested_join->join_list.head() == embedded);
  }

  DBUG_RETURN(0);
}


/**
  Read a constant table when there is at most one matching row, using a table
  scan.

  @param table			Table to read

  @retval  0  Row was found
  @retval  -1 Row was not found
  @retval  1  Got an error (other than row not found) during read
*/
static int read_system(TABLE *table)
{
  int error;
  if (table->status & STATUS_GARBAGE)		// If first read
  {
    if ((error=table->file->read_first_row(table->record[0],
					   table->s->primary_key)))
    {
      if (error != HA_ERR_END_OF_FILE)
	return report_handler_error(table, error);
      table->set_null_row();
      empty_record(table);			// Make empty record
      return -1;
    }
    store_record(table,record[1]);
  }
  else if (!table->status)			// Only happens with left join
    restore_record(table,record[1]);			// restore old record
  table->reset_null_row();
  return table->status ? -1 : 0;
}


/**
  Read a constant table when there is at most one matching row, using an
  index lookup.

  @param tab			Table to read

  @retval 0  Row was found
  @retval -1 Row was not found
  @retval 1  Got an error (other than row not found) during read
*/

static int
join_read_const(QEP_TAB *tab)
{
  return read_const(tab->table(), &tab->ref());
}

static int read_const(TABLE *table, TABLE_REF *ref)
{
  int error;
  DBUG_ENTER("read_const");

  if (table->status & STATUS_GARBAGE)		// If first read
  {
    table->status= 0;
    if (cp_buffer_from_ref(table->in_use, table, ref))
      error=HA_ERR_KEY_NOT_FOUND;
    else
    {
      error=table->file->ha_index_read_idx_map(table->record[0],ref->key,
                                               ref->key_buff,
                                               make_prev_keypart_map(ref->key_parts),
                                               HA_READ_KEY_EXACT);
    }
    if (error)
    {
      table->status= STATUS_NOT_FOUND;
      table->set_null_row();
      empty_record(table);
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        const int ret= report_handler_error(table, error);
        DBUG_RETURN(ret);
      }
      DBUG_RETURN(-1);
    }
    store_record(table,record[1]);
  }
  else if (!(table->status & ~STATUS_NULL_ROW))	// Only happens with left join
  {
    table->status=0;
    restore_record(table,record[1]);			// restore old record
  }
  table->reset_null_row();
  DBUG_RETURN(table->status ? -1 : 0);
}


/**
  Read row using unique key: eq_ref access method implementation

  @details
    This is the "read_first" function for the eq_ref access method.
    The difference from ref access function is that it has a one-element
    lookup cache (see cmp_buffer_with_ref)

  @param tab   JOIN_TAB of the accessed table

  @retval  0 - Ok
  @retval -1 - Row not found 
  @retval  1 - Error
*/

static int
join_read_key(QEP_TAB *tab)
{
  TABLE *const table= tab->table();
  TABLE_REF *table_ref= &tab->ref();
  int error;

  if (!table->file->inited)
  {
    /*
      Disable caching for inner table of outer join, since setting the NULL
      property on the table will overwrite NULL bits and hence destroy the
      current row for later use as a cached row.
    */
    if (tab->table_ref->is_inner_table_of_outer_join())
      table_ref->disable_cache= true;
    DBUG_ASSERT(!tab->use_order()); //Don't expect sort req. for single row.
    if ((error= table->file->ha_index_init(table_ref->key, tab->use_order())))
    {
      (void) report_handler_error(table, error);
      return 1;
    }
  }

  /*
    We needn't do "Late NULLs Filtering" because eq_ref is restricted to
    indices on NOT NULL columns (see create_ref_for_key()).
  */
  if (cmp_buffer_with_ref(tab->join()->thd, table, table_ref) ||
      (table->status & (STATUS_GARBAGE | STATUS_NULL_ROW)))
  {
    if (table_ref->key_err)
    {
      table->status=STATUS_NOT_FOUND;
      return -1;
    }
    /*
      Moving away from the current record. Unlock the row
      in the handler if it did not match the partial WHERE.
    */
    if (table_ref->has_record && table_ref->use_count == 0)
    {
      table->file->unlock_row();
      table_ref->has_record= FALSE;
    }
    error= table->file->ha_index_read_map(table->record[0],
                                          table_ref->key_buff,
                                          make_prev_keypart_map(table_ref->key_parts),
                                          HA_READ_KEY_EXACT);
    if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return report_handler_error(table, error);

    if (! error)
    {
      table_ref->has_record= TRUE;
      table_ref->use_count= 1;
    }
  }
  else if (table->status == 0)
  {
    DBUG_ASSERT(table_ref->has_record);
    table_ref->use_count++;
  }
  table->reset_null_row();
  return table->status ? -1 : 0;
}

/**
  Since join_read_key may buffer a record, do not unlock
  it if it was not used in this invocation of join_read_key().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  TABLE_REF buffer.
  @sa join_read_key()
*/

void
join_read_key_unlock_row(QEP_TAB *tab)
{
  DBUG_ASSERT(tab->ref().use_count);
  if (tab->ref().use_count)
    tab->ref().use_count--;
}

/**
  Read a table *assumed* to be included in execution of a pushed join.
  This is the counterpart of join_read_key() / join_read_always_key()
  for child tables in a pushed join.

  When the table access is performed as part of the pushed join,
  all 'linked' child colums are prefetched together with the parent row.
  The handler will then only format the row as required by MySQL and set
  'table->status' accordingly.

  However, there may be situations where the prepared pushed join was not
  executed as assumed. It is the responsibility of the handler to handle
  these situation by letting ::index_read_pushed() then effectively do a 
  plain old' index_read_map(..., HA_READ_KEY_EXACT);
  
  @param tab			Table to read

  @retval
    0	Row was found
  @retval
    -1   Row was not found
  @retval
    1   Got an error (other than row not found) during read
*/
static int
join_read_linked_first(QEP_TAB *tab)
{
  int error;
  TABLE *table= tab->table();
  DBUG_ENTER("join_read_linked_first");

  DBUG_ASSERT(!tab->use_order()); // Pushed child can't be sorted
  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref().key, tab->use_order())))
  {
    (void) report_handler_error(table, error);
    DBUG_RETURN(error);
  }

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  if (tab->ref().impossible_null_ref())
  {
    DBUG_PRINT("info", ("join_read_linked_first null_rejected"));
    DBUG_RETURN(-1);
  }

  if (cp_buffer_from_ref(tab->join()->thd, table, &tab->ref()))
  {
    table->status=STATUS_NOT_FOUND;
    DBUG_RETURN(-1);
  }

  // 'read' itself is a NOOP: 
  //  handler::index_read_pushed() only unpack the prefetched row and set 'status'
  error=table->file->index_read_pushed(table->record[0],
                                       tab->ref().key_buff,
                                       make_prev_keypart_map(tab->ref().key_parts));
  if (unlikely(error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE))
    DBUG_RETURN(report_handler_error(table, error));

  table->reset_null_row();
  int rc= table->status ? -1 : 0;
  DBUG_RETURN(rc);
}

static int
join_read_linked_next(READ_RECORD *info)
{
  TABLE *table= info->table;
  DBUG_ENTER("join_read_linked_next");

  int error=table->file->index_next_pushed(table->record[0]);
  if (error)
  {
    if (unlikely(error != HA_ERR_END_OF_FILE))
      DBUG_RETURN(report_handler_error(table, error));
    table->status= STATUS_GARBAGE;
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(error);
}

/*
  ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_always_key()
      tab  JOIN_TAB of the accessed table

  DESCRIPTION
    This is "read_fist" function for the "ref" access method.
   
    The functon must leave the index initialized when it returns.
    ref_or_null access implementation depends on that.

  RETURN
    0  - Ok
   -1  - Row not found 
    1  - Error
*/

static int
join_read_always_key(QEP_TAB *tab)
{
  int error;
  TABLE *table= tab->table();

  /* Initialize the index first */
  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref().key, tab->use_order())))
  {
    (void) report_handler_error(table, error);
    return 1;
  }

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  TABLE_REF *ref= &tab->ref();
  if (ref->impossible_null_ref())
  {
    DBUG_PRINT("info", ("join_read_always_key null_rejected"));
    return -1;
  }

  if (cp_buffer_from_ref(tab->join()->thd, table, ref))
    return -1;
  if ((error= table->file->ha_index_read_map(table->record[0],
                                             tab->ref().key_buff,
                                             make_prev_keypart_map(tab->ref().key_parts),
                                             HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return report_handler_error(table, error);
    return -1; /* purecov: inspected */
  }
  return 0;
}


/**
  This function is used when optimizing away ORDER BY in 
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/
  
int
join_read_last_key(QEP_TAB *tab)
{
  int error;
  TABLE *table= tab->table();

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref().key, tab->use_order())))
  {
    (void) report_handler_error(table, error);
    return 1;
  }
  if (cp_buffer_from_ref(tab->join()->thd, table, &tab->ref()))
    return -1;
  if ((error=table->file->ha_index_read_last_map(table->record[0],
                                                 tab->ref().key_buff,
                                                 make_prev_keypart_map(tab->ref().key_parts))))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      return report_handler_error(table, error);
    return -1; /* purecov: inspected */
  }
  return 0;
}


	/* ARGSUSED */
static int
join_no_more_records(READ_RECORD *info MY_ATTRIBUTE((unused)))
{
  return -1;
}


static int
join_read_next_same(READ_RECORD *info)
{
  int error;
  TABLE *table= info->table;
  QEP_TAB *tab=table->reginfo.qep_tab;

  if ((error= table->file->ha_index_next_same(table->record[0],
                                              tab->ref().key_buff,
                                              tab->ref().key_length)))
  {
    if (error != HA_ERR_END_OF_FILE)
      return report_handler_error(table, error);
    table->status= STATUS_GARBAGE;
    return -1;
  }
  return 0;
}


int
join_read_prev_same(READ_RECORD *info)
{
  int error;
  TABLE *table= info->table;
  QEP_TAB *tab=table->reginfo.qep_tab;

  /*
    Using ha_index_prev() for reading records from the table can cause
    performance issues if used in combination with ICP. The ICP code
    in the storage engine does not know when to stop reading from the
    index and a call to ha_index_prev() might cause the storage engine
    to read to the beginning of the index if no qualifying record is
    found.
  */
  DBUG_ASSERT(table->file->pushed_idx_cond == NULL);

  if ((error= table->file->ha_index_prev(table->record[0])))
    return report_handler_error(table, error);
  if (key_cmp_if_same(table, tab->ref().key_buff, tab->ref().key,
                      tab->ref().key_length))
  {
    table->status=STATUS_NOT_FOUND;
    error= -1;
  }
  return error;
}


int
join_init_quick_read_record(QEP_TAB *tab)
{
  /*
    This is for QS_DYNAMIC_RANGE, i.e., "Range checked for each
    record". The trace for the range analysis below this point will
    be printed with different ranges for every record to the left of
    this table in the join.
  */

  THD *const thd= tab->join()->thd;
#ifdef OPTIMIZER_TRACE
  Opt_trace_context * const trace= &thd->opt_trace;
  const bool disable_trace=
    tab->quick_traced_before &&
    !trace->feature_enabled(Opt_trace_context::DYNAMIC_RANGE);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);

  tab->quick_traced_before= true;

  Opt_trace_object wrapper(trace);
  Opt_trace_object trace_table(trace, "rows_estimation_per_outer_row");
  trace_table.add_utf8_table(tab->table_ref);
#endif

  /* 
    If this join tab was read through a QUICK for the last record
    combination from earlier tables, deleting that quick will close the
    index. Otherwise, we need to close the index before the next join
    iteration starts because the handler object might be reused by a different
    access strategy.
  */
  if (!tab->quick() &&
      (tab->table()->file->inited != handler::NONE))
      tab->table()->file->ha_index_or_rnd_end();

  key_map needed_reg_dummy;
  QUICK_SELECT_I *old_qck= tab->quick();
  QUICK_SELECT_I *qck;
  DEBUG_SYNC(thd, "quick_not_created");
  const int rc= test_quick_select(thd,
                                  tab->keys(),
                                  0,          // empty table map
                                  HA_POS_ERROR,
                                  false,      // don't force quick range
                                  ORDER::ORDER_NOT_RELEVANT, tab,
                                  tab->condition(), &needed_reg_dummy, &qck);
  DBUG_ASSERT(old_qck == NULL || old_qck != qck) ;
  tab->set_quick(qck);

  /*
    EXPLAIN CONNECTION is used to understand why a query is currently taking
    so much time. So it makes sense to show what the execution is doing now:
    is it a table scan or a range scan? A range scan on which index.
    So: below we want to change the type and quick visible in EXPLAIN, and for
    that, we need to take mutex and change type and quick_optim.
  */

  DEBUG_SYNC(thd, "quick_created_before_mutex");

  thd->lock_query_plan();
  tab->set_type(qck ? calc_join_type(qck->get_type()) : JT_ALL);
  tab->set_quick_optim();
  thd->unlock_query_plan();

  delete old_qck;
  DEBUG_SYNC(thd, "quick_droped_after_mutex");

  return (rc == -1) ?
    -1 :				/* No possible records */
    join_init_read_record(tab);
}


int read_first_record_seq(QEP_TAB *tab)
{
  if (tab->read_record.table->file->ha_rnd_init(1))
    return 1;
  return (*tab->read_record.read_record)(&tab->read_record);
}


/**
  @brief Prepare table for reading rows and read first record.
  @details
    Prior to reading the table following tasks are done, (in the order of
    execution):
      .) derived tables are materialized
      .) duplicates removed (tmp tables only)
      .) table is sorted with filesort (both non-tmp and tmp tables)
    After this have been done this function resets quick select, if it's
    present, sets up table reading functions, and reads first record.

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

int join_init_read_record(QEP_TAB *tab)
{
  int error;

  if (tab->distinct && tab->remove_duplicates())  // Remove duplicates.
    return 1;
  if (tab->filesort && tab->sort_table())     // Sort table.
    return 1;

  if (tab->quick() && (error= tab->quick()->reset()))
  {
    /* Ensures error status is propageted back to client */
    report_handler_error(tab->table(), error);
    return 1;
  }
  if (init_read_record(&tab->read_record, tab->join()->thd, NULL, tab,
                       1, 1, FALSE))
    return 1;

  return (*tab->read_record.read_record)(&tab->read_record);
}

/*
  This helper function materializes derived table/view and then calls
  read_first_record function to set up access to the materialized table.
*/

int join_materialize_derived(QEP_TAB *tab)
{
  THD *const thd= tab->table()->in_use;
  TABLE_LIST *const derived= tab->table_ref;

  DBUG_ASSERT(derived->uses_materialization() && !tab->materialized);

  if (derived->materializable_is_const()) // Has been materialized by optimizer
    return NESTED_LOOP_OK;

  bool res= derived->materialize_derived(thd);
  res|= derived->cleanup_derived();
  DEBUG_SYNC(thd, "after_materialize_derived");
  return res ? NESTED_LOOP_ERROR : NESTED_LOOP_OK;
}



/*
  Helper function for materialization of a semi-joined subquery.

  @param tab JOIN_TAB referencing a materialized semi-join table

  @return Nested loop state
*/

int
join_materialize_semijoin(QEP_TAB *tab)
{
  DBUG_ENTER("join_materialize_semijoin");

  Semijoin_mat_exec *const sjm= tab->sj_mat_exec();

  QEP_TAB *const first= tab->join()->qep_tab + sjm->inner_table_index;
  QEP_TAB *const last= first + (sjm->table_count - 1);
  /*
    Set up the end_sj_materialize function after the last inner table,
    so that generated rows are inserted into the materialized table.
  */
  last->next_select= end_sj_materialize;
  last->set_sj_mat_exec(sjm); // TODO: This violates comment for sj_mat_exec!
  if (tab->table()->hash_field)
    tab->table()->file->ha_index_init(0, 0);
  int rc;
  if ((rc= sub_select(tab->join(), first, false)) < 0)
    DBUG_RETURN(rc);
  if ((rc= sub_select(tab->join(), first, true)) < 0)
    DBUG_RETURN(rc);
  if (tab->table()->hash_field)
    tab->table()->file->ha_index_or_rnd_end();

  last->next_select= NULL;
  last->set_sj_mat_exec(NULL);

#if !defined(DBUG_OFF) || defined(HAVE_VALGRIND)
  // Fields of inner tables should not be read anymore:
  for (QEP_TAB *t= first; t <= last; t++)
  {
    TABLE *const inner_table= t->table();
    TRASH(inner_table->record[0], inner_table->s->reclength);
  }
#endif

  DBUG_RETURN(NESTED_LOOP_OK);
}


/**
  Check if access to this JOIN_TAB has to retrieve rows
  in sorted order as defined by the ordered index
  used to access this table.
*/
bool
QEP_TAB::use_order() const
{
  /*
    No need to require sorted access for single row reads
    being performed by const- or EQ_REF-accessed tables.
  */
  if (type() == JT_EQ_REF || type() == JT_CONST  || type() == JT_SYSTEM)
    return false;

  /*
    First non-const table requires sorted results 
    if ORDER or GROUP BY use ordered index. 
  */
  if ((uint)idx() == join()->const_tables &&
      join()->ordered_index_usage != JOIN::ordered_index_void)
    return true;

  /*
    LooseScan strategy for semijoin requires sorted
    results even if final result is not to be sorted.
  */
  if (position()->sj_strategy == SJ_OPT_LOOSE_SCAN)
    return true;

  /* Fall through: Results don't have to be sorted */
  return false;
}

/*
  Helper function for sorting table with filesort.
*/

bool
QEP_TAB::sort_table()
{
  DBUG_PRINT("info",("Sorting for index"));
  THD_STAGE_INFO(join()->thd, stage_creating_sort_index);
  DBUG_ASSERT(join()->ordered_index_usage != (filesort->order == join()->order ?
                                              JOIN::ordered_index_order_by :
                                              JOIN::ordered_index_group_by));
  const bool rc= create_sort_index(join()->thd, join(), this) != 0;
  /*
    Filesort has filtered rows already (see skip_record() in
    find_all_keys()): so we can simply scan the cache, so have to set
    quick=NULL.
    But if we do this, we still need to delete the quick, now or later. We
    cannot do it now: the dtor of quick_index_merge would do free_io_cache,
    but the cache has to remain, because scan will read from it.
    So we delay deletion: we just let the "quick" continue existing in
    "quick_optim"; double benefit:
    - EXPLAIN will show the "quick_optim"
    - it will be deleted late enough.
  */
  set_quick(NULL);
  set_condition(NULL);
  return rc;
}


int
join_read_first(QEP_TAB *tab)
{
  int error;
  TABLE *table=tab->table();
  if (table->covering_keys.is_set(tab->index()) && !table->no_keyread)
    table->set_keyread(TRUE);
  table->status=0;
  tab->read_record.table=table;
  tab->read_record.record=table->record[0];
  tab->read_record.read_record=join_read_next;

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->index(), tab->use_order())))
  {
    (void) report_handler_error(table, error);
    return 1;
  }
  if ((error= table->file->ha_index_first(tab->table()->record[0])))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      report_handler_error(table, error);
    return -1;
  }
  return 0;
}


static int
join_read_next(READ_RECORD *info)
{
  int error;
  if ((error= info->table->file->ha_index_next(info->record)))
    return report_handler_error(info->table, error);
  return 0;
}


int
join_read_last(QEP_TAB *tab)
{
  TABLE *table=tab->table();
  int error;
  if (table->covering_keys.is_set(tab->index()) && !table->no_keyread)
    table->set_keyread(TRUE);
  table->status=0;
  tab->read_record.read_record=join_read_prev;
  tab->read_record.table=table;
  tab->read_record.record=table->record[0];
  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->index(), tab->use_order())))
  {
    (void) report_handler_error(table, error);
    return 1;
  }
  if ((error= table->file->ha_index_last(table->record[0])))
    return report_handler_error(table, error);
  return 0;
}


static int
join_read_prev(READ_RECORD *info)
{
  int error;
  if ((error= info->table->file->ha_index_prev(info->record)))
    return report_handler_error(info->table, error);
  return 0;
}


static int
join_ft_read_first(QEP_TAB *tab)
{
  int error;
  TABLE *table= tab->table();

  if (!table->file->inited &&
      (error= table->file->ha_index_init(tab->ref().key, tab->use_order())))
  {
    (void) report_handler_error(table, error);
    return 1;
  }
  table->file->ft_init();

  if ((error= table->file->ft_read(table->record[0])))
    return report_handler_error(table, error);
  return 0;
}

static int
join_ft_read_next(READ_RECORD *info)
{
  int error;
  if ((error= info->table->file->ft_read(info->table->record[0])))
    return report_handler_error(info->table, error);
  return 0;
}


/**
  Reading of key with key reference and one part that may be NULL.
*/

static int
join_read_always_key_or_null(QEP_TAB *tab)
{
  int res;

  /* First read according to key which is NOT NULL */
  *tab->ref().null_ref_key= 0;			// Clear null byte
  if ((res= join_read_always_key(tab)) >= 0)
    return res;

  /* Then read key with null value */
  *tab->ref().null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);
}


static int
join_read_next_same_or_null(READ_RECORD *info)
{
  int error;
  if ((error= join_read_next_same(info)) >= 0)
    return error;
  QEP_TAB *tab= info->table->reginfo.qep_tab;

  /* Test if we have already done a read after null key */
  if (*tab->ref().null_ref_key)
    return -1;					// All keys read
  *tab->ref().null_ref_key= 1;			// Set null byte
  return safe_index_read(tab);			// then read null keys
}


/**
  Pick the appropriate access method functions

  Sets the functions for the selected table access method

  @param      join_tab             JOIN_TAB for this QEP_TAB

  @todo join_init_read_record/join_read_(last|first) set
  tab->read_record.read_record internally. Do the same in other first record
  reading functions.
*/

void QEP_TAB::pick_table_access_method(const JOIN_TAB *join_tab)
{
  ASSERT_BEST_REF_IN_JOIN_ORDER(join());
  DBUG_ASSERT(join_tab == join()->best_ref[idx()]);
  DBUG_ASSERT(table());
  DBUG_ASSERT(read_first_record == NULL);
  // Only some access methods support reversed access:
  DBUG_ASSERT(!join_tab->reversed_access || type() == JT_REF ||
              type() == JT_INDEX_SCAN);
  // Fall through to set default access functions:
  switch (type())
  {
  case JT_REF:
    if (join_tab->reversed_access)
    {
      read_first_record= join_read_last_key;
      read_record.read_record= join_read_prev_same;
    }
    else
    {
      read_first_record= join_read_always_key;
      read_record.read_record= join_read_next_same;
    }
    break;

  case JT_REF_OR_NULL:
    read_first_record= join_read_always_key_or_null;
    read_record.read_record= join_read_next_same_or_null;
    break;

  case JT_CONST:
    read_first_record= join_read_const;
    read_record.read_record= join_no_more_records;
    break;

  case JT_EQ_REF:
    read_first_record= join_read_key;
    read_record.read_record= join_no_more_records;
    read_record.unlock_row= join_read_key_unlock_row;
    break;

  case JT_FT:
    read_first_record= join_ft_read_first;
    read_record.read_record= join_ft_read_next;
    break;

  case JT_INDEX_SCAN:
    read_first_record= join_tab->reversed_access ?
      join_read_last : join_read_first;
    break;
  case JT_ALL:
  case JT_RANGE:
  case JT_INDEX_MERGE:
    read_first_record= (join_tab->use_quick == QS_DYNAMIC_RANGE) ?
      join_init_quick_read_record : join_init_read_record;
    break;
  default:
    DBUG_ASSERT(0);
    break;
  }
}


/**
  Install the appropriate 'linked' access method functions
  if this part of the join have been converted to pushed join.
*/

void QEP_TAB::set_pushed_table_access_method(void)
{
  DBUG_ENTER("set_pushed_table_access_method");
  DBUG_ASSERT(table());

  /**
    Setup modified access function for children of pushed joins.
  */
  const TABLE *pushed_root= table()->file->root_of_pushed_join();
  if (pushed_root && pushed_root != table())
  {
    /**
      Is child of a pushed join operation:
      Replace access functions with its linked counterpart.
      ... Which is effectively a NOOP as the row is already fetched
      together with the root of the linked operation.
     */
    DBUG_PRINT("info", ("Modifying table access method for '%s'",
                        table()->s->table_name.str));
    DBUG_ASSERT(type() != JT_REF_OR_NULL);
    read_first_record= join_read_linked_first;
    read_record.read_record= join_read_linked_next;
    // Use the default unlock_row function
    read_record.unlock_row = rr_unlock_row;
  }
  DBUG_VOID_RETURN;
}

/*****************************************************************************
  DESCRIPTION
    Functions that end one nested loop iteration. Different functions
    are used to support GROUP BY clause and to redirect records
    to a table (e.g. in case of SELECT into a temporary table) or to the
    network client.
    See the enum_nested_loop_state enumeration for the description of return
    values.
*****************************************************************************/

/* ARGSUSED */
static enum_nested_loop_state
end_send(JOIN *join, QEP_TAB *qep_tab, bool end_of_records)
{
  DBUG_ENTER("end_send");
  /*
    When all tables are const this function is called with jointab == NULL.
    This function shouldn't be called for the first join_tab as it needs
    to get fields from previous tab.

    Note that qep_tab may be one past the last of qep_tab! So don't read its
    pointed content. But you can read qep_tab[-1] then.
  */
  DBUG_ASSERT(qep_tab == NULL || qep_tab > join->qep_tab);
  //TODO pass fields via argument
  List<Item> *fields= qep_tab ? qep_tab[-1].fields : join->fields;

  if (!end_of_records)
  {
    int error;

    if (join->tables &&
        // In case filesort has been used and zeroed quick():
        (join->qep_tab[0].quick_optim() &&
         join->qep_tab[0].quick_optim()->is_loose_index_scan()))
    {
      // Copy non-aggregated fields when loose index scan is used.
      if (copy_fields(&join->tmp_table_param, join->thd))
        DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    }
    // Use JOIN's HAVING for the case of tableless SELECT.
    if (join->having_cond && join->having_cond->val_int() == 0)
      DBUG_RETURN(NESTED_LOOP_OK);               // Didn't match having
    error=0;
    if (join->do_send_rows)
      error= join->select_lex->query_result()->send_data(*fields);
    if (error)
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    ++join->send_records;
    if (join->send_records >= join->unit->select_limit_cnt &&
        !join->do_send_rows)
    {
      /*
        If we have used Priority Queue for optimizing order by with limit,
        then stop here, there are no more records to consume.
        When this optimization is used, end_send is called on the next
        join_tab.
      */
      if (join->order &&
          join->calc_found_rows &&
          qep_tab > join->qep_tab &&
          qep_tab[-1].filesort &&
          qep_tab[-1].filesort->using_pq)
      {
        DBUG_PRINT("info", ("filesort NESTED_LOOP_QUERY_LIMIT"));
        DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);
      }
    }
    if (join->send_records >= join->unit->select_limit_cnt &&
        join->do_send_rows)
    {
      if (join->calc_found_rows)
      {
        QEP_TAB *first= &join->qep_tab[0];
	if ((join->primary_tables == 1) &&
            !join->sort_and_group &&
            !join->send_group_parts &&
            !join->having_cond &&
            !first->condition() &&
            !(first->quick()) &&
	    (first->table()->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT) &&
            (first->ref().key < 0))
	{
	  /* Join over all rows in table;  Return number of found rows */
	  TABLE *table= first->table();

	  if (table->sort.has_filesort_result())
	  {
	    /* Using filesort */
	    join->send_records= table->sort.found_records;
	  }
	  else
	  {
	    table->file->info(HA_STATUS_VARIABLE);
	    join->send_records= table->file->stats.records;
	  }
	}
	else 
	{
	  join->do_send_rows= 0;
	  if (join->unit->fake_select_lex)
	    join->unit->fake_select_lex->select_limit= 0;
	  DBUG_RETURN(NESTED_LOOP_OK);
	}
      }
      DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);      // Abort nicely
    }
    else if (join->send_records >= join->fetch_limit)
    {
      /*
        There is a server side cursor and all rows for
        this fetch request are sent.
      */
      DBUG_RETURN(NESTED_LOOP_CURSOR_LIMIT);
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
enum_nested_loop_state
end_send_group(JOIN *join, QEP_TAB *qep_tab, bool end_of_records)
{
  int idx= -1;
  enum_nested_loop_state ok_code= NESTED_LOOP_OK;
  List<Item> *fields= qep_tab ? qep_tab[-1].fields : join->fields;
  DBUG_ENTER("end_send_group");


  if (!join->items3.is_null() && !join->set_group_rpa)
  {
    join->set_group_rpa= true;
    join->set_items_ref_array(join->items3);
  }

  if (!join->first_record || end_of_records ||
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    if (!join->group_sent &&
        (join->first_record ||
         (end_of_records && !join->grouped && !join->group_optimized_away)))
    {
      if (idx < (int) join->send_group_parts)
      {
	int error=0;
	{
          table_map save_nullinfo= 0;
          if (!join->first_record)
          {
            /*
              If this is a subquery, we need to save and later restore
              the const table NULL info before clearing the tables
              because the following executions of the subquery do not
              reevaluate constant fields. @see save_const_null_info
              and restore_const_null_info
            */
            if (join->select_lex->master_unit()->item && join->const_tables)
              save_const_null_info(join, &save_nullinfo);

            // Calculate aggregate functions for no rows
            List_iterator_fast<Item> it(*fields);
            Item *item;

            while ((item= it++))
              item->no_rows_in_result();

            // Mark tables as containing only NULL values
            if (join->clear())
              DBUG_RETURN(NESTED_LOOP_ERROR);        /* purecov: inspected */
	  }
	  if (join->having_cond && join->having_cond->val_int() == 0)
	    error= -1;				// Didn't satisfy having
	  else
	  {
	    if (join->do_send_rows)
	      error= join->select_lex->query_result()->send_data(*fields);
	    join->send_records++;
            join->group_sent= true;
	  }
	  if (join->rollup.state != ROLLUP::STATE_NONE && error <= 0)
	  {
	    if (join->rollup_send_data((uint) (idx+1)))
	      error= 1;
	  }
          if (save_nullinfo)
            restore_const_null_info(join, save_nullinfo);

	}
	if (error > 0)
          DBUG_RETURN(NESTED_LOOP_ERROR);        /* purecov: inspected */
	if (end_of_records)
	  DBUG_RETURN(NESTED_LOOP_OK);
	if (join->send_records >= join->unit->select_limit_cnt &&
	    join->do_send_rows)
	{
	  if (!join->calc_found_rows)
	    DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT); // Abort nicely
	  join->do_send_rows=0;
	  join->unit->select_limit_cnt = HA_POS_ERROR;
        }
        else if (join->send_records >= join->fetch_limit)
        {
          /*
            There is a server side cursor and all rows
            for this fetch request are sent.
          */
          /*
            Preventing code duplication. When finished with the group reset
            the group functions and copy_fields. We fall through. bug #11904
          */
          ok_code= NESTED_LOOP_CURSOR_LIMIT;
        }
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record=1;
      (void)(test_if_item_cache_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      /*
        This branch is executed also for cursors which have finished their
        fetch limit - the reason for ok_code.
      */
      if (copy_fields(&join->tmp_table_param, join->thd))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
	DBUG_RETURN(NESTED_LOOP_ERROR);
      join->group_sent= false;
      DBUG_RETURN(ok_code);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(NESTED_LOOP_ERROR);
  DBUG_RETURN(NESTED_LOOP_OK);
}

static bool cmp_field_value(Field *field, my_ptrdiff_t diff)
{
  DBUG_ASSERT(field);
  /*
    Records are different when:
    1) NULL flags aren't the same
    2) length isn't the same
    3) data isn't the same
  */
  const bool value1_isnull= field->is_real_null();
  const bool value2_isnull= field->is_real_null(diff);

  if (value1_isnull != value2_isnull)   // 1
    return true;
  if (value1_isnull)
    return false; // Both values are null, no need to proceed.

  const size_t value1_length= field->data_length();
  const size_t value2_length= field->data_length(diff);

  if (field->type() == MYSQL_TYPE_JSON)
  {
    Field_json *json_field= down_cast<Field_json *>(field);

    // Fetch the JSON value on the left side of the comparison.
    Json_wrapper left_wrapper;
    if (json_field->val_json(&left_wrapper))
      return true;                            /* purecov: inspected */

    // Fetch the JSON value on the right side of the comparison.
    Json_wrapper right_wrapper;
    json_field->ptr+= diff;
    bool err= json_field->val_json(&right_wrapper);
    json_field->ptr-= diff;
    if (err)
      return true;                            /* purecov: inspected */

    return (left_wrapper.compare(right_wrapper) != 0);
  }

  // Trailing space can't be skipped and length is different
  if (!field->is_text_key_type() && value1_length != value2_length)     // 2
    return true;

  if (field->cmp_max(field->ptr, field->ptr + diff,       // 3
                     std::max(value1_length, value2_length)))
    return true;

  return false;
}

/**
  Compare GROUP BY in from tmp table's record[0] and record[1]
  
  @returns
    true  records are different
    false records are the same
*/

bool group_rec_cmp(ORDER *group, uchar *rec0, uchar *rec1)
{
  my_ptrdiff_t diff= rec1 - rec0;

  for (ORDER *grp= group; grp; grp= grp->next)
  {
    Item *item= *(grp->item);
    Field *field= item->get_tmp_table_field();
    if (cmp_field_value(field, diff))
      return true;
  }
  return false;
}


/**
  Compare GROUP BY in from tmp table's record[0] and record[1]
  
  @returns
    true  records are different
    false records are the same
*/

bool table_rec_cmp(TABLE *table)
{
  my_ptrdiff_t diff= table->record[1] - table->record[0];
  Field **fields= table->visible_field_ptr();

  for (uint i= 0; i < table->visible_field_count() ; i++)
  {
    Field *field= fields[i];
    if (cmp_field_value(field, diff))
      return true;
  }
  return false;
}


/**
  Generate hash for a field

  @returns generated hash
*/

ulonglong unique_hash(Field *field, ulonglong *hash_val)
{
  uchar *pos, *end;
  ulong seed1=0, seed2= 4;
  ulonglong crc= *hash_val;

  if (field->is_null())
  {
    /*
      Change crc in a way different from an empty string or 0.
      (This is an optimisation;  The code will work even if
      this isn't done)
    */
    crc=((crc << 8) + 511+
         (crc >> (8*sizeof(ha_checksum)-8)));
    goto finish;
  }

  field->get_ptr(&pos);
  end= pos + field->data_length();

  if (field->type() == MYSQL_TYPE_JSON)
  {
    Field_json *json_field= down_cast<Field_json *>(field);

    crc= json_field->make_hash_key(hash_val);
  }
  else if (field->key_type() == HA_KEYTYPE_TEXT ||
      field->key_type() == HA_KEYTYPE_VARTEXT1 ||
      field->key_type() == HA_KEYTYPE_VARTEXT2)
  {
    field->charset()->coll->hash_sort(field->charset(), (const uchar*) pos,
                                      field->data_length(), &seed1, &seed2);
    crc^= seed1;
  }
  else
    while (pos != end)
      crc=((crc << 8) +
           (((uchar)  *(uchar*) pos++))) +
        (crc >> (8*sizeof(ha_checksum)-8));
finish:
  *hash_val= crc;
  return crc;
}


/* Generate hash for unique constraint according to group-by list */

ulonglong unique_hash_group(ORDER *group)
{
  ulonglong crc= 0;
  Field *field;

  for (ORDER *ord= group; ord ; ord= ord->next)
  {
    Item *item= *(ord->item);
    field= item->get_tmp_table_field();
    DBUG_ASSERT(field);
    unique_hash(field, &crc);
  }

  return crc;
}


/* Generate hash for unique_constraint for all visible fields of a table */

ulonglong unique_hash_fields(TABLE *table)
{
  ulonglong crc= 0;
  Field **fields= table->visible_field_ptr();

  for (uint i=0 ; i < table->visible_field_count() ; i++)
    unique_hash(fields[i], &crc);

  return crc;
}


/**
  Check unique_constraint.

  @details Calculates record's hash and checks whether the record given in
  table->record[0] is already present in the tmp table.

  @param tab JOIN_TAB of tmp table to check

  @notes This function assumes record[0] is already filled by the caller.
  Depending on presence of table->group, it's or full list of table's fields
  are used to calculate hash.

  @returns
    false same record was found
    true  record wasn't found
*/

bool check_unique_constraint(TABLE *table)
{
  ulonglong hash;

  if (!table->hash_field)
    return true;

  if (table->no_keyread)
    return true;

  if (table->group)
    hash= unique_hash_group(table->group);
  else
    hash= unique_hash_fields(table);
  table->hash_field->store(hash, true);
  int res= table->file->ha_index_read_map(table->record[1],
                                          table->hash_field->ptr,
                                          HA_WHOLE_KEY,
                                          HA_READ_KEY_EXACT);
  while (!res)
  {
    // Check whether records are the same.
    if (!(table->distinct ?
          table_rec_cmp(table) :
          group_rec_cmp(table->group, table->record[0], table->record[1])))
      return false; // skip it
    res= table->file->ha_index_next_same(table->record[1],
                                         table->hash_field->ptr,
                                         sizeof(hash));
  }
  return true;
}


  /* ARGSUSED */
static enum_nested_loop_state
end_write(JOIN *join, QEP_TAB *const qep_tab, bool end_of_records)
{
  TABLE *const table= qep_tab->table();
  DBUG_ENTER("end_write");

  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }
  if (!end_of_records)
  {
    Temp_table_param *const tmp_tbl= qep_tab->tmp_table_param;
    if (copy_fields(tmp_tbl, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */
    if (copy_funcs(tmp_tbl->items_to_copy, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */

    if (!qep_tab->having || qep_tab->having->val_int())
    {
      int error;
      join->found_records++;

      if (!check_unique_constraint(table))
        goto end; // skip it

      if ((error=table->file->ha_write_row(table->record[0])))
      {
        if (table->file->is_ignorable_error(error))
	  goto end;
	if (create_ondisk_from_heap(join->thd, table,
                                    tmp_tbl->start_recinfo,
                                    &tmp_tbl->recinfo,
				    error, TRUE, NULL))
	  DBUG_RETURN(NESTED_LOOP_ERROR);        // Not a table_is_full error
	table->s->uniques=0;			// To ensure rows are the same
      }
      if (++qep_tab->send_records >=
            tmp_tbl->end_write_records &&
	  join->do_send_rows)
      {
	if (!join->calc_found_rows)
	  DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);
	join->do_send_rows=0;
	join->unit->select_limit_cnt = HA_POS_ERROR;
	DBUG_RETURN(NESTED_LOOP_OK);
      }
    }
  }
end:
  DBUG_RETURN(NESTED_LOOP_OK);
}


/* ARGSUSED */
/** Group by searching after group record and updating it if possible. */

static enum_nested_loop_state
end_update(JOIN *join, QEP_TAB *const qep_tab, bool end_of_records)
{
  TABLE *const table= qep_tab->table();
  ORDER *group;
  int error;
  bool group_found= false;
  DBUG_ENTER("end_update");

  if (end_of_records)
    DBUG_RETURN(NESTED_LOOP_OK);
  if (join->thd->killed)			// Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }

  Temp_table_param *const tmp_tbl= qep_tab->tmp_table_param;
  join->found_records++;
  if (copy_fields(tmp_tbl, join->thd))	// Groups are copied twice.
    DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */
      
  /* Make a key of group index */
  if (table->hash_field)
  {
    /*
      We need to call to copy_funcs here in order to get correct value for
      hash_field. However, this call isn't needed so early when hash_field
      isn't used as it would cause unnecessary additional evaluation of
      functions to be copied when 2nd and further records in group are
      found.
    */
    if (copy_funcs(tmp_tbl->items_to_copy, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */
    if (!check_unique_constraint(table))
      group_found= true;
  }
  else
  {
    for (group=table->group ; group ; group=group->next)
    {
      Item *item= *group->item;
      item->save_org_in_field(group->field);
      /* Store in the used key if the field was 0 */
      if (item->maybe_null)
        group->buff[-1]= (char) group->field->is_null();
    }
    const uchar *key= tmp_tbl->group_buff;
    if (!table->file->ha_index_read_map(table->record[1],
                                        key,
                                        HA_WHOLE_KEY,
                                        HA_READ_KEY_EXACT))
      group_found= true;
  }
  if (group_found)
  {
    /* Update old record */
    restore_record(table, record[1]);
    update_tmptable_sum_func(join->sum_funcs, table);
    if ((error=table->file->ha_update_row(table->record[1],
                                          table->record[0])))
    {
      // Old and new records are the same, ok to ignore
      if (error == HA_ERR_RECORD_IS_THE_SAME)
        DBUG_RETURN(NESTED_LOOP_OK);
      table->file->print_error(error, MYF(0));   /* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);            /* purecov: inspected */
    }
    DBUG_RETURN(NESTED_LOOP_OK);
  }

  /*
    Copy null bits from group key to table
    We can't copy all data as the key may have different format
    as the row data (for example as with VARCHAR keys)
  */
  if (!table->hash_field)
  {
    KEY_PART_INFO *key_part;
    for (group= table->group, key_part= table->key_info[0].key_part;
         group;
         group= group->next, key_part++)
    {
      if (key_part->null_bit)
        memcpy(table->record[0] + key_part->offset, group->buff, 1);
    }
    /* See comment on copy_funcs above. */
    if (copy_funcs(tmp_tbl->items_to_copy, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR);           /* purecov: inspected */
  }
  init_tmptable_sum_functions(join->sum_funcs);
  if ((error=table->file->ha_write_row(table->record[0])))
  {
    if (create_ondisk_from_heap(join->thd, table,
                                tmp_tbl->start_recinfo,
                                &tmp_tbl->recinfo,
				error, FALSE, NULL))
      DBUG_RETURN(NESTED_LOOP_ERROR);            // Not a table_is_full error
    /* Change method to update rows */
    if ((error= table->file->ha_index_init(0, 0)))
    {
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(NESTED_LOOP_ERROR);
    }
  }
  qep_tab->send_records++;
  DBUG_RETURN(NESTED_LOOP_OK);
}


	/* ARGSUSED */
enum_nested_loop_state
end_write_group(JOIN *join, QEP_TAB *const qep_tab, bool end_of_records)
{
  TABLE *table= qep_tab->table();
  int	  idx= -1;
  DBUG_ENTER("end_write_group");

  if (join->thd->killed)
  {						// Aborted by user
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);             /* purecov: inspected */
  }
  if (!join->first_record || end_of_records ||
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    Temp_table_param *const tmp_tbl= qep_tab->tmp_table_param;
    if (join->first_record || (end_of_records && !join->grouped))
    {
      int send_group_parts= join->send_group_parts;
      if (idx < send_group_parts)
      {
        table_map save_nullinfo= 0;
        if (!join->first_record)
        {
          // Dead code or we need a test case for this branch
          DBUG_ASSERT(false);
          /*
            If this is a subquery, we need to save and later restore
            the const table NULL info before clearing the tables
            because the following executions of the subquery do not
            reevaluate constant fields. @see save_const_null_info
            and restore_const_null_info
          */
          if (join->select_lex->master_unit()->item && join->const_tables)
            save_const_null_info(join, &save_nullinfo);

          // Calculate aggregate functions for no rows
          List_iterator_fast<Item> it(*(qep_tab-1)->fields);
          Item *item;
          while ((item= it++))
            item->no_rows_in_result();

          // Mark tables as containing only NULL values
          if (join->clear())
            DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        copy_sum_funcs(join->sum_funcs,
                       join->sum_funcs_end[send_group_parts]);
	if (!qep_tab->having || qep_tab->having->val_int())
	{
          int error= table->file->ha_write_row(table->record[0]);
          if (error &&
              create_ondisk_from_heap(join->thd, table,
                                      tmp_tbl->start_recinfo,
                                      &tmp_tbl->recinfo,
                                      error, FALSE, NULL))
            DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        if (join->rollup.state != ROLLUP::STATE_NONE)
	{
	  if (join->rollup_write_data((uint) (idx+1), table))
	    DBUG_RETURN(NESTED_LOOP_ERROR);
	}
        if (save_nullinfo)
          restore_const_null_info(join, save_nullinfo);

	if (end_of_records)
	  DBUG_RETURN(NESTED_LOOP_OK);
      }
    }
    else
    {
      if (end_of_records)
	DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record=1;

      (void)(test_if_item_cache_changed(join->group_fields));
    }
    if (idx < (int) join->send_group_parts)
    {
      if (copy_fields(tmp_tbl, join->thd))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      if (copy_funcs(tmp_tbl->items_to_copy, join->thd))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx+1]))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      DBUG_RETURN(NESTED_LOOP_OK);
    }
  }
  if (update_sum_func(join->sum_funcs))
    DBUG_RETURN(NESTED_LOOP_ERROR);
  DBUG_RETURN(NESTED_LOOP_OK);
}


/*
  If not selecting by given key, create an index how records should be read

  SYNOPSIS
   create_sort_index()
     thd		Thread handler
     join		Join with table to sort
     order		How table should be sorted
     filesort_limit	Max number of rows that needs to be sorted
     select_limit	Max number of rows in final output
		        Used to decide if we should use index or not
  IMPLEMENTATION
   - If there is an index that can be used, the first non-const join_tab in
     'join' is modified to use this index.
   - If no index, create with filesort() an index file that can be used to
     retrieve rows in order (should be done with 'read_record').
     The sorted data is stored in tab->table() and will be freed when calling
     free_io_cache(tab->table()).

  RETURN VALUES
    0		ok
    -1		Some fatal error
    1		No records
*/

static int
create_sort_index(THD *thd, JOIN *join, QEP_TAB *tab)
{
  ha_rows examined_rows, found_rows, returned_rows;
  TABLE *table;
  bool status;
  Filesort *fsort= tab->filesort;
  DBUG_ENTER("create_sort_index");

  // One row, no need to sort. make_tmp_tables_info should already handle this.
  DBUG_ASSERT(!join->plan_is_const() && fsort);
  table=  tab->table();

  table->sort.io_cache=(IO_CACHE*) my_malloc(key_memory_TABLE_sort_io_cache,
                                             sizeof(IO_CACHE),
                                             MYF(MY_WME | MY_ZEROFILL));
  table->status=0;				// May be wrong if quick_select

  // If table has a range, move it to select
  if (tab->quick() && tab->ref().key >= 0)
  {
    if (tab->type() != JT_REF_OR_NULL && tab->type() != JT_FT)
    {
      DBUG_ASSERT(tab->type() == JT_REF || tab->type() == JT_EQ_REF);
      // Update ref value
      if ((cp_buffer_from_ref(thd, table, &tab->ref()) && thd->is_fatal_error))
        goto err;                                   // out of memory
    }
  }

  /* Fill schema tables with data before filesort if it's necessary */
  if ((join->select_lex->active_options() & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(join, PROCESSED_BY_CREATE_SORT_INDEX))
    goto err;

  if (table->s->tmp_table)
    table->file->info(HA_STATUS_VARIABLE);	// Get record count
  status= filesort(thd, fsort, tab->keep_current_rowid,
                   &examined_rows, &found_rows, &returned_rows);
  table->sort.found_records= returned_rows;
  tab->set_records(found_rows);                     // For SQL_CALC_ROWS
  tab->join()->examined_rows+=examined_rows;
  table->set_keyread(FALSE); // Restore if we used indexes
  if (tab->type() == JT_FT)
    table->file->ft_end();
  else
    table->file->ha_index_or_rnd_end();
  DBUG_RETURN(status);
err:
  DBUG_RETURN(-1);
}


/*****************************************************************************
  Remove duplicates from tmp table
  This should be recoded to add a unique index to the table and remove
  duplicates
  Table is a locked single thread table
  fields is the number of fields to check (from the end)
*****************************************************************************/

static bool compare_record(TABLE *table, Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_offset(table->s->rec_buff_length))
      return 1;
  }
  return 0;
}

static bool copy_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      if (((Field_blob *) (*ptr))->copy())
	return 1;				// Error
  }
  return 0;
}

static void free_blobs(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->mem_free();
  }
}


bool
QEP_TAB::remove_duplicates()
{
  bool error;
  ulong reclength,offset;
  uint field_count;
  List<Item> *field_list= (this-1)->fields;
  DBUG_ENTER("remove_duplicates");

  DBUG_ASSERT(join()->tmp_tables > 0 && table()->s->tmp_table != NO_TMP_TABLE);
  THD_STAGE_INFO(join()->thd, stage_removing_duplicates);

  TABLE *const tbl= table();

  tbl->reginfo.lock_type=TL_WRITE;

  /* Calculate how many saved fields there is in list */
  field_count=0;
  List_iterator<Item> it(*field_list);
  Item *item;
  while ((item=it++))
  {
    if (item->get_tmp_table_field() && ! item->const_item())
      field_count++;
  }

  if (!field_count &&
      !join()->calc_found_rows &&
      !having) 
  {                    // only const items with no OPTION_FOUND_ROWS
    join()->unit->select_limit_cnt= 1;		// Only send first row
    DBUG_RETURN(false);
  }
  Field **first_field= tbl->field+ tbl->s->fields - field_count;
  offset= (field_count ? 
           tbl->field[tbl->s->fields - field_count]->
           offset(tbl->record[0]) : 0);
  reclength= tbl->s->reclength-offset;

  free_io_cache(tbl);				// Safety
  tbl->file->info(HA_STATUS_VARIABLE);
  if (tbl->s->db_type() == heap_hton ||
      (!tbl->s->blob_fields &&
       ((ALIGN_SIZE(reclength) + HASH_OVERHEAD) * tbl->file->stats.records <
	join()->thd->variables.sortbuff_size)))
    error=remove_dup_with_hash_index(join()->thd, tbl,
				     field_count, first_field,
				     reclength, having);
  else
    error=remove_dup_with_compare(join()->thd, tbl, first_field, offset,
				  having);

  free_blobs(first_field);
  DBUG_RETURN(error);
}


static bool remove_dup_with_compare(THD *thd, TABLE *table, Field **first_field,
                                    ulong offset, Item *having)
{
  handler *file=table->file;
  char *org_record,*new_record;
  uchar *record;
  int error;
  ulong reclength= table->s->reclength-offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record=(char*) (record=table->record[0])+offset;
  new_record=(char*) table->record[1]+offset;

  if ((error= file->ha_rnd_init(1)))
    goto err;
  error=file->ha_rnd_next(record);
  for (;;)
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      error=0;
      goto err;
    }
    if (error)
    {
      if (error == HA_ERR_RECORD_DELETED)
      {
        error= file->ha_rnd_next(record);
        continue;
      }
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->ha_delete_row(record)))
	goto err;
      error=file->ha_rnd_next(record);
      continue;
    }
    if (copy_blobs(first_field))
    {
      error=0;
      goto err;
    }
    memcpy(new_record,org_record,reclength);

    /* Read through rest of file and mark duplicated rows deleted */
    bool found=0;
    for (;;)
    {
      if ((error=file->ha_rnd_next(record)))
      {
	if (error == HA_ERR_RECORD_DELETED)
	  continue;
	if (error == HA_ERR_END_OF_FILE)
	  break;
	goto err;
      }
      if (compare_record(table, first_field) == 0)
      {
	if ((error=file->ha_delete_row(record)))
	  goto err;
      }
      else if (!found)
      {
	found=1;
	file->position(record);	// Remember position
      }
    }
    if (!found)
      break;					// End of file
    /* Restart search on next row */
    error=file->ha_rnd_pos(record, file->ref);
  }

  file->extra(HA_EXTRA_NO_CACHE);
  DBUG_RETURN(false);
err:
  file->extra(HA_EXTRA_NO_CACHE);
  if (file->inited)
    (void) file->ha_rnd_end();
  if (error)
    file->print_error(error,MYF(0));
  DBUG_RETURN(true);
}


/**
  Generate a hash index for each row to quickly find duplicate rows.

  @note
    Note that this will not work on tables with blobs!
*/

static bool remove_dup_with_hash_index(THD *thd, TABLE *table,
                                       uint field_count,
                                       Field **first_field,
                                       ulong key_length,
                                       Item *having)
{
  uchar *key_buffer, *key_pos, *record=table->record[0];
  int error;
  handler *file= table->file;
  ulong extra_length= ALIGN_SIZE(key_length)-key_length;
  uint *field_lengths,*field_length;
  HASH hash;
  DBUG_ENTER("remove_dup_with_hash_index");

  if (!my_multi_malloc(key_memory_hash_index_key_buffer,
                       MYF(MY_WME),
		       &key_buffer,
		       (uint) ((key_length + extra_length) *
			       (long) file->stats.records),
		       &field_lengths,
		       (uint) (field_count*sizeof(*field_lengths)),
		       NullS))
    DBUG_RETURN(true);

  {
    Field **ptr;
    ulong total_length= 0;
    for (ptr= first_field, field_length=field_lengths ; *ptr ; ptr++)
    {
      uint length= (*ptr)->sort_length();
      (*field_length++)= length;
      total_length+= length;
    }
    DBUG_PRINT("info",("field_count: %u  key_length: %lu  total_length: %lu",
                       field_count, key_length, total_length));
    DBUG_ASSERT(total_length <= key_length);
    key_length= total_length;
    extra_length= ALIGN_SIZE(key_length)-key_length;
  }

  if (my_hash_init(&hash, &my_charset_bin, (uint) file->stats.records, 0, 
                   key_length, (my_hash_get_key) 0, 0, 0,
                   key_memory_hash_index_key_buffer))
  {
    my_free(key_buffer);
    DBUG_RETURN(true);
  }

  if ((error= file->ha_rnd_init(1)))
    goto err;
  key_pos=key_buffer;
  for (;;)
  {
    uchar *org_key_pos;
    if (thd->killed)
    {
      thd->send_kill_message();
      error=0;
      goto err;
    }
    if ((error=file->ha_rnd_next(record)))
    {
      if (error == HA_ERR_RECORD_DELETED)
	continue;
      if (error == HA_ERR_END_OF_FILE)
	break;
      goto err;
    }
    if (having && !having->val_int())
    {
      if ((error=file->ha_delete_row(record)))
	goto err;
      continue;
    }

    /* copy fields to key buffer */
    org_key_pos= key_pos;
    field_length=field_lengths;
    for (Field **ptr= first_field ; *ptr ; ptr++)
    {
      (*ptr)->make_sort_key(key_pos,*field_length);
      key_pos+= *field_length++;
    }
    /* Check if it exists before */
    if (my_hash_search(&hash, org_key_pos, key_length))
    {
      /* Duplicated found ; Remove the row */
      if ((error=file->ha_delete_row(record)))
	goto err;
    }
    else
    {
      if (my_hash_insert(&hash, org_key_pos))
        goto err;
    }
    key_pos+=extra_length;
  }
  my_free(key_buffer);
  my_hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  (void) file->ha_rnd_end();
  DBUG_RETURN(false);

err:
  my_free(key_buffer);
  my_hash_free(&hash);
  file->extra(HA_EXTRA_NO_CACHE);
  if (file->inited)
    (void) file->ha_rnd_end();
  if (error)
    file->print_error(error,MYF(0));
  DBUG_RETURN(true);
}


/*
  eq_ref: Create the lookup key and check if it is the same as saved key

  SYNOPSIS
    cmp_buffer_with_ref()
      tab      Join tab of the accessed table
      table    The table to read.  This is usually tab->table(), except for 
               semi-join when we might need to make a lookup in a temptable
               instead.
      tab_ref  The structure with methods to collect index lookup tuple. 
               This is usually table->ref, except for the case of when we're 
               doing lookup into semi-join materialization table.

  DESCRIPTION 
    Used by eq_ref access method: create the index lookup key and check if 
    we've used this key at previous lookup (If yes, we don't need to repeat
    the lookup - the record has been already fetched)

  RETURN 
    TRUE   No cached record for the key, or failed to create the key (due to
           out-of-domain error)
    FALSE  The created key is the same as the previous one (and the record 
           is already in table->record)
*/

static bool
cmp_buffer_with_ref(THD *thd, TABLE *table, TABLE_REF *tab_ref)
{
  bool no_prev_key;
  if (!tab_ref->disable_cache)
  {
    if (!(no_prev_key= tab_ref->key_err))
    {
      /* Previous access found a row. Copy its key */
      memcpy(tab_ref->key_buff2, tab_ref->key_buff, tab_ref->key_length);
    }
  }
  else 
    no_prev_key= TRUE;
  if ((tab_ref->key_err= cp_buffer_from_ref(thd, table, tab_ref)) ||
      no_prev_key)
    return 1;
  return memcmp(tab_ref->key_buff2, tab_ref->key_buff, tab_ref->key_length)
    != 0;
}


bool
cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref)
{
  enum enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  bool result= 0;

  for (uint part_no= 0; part_no < ref->key_parts; part_no++)
  {
    store_key *s_key= ref->key_copy[part_no];
    if (!s_key)
      continue;

    if (s_key->copy() & 1)
    {
      result= 1;
      break;
    }
  }
  thd->count_cuted_fields= save_count_cuted_fields;
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return result;
}


/**
  allocate group fields or take prepared (cached).

  @param main_join   join of current select
  @param curr_join   current join (join of current select or temporary copy
                     of it)

  @retval
    0   ok
  @retval
    1   failed
*/

bool
make_group_fields(JOIN *main_join, JOIN *curr_join)
{
  if (main_join->group_fields_cache.elements)
  {
    curr_join->group_fields= main_join->group_fields_cache;
    curr_join->sort_and_group= 1;
  }
  else
  {
    if (alloc_group_fields(curr_join, curr_join->group_list))
      return (1);
    main_join->group_fields_cache= curr_join->group_fields;
  }
  return (0);
}


/**
  Get a list of buffers for saveing last group.

  Groups are saved in reverse order for easyer check loop.
*/

bool
alloc_group_fields(JOIN *join, ORDER *group)
{
  if (group)
  {
    for (; group ; group=group->next)
    {
      Cached_item *tmp=new_Cached_item(join->thd, *group->item, FALSE);
      if (!tmp || join->group_fields.push_front(tmp))
	return TRUE;
    }
  }
  join->sort_and_group=1;			/* Mark for do_select */
  return FALSE;
}


/*
  Test if a single-row cache of items changed, and update the cache.

  @details Test if a list of items that typically represents a result
  row has changed. If the value of some item changed, update the cached
  value for this item.
  
  @param list list of <item, cached_value> pairs stored as Cached_item.

  @return -1 if no item changed
  @return index of the first item that changed
*/

int test_if_item_cache_changed(List<Cached_item> &list)
{
  DBUG_ENTER("test_if_item_cache_changed");
  List_iterator<Cached_item> li(list);
  int idx= -1,i;
  Cached_item *buff;

  for (i=(int) list.elements-1 ; (buff=li++) ; i--)
  {
    if (buff->cmp())
      idx=i;
  }
  DBUG_PRINT("info", ("idx: %d", idx));
  DBUG_RETURN(idx);
}


/**
  Setup copy_fields to save fields at start of new group.

  Setup copy_fields to save fields at start of new group

  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_result_set_metadata.

  @param thd                   THD pointer
  @param param                 temporary table parameters
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @todo
    In most cases this result will be sent to the user.
    This should be changed to use copy_int or copy_real depending
    on how the value is to be used: In some cases this may be an
    argument in a group function, like: IF(ISNULL(col),0,COUNT(*))

  @retval
    0     ok
  @retval
    !=0   error
*/

bool
setup_copy_fields(THD *thd, Temp_table_param *param,
		  Ref_ptr_array ref_pointer_array,
		  List<Item> &res_selected_fields, List<Item> &res_all_fields,
		  uint elements, List<Item> &all_fields)
{
  Item *pos;
  List_iterator_fast<Item> li(all_fields);
  Copy_field *copy= NULL;
  Copy_field *copy_start MY_ATTRIBUTE((unused));
  res_selected_fields.empty();
  res_all_fields.empty();
  List_iterator_fast<Item> itr(res_all_fields);
  List<Item> extra_funcs;
  uint i, border= all_fields.elements - elements;
  DBUG_ENTER("setup_copy_fields");

  if (param->field_count && 
      !(copy=param->copy_field= new Copy_field[param->field_count]))
    goto err2;

  param->copy_funcs.empty();
  copy_start= copy;
  for (i= 0; (pos= li++); i++)
  {
    Field *field;
    uchar *tmp;
    Item *real_pos= pos->real_item();
    /*
      Aggregate functions can be substituted for fields (by e.g. temp tables).
      We need to filter those substituted fields out.
    */
    if (real_pos->type() == Item::FIELD_ITEM &&
        !(real_pos != pos &&
          ((Item_ref *)pos)->ref_type() == Item_ref::AGGREGATE_REF))
    {
      Item_field *item;
      if (!(item= new Item_field(thd, ((Item_field*) real_pos))))
	goto err;
      if (pos->type() == Item::REF_ITEM)
      {
        /* preserve the names of the ref when dereferncing */
        Item_ref *ref= (Item_ref *) pos;
        item->db_name= ref->db_name;
        item->table_name= ref->table_name;
        item->item_name= ref->item_name;
      }
      pos= item;
      if (item->field->flags & BLOB_FLAG)
      {
	if (!(pos= Item_copy::create(pos)))
	  goto err;
       /*
         Item_copy_string::copy for function can call 
         Item_copy_string::val_int for blob via Item_ref.
         But if Item_copy_string::copy for blob isn't called before,
         it's value will be wrong
         so let's insert Item_copy_string for blobs in the beginning of 
         copy_funcs
         (to see full test case look at having.test, BUG #4358) 
       */
	if (param->copy_funcs.push_front(pos))
	  goto err;
      }
      else
      {
	/* 
	   set up save buffer and change result_field to point at 
	   saved value
	*/
	field= item->field;
	item->result_field=field->new_field(thd->mem_root,field->table, 1);
        /*
          We need to allocate one extra byte for null handling.
        */
	if (!(tmp= static_cast<uchar*>(sql_alloc(field->pack_length() + 1))))
	  goto err;
        if (copy)
        {
          DBUG_ASSERT (param->field_count > (uint) (copy - copy_start));
          copy->set(tmp, item->result_field);
          item->result_field->move_field(copy->to_ptr, copy->to_null_ptr, 1);
          copy++;
        }
      }
    }
    else if ((real_pos->type() == Item::FUNC_ITEM ||
	      real_pos->type() == Item::SUBSELECT_ITEM ||
	      real_pos->type() == Item::CACHE_ITEM ||
	      real_pos->type() == Item::COND_ITEM) &&
	     !real_pos->with_sum_func)
    {						// Save for send fields
      pos= real_pos;
      /* TODO:
	 In most cases this result will be sent to the user.
	 This should be changed to use copy_int or copy_real depending
	 on how the value is to be used: In some cases this may be an
	 argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      if (!(pos= Item_copy::create(pos)))
	goto err;
      if (i < border)                           // HAVING, ORDER and GROUP BY
      {
        if (extra_funcs.push_back(pos))
          goto err;
      }
      else if (param->copy_funcs.push_back(pos))
	goto err;
    }
    res_all_fields.push_back(pos);
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      pos;
  }
  param->copy_field_end= copy;

  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  /*
    Put elements from HAVING, ORDER BY and GROUP BY last to ensure that any
    reference used in these will resolve to a item that is already calculated
  */
  param->copy_funcs.concat(&extra_funcs);

  DBUG_RETURN(0);

 err:
  if (copy)
    delete [] param->copy_field;			// This is never 0
  param->copy_field=0;
err2:
  DBUG_RETURN(TRUE);
}


/**
  Make a copy of all simple SELECT'ed items.

  This is done at the start of a new group so that we can retrieve
  these later when the group changes.
  @returns false if OK, true on error.
*/

bool
copy_fields(Temp_table_param *param, const THD *thd)
{
  Copy_field *ptr=param->copy_field;
  Copy_field *end=param->copy_field_end;

  DBUG_ASSERT((ptr != NULL && end >= ptr) || (ptr == NULL && end == NULL));

  for (; ptr < end; ptr++)
    ptr->invoke_do_copy(ptr);

  List_iterator_fast<Item> it(param->copy_funcs);
  Item_copy *item;
  bool is_error= thd->is_error();
  while (!is_error && (item= (Item_copy*) it++))
    is_error= item->copy(thd);

  return is_error;
}


/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param thd                   THD pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0     ok
  @retval
    !=0   error
*/

bool
change_to_use_tmp_fields(THD *thd, Ref_ptr_array ref_pointer_array,
			 List<Item> &res_selected_fields,
			 List<Item> &res_all_fields,
			 uint elements, List<Item> &all_fields)
{
  List_iterator_fast<Item> it(all_fields);
  Item *item_field,*item;
  DBUG_ENTER("change_to_use_tmp_fields");

  res_selected_fields.empty();
  res_all_fields.empty();

  uint border= all_fields.elements - elements;
  for (uint i= 0; (item= it++); i++)
  {
    Field *field;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
      item_field= item;
    else if (item->type() == Item::FIELD_ITEM)
      item_field= item->get_tmp_table_item(thd);
    else if (item->type() == Item::FUNC_ITEM &&
             ((Item_func*)item)->functype() == Item_func::SUSERVAR_FUNC)
    {
      field= item->get_tmp_table_field();
      if (field != NULL)
      {
        /*
          Replace "@:=<expression>" with "@:=<tmp table column>". Otherwise, we
          would re-evaluate <expression>, and if expression were a subquery, this
          would access already-unlocked tables.
        */
        Item_func_set_user_var* suv=
          new Item_func_set_user_var(thd, (Item_func_set_user_var*) item);
        Item_field *new_field= new Item_field(field);
        if (!suv || !new_field)
          DBUG_RETURN(true);                  // Fatal error
        List<Item> list;
        list.push_back(new_field);
        suv->set_arguments(list, true);
        item_field= suv;
      }
      else
        item_field= item;
    }
    else if ((field= item->get_tmp_table_field()))
    {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group)
        item_field= ((Item_sum*) item)->result_item(field);
      else
        item_field= (Item*) new Item_field(field);
      if (!item_field)
        DBUG_RETURN(true);                    // Fatal error

      if (item->real_item()->type() != Item::FIELD_ITEM)
        field->orig_table= 0;
      item_field->item_name= item->item_name;
      if (item->type() == Item::REF_ITEM)
      {
        Item_field *ifield= (Item_field *) item_field;
        Item_ref *iref= (Item_ref *) item;
        ifield->table_name= iref->table_name;
        ifield->db_name= iref->db_name;
      }
#ifndef DBUG_OFF
      if (!item_field->item_name.is_set())
      {
        char buff[256];
        String str(buff,sizeof(buff),&my_charset_bin);
        str.length(0);
        item->print(&str, QT_ORDINARY);
        item_field->item_name.copy(str.ptr(), str.length());
      }
#endif
    }
    else
      item_field= item;

    res_all_fields.push_back(item_field);
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      item_field;
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (uint i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);
  DBUG_RETURN(false);
}


/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  @param thd                   THD pointer
  @param ref_pointer_array     array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @retval
    0	ok
  @retval
    1	error
*/

bool
change_refs_to_tmp_fields(THD *thd, Ref_ptr_array ref_pointer_array,
			  List<Item> &res_selected_fields,
			  List<Item> &res_all_fields, uint elements,
			  List<Item> &all_fields)
{
  List_iterator_fast<Item> it(all_fields);
  Item *item, *new_item;
  res_selected_fields.empty();
  res_all_fields.empty();

  uint i, border= all_fields.elements - elements;
  for (i= 0; (item= it++); i++)
  {
    res_all_fields.push_back(new_item= item->get_tmp_table_item(thd));
    ref_pointer_array[((i < border)? all_fields.elements-i-1 : i-border)]=
      new_item;
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (i= 0; i < border; i++)
    itr++;
  itr.sublist(res_selected_fields, elements);

  return thd->is_fatal_error;
}


/**
  Save NULL-row info for constant tables. Used in conjunction with
  restore_const_null_info() to restore constant table null_row and
  status values after temporarily marking rows as NULL. This is only
  done for const tables in subqueries because these values are not
  recalculated on next execution of the subquery.

  @param join               The join for which const tables are about to be
                            marked as containing only NULL values
  @param[out] save_nullinfo Const tables that have null_row=false and
                            STATUS_NULL_ROW set are tagged in this
                            table_map so that the value can be
                            restored by restore_const_null_info()

  @see TABLE::set_null_row
  @see restore_const_null_info
*/
static void save_const_null_info(JOIN *join, table_map *save_nullinfo)
{
  DBUG_ASSERT(join->const_tables);

  for (uint tableno= 0; tableno < join->const_tables; tableno++)
  {
    QEP_TAB *const tab= join->qep_tab + tableno;
    TABLE *const table= tab->table();
    /*
      table->status and table->null_row must be in sync: either both set
      or none set. Otherwise, an additional table_map parameter is
      needed to save/restore_const_null_info() these separately
    */
    DBUG_ASSERT(table->has_null_row() ? (table->status & STATUS_NULL_ROW) :
                                        !(table->status & STATUS_NULL_ROW));

    if (!table->has_null_row())
      *save_nullinfo|= tab->table_ref->map();
  }
}

/**
  Restore NULL-row info for constant tables. Used in conjunction with
  save_const_null_info() to restore constant table null_row and status
  values after temporarily marking rows as NULL. This is only done for
  const tables in subqueries because these values are not recalculated
  on next execution of the subquery.

  @param join            The join for which const tables have been
                         marked as containing only NULL values
  @param save_nullinfo   Const tables that had null_row=false and
                         STATUS_NULL_ROW set when
                         save_const_null_info() was called

  @see TABLE::set_null_row
  @see save_const_null_info
*/
static void restore_const_null_info(JOIN *join, table_map save_nullinfo)
{
  DBUG_ASSERT(join->const_tables && save_nullinfo);

  for (uint tableno= 0; tableno < join->const_tables; tableno++)
  {
    QEP_TAB *const tab= join->qep_tab + tableno;
    if ((save_nullinfo & tab->table_ref->map()))
    {
      /*
        The table had null_row=false and STATUS_NULL_ROW set when
        save_const_null_info was called
      */
      tab->table()->reset_null_row();
    }
  }
}


/****************************************************************************
  QEP_tmp_table implementation
****************************************************************************/

/**
  @brief Instantiate tmp table and start index scan if necessary
  @todo Tmp table always would be created, even for empty result. Extend
        executor to avoid tmp table creation when no rows were written
        into tmp table.
  @return
    true  error
    false ok
*/

bool
QEP_tmp_table::prepare_tmp_table()
{
  TABLE *table= qep_tab->table();
  JOIN *join= qep_tab->join();
  int rc= 0;

  Temp_table_param *const tmp_tbl= qep_tab->tmp_table_param;
  if (!table->is_created())
  {
    if (instantiate_tmp_table(table, tmp_tbl->keyinfo,
                              tmp_tbl->start_recinfo,
                              &tmp_tbl->recinfo,
                              join->select_lex->active_options(),
                              join->thd->variables.big_tables,
                              &join->thd->opt_trace))
      return true;
    (void) table->file->extra(HA_EXTRA_WRITE_CACHE);
    empty_record(table);
  }
  /* If it wasn't already, start index scan for grouping using table index. */
  if (!table->file->inited &&
      ((table->group &&
        tmp_tbl->sum_func_count && table->s->keys) ||
       table->hash_field))
    rc= table->file->ha_index_init(0, 0);
  else
  {
    /* Start index scan in scanning mode */
    rc= table->file->ha_rnd_init(true);
  }
  if (rc)
  {
    table->file->print_error(rc, MYF(0));
    return true;
  }
  return false;
}


/**
  @brief Prepare table if necessary and call write_func to save record

  @param end_of_record  the end_of_record signal to pass to the writer

  @return return one of enum_nested_loop_state.
*/

enum_nested_loop_state
QEP_tmp_table::put_record(bool end_of_records)
{
  // Lasy tmp table creation/initialization
  if (!qep_tab->table()->file->inited && prepare_tmp_table())
    return NESTED_LOOP_ERROR;
  enum_nested_loop_state rc= (*write_func)(qep_tab->join(), qep_tab,
                                           end_of_records);
  return rc;
}


/**
  @brief Finish rnd/index scan after accumulating records, switch ref_array,
         and send accumulated records further.
  @return return one of enum_nested_loop_state.
*/

enum_nested_loop_state
QEP_tmp_table::end_send()
{
  enum_nested_loop_state rc= NESTED_LOOP_OK;
  TABLE *table= qep_tab->table();
  JOIN *join= qep_tab->join();

  // All records were stored, send them further
  int tmp, new_errno= 0;

  if ((rc= put_record(true)) < NESTED_LOOP_OK)
    return rc;

  if ((tmp= table->file->extra(HA_EXTRA_NO_CACHE)))
  {
    DBUG_PRINT("error",("extra(HA_EXTRA_NO_CACHE) failed"));
    new_errno= tmp;
  }
  if ((tmp= table->file->ha_index_or_rnd_end()))
  {
    DBUG_PRINT("error",("ha_index_or_rnd_end() failed"));
    new_errno= tmp;
  }
  if (new_errno)
  {
    table->file->print_error(new_errno,MYF(0));
    return NESTED_LOOP_ERROR;
  }
  // Update ref array
  join->set_items_ref_array(*qep_tab->ref_array);
  table->reginfo.lock_type= TL_UNLOCK;

  bool in_first_read= true;
  while (rc == NESTED_LOOP_OK)
  {
    int error;
    if (in_first_read)
    {
      in_first_read= false;
      error= join_init_read_record(qep_tab);
    }
    else
      error= qep_tab->read_record.read_record(&qep_tab->read_record);

    if (error > 0 || (join->thd->is_error()))   // Fatal error
      rc= NESTED_LOOP_ERROR;
    else if (error < 0)
      break;
    else if (join->thd->killed)		  // Aborted by user
    {
      join->thd->send_kill_message();
      rc= NESTED_LOOP_KILLED;
    }
    else
      rc= evaluate_join_record(join, qep_tab);
  }

  // Finish rnd scn after sending records
  if (table->file->inited)
    table->file->ha_rnd_end();

  return rc;
}


/******************************************************************************
  Code for pfs_batch_update
******************************************************************************/


bool QEP_TAB::pfs_batch_update(JOIN *join)
{
  /*
    Use PFS batch mode unless
     1. tab is not an inner-most table, or
     2. a table has eq_ref or const access type, or
     3. this tab contains a subquery that accesses one or more tables
  */

  return !((join->qep_tab + join->primary_tables - 1) != this || // 1
           this->type() == JT_EQ_REF ||                          // 2
           this->type() == JT_CONST  ||
           this->type() == JT_SYSTEM ||
           (condition() && condition()->has_subquery()));        // 3
}

/**
  @} (end of group Query_Executor)
*/


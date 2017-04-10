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

/*
  Delete of records tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/

#include "sql_delete.h"

#include <limits.h>
#include <string.h>

#include "auth_acls.h"
#include "auth_common.h"              // check_table_access
#include "binlog.h"                   // mysql_bin_log
#include "debug_sync.h"               // DEBUG_SYNC
#include "filesort.h"                 // Filesort
#include "handler.h"
#include "item.h"
#include "key.h"
#include "mem_root_array.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld.h"                   // stage_...
#include "mysqld_error.h"
#include "opt_explain.h"              // Modification_plan
#include "opt_explain_format.h"
#include "opt_range.h"                // prune_partitions
#include "opt_trace.h"                // Opt_trace_object
#include "psi_memory_key.h"
#include "query_options.h"
#include "records.h"                  // READ_RECORD
#include "session_tracker.h"
#include "sql_base.h"                 // update_non_unique_table_error
#include "sql_bitmap.h"
#include "sql_cache.h"                // query_cache
#include "sql_class.h"
#include "sql_const.h"
#include "sql_executor.h"
#include "sql_list.h"
#include "sql_optimizer.h"            // optimize_cond, substitute_gc
#include "sql_resolver.h"             // setup_order
#include "sql_select.h"
#include "sql_sort.h"
#include "sql_string.h"
#include "sql_view.h"                 // check_key_in_view
#include "system_variables.h"
#include "table.h"
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "thr_malloc.h"
#include "transaction_info.h"
#include "trigger_def.h"
#include "uniques.h"                  // Unique

class COND_EQUAL;
class Item_exists_subselect;
class Opt_trace_context;

bool Sql_cmd_delete::precheck(THD *thd)
{
  DBUG_ENTER("Sql_cmd_delete::precheck");

  TABLE_LIST *tables= lex->query_tables;

  if (!multitable)
  {
    if (check_one_table_access(thd, DELETE_ACL, tables))
      DBUG_RETURN(true);

    // Set desired privilege for the columns of the WHERE clause
    tables->set_want_privilege(SELECT_ACL);
  }
  else
  {
    TABLE_LIST *aux_tables= delete_tables->first;
    TABLE_LIST **save_query_tables_own_last= lex->query_tables_own_last;

    if (check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
      DBUG_RETURN(true);

    /*
      Since aux_tables list is not part of LEX::query_tables list we
      have to juggle with LEX::query_tables_own_last value to be able
      call check_table_access() safely.
    */
    lex->query_tables_own_last= 0;
    if (check_table_access(thd, DELETE_ACL, aux_tables, false, UINT_MAX, false))
    {
      lex->query_tables_own_last= save_query_tables_own_last;
      DBUG_RETURN(true);
    }
    lex->query_tables_own_last= save_query_tables_own_last;
  }
  DBUG_RETURN(false);
}


/**
  Delete a set of rows from a single table.

  @param thd    Thread handler

  @returns false on success, true on error

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool Sql_cmd_delete::delete_from_single_table(THD *thd)
{
  DBUG_ENTER("Sql_cmd_delete::delete_from_single_table");

  myf           error_flags= MYF(0);            /**< Flag for fatal errors */
  bool          will_batch;
  /*
    Most recent handler error
    =  1: Some non-handler error
    =  0: Success
    = -1: No more rows to process, or reached limit
  */
  int           error= 0;
  READ_RECORD   info;
  ha_rows       deleted_rows= 0;
  bool          reverse= false;
  /// read_removal is only used by NDB storage engine
  bool          read_removal= false;
  bool          need_sort= false;

  uint usable_index= MAX_KEY;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= select_lex->master_unit();
  ORDER *order= select_lex->order_list.first;
  TABLE_LIST *const table_list= select_lex->get_table_list();
  THD::killed_state killed_status= THD::NOT_KILLED;
  THD::enum_binlog_query_type query_type= THD::ROW_QUERY_TYPE;

  const bool safe_update= thd->variables.option_bits & OPTION_SAFE_UPDATES;

  TABLE_LIST *const delete_table_ref= table_list->updatable_base_table();
  TABLE *const table= delete_table_ref->table;

  const bool transactional_table= table->file->has_transactions();

  const bool has_delete_triggers=
    table->triggers && table->triggers->has_delete_triggers();

  const bool has_before_triggers=
    has_delete_triggers && table->triggers->has_triggers(TRG_EVENT_DELETE,
                                                         TRG_ACTION_BEFORE);
  const bool has_after_triggers=
    has_delete_triggers && table->triggers->has_triggers(TRG_EVENT_DELETE,
                                                         TRG_ACTION_AFTER);
  unit->set_limit(thd, select_lex);

  ha_rows limit= unit->select_limit_cnt;
  const bool using_limit= limit != HA_POS_ERROR;

  // Used to track whether there are no rows that need to be read
  bool no_rows= limit == 0;

  Item *conds;
  if (select_lex->get_optimizable_conditions(thd, &conds, NULL))
    DBUG_RETURN(true);                 /* purecov: inspected */

  /*
    See if we can substitute expressions with equivalent generated
    columns in the WHERE and ORDER BY clauses of the DELETE statement.
    It is unclear if this is best to do before or after the other
    substitutions performed by substitute_for_best_equal_field(). Do
    it here for now, to keep it consistent with how multi-table
    deletes are optimized in JOIN::optimize().
  */
  if (conds || order)
    static_cast<void>(substitute_gc(thd, select_lex, conds, NULL, order));

  QEP_TAB_standalone qep_tab_st;
  QEP_TAB &qep_tab= qep_tab_st.as_QEP_TAB();

  if (table->all_partitions_pruned_away)
  {
    /*
      All partitions were pruned away during preparation. Shortcut further
      processing by "no rows". If explaining, report the plan and bail out.
    */
    no_rows= true;

    if (lex->describe)
    {
      Modification_plan plan(thd, MT_DELETE, table,
                             "No matching rows after partition pruning",
                             true, 0);
      bool err= explain_single_table_modification(thd, &plan, select_lex);
      DBUG_RETURN(err);
    }
  }

  const bool const_cond= (!conds || conds->const_item());
  if (safe_update && const_cond)
  {
    // Safe mode is a runtime check, so apply it in execution and not prepare
    my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
    DBUG_RETURN(true);
  }

  const bool const_cond_result= const_cond && (!conds || conds->val_int());
  if (thd->is_error())   // Error during val_int()
    DBUG_RETURN(true);                 /* purecov: inspected */
  /*
    We are passing HA_EXTRA_IGNORE_DUP_KEY flag here to recreate query with
    IGNORE keyword within federated storage engine. If federated engine is
    removed in the future, use of HA_EXTRA_IGNORE_DUP_KEY and
    HA_EXTRA_NO_IGNORE_DUP_KEY flag should be removed from
    delete_from_single_table(), Query_result_delete::optimize() and
  */
  if (lex->is_ignore())
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);

  /*
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.

    We can use delete_all_rows() if and only if:
    - We allow new functions (not using option --skip-new)
    - There is no limit clause
    - The condition is constant
    - If there is a condition, then it it produces a non-zero value
    - If the current command is DELETE FROM with no where clause, then:
      - We will not be binlogging this statement in row-based, and
      - there should be no delete triggers associated with the table.
  */
  if (!using_limit && const_cond_result &&
      !(specialflag & SPECIAL_NO_NEW_FUNC) &&
      ((!thd->is_current_stmt_binlog_format_row() ||  // not ROW binlog-format
        thd->is_current_stmt_binlog_disabled()) && // no binlog for this command
       !has_delete_triggers))
  {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->file->stats.records;

    Modification_plan plan(thd, MT_DELETE, table,
                           "Deleting all rows", false, maybe_deleted);
    if (lex->describe)
    {
      bool err= explain_single_table_modification(thd, &plan, select_lex);
      DBUG_RETURN(err);
    }

    DBUG_PRINT("debug", ("Trying to use delete_all_rows()"));
    if (!(error=table->file->ha_delete_all_rows()))
    {
      /*
        As delete_all_rows() was used, we have to log it in statement format.
      */
      query_type= THD::STMT_QUERY_TYPE;
      error= -1;
      deleted_rows= maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      if (table->file->is_fatal_error(error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }

  if (conds)
  {
    COND_EQUAL *cond_equal= NULL;
    Item::cond_result result;

    if (optimize_cond(thd, &conds, &cond_equal, select_lex->join_list,
                      &result))
      DBUG_RETURN(true);
    if (result == Item::COND_FALSE)             // Impossible where
    {
      no_rows= true;

      if (lex->describe)
      {
        Modification_plan plan(thd, MT_DELETE, table,
                               "Impossible WHERE", true, 0);
        bool err= explain_single_table_modification(thd, &plan, select_lex);
        DBUG_RETURN(err);
      }
    }
    if (conds)
    {
      conds= substitute_for_best_equal_field(conds, cond_equal, 0);
      if (conds == NULL)
        DBUG_RETURN(true);

      conds->update_used_tables();
    }
  }

  // Initialize the cost model that will be used for this table
  table->init_cost_model(thd->cost_model());

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  // These have been cleared when binding the TABLE object.
  DBUG_ASSERT(table->quick_keys.is_clear_all() &&
              table->possible_quick_keys.is_clear_all());

  table->covering_keys.clear_all();

  /* Prune a second time to be able to prune on subqueries in WHERE clause. */
  if (prune_partitions(thd, table, conds))
    DBUG_RETURN(true);
  if (table->all_partitions_pruned_away)
  {
    /* No matching records */
    if (lex->describe)
    {
      Modification_plan plan(thd, MT_DELETE, table,
                             "No matching rows after partition pruning",
                             true, 0);
      bool err= explain_single_table_modification(thd, &plan, select_lex);
      DBUG_RETURN(err);
    }
    my_ok(thd, 0);
    DBUG_RETURN(false);
  }

  qep_tab.set_table(table);
  qep_tab.set_condition(conds);

  { // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(delete_table_ref);

    if (!no_rows && conds != NULL)
    {
      Key_map keys_to_use(Key_map::ALL_BITS), needed_reg_dummy;
      QUICK_SELECT_I *qck;
      no_rows= test_quick_select(thd, keys_to_use, 0, limit, safe_update,
                                 ORDER_NOT_RELEVANT, &qep_tab,
                                 conds, &needed_reg_dummy, &qck) < 0;
      qep_tab.set_quick(qck);
    }
    if (thd->is_error()) // test_quick_select() has improper error propagation
      DBUG_RETURN(true);

    if (no_rows)
    {
      if (lex->describe)
      {
        Modification_plan plan(thd, MT_DELETE, table,
                               "Impossible WHERE", true, 0);
        bool err= explain_single_table_modification(thd, &plan, select_lex);
        DBUG_RETURN(err);
      }

      my_ok(thd, 0);
      DBUG_RETURN(false);                       // Nothing to delete
    }
  } // Ends scope for optimizer trace wrapper

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
      DBUG_RETURN(true);
    }
  }

  if (order)
  {
    table->update_const_key_parts(conds);
    order= simple_remove_const(order, conds);
    ORDER_with_src order_src(order, ESC_ORDER_BY);
    usable_index= get_index_for_order(&order_src, &qep_tab, limit,
                                      &need_sort, &reverse);
  }

  // Reaching here only when table must be accessed
  DBUG_ASSERT(!no_rows);

  {
    ha_rows rows;
    if (qep_tab.quick())
      rows= qep_tab.quick()->records;
    else if (!conds && !need_sort && limit != HA_POS_ERROR)
      rows= limit;
    else
    {
      delete_table_ref->fetch_number_of_rows();
      rows= table->file->stats.records;
    }
    qep_tab.set_quick_optim();
    qep_tab.set_condition_optim();
    Modification_plan plan(thd, MT_DELETE, &qep_tab,
                           usable_index, limit, false, need_sort,
                           false, rows);
    DEBUG_SYNC(thd, "planned_single_delete");

    if (lex->describe)
    {
      bool err= explain_single_table_modification(thd, &plan, select_lex);
      DBUG_RETURN(err);
    }

    if (select_lex->active_options() & OPTION_QUICK)
      (void) table->file->extra(HA_EXTRA_QUICK);

    if (need_sort)
    {
      ha_rows examined_rows, found_rows, returned_rows;

      Filesort fsort(&qep_tab, order, HA_POS_ERROR);
      DBUG_ASSERT(usable_index == MAX_KEY);
      table->sort.io_cache=
        (IO_CACHE *) my_malloc(key_memory_TABLE_sort_io_cache,
                               sizeof(IO_CACHE), MYF(MY_FAE | MY_ZEROFILL));

      if (filesort(thd, &fsort, true,
                   &examined_rows, &found_rows, &returned_rows))
        DBUG_RETURN(true);

      table->sort.found_records= returned_rows;
      thd->inc_examined_row_count(examined_rows);
      /*
        Filesort has already found and selected the rows we want to delete,
        so we don't need the where clause
      */
      qep_tab.set_quick(NULL);
      qep_tab.set_condition(NULL);
      table->file->ha_index_or_rnd_end();
    }

    /* If quick select is used, initialize it before retrieving rows. */
    if (qep_tab.quick() && (error= qep_tab.quick()->reset()))
    {
      if (table->file->is_fatal_error(error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      DBUG_RETURN(true);
    }

    if (usable_index==MAX_KEY || qep_tab.quick())
      error= init_read_record(&info, thd, NULL, &qep_tab, 1, 1, FALSE);
    else
      error= init_read_record_idx(&info, thd, table, 1, usable_index, reverse);

    if (error)
      DBUG_RETURN(true);                 /* purecov: inspected */

    if (select_lex->has_ft_funcs() && init_ftfuncs(thd, select_lex))
      DBUG_RETURN(true);                 /* purecov: inspected */

    THD_STAGE_INFO(thd, stage_updating);

    if (has_after_triggers)
    {
      /*
        The table has AFTER DELETE triggers that might access to subject table
        and therefore might need delete to be done immediately. So we turn-off
        the batching.
      */
      (void) table->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
      will_batch= false;
    }
    else
    {
      // No after delete triggers, attempt to start bulk delete
      will_batch= !table->file->start_bulk_delete();
    }
    table->mark_columns_needed_for_delete(thd);
    if (thd->is_error())
      DBUG_RETURN(true);

    if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
        !using_limit &&
        !has_delete_triggers &&
        qep_tab.quick() && qep_tab.quick()->index != MAX_KEY)
      read_removal= table->check_read_removal(qep_tab.quick()->index);

    DBUG_ASSERT(limit > 0);

    // The loop that reads rows and delete those that qualify

    while (!(error=info.read_record(&info)) && !thd->killed)
    {
      DBUG_ASSERT(!thd->is_error());
      thd->inc_examined_row_count(1);

      bool skip_record;
      if (qep_tab.skip_record(thd, &skip_record))
      {
        error= 1;
        break;
      }
      if (skip_record)
      {
        table->file->unlock_row();  // Row failed condition check, release lock
        continue;
      }

      DBUG_ASSERT(!thd->is_error());
      if (has_before_triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
      {
        error= 1;
        break;
      }

      if ((error= table->file->ha_delete_row(table->record[0])))
      {
        if (table->file->is_fatal_error(error))
          error_flags|= ME_FATALERROR;

        table->file->print_error(error, error_flags);
        /*
          In < 4.0.14 we set the error number to 0 here, but that
          was not sensible, because then MySQL would not roll back the
          failed DELETE, and also wrote it to the binlog. For MyISAM
          tables a DELETE probably never should fail (?), but for
          InnoDB it can fail in a FOREIGN KEY error or an
          out-of-tablespace error.
        */
        if (thd->is_error()) // Could be downgraded to warning by IGNORE
        {
          error= 1;
          break;
        }
      }

      deleted_rows++;
      if (has_after_triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_AFTER, FALSE))
      {
        error= 1;
        break;
      }
      if (!--limit && using_limit)
      {
        error= -1;
        break;
      }
    }

    killed_status= thd->killed;
    if (killed_status != THD::NOT_KILLED || thd->is_error())
      error= 1;					// Aborted
    int loc_error;
    if (will_batch && (loc_error= table->file->end_bulk_delete()))
    {
      /* purecov: begin inspected */
      if (error != 1)
      {
        if (table->file->is_fatal_error(loc_error))
          error_flags|= ME_FATALERROR;

        table->file->print_error(loc_error, error_flags);
      }
      error=1;
      /* purecov: end */
    }
    if (read_removal)
    {
      /* Only handler knows how many records were really written */
      deleted_rows= table->file->end_read_removal();
    }
    end_read_record(&info);
    if (select_lex->active_options() & OPTION_QUICK)
      (void) table->file->extra(HA_EXTRA_NORMAL);
  } // End of scope for Modification_plan

cleanup:
  DBUG_ASSERT(!lex->describe);
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted_rows > 0)
    query_cache.invalidate_single(thd, delete_table_ref, true);

  if (!transactional_table && deleted_rows > 0)
    thd->get_transaction()->mark_modified_non_trans_table(
      Transaction_ctx::STMT);
  
  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || thd->get_transaction()->cannot_safely_rollback(
      Transaction_ctx::STMT))
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

      /*
        [binlog]: As we don't allow the use of 'handler:delete_all_rows()' when
        binlog_format == ROW, if 'handler::delete_all_rows()' was called
        we replicate statement-based; otherwise, 'ha_delete_row()' was used to
        delete specific rows which we might log row-based.
      */
      int log_result= thd->binlog_query(query_type,
                                        thd->query().str, thd->query().length,
                                        transactional_table, FALSE, FALSE,
                                        errcode);

      if (log_result)
      {
	error=1;
      }
    }
  }
  DBUG_ASSERT(transactional_table ||
              deleted_rows == 0 ||
              thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::STMT));
  if (error < 0)
  {
    my_ok(thd, deleted_rows);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted_rows));
  }
  DBUG_RETURN(error > 0);
}


/**
  Prepare a DELETE statement
*/

bool Sql_cmd_delete::prepare_inner(THD *thd)
{
  DBUG_ENTER("Sql_cmd_delete::prepare_inner");

  Prepare_error_tracker tracker(thd);

  SELECT_LEX *const select= lex->select_lex;
  TABLE_LIST *const table_list= select->get_table_list();

  bool apply_semijoin;

  Mem_root_array<Item_exists_subselect *> sj_candidates_local(thd->mem_root);

  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_prepare(trace, "delete_preparation");
  trace_prepare.add_select_number(select->select_number);
  Opt_trace_array trace_steps(trace, "steps");

  if (multitable)
  {
    if (select->top_join_list.elements > 0)
      propagate_nullability(&select->top_join_list, false);

    Prepared_stmt_arena_holder ps_holder(thd);
    result= new Query_result_delete(thd);
    if (result == NULL)
      DBUG_RETURN(true);            /* purecov: inspected */

    select->set_query_result(result);

    select->make_active_options(SELECT_NO_JOIN_CACHE |
                                SELECT_NO_UNLOCK,
                                OPTION_BUFFER_RESULT);
    apply_semijoin= true;
    select->set_sj_candidates(&sj_candidates_local);
  }
  else
  {
    table_list->updating= true;
    select->make_active_options(0, 0);
    apply_semijoin= false;
  }

  if (select->setup_tables(thd, table_list, false))
    DBUG_RETURN(true);            /* purecov: inspected */

  if (select->derived_table_count)
  {
    if (select->resolve_derived(thd, apply_semijoin))
      DBUG_RETURN(true);

    if (select->check_view_privileges(thd, DELETE_ACL, SELECT_ACL))
      DBUG_RETURN(true);
  }

  /*
    Deletability test is spread across several places:
    - Target table or view must be updatable (checked below)
    - A view has special requirements with respect to keys
                                          (checked in check_key_in_view)
    - Target table must not be same as one selected from
                                          (checked in unique_table)
  */

  // Check the list of tables to be deleted from
  for (TABLE_LIST *table_ref= table_list;
       table_ref;
       table_ref= table_ref->next_local)
  {
    // Skip tables that are only selected from
    if (!table_ref->updating)
      continue;

    if (!table_ref->is_updatable())
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
               table_ref->alias, "DELETE");
      DBUG_RETURN(true);
    }

    // DELETE does not allow deleting from multi-table views
    if (table_ref->is_multiple_tables())
    {
      my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
               table_ref->view_db.str, table_ref->view_name.str);
      DBUG_RETURN(true);
    }

    if (check_key_in_view(thd, table_ref, table_ref->updatable_base_table()))
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
               table_ref->alias, "DELETE");
      DBUG_RETURN(true);
    }

    // A view must be merged, and thus cannot have a TABLE
    DBUG_ASSERT(!table_ref->is_view() || table_ref->table == NULL);

    for (TABLE_LIST *tr= table_ref->updatable_base_table();
         tr;
         tr= tr->referencing_view)
    {
      tr->updating= true;
    }
  }

  // Precompute and store the row types of NATURAL/USING joins.
  if (select->leaf_table_count >= 2 &&
      setup_natural_join_row_types(thd, select->join_list, &select->context))
    DBUG_RETURN(true);

  // Enable the following code if allowing LIMIT with multi-table DELETE
  DBUG_ASSERT(sql_command_code() == SQLCOM_DELETE || select->select_limit == 0);

  lex->allow_sum_func= 0;

  ulong want_privilege_saved= thd->want_privilege;
  thd->want_privilege= SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved= thd->mark_used_columns;
  thd->mark_used_columns= MARK_COLUMNS_READ;

  if (select->setup_conds(thd))
    DBUG_RETURN(true);

  DBUG_ASSERT(select->having_cond() == NULL &&
              select->group_list.elements == 0 &&
              select->offset_limit == NULL);

  if (select->master_unit()->prepare_limit(thd, select))
    DBUG_RETURN(true);                       /* purecov: inspected */

  // check ORDER BY even if it can be ignored
  if (select->order_list.first)
  {
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    memset(&tables, 0, sizeof(tables));
    tables.table = table_list->table;
    tables.alias = table_list->alias;

    DBUG_ASSERT(!select->group_list.elements);
    if (select->setup_base_ref_items(thd))
      DBUG_RETURN(true);                     /* purecov: inspected */
    if (setup_order(thd, select->base_ref_items, &tables,
                    fields, all_fields, select->order_list.first))
      DBUG_RETURN(true);
  }

  thd->want_privilege= want_privilege_saved;
  thd->mark_used_columns= mark_used_columns_saved;

  if (select->has_ft_funcs() && setup_ftfuncs(select))
    DBUG_RETURN(true);                       /* purecov: inspected */

  /*
    Check tables to be deleted from for duplicate entries -
    must be done after conditions have been prepared.
  */
  select->exclude_from_table_unique_test= true;

  for (TABLE_LIST *table_ref= table_list;
       table_ref;
       table_ref= table_ref->next_local)
  {
    if (!table_ref->updating)
      continue;
    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    TABLE_LIST *duplicate= unique_table(table_ref->updatable_base_table(),
                                        lex->query_tables, false);
    if (duplicate)
    {
      update_non_unique_table_error(table_ref, "DELETE", duplicate);
      DBUG_RETURN(true);
    }
  }

  select->exclude_from_table_unique_test= false;

  if (select->inner_refs_list.elements && select->fix_inner_refs(thd))
    DBUG_RETURN(true);                       /* purecov: inspected */

  if (select->query_result() &&
      select->query_result()->prepare(select->fields_list, lex->unit))
    DBUG_RETURN(true);                       /* purecov: inspected */

  opt_trace_print_expanded_query(thd, select, &trace_wrapper);

  if (select->has_sj_candidates() && select->flatten_subqueries())
    DBUG_RETURN(true);

  select->set_sj_candidates(NULL);

  if (select->apply_local_transforms(thd, true))
    DBUG_RETURN(true);                       /* purecov: inspected */

  if (!multitable && select->is_empty_query())
    set_empty_query();

  DBUG_RETURN(false);
}


/**
  Execute a DELETE statement.
*/
bool Sql_cmd_delete::execute_inner(THD *thd)
{
  return multitable ?
    Sql_cmd_dml::execute_inner(thd) :
    delete_from_single_table(thd);
}


/***************************************************************************
  Delete multiple tables from join 
***************************************************************************/

extern "C" int refpos_order_cmp(const void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const uchar*)a, (const uchar*)b);
}


bool Query_result_delete::prepare(List<Item>&, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("Query_result_delete::prepare");
  unit= u;

  for (TABLE_LIST *tr= u->first_select()->leaf_tables; tr; tr= tr->next_leaf)
  {
    if (tr->updating)
    {
      // Count number of tables deleted from
      delete_table_count++;

      // Don't use KEYREAD optimization on this table
      tr->table->no_keyread= true;
    }
  }

  THD_STAGE_INFO(thd, stage_deleting_from_main_table);
  DBUG_RETURN(false);
}


/**
  Optimize for deletion from one or more tables in a multi-table DELETE

  Function is called when the join order has been determined.
  Calculate which tables can be deleted from immediately and which tables
  must be delayed. Create objects for handling of delayed deletes.
*/

bool Query_result_delete::optimize()
{
  DBUG_ENTER("Query_result_delete::optimize");

  SELECT_LEX *const select= unit->first_select();

  JOIN *const join= select->join;

  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) &&
      error_if_full_join(join))
    DBUG_RETURN(true);

  if (!(tempfiles= (Unique **) sql_calloc(sizeof(Unique *)*delete_table_count)))
    DBUG_RETURN(true);                        /* purecov: inspected */

  if (!(tables= (TABLE **) sql_calloc(sizeof(TABLE *) * delete_table_count)))
    DBUG_RETURN(true);                        /* purecov: inspected */

  bool delete_while_scanning= true;
  for (TABLE_LIST *tr= select->leaf_tables; tr; tr= tr->next_leaf)
  {
    if (!tr->updating)
      continue;
    delete_table_map|= tr->map();
    if (delete_while_scanning &&
        unique_table(tr, join->tables_list, false))
    {
      /*
        If the table being deleted from is also referenced in the query,
        defer delete so that the delete doesn't interfer with reading of this table.
      */
      delete_while_scanning= false;
    }
  }

  for (uint i= 0; i < join->primary_tables; i++)
  {
    TABLE *const table= join->best_ref[i]->table();
    const table_map map= join->best_ref[i]->table_ref->map();
    if (!(map & delete_table_map))
      continue;

    // We are going to delete from this table
    // Don't use record cache
    table->no_cache= 1;
    table->covering_keys.clear_all();
    if (table->file->has_transactions())
      transactional_table_map|= map;
    else
      non_transactional_table_map|= map;
    if (table->triggers &&
        table->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
    {
      /*
        The table has AFTER DELETE triggers that might access the subject
        table and therefore might need delete to be done immediately.
        So we turn-off the batching.
      */
      (void) table->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
    }
    if (thd->lex->is_ignore())
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    table->prepare_for_position();
    table->mark_columns_needed_for_delete(thd);
    if (thd->is_error())
      DBUG_RETURN(true);
  }
  /*
    In some cases, rows may be deleted from the first table(s) in the join order
    while performing the join operation when "delete_while_scanning" is true and
      1. deleting from one of the const tables, or
      2. deleting from the first non-const table
  */
  table_map possible_tables= join->const_table_map;                       // 1
  if (join->primary_tables > join->const_tables)
    possible_tables|= join->best_ref[join->const_tables]->table_ref->map();// 2
  if (delete_while_scanning)
    delete_immediate= delete_table_map & possible_tables;

  // Set up a Unique object for each table whose delete operation is deferred:

  Unique **tempfile= tempfiles;
  TABLE  **table_ptr= tables;
  for (uint i= 0; i < join->primary_tables; i++)
  {
    const table_map map= join->best_ref[i]->table_ref->map();

    if (!(map & delete_table_map & ~delete_immediate))
      continue;

    TABLE *const table= join->best_ref[i]->table();
    if (!(*tempfile++= new Unique(refpos_order_cmp,
                                  (void *) table->file,
                                  table->file->ref_length,
                                  thd->variables.sortbuff_size)))
      DBUG_RETURN(true);                     /* purecov: inspected */
    *(table_ptr++)= table;
  }
  DBUG_ASSERT(select == thd->lex->current_select());

  if (select->has_ft_funcs() && init_ftfuncs(thd, select))
    DBUG_RETURN(true);

  DBUG_RETURN(thd->is_fatal_error != 0);
}


void Query_result_delete::cleanup()
{
  // Cleanup only needed if result object has been prepared
  if (delete_table_count == 0)
    return;

  // Remove optimize structs for this operation.
  for (uint counter= 0; counter < delete_table_count; counter++)
  {
    if (tempfiles && tempfiles[counter])
      delete tempfiles[counter];
  }
  tempfiles= NULL;
  tables= NULL;
}


bool Query_result_delete::send_data(List<Item>&)
{
  DBUG_ENTER("Query_result_delete::send_data");

  JOIN *const join= unit->first_select()->join;

  DBUG_ASSERT(thd->lex->current_select() == unit->first_select());
  int unique_counter= 0;

  for (uint i= 0; i < join->primary_tables; i++)
  {
    const table_map map= join->qep_tab[i].table_ref->map();

    // Check whether this table is being deleted from
    if (!(map & delete_table_map))
      continue;

    const bool immediate= map & delete_immediate;

    TABLE *const table= join->qep_tab[i].table();

    DBUG_ASSERT(immediate || table == tables[unique_counter]);

    /*
      If not doing immediate deletion, increment unique_counter and assign
      "tempfile" here, so that it is available when and if it is needed.
    */
    Unique *const tempfile= immediate ? NULL : tempfiles[unique_counter++];

    // Check if using outer join and no row found, or row is already deleted
    if (table->has_null_row() || table->has_deleted_row())
      continue;

    table->file->position(table->record[0]);
    found_rows++;

    if (immediate)
    {
      // Rows from this table can be deleted immediately
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
        DBUG_RETURN(true);
      table->set_deleted_row();
      if (map & non_transactional_table_map)
        non_transactional_deleted= true;
      if (!(error=table->file->ha_delete_row(table->record[0])))
      {
        deleted_rows++;
        if (!table->file->has_transactions())
          thd->get_transaction()->mark_modified_non_trans_table(
            Transaction_ctx::STMT);
        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, FALSE))
          DBUG_RETURN(true);
      }
      else
      {
        myf error_flags= MYF(0);
        if (table->file->is_fatal_error(error))
          error_flags|= ME_FATALERROR;
        table->file->print_error(error, error_flags);

        /*
          If IGNORE option is used errors caused by ha_delete_row will
          be downgraded to warnings and don't have to stop the iteration.
        */
        if (thd->is_error())
          DBUG_RETURN(true);

        /*
          If IGNORE keyword is used, then 'error' variable will have the error
          number which is ignored. Reset the 'error' variable if IGNORE is used.
          This is necessary to call my_ok().
        */
        error= 0;
      }
    }
    else
    {
      // Save deletes in a Unique object, to be carried out later.
      error= tempfile->unique_add((char*) table->file->ref);
      if (error)
      {
        /* purecov: begin inspected */
        error= 1;
        DBUG_RETURN(true);
        /* purecov: end */
      }
    }
  }
  DBUG_RETURN(false);
}


void Query_result_delete::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("Query_result_delete::send_error");

  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));

  DBUG_VOID_RETURN;
}


/**
  Wrapper function for query cache invalidation.

  @param thd         THD pointer
  @param leaf_tables Pointer to list of tables to invalidate cache for.
                     Skip tables without "updating" state
*/

static void invalidate_delete_tables(THD *thd, TABLE_LIST *leaf_tables)
{
  for (TABLE_LIST *tl= leaf_tables; tl != NULL; tl= tl->next_leaf)
  {
    if (tl->updating)
      query_cache.invalidate_single(thd, tl, 1);
  }
}


void Query_result_delete::abort_result_set()
{
  DBUG_ENTER("Query_result_delete::abort_result_set");

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->get_transaction()->cannot_safely_rollback(
        Transaction_ctx::STMT) && deleted_rows == 0))
    DBUG_VOID_RETURN;

  /* Something already deleted so we have to invalidate cache */
  if (deleted_rows > 0)
    invalidate_delete_tables(thd, unit->first_select()->leaf_tables);

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (!delete_completed && non_transactional_deleted)
  {
    /*
      We have to execute the recorded do_deletes() and write info into the
      error log
    */
    error= 1;
    send_eof();
    DBUG_ASSERT(error_handled);
    DBUG_VOID_RETURN;
  }
  
  if (thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT))
  {
    /* 
       there is only side effects; to binlog with the error
    */
    if (mysql_bin_log.is_open())
    {
      int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
      /* possible error of writing binary log is ignored deliberately */
      (void) thd->binlog_query(THD::ROW_QUERY_TYPE,
                               thd->query().str, thd->query().length,
                               transactional_table_map != 0, FALSE, FALSE,
                               errcode);
    }
  }
  DBUG_VOID_RETURN;
}



/**
  Do delete from other tables.

  @retval 0 ok
  @retval 1 error
*/

int Query_result_delete::do_deletes()
{
  DBUG_ENTER("Query_result_delete::do_deletes");
  DBUG_ASSERT(!delete_completed);

  DBUG_ASSERT(thd->lex->current_select() == unit->first_select());
  delete_completed= true;               // Mark operation as complete
  if (found_rows == 0)
    DBUG_RETURN(0);

  for (uint counter= 0; counter < delete_table_count; counter++)
  {
    TABLE *const table= tables[counter];
    if (table == NULL)
      break;

    if (tempfiles[counter]->get(table))
      DBUG_RETURN(1);

    int local_error= do_table_deletes(table);

    if (thd->killed && !local_error)
      DBUG_RETURN(1);

    if (local_error == -1)				// End of file
      local_error = 0;

    if (local_error)
      DBUG_RETURN(local_error);
  }
  DBUG_RETURN(0);
}


/**
   Implements the inner loop of nested-loops join within multi-DELETE
   execution.

   @param table The table from which to delete.

   @return Status code

   @retval  0 All ok.
   @retval  1 Triggers or handler reported error.
   @retval -1 End of file from handler.
*/
int Query_result_delete::do_table_deletes(TABLE *table)
{
  myf error_flags= MYF(0);                      /**< Flag for fatal errors */
  int local_error= 0;
  READ_RECORD info;
  ha_rows last_deleted= deleted_rows;
  DBUG_ENTER("Query_result_delete::do_table_deletes");
  if (init_read_record(&info, thd, table, NULL, 0, 1, FALSE))
    DBUG_RETURN(1);
  /*
    Ignore any rows not found in reference tables as they may already have
    been deleted by foreign key handling
  */
  info.ignore_not_found_rows= 1;
  bool will_batch= !table->file->start_bulk_delete();
  while (!(local_error= info.read_record(&info)) && !thd->killed)
  {
    if (table->triggers &&
        table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                          TRG_ACTION_BEFORE, FALSE))
    {
      local_error= 1;
      break;
    }

    local_error= table->file->ha_delete_row(table->record[0]);
    if (local_error)
    {
      if (table->file->is_fatal_error(local_error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      /*
        If IGNORE option is used errors caused by ha_delete_row will
        be downgraded to warnings and don't have to stop the iteration.
      */
      if (thd->is_error())
        break;
    }

    /*
      Increase the reported number of deleted rows only if no error occurred
      during ha_delete_row.
      Also, don't execute the AFTER trigger if the row operation failed.
    */
    if (!local_error)
    {
      deleted_rows++;
      if (table->pos_in_table_list->map() & non_transactional_table_map)
        non_transactional_deleted= true;

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_AFTER, FALSE))
      {
        local_error= 1;
        break;
      }
    }
  }
  if (will_batch)
  {
    int tmp_error= table->file->end_bulk_delete();
    if (tmp_error && !local_error)
    {
      local_error= tmp_error;
      if (table->file->is_fatal_error(local_error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
    }
  }
  if (last_deleted != deleted_rows && !table->file->has_transactions())
    thd->get_transaction()->mark_modified_non_trans_table(
      Transaction_ctx::STMT);

  end_read_record(&info);

  DBUG_RETURN(local_error);
}

/**
  Send ok to the client

  The function has to perform all deferred deletes that have been queued up.

  @return false if success, true if error
*/

bool Query_result_delete::send_eof()
{
  THD::killed_state killed_status= THD::NOT_KILLED;
  THD_STAGE_INFO(thd, stage_deleting_from_reference_tables);

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error= do_deletes();		// returns 0 if success

  /* compute a total error to know if something failed */
  local_error= local_error || error;
  killed_status= (local_error == 0)? THD::NOT_KILLED : thd->killed;
  /* reset used flags */

  /*
    We must invalidate the query cache before binlog writing and
    ha_autocommit_...
  */
  if (deleted_rows > 0)
    invalidate_delete_tables(thd, unit->first_select()->leaf_tables);

  if ((local_error == 0) ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT))
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (local_error == 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query().str, thd->query().length,
                            transactional_table_map != 0, FALSE, FALSE,
                            errcode) &&
          !non_transactional_table_map)
      {
	local_error=1;  // Log write failed: roll back the SQL statement
      }
    }
  }
  if (local_error != 0)
    error_handled= TRUE; // to force early leave from ::send_error()

  if (!local_error)
  {
    ::my_ok(thd, deleted_rows);
  }
  return 0;
}

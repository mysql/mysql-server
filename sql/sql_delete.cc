/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "binlog.h"                   // mysql_bin_log
#include "debug_sync.h"               // DEBUG_SYNC
#include "opt_explain.h"              // Modification_plan
#include "opt_trace.h"                // Opt_trace_object
#include "records.h"                  // READ_RECORD
#include "sql_base.h"                 // open_tables_for_query
#include "sql_optimizer.h"            // optimize_cond, substitute_gc
#include "sql_resolver.h"             // setup_order
#include "sql_select.h"               // free_underlaid_joins
#include "sql_view.h"                 // check_key_in_view
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "uniques.h"                  // Unique
#include "probes_mysql.h"
#include "auth_common.h"


/**
  Implement DELETE SQL word.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool Sql_cmd_delete::mysql_delete(THD *thd, ha_rows limit)
{
  DBUG_ENTER("mysql_delete");

  myf           error_flags= MYF(0);            /**< Flag for fatal errors */
  bool          will_batch;
  int           error, loc_error;
  READ_RECORD   info;
  const bool    using_limit= limit != HA_POS_ERROR;
  ha_rows       deleted= 0;
  bool          reverse= false;
  bool          read_removal= false;
  bool          skip_record;
  bool          need_sort= false;
  bool          err= true;
  bool          transactional_table, const_cond_result, const_cond; // const

  uint usable_index= MAX_KEY;
  SELECT_LEX *const select_lex= thd->lex->select_lex;
  ORDER *order= select_lex->order_list.first;
  TABLE_LIST *const table_list= select_lex->get_table_list();
  THD::killed_state killed_status= THD::NOT_KILLED;
  THD::enum_binlog_query_type query_type= THD::ROW_QUERY_TYPE;

  select_lex->make_active_options(0, 0);

  const bool safe_update= thd->variables.option_bits & OPTION_SAFE_UPDATES;

  if (open_tables_for_query(thd, table_list, 0))
    DBUG_RETURN(TRUE);

  THD_STAGE_INFO(thd, stage_init);

  if (run_before_dml_hook(thd))
    DBUG_RETURN(true);

  if (mysql_prepare_delete(thd))
    DBUG_RETURN(TRUE);

  TABLE_LIST *const delete_table_ref= table_list->updatable_base_table();
  TABLE *const table= delete_table_ref->table;

  Item *conds;
  if (select_lex->get_optimizable_conditions(thd, &conds, NULL))
    DBUG_RETURN(TRUE);

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

  /*
    Non delete tables are pruned in SELECT_LEX::prepare,
    only the delete table needs this.
  */
  if (prune_partitions(thd, table, conds))
    DBUG_RETURN(true);
  if (table->all_partitions_pruned_away)
  {
    /* No matching records */
    if (thd->lex->describe)
    {
      /*
        Initialize plan only for regular EXPLAIN. Don't do it for EXPLAIN
        FOR CONNECTION as the plan would exist for very short period of time
        but will cost taking/releasing of a mutex, so it's not worth
        bothering with. Same for similar cases below.
      */
      Modification_plan plan(thd, MT_DELETE, table,
                             "No matching rows after partition pruning",
                             true, 0);
      err= explain_single_table_modification(thd, &plan, select_lex);
      goto exit_without_my_ok;
    }
    my_ok(thd, 0);
    DBUG_RETURN(0);
  }

  if (lock_tables(thd, table_list, thd->lex->table_count, 0))
    DBUG_RETURN(true);

  const_cond= (!conds || conds->const_item());
  if (safe_update && const_cond)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }

  const_cond_result= const_cond && (!conds || conds->val_int());
  if (thd->is_error())
  {
    /* Error evaluating val_int(). */
    DBUG_RETURN(TRUE);
  }
  /*
    We are passing HA_EXTRA_IGNORE_DUP_KEY flag here to recreate query with
    IGNORE keyword within federated storage engine. If federated engine is
    removed in the future, use of HA_EXTRA_IGNORE_DUP_KEY and
    HA_EXTRA_NO_IGNORE_DUP_KEY flag should be removed from mysql_delete(),
    Query_result_delete::initialize_tables() and
    Query_result_delete destructor.
  */
  if (thd->lex->is_ignore())
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
      ((!thd->is_current_stmt_binlog_format_row() ||   /* not ROW binlog-format */
        thd->is_current_stmt_binlog_disabled()) && /* no binlog for this command */
       !(table->triggers && table->triggers->has_delete_triggers())))
  {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->file->stats.records;

    Modification_plan plan(thd, MT_DELETE, table,
                           "Deleting all rows", false, maybe_deleted);
    if (thd->lex->describe)
    {
      err= explain_single_table_modification(thd, &plan, select_lex);
      goto exit_without_my_ok;
    }

    DBUG_PRINT("debug", ("Trying to use delete_all_rows()"));
    if (!(error=table->file->ha_delete_all_rows()))
    {
      /*
        As delete_all_rows() was used, we have to log it in statement format.
      */
      query_type= THD::STMT_QUERY_TYPE;
      error= -1;
      deleted= maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      if (table->file->is_fatal_error(error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      error=0;
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
      limit= 0;

      if (thd->lex->describe)
      {
        Modification_plan plan(thd, MT_DELETE, table,
                               "Impossible WHERE", true, 0);
        err= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }
    }
    if (conds)
    {
      conds= substitute_for_best_equal_field(conds, cond_equal, 0);
      if (conds == NULL)
      {
        err= true;
        goto exit_without_my_ok;
      }
      conds->update_used_tables();
    }
  }

  // Initialize the cost model that will be used for this table
  table->init_cost_model(thd->cost_model());

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.clear_all();
  table->quick_keys.clear_all();		// Can't use 'only index'
  table->possible_quick_keys.clear_all();

  /* Prune a second time to be able to prune on subqueries in WHERE clause. */
  if (prune_partitions(thd, table, conds))
    DBUG_RETURN(true);
  if (table->all_partitions_pruned_away)
  {
    /* No matching records */
    if (thd->lex->describe)
    {
      Modification_plan plan(thd, MT_DELETE, table,
                             "No matching rows after partition pruning",
                             true, 0);
      err= explain_single_table_modification(thd, &plan, select_lex);
      goto exit_without_my_ok;
    }
    my_ok(thd, 0);
    DBUG_RETURN(0);
  }

  error= 0;
  qep_tab.set_table(table);
  qep_tab.set_condition(conds);

  { // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(delete_table_ref);
    bool zero_rows= false; // True if it's sure that we'll find no rows
    if (limit == 0)
      zero_rows= true;
    else if (conds != NULL)
    {
      key_map keys_to_use(key_map::ALL_BITS), needed_reg_dummy;
      QUICK_SELECT_I *qck;
      zero_rows= test_quick_select(thd, keys_to_use, 0, limit, safe_update,
                                   ORDER::ORDER_NOT_RELEVANT, &qep_tab,
                                   conds, &needed_reg_dummy, &qck) < 0;
      qep_tab.set_quick(qck);
    }
    if (zero_rows)
    {
      if (thd->lex->describe && !error && !thd->is_error())
      {
        Modification_plan plan(thd, MT_DELETE, table,
                               "Impossible WHERE", true, 0);
        err= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }

      free_underlaid_joins(thd, select_lex);
      /*
         Error was already created by quick select evaluation (check_quick()).
         TODO: Add error code output parameter to Item::val_xxx() methods.
         Currently they rely on the user checking DA for
         errors when unwinding the stack after calling Item::val_xxx().
      */
      if (thd->is_error())
        DBUG_RETURN(true);
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
      free_underlaid_joins(thd, select_lex);
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      DBUG_RETURN(TRUE);
    }
  }

  if (order)
  {
    table->update_const_key_parts(conds);
    order= simple_remove_const(order, conds);

    usable_index= get_index_for_order(order, &qep_tab, limit,
                                      &need_sort, &reverse);
  }

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

    if (thd->lex->describe)
    {
      err= explain_single_table_modification(thd, &plan, select_lex);
      goto exit_without_my_ok;
    }

    if (select_lex->active_options() & OPTION_QUICK)
      (void) table->file->extra(HA_EXTRA_QUICK);

    if (need_sort)
    {
      ha_rows examined_rows, found_rows, returned_rows;

      {
        Filesort fsort(&qep_tab, order, HA_POS_ERROR);
        DBUG_ASSERT(usable_index == MAX_KEY);
        table->sort.io_cache= (IO_CACHE *) my_malloc(key_memory_TABLE_sort_io_cache,
                                                     sizeof(IO_CACHE),
                                                     MYF(MY_FAE | MY_ZEROFILL));

        if (filesort(thd, &fsort, true,
                     &examined_rows, &found_rows, &returned_rows))
        {
          err= true;
          goto exit_without_my_ok;
        }
        table->sort.found_records= returned_rows;
        thd->inc_examined_row_count(examined_rows);
        free_underlaid_joins(thd, select_lex);
        /*
          Filesort has already found and selected the rows we want to delete,
          so we don't need the where clause
        */
        qep_tab.set_quick(NULL);
        qep_tab.set_condition(NULL);
        table->file->ha_index_or_rnd_end();
      }
    }

    /* If quick select is used, initialize it before retrieving rows. */
    if (qep_tab.quick() && (error= qep_tab.quick()->reset()))
    {
      if (table->file->is_fatal_error(error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      err= true;
      goto exit_without_my_ok;
    }

    if (usable_index==MAX_KEY || qep_tab.quick())
      error= init_read_record(&info, thd, NULL, &qep_tab, 1, 1, FALSE);
    else
      error= init_read_record_idx(&info, thd, table, 1, usable_index, reverse);

    if (error)
    {
      err= true; /* purecov: inspected */
      goto exit_without_my_ok;
    }

    if (select_lex->has_ft_funcs() && init_ftfuncs(thd, select_lex))
      goto exit_without_my_ok;

    THD_STAGE_INFO(thd, stage_updating);

    if (table->triggers &&
        table->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
    {
      /*
        The table has AFTER DELETE triggers that might access to subject table
        and therefore might need delete to be done immediately. So we turn-off
        the batching.
      */
      (void) table->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
      will_batch= FALSE;
    }
    else
      will_batch= !table->file->start_bulk_delete();

    table->mark_columns_needed_for_delete();
    if (thd->is_error())
      goto exit_without_my_ok;

    if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
        !using_limit &&
        !(table->triggers && table->triggers->has_delete_triggers()) &&
        qep_tab.quick() && qep_tab.quick()->index != MAX_KEY)
      read_removal= table->check_read_removal(qep_tab.quick()->index);

    while (!(error=info.read_record(&info)) && !thd->killed &&
           ! thd->is_error())
    {
      thd->inc_examined_row_count(1);
      // thd->is_error() is tested to disallow delete row on error
      if (!qep_tab.skip_record(thd, &skip_record) && !skip_record)
      {

        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_BEFORE, FALSE))
        {
          error= 1;
          break;
        }

        if (!(error= table->file->ha_delete_row(table->record[0])))
        {
          deleted++;
          if (table->triggers &&
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
        else
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
      }
      /*
        Don't try unlocking the row if skip_record reported an error since in
        this case the transaction might have been rolled back already.
      */
      else if (!thd->is_error())
        table->file->unlock_row();  // Row failed selection, release lock on it
      else
        break;
    }

    killed_status= thd->killed;
    if (killed_status != THD::NOT_KILLED || thd->is_error())
      error= 1;					// Aborted
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
      deleted= table->file->end_read_removal();
    }
    if (thd->lex->is_ignore())
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    THD_STAGE_INFO(thd, stage_end);
    end_read_record(&info);
    if (select_lex->active_options() & OPTION_QUICK)
      (void) table->file->extra(HA_EXTRA_NORMAL);
  } // End of scope for Modification_plan

cleanup:
  DBUG_ASSERT(!thd->lex->describe);
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted)
    query_cache.invalidate_single(thd, delete_table_ref, true);

  transactional_table= table->file->has_transactions();

  if (!transactional_table && deleted > 0)
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
              !deleted ||
              thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::STMT));
  free_underlaid_joins(thd, select_lex);
  if (error < 0)
  {
    my_ok(thd, deleted);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted));
  }
  DBUG_RETURN(thd->is_error() || thd->killed);

exit_without_my_ok:
  free_underlaid_joins(thd, select_lex);
  table->set_keyread(false);
  DBUG_RETURN((err || thd->is_error() || thd->killed) ? 1 : 0);
}


/**
  Prepare items in DELETE statement

  @param thd        - thread handler

  @return false if success, true if error
*/

bool Sql_cmd_delete::mysql_prepare_delete(THD *thd)
{
  DBUG_ENTER("mysql_prepare_delete");

  List<Item> all_fields;
  SELECT_LEX *const select= thd->lex->select_lex;
  TABLE_LIST *const table_list= select->get_table_list();

  if (select->setup_tables(thd, table_list, false))
    DBUG_RETURN(true);            /* purecov: inspected */

  if (table_list->is_view() && select->resolve_derived(thd, false))
    DBUG_RETURN(true);            /* purecov: inspected */

  if (!table_list->is_updatable())
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(true);
  }

  if (table_list->is_multiple_tables())
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
             table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }

  TABLE_LIST *const delete_table_ref= table_list->updatable_base_table();

  thd->lex->allow_sum_func= 0;
  if (table_list->is_view() &&
      select->check_view_privileges(thd, DELETE_ACL, SELECT_ACL))
    DBUG_RETURN(true);

  ulong want_privilege_saved= thd->want_privilege;
  thd->want_privilege= SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved= thd->mark_used_columns;
  thd->mark_used_columns= MARK_COLUMNS_READ;

  if (select->setup_conds(thd))
    DBUG_RETURN(true);

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
    if (select->setup_ref_array(thd))
      DBUG_RETURN(true);                     /* purecov: inspected */
    if (setup_order(thd, select->ref_pointer_array, &tables,
                    fields, all_fields, select->order_list.first))
      DBUG_RETURN(true);
  }

  thd->want_privilege= want_privilege_saved;
  thd->mark_used_columns= mark_used_columns_saved;

  if (setup_ftfuncs(select))
    DBUG_RETURN(true);                       /* purecov: inspected */

  // check_key_in_view() may send an SQL note, but we only want it once.
  if (select->first_execution &&
      check_key_in_view(thd, table_list, delete_table_ref))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(true);
  }

  TABLE_LIST *const duplicate= unique_table(thd, delete_table_ref,
                                            table_list->next_global, false);
  if (duplicate)
  {
    update_non_unique_table_error(table_list, "DELETE", duplicate);
    DBUG_RETURN(true);
  }

  if (select->inner_refs_list.elements && select->fix_inner_refs(thd))
    DBUG_RETURN(true);                       /* purecov: inspected */

  if (select->apply_local_transforms(thd, false))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/***************************************************************************
  Delete multiple tables from join 
***************************************************************************/

#define MEM_STRIP_BUF_SIZE current_thd->variables.sortbuff_size

extern "C" int refpos_order_cmp(const void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const uchar*)a, (const uchar*)b);
}

/**
  Make delete specific preparation and checks after opening tables.

  @param      thd          Thread context.
  @param[out] table_count  Number of tables to be deleted from.

  @retval false - success.
  @retval true  - error.
*/

int Sql_cmd_delete_multi::mysql_multi_delete_prepare(THD *thd,
                                                     uint *table_count)
{
  DBUG_ENTER("mysql_multi_delete_prepare");

  Prepare_error_tracker tracker(thd);

  LEX        *const lex= thd->lex;
  SELECT_LEX *const select= lex->select_lex;

  /*
    setup_tables() need for VIEWs. SELECT_LEX::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE SELECT_LEX
  */
  if (select->setup_tables(thd, lex->query_tables, false))
    DBUG_RETURN(true);               /* purecov: inspected */

  if (select->derived_table_count)
  {
    if (select->resolve_derived(thd, true))
      DBUG_RETURN(true);

    if (select->check_view_privileges(thd, DELETE_ACL, SELECT_ACL))
      DBUG_RETURN(true);
  }
  *table_count= 0;

  // Check the list of tables to be deleted from
  for (TABLE_LIST *delete_target= lex->auxiliary_table_list.first;
       delete_target;
       delete_target= delete_target->next_local)
  {
    ++(*table_count);

    TABLE_LIST *const table_ref= delete_target->correspondent_table;

    if (!table_ref->is_updatable())
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
               delete_target->table_name, "DELETE");
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
               delete_target->table_name, "DELETE");
      DBUG_RETURN(true);
    }

    // A view must be merged, and thus cannot have a TABLE 
    DBUG_ASSERT(!table_ref->is_view() || table_ref->table == NULL);

    // Enable the following code if allowing LIMIT with multi-table DELETE
    DBUG_ASSERT(select->select_limit == 0);
  }

  DBUG_RETURN(false);
}


Query_result_delete::Query_result_delete(TABLE_LIST *dt,
                                         uint num_of_tables_arg)
  : delete_tables(dt), tempfiles(NULL), tables(NULL), deleted(0), found(0),
    num_of_tables(num_of_tables_arg), error(0),
    delete_table_map(0), delete_immediate(0),
    transactional_table_map(0), non_transactional_table_map(0),
    do_delete(0), non_transactional_deleted(false), error_handled(false)
{
}


int Query_result_delete::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("Query_result_delete::prepare");
  unit= u;
  do_delete= true;
  /*
    Multi-delete can't be constructed over-union => we always have
    single SELECT on top and have to check underlying SELECTs of it
  */
  SELECT_LEX *const select= unit->first_select();
  select->exclude_from_table_unique_test= true;

  for (TABLE_LIST *walk= delete_tables; walk; walk= walk->next_local)
  {
    if (walk->correspondent_table == NULL)
      continue;

    TABLE_LIST *ref= walk->correspondent_table->updatable_base_table();

    // Don't use KEYREAD optimization on this table
    ref->table->no_keyread= true;

    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    TABLE_LIST *duplicate= unique_table(thd, ref,
                                        thd->lex->query_tables, false);
    if (duplicate)
    {
      update_non_unique_table_error(walk->correspondent_table,
                                    "DELETE", duplicate);
      DBUG_RETURN(1);
    }
  }

  /*
    Reset the exclude flag to false so it doesn't interfer
    with further calls to unique_table
  */
  select->exclude_from_table_unique_test= false;

  THD_STAGE_INFO(thd, stage_deleting_from_main_table);
  DBUG_RETURN(0);
}


bool Query_result_delete::initialize_tables(JOIN *join)
{
  DBUG_ENTER("Query_result_delete::initialize_tables");
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  SELECT_LEX *const select= unit->first_select();
  DBUG_ASSERT(join == select->join);

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) &&
      error_if_full_join(join))
    DBUG_RETURN(true);

  if (!(tempfiles= (Unique **) sql_calloc(sizeof(Unique *) * num_of_tables)))
    DBUG_RETURN(true);                        /* purecov: inspected */

  if (!(tables= (TABLE **) sql_calloc(sizeof(TABLE *) * num_of_tables)))
    DBUG_RETURN(true);                        /* purecov: inspected */

  bool delete_while_scanning= true;
  for (TABLE_LIST *walk= delete_tables; walk; walk= walk->next_local)
  {
    TABLE_LIST *const ref= walk->correspondent_table->updatable_base_table();
    delete_table_map|= ref->map();
    if (delete_while_scanning &&
        unique_table(thd, ref, join->tables_list, false))
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
    table->mark_columns_needed_for_delete();
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
                                  MEM_STRIP_BUF_SIZE)))
      DBUG_RETURN(true);                     /* purecov: inspected */
    *(table_ptr++)= table;
  }
  DBUG_ASSERT(select == thd->lex->current_select());

  if (select->has_ft_funcs() && init_ftfuncs(thd, select))
    DBUG_RETURN(true);

  DBUG_RETURN(thd->is_fatal_error != 0);
}


Query_result_delete::~Query_result_delete()
{
  for (TABLE_LIST *tbl_ref= delete_tables; tbl_ref;
       tbl_ref= tbl_ref->next_local)
  {
    TABLE *table= tbl_ref->correspondent_table->updatable_base_table()->table;
    if (thd->lex->is_ignore())
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  }

  for (uint counter= 0; counter < num_of_tables; counter++)
  {
    if (tempfiles && tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool Query_result_delete::send_data(List<Item> &values)
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
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    found++;

    if (immediate)
    {
      // Rows from this table can be deleted immediately
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
        DBUG_RETURN(true);
      table->status|= STATUS_DELETED;
      if (map & non_transactional_table_map)
        non_transactional_deleted= true;
      if (!(error=table->file->ha_delete_row(table->record[0])))
      {
        deleted++;
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

  @param thd           THD pointer
  @param delete_tables Pointer to list of tables to invalidate cache for.
*/

static void invalidate_delete_tables(THD *thd, TABLE_LIST *delete_tables)
{
  for (TABLE_LIST *tl= delete_tables; tl != NULL; tl= tl->next_local)
  {
    query_cache.invalidate_single(thd,
                          tl->correspondent_table->updatable_base_table(), 1);
  }
}


void Query_result_delete::abort_result_set()
{
  DBUG_ENTER("Query_result_delete::abort_result_set");

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->get_transaction()->cannot_safely_rollback(
        Transaction_ctx::STMT) && !deleted))
    DBUG_VOID_RETURN;

  /* Something already deleted so we have to invalidate cache */
  if (deleted)
    invalidate_delete_tables(thd, delete_tables);

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (do_delete && non_transactional_deleted)
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

  @todo Is there any reason not use the normal nested-loops join? If not, and
  there is no documentation supporting it, this method and callee should be
  removed and there should be hooks within normal execution.
*/

int Query_result_delete::do_deletes()
{
  DBUG_ENTER("Query_result_delete::do_deletes");
  DBUG_ASSERT(do_delete);

  DBUG_ASSERT(thd->lex->current_select() == unit->first_select());
  do_delete= false;                                 // Mark called
  if (!found)
    DBUG_RETURN(0);

  for (uint counter= 0; counter < num_of_tables; counter++)
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
  ha_rows last_deleted= deleted;
  DBUG_ENTER("do_deletes_for_table");
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
      deleted++;
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
  if (last_deleted != deleted && !table->file->has_transactions())
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
  THD_STAGE_INFO(thd, stage_end);

  /*
    We must invalidate the query cache before binlog writing and
    ha_autocommit_...
  */
  if (deleted)
    invalidate_delete_tables(thd, delete_tables);

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
    ::my_ok(thd, deleted);
  }
  return 0;
}


bool Sql_cmd_delete::execute(THD *thd)
{
  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_DELETE);

  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= lex->unit;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  if (delete_precheck(thd, all_tables))
    return true;
  DBUG_ASSERT(select_lex->offset_limit == 0);
  unit->set_limit(select_lex);

  /* Push ignore / strict error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  MYSQL_DELETE_START(const_cast<char*>(thd->query().str));
  bool res = mysql_delete(thd, unit->select_limit_cnt);
  MYSQL_DELETE_DONE(res, (ulong) thd->get_row_count_func());

  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  return res;
}


bool Sql_cmd_delete_multi::execute(THD *thd)
{
  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_DELETE_MULTI);

  bool res= false;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
  uint del_table_count;
  Query_result_delete *del_result;

  if (multi_delete_precheck(thd, all_tables))
    return true;

  /* condition will be TRUE on SP re-excuting */
  if (select_lex->item_list.elements != 0)
    select_lex->item_list.empty();
  if (add_item_to_list(thd, new Item_null()))
    return true;

  THD_STAGE_INFO(thd, stage_init);
  if ((res= open_tables_for_query(thd, all_tables, 0)))
    return true;

  if (run_before_dml_hook(thd))
    return true;

  MYSQL_MULTI_DELETE_START(const_cast<char*>(thd->query().str));
  if (mysql_multi_delete_prepare(thd, &del_table_count))
  {
    MYSQL_MULTI_DELETE_DONE(1, 0);
    return true;
  }

  if (!thd->is_fatal_error &&
      (del_result= new Query_result_delete(aux_tables, del_table_count)))
  {
    DBUG_ASSERT(select_lex->having_cond() == NULL &&
                !select_lex->order_list.elements &&
                !select_lex->group_list.elements);

    Ignore_error_handler ignore_handler;
    Strict_error_handler strict_handler;
    if (thd->lex->is_ignore())
      thd->push_internal_handler(&ignore_handler);
    else if (thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);

    res= handle_query(thd, lex, del_result,
                      SELECT_NO_JOIN_CACHE |
                      SELECT_NO_UNLOCK |
                      OPTION_SETUP_TABLES_DONE,
                      OPTION_BUFFER_RESULT);

    if (thd->lex->is_ignore() || thd->is_strict_mode())
      thd->pop_internal_handler();

    if (res)
      del_result->abort_result_set();

    MYSQL_MULTI_DELETE_DONE(res, del_result->num_deleted());
    delete del_result;
  }
  else
  {
    res= true;                                // Error
    MYSQL_MULTI_DELETE_DONE(1, 0);
  }

  return res;
}

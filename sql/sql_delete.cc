/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "unireg.h"
#include "sql_delete.h"
#include "sql_cache.h"                          // query_cache_*
#include "sql_base.h"                           // open_temprary_table
#include "sql_table.h"                         // build_table_filename
#include "sql_view.h"             // check_key_in_view, mysql_frm_type
#include "sql_acl.h"              // *_ACL
#include "filesort.h"             // filesort
#include "sql_select.h"
#include "opt_trace.h"                          // Opt_trace_object
#include "opt_explain.h"
#include "records.h"                            // init_read_record,
                                                // end_read_record
#include "sql_optimizer.h"                      // remove_eq_conds
#include "sql_resolver.h"                       // setup_order, fix_inner_refs

/**
  Implement DELETE SQL word.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_delete(THD *thd, TABLE_LIST *table_list, Item *conds,
                  SQL_I_List<ORDER> *order_list, ha_rows limit, ulonglong options)
{
  bool          will_batch;
  int		error, loc_error;
  TABLE		*table;
  SQL_SELECT	*select=0;
  READ_RECORD	info;
  bool          using_limit=limit != HA_POS_ERROR;
  bool		transactional_table, safe_update, const_cond;
  bool          const_cond_result;
  ha_rows	deleted= 0;
  bool          reverse= FALSE;
  bool          read_removal= false;
  bool          skip_record;
  bool          need_sort= FALSE;
  bool          err= true;
  ORDER *order= (ORDER *) ((order_list && order_list->elements) ?
                           order_list->first : NULL);
  uint usable_index= MAX_KEY;
  SELECT_LEX   *select_lex= &thd->lex->select_lex;
  THD::killed_state killed_status= THD::NOT_KILLED;
  THD::enum_binlog_query_type query_type= THD::ROW_QUERY_TYPE;
  DBUG_ENTER("mysql_delete");

  if (open_normal_and_derived_tables(thd, table_list, 0))
    DBUG_RETURN(TRUE);

  if (!(table= table_list->table))
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
	     table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }
  THD_STAGE_INFO(thd, stage_init);
  table->map=1;

  if (mysql_prepare_delete(thd, table_list, &conds))
    DBUG_RETURN(TRUE);

  /* check ORDER BY even if it can be ignored */
  if (order)
  {
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    memset(&tables, 0, sizeof(tables));
    tables.table = table;
    tables.alias = table_list->alias;

      if (select_lex->setup_ref_array(thd, order_list->elements) ||
	  setup_order(thd, select_lex->ref_pointer_array, &tables,
                    fields, all_fields, order))
    {
      free_underlaid_joins(thd, &thd->lex->select_lex);
      DBUG_RETURN(TRUE);
    }
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /*
    Non delete tables are pruned in JOIN::prepare,
    only the delete table needs this.
  */
  if (prune_partitions(thd, table, conds))
    DBUG_RETURN(true);
  if (table->all_partitions_pruned_away)
    goto exit_all_parts_pruned_away;
#endif

  if (lock_tables(thd, table_list, thd->lex->table_count, 0))
    DBUG_RETURN(true);

  const_cond= (!conds || conds->const_item());
  safe_update=test(thd->variables.option_bits & OPTION_SAFE_UPDATES);
  if (safe_update && const_cond)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }

  select_lex->no_error= thd->lex->ignore;

  const_cond_result= const_cond && (!conds || conds->val_int());
  if (thd->is_error())
  {
    /* Error evaluating val_int(). */
    DBUG_RETURN(TRUE);
  }

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
      - We should not be binlogging this statement in row-based, and
      - there should be no delete triggers associated with the table.
  */
  if (!using_limit && const_cond_result &&
      !(specialflag & SPECIAL_NO_NEW_FUNC) &&
       (!thd->is_current_stmt_binlog_format_row() &&
        !(table->triggers && table->triggers->has_delete_triggers())))
  {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->file->stats.records;

    if (thd->lex->describe)
    {
      err= explain_no_table(thd, "Deleting all rows", maybe_deleted);
      goto exit_without_my_ok;
    }

    DBUG_PRINT("debug", ("Trying to use delete_all_rows()"));
    if (!(error=table->file->ha_delete_all_rows()))
    {
      /*
        If delete_all_rows() is used, it is not possible to log the
        query in row format, so we have to log it in statement format.
      */
      query_type= THD::STMT_QUERY_TYPE;
      error= -1;
      deleted= maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      table->file->print_error(error,MYF(0));
      error=0;
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }

  if (conds)
  {
    COND_EQUAL *cond_equal= NULL;
    Item::cond_result result;

    conds= optimize_cond(thd, conds, &cond_equal, select_lex->join_list, 
                         true, &result);
    if (result == Item::COND_FALSE)             // Impossible where
    {
      limit= 0;

      if (thd->lex->describe)
      {
        err= explain_no_table(thd, "Impossible WHERE");
        goto exit_without_my_ok;
      }
    }
    if (conds)
    {
      conds= substitute_for_best_equal_field(conds, cond_equal, 0);
      conds->update_used_tables();
    }
  }

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.clear_all();
  table->quick_keys.clear_all();		// Can't use 'only index'

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /* Prune a second time to be able to prune on subqueries in WHERE clause. */
  if (prune_partitions(thd, table, conds))
    DBUG_RETURN(true);
  if (table->all_partitions_pruned_away)
    goto exit_all_parts_pruned_away;
#endif

  select=make_select(table, 0, 0, conds, 0, &error);
  if (error)
    DBUG_RETURN(TRUE);

  { // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(table);

    if ((select && select->check_quick(thd, safe_update, limit)) || !limit)
    {
      if (thd->lex->describe && !error && !thd->is_error())
      {
        err= explain_no_table(thd, "Impossible WHERE");
        goto exit_without_my_ok;
      }
      delete select;
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
      delete select;
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

    usable_index= get_index_for_order(order, table, select, limit,
                                      &need_sort, &reverse);
  }

  if (thd->lex->describe)
  {
    err= explain_single_table_modification(thd, table, select, usable_index,
                                           limit, false, need_sort, false);
    goto exit_without_my_ok;
  }

  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (need_sort)
  {
    ha_rows examined_rows;
    ha_rows found_rows;
    
    {
      Filesort fsort(order, HA_POS_ERROR, select);
      DBUG_ASSERT(usable_index == MAX_KEY);
      table->sort.io_cache= (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                                   MYF(MY_FAE | MY_ZEROFILL));

      if ((table->sort.found_records= filesort(thd, table, &fsort, true,
                                               &examined_rows, &found_rows))
	  == HA_POS_ERROR)
      {
        goto exit_without_my_ok;
      }
      thd->inc_examined_row_count(examined_rows);
      /*
        Filesort has already found and selected the rows we want to delete,
        so we don't need the where clause
      */
      delete select;
      free_underlaid_joins(thd, select_lex);
      select= 0;
    }
  }

  /* If quick select is used, initialize it before retrieving rows. */
  if (select && select->quick && select->quick->reset())
    goto exit_without_my_ok;
  if (usable_index==MAX_KEY || (select && select->quick))
    error= init_read_record(&info, thd, table, select, 1, 1, FALSE);
  else
    error= init_read_record_idx(&info, thd, table, 1, usable_index, reverse);

  if (error)
    goto exit_without_my_ok;
  init_ftfuncs(thd, select_lex, 1);
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

  if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
      !using_limit &&
      select && select->quick && select->quick->index != MAX_KEY)
    read_removal= table->check_read_removal(select->quick->index);

  while (!(error=info.read_record(&info)) && !thd->killed &&
	 ! thd->is_error())
  {
    thd->inc_examined_row_count(1);
    // thd->is_error() is tested to disallow delete row on error
    if (!select || (!select->skip_record(thd, &skip_record) && !skip_record))
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
	table->file->print_error(error,MYF(0));
	/*
	  In < 4.0.14 we set the error number to 0 here, but that
	  was not sensible, because then MySQL would not roll back the
	  failed DELETE, and also wrote it to the binlog. For MyISAM
	  tables a DELETE probably never should fail (?), but for
	  InnoDB it can fail in a FOREIGN KEY error or an
	  out-of-tablespace error.
	*/
 	error= 1;
	break;
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
    if (error != 1)
      table->file->print_error(loc_error,MYF(0));
    error=1;
  }
  if (read_removal)
  {
    /* Only handler knows how many records were really written */
    deleted= table->file->end_read_removal();
  }
  THD_STAGE_INFO(thd, stage_end);
  end_read_record(&info);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

cleanup:
  DBUG_ASSERT(!thd->lex->describe);
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted)
  {
    query_cache_invalidate3(thd, table_list, 1);
  }

  delete select;
  select= NULL;
  transactional_table= table->file->has_transactions();

  if (!transactional_table && deleted > 0)
    thd->transaction.stmt.mark_modified_non_trans_table();
  
  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || thd->transaction.stmt.cannot_safely_rollback())
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

      /*
        [binlog]: If 'handler::delete_all_rows()' was called and the
        storage engine does not inject the rows itself, we replicate
        statement-based; otherwise, 'ha_delete_row()' was used to
        delete specific rows which we might log row-based.
      */
      int log_result= thd->binlog_query(query_type,
                                        thd->query(), thd->query_length(),
                                        transactional_table, FALSE, FALSE,
                                        errcode);

      if (log_result)
      {
	error=1;
      }
    }
  }
  DBUG_ASSERT(transactional_table || !deleted || thd->transaction.stmt.cannot_safely_rollback());
  free_underlaid_joins(thd, select_lex);
  if (error < 0 || 
      (thd->lex->ignore && !thd->is_error() && !thd->is_fatal_error))
  {
    my_ok(thd, deleted);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted));
  }
  DBUG_RETURN(thd->is_error() || thd->killed);

#ifdef WITH_PARTITION_STORAGE_ENGINE
exit_all_parts_pruned_away:
  /* No matching records */
  if (!thd->lex->describe)
  {
    my_ok(thd, 0);
    DBUG_RETURN(0);
  }
  err= explain_no_table(thd, "No matching rows after partition pruning");
#endif

exit_without_my_ok:
  delete select;
  free_underlaid_joins(thd, select_lex);
  table->set_keyread(false);
  DBUG_RETURN((err || thd->is_error() || thd->killed) ? 1 : 0);
}


/*
  Prepare items in DELETE statement

  SYNOPSIS
    mysql_prepare_delete()
    thd			- thread handler
    table_list		- global/local table list
    conds		- conditions

  RETURN VALUE
    FALSE OK
    TRUE  error
*/
int mysql_prepare_delete(THD *thd, TABLE_LIST *table_list, Item **conds)
{
  Item *fake_conds= 0;
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  DBUG_ENTER("mysql_prepare_delete");
  List<Item> all_fields;

  thd->lex->allow_sum_func= 0;
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list, 
                                    &select_lex->leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      setup_ftfuncs(select_lex))
    DBUG_RETURN(TRUE);
  if (!table_list->updatable || check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(TRUE);
  }
  {
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 0)))
    {
      update_non_unique_table_error(table_list, "DELETE", duplicate);
      DBUG_RETURN(TRUE);
    }
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(thd, all_fields, select_lex, select_lex->ref_pointer_array))
    DBUG_RETURN(TRUE);

  select_lex->fix_prepare_information(thd, conds, &fake_conds);
  DBUG_RETURN(FALSE);
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

int mysql_multi_delete_prepare(THD *thd, uint *table_count)
{
  LEX *lex= thd->lex;
  TABLE_LIST *aux_tables= lex->auxiliary_table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("mysql_multi_delete_prepare");

  /*
    setup_tables() need for VIEWs. JOIN::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE SELECT_LEX
  */
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    lex->query_tables,
                                    &lex->select_lex.leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL))
    DBUG_RETURN(TRUE);

  *table_count= 0;

  /*
    Multi-delete can't be constructed over-union => we always have
    single SELECT on top and have to check underlying SELECTs of it
  */
  lex->select_lex.exclude_from_table_unique_test= TRUE;
  /* Fix tables-to-be-deleted-from list to point at opened tables */
  for (target_tbl= (TABLE_LIST*) aux_tables;
       target_tbl;
       target_tbl= target_tbl->next_local)
  {
    ++(*table_count);

    if (!(target_tbl->table= target_tbl->correspondent_table->table))
    {
      DBUG_ASSERT(target_tbl->correspondent_table->view &&
                  target_tbl->correspondent_table->multitable_view);
      my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
               target_tbl->correspondent_table->view_db.str,
               target_tbl->correspondent_table->view_name.str);
      DBUG_RETURN(TRUE);
    }

    if (!target_tbl->correspondent_table->updatable ||
        check_key_in_view(thd, target_tbl->correspondent_table))
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
               target_tbl->table_name, "DELETE");
      DBUG_RETURN(TRUE);
    }
    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    {
      TABLE_LIST *duplicate;
      if ((duplicate= unique_table(thd, target_tbl->correspondent_table,
                                   lex->query_tables, 0)))
      {
        update_non_unique_table_error(target_tbl->correspondent_table,
                                      "DELETE", duplicate);
        DBUG_RETURN(TRUE);
      }
    }
  }
  /*
    Reset the exclude flag to false so it doesn't interfare
    with further calls to unique_table
  */
  lex->select_lex.exclude_from_table_unique_test= FALSE;
  DBUG_RETURN(FALSE);
}


multi_delete::multi_delete(TABLE_LIST *dt, uint num_of_tables_arg)
  : delete_tables(dt), deleted(0), found(0),
    num_of_tables(num_of_tables_arg), error(0),
    do_delete(0), transactional_tables(0), normal_tables(0), error_handled(0)
{
  tempfiles= (Unique **) sql_calloc(sizeof(Unique *) * num_of_tables);
}


int
multi_delete::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("multi_delete::prepare");
  unit= u;
  do_delete= 1;
  THD_STAGE_INFO(thd, stage_deleting_from_main_table);
  DBUG_RETURN(0);
}


bool
multi_delete::initialize_tables(JOIN *join)
{
  TABLE_LIST *walk;
  Unique **tempfiles_ptr;
  DBUG_ENTER("initialize_tables");

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    DBUG_RETURN(1);

  table_map tables_to_delete_from=0;
  delete_while_scanning= 1;
  for (walk= delete_tables; walk; walk= walk->next_local)
  {
    tables_to_delete_from|= walk->table->map;
    if (delete_while_scanning &&
        unique_table(thd, walk, join->tables_list, false))
    {
      /*
        If the table we are going to delete from appears
        in join, we need to defer delete. So the delete
        doesn't interfers with the scaning of results.
      */
      delete_while_scanning= 0;
    }
  }


  walk= delete_tables;
  for (uint i= 0; i < join->primary_tables; i++)
  {
    JOIN_TAB *const tab= join->join_tab + i;
    if (tab->table->map & tables_to_delete_from)
    {
      /* We are going to delete from this table */
      TABLE *tbl=walk->table=tab->table;
      walk= walk->next_local;
      /* Don't use KEYREAD optimization on this table */
      tbl->no_keyread=1;
      /* Don't use record cache */
      tbl->no_cache= 1;
      tbl->covering_keys.clear_all();
      if (tbl->file->has_transactions())
	transactional_tables= 1;
      else
	normal_tables= 1;
      if (tbl->triggers &&
          tbl->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
      {
	/*
          The table has AFTER DELETE triggers that might access to subject 
          table and therefore might need delete to be done immediately. 
          So we turn-off the batching.
        */
	(void) tbl->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
      }
      tbl->prepare_for_position();
      tbl->mark_columns_needed_for_delete();
    }
    else if ((tab->type != JT_SYSTEM && tab->type != JT_CONST) &&
             walk == delete_tables)
    {
      /*
        We are not deleting from the table we are scanning. In this
        case send_data() shouldn't delete any rows a we may touch
        the rows in the deleted table many times
      */
      delete_while_scanning= 0;
    }
  }
  walk= delete_tables;
  tempfiles_ptr= tempfiles;
  if (delete_while_scanning)
  {
    table_being_deleted= delete_tables;
    walk= walk->next_local;
  }
  for (;walk ;walk= walk->next_local)
  {
    TABLE *table=walk->table;
    *tempfiles_ptr++= new Unique (refpos_order_cmp,
				  (void *) table->file,
				  table->file->ref_length,
				  MEM_STRIP_BUF_SIZE);
  }
  init_ftfuncs(thd, thd->lex->current_select, 1);
  DBUG_RETURN(thd->is_fatal_error != 0);
}


multi_delete::~multi_delete()
{
  for (table_being_deleted= delete_tables;
       table_being_deleted;
       table_being_deleted= table_being_deleted->next_local)
  {
    TABLE *table= table_being_deleted->table;
    table->no_keyread=0;
  }

  for (uint counter= 0; counter < num_of_tables; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter= delete_while_scanning ? -1 : 0;
  TABLE_LIST *del_table;
  DBUG_ENTER("multi_delete::send_data");

  bool ignore= thd->lex->current_select->no_error;

  for (del_table= delete_tables;
       del_table;
       del_table= del_table->next_local, secure_counter++)
  {
    TABLE *table= del_table->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    found++;

    if (secure_counter < 0)
    {
      /* We are scanning the current table */
      DBUG_ASSERT(del_table == table_being_deleted);
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
        DBUG_RETURN(1);
      table->status|= STATUS_DELETED;
      if (!(error=table->file->ha_delete_row(table->record[0])))
      {
        deleted++;
        if (!table->file->has_transactions())
          thd->transaction.stmt.mark_modified_non_trans_table();
        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, FALSE))
          DBUG_RETURN(1);
      }
      else if (!ignore)
      {
        /*
          If the IGNORE option is used errors caused by ha_delete_row don't
          have to stop the iteration.
        */
        table->file->print_error(error,MYF(0));
        DBUG_RETURN(1);
      }
    }
    else
    {
      error=tempfiles[secure_counter]->unique_add((char*) table->file->ref);
      if (error)
      {
	error= 1;                               // Fatal error
	DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
}


void multi_delete::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("multi_delete::send_error");

  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));

  DBUG_VOID_RETURN;
}


void multi_delete::abort_result_set()
{
  DBUG_ENTER("multi_delete::abort_result_set");

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->transaction.stmt.cannot_safely_rollback() && !deleted))
    DBUG_VOID_RETURN;

  /* Something already deleted so we have to invalidate cache */
  if (deleted)
    query_cache_invalidate3(thd, delete_tables, 1);

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (do_delete && normal_tables &&
      (table_being_deleted != delete_tables ||
       !table_being_deleted->table->file->has_transactions()))
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
  
  if (thd->transaction.stmt.cannot_safely_rollback())
  {
    /* 
       there is only side effects; to binlog with the error
    */
    if (mysql_bin_log.is_open())
    {
      int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
      /* possible error of writing binary log is ignored deliberately */
      (void) thd->binlog_query(THD::ROW_QUERY_TYPE,
                               thd->query(), thd->query_length(),
                               transactional_tables, FALSE, FALSE, errcode);
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

int multi_delete::do_deletes()
{
  DBUG_ENTER("do_deletes");
  DBUG_ASSERT(do_delete);

  do_delete= 0;                                 // Mark called
  if (!found)
    DBUG_RETURN(0);

  table_being_deleted= (delete_while_scanning ? delete_tables->next_local :
                        delete_tables);
 
  for (uint counter= 0; table_being_deleted;
       table_being_deleted= table_being_deleted->next_local, counter++)
  { 
    TABLE *table = table_being_deleted->table;
    if (tempfiles[counter]->get(table))
      DBUG_RETURN(1);

    int local_error= 
      do_table_deletes(table, thd->lex->current_select->no_error);

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

   @param ignore If used, all non fatal errors will be translated
   to warnings and we should not break the row-by-row iteration.

   @return Status code

   @retval  0 All ok.
   @retval  1 Triggers or handler reported error.
   @retval -1 End of file from handler.
*/
int multi_delete::do_table_deletes(TABLE *table, bool ignore)
{
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
    if (local_error && !ignore)
    {
      table->file->print_error(local_error, MYF(0));
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
      table->file->print_error(local_error, MYF(0));
    }
  }
  if (last_deleted != deleted && !table->file->has_transactions())
    thd->transaction.stmt.mark_modified_non_trans_table();

  end_read_record(&info);

  DBUG_RETURN(local_error);
}

/*
  Send ok to the client

  return:  0 sucess
	   1 error
*/

bool multi_delete::send_eof()
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
  {
    query_cache_invalidate3(thd, delete_tables, 1);
  }
  if ((local_error == 0) || thd->transaction.stmt.cannot_safely_rollback())
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (local_error == 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query(), thd->query_length(),
                            transactional_tables, FALSE, FALSE, errcode) &&
          !normal_tables)
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


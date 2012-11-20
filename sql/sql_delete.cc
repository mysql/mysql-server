/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  Delete of records and truncate of tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/

#include "mysql_priv.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_handler.h"

/**
  Implement DELETE SQL word.

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_delete(THD *thd, TABLE_LIST *table_list, COND *conds,
                  SQL_I_List<ORDER> *order, ha_rows limit, ulonglong options,
                  bool reset_auto_increment)
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
  bool          triggers_applicable;
  uint usable_index= MAX_KEY;
  SELECT_LEX   *select_lex= &thd->lex->select_lex;
  killed_state killed_status= NOT_KILLED;
  DBUG_ENTER("mysql_delete");
  bool save_binlog_row_based;

  THD::enum_binlog_query_type query_type=
    thd->lex->sql_command == SQLCOM_TRUNCATE ?
    THD::STMT_QUERY_TYPE :
    THD::ROW_QUERY_TYPE;

  if (open_and_lock_tables(thd, table_list))
    DBUG_RETURN(TRUE);

  if (mysql_handle_list_of_derived(thd->lex, table_list, DT_MERGE_FOR_INSERT))
    DBUG_RETURN(TRUE);
  if (mysql_handle_list_of_derived(thd->lex, table_list, DT_PREPARE))
    DBUG_RETURN(TRUE);

  if (!table_list->single_table_updatable())
  {
     my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
     DBUG_RETURN(TRUE);
  }
  if (!(table= table_list->table) || !table->created)
  {
      my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
	       table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }
  thd_proc_info(thd, "init");
  table->map=1;

  if (mysql_prepare_delete(thd, table_list, &conds))
    DBUG_RETURN(TRUE);

  if (thd->lex->current_select->first_cond_optimization)
  {
    thd->lex->current_select->save_leaf_tables(thd);
    thd->lex->current_select->first_cond_optimization= 0;
  }
  /* check ORDER BY even if it can be ignored */
  if (order && order->elements)
  {
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;
    tables.alias = table_list->alias;

      if (select_lex->setup_ref_array(thd, order->elements) ||
	  setup_order(thd, select_lex->ref_pointer_array, &tables,
                    fields, all_fields, order->first))
    {
      delete select;
      free_underlaid_joins(thd, &thd->lex->select_lex);
      DBUG_RETURN(TRUE);
    }
  }

  /* Apply the IN=>EXISTS transformation to all subqueries and optimize them. */
  if (select_lex->optimize_unflattened_subqueries())
    DBUG_RETURN(TRUE);

  const_cond= (!conds || conds->const_item());
  safe_update=test(thd->options & OPTION_SAFE_UPDATES);
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
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.

    We implement fast TRUNCATE for InnoDB even if triggers are
    present.  TRUNCATE ignores triggers.

    We can use delete_all_rows() if and only if:
    - We allow new functions (not using option --skip-new), and are
      not in safe mode (not using option --safe-mode)
    - There is no limit clause
    - The condition is constant
    - If there is a condition, then it it produces a non-zero value
    - If the current command is DELETE FROM with no where clause
      (i.e., not TRUNCATE) then:
      - We should not be binlogging this statement row-based, and
      - there should be no delete triggers associated with the table.
  */
  if (!using_limit && const_cond_result &&
      (thd->lex->sql_command == SQLCOM_TRUNCATE ||
       (!thd->current_stmt_binlog_row_based &&
        !(table->triggers && table->triggers->has_delete_triggers()))))
  {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted= table->file->stats.records;
    DBUG_PRINT("debug", ("Trying to use delete_all_rows()"));
    if (!(error=table->file->ha_delete_all_rows()))
    {
      /*
        If delete_all_rows() is used, it is not possible to log the
        query in row format, so we have to log it in statement format.
      */
      query_type= THD::STMT_QUERY_TYPE;
      error= -1;				// ok
      deleted= maybe_deleted;
      save_binlog_row_based= thd->current_stmt_binlog_row_based;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      table->file->print_error(error,MYF(0));
      error=0;
      save_binlog_row_based= thd->current_stmt_binlog_row_based;
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }
  if (conds)
  {
    Item::cond_result result;
    conds= remove_eq_conds(thd, conds, &result);
    if (result == Item::COND_FALSE)             // Impossible where
      limit= 0;
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (prune_partitions(thd, table, conds))
  {
    free_underlaid_joins(thd, select_lex);
    thd->row_count_func= 0;
    my_ok(thd, (ha_rows) thd->row_count_func);  // No matching records
    DBUG_RETURN(0);
  }
#endif
  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.clear_all();
  table->quick_keys.clear_all();		// Can't use 'only index'
  select=make_select(table, 0, 0, conds, 0, &error);
  if (error)
    DBUG_RETURN(TRUE);
  if ((select && select->check_quick(thd, safe_update, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    thd->row_count_func= 0;
    /* 
      Error was already created by quick select evaluation (check_quick()).
      TODO: Add error code output parameter to Item::val_xxx() methods.
      Currently they rely on the user checking DA for
      errors when unwinding the stack after calling Item::val_xxx().
    */
    if (thd->is_error())
      DBUG_RETURN(TRUE);
    my_ok(thd, (ha_rows) thd->row_count_func);
    /*
      We don't need to call reset_auto_increment in this case, because
      mysql_truncate always gives a NULL conds argument, hence we never
      get here.
    */
    DBUG_RETURN(0);				// Nothing to delete
  }

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
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order && order->elements)
  {
    uint         length= 0;
    SORT_FIELD  *sortorder;
    ha_rows examined_rows;
    
    if ((!select || table->quick_keys.is_clear_all()) && limit != HA_POS_ERROR)
      usable_index= get_index_for_order(table, order->first, limit);

    if (usable_index == MAX_KEY)
    {
      table->sort.io_cache= (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                                   MYF(MY_FAE | MY_ZEROFILL));
    
      if (!(sortorder= make_unireg_sortorder(order->first,
                                             &length, NULL)) ||
	  (table->sort.found_records = filesort(thd, table, sortorder, length,
                                                select, HA_POS_ERROR, 1,
                                                &examined_rows))
	  == HA_POS_ERROR)
      {
        delete select;
        free_underlaid_joins(thd, &thd->lex->select_lex);
        DBUG_RETURN(TRUE);
      }
      thd->examined_row_count+= examined_rows;
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
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    DBUG_RETURN(TRUE);
  }
  if (usable_index == MAX_KEY || (select && select->quick))
  {
    if (init_read_record(&info, thd, table, select, 1, 1, FALSE))
    {
      delete select;
      free_underlaid_joins(thd, select_lex);
      DBUG_RETURN(TRUE);
    }
  }
  else
    init_read_record_idx(&info, thd, table, 1, usable_index);

  init_ftfuncs(thd, select_lex, 1);
  thd_proc_info(thd, "updating");

  /* NOTE: TRUNCATE must not invoke triggers. */

  triggers_applicable= table->triggers &&
                       thd->lex->sql_command != SQLCOM_TRUNCATE;

  if (triggers_applicable &&
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

  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  if (thd->lex->sql_command == SQLCOM_TRUNCATE &&
      thd->current_stmt_binlog_row_based)
    thd->clear_current_stmt_binlog_row_based();

  while (!(error=info.read_record(&info)) && !thd->killed &&
	 ! thd->is_error())
  {
    if (table->vfield)
      update_virtual_fields(thd, table,
                            triggers_applicable ? VCOL_UPDATE_ALL :
                                                  VCOL_UPDATE_FOR_READ);
    thd->examined_row_count++;
    // thd->is_error() is tested to disallow delete row on error
    if (!select || select->skip_record(thd) > 0)
    {
      if (triggers_applicable &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
      {
        error= 1;
        break;
      }

      if (!(error= table->file->ha_delete_row(table->record[0])))
      {
	deleted++;
        if (triggers_applicable &&
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
	table->file->print_error(error,
                                 MYF(thd->lex->ignore ? ME_JUST_WARNING : 0));
        if (thd->is_error())
        {
          error= 1;
          break;
        }
      }
    }
    else
      table->file->unlock_row();  // Row failed selection, release lock on it
  }
  killed_status= thd->killed;
  if (killed_status != NOT_KILLED || thd->is_error())
    error= 1;					// Aborted
  if (will_batch && (loc_error= table->file->end_bulk_delete()))
  {
    if (error != 1)
      table->file->print_error(loc_error,MYF(0));
    error=1;
  }
  thd_proc_info(thd, "end");
  end_read_record(&info);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

  if (reset_auto_increment && (error < 0))
  {
    /*
      We're really doing a truncate and need to reset the table's
      auto-increment counter.
    */
    int error2= table->file->ha_reset_auto_increment(0);

    if (error2 && (error2 != HA_ERR_WRONG_COMMAND))
    {
      table->file->print_error(error2, MYF(0));
      error= 1;
    }
  }

cleanup:
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted)
  {
    query_cache_invalidate3(thd, table_list, 1);
  }

  if (thd->lex->current_select->first_cond_optimization)
  {
    thd->lex->current_select->save_leaf_tables(thd);
    thd->lex->current_select->first_cond_optimization= 0;
  }

  delete select;
  transactional_table= table->file->has_transactions();

  if (!transactional_table && deleted > 0)
    thd->transaction.stmt.modified_non_trans_table= TRUE;
  
  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open() &&
        !(thd->lex->sql_command == SQLCOM_TRUNCATE &&
          thd->current_stmt_binlog_row_based &&
          find_temporary_table(thd, table_list)))
    {
      bool const is_trans=
        thd->lex->sql_command == SQLCOM_TRUNCATE ?
        FALSE :
        transactional_table;

      int errcode= 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == NOT_KILLED);
      
      /*
        [binlog]: If 'handler::delete_all_rows()' was called and the
        storage engine does not inject the rows itself, we replicate
        statement-based; otherwise, 'ha_delete_row()' was used to
        delete specific rows which we might log row-based.

        Note that TRUNCATE TABLE is not transactional and should
        therefore be treated as a DDL.
      */
      int log_result= thd->binlog_query(query_type,
                                        thd->query(), thd->query_length(),
                                        is_trans, FALSE, errcode);

      if (log_result)
      {
	error=1;
      }
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= TRUE;
  }
  thd->current_stmt_binlog_row_based= save_binlog_row_based;
  DBUG_ASSERT(transactional_table || !deleted || thd->transaction.stmt.modified_non_trans_table);
  free_underlaid_joins(thd, select_lex);
  if (error < 0 || 
      (thd->lex->ignore && !thd->is_error() && !thd->is_fatal_error))
  {
    /*
      If a TRUNCATE TABLE was issued, the number of rows should be reported as
      zero since the exact number is unknown.
    */
    thd->row_count_func= reset_auto_increment ? 0 : deleted;
    my_ok(thd, (ha_rows) thd->row_count_func);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted));
  }
  DBUG_RETURN(error >= 0 || thd->is_error());
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

  /*
    Statement-based replication of DELETE ... LIMIT is not safe as order of
    rows is not defined, so in mixed mode we go to row-based.

    Note that we may consider a statement as safe if ORDER BY primary_key
    is present. However it may confuse users to see very similiar statements
    replicated differently.
  */
  if (thd->lex->current_select->select_limit)
  {
    thd->lex->set_stmt_unsafe();
    thd->set_current_stmt_binlog_row_based_if_mixed();
  }
  thd->lex->allow_sum_func= 0;
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list, 
                                    select_lex->leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL, TRUE) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      setup_ftfuncs(select_lex))
    DBUG_RETURN(TRUE);
  if (!table_list->single_table_updatable() ||
      check_key_in_view(thd, table_list))
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

extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const uchar*)a, (const uchar*)b);
}

/*
  make delete specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_delete_prepare()
    thd         thread handler

  RETURN
    FALSE OK
    TRUE  Error
*/

int mysql_multi_delete_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  TABLE_LIST *aux_tables= lex->auxiliary_table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("mysql_multi_delete_prepare");

  if (mysql_handle_derived(lex, DT_INIT))
    DBUG_RETURN(TRUE);
  if (mysql_handle_derived(lex, DT_MERGE_FOR_INSERT))
    DBUG_RETURN(TRUE);
  if (mysql_handle_derived(lex, DT_PREPARE))
    DBUG_RETURN(TRUE);
  /*
    setup_tables() need for VIEWs. JOIN::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE SELECT_LEX
  */
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    lex->query_tables,
                                    lex->select_lex.leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL, FALSE))
    DBUG_RETURN(TRUE);

  if (lex->select_lex.handle_derived(thd->lex, DT_MERGE))  
    DBUG_RETURN(TRUE);

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

    target_tbl->table= target_tbl->correspondent_table->table;
    if (target_tbl->correspondent_table->is_multitable())
    {
       my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
                target_tbl->correspondent_table->view_db.str,
                target_tbl->correspondent_table->view_name.str);
       DBUG_RETURN(TRUE);
    }

    if (!target_tbl->correspondent_table->single_table_updatable() ||
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
  
  if (lex->select_lex.save_prep_leaf_tables(thd))
    DBUG_RETURN(TRUE);
  
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
  thd_proc_info(thd, "deleting from main table");
  SELECT_LEX *select_lex= u->first_select();
  if (select_lex->first_cond_optimization)
  {
    if (select_lex->handle_derived(thd->lex, DT_MERGE))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(0);
}


bool
multi_delete::initialize_tables(JOIN *join)
{
  TABLE_LIST *walk;
  Unique **tempfiles_ptr;
  DBUG_ENTER("initialize_tables");

  if ((thd->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
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

  for (JOIN_TAB *tab= first_linear_tab(join, WITH_CONST_TABLES); 
       tab; 
       tab= next_linear_tab(join, tab, WITHOUT_BUSH_ROOTS))
  {
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


int multi_delete::send_data(List<Item> &values)
{
  int secure_counter= delete_while_scanning ? -1 : 0;
  TABLE_LIST *del_table;
  DBUG_ENTER("multi_delete::send_data");

  bool ignore= thd->lex->ignore;

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
          thd->transaction.stmt.modified_non_trans_table= TRUE;
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


void multi_delete::abort()
{
  DBUG_ENTER("multi_delete::abort");

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->transaction.stmt.modified_non_trans_table && !deleted))
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
  
  if (thd->transaction.stmt.modified_non_trans_table)
  {
    /* 
       there is only side effects; to binlog with the error
    */
    if (mysql_bin_log.is_open())
    {
      int errcode= query_error_code(thd, thd->killed == NOT_KILLED);
      /* possible error of writing binary log is ignored deliberately */
      (void) thd->binlog_query(THD::ROW_QUERY_TYPE,
                              thd->query(), thd->query_length(),
                              transactional_tables, FALSE, errcode);
    }
    thd->transaction.all.modified_non_trans_table= true;
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
    int local_error; 
    if (tempfiles[counter]->get(table))
      DBUG_RETURN(1);

    local_error= do_table_deletes(table, thd->lex->ignore);

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
    thd->transaction.stmt.modified_non_trans_table= TRUE;

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
  killed_state killed_status= NOT_KILLED;
  thd_proc_info(thd, "deleting from reference tables");

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error= do_deletes();		// returns 0 if success

  /* compute a total error to know if something failed */
  local_error= local_error || error;
  killed_status= (local_error == 0)? NOT_KILLED : thd->killed;
  /* reset used flags */
  thd_proc_info(thd, "end");

  /*
    We must invalidate the query cache before binlog writing and
    ha_autocommit_...
  */
  if (deleted)
  {
    query_cache_invalidate3(thd, delete_tables, 1);
  }
  if ((local_error == 0) || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (local_error == 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query(), thd->query_length(),
                            transactional_tables, FALSE, errcode) &&
          !normal_tables)
      {
	local_error=1;  // Log write failed: roll back the SQL statement
      }
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= TRUE;
  }
  if (local_error != 0)
    error_handled= TRUE; // to force early leave from ::send_error()

  if (!local_error)
  {
    thd->row_count_func= deleted;
    ::my_ok(thd, (ha_rows) thd->row_count_func);
  }
  return 0;
}


/***************************************************************************
  TRUNCATE TABLE
****************************************************************************/

/*
  Row-by-row truncation if the engine does not support table recreation.
  Probably a InnoDB table.
*/

static bool mysql_truncate_by_delete(THD *thd, TABLE_LIST *table_list)
{
  bool error;
  DBUG_ENTER("mysql_truncate_by_delete");
  table_list->lock_type= TL_WRITE;
  mysql_init_select(thd->lex);
  error= mysql_delete(thd, table_list, NULL, NULL, HA_POS_ERROR, LL(0), TRUE);
  ha_autocommit_or_rollback(thd, error);
  end_trans(thd, error ? ROLLBACK : COMMIT);
  DBUG_RETURN(error);
}


/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed

  dont_send_ok should be set if:
  - We should always wants to generate the table (even if the table type
    normally can't safely do this.
  - We don't want an ok to be sent to the end user.
  - We don't want to log the truncate command
  - If we want to have a name lock on the table on exit without errors.
*/

bool mysql_truncate(THD *thd, TABLE_LIST *table_list, bool dont_send_ok)
{
  HA_CREATE_INFO create_info;
  char path[FN_REFLEN + 1];
  TABLE *table;
  TABLE_LIST *tbl;
  bool error;
  uint path_length;
  bool is_temporary_table= false;
  DBUG_ENTER("mysql_truncate");

  bzero((char*) &create_info,sizeof(create_info));

  /* Remove tables from the HANDLER's hash. */
  mysql_ha_rm_tables(thd, table_list, FALSE);

  /* If it is a temporary table, close and regenerate it */
  if (!dont_send_ok && (table= find_temporary_table(thd, table_list)))
  {
    TABLE_SHARE *share= table->s;
    handlerton *table_type= share->db_type();
    is_temporary_table= true;

    if (!ha_check_storage_engine_flag(table_type, HTON_CAN_RECREATE))
      goto trunc_by_del;

    for (tbl= table_list; tbl; tbl= tbl->next_local)
      tbl->deleting= TRUE; /* to trigger HA_PREPARE_FOR_DROP */

    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);

    create_info.options|= HA_LEX_CREATE_TMP_TABLE;
    close_temporary_table(thd, table, 0, 0);    // Don't free share
    ha_create_table(thd, share->normalized_path.str,
                    share->db.str, share->table_name.str, &create_info, 1);
    // We don't need to call invalidate() because this table is not in cache
    if ((error= (int) !(open_temporary_table(thd, share->path.str,
                                             share->db.str,
					     share->table_name.str, 1))))
      (void) rm_temporary_table(table_type, share->path.str);
    else
      thd->thread_specific_used= TRUE;
    
    free_table_share(share);
    my_free((char*) table,MYF(0));
    /*
      If we return here we will not have logged the truncation to the bin log
      and we will not my_ok() to the client.
    */
    goto end;
  }

  path_length= build_table_filename(path, sizeof(path) - 1, table_list->db,
                                    table_list->table_name, reg_ext, 0);

  if (!dont_send_ok)
  {
    enum legacy_db_type table_type;
    mysql_frm_type(thd, path, &table_type);
    if (table_type == DB_TYPE_UNKNOWN)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0),
               table_list->db, table_list->table_name);
      DBUG_RETURN(TRUE);
    }
    if (!ha_check_storage_engine_flag(ha_resolve_by_legacy_type(thd,
                                                                table_type),
                                      HTON_CAN_RECREATE))
      goto trunc_by_del;

    if (lock_and_wait_for_table_name(thd, table_list))
      DBUG_RETURN(TRUE);
  }

  /*
    Remove the .frm extension AIX 5.2 64-bit compiler bug (BUG#16155): this
    crashes, replacement works.  *(path + path_length - reg_ext_length)=
     '\0';
  */
  path[path_length - reg_ext_length] = 0;
  VOID(pthread_mutex_lock(&LOCK_open));
  error= ha_create_table(thd, path, table_list->db, table_list->table_name,
                         &create_info, 1);
  VOID(pthread_mutex_unlock(&LOCK_open));
  query_cache_invalidate3(thd, table_list, 0);

end:
  if (!dont_send_ok)
  {
    if (!error)
    {
      /* In RBR, the statement is not binlogged if the table is temporary. */
      if (!is_temporary_table || !thd->current_stmt_binlog_row_based)
        error= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
      if (!error)
        my_ok(thd);		// This should return record count
    }
    VOID(pthread_mutex_lock(&LOCK_open));
    unlock_table_name(thd, table_list);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  else if (error)
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    unlock_table_name(thd, table_list);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  DBUG_RETURN(error);

trunc_by_del:
  error= mysql_truncate_by_delete(thd, table_list);
  DBUG_RETURN(error);
}

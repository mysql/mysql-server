/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "transaction.h"
#include "debug_sync.h"
#include "records.h"     // READ_RECORD
#include "table.h"       // TABLE
#include "sql_class.h"   // THD
#include "sql_base.h"    // open_and_lock_tables
#include "sql_table.h"   // write_bin_log
#include "sql_handler.h" // mysql_ha_rm_tables
#include "datadict.h"    // dd_recreate_table()
#include "lock.h"        // MYSQL_OPEN_TEMPORARY_ONLY
#include "sql_acl.h"     // DROP_ACL
#include "sql_parse.h"   // check_one_table_access()
#include "sql_truncate.h"


/*
  Delete all rows of a locked table.

  @param  thd           Thread context.
  @param  table_list    Table list element for the table.
  @param  rows_deleted  Whether rows might have been deleted.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

static bool
delete_all_rows(THD *thd, TABLE *table)
{
  int error;
  READ_RECORD info;
  bool is_bulk_delete;
  bool some_rows_deleted= FALSE;
  bool save_binlog_row_based= thd->is_current_stmt_binlog_format_row();
  DBUG_ENTER("delete_all_rows");

  /* Replication of truncate table must be statement based. */
  thd->clear_current_stmt_binlog_format_row();

  /*
    Update handler statistics (e.g. table->file->stats.records).
    Might be used by the storage engine to aggregate information
    necessary to allow deletion. Currently, this seems to be
    meaningful only to the archive storage engine, which uses
    the info method to set the number of records. Although
    archive does not support deletion, it becomes necessary in
    order to return a error if the table is not empty.
  */
  error= table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  if (error && error != HA_ERR_WRONG_COMMAND)
  {
    table->file->print_error(error, MYF(0));
    goto end;
  }

  /*
    Attempt to delete all rows in the table.
    If it is unsupported, switch to row by row deletion.
  */
  if (! (error= table->file->ha_delete_all_rows()))
    goto end;

  if (error != HA_ERR_WRONG_COMMAND)
  {
    /*
      If a transactional engine fails in the middle of deletion,
      we expect it to be able to roll it back.  Some reasons
      for the engine to fail would be media failure or corrupted
      data dictionary (i.e. in case of a partitioned table). We
      have sufficiently strong metadata locks to rule out any
      potential deadlocks.

      If a non-transactional engine fails here (that would
      not be MyISAM, since MyISAM does TRUNCATE by recreate),
      and binlog is on, replication breaks, since nothing gets
      written to the binary log.  (XXX: is this a bug?)
    */
    table->file->print_error(error, MYF(0));
    goto end;
  }

  /*
    A workaround for Bug#53696  "Performance schema engine violates the
    PSEA API by calling my_error()".
  */
  if (thd->is_error())
    goto end;

  /* Handler didn't support fast delete. Delete rows one by one. */

  init_read_record(&info, thd, table, NULL, TRUE, TRUE, FALSE);

  /*
    Start bulk delete. If the engine does not support it, go on,
    it's not an error.
  */
  is_bulk_delete= ! table->file->start_bulk_delete();

  table->mark_columns_needed_for_delete();

  while (!(error= info.read_record(&info)) && !thd->killed)
  {
    if ((error= table->file->ha_delete_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
      break;
    }

    some_rows_deleted= TRUE;
  }

  /* HA_ERR_END_OF_FILE */
  if (error == -1)
    error= 0;

  /* Close down the bulk delete. */
  if (is_bulk_delete)
  {
    int bulk_delete_error= table->file->end_bulk_delete();
    if (bulk_delete_error && !error)
    {
      table->file->print_error(bulk_delete_error, MYF(0));
      error= bulk_delete_error;
    }
  }

  end_read_record(&info);

  /*
    Regardless of the error status, the query must be written to the
    binary log if rows of the table is non-transactional.
  */
  if (some_rows_deleted && !table->file->has_transactions())
  {
    thd->transaction.stmt.modified_non_trans_table= TRUE;
    thd->transaction.all.modified_non_trans_table= TRUE;
  }

  if (error || thd->killed)
    goto end;

  /* Truncate resets the auto-increment counter. */
  error= table->file->ha_reset_auto_increment(0);
  if (error)
  {
    if (error != HA_ERR_WRONG_COMMAND)
      table->file->print_error(error, MYF(0));
    else
      error= 0;
  }

end:
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_format_row();

  DBUG_RETURN(error);
}


/*
  Close and recreate a temporary table. In case of success,
  write truncate statement into the binary log if in statement
  mode.

  @param  thd     Thread context.
  @param  table   The temporary table.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

static bool recreate_temporary_table(THD *thd, TABLE *table)
{
  bool error= TRUE;
  TABLE_SHARE *share= table->s;
  HA_CREATE_INFO create_info;
  handlerton *table_type= table->s->db_type();
  DBUG_ENTER("recreate_temporary_table");

  memset(&create_info, 0, sizeof(create_info));

  table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);

  /* Don't free share. */
  close_temporary_table(thd, table, FALSE, FALSE);

  /*
    We must use share->normalized_path.str since for temporary tables it
    differs from what dd_recreate_table() would generate based
    on table and schema names.
  */
  ha_create_table(thd, share->normalized_path.str, share->db.str,
                  share->table_name.str, &create_info, 1);

  if (open_temporary_table(thd, share->path.str, share->db.str,
                           share->table_name.str, 1))
  {
    error= FALSE;
    thd->thread_specific_used= TRUE;
  }
  else
    rm_temporary_table(table_type, share->path.str);

  free_table_share(share);
  my_free(table);

  DBUG_RETURN(error);
}


/*
  Handle opening and locking if a base table for truncate.

  @param[in]  thd               Thread context.
  @param[in]  table_ref         Table list element for the table to
                                be truncated.
  @param[out] hton_can_recreate Set to TRUE if table can be dropped
                                and recreated.
  @param[out] ticket_downgrade  Set if a lock must be downgraded after
                                truncate is done.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

static bool open_and_lock_table_for_truncate(THD *thd, TABLE_LIST *table_ref,
                                             bool *hton_can_recreate,
                                             MDL_ticket **ticket_downgrade)
{
  TABLE *table= NULL;
  handlerton *table_type;
  DBUG_ENTER("open_and_lock_table_for_truncate");

  DBUG_ASSERT(table_ref->lock_type == TL_WRITE);
  DBUG_ASSERT(table_ref->mdl_request.type == MDL_SHARED_NO_READ_WRITE);
  /*
    Before doing anything else, acquire a metadata lock on the table,
    or ensure we have one.  We don't use open_and_lock_tables()
    right away because we want to be able to truncate (and recreate)
    corrupted tables, those that we can't fully open.

    MySQL manual documents that TRUNCATE can be used to repair a
    damaged table, i.e. a table that can not be fully "opened".
    In particular MySQL manual says: As long as the table format
    file tbl_name.frm is valid, the table can be re-created as
    an empty table with TRUNCATE TABLE, even if the data or index
    files have become corrupted.
  */
  if (thd->locked_tables_mode)
  {
    if (!(table= find_table_for_mdl_upgrade(thd->open_tables, table_ref->db,
                                            table_ref->table_name, FALSE)))
      DBUG_RETURN(TRUE);

    table_type= table->s->db_type();
    *hton_can_recreate= ha_check_storage_engine_flag(table_type,
                                                     HTON_CAN_RECREATE);
    table_ref->mdl_request.ticket= table->mdl_ticket;
  }
  else
  {
    /*
      Even though we could use the previous execution branch here just as
      well, we must not try to open the table:
    */
    DBUG_ASSERT(table_ref->next_global == NULL);
    if (lock_table_names(thd, table_ref, NULL,
                         thd->variables.lock_wait_timeout,
                         MYSQL_OPEN_SKIP_TEMPORARY))
      DBUG_RETURN(TRUE);

    if (dd_frm_storage_engine(thd, table_ref->db, table_ref->table_name,
                              &table_type))
      DBUG_RETURN(TRUE);
    *hton_can_recreate= ha_check_storage_engine_flag(table_type,
                                                     HTON_CAN_RECREATE);
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /*
    TODO: Add support for TRUNCATE PARTITION for NDB and other engines
    supporting native partitioning.
  */
  if (thd->lex->alter_info.flags & ALTER_ADMIN_PARTITION &&
      table_type != partition_hton)
  {
    my_error(ER_PARTITION_MGMT_ON_NONPARTITIONED, MYF(0));
    DBUG_RETURN(TRUE);
  }
#endif
  DEBUG_SYNC(thd, "lock_table_for_truncate");

  if (*hton_can_recreate)
  {
    /*
      Acquire an exclusive lock. The storage engine can recreate the
      table only if there are no references to it from anywhere, i.e.
      no cached TABLE in the table cache. To remove the table from the
      cache we need an exclusive lock.
    */
    if (thd->locked_tables_mode)
    {
      if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
        DBUG_RETURN(TRUE);
      *ticket_downgrade= table->mdl_ticket;
      close_all_tables_for_name(thd, table->s, FALSE);
    }
    else
    {
      ulong timeout= thd->variables.lock_wait_timeout;
      if (thd->mdl_context.
          upgrade_shared_lock_to_exclusive(table_ref->mdl_request.ticket,
                                           timeout))
        DBUG_RETURN(TRUE);

      tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table_ref->db,
                       table_ref->table_name, FALSE);
    }
  }
  else
  {
    /*
      Can't recreate, we must mechanically delete all rows in
      the table. Our metadata lock guarantees that no transaction
      is reading or writing into the table. Yet, to open a write
      cursor we need a thr_lock lock. Use open_and_lock_tables()
      to do the necessary job.
    */

    /* Allow to open base tables only. */
    table_ref->required_type= FRMTYPE_TABLE;
    /* We don't need to load triggers. */
    DBUG_ASSERT(table_ref->trg_event_map == 0);
    /*
      Even though we have an MDL lock on the table here, we don't
      pass MYSQL_OPEN_HAS_MDL_LOCK to open_and_lock_tables
      since to truncate a MERGE table, we must open and lock
      merge children, and on those we don't have an MDL lock.
      Thus clear the ticket to satisfy MDL asserts.
    */
    table_ref->mdl_request.ticket= NULL;

    /*
      Open the table as it will handle some required preparations.
      Ignore pending FLUSH TABLES since we don't want to release
      the MDL lock taken above and otherwise there is no way to
      wait for FLUSH TABLES in deadlock-free fashion.
    */
    if (open_and_lock_tables(thd, table_ref, FALSE,
                             MYSQL_OPEN_IGNORE_FLUSH |
                             MYSQL_OPEN_SKIP_TEMPORARY))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/*
  Optimized delete of all rows by doing a full generate of the table.

  @remark Will work even if the .MYI and .MYD files are destroyed.
          In other words, it works as long as the .FRM is intact and
          the engine supports re-create.

  @param  thd         Thread context.
  @param  table_ref   Table list element for the table to be truncated.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

bool mysql_truncate_table(THD *thd, TABLE_LIST *table_ref)
{
  TABLE *table;
  bool error= TRUE, binlog_stmt;
  MDL_ticket *mdl_ticket= NULL;
  DBUG_ENTER("mysql_truncate_table");

  /* Remove tables from the HANDLER's hash. */
  mysql_ha_rm_tables(thd, table_ref);

  /* If it is a temporary table, no need to take locks. */
  if ((table= find_temporary_table(thd, table_ref)))
  {
    /* In RBR, the statement is not binlogged if the table is temporary. */
    binlog_stmt= !thd->is_current_stmt_binlog_format_row();

    /* Note that a temporary table cannot be partitioned. */
    if (ha_check_storage_engine_flag(table->s->db_type(), HTON_CAN_RECREATE))
    {
      if ((error= recreate_temporary_table(thd, table)))
        binlog_stmt= FALSE; /* No need to binlog failed truncate-by-recreate. */

      DBUG_ASSERT(! thd->transaction.stmt.modified_non_trans_table);
    }
    else
    {
      /*
        The engine does not support truncate-by-recreate. Open the
        table and delete all rows. In such a manner this can in fact
        open several tables if it's a temporary MyISAMMRG table.
      */
      if (open_and_lock_tables(thd, table_ref, FALSE,
                               MYSQL_OPEN_TEMPORARY_ONLY))
        DBUG_RETURN(TRUE);

      error= delete_all_rows(thd, table_ref->table);
    }

    /*
      No need to invalidate the query cache, queries with temporary
      tables are not in the cache. No need to write to the binary
      log a failed row-by-row delete even if under RBR as the table
      might not exist on the slave.
    */
  }
  else /* It's not a temporary table. */
  {
    bool hton_can_recreate;

    if (open_and_lock_table_for_truncate(thd, table_ref,
                                         &hton_can_recreate, &mdl_ticket))
      DBUG_RETURN(TRUE);

    if (hton_can_recreate)
    {
     /*
        The storage engine can truncate the table by creating an
        empty table with the same structure.
      */
      error= dd_recreate_table(thd, table_ref->db, table_ref->table_name);

      if (thd->locked_tables_mode && thd->locked_tables_list.reopen_tables(thd))
          thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);

      /* No need to binlog a failed truncate-by-recreate. */
      binlog_stmt= !error;
    }
    else
    {
      error= delete_all_rows(thd, table_ref->table);

      /*
        Regardless of the error status, the query must be written to the
        binary log if rows of a non-transactional table were deleted.
      */
      binlog_stmt= !error || thd->transaction.stmt.modified_non_trans_table;
    }

    query_cache_invalidate3(thd, table_ref, FALSE);
  }

  /* DDL is logged in statement format, regardless of binlog format. */
  if (binlog_stmt)
    error|= write_bin_log(thd, !error, thd->query(), thd->query_length());

  /*
    All effects of a TRUNCATE TABLE operation are rolled back if a row
    by row deletion fails. Otherwise, it is automatically committed at
    the end.
  */
  if (error)
  {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  }

  /*
    A locked table ticket was upgraded to a exclusive lock. After the
    the query has been written to the binary log, downgrade the lock
    to a shared one.
  */
  if (mdl_ticket)
    mdl_ticket->downgrade_exclusive_lock(MDL_SHARED_NO_READ_WRITE);

  DBUG_PRINT("exit", ("error: %d", error));
  DBUG_RETURN(test(error));
}


bool Truncate_statement::execute(THD *thd)
{
  TABLE_LIST *first_table= thd->lex->select_lex.table_list.first;
  bool res= TRUE;
  DBUG_ENTER("Truncate_statement::execute");

  if (check_one_table_access(thd, DROP_ACL, first_table))
    goto error;
  /*
    Don't allow this within a transaction because we want to use
    re-generate table
  */
  if (thd->in_active_multi_stmt_transaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    goto error;
  }
  if (! (res= mysql_truncate_table(thd, first_table)))
    my_ok(thd);
error:
  DBUG_RETURN(res);
}

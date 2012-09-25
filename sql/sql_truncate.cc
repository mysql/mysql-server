/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "debug_sync.h"  // DEBUG_SYNC
#include "table.h"       // TABLE, FOREIGN_KEY_INFO
#include "sql_class.h"   // THD
#include "sql_base.h"    // open_and_lock_tables
#include "sql_table.h"   // write_bin_log
#include "datadict.h"    // dd_recreate_table()
#include "lock.h"        // MYSQL_OPEN_* flags
#include "sql_acl.h"     // DROP_ACL
#include "sql_parse.h"   // check_one_table_access()
#include "sql_truncate.h"
#include "sql_show.h"    //append_identifier()


/**
  Append a list of field names to a string.

  @param  str     The string.
  @param  fields  The list of field names.

  @return TRUE on failure, FALSE otherwise.
*/

static bool fk_info_append_fields(String *str, List<LEX_STRING> *fields)
{
  bool res= FALSE;
  LEX_STRING *field;
  List_iterator_fast<LEX_STRING> it(*fields);

  while ((field= it++))
  {
    append_identifier(NULL, str, field->str, field->length);
    res|= str->append(", ");
  }

  str->chop();
  str->chop();

  return res;
}


/**
  Generate a foreign key description suitable for a error message.

  @param thd          Thread context.
  @param fk_info   The foreign key information.

  @return A human-readable string describing the foreign key.
*/

static const char *fk_info_str(THD *thd, FOREIGN_KEY_INFO *fk_info)
{
  bool res= FALSE;
  char buffer[STRING_BUFFER_USUAL_SIZE*2];
  String str(buffer, sizeof(buffer), system_charset_info);

  str.length(0);

  /*
    `db`.`tbl`, CONSTRAINT `id` FOREIGN KEY (`fk`) REFERENCES `db`.`tbl` (`fk`)
  */

  append_identifier(NULL, &str, fk_info->foreign_db->str,
                    fk_info->foreign_db->length);
  res|= str.append(".");
  append_identifier(NULL, &str, fk_info->foreign_table->str,
                    fk_info->foreign_table->length);
  res|= str.append(", CONSTRAINT ");
  append_identifier(NULL, &str, fk_info->foreign_id->str,
                    fk_info->foreign_id->length);
  res|= str.append(" FOREIGN KEY (");
  res|= fk_info_append_fields(&str, &fk_info->foreign_fields);
  res|= str.append(") REFERENCES ");
  append_identifier(NULL, &str, fk_info->referenced_db->str,
                    fk_info->referenced_db->length);
  res|= str.append(".");
  append_identifier(NULL, &str, fk_info->referenced_table->str,
                    fk_info->referenced_table->length);
  res|= str.append(" (");
  res|= fk_info_append_fields(&str, &fk_info->referenced_fields);
  res|= str.append(')');

  return res ? NULL : thd->strmake(str.ptr(), str.length());
}


/**
  Check and emit a fatal error if the table which is going to be
  affected by TRUNCATE TABLE is a parent table in some non-self-
  referencing foreign key.

  @remark The intention is to allow truncate only for tables that
          are not dependent on other tables.

  @param  thd    Thread context.
  @param  table  Table handle.

  @retval FALSE  This table is not parent in a non-self-referencing foreign
                 key. Statement can proceed.
  @retval TRUE   This table is parent in a non-self-referencing foreign key,
                 error was emitted.
*/

static bool
fk_truncate_illegal_if_parent(THD *thd, TABLE *table)
{
  FOREIGN_KEY_INFO *fk_info;
  List<FOREIGN_KEY_INFO> fk_list;
  List_iterator_fast<FOREIGN_KEY_INFO> it;

  /*
    Bail out early if the table is not referenced by a foreign key.
    In this case, the table could only be, if at all, a child table.
  */
  if (! table->file->referenced_by_foreign_key())
    return FALSE;

  /*
    This table _is_ referenced by a foreign key. At this point, only
    self-referencing keys are acceptable. For this reason, get the list
    of foreign keys referencing this table in order to check the name
    of the child (dependent) tables.
  */
  table->file->get_parent_foreign_key_list(thd, &fk_list);

  /* Out of memory when building list. */
  if (thd->is_error())
    return TRUE;

  it.init(fk_list);

  /* Loop over the set of foreign keys for which this table is a parent. */
  while ((fk_info= it++))
  {
    DBUG_ASSERT(!my_strcasecmp(system_charset_info,
                               fk_info->referenced_db->str,
                               table->s->db.str));

    DBUG_ASSERT(!my_strcasecmp(system_charset_info,
                               fk_info->referenced_table->str,
                               table->s->table_name.str));

    if (my_strcasecmp(system_charset_info, fk_info->foreign_db->str,
                      table->s->db.str) ||
        my_strcasecmp(system_charset_info, fk_info->foreign_table->str,
                      table->s->table_name.str))
      break;
  }

  /* Table is parent in a non-self-referencing foreign key. */
  if (fk_info)
  {
    my_error(ER_TRUNCATE_ILLEGAL_FK, MYF(0), fk_info_str(thd, fk_info));
    return TRUE;
  }

  return FALSE;
}


/*
  Open and truncate a locked table.

  @param  thd           Thread context.
  @param  table_ref     Table list element for the table to be truncated.
  @param  is_tmp_table  True if element refers to a temp table.

  @retval  0    Success.
  @retval  > 0  Error code.
*/

int Sql_cmd_truncate_table::handler_truncate(THD *thd, TABLE_LIST *table_ref,
                                             bool is_tmp_table)
{
  int error= 0;
  uint flags= 0;
  DBUG_ENTER("Sql_cmd_truncate_table::handler_truncate");

  /*
    Can't recreate, the engine must mechanically delete all rows
    in the table. Use open_and_lock_tables() to open a write cursor.
  */

  /* If it is a temporary table, no need to take locks. */
  if (!is_tmp_table)
  {
    /* We don't need to load triggers. */
    DBUG_ASSERT(table_ref->trg_event_map == 0);
    /*
      Our metadata lock guarantees that no transaction is reading
      or writing into the table. Yet, to open a write cursor we need
      a thr_lock lock. Allow to open base tables only.
    */
    table_ref->required_type= FRMTYPE_TABLE;
    /*
      Ignore pending FLUSH TABLES since we don't want to release
      the MDL lock taken above and otherwise there is no way to
      wait for FLUSH TABLES in deadlock-free fashion.
    */
    flags= MYSQL_OPEN_IGNORE_FLUSH;
    /*
      Even though we have an MDL lock on the table here, we don't
      pass MYSQL_OPEN_HAS_MDL_LOCK to open_and_lock_tables
      since to truncate a MERGE table, we must open and lock
      merge children, and on those we don't have an MDL lock.
      Thus clear the ticket to satisfy MDL asserts.
    */
    table_ref->mdl_request.ticket= NULL;
  }

  /* Open the table as it will handle some required preparations. */
  if (open_and_lock_tables(thd, table_ref, FALSE, flags))
    DBUG_RETURN(1);

  /* Whether to truncate regardless of foreign keys. */
  if (! (thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
    error= fk_truncate_illegal_if_parent(thd, table_ref->table);

  if (!error && (error= table_ref->table->file->ha_truncate()))
    table_ref->table->file->print_error(error, MYF(0));

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

  if (open_table_uncached(thd, share->path.str, share->db.str,
                          share->table_name.str, true, true))
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
  Handle locking a base table for truncate.

  @param[in]  thd               Thread context.
  @param[in]  table_ref         Table list element for the table to
                                be truncated.
  @param[out] hton_can_recreate Set to TRUE if table can be dropped
                                and recreated.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

bool Sql_cmd_truncate_table::lock_table(THD *thd, TABLE_LIST *table_ref,
                                        bool *hton_can_recreate)
{
  TABLE *table= NULL;
  DBUG_ENTER("Sql_cmd_truncate_table::lock_table");

  /* Lock types are set in the parser. */
  DBUG_ASSERT(table_ref->lock_type == TL_WRITE);
  /* The handler truncate protocol dictates a exclusive lock. */
  DBUG_ASSERT(table_ref->mdl_request.type == MDL_EXCLUSIVE);

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
    if (!(table= find_table_for_mdl_upgrade(thd, table_ref->db,
                                            table_ref->table_name, FALSE)))
      DBUG_RETURN(TRUE);

    *hton_can_recreate= ha_check_storage_engine_flag(table->s->db_type(),
                                                     HTON_CAN_RECREATE);
    table_ref->mdl_request.ticket= table->mdl_ticket;
  }
  else
  {
    /* Acquire an exclusive lock. */
    DBUG_ASSERT(table_ref->next_global == NULL);
    if (lock_table_names(thd, table_ref, NULL,
                         thd->variables.lock_wait_timeout, 0))
      DBUG_RETURN(TRUE);

    if (dd_check_storage_engine_flag(thd, table_ref->db, table_ref->table_name,
                                     HTON_CAN_RECREATE, hton_can_recreate))
      DBUG_RETURN(TRUE);
  }

  /*
    A storage engine can recreate or truncate the table only if there
    are no references to it from anywhere, i.e. no cached TABLE in the
    table cache.
  */
  if (thd->locked_tables_mode)
  {
    DEBUG_SYNC(thd, "upgrade_lock_for_truncate");
    /* To remove the table from the cache we need an exclusive lock. */
    if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      DBUG_RETURN(TRUE);
    m_ticket_downgrade= table->mdl_ticket;
    /* Close if table is going to be recreated. */
    if (*hton_can_recreate)
      close_all_tables_for_name(thd, table->s, false, NULL);
  }
  else
  {
    /* Table is already locked exclusively. Remove cached instances. */
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table_ref->db,
                     table_ref->table_name, FALSE);
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

bool Sql_cmd_truncate_table::truncate_table(THD *thd, TABLE_LIST *table_ref)
{
  int error;
  bool binlog_stmt;
  DBUG_ENTER("Sql_cmd_truncate_table::truncate_table");

  DBUG_ASSERT((!table_ref->table) ||
              (table_ref->table && table_ref->table->s));

  /* Initialize, or reinitialize in case of reexecution (SP). */
  m_ticket_downgrade= NULL;

  /* If it is a temporary table, no need to take locks. */
  if (is_temporary_table(table_ref))
  {
    TABLE *tmp_table= table_ref->table;

    /* In RBR, the statement is not binlogged if the table is temporary. */
    binlog_stmt= !thd->is_current_stmt_binlog_format_row();

    /* Note that a temporary table cannot be partitioned. */
    if (ha_check_storage_engine_flag(tmp_table->s->db_type(), HTON_CAN_RECREATE))
    {
      if ((error= recreate_temporary_table(thd, tmp_table)))
        binlog_stmt= FALSE; /* No need to binlog failed truncate-by-recreate. */

      DBUG_ASSERT(! thd->transaction.stmt.cannot_safely_rollback());
    }
    else
    {
      /*
        The engine does not support truncate-by-recreate. Open the
        table and invoke the handler truncate. In such a manner this
        can in fact open several tables if it's a temporary MyISAMMRG
        table.
      */
      error= handler_truncate(thd, table_ref, TRUE);
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

    if (lock_table(thd, table_ref, &hton_can_recreate))
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
      /*
        The engine does not support truncate-by-recreate.
        Attempt to use the handler truncate method.
      */
      error= handler_truncate(thd, table_ref, FALSE);

      /*
        All effects of a TRUNCATE TABLE operation are committed even if
        truncation fails. Thus, the query must be written to the binary
        log. The only exception is a unimplemented truncate method.
      */
      binlog_stmt= !error || error != HA_ERR_WRONG_COMMAND;
    }

    /*
      If we tried to open a MERGE table and failed due to problems with the
      children tables, the table will have been closed and table_ref->table
      will be invalid. Reset the pointer here in any case as
      query_cache_invalidate does not need a valid TABLE object.
    */
    table_ref->table= NULL;
    query_cache_invalidate3(thd, table_ref, FALSE);
  }

  /* DDL is logged in statement format, regardless of binlog format. */
  if (binlog_stmt)
    error|= write_bin_log(thd, !error, thd->query(), thd->query_length());

  /*
    A locked table ticket was upgraded to a exclusive lock. After the
    the query has been written to the binary log, downgrade the lock
    to a shared one.
  */
  if (m_ticket_downgrade)
    m_ticket_downgrade->downgrade_lock(MDL_SHARED_NO_READ_WRITE);

  DBUG_RETURN(error);
}


/**
  Execute a TRUNCATE statement at runtime.

  @param  thd   The current thread.

  @return FALSE on success.
*/
bool Sql_cmd_truncate_table::execute(THD *thd)
{
  bool res= TRUE;
  TABLE_LIST *first_table= thd->lex->select_lex.table_list.first;
  DBUG_ENTER("Sql_cmd_truncate_table::execute");

  if (check_one_table_access(thd, DROP_ACL, first_table))
    DBUG_RETURN(res);

  if (! (res= truncate_table(thd, first_table)))
    my_ok(thd);

  DBUG_RETURN(res);
}


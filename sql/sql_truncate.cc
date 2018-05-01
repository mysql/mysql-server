/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/sql_truncate.h"

#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld.h"  // table_alias_charset
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"            // DROP_ACL
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_schema.h"                // dd::Schema_MDL_locker
#include "sql/dd/dd_table.h"                 // dd::table_storage_engine
#include "sql/dd/types/abstract_table.h"     // dd::enum_table_type
#include "sql/dd/types/table.h"              // dd::Table
#include "sql/debug_sync.h"                  // DEBUG_SYNC
#include "sql/handler.h"
#include "sql/lock.h"  // MYSQL_OPEN_* flags
#include "sql/mdl.h"
#include "sql/query_options.h"
#include "sql/sql_audit.h"        // mysql_audit_table_access_notify
#include "sql/sql_backup_lock.h"  // acquire_shared_backup_lock
#include "sql/sql_base.h"         // open_and_lock_tables
#include "sql/sql_class.h"        // THD
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_show.h"   // append_identifier()
#include "sql/sql_table.h"  // write_bin_log
#include "sql/system_variables.h"
#include "sql/table.h"        // TABLE, FOREIGN_KEY_INFO
#include "sql/transaction.h"  // trans_commit_stmt()
#include "sql/transaction_info.h"
#include "sql_string.h"
#include "thr_lock.h"

namespace dd {
class Table;
}  // namespace dd

/**
  Generate a foreign key description suitable for a error message.

  @param thd    Thread context.
  @param fk_p   Object describing foreign key in parent table.

  @return A human-readable string describing the foreign key.
*/

static const char *fk_info_str(THD *thd, const dd::Foreign_key_parent *fk_p) {
  bool res = false;
  char buffer[STRING_BUFFER_USUAL_SIZE * 2];
  String str(buffer, sizeof(buffer), system_charset_info);

  str.length(0);

  /*
    `db`.`tbl`, CONSTRAINT `id`
  */

  append_identifier(NULL, &str, fk_p->child_schema_name().c_str(),
                    fk_p->child_schema_name().length());
  res |= str.append(".");
  append_identifier(NULL, &str, fk_p->child_table_name().c_str(),
                    fk_p->child_table_name().length());
  res |= str.append(", CONSTRAINT ");
  append_identifier(NULL, &str, fk_p->fk_name().c_str(),
                    fk_p->fk_name().length());

  return res ? NULL : thd->strmake(str.ptr(), str.length());
}

/**
  Check and emit a fatal error if the table which is going to be
  affected by TRUNCATE TABLE is a parent table in some non-self-
  referencing foreign key.

  @remark The intention is to allow truncate only for tables that
          are not dependent on other tables.

  @param  thd         Thread context.
  @param  table_list  Table list element for the table.
  @param  table_def   dd::Table describing the table.

  @retval false  This table is not parent in a non-self-referencing foreign
                 key. Statement can proceed.
  @retval true   This table is parent in a non-self-referencing foreign key,
                 error was emitted.
*/

static bool fk_truncate_illegal_if_parent(THD *thd, TABLE_LIST *table_list,
                                          dd::Table *table_def) {
  for (const dd::Foreign_key_parent *fk_p : table_def->foreign_key_parents()) {
    if (my_strcasecmp(table_alias_charset, fk_p->child_schema_name().c_str(),
                      table_list->db) == 0 &&
        my_strcasecmp(table_alias_charset, fk_p->child_table_name().c_str(),
                      table_list->table_name) == 0)
      continue;

    /* Table is parent in a non-self-referencing foreign key. */
    my_error(ER_TRUNCATE_ILLEGAL_FK, MYF(0), fk_info_str(thd, fk_p));
    return true;
  }
  return false;
}

enum truncate_result {
  TRUNCATE_OK = 0,
  TRUNCATE_FAILED_BUT_BINLOG,
  TRUNCATE_FAILED_SKIP_BINLOG,
  TRUNCATE_FAILED_OPEN
};

/**
  Open and truncate a locked base table.

  @param  thd           Thread context.
  @param  table_ref     Table list element for the table to be truncated.
  @param  table_def     Dictionary table object.

  @retval TRUNCATE_OK   Truncate was successful and statement can be safely
                        binlogged.
  @retval TRUNCATE_FAILED_BUT_BINLOG Truncate failed but still go ahead with
                        binlogging as in case of non transactional tables
                        partial truncation is possible.

  @retval TRUNCATE_FAILED_SKIP_BINLOG Truncate was not successful hence do not
                        binlog the statement.
  @retval TRUNCATE_FAILED_OPEN Truncate failed to open table, do not binlog
                        the statement.
*/

static truncate_result handler_truncate_base(THD *thd, TABLE_LIST *table_ref,
                                             dd::Table *table_def) {
  DBUG_ENTER("handler_truncate_base");
  DBUG_ASSERT(table_def != nullptr);

  /*
    Can't recreate, the engine must mechanically delete all rows
    in the table. Use open_and_lock_tables() to open a write cursor.
  */

  /* We don't need to load triggers. */
  DBUG_ASSERT(table_ref->trg_event_map == 0);
  /*
    Our metadata lock guarantees that no transaction is reading
    or writing into the table. Yet, to open a write cursor we need
    a thr_lock lock. Allow to open base tables only.
  */
  table_ref->required_type = dd::enum_table_type::BASE_TABLE;
  /*
    Ignore pending FLUSH TABLES since we don't want to release
    the MDL lock taken above and otherwise there is no way to
    wait for FLUSH TABLES in deadlock-free fashion.
  */
  uint flags = MYSQL_OPEN_IGNORE_FLUSH;
  /*
    Even though we have an MDL lock on the table here, we don't
    pass MYSQL_OPEN_HAS_MDL_LOCK to open_and_lock_tables
    since to truncate a MERGE table, we must open and lock
    merge children, and on those we don't have an MDL lock.
    Thus clear the ticket to satisfy MDL asserts.
  */
  table_ref->mdl_request.ticket = NULL;

  /* Open the table as it will handle some required preparations. */
  if (open_and_lock_tables(thd, table_ref, flags))
    DBUG_RETURN(TRUNCATE_FAILED_OPEN);

  /* Whether to truncate regardless of foreign keys. */
  if (!(thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
    if (fk_truncate_illegal_if_parent(thd, table_ref, table_def))
      DBUG_RETURN(TRUNCATE_FAILED_SKIP_BINLOG);

  /*
    Remove all TABLE/handler instances except the one to be used for
    handler::ha_truncate() call. This is necessary for InnoDB to be
    able to correctly handle truncate as atomic drop and re-create
    internally. If we under LOCK TABLES the caller will re-open tables
    as necessary later.
  */
  close_all_tables_for_name(thd, table_ref->table->s, false, table_ref->table);

  int error = table_ref->table->file->ha_truncate(table_def);

  if (error) {
    table_ref->table->file->print_error(error, MYF(0));
    /*
      If truncate method is not implemented then we don't binlog the
      statement. If truncation has failed in a transactional engine then also we
      donot binlog the statment. Only in non transactional engine we binlog
      inspite of errors.
     */
    if (error == HA_ERR_WRONG_COMMAND ||
        table_ref->table->file->has_transactions())
      DBUG_RETURN(TRUNCATE_FAILED_SKIP_BINLOG);
    else
      DBUG_RETURN(TRUNCATE_FAILED_BUT_BINLOG);
  } else if ((table_ref->table->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL)) {
    if (thd->dd_client()->update(table_def)) {
      /* Statement rollback will revert effect of handler::truncate() as well.
       */
      DBUG_RETURN(TRUNCATE_FAILED_SKIP_BINLOG);
    }
  }
  DBUG_RETURN(TRUNCATE_OK);
}

/**
  Open and truncate a locked temporary table.

  @param  thd           Thread context.
  @param  table_ref     Table list element for the table to be truncated.

  @retval TRUNCATE_OK   Truncate was successful and statement can be safely
                        binlogged.
  @retval TRUNCATE_FAILED_BUT_BINLOG Truncate failed but still go ahead with
                        binlogging as in case of non transactional tables
                        partial truncation is possible.

  @retval TRUNCATE_FAILED_SKIP_BINLOG Truncate was not successful hence do not
                        binlog the statement.
  @retval TRUNCATE_FAILED_OPEN Truncate failed to open table, do not binlog
                        the statement.
*/

static truncate_result handler_truncate_temporary(THD *thd,
                                                  TABLE_LIST *table_ref) {
  DBUG_ENTER("handler_truncate_temporary");

  /*
    Can't recreate, the engine must mechanically delete all rows
    in the table. Use open_and_lock_tables() to open a write cursor.
  */

  /* Open the table as it will handle some required preparations. */
  if (open_and_lock_tables(thd, table_ref, 0))
    DBUG_RETURN(TRUNCATE_FAILED_OPEN);

  int error =
      table_ref->table->file->ha_truncate(table_ref->table->s->tmp_table_def);

  if (error) {
    table_ref->table->file->print_error(error, MYF(0));
    /*
      If truncate method is not implemented then we don't binlog the
      statement. If truncation has failed in a transactional engine then also we
      donot binlog the statment. Only in non transactional engine we binlog
      inspite of errors.
     */
    if (error == HA_ERR_WRONG_COMMAND ||
        table_ref->table->file->has_transactions())
      DBUG_RETURN(TRUNCATE_FAILED_SKIP_BINLOG);
    else
      DBUG_RETURN(TRUNCATE_FAILED_BUT_BINLOG);
  }
  DBUG_RETURN(TRUNCATE_OK);
}

/*
  Handle locking a base table for truncate.

  @param[in]  thd               Thread context.
  @param[in]  table_ref         Table list element for the table to
                                be truncated.
  @param[out] hton              Pointer to handlerton object for the
                                table's storage engine.

  @retval  false  Success.
  @retval  true   Error.
*/

bool Sql_cmd_truncate_table::lock_table(THD *thd, TABLE_LIST *table_ref,
                                        handlerton **hton) {
  TABLE *table = NULL;
  DBUG_ENTER("Sql_cmd_truncate_table::lock_table");

  /* Lock types are set in the parser. */
  DBUG_ASSERT(table_ref->lock_descriptor().type == TL_WRITE);
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
  if (thd->locked_tables_mode) {
    if (!(table = find_table_for_mdl_upgrade(thd, table_ref->db,
                                             table_ref->table_name, false)))
      DBUG_RETURN(true);

    if (acquire_shared_backup_lock(thd, thd->variables.lock_wait_timeout))
      DBUG_RETURN(true);

    *hton = table->s->db_type();

    table_ref->mdl_request.ticket = table->mdl_ticket;
  } else {
    /* Acquire an exclusive lock. */
    DBUG_ASSERT(table_ref->next_global == NULL);
    if (lock_table_names(thd, table_ref, NULL, thd->variables.lock_wait_timeout,
                         0))
      DBUG_RETURN(true);

    const char *schema_name = table_ref->db;
    const char *table_name = table_ref->table_name;

    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Table *table = NULL;
    if (thd->dd_client()->acquire(schema_name, table_name, &table)) {
      // Error is reported by the dictionary subsystem.
      DBUG_RETURN(true);
    }

    if (table == nullptr ||
        table->hidden() == dd::Abstract_table::HT_HIDDEN_SE) {
      my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
      DBUG_RETURN(true);
    }

    if (dd::table_storage_engine(thd, table, hton)) DBUG_RETURN(true);
  }

  /*
    A storage engine can recreate or truncate the table only if there
    are no references to it from anywhere, i.e. no cached TABLE in the
    table cache.
  */
  if (thd->locked_tables_mode) {
    DEBUG_SYNC(thd, "upgrade_lock_for_truncate");
    /* To remove the table from the cache we need an exclusive lock. */
    if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      DBUG_RETURN(true);
    m_ticket_downgrade = table->mdl_ticket;
    /* Close if table is going to be recreated. */
    if ((*hton)->flags & HTON_CAN_RECREATE)
      close_all_tables_for_name(thd, table->s, false, NULL);
  } else {
    /* Table is already locked exclusively. Remove cached instances. */
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table_ref->db,
                     table_ref->table_name, false);
  }

  DBUG_RETURN(false);
}

/*
  Optimized delete of all rows by doing a full generate of the table.

  @remark Will work even if the .MYI and .MYD files are destroyed.
          In other words, it works as long as the .FRM is intact and
          the engine supports re-create.

  @param  thd         Thread context.
  @param  table_ref   Table list element for the table to be truncated.

  @retval  false  Success.
  @retval  true   Error.
*/

bool Sql_cmd_truncate_table::truncate_table(THD *thd, TABLE_LIST *table_ref) {
  int error;
  bool binlog_stmt;
  bool binlog_is_trans;
  handlerton *hton = nullptr;
  bool is_temporary;
  /* Context for reopening temporary table. */
  std::unique_ptr<dd::Table> tmp_table_def, old_tmp_table_def;
  char saved_path[FN_REFLEN + 1];

  DBUG_ENTER("Sql_cmd_truncate_table::truncate_table");

  DBUG_ASSERT((!table_ref->table) || (table_ref->table && table_ref->table->s));

  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  /* Initialize, or reinitialize in case of reexecution (SP). */
  m_ticket_downgrade = NULL;

  /* If it is a temporary table, no need to take locks. */
  is_temporary = is_temporary_table(table_ref);

  if (is_temporary) {
    TABLE *tmp_table = table_ref->table;
    hton = tmp_table->s->db_type();

    /*
      THD::decide_logging_format has not yet been called and may
      not be called at all depending on the engine, so call it here.
    */
    if (thd->decide_logging_format(table_ref) != 0) DBUG_RETURN(true);
    /* In RBR, the statement is not binlogged if the table is temporary. */
    binlog_stmt = !thd->is_current_stmt_binlog_format_row();

    /* Note that a temporary table cannot be partitioned. */
    if (hton->flags & HTON_CAN_RECREATE) {
      HA_CREATE_INFO create_info;
      char saved_norm_path[FN_REFLEN + 1];

      tmp_table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);
      /*
        If LOCK TABLES list is not empty and contains this table
        then unlock the table and remove it from this list.
      */
      mysql_lock_remove(thd, thd->lock, tmp_table);

      /*
        Transfer ownership of dd::Table object and save path so
        we can reopen table after freeing the TABLE_SHARE.
      */
      tmp_table_def.reset(tmp_table->s->tmp_table_def);
      tmp_table->s->tmp_table_def = nullptr;
      old_tmp_table_def.reset(tmp_table_def->clone());
      my_stpcpy(saved_path, tmp_table->s->path.str);

      /* Save normalized path so we can free TABLE_SHARE. */
      my_stpcpy(saved_norm_path, tmp_table->s->normalized_path.str);

      /* Free TABLE and TABLE_SHARE but don't delete table. */
      close_temporary_table(thd, tmp_table, true, false);

      /*
        We must use normalized_path since for temporary tables it
        differs from what dd_recreate_table() would generate based
        on table and schema names.
      */
      if ((error = ha_create_table(thd, saved_norm_path, table_ref->db,
                                   table_ref->table_name, &create_info, true,
                                   true, tmp_table_def.get()))) {
        /* No need to binlog failed truncate-by-recreate. */
        binlog_stmt = false;
      }

      DBUG_ASSERT(!thd->get_transaction()->cannot_safely_rollback(
          Transaction_ctx::STMT));

      binlog_is_trans = (hton->flags & HTON_SUPPORTS_ATOMIC_DDL);
    } else {
      /*
        The engine does not support truncate-by-recreate. Open the
        table and invoke the handler truncate. In such a manner this
        can in fact open several tables if it's a temporary MyISAMMRG
        table.
      */
      error = handler_truncate_temporary(thd, table_ref);

      binlog_is_trans = table_ref->table->file->has_transactions();
    }
  } else /* It's not a temporary table. */
  {
    if (mdl_locker.ensure_locked(table_ref->db)) DBUG_RETURN(true);

    if (lock_table(thd, table_ref, &hton)) DBUG_RETURN(true);

    dd::Table *table_def = nullptr;
    if (thd->dd_client()->acquire_for_modification(
            table_ref->db, table_ref->table_name, &table_def))
      DBUG_RETURN(true);

    DBUG_ASSERT(table_def != nullptr);

    if (hton->flags & HTON_CAN_RECREATE) {
      error = mysql_audit_table_access_notify(thd, table_ref);
      /*
        The storage engine can truncate the table by creating an
        empty table with the same structure.
      */

      /*
        Check if table can't be truncated because there is a foreign key
        on some other table which references it.
      */
      if (!error &&
          !(thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
        error = fk_truncate_illegal_if_parent(thd, table_ref, table_def);

      if (!error) {
        HA_CREATE_INFO create_info;

        // Create a path to the table, but without a extension
        char path[FN_REFLEN + 1];
        build_table_filename(path, sizeof(path) - 1, table_ref->db,
                             table_ref->table_name, "", 0);

        // Attempt to reconstruct the table
        error = ha_create_table(thd, path, table_ref->db, table_ref->table_name,
                                &create_info, true, false, table_def);
      }

      /* No need to binlog a failed truncate-by-recreate. */
      binlog_stmt = !error;
      binlog_is_trans = (hton->flags & HTON_SUPPORTS_ATOMIC_DDL);
    } else {
      /*
        The engine does not support truncate-by-recreate.
        Attempt to use the handler truncate method.
        MYSQL_AUDIT_TABLE_ACCESS_READ audit event is generated when opening
        tables using open_tables function.
      */
      error = handler_truncate_base(thd, table_ref, table_def);

      /*
        All effects of a TRUNCATE TABLE operation are committed even if
        truncation fails in the case of non transactional tables. Thus, the
        query must be written to the binary log for such tables.
        The exceptions are failure to open table or unimplemented truncate
        method.
      */
      if (error == TRUNCATE_OK || error == TRUNCATE_FAILED_BUT_BINLOG) {
        binlog_stmt = true;
        binlog_is_trans = table_ref->table->file->has_transactions();
      } else {
        binlog_stmt = false;
        binlog_is_trans = false;  // Safety.
      }

      /*
        Call to handler_truncate() might have updated table definition
        in the data-dictionary, let us remove TABLE_SHARE from the TDC.
        This needs to be done even in case of failure so InnoDB SE
        properly invalidates its internal cache.
      */
      if (error != TRUNCATE_FAILED_OPEN) {
        close_all_tables_for_name(thd, table_ref->table->s, false, NULL);
      }
    }
  }

  /* DDL is logged in statement format, regardless of binlog format. */
  if (binlog_stmt)
    error |= write_bin_log(thd, !error, thd->query().str, thd->query().length,
                           binlog_is_trans);

  /* Commit or rollback statement before downgrading metadata lock. */
  if (!error) error = (trans_commit_stmt(thd) || trans_commit_implicit(thd));

  if (error) {
    trans_rollback_stmt(thd);
    /*
      Full rollback in case we have THD::transaction_rollback_request
      and to synchronize DD state in cache and on disk (as statement
      rollback doesn't clear DD cache of modified uncommitted objects).
    */
    trans_rollback(thd);
  }

  if (is_temporary) {
    if (hton->flags & HTON_CAN_RECREATE) {
      /* Temporary table was closed and needs to be reopened. */

      if (error && (hton->flags & HTON_SUPPORTS_ATOMIC_DDL))
        tmp_table_def = std::move(old_tmp_table_def);

      TABLE *new_table;

      if ((new_table = open_table_uncached(thd, saved_path, table_ref->db,
                                           table_ref->table_name, true, true,
                                           *tmp_table_def))) {
        /* Transfer ownership of dd::Table object to the new TABLE_SHARE. */
        new_table->s->tmp_table_def = tmp_table_def.release();
        thd->thread_specific_used = true;
      } else {
        rm_temporary_table(thd, hton, saved_path, tmp_table_def.get());
        error = true;
      }
    }
  } else {
    /* Base table case. */
    if (thd->locked_tables_mode && thd->locked_tables_list.reopen_tables(thd))
      thd->locked_tables_list.unlink_all_closed_tables(thd, NULL, 0);

    if ((hton->flags & HTON_SUPPORTS_ATOMIC_DDL) && hton->post_ddl)
      hton->post_ddl(thd);
  }

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

  @return false on success.
*/
bool Sql_cmd_truncate_table::execute(THD *thd) {
  bool res = true;
  TABLE_LIST *first_table = thd->lex->select_lex->table_list.first;
  DBUG_ENTER("Sql_cmd_truncate_table::execute");

  if (check_one_table_access(thd, DROP_ACL, first_table)) DBUG_RETURN(res);

  if (!(res = truncate_table(thd, first_table))) my_ok(thd);

  DBUG_RETURN(res);
}
